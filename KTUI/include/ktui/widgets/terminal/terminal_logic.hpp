#pragma once

#include "terminal_data.hpp"
#include "terminal_style.hpp"
#include <ktui/core/strings.hpp>
#include "../box/box_logic.hpp"
#include "../button/button_logic.hpp"
#include "../hstack/hstack_logic.hpp"
#include "../menu_flyout/menu_flyout_logic.hpp"
#include "../scroll/scrollbar.hpp"
#include "../text/text_logic.hpp"
#include "../textbox/textbox_logic.hpp"
#include "../toggle_button/toggle_button_logic.hpp"
#include "../widget.hpp"
#include <ktui/app/application.hpp>
#include <ktui/app/window.hpp>
#include <ktui/core/light_mutex.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ktui {

class RenderTerminal;

// ==============================================================================
// 运行时终端控制器句柄 (TerminalController)
// 供宿主在任意时刻切换后端 (ConPTY / SSH / 串口 / DemoShell)、发送输入、切换配色/光标/字号、打开搜索栏
// ==============================================================================
class TerminalController {
public:
    void bind(std::weak_ptr<RenderTerminal> node) noexcept { m_node = std::move(node); }

    void set_backend(std::shared_ptr<ITerminalBackend> backend);
    void write_input(std::string_view utf8_bytes);
    void feed_output(std::string_view vt_bytes);
    void clear();
    void set_color_scheme(TerminalColorScheme scheme);
    void set_cursor_style(TerminalCursorStyle style);
    void set_font_size(float pt);
    void toggle_find();
    [[nodiscard]] std::string title() const;
    [[nodiscard]] std::string backend_name() const;
    [[nodiscard]] std::pair<int, int> grid_size() const;

private:
    std::weak_ptr<RenderTerminal> m_node;
};

// ==============================================================================
// 终端渲染节点 (RenderTerminal)
// 对标 Microsoft Windows Terminal (TermControl):
//   - 自由对接任意 ITerminalBackend (ConPTY / SSH / 串口 / 管道 / DemoShell)
//   - 线程安全异步输出缓冲 + UI 主线程低延迟唤醒
//   - 全量 VT500/ANSI/256色/24-bit TrueColor/粗体/斜体/下划线/删除线/反色/宽字符渲染
//   - 5 种 Windows Terminal 光标样式 + 平滑闪烁 + 失焦空心框
//   - 线性选区 + Alt 矩形块选区 + 双击选词 + 三击选行 + Ctrl+Shift+C/V + 括号粘贴 (?2004h)
//   - Ctrl+Shift+F 顶部浮动内联搜索栏 (高亮所有命中 + 橙色当前命中 + 上下跳转 + 大小写切换)
//   - Ctrl+=/Ctrl+-/Ctrl+0 动态缩放字号 + 右键 MenuFlyout 上下文菜单
//   - 复用 WinUIScrollBar 滚动历史回滚缓冲 (绝不自绘滚动条)
//   - 继承 ITextInputClient 原生支持中文/日文/韩文/Emoji 系统输入法 (IME) 光标跟随与内联预编辑
//   - 支持 Windows Terminal OSC 9;4 顶部进度条指示器
// ==============================================================================
class RenderTerminal : public RenderBox, public ITextInputClient {
public:
    static constexpr float kPadX = 10.0f;
    static constexpr float kPadY = 8.0f;
    static constexpr float kMinFontSize = 8.0f;
    static constexpr float kMaxFontSize = 32.0f;

    RenderTerminal(std::shared_ptr<ITerminalBackend> backend = nullptr,
                   TerminalColorScheme scheme = TerminalColorScheme::campbell(),
                   TerminalCursorStyle cursor_style = TerminalCursorStyle::Bar,
                   float font_size = 13.5f,
                   size_t max_scrollback = 9001,
                   bool copy_on_select = false,
                   Fn<void(const std::string&)> on_title_changed = nullptr,
                   Fn<void(int)> on_exit = nullptr)
        : m_buffer(80, 24, max_scrollback),
          m_scheme(std::move(scheme)),
          m_cursor_style(cursor_style),
          m_font_size(std::clamp(font_size, kMinFontSize, kMaxFontSize)),
          m_default_font_size(std::clamp(font_size, kMinFontSize, kMaxFontSize)),
          m_copy_on_select(copy_on_select),
          m_on_title_changed(std::move(on_title_changed)),
          m_on_exit(std::move(on_exit)) {
        is_focusable = true;
        cursor_shape = CursorShape::IBeam;
        recompute_cell_metrics();

        m_buffer.on_title_changed = [this](const std::string& t) {
            if (m_on_title_changed) m_on_title_changed(t);
        };
        m_buffer.on_progress_changed = [this](TerminalProgress) {
            mark_needs_paint();
        };
        m_buffer.on_reply = [this](std::string_view reply) {
            if (m_backend && m_backend->is_running()) {
                m_backend->write(reply);
            }
        };

        setup_behavior();

        if (!backend) {
            backend = std::make_shared<DemoShellBackend>();
        }
        set_backend(std::move(backend));
    }

    ~RenderTerminal() override {
        *m_alive = false;
        if (m_find_bar_node && m_find_bar_node->parent() == this) {
            remove_child(m_find_bar_node.get());
        }
        detach_backend();
    }

    // 零 RTTI 暴露 ITextInputClient 接口供 Window 自动接管 IME
    [[nodiscard]] ITextInputClient* as_text_input_client() noexcept override { return this; }

    // ==================== ITextInputClient 实现 (中文/CJK/Emoji IME) ====================
    [[nodiscard]] bool accepts_text_input() const noexcept override { return true; }

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        Vec2 g = global_offset();
        int vis_row = m_buffer.cursor_row() + static_cast<int>(m_buffer.scrollback_count()) - first_visible_row();
        float cx = g.x + kPadX + static_cast<float>(m_buffer.cursor_col()) * m_cell_w;
        float cy = g.y + kPadY + static_cast<float>(std::clamp(vis_row, 0, m_buffer.rows() - 1)) * m_cell_h;
        return Rect::from_xywh(cx, cy, m_cell_w, m_cell_h);
    }

    void on_ime_preedit(const std::string& text, size_t) override {
        m_ime_preedit = text;
        mark_needs_paint();
    }

    void on_ime_commit(const std::string& utf8_text) override {
        m_ime_preedit.clear();
        if (utf8_text.empty()) return;
        if (m_find_open) {
            m_find_query.append(utf8_text);
            m_find_query_prop.set(m_find_query);
            run_find(true);
            mark_needs_paint();
            return;
        }
        send_to_backend(utf8_text);
    }

    bool tick_cursor_blink() override {
        if (!m_focused || !m_buffer.cursor_blink()) return false;
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_blink_epoch).count();
        bool next_on = ((ms / 530) % 2) == 0;
        if (next_on != m_cursor_blink_on) {
            m_cursor_blink_on = next_on;
            mark_needs_paint();
            return true;
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept override {
        if (!m_focused || !m_buffer.cursor_blink()) return 530;
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_blink_epoch).count();
        return static_cast<uint32_t>(530 - (ms % 530));
    }

    // ==================== 公共控制 API ====================
    void set_backend(std::shared_ptr<ITerminalBackend> backend) {
        detach_backend();
        m_buffer.clear_all();
        clear_selection();
        m_scroll_y = 0.0f;
        m_stick_to_bottom = true;
        m_backend = std::move(backend);
        if (!m_backend) return;

        std::weak_ptr<bool> weak_alive = m_alive;
        m_backend->on_output = [this, weak_alive](std::string_view chunk) {
            if (weak_alive.expired() || chunk.empty()) return;
            enqueue_rx(chunk);
        };
        m_backend->on_exit = [this, weak_alive](int code) {
            if (weak_alive.expired()) return;
            auto post_exit = [this, weak_alive, code]() {
                if (weak_alive.expired()) return;
                drain_rx_queue();
                if (m_on_exit) m_on_exit(code);
                mark_needs_paint();
                request_host_redraw();
            };
            if (Application* app = Application::current()) {
                app->dispatcher().run_on_ui_thread(std::move(post_exit));
                request_host_redraw();
            } else {
                post_exit();
            }
        };

        m_backend->start(m_buffer.cols(), m_buffer.rows());
        drain_rx_queue();
        mark_needs_paint();
    }

    void write_input(std::string_view utf8_bytes) {
        send_to_backend(utf8_bytes);
    }

    void feed_output(std::string_view vt_bytes) {
        m_buffer.feed(vt_bytes);
        if (m_stick_to_bottom) {
            scroll_to_bottom();
        }
        if (m_find_open && !m_find_query.empty()) {
            run_find(false);
        }
        mark_needs_paint();
    }

    void clear() {
        m_buffer.clear_all();
        clear_selection();
        m_scroll_y = 0.0f;
        m_stick_to_bottom = true;
        mark_needs_paint();
    }

    void set_color_scheme(TerminalColorScheme scheme) {
        m_scheme = std::move(scheme);
        mark_needs_paint();
    }

    void set_cursor_style(TerminalCursorStyle style) noexcept {
        m_cursor_style = style;
        mark_needs_paint();
    }

    void set_font_size(float pt) {
        float clamped = std::clamp(pt, kMinFontSize, kMaxFontSize);
        if (std::abs(clamped - m_font_size) > 0.05f) {
            m_font_size = clamped;
            recompute_cell_metrics();
            sync_grid_size();
            mark_needs_paint();
        }
    }

    void set_copy_on_select(bool v) noexcept { m_copy_on_select = v; }

    void toggle_find() {
        m_find_open = !m_find_open;
        if (m_find_open) {
            ensure_find_bar();
            if (has_selection()) {
                std::string sel = selected_text();
                if (!sel.empty() && sel.find('\n') == std::string::npos && sel.size() <= 64) {
                    m_find_query = sel;
                    m_find_query_prop.set(sel);
                }
            }
            layout_find_bar();
            run_find(true);
            if (Window* win = Window::active_window()) {
                if (RenderBox* tb = find_focusable_text_input(m_find_bar_node.get())) {
                    win->focus_manager().request_focus(tb);
                }
            }
        } else {
            m_find_matches.clear();
            m_find_idx = -1;
            m_find_count_prop.set("");
            if (Window* win = Window::active_window()) {
                win->focus_manager().request_focus(this);
            }
        }
        mark_needs_paint();
    }

    [[nodiscard]] const TerminalBuffer& buffer() const noexcept { return m_buffer; }
    [[nodiscard]] TerminalBuffer& buffer() noexcept { return m_buffer; }
    [[nodiscard]] const TerminalColorScheme& color_scheme() const noexcept { return m_scheme; }
    [[nodiscard]] TerminalCursorStyle cursor_style() const noexcept { return m_cursor_style; }
    [[nodiscard]] float font_size() const noexcept { return m_font_size; }
    [[nodiscard]] bool is_find_open() const noexcept { return m_find_open; }
    [[nodiscard]] const std::vector<TerminalSearchMatch>& find_matches() const noexcept { return m_find_matches; }
    [[nodiscard]] std::string backend_name() const { return m_backend ? m_backend->backend_name() : "Disconnected"; }
    [[nodiscard]] bool has_selection() const noexcept { return m_has_selection; }

    [[nodiscard]] std::string selected_text() const {
        if (!m_has_selection) return {};
        return m_buffer.extract_text(m_sel_start, m_sel_end, m_sel_block);
    }

    // ==================== 布局与命中测试 ====================
    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(m_constraints.min_width, 680.0f);
        float h = m_constraints.has_bounded_height() ? m_constraints.max_height : 400.0f;
        m_size = m_constraints.constrain(Vec2{std::max(240.0f, w), std::max(120.0f, h)});
        sync_grid_size();
        if (m_find_open && m_find_bar_node) {
            layout_find_bar();
        }
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (!bounds().contains(local_position)) return false;
        if (m_find_open && m_find_bar_node) {
            PointF cp = local_position - m_find_bar_node->offset();
            if (m_find_bar_node->hit_test(result, cp)) {
                // 彻底拦截搜索浮层区域的指针命中，严禁点击/悬停/滚轮穿透到底层终端
                return true;
            }
        }
        result.add(this, local_position);
        return true;
    }

    // ==================== 渲染 (对齐 Windows Terminal Atlas 视觉风格) ====================
    void paint(PaintContext& ctx) override;

private:
    void detach_backend() {
        if (m_backend) {
            m_backend->on_output = nullptr;
            m_backend->on_exit = nullptr;
            m_backend->close();
            m_backend.reset();
        }
    }

    void enqueue_rx(std::string_view chunk) {
        {
            LightLock lk(m_rx_mutex);
            m_rx_pending.append(chunk);
        }
        std::weak_ptr<bool> weak_alive = m_alive;
        if (Application* app = Application::current()) {
            app->dispatcher().post_task([this, weak_alive]() {
                if (weak_alive.expired()) return;
                drain_rx_queue();
                mark_needs_paint();
                request_host_redraw();
            });
            request_host_redraw();
        }
    }

    void drain_rx_queue() {
        std::string batch;
        {
            LightLock lk(m_rx_mutex);
            if (m_rx_pending.empty()) return;
            batch.swap(m_rx_pending);
        }
        m_buffer.feed(batch);
        if (m_stick_to_bottom) {
            scroll_to_bottom();
        }
        if (m_find_open && !m_find_query.empty()) {
            run_find(false);
        }
    }

    static void request_host_redraw() {
        if (Window* win = Window::active_window()) {
            win->request_redraw();
            if (auto* pw = win->platform_window()) {
                pw->invalidate();
            }
        }
    }

    void recompute_cell_metrics() {
        Vec2 m10 = TextMeasure::measure("0000000000", m_font_size, FontWeight::Regular, false, m_font_family);
        m_cell_w = std::max(5.0f, m10.x * 0.1f);
        m_cell_h = std::max(12.0f, std::round(std::max(m10.y, m_font_size * 1.32f)));
        m_text_y_off = std::max(0.0f, (m_cell_h - m10.y) * 0.5f);
    }

    void sync_grid_size() {
        float avail_w = std::max(40.0f, m_size.x - kPadX * 2.0f - WinUIScrollBar::kTrackWidth);
        float avail_h = std::max(24.0f, m_size.y - kPadY * 2.0f);
        int new_cols = std::max(10, static_cast<int>(std::floor(avail_w / m_cell_w)));
        int new_rows = std::max(4, static_cast<int>(std::floor(avail_h / m_cell_h)));
        if (new_cols != m_buffer.cols() || new_rows != m_buffer.rows()) {
            m_buffer.resize(new_cols, new_rows);
            if (m_backend && m_backend->is_running()) {
                m_backend->resize(new_cols, new_rows);
            }
            if (m_stick_to_bottom) {
                scroll_to_bottom();
            } else {
                clamp_scroll();
            }
        }
    }

    [[nodiscard]] float max_scroll_y() const noexcept {
        return static_cast<float>(std::max(0, m_buffer.total_rows() - m_buffer.rows())) * m_cell_h;
    }

    [[nodiscard]] float total_content_height() const noexcept {
        return m_size.y + max_scroll_y();
    }

    void clamp_scroll() noexcept {
        float ms = max_scroll_y();
        m_scroll_y = std::clamp(m_scroll_y, 0.0f, ms);
        m_stick_to_bottom = (ms <= 0.5f) || (m_scroll_y >= ms - m_cell_h * 0.5f);
    }

    void scroll_to_bottom() noexcept {
        m_scroll_y = max_scroll_y();
        m_stick_to_bottom = true;
    }

    [[nodiscard]] int first_visible_row() const noexcept {
        int max_first = std::max(0, m_buffer.total_rows() - m_buffer.rows());
        if (m_stick_to_bottom) return max_first;
        int r = static_cast<int>(std::round(m_scroll_y / m_cell_h));
        return std::clamp(r, 0, max_first);
    }

    [[nodiscard]] TerminalCoord point_to_coord(PointF local_pos) const noexcept {
        int col = static_cast<int>(std::floor((local_pos.x - kPadX) / m_cell_w));
        int vr  = static_cast<int>(std::floor((local_pos.y - kPadY) / m_cell_h));
        col = std::clamp(col, 0, m_buffer.cols() - 1);
        int abs_r = std::clamp(first_visible_row() + vr, 0, std::max(0, m_buffer.total_rows() - 1));
        return TerminalCoord{col, abs_r};
    }

    void clear_selection() noexcept {
        m_has_selection = false;
        m_dragging_sel = false;
    }

    void send_to_backend(std::string_view bytes) {
        if (bytes.empty()) return;
        reset_blink();
        scroll_to_bottom();
        if (m_backend && m_backend->is_running()) {
            m_backend->write(bytes);
        }
        mark_needs_paint();
    }

    void reset_blink() noexcept {
        m_blink_epoch = std::chrono::steady_clock::now();
        m_cursor_blink_on = true;
    }

    void copy_selection() {
        if (!m_has_selection) return;
        std::string txt = selected_text();
        if (!txt.empty()) {
            Clipboard::set_text(txt);
        }
    }

    void paste_from_clipboard() {
        std::string txt = Clipboard::get_text();
        if (txt.empty()) return;
        if (m_buffer.bracketed_paste()) {
            send_to_backend("\x1b[200~" + txt + "\x1b[201~");
        } else {
            send_to_backend(txt);
        }
    }

    // ==================== 内联搜索栏 (Ctrl+Shift+F，100% 复用真实 KTUI 控件) ====================
    static RenderBox* find_focusable_text_input(RenderNode* node) noexcept {
        if (!node) return nullptr;
        if (auto* box = node->as_render_box()) {
            if (box->is_focusable && box->as_text_input_client() != nullptr) return box;
        }
        for (RenderNode* c = node->first_child(); c != nullptr; c = c->next_sibling()) {
            if (RenderBox* found = find_focusable_text_input(c)) return found;
        }
        return nullptr;
    }

    void ensure_find_bar();

    void layout_find_bar() {
        if (!m_find_bar_node) return;
        float bar_w = std::min(360.0f, std::max(240.0f, m_size.x - 28.0f));
        m_find_bar_node->layout(BoxConstraints{bar_w, bar_w, 0.0f, 56.0f}, true);
        float fx = std::max(8.0f, m_size.x - m_find_bar_node->size().x - 16.0f);
        m_find_bar_node->set_offset(Vec2{std::round(fx), 8.0f});
    }

    void update_find_count() {
        if (m_find_query.empty()) {
            m_find_count_prop.set("");
        } else if (m_find_matches.empty()) {
            m_find_count_prop.set("0/0");
        } else {
            m_find_count_prop.set(std::to_string(m_find_idx + 1) + "/" + std::to_string(m_find_matches.size()));
        }
    }

    void run_find(bool jump_to_match) {
        m_find_matches = m_buffer.find_all(m_find_query, m_find_case);
        if (m_find_matches.empty()) {
            m_find_idx = -1;
            update_find_count();
            return;
        }
        if (m_find_idx < 0 || m_find_idx >= static_cast<int>(m_find_matches.size())) {
            m_find_idx = static_cast<int>(m_find_matches.size()) - 1;
        }
        update_find_count();
        if (m_find_open && m_find_bar_node) layout_find_bar();
        if (jump_to_match) scroll_to_match(m_find_idx);
    }

    void step_find(int delta) {
        if (m_find_matches.empty()) return;
        int n = static_cast<int>(m_find_matches.size());
        m_find_idx = (m_find_idx + delta + n) % n;
        update_find_count();
        scroll_to_match(m_find_idx);
        mark_needs_paint();
    }

    void scroll_to_match(int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_find_matches.size())) return;
        int row = m_find_matches[static_cast<size_t>(idx)].row;
        float target = std::max(0.0f, (static_cast<float>(row) - static_cast<float>(m_buffer.rows()) * 0.4f) * m_cell_h);
        m_scroll_y = std::clamp(target, 0.0f, max_scroll_y());
        m_stick_to_bottom = false;
    }

    // ==================== 右键上下文菜单 (复用 MenuFlyout) ====================
    void show_context_menu(PointF global_pos);

    // ==================== VT / SGR 鼠标回报编码 (支持 Windows edit / vim / htop / btop / tmux) ====================
    [[nodiscard]] bool should_report_mouse(const KeyModifiers& mods) const noexcept {
        // 对标 Windows Terminal：TUI 开启鼠标追踪时，按住 Shift 可临时旁路以进行本地选区或右键菜单
        return m_buffer.mouse_tracking_mode() != 0 && !mods.shift;
    }

    [[nodiscard]] std::pair<int, int> point_to_vt_cell_1based(PointF local_pos) const noexcept {
        int col = std::clamp(static_cast<int>(std::floor((local_pos.x - kPadX) / m_cell_w)), 0, m_buffer.cols() - 1) + 1;
        int row = std::clamp(static_cast<int>(std::floor((local_pos.y - kPadY) / m_cell_h)), 0, m_buffer.rows() - 1) + 1;
        return {col, row};
    }

    void send_vt_mouse(int cb, int col1, int row1, bool is_release, const KeyModifiers& mods) {
        if (mods.shift) cb |= 4;
        if (mods.alt)   cb |= 8;
        if (mods.ctrl)  cb |= 16;

        char seq[48];
        if (m_buffer.mouse_sgr_mode()) {
            std::snprintf(seq, sizeof(seq), "\x1b[<%d;%d;%d%c", cb, col1, row1, is_release ? 'm' : 'M');
            send_to_backend(seq);
        } else {
            int cx = std::clamp(col1, 1, 223);
            int cy = std::clamp(row1, 1, 223);
            int enc_cb = is_release ? (3 | (cb & 28)) : cb;
            seq[0] = '\x1b';
            seq[1] = '[';
            seq[2] = 'M';
            seq[3] = static_cast<char>(32 + enc_cb);
            seq[4] = static_cast<char>(32 + cx);
            seq[5] = static_cast<char>(32 + cy);
            send_to_backend(std::string_view(seq, 6));
        }
    }

    // ==================== 交互与键盘 VT 编码 ====================
    void setup_behavior();

    void select_word_at(TerminalCoord pos) {
        const auto& row = m_buffer.row_at(pos.row);
        if (row.cells.empty()) return;
        auto is_word = [](uint32_t cp) noexcept -> bool {
            return cp == 0 || (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') ||
                   (cp >= 'a' && cp <= 'z') || cp == '_' || cp == '-' || cp >= 0x80u;
        };
        int c0 = pos.col;
        int c1 = pos.col;
        while (c0 > 0 && is_word(row.cells[static_cast<size_t>(c0 - 1)].codepoint)) --c0;
        while (c1 + 1 < static_cast<int>(row.cells.size()) && is_word(row.cells[static_cast<size_t>(c1 + 1)].codepoint)) ++c1;
        if (row.cells[static_cast<size_t>(c0)].width == 0 && c0 > 0) --c0;
        if (row.cells[static_cast<size_t>(c1)].width == 2 && c1 + 1 < static_cast<int>(row.cells.size())) ++c1;
        m_has_selection = true;
        m_sel_block = false;
        m_sel_start = TerminalCoord{c0, pos.row};
        m_sel_end = TerminalCoord{c1, pos.row};
    }

    // 完整 VT100 / xterm 键盘编码器
    [[nodiscard]] static std::string encode_vt_key(uint32_t vk, bool ctrl, bool shift, bool alt, bool app_cursor);

    TerminalBuffer m_buffer;
    std::shared_ptr<ITerminalBackend> m_backend;
    TerminalColorScheme m_scheme;
    TerminalCursorStyle m_cursor_style{TerminalCursorStyle::Bar};
    std::string m_font_family{"monospace"};
    float m_font_size{13.5f};
    float m_default_font_size{13.5f};
    float m_cell_w{8.0f};
    float m_cell_h{19.0f};
    float m_text_y_off{2.0f};
    bool m_copy_on_select{false};

    bool m_focused{false};
    bool m_cursor_blink_on{true};
    std::chrono::steady_clock::time_point m_blink_epoch{std::chrono::steady_clock::now()};
    std::string m_ime_preedit;

    float m_scroll_y{0.0f};
    bool m_stick_to_bottom{true};
    WinUIScrollBar m_scrollbar;

    bool m_has_selection{false};
    bool m_dragging_sel{false};
    bool m_sel_block{false};
    TerminalCoord m_sel_start{};
    TerminalCoord m_sel_end{};
    std::chrono::steady_clock::time_point m_last_click_time{};
    TerminalCoord m_last_click_coord{};
    int m_click_streak{0};
    int m_vt_mouse_button{-1};
    int m_vt_last_col{-1};
    int m_vt_last_row{-1};

    bool m_find_open{false};
    bool m_find_case{false};
    std::string m_find_query;
    std::vector<TerminalSearchMatch> m_find_matches;
    int m_find_idx{-1};
    std::shared_ptr<RenderBox> m_find_bar_node;
    Property<std::string> m_find_query_prop{""};
    Property<std::string> m_find_count_prop{""};
    Property<ButtonVariant> m_find_case_variant{ButtonVariant::Subtle};

    LightMutex m_rx_mutex;
    std::string m_rx_pending;
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

    Fn<void(const std::string&)> m_on_title_changed;
    Fn<void(int)> m_on_exit;
};

} // namespace ktui

#include "terminal_paint.hpp"
#include "terminal_behavior.hpp"
#include "terminal_widget.hpp"
