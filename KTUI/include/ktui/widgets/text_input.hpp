#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <optional>
#include <string>
#include <utility>
#include <algorithm>
#include <vector>
#include <chrono>

namespace ktui {

// 文本编辑撤销重做快照
struct UndoRecord {
    std::string text;
    size_t cursor_index{0};
    size_t selection_anchor{0};
};

// UTF-8 码点边界安全工具
namespace utf8_util {
    inline size_t prev_char_pos(const std::string& s, size_t byte_pos) noexcept {
        if (byte_pos == 0 || s.empty()) return 0;
        size_t pos = std::min(byte_pos, s.size()) - 1;
        while (pos > 0 && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) {
            --pos;
        }
        return pos;
    }

    inline size_t next_char_pos(const std::string& s, size_t byte_pos) noexcept {
        if (byte_pos >= s.size()) return s.size();
        size_t pos = byte_pos + 1;
        while (pos < s.size() && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) {
            ++pos;
        }
        return pos;
    }

    inline float estimate_width(std::string_view s, float font_size) noexcept {
        float w = 0.0f;
        size_t i = 0;
        while (i < s.size()) {
            uint8_t b = static_cast<uint8_t>(s[i]);
            if (b < 0x80) {
                w += font_size * 0.52f;
                i += 1;
            } else {
                w += font_size * 1.0f; // CJK 全角估算
                if ((b & 0xE0) == 0xC0) i += 2;
                else if ((b & 0xF0) == 0xE0) i += 3;
                else if ((b & 0xF8) == 0xF0) i += 4;
                else i += 1;
            }
        }
        return w;
    }
}

// 桌面紧凑单行文本输入框渲染盒 (RenderTextInput)
class RenderTextInput : public RenderBox, public ITextInputClient {
public:
    RenderTextInput() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::IBeam;
        setup_behavior();
    }

    ~RenderTextInput() override = default;

    void set_properties(std::string text, std::string placeholder,
                        float h, float radius, EdgeInsets pad,
                        Color bg, Color border_normal, Color border_focus,
                        Color txt_col, Color ph_col, float font_sz,
                        std::function<void(const std::string&)> on_ch,
                        std::function<void(const std::string&)> on_sub) {
        if (m_text != text) {
            m_text = std::move(text);
            m_cursor_index = std::min(m_cursor_index, m_text.length());
            m_selection_anchor = std::min(m_selection_anchor, m_text.length());
        }
        m_placeholder = std::move(placeholder);
        m_height = h;
        m_corner_radius = radius;
        m_padding = pad;
        m_bg_color = bg;
        m_border_normal = border_normal;
        m_border_focus = border_focus;
        m_text_color = txt_col;
        m_placeholder_color = ph_col;
        m_font_size = font_sz;
        m_on_change = std::move(on_ch);
        m_on_submit = std::move(on_sub);

        mark_needs_layout();
    }

    void set_text(std::string text) {
        if (m_text != text) {
            m_text = std::move(text);
            m_cursor_index = std::min(m_cursor_index, m_text.length());
            m_selection_anchor = m_cursor_index;
            ensure_cursor_visible();
            mark_needs_paint();
        }
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] size_t cursor_index() const noexcept { return m_cursor_index; }
    void set_cursor_index(size_t idx) noexcept {
        m_cursor_index = std::min(idx, m_text.length());
        m_selection_anchor = m_cursor_index;
        ensure_cursor_visible();
        mark_needs_paint();
    }

    [[nodiscard]] bool has_selection() const noexcept {
        return m_cursor_index != m_selection_anchor;
    }

    [[nodiscard]] std::pair<size_t, size_t> selection_range() const noexcept {
        return {std::min(m_cursor_index, m_selection_anchor),
                std::max(m_cursor_index, m_selection_anchor)};
    }

    [[nodiscard]] std::string selected_text() const {
        if (!has_selection()) return "";
        auto [s, e] = selection_range();
        return m_text.substr(s, e - s);
    }

    void select_all() noexcept {
        m_selection_anchor = 0;
        m_cursor_index = m_text.length();
        ensure_cursor_visible();
        mark_needs_paint();
    }

    void clear_selection() noexcept {
        if (m_selection_anchor != m_cursor_index) {
            m_selection_anchor = m_cursor_index;
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool is_input_focused() const noexcept { return m_is_focused; }
    void set_input_focused(bool focused) noexcept {
        if (m_is_focused != focused) {
            m_is_focused = focused;
            m_last_activity_time = std::chrono::steady_clock::now();
            if (!m_is_focused) {
                m_selection_anchor = m_cursor_index;
            }
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool is_cursor_blink_visible() const noexcept {
        if (!m_is_focused) return false;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_activity_time).count();
        if (elapsed < 530) return true;
        return ((elapsed / 530) % 2) == 0;
    }

    // ==================== 撤销 / 重做栈 (Undo / Redo) ====================
    void push_undo_snapshot() {
        if (!m_undo_stack.empty() && m_undo_stack.back().text == m_text) {
            return;
        }
        m_undo_stack.push_back({m_text, m_cursor_index, m_selection_anchor});
        if (m_undo_stack.size() > kMaxUndoSteps) {
            m_undo_stack.erase(m_undo_stack.begin());
        }
        m_redo_stack.clear();
    }

    void undo() {
        if (m_undo_stack.empty()) return;
        m_redo_stack.push_back({m_text, m_cursor_index, m_selection_anchor});
        auto rec = m_undo_stack.back();
        m_undo_stack.pop_back();
        m_text = std::move(rec.text);
        m_cursor_index = std::min(rec.cursor_index, m_text.length());
        m_selection_anchor = std::min(rec.selection_anchor, m_text.length());
        m_last_activity_time = std::chrono::steady_clock::now();
        notify_changed();
    }

    void redo() {
        if (m_redo_stack.empty()) return;
        m_undo_stack.push_back({m_text, m_cursor_index, m_selection_anchor});
        auto rec = m_redo_stack.back();
        m_redo_stack.pop_back();
        m_text = std::move(rec.text);
        m_cursor_index = std::min(rec.cursor_index, m_text.length());
        m_selection_anchor = std::min(rec.selection_anchor, m_text.length());
        m_last_activity_time = std::chrono::steady_clock::now();
        notify_changed();
    }

    [[nodiscard]] bool can_undo() const noexcept { return !m_undo_stack.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !m_redo_stack.empty(); }

    static bool is_word_char(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }

    std::pair<size_t, size_t> find_word_boundaries(size_t index) const {
        if (m_text.empty()) return {0, 0};
        size_t idx = std::min(index, m_text.length());
        if (idx == m_text.length() && idx > 0) {
            idx = utf8_util::prev_char_pos(m_text, idx);
        }

        uint8_t b = static_cast<uint8_t>(m_text[idx]);
        if (b >= 0x80) {
            size_t start = idx;
            while (start > 0 && (static_cast<uint8_t>(m_text[start]) & 0xC0) == 0x80) {
                --start;
            }
            size_t end = utf8_util::next_char_pos(m_text, start);
            return {start, end};
        }

        char c = m_text[idx];
        if (is_word_char(c)) {
            size_t start = idx;
            while (start > 0 && is_word_char(m_text[start - 1])) {
                --start;
            }
            size_t end = idx;
            while (end < m_text.size() && is_word_char(m_text[end])) {
                ++end;
            }
            return {start, end};
        } else if (c == ' ' || c == '\t') {
            size_t start = idx;
            while (start > 0 && (m_text[start - 1] == ' ' || m_text[start - 1] == '\t')) {
                --start;
            }
            size_t end = idx;
            while (end < m_text.size() && (m_text[end] == ' ' || m_text[end] == '\t')) {
                ++end;
            }
            return {start, end};
        } else {
            return {idx, std::min(idx + 1, m_text.size())};
        }
    }

    void perform_layout() override {
        // 固定输入框自身布局宽度（优先填满父容器有限约束，否则按占位符或默认 180px，绝不因长文本输入无限撑爆容器！）
        float ph_w = TextMeasure::measure_width(m_placeholder, m_font_size);
        float desired_w = std::max(180.0f, ph_w + m_padding.horizontal() + 20.0f);
        if (m_constraints.max_width < BoxConstraints::kInfinity) {
            desired_w = m_constraints.max_width;
        }
        float desired_h = m_height;

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
        ensure_cursor_visible();
    }

    void paint(PaintContext& context) override {
        // 1. 绘制硬朗微圆角实心背景与 1px 细边框 (聚焦时高亮蓝色)
        Color border_col = m_is_focused ? m_border_focus : (is_hovered ? Color{0.38f, 0.38f, 0.44f, 1.0f} : m_border_normal);
        context.draw_rect(bounds(), m_bg_color, m_corner_radius, 0.0f);
        context.draw_rect(bounds(), border_col, m_corner_radius, 1.0f);

        // 2. 开启内边距视口硬件裁剪 (彻底杜绝长文本或光标溢出到输入框外！)
        float view_x = m_padding.left;
        float view_w = std::max(0.0f, m_size.x - m_padding.horizontal());
        Rect inner_clip = Rect::from_xywh(view_x, 1.0f, view_w, std::max(0.0f, m_size.y - 2.0f));
        context.push_clip(inner_clip, 0.0f);

        float text_x = view_x - m_scroll_x;
        float text_y = std::max(0.0f, (m_size.y - m_font_size * 1.30f) * 0.5f);

        // 2.1 绘制文本选区高亮背景
        if (m_is_focused && has_selection() && m_preedit_text.empty()) {
            auto [sel_start, sel_end] = selection_range();
            float x0 = text_x + TextMeasure::measure_width(std::string_view(m_text).substr(0, sel_start), m_font_size);
            float x1 = text_x + TextMeasure::measure_width(std::string_view(m_text).substr(0, sel_end), m_font_size);
            float sel_h = m_font_size * 1.25f;
            float sel_y = std::max(0.0f, (m_size.y - sel_h) * 0.5f);
            context.draw_rect(Rect::from_xywh(x0, sel_y, std::max(2.0f, x1 - x0), sel_h),
                              Color{0.20f, 0.52f, 0.92f, 0.45f}, 1.0f, 0.0f);
        }

        // 2.2 绘制内文（含 IME 预编辑组合态）或占位提示
        std::string display_text = m_text;
        float preedit_start_x = text_x + TextMeasure::measure_width(std::string_view(m_text).substr(0, m_cursor_index), m_font_size);
        float preedit_w = 0.0f;
        if (!m_preedit_text.empty()) {
            display_text.insert(m_cursor_index, m_preedit_text);
            preedit_w = TextMeasure::measure_width(m_preedit_text, m_font_size);
        }

        if (display_text.empty() && !m_placeholder.empty()) {
            context.draw_text(m_placeholder, PointF{view_x, text_y}, m_font_size, m_placeholder_color);
        } else if (!display_text.empty()) {
            context.draw_text(display_text, PointF{text_x, text_y}, m_font_size, m_text_color);
        }

        // 2.3 若存在 IME 预编辑文本，在其下方绘制强调下划线
        if (!m_preedit_text.empty() && preedit_w > 0.0f) {
            float underline_y = text_y + m_font_size * 1.15f;
            context.draw_rect(Rect::from_xywh(preedit_start_x, underline_y, preedit_w, 1.5f),
                              m_border_focus, 0.0f, 0.0f);
        }

        // 3. 聚焦且光标处于闪烁可见周期时绘制紧凑光标竖线 (530ms 周期)
        if (m_is_focused && is_cursor_blink_visible()) {
            float cursor_x = preedit_start_x + preedit_w;
            float cursor_h = m_font_size * 1.15f;
            float cursor_y = std::max(0.0f, (m_size.y - cursor_h) * 0.5f);

            context.draw_rect(Rect::from_xywh(cursor_x, cursor_y, 1.5f, cursor_h),
                              m_text_color, 0.0f, 0.0f);
        }

        context.pop_clip();
    }

    // ==================== ITextInputClient 框架级 IME 协议实现 ====================
    [[nodiscard]] bool accepts_text_input() const noexcept override { return true; }

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        Vec2 abs = global_offset();
        float view_x = m_padding.left;
        float view_w = std::max(0.0f, m_size.x - m_padding.horizontal());
        float prefix_w = TextMeasure::measure_width(std::string_view(m_text).substr(0, m_cursor_index), m_font_size);
        float preedit_w = TextMeasure::measure_width(m_preedit_text, m_font_size);
        float local_cx = std::clamp(view_x - m_scroll_x + prefix_w + preedit_w, view_x, view_x + view_w);
        float cursor_h = m_font_size * 1.2f;
        float cursor_y = std::max(0.0f, (m_size.y - cursor_h) * 0.5f);
        return Rect::from_xywh(abs.x + local_cx, abs.y + cursor_y, 2.0f, cursor_h);
    }

    void on_ime_preedit(const std::string& text, size_t /*cursor_byte_pos*/) override {
        if (!text.empty() && has_selection()) {
            delete_selection_internal();
        }
        m_preedit_text = text;
        ensure_cursor_visible();
        mark_needs_paint();
    }

    void on_ime_commit(const std::string& utf8_text) override {
        m_preedit_text.clear();
        if (!utf8_text.empty()) {
            append_chars(utf8_text);
        } else {
            ensure_cursor_visible();
            mark_needs_paint();
        }
    }

    // 外部或测试注入按键处理 (完整支持选区、Ctrl+A/C/X/V 剪贴板、Shift+导航)
    void handle_text_key(const KeyEvent& ev) {
        if (!m_is_focused) return;

        if (ev.type == PlatformEventType::KeyDown) {
            constexpr uint32_t kKeyBackspace = 8;
            constexpr uint32_t kKeyEnter = 13;
            constexpr uint32_t kKeyDelete = 46;
            constexpr uint32_t kKeyLeft = 37;
            constexpr uint32_t kKeyRight = 39;
            constexpr uint32_t kKeyHome = 36;
            constexpr uint32_t kKeyEnd = 35;

            // Ctrl 组合快捷键: Ctrl+A (全选), Ctrl+C (复制), Ctrl+X (剪切), Ctrl+V (粘贴), Ctrl+Z (撤销), Ctrl+Y / Ctrl+Shift+Z (重做)
            if (ev.modifiers.ctrl) {
                uint32_t kc = ev.key_code;
                if (kc == 'A' || kc == 'a') {
                    select_all();
                    return;
                } else if (kc == 'C' || kc == 'c') {
                    if (has_selection()) {
                        Clipboard::set_text(selected_text());
                    }
                    return;
                } else if (kc == 'X' || kc == 'x') {
                    if (has_selection()) {
                        push_undo_snapshot();
                        Clipboard::set_text(selected_text());
                        delete_selection_internal();
                        notify_changed();
                    }
                    return;
                } else if (kc == 'V' || kc == 'v') {
                    std::string clip = Clipboard::get_text();
                    // 过滤单行输入框不支持的回车换行符
                    std::string clean;
                    clean.reserve(clip.size());
                    for (char c : clip) {
                        if (c != '\r' && c != '\n') clean.push_back(c);
                    }
                    if (!clean.empty()) {
                        push_undo_snapshot();
                        append_chars(clean);
                    }
                    return;
                } else if (kc == 'Z' || kc == 'z') {
                    if (ev.modifiers.shift) {
                        redo();
                    } else {
                        undo();
                    }
                    return;
                } else if (kc == 'Y' || kc == 'y') {
                    redo();
                    return;
                }
            }

            if (ev.key_code == kKeyBackspace) {
                if (has_selection()) {
                    push_undo_snapshot();
                    delete_selection_internal();
                    notify_changed();
                } else if (m_cursor_index > 0 && !m_text.empty()) {
                    push_undo_snapshot();
                    size_t prev = utf8_util::prev_char_pos(m_text, m_cursor_index);
                    m_text.erase(prev, m_cursor_index - prev);
                    m_cursor_index = prev;
                    m_selection_anchor = m_cursor_index;
                    notify_changed();
                }
            } else if (ev.key_code == kKeyDelete) {
                if (has_selection()) {
                    push_undo_snapshot();
                    delete_selection_internal();
                    notify_changed();
                } else if (m_cursor_index < m_text.length()) {
                    push_undo_snapshot();
                    size_t next = utf8_util::next_char_pos(m_text, m_cursor_index);
                    m_text.erase(m_cursor_index, next - m_cursor_index);
                    m_selection_anchor = m_cursor_index;
                    notify_changed();
                }
            } else if (ev.key_code == kKeyLeft) {
                if (has_selection() && !ev.modifiers.shift) {
                    m_cursor_index = selection_range().first;
                    m_selection_anchor = m_cursor_index;
                } else if (m_cursor_index > 0) {
                    m_cursor_index = utf8_util::prev_char_pos(m_text, m_cursor_index);
                    if (!ev.modifiers.shift) m_selection_anchor = m_cursor_index;
                }
                ensure_cursor_visible();
                mark_needs_paint();
            } else if (ev.key_code == kKeyRight) {
                if (has_selection() && !ev.modifiers.shift) {
                    m_cursor_index = selection_range().second;
                    m_selection_anchor = m_cursor_index;
                } else if (m_cursor_index < m_text.length()) {
                    m_cursor_index = utf8_util::next_char_pos(m_text, m_cursor_index);
                    if (!ev.modifiers.shift) m_selection_anchor = m_cursor_index;
                }
                ensure_cursor_visible();
                mark_needs_paint();
            } else if (ev.key_code == kKeyHome) {
                m_cursor_index = 0;
                if (!ev.modifiers.shift) m_selection_anchor = m_cursor_index;
                ensure_cursor_visible();
                mark_needs_paint();
            } else if (ev.key_code == kKeyEnd) {
                m_cursor_index = m_text.length();
                if (!ev.modifiers.shift) m_selection_anchor = m_cursor_index;
                ensure_cursor_visible();
                mark_needs_paint();
            } else if (ev.key_code == kKeyEnter) {
                if (m_on_submit) {
                    m_on_submit(m_text);
                }
            }
        }
        
        if (ev.type == PlatformEventType::ImeCommit || !ev.text.empty()) {
            if (!ev.text.empty() && !ev.modifiers.ctrl) {
                append_chars(ev.text);
            }
        }
    }

    // 直接键入 UTF-8 字符序列 (若当前存在选区则自动替换选区)
    void append_chars(const std::string& chars) {
        if (chars.empty()) return;
        push_undo_snapshot();
        if (has_selection()) {
            delete_selection_internal();
        }
        m_text.insert(m_cursor_index, chars);
        m_cursor_index += chars.size();
        m_selection_anchor = m_cursor_index;
        m_last_activity_time = std::chrono::steady_clock::now();
        notify_changed();
    }

private:
    void delete_selection_internal() {
        if (!has_selection()) return;
        auto [s, e] = selection_range();
        m_text.erase(s, e - s);
        m_cursor_index = s;
        m_selection_anchor = s;
    }

    void ensure_cursor_visible() {
        float view_w = std::max(0.0f, m_size.x - m_padding.horizontal());
        if (view_w <= 1.0f) {
            m_scroll_x = 0.0f;
            return;
        }
        float cursor_x = TextMeasure::measure_width(std::string_view(m_text).substr(0, m_cursor_index), m_font_size)
                       + TextMeasure::measure_width(m_preedit_text, m_font_size);
        float total_w = TextMeasure::measure_width(m_text, m_font_size)
                      + TextMeasure::measure_width(m_preedit_text, m_font_size);

        constexpr float kMargin = 4.0f;
        if (cursor_x - m_scroll_x > view_w - kMargin) {
            m_scroll_x = cursor_x - view_w + kMargin;
        } else if (cursor_x - m_scroll_x < 0.0f) {
            m_scroll_x = cursor_x;
        }

        // 若删除文本后右侧出现过多空白，自动回拉滚动条
        float max_scroll = std::max(0.0f, total_w - view_w + kMargin);
        m_scroll_x = std::clamp(m_scroll_x, 0.0f, max_scroll);
    }

    void notify_changed() {
        ensure_cursor_visible();
        mark_needs_paint();
        if (m_on_change) {
            m_on_change(m_text);
        }
    }

    void setup_behavior() {
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary) {
                set_input_focused(true);
                m_last_activity_time = std::chrono::steady_clock::now();

                uint64_t now_ms = ev.timestamp_ms;
                if (now_ms == 0) {
                    now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                        m_last_activity_time.time_since_epoch()).count());
                }
                float dist_sq = (ev.position.x - m_last_click_pos.x) * (ev.position.x - m_last_click_pos.x)
                              + (ev.position.y - m_last_click_pos.y) * (ev.position.y - m_last_click_pos.y);
                if (now_ms - m_last_click_time_ms < 400 && dist_sq < 36.0f) {
                    m_click_count = (m_click_count % 3) + 1;
                } else {
                    m_click_count = 1;
                }
                m_last_click_time_ms = now_ms;
                m_last_click_pos = ev.position;

                float click_x = (ev.local_position.x > 0.0f || ev.local_position.y > 0.0f) ? ev.local_position.x : ev.position.x;
                float relative_x = click_x - m_padding.left + m_scroll_x;
                m_cursor_index = TextMeasure::hit_test_cursor(m_text, m_font_size, relative_x);

                if (m_click_count == 2) {
                    // 双击按词分词选中
                    auto [w_start, w_end] = find_word_boundaries(m_cursor_index);
                    m_selection_anchor = w_start;
                    m_cursor_index = w_end;
                } else if (m_click_count == 3) {
                    // 三击全选当前文本
                    select_all();
                } else {
                    if (!ev.modifiers.shift) {
                        m_selection_anchor = m_cursor_index;
                    }
                }
                ensure_cursor_visible();
                mark_needs_paint();
            }
        };

        on_pointer_move = [this](PointerEvent& ev) {
            if (is_active && m_is_focused) {
                m_last_activity_time = std::chrono::steady_clock::now();
                float click_x = (ev.local_position.x != 0.0f || ev.local_position.y != 0.0f) ? ev.local_position.x : ev.position.x;
                float relative_x = click_x - m_padding.left + m_scroll_x;
                size_t new_idx = TextMeasure::hit_test_cursor(m_text, m_font_size, relative_x);
                if (new_idx != m_cursor_index) {
                    m_cursor_index = new_idx;
                    ensure_cursor_visible();
                    mark_needs_paint();
                }
            }
        };

        on_focus = [this](FocusEvent& ev) {
            set_input_focused(ev.gained_focus);
            if (!ev.gained_focus) {
                m_preedit_text.clear();
                m_selection_anchor = m_cursor_index;
            }
        };

        on_key = [this](KeyEvent& ev) {
            m_last_activity_time = std::chrono::steady_clock::now();
            handle_text_key(ev);
        };
    }

    std::string m_text;
    std::string m_preedit_text;
    std::string m_placeholder;
    size_t m_cursor_index{0};
    size_t m_selection_anchor{0};
    float m_scroll_x{0.0f};
    bool m_is_focused{false};

    std::vector<UndoRecord> m_undo_stack;
    std::vector<UndoRecord> m_redo_stack;
    static constexpr size_t kMaxUndoSteps = 100;

    uint64_t m_last_click_time_ms{0};
    Vec2 m_last_click_pos{0.0f, 0.0f};
    int m_click_count{0};
    std::chrono::steady_clock::time_point m_last_activity_time{std::chrono::steady_clock::now()};

    float m_height{26.0f};          // 桌面紧凑输入框标准 26px
    float m_corner_radius{3.0f};    // 桌面精致硬朗微圆角 3px
    EdgeInsets m_padding{EdgeInsets(4.0f, 8.0f)};
    Color m_bg_color{Color{0.14f, 0.14f, 0.16f, 1.0f}};
    Color m_border_normal{Color{0.25f, 0.25f, 0.28f, 1.0f}};
    Color m_border_focus{Color{0.20f, 0.55f, 0.95f, 1.0f}}; // 桌面焦点蓝环
    Color m_text_color{Color{0.92f, 0.92f, 0.95f, 1.0f}};
    Color m_placeholder_color{Color{0.45f, 0.45f, 0.48f, 1.0f}};
    float m_font_size{13.0f};

    std::function<void(const std::string&)> m_on_change{nullptr};
    std::function<void(const std::string&)> m_on_submit{nullptr};
};

class TextInputWidget;

// C++20 指定初始化器单行文本输入框 (TextInput)
// 桌面专业标准：高度 26px，硬朗微圆角 3px，双向绑定响应式同步
struct TextInput {
    std::string text{""};
    std::string placeholder{""};
    BindRef<std::string> bind_text{nullptr};
    float height{26.0f};          // 桌面高信息密度 26px
    float corner_radius{3.0f};    // 硬朗微圆角 3px
    EdgeInsets padding{EdgeInsets(4.0f, 8.0f)};
    Color background_color{Color{0.14f, 0.14f, 0.16f, 1.0f}};
    Color border_color{Color{0.25f, 0.25f, 0.28f, 1.0f}};
    Color focused_border_color{Color{0.20f, 0.55f, 0.95f, 1.0f}};
    Color text_color{Color{0.92f, 0.92f, 0.95f, 1.0f}};
    Color placeholder_color{Color{0.45f, 0.45f, 0.48f, 1.0f}};
    float font_size{13.0f};
    std::function<void(const std::string&)> on_change{nullptr};
    std::function<void(const std::string&)> on_submit{nullptr};

    operator WidgetPtr() const;
};

// TextInputWidget 包装实现
class TextInputWidget : public Widget {
public:
    explicit TextInputWidget(const TextInput& desc) : m_desc(desc) {}

    ~TextInputWidget() override {
        unsubscribe();
    }

    std::shared_ptr<RenderBox> create_render_node() override {
        std::string init_text = m_desc.bind_text ? m_desc.bind_text->get() : m_desc.text;
        auto node = std::make_shared<RenderTextInput>();

        // 双向同步回调
        auto on_ch = [this, node_weak = std::weak_ptr<RenderTextInput>(node)](const std::string& val) {
            m_is_internal_update = true;
            if (m_desc.bind_text) {
                m_desc.bind_text->set(val);
            }
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
            m_is_internal_update = false;
        };

        node->set_properties(init_text, m_desc.placeholder, m_desc.height, m_desc.corner_radius,
                             m_desc.padding, m_desc.background_color, m_desc.border_color,
                             m_desc.focused_border_color, m_desc.text_color,
                             m_desc.placeholder_color, m_desc.font_size, on_ch, m_desc.on_submit);

        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto input_box = std::dynamic_pointer_cast<RenderTextInput>(node)) {
            std::string current_val = m_desc.bind_text ? m_desc.bind_text->get() : m_desc.text;
            auto on_ch = [this](const std::string& val) {
                m_is_internal_update = true;
                if (m_desc.bind_text) {
                    m_desc.bind_text->set(val);
                }
                if (m_desc.on_change) {
                    m_desc.on_change(val);
                }
                m_is_internal_update = false;
            };

            input_box->set_properties(current_val, m_desc.placeholder, m_desc.height, m_desc.corner_radius,
                                      m_desc.padding, m_desc.background_color, m_desc.border_color,
                                      m_desc.focused_border_color, m_desc.text_color,
                                      m_desc.placeholder_color, m_desc.font_size, on_ch, m_desc.on_submit);

            setup_binding(input_box);
        }
    }

    [[nodiscard]] const TextInput& desc() const noexcept { return m_desc; }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_text) {
            m_desc.bind_text->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderTextInput> input_box) {
        unsubscribe();
        if (m_desc.bind_text) {
            std::weak_ptr<RenderTextInput> weak_box = input_box;
            m_binding_token = m_desc.bind_text->subscribe([this, weak_box](const std::string& new_val) {
                if (m_is_internal_update) return; // 避免双向更新死循环
                if (auto box = weak_box.lock()) {
                    box->set_text(new_val);
                }
            });
        }
    }

    TextInput m_desc;
    uint64_t m_binding_token{0};
    bool m_is_internal_update{false};
};

inline TextInput::operator WidgetPtr() const {
    return std::make_shared<TextInputWidget>(*this);
}

} // namespace ktui
