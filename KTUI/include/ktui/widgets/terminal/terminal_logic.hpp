#pragma once

#include "terminal_backend.hpp"
#include "terminal_types.hpp"
#include "vt_parser.hpp"
#include "../box.hpp"
#include "../button.hpp"
#include "../hstack.hpp"
#include "../menu_flyout.hpp"
#include "../scroll/scrollbar.hpp"
#include "../text.hpp"
#include "../textbox.hpp"
#include "../toggle_button.hpp"
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
    void paint(PaintContext& ctx) override {
        drain_rx_queue();

        const Theme& theme = ThemeContext::current();
        const Rect b = bounds();
        const Color bg = m_scheme.background;
        const Color bd = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.11f} : Color{0.0f, 0.0f, 0.0f, 0.14f};

        // 1. 终端主背景与圆角裁剪
        ctx.draw_rect(b, bg, 6.0f, 0.0f);
        ctx.push_clip(b, 6.0f);

        const int total_r = m_buffer.total_rows();
        const int first_r = first_visible_row();
        const int vis_rows = m_buffer.rows();
        const int cols = m_buffer.cols();

        TerminalCoord sel_a = m_sel_start;
        TerminalCoord sel_b = m_sel_end;
        if (m_has_selection && sel_b < sel_a) std::swap(sel_a, sel_b);

        // 2. 绘制所有可见行（背景批处理合并 + 选区/搜索高亮 + 文本与装饰线）
        for (int vr = 0; vr < vis_rows; ++vr) {
            int abs_r = first_r + vr;
            if (abs_r < 0 || abs_r >= total_r) continue;
            const auto& row = m_buffer.row_at(abs_r);
            const float y = kPadY + static_cast<float>(vr) * m_cell_h;
            const int row_cols = std::min(cols, static_cast<int>(row.cells.size()));

            // 2a. 合并连续同色背景格，大幅减少 GPU 图元数
            int run_start = -1;
            Color run_col{};
            auto flush_bg = [&](int end_col) {
                if (run_start >= 0 && end_col > run_start) {
                    float rx = kPadX + static_cast<float>(run_start) * m_cell_w;
                    float rw = static_cast<float>(end_col - run_start) * m_cell_w;
                    ctx.draw_rect(Rect::from_xywh(rx, y, rw + 0.5f, m_cell_h + 0.5f), run_col, 0.0f, 0.0f);
                }
                run_start = -1;
            };

            for (int c = 0; c < row_cols; ++c) {
                const auto& cell = row.cells[static_cast<size_t>(c)];
                bool inv = (cell.attrs & TerminalAttr::Inverse) != 0;
                bool has_custom_bg = inv || (cell.bg.kind != TerminalColor::Kind::Default);
                if (!has_custom_bg) {
                    flush_bg(c);
                    continue;
                }
                Color cbg = inv ? m_scheme.resolve(cell.fg, true, (cell.attrs & TerminalAttr::Bold) != 0)
                                : m_scheme.resolve(cell.bg, false, false);
                if (run_start >= 0 &&
                    std::abs(cbg.r - run_col.r) < 0.002f &&
                    std::abs(cbg.g - run_col.g) < 0.002f &&
                    std::abs(cbg.b - run_col.b) < 0.002f &&
                    std::abs(cbg.a - run_col.a) < 0.002f) {
                    continue;
                }
                flush_bg(c);
                run_start = c;
                run_col = cbg;
            }
            flush_bg(row_cols);

            // 2b. 搜索命中高亮 (Windows Terminal 风格：普通命中暗黄，当前命中亮橙)
            if (m_find_open && !m_find_matches.empty()) {
                for (size_t mi = 0; mi < m_find_matches.size(); ++mi) {
                    const auto& m = m_find_matches[mi];
                    if (m.row != abs_r) continue;
                    bool is_cur = (static_cast<int>(mi) == m_find_idx);
                    Color hc = is_cur ? Color{1.0f, 0.55f, 0.0f, 0.55f} : Color{1.0f, 0.85f, 0.20f, 0.30f};
                    float mx = kPadX + static_cast<float>(m.col_start) * m_cell_w;
                    float mw = static_cast<float>(std::max(1, m.col_end - m.col_start)) * m_cell_w;
                    ctx.draw_rect(Rect::from_xywh(mx, y, mw, m_cell_h), hc, 2.0f, 0.0f);
                }
            }

            // 2c. 选区高亮 (支持线性选区与 Alt 块选区，宽字符自动整字对齐绝不选半个汉字)
            if (m_has_selection && abs_r >= sel_a.row && abs_r <= sel_b.row) {
                int sc0 = m_sel_block ? std::min(sel_a.col, sel_b.col) : ((abs_r == sel_a.row) ? sel_a.col : 0);
                int sc1 = m_sel_block ? std::max(sel_a.col, sel_b.col) : ((abs_r == sel_b.row) ? sel_b.col : row_cols - 1);
                sc0 = std::clamp(sc0, 0, row_cols - 1);
                sc1 = std::clamp(sc1, 0, row_cols - 1);
                if (row.cells[static_cast<size_t>(sc0)].width == 0 && sc0 > 0) --sc0;
                if (row.cells[static_cast<size_t>(sc1)].width == 2 && sc1 + 1 < row_cols) ++sc1;
                if (sc1 >= sc0) {
                    float sx = kPadX + static_cast<float>(sc0) * m_cell_w;
                    float sw = static_cast<float>(sc1 - sc0 + 1) * m_cell_w;
                    ctx.draw_rect(Rect::from_xywh(sx, y, sw, m_cell_h), m_scheme.selection_bg, 0.0f, 0.0f);
                }
            }

            // 2d. 字符与装饰线绘制 (真正等宽字体 Cascadia Mono / Consolas + 连续 ASCII 同样式合批，极速且零字距撕裂)
            int c = 0;
            while (c < row_cols) {
                const auto& cell = row.cells[static_cast<size_t>(c)];
                if (cell.width == 0 || (cell.attrs & TerminalAttr::Invisible) != 0) {
                    ++c;
                    continue;
                }

                bool inv = (cell.attrs & TerminalAttr::Inverse) != 0;
                bool bold = (cell.attrs & TerminalAttr::Bold) != 0;
                bool dim = (cell.attrs & TerminalAttr::Dim) != 0;
                bool italic = (cell.attrs & TerminalAttr::Italic) != 0;
                Color fg = inv ? m_scheme.resolve(cell.bg, false, false)
                               : m_scheme.resolve(cell.fg, true, bold);
                if (dim) fg.a *= 0.55f;

                // 窄 ASCII 字符与同样式连续合并为一个 draw_text 批次（完美复用等宽字体自带精确步进）
                if (cell.width == 1 && cell.codepoint >= 0x20u && cell.codepoint < 0x7Fu) {
                    int start_c = c;
                    std::string run;
                    while (c < row_cols) {
                        const auto& nc = row.cells[static_cast<size_t>(c)];
                        if (nc.width != 1 || nc.codepoint < 0x20u || nc.codepoint >= 0x7Fu) break;
                        if (nc.fg != cell.fg || nc.bg != cell.bg || nc.attrs != cell.attrs) break;
                        run.push_back(static_cast<char>(nc.codepoint));
                        ++c;
                    }
                    float rx = kPadX + static_cast<float>(start_c) * m_cell_w;
                    float rw = static_cast<float>(c - start_c) * m_cell_w;
                    bool has_non_space = false;
                    for (char ch : run) {
                        if (ch != ' ') { has_non_space = true; break; }
                    }
                    if (has_non_space) {
                        ctx.draw_text(run, PointF{std::round(rx), std::round(y + m_text_y_off)},
                                      m_font_size, fg, bold ? FontWeight::Bold : FontWeight::Regular,
                                      italic, m_font_family);
                    }
                    if ((cell.attrs & (TerminalAttr::Underline | TerminalAttr::Hyperlink)) != 0) {
                        ctx.draw_rect(Rect::from_xywh(rx, y + m_cell_h - 2.0f, rw, 1.0f), fg, 0.0f, 0.0f);
                    }
                    if ((cell.attrs & TerminalAttr::DoubleUnderline) != 0) {
                        ctx.draw_rect(Rect::from_xywh(rx, y + m_cell_h - 3.5f, rw, 1.0f), fg, 0.0f, 0.0f);
                        ctx.draw_rect(Rect::from_xywh(rx, y + m_cell_h - 1.5f, rw, 1.0f), fg, 0.0f, 0.0f);
                    }
                    if ((cell.attrs & TerminalAttr::Strikethrough) != 0) {
                        ctx.draw_rect(Rect::from_xywh(rx, y + m_cell_h * 0.52f, rw, 1.0f), fg, 0.0f, 0.0f);
                    }
                    if ((cell.attrs & TerminalAttr::Overline) != 0) {
                        ctx.draw_rect(Rect::from_xywh(rx, y + 1.0f, rw, 1.0f), fg, 0.0f, 0.0f);
                    }
                    continue;
                }

                // 宽字符 (CJK/Emoji) 或非 ASCII 符号单格精准定位绘制
                float x = kPadX + static_cast<float>(c) * m_cell_w;
                float cw = static_cast<float>(cell.width) * m_cell_w;
                if (cell.codepoint != ' ' && cell.codepoint != 0) {
                    std::string utf8;
                    TerminalBuffer::append_utf8(utf8, cell.codepoint);
                    ctx.draw_text(utf8, PointF{std::round(x), std::round(y + m_text_y_off)},
                                  m_font_size, fg, bold ? FontWeight::Bold : FontWeight::Regular,
                                  italic, m_font_family);
                }
                if ((cell.attrs & (TerminalAttr::Underline | TerminalAttr::Hyperlink)) != 0) {
                    ctx.draw_rect(Rect::from_xywh(x, y + m_cell_h - 2.0f, cw, 1.0f), fg, 0.0f, 0.0f);
                }
                if ((cell.attrs & TerminalAttr::DoubleUnderline) != 0) {
                    ctx.draw_rect(Rect::from_xywh(x, y + m_cell_h - 3.5f, cw, 1.0f), fg, 0.0f, 0.0f);
                    ctx.draw_rect(Rect::from_xywh(x, y + m_cell_h - 1.5f, cw, 1.0f), fg, 0.0f, 0.0f);
                }
                if ((cell.attrs & TerminalAttr::Strikethrough) != 0) {
                    ctx.draw_rect(Rect::from_xywh(x, y + m_cell_h * 0.52f, cw, 1.0f), fg, 0.0f, 0.0f);
                }
                if ((cell.attrs & TerminalAttr::Overline) != 0) {
                    ctx.draw_rect(Rect::from_xywh(x, y + 1.0f, cw, 1.0f), fg, 0.0f, 0.0f);
                }
                ++c;
            }
        }

        // 3. 光标与 IME 组合态预编辑绘制
        const int cursor_abs_r = static_cast<int>(m_buffer.scrollback_count()) + m_buffer.cursor_row();
        if (m_buffer.cursor_visible() && cursor_abs_r >= first_r && cursor_abs_r < first_r + vis_rows) {
            float cx = kPadX + static_cast<float>(m_buffer.cursor_col()) * m_cell_w;
            float cy = kPadY + static_cast<float>(cursor_abs_r - first_r) * m_cell_h;

            if (!m_ime_preedit.empty()) {
                Vec2 psz = TextMeasure::measure(m_ime_preedit, m_font_size, FontWeight::Regular, false, m_font_family);
                Rect pr = Rect::from_xywh(cx, cy, std::max(m_cell_w, psz.x + 6.0f), m_cell_h);
                ctx.draw_rect(pr, m_scheme.background, 2.0f, 0.0f);
                ctx.draw_text(m_ime_preedit, PointF{std::round(cx + 2.0f), std::round(cy + m_text_y_off)},
                              m_font_size, m_scheme.foreground, FontWeight::Regular, false, m_font_family);
                ctx.draw_rect(Rect::from_xywh(cx, cy + m_cell_h - 2.0f, pr.width(), 1.5f), theme.colors.primary, 0.0f, 0.0f);
            } else if (!m_focused) {
                // 失焦时绘制 Windows Terminal 标志性空心光标框
                ctx.draw_rect(Rect::from_xywh(cx, cy + 1.0f, m_cell_w, m_cell_h - 2.0f),
                              Color{m_scheme.cursor_color.r, m_scheme.cursor_color.g, m_scheme.cursor_color.b, 0.55f},
                              0.0f, 1.0f);
            } else if (m_cursor_blink_on || !m_buffer.cursor_blink()) {
                TerminalCursorStyle st = m_buffer.has_custom_cursor_style() ? m_buffer.vt_cursor_style() : m_cursor_style;
                Color cc = m_scheme.cursor_color;
                switch (st) {
                    case TerminalCursorStyle::Bar:
                        ctx.draw_rect(Rect::from_xywh(cx, cy + 1.0f, 2.0f, m_cell_h - 2.0f), cc, 0.5f, 0.0f);
                        break;
                    case TerminalCursorStyle::Vintage:
                        ctx.draw_rect(Rect::from_xywh(cx, cy + m_cell_h - 3.0f, m_cell_w, 2.5f), cc, 0.0f, 0.0f);
                        break;
                    case TerminalCursorStyle::DoubleUnderscore:
                        ctx.draw_rect(Rect::from_xywh(cx, cy + m_cell_h - 4.5f, m_cell_w, 1.5f), cc, 0.0f, 0.0f);
                        ctx.draw_rect(Rect::from_xywh(cx, cy + m_cell_h - 2.0f, m_cell_w, 1.5f), cc, 0.0f, 0.0f);
                        break;
                    case TerminalCursorStyle::EmptyBox:
                        ctx.draw_rect(Rect::from_xywh(cx, cy + 1.0f, m_cell_w, m_cell_h - 2.0f), cc, 0.0f, 1.2f);
                        break;
                    case TerminalCursorStyle::FilledBox:
                        ctx.draw_rect(Rect::from_xywh(cx, cy + 1.0f, m_cell_w, m_cell_h - 2.0f),
                                      Color{cc.r, cc.g, cc.b, 0.80f}, 0.0f, 0.0f);
                        break;
                }
            }
        }

        // 4. Windows Terminal OSC 9;4 顶部进度条指示器
        const TerminalProgress prog = m_buffer.progress();
        if (prog.state != TerminalProgressState::Hidden) {
            Color pc = theme.colors.primary;
            if (prog.state == TerminalProgressState::Error) pc = Color{0.90f, 0.24f, 0.24f, 1.0f};
            else if (prog.state == TerminalProgressState::Paused) pc = Color{0.96f, 0.72f, 0.14f, 1.0f};
            float frac = (prog.state == TerminalProgressState::Indeterminate) ? 0.35f : (static_cast<float>(prog.value) / 100.0f);
            ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, m_size.x * frac, 2.5f), pc, 0.0f, 0.0f);
        }

        // 5. 复用 WinUIScrollBar 绘制回滚历史滚动条 (严禁自绘滚动条)
        const float content_h = total_content_height();
        if (m_scrollbar.update_animations(m_scrollbar.is_hovered() || m_scrollbar.is_dragging())) {
            mark_needs_paint();
        }
        m_scrollbar.paint(ctx, b, content_h, m_scroll_y, theme);

        // 6. Ctrl+Shift+F 顶部浮动内联搜索栏 (完全复用真实 TextBox / Text / ToggleButton / Button 控件)
        if (m_find_open && m_find_bar_node) {
            Rect fbr = Rect::from_xywh(m_find_bar_node->offset().x, m_find_bar_node->offset().y,
                                       m_find_bar_node->size().x, m_find_bar_node->size().y);
            ctx.draw_elevation_shadow(fbr, 6.0f, theme.is_dark, 16.0f, 0.85f);
            PaintContext::OffsetScope scope(ctx, m_find_bar_node->offset());
            m_find_bar_node->paint(ctx);
        }

        ctx.pop_clip();

        // 7. 常规细边框 (无蓝框焦点边框，与 Windows Terminal 视觉一致)
        ctx.draw_rect(b, bd, 6.0f, 1.0f);
    }

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

    void ensure_find_bar() {
        if (m_find_bar_node) return;
        const Theme& t = ThemeContext::current();
        Color surf = t.is_dark ? Color{0x2C / 255.0f, 0x2C / 255.0f, 0x2C / 255.0f, 0.98f}
                               : Color{0xF9 / 255.0f, 0xF9 / 255.0f, 0xF9 / 255.0f, 0.98f};
        Color bd = t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.14f}
                             : Color{0.0f, 0.0f, 0.0f, 0.12f};
        WidgetPtr bar = Box {
            .width = 360.0f,
            .background_color = surf,
            .border_color = bd,
            .border_width = 1.0f,
            .corner_radius = 8.0f,
            .padding = EdgeInsets(6.0f, 8.0f),
            .child = HStack {
                .align_items = AlignItems::Center,
                .spacing = 4.0f,
                .content = {
                    TextBox {
                        .placeholder = "查找...",
                        .on_change = [this](const std::string& q) {
                            m_find_query = q;
                            run_find(true);
                            mark_needs_paint();
                        },
                        .on_submit = [this](const std::string&) {
                            step_find(+1);
                        }
                    }.bind_two_way(m_find_query_prop),
                    Text { .font_size = 11.5f }.bind(&RenderText::set_text, m_find_count_prop),
                    Button {
                        .text = "Aa",
                        .variant = ButtonVariant::Subtle,
                        .height = 30.0f,
                        .padding = EdgeInsets(0.0f, 8.0f),
                        .on_click = [this]() {
                            m_find_case = !m_find_case;
                            m_find_case_variant.set(m_find_case ? ButtonVariant::Accent : ButtonVariant::Subtle);
                            run_find(true);
                            mark_needs_paint();
                        }
                    }.bind(&RenderButton::set_variant, m_find_case_variant),
                    Button { .text = "↑", .variant = ButtonVariant::Subtle, .height = 30.0f, .padding = EdgeInsets(0.0f, 8.0f), .on_click = [this]() { step_find(-1); } },
                    Button { .text = "↓", .variant = ButtonVariant::Subtle, .height = 30.0f, .padding = EdgeInsets(0.0f, 8.0f), .on_click = [this]() { step_find(+1); } },
                    Button { .text = "×", .variant = ButtonVariant::Subtle, .height = 30.0f, .padding = EdgeInsets(0.0f, 8.0f), .on_click = [this]() { toggle_find(); } }
                }
            }
        };
        m_find_bar_node = bar->create_render_node();
        if (m_find_bar_node) {
            if (RenderBox* tb = find_focusable_text_input(m_find_bar_node.get())) {
                tb->flex_data().flex_grow = 1.0f;
            }
            m_find_bar_node->on_pointer_down = [](PointerEvent& e) { e.stop_propagation(); };
            m_find_bar_node->on_pointer_move = [](PointerEvent& e) { e.stop_propagation(); };
            m_find_bar_node->on_pointer_up   = [](PointerEvent& e) { e.stop_propagation(); };
            append_child(m_find_bar_node.get());
        }
    }

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
    void show_context_menu(PointF global_pos) {
        Window* win = Window::active_window();
        if (!win) return;
        Vec2 wp{global_pos.x, global_pos.y + win->titlebar_height()};

        std::vector<MenuFlyoutItem> items = {
            MenuFlyoutItem{
                .label = "复制",
                .shortcut = "Ctrl+Shift+C",
                .disabled = !m_has_selection,
                .on_click = [this]() { copy_selection(); }
            },
            MenuFlyoutItem{
                .label = "粘贴",
                .shortcut = "Ctrl+Shift+V",
                .on_click = [this]() { paste_from_clipboard(); }
            },
            MenuFlyoutItem{
                .label = "全选缓冲区",
                .on_click = [this]() {
                    m_has_selection = true;
                    m_sel_block = false;
                    m_sel_start = TerminalCoord{0, 0};
                    m_sel_end = TerminalCoord{m_buffer.cols() - 1, std::max(0, m_buffer.total_rows() - 1)};
                    mark_needs_paint();
                }
            },
            MenuFlyoutItem::separator(),
            MenuFlyoutItem{
                .label = "查找...",
                .shortcut = "Ctrl+Shift+F",
                .on_click = [this]() { toggle_find(); }
            },
            MenuFlyoutItem{
                .label = "清空缓冲区",
                .on_click = [this]() { clear(); }
            },
            MenuFlyoutItem::separator(),
            MenuFlyoutItem{
                .label = "光标样式",
                .sub_items = {
                    MenuFlyoutItem{ .label = "竖线 (Bar |)", .checked = (m_cursor_style == TerminalCursorStyle::Bar), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::Bar); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "实心方块 (FilledBox)", .checked = (m_cursor_style == TerminalCursorStyle::FilledBox), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::FilledBox); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "下划线 (Vintage _)", .checked = (m_cursor_style == TerminalCursorStyle::Vintage), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::Vintage); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "空心方块 (EmptyBox)", .checked = (m_cursor_style == TerminalCursorStyle::EmptyBox), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::EmptyBox); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "双下划线 (DoubleUnderscore)", .checked = (m_cursor_style == TerminalCursorStyle::DoubleUnderscore), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::DoubleUnderscore); }, .is_radio = true }
                }
            },
            MenuFlyoutItem{
                .label = "配色方案",
                .sub_items = {
                    MenuFlyoutItem{ .label = "Campbell (WinTerminal 默认)", .checked = (m_scheme.name == "Campbell"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::campbell()); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "One Half Dark", .checked = (m_scheme.name == "One Half Dark"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::one_half_dark()); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "One Half Light", .checked = (m_scheme.name == "One Half Light"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::one_half_light()); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "Solarized Dark", .checked = (m_scheme.name == "Solarized Dark"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::solarized_dark()); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "Tango Dark", .checked = (m_scheme.name == "Tango Dark"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::tango_dark()); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "Vintage", .checked = (m_scheme.name == "Vintage"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::vintage()); }, .is_radio = true }
                }
            },
            MenuFlyoutItem::separator(),
            MenuFlyoutItem{ .label = "放大字号", .shortcut = "Ctrl++", .on_click = [this]() { set_font_size(m_font_size + 1.5f); } },
            MenuFlyoutItem{ .label = "缩小字号", .shortcut = "Ctrl+-", .on_click = [this]() { set_font_size(m_font_size - 1.5f); } },
            MenuFlyoutItem{ .label = "重置缩放", .shortcut = "Ctrl+0", .on_click = [this]() { set_font_size(m_default_font_size); } }
        };
        MenuFlyout::show_at(std::move(items), wp);
    }

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
    void setup_behavior() {
        on_focus = [this](FocusEvent& e) {
            m_focused = e.gained_focus;
            if (m_focused) reset_blink();
            else m_ime_preedit.clear();
            if (m_buffer.focus_reporting()) {
                send_to_backend(m_focused ? "\x1b[I" : "\x1b[O");
            }
            mark_needs_paint();
        };

        on_pointer_down = [this](PointerEvent& e) {
            // 优先交给 WinUIScrollBar 处理（即使 TUI 开启鼠标模式，右侧滚动条仍可拖拽）
            if (e.button == PointerButton::Primary &&
                m_scrollbar.handle_pointer_down_direct(e.local_position, bounds(), total_content_height(),
                                                       m_scroll_y, max_scroll_y(), m_cell_h * 3.0f)) {
                clamp_scroll();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            // TUI 应用启用鼠标追踪 (?1000/?1002/?1003)：直接向后端发送 VT/SGR 鼠标按下序列
            if (should_report_mouse(e.modifiers) && e.button != PointerButton::None) {
                int btn = (e.button == PointerButton::Primary) ? 0
                        : (e.button == PointerButton::Auxiliary ? 1 : 2);
                auto [c1, r1] = point_to_vt_cell_1based(e.local_position);
                m_vt_mouse_button = btn;
                m_vt_last_col = c1;
                m_vt_last_row = r1;
                send_vt_mouse(btn, c1, r1, false, e.modifiers);
                e.stop_propagation();
                return;
            }

            if (e.button == PointerButton::Secondary) {
                show_context_menu(e.position);
                e.stop_propagation();
                return;
            }

            if (e.button == PointerButton::Primary) {
                TerminalCoord pos = point_to_coord(e.local_position);
                auto now = std::chrono::steady_clock::now();
                auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
                if (dt_ms < 400 && pos == m_last_click_coord) {
                    m_click_streak = (m_click_streak % 3) + 1;
                } else {
                    m_click_streak = 1;
                }
                m_last_click_time = now;
                m_last_click_coord = pos;

                if (m_click_streak == 2) {
                    select_word_at(pos);
                    m_dragging_sel = false;
                } else if (m_click_streak == 3) {
                    m_has_selection = true;
                    m_sel_block = false;
                    m_sel_start = TerminalCoord{0, pos.row};
                    m_sel_end = TerminalCoord{m_buffer.cols() - 1, pos.row};
                    m_dragging_sel = false;
                } else {
                    m_has_selection = false;
                    m_dragging_sel = true;
                    m_sel_block = e.modifiers.alt;
                    m_sel_start = pos;
                    m_sel_end = pos;
                }
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (e.type == PlatformEventType::PointerScroll) {
                // KTUI Win32 PAL 约定：delta.y < 0 为向上滚，delta.y > 0 为向下滚，每格约 ±36px
                const float dy = (std::abs(e.delta.y) > 0.001f) ? e.delta.y : e.delta.x;
                if (e.modifiers.ctrl) {
                    set_font_size(m_font_size + (dy < 0.0f ? 1.5f : -1.5f));
                    e.stop_propagation();
                    return;
                }
                // TUI 鼠标追踪模式下滚轮转为 VT 滚轮事件 (64=上滚, 65=下滚)
                if (should_report_mouse(e.modifiers)) {
                    auto [c1, r1] = point_to_vt_cell_1based(e.local_position);
                    int wheel_cb = (dy < 0.0f) ? 64 : 65;
                    send_vt_mouse(wheel_cb, c1, r1, false, e.modifiers);
                    e.stop_propagation();
                    return;
                }
                // 标准 Windows Terminal 滚动手感：每格滚轮精确滚动 3 行
                float notches = dy / 36.0f;
                float delta_px = notches * m_cell_h * 3.0f;
                m_scroll_y = std::clamp(m_scroll_y + delta_px, 0.0f, max_scroll_y());
                clamp_scroll();
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            bool sb_consumed = false;
            if (m_scrollbar.handle_pointer_move_direct(e.local_position, bounds(), total_content_height(),
                                                       m_scroll_y, max_scroll_y(), sb_consumed)) {
                clamp_scroll();
                mark_needs_paint();
            }
            if (sb_consumed) {
                cursor_shape = CursorShape::Arrow;
                e.stop_propagation();
                return;
            }
            const int mouse_mode = m_buffer.mouse_tracking_mode();
            cursor_shape = (mouse_mode != 0 && !e.modifiers.shift)
                ? CursorShape::Arrow
                : CursorShape::IBeam;

            // TUI 鼠标移动追踪 (?1002 按键拖拽移动 / ?1003 全量悬停与拖拽移动)
            if (should_report_mouse(e.modifiers) &&
                ((mouse_mode == 1003) || (mouse_mode == 1002 && m_vt_mouse_button >= 0))) {
                auto [c1, r1] = point_to_vt_cell_1based(e.local_position);
                if (c1 != m_vt_last_col || r1 != m_vt_last_row) {
                    m_vt_last_col = c1;
                    m_vt_last_row = r1;
                    int cb = (m_vt_mouse_button >= 0) ? (m_vt_mouse_button | 32) : 35;
                    send_vt_mouse(cb, c1, r1, false, e.modifiers);
                }
                e.stop_propagation();
                return;
            }

            if (m_dragging_sel) {
                TerminalCoord cur = point_to_coord(e.local_position);
                if (cur != m_sel_end) {
                    m_sel_end = cur;
                    m_has_selection = (m_sel_start != m_sel_end);
                    mark_needs_paint();
                }
                e.stop_propagation();
            }
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (m_scrollbar.handle_pointer_up()) {
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (m_vt_mouse_button >= 0) {
                int btn = m_vt_mouse_button;
                m_vt_mouse_button = -1;
                if (m_buffer.mouse_tracking_mode() != 0) {
                    auto [c1, r1] = point_to_vt_cell_1based(e.local_position);
                    send_vt_mouse(btn, c1, r1, true, e.modifiers);
                }
                e.stop_propagation();
                return;
            }
            if (m_dragging_sel) {
                m_dragging_sel = false;
                if (m_copy_on_select && m_has_selection) {
                    copy_selection();
                }
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };

        on_key = [this](KeyEvent& e) {
            if (e.type != PlatformEventType::KeyDown) return;

            const uint32_t vk = e.key_code;
            const bool ctrl = e.modifiers.ctrl;
            const bool shift = e.modifiers.shift;
            const bool alt = e.modifiers.alt;

            // Windows Terminal 全局快捷键
            if (ctrl && shift && vk == 'F') {
                toggle_find();
                e.stop_propagation();
                return;
            }
            if (ctrl && shift && vk == 'C') {
                copy_selection();
                e.stop_propagation();
                return;
            }
            if (ctrl && shift && vk == 'V') {
                paste_from_clipboard();
                e.stop_propagation();
                return;
            }
            if (ctrl && !alt && (vk == 0xBB /* VK_OEM_PLUS */ || vk == 0x6B /* VK_ADD */)) {
                set_font_size(m_font_size + 1.5f);
                e.stop_propagation();
                return;
            }
            if (ctrl && !alt && (vk == 0xBD /* VK_OEM_MINUS */ || vk == 0x6D /* VK_SUBTRACT */)) {
                set_font_size(m_font_size - 1.5f);
                e.stop_propagation();
                return;
            }
            if (ctrl && !alt && (vk == '0' || vk == 0x60)) {
                set_font_size(m_default_font_size);
                e.stop_propagation();
                return;
            }

            // 查找栏激活态拦截按键
            if (m_find_open) {
                if (vk == 0x1B) { // Escape
                    toggle_find();
                } else if (vk == 0x0D) { // Enter
                    step_find(shift ? -1 : +1);
                } else if (vk == 0x08) { // Backspace
                    if (!m_find_query.empty()) {
                        size_t pop_n = 1;
                        while (pop_n < m_find_query.size() &&
                               (static_cast<unsigned char>(m_find_query[m_find_query.size() - pop_n]) & 0xC0u) == 0x80u) {
                            ++pop_n;
                        }
                        m_find_query.resize(m_find_query.size() - pop_n);
                        run_find(true);
                        mark_needs_paint();
                    }
                }
                e.stop_propagation();
                return;
            }

            // Shift+PgUp / Shift+PgDn 翻页浏览历史回滚
            if (shift && !ctrl && vk == 0x21) { // PageUp
                m_scroll_y = std::max(0.0f, m_scroll_y - m_size.y * 0.85f);
                clamp_scroll();
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (shift && !ctrl && vk == 0x22) { // PageDown
                m_scroll_y = std::min(max_scroll_y(), m_scroll_y + m_size.y * 0.85f);
                clamp_scroll();
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            // 编码标准 VT100 / xterm 序列发给后端
            std::string seq = encode_vt_key(vk, ctrl, shift, alt, m_buffer.app_cursor_keys());
            if (!seq.empty()) {
                clear_selection();
                send_to_backend(seq);
                e.stop_propagation();
            }
        };
    }

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
    [[nodiscard]] static std::string encode_vt_key(uint32_t vk, bool ctrl, bool shift, bool alt, bool app_cursor) {
        int mod = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0);
        auto csi_dir = [&](char letter) -> std::string {
            if (mod > 1) return "\x1b[1;" + std::to_string(mod) + letter;
            return std::string(app_cursor ? "\x1bO" : "\x1b[") + letter;
        };
        auto csi_tilde = [&](int code) -> std::string {
            if (mod > 1) return "\x1b[" + std::to_string(code) + ";" + std::to_string(mod) + "~";
            return "\x1b[" + std::to_string(code) + "~";
        };

        switch (vk) {
            case 0x0D: return "\r";                            // Enter
            case 0x08: return ctrl ? "\x08" : "\x7f";          // Backspace
            case 0x09: return shift ? "\x1b[Z" : "\t";         // Tab / Shift+Tab
            case 0x1B: return "\x1b";                          // Escape
            case 0x26: return csi_dir('A');                    // Up
            case 0x28: return csi_dir('B');                    // Down
            case 0x27: return csi_dir('C');                    // Right
            case 0x25: return csi_dir('D');                    // Left
            case 0x24: return csi_dir('H');                    // Home
            case 0x23: return csi_dir('F');                    // End
            case 0x2D: return csi_tilde(2);                    // Insert
            case 0x2E: return csi_tilde(3);                    // Delete
            case 0x21: return csi_tilde(5);                    // PageUp
            case 0x22: return csi_tilde(6);                    // PageDown
            case 0x70: return mod > 1 ? ("\x1b[1;" + std::to_string(mod) + "P") : "\x1bOP"; // F1
            case 0x71: return mod > 1 ? ("\x1b[1;" + std::to_string(mod) + "Q") : "\x1bOQ"; // F2
            case 0x72: return mod > 1 ? ("\x1b[1;" + std::to_string(mod) + "R") : "\x1bOR"; // F3
            case 0x73: return mod > 1 ? ("\x1b[1;" + std::to_string(mod) + "S") : "\x1bOS"; // F4
            case 0x74: return csi_tilde(15); // F5
            case 0x75: return csi_tilde(17); // F6
            case 0x76: return csi_tilde(18); // F7
            case 0x77: return csi_tilde(19); // F8
            case 0x78: return csi_tilde(20); // F9
            case 0x79: return csi_tilde(21); // F10
            case 0x7A: return csi_tilde(23); // F11
            case 0x7B: return csi_tilde(24); // F12
            default: break;
        }

        // Ctrl+A .. Ctrl+Z -> 0x01 .. 0x1A
        if (ctrl && !alt && vk >= 'A' && vk <= 'Z') {
            char c = static_cast<char>(vk - 'A' + 1);
            return std::string(1, c);
        }
        // Alt+字母 -> ESC + letter
        if (alt && !ctrl && vk >= 'A' && vk <= 'Z') {
            char c = static_cast<char>(shift ? vk : (vk - 'A' + 'a'));
            return std::string("\x1b") + c;
        }
        return {};
    }

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

// ==================== TerminalController 内联方法实现 ====================
inline void TerminalController::set_backend(std::shared_ptr<ITerminalBackend> backend) {
    if (auto n = m_node.lock()) n->set_backend(std::move(backend));
}
inline void TerminalController::write_input(std::string_view utf8_bytes) {
    if (auto n = m_node.lock()) n->write_input(utf8_bytes);
}
inline void TerminalController::feed_output(std::string_view vt_bytes) {
    if (auto n = m_node.lock()) n->feed_output(vt_bytes);
}
inline void TerminalController::clear() {
    if (auto n = m_node.lock()) n->clear();
}
inline void TerminalController::set_color_scheme(TerminalColorScheme scheme) {
    if (auto n = m_node.lock()) n->set_color_scheme(std::move(scheme));
}
inline void TerminalController::set_cursor_style(TerminalCursorStyle style) {
    if (auto n = m_node.lock()) n->set_cursor_style(style);
}
inline void TerminalController::set_font_size(float pt) {
    if (auto n = m_node.lock()) n->set_font_size(pt);
}
inline void TerminalController::toggle_find() {
    if (auto n = m_node.lock()) n->toggle_find();
}
inline std::string TerminalController::title() const {
    if (auto n = m_node.lock()) return n->buffer().title();
    return {};
}
inline std::string TerminalController::backend_name() const {
    if (auto n = m_node.lock()) return n->backend_name();
    return {};
}
inline std::pair<int, int> TerminalController::grid_size() const {
    if (auto n = m_node.lock()) return {n->buffer().cols(), n->buffer().rows()};
    return {0, 0};
}

// ==================== 声明式 Terminal Widget ====================
struct Terminal {
    std::shared_ptr<ITerminalBackend> backend{nullptr}; // nullptr 时默认内置 DemoShellBackend；可传 ConPtyTerminalBackend / CallbackTerminalBackend / 任意自定义后端
    std::shared_ptr<TerminalController> controller{nullptr};
    TerminalColorScheme color_scheme{TerminalColorScheme::campbell()};
    TerminalCursorStyle cursor_style{TerminalCursorStyle::Bar};
    float font_size{13.5f};
    size_t max_scrollback{9001};
    bool copy_on_select{false};
    float flex_grow{0.0f};
    Fn<void(const std::string&)> on_title_changed{nullptr};
    Fn<void(int)> on_exit{nullptr};

    operator WidgetPtr() const;
};

class TerminalWidget : public Widget {
public:
    explicit TerminalWidget(Terminal desc) : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTerminal>(
            m_desc.backend,
            m_desc.color_scheme,
            m_desc.cursor_style,
            m_desc.font_size,
            m_desc.max_scrollback,
            m_desc.copy_on_select,
            m_desc.on_title_changed,
            m_desc.on_exit);
        if (m_desc.flex_grow > 0.0f) {
            node->flex_data().flex_grow = m_desc.flex_grow;
        }
        if (m_desc.controller) {
            m_desc.controller->bind(node);
        }
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto t = std::static_pointer_cast<RenderTerminal>(node)) {
            t->set_color_scheme(m_desc.color_scheme);
            t->set_cursor_style(m_desc.cursor_style);
            t->set_font_size(m_desc.font_size);
            t->set_copy_on_select(m_desc.copy_on_select);
            if (m_desc.controller) {
                m_desc.controller->bind(t);
            }
        }
    }

private:
    Terminal m_desc;
};

inline Terminal::operator WidgetPtr() const {
    return make_widget<TerminalWidget>(*this);
}

} // namespace ktui
