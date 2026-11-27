#pragma once

// From Uiverse.io by WhiteNervosa - https://uiverse.io/WhiteNervosa/bad-mole-33
// MIT License, Copyright (c) WhiteNervosa (WhiteNervosa)

#include "textbox_data.hpp"
#include "textbox_style.hpp"
#include <ktui/animation/animation.hpp>
#include <ktui/animation/cubic_bezier.hpp>
#include <ktui/animation/spring_caret.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/theme/theme.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/widgets/common/text_context_menu.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ktui {

// 桌面级紧凑单行文本框渲染盒 (RenderTextBox - Logic Layer)
// 职责：文本编辑、IME 输入法集成、撤销/重做、选区管理、剪贴板交互、光标闪烁与 Fluent 底部活动聚焦线驱动
class RenderTextBox : public RenderBox, public ITextInputClient {
public:
    RenderTextBox() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::IBeam;
        setup_behavior();
    }

    ~RenderTextBox() override = default;

    // 向后兼容历史接口 (Legacy Setter)
    void set_properties(std::string text, std::string placeholder,
                        float h, float radius, EdgeInsets pad,
                        Color bg, Color border_normal, Color border_focus,
                        Color txt_col, Color ph_col, float font_sz,
                        Fn<void(const std::string&)> on_ch,
                        Fn<void(const std::string&)> on_sub) {
        if (m_data.text != text) {
            m_data.text = std::move(text);
            m_cursor_index = std::min(m_cursor_index, m_data.text.length());
            m_selection_anchor = std::min(m_selection_anchor, m_data.text.length());
        }
        m_data.placeholder = std::move(placeholder);
        m_custom_height = (h > 0.0f) ? std::optional<float>(h) : std::nullopt;
        m_custom_radius = (radius > 0.0f) ? std::optional<float>(radius) : std::nullopt;
        m_custom_padding = (pad.left >= 0.0f) ? std::optional<EdgeInsets>(pad) : std::nullopt;
        m_custom_bg = (bg.r >= 0.0f) ? std::optional<Color>(bg) : std::nullopt;
        m_custom_border = (border_normal.r >= 0.0f) ? std::optional<Color>(border_normal) : std::nullopt;
        m_custom_focused_border = (border_focus.r >= 0.0f) ? std::optional<Color>(border_focus) : std::nullopt;
        m_custom_text = (txt_col.r >= 0.0f) ? std::optional<Color>(txt_col) : std::nullopt;
        m_custom_placeholder = (ph_col.r >= 0.0f) ? std::optional<Color>(ph_col) : std::nullopt;
        m_custom_font_size = (font_sz > 0.0f) ? std::optional<float>(font_sz) : std::nullopt;
        m_data.on_change = std::move(on_ch);
        m_data.on_submit = std::move(on_sub);

        mark_needs_layout();
    }

    // ==================== 数据与状态读写 ====================

    [[nodiscard]] const TextBoxData& data() const noexcept { return m_data; }
    [[nodiscard]] const TextBoxInteractiveState& state() const noexcept { return m_state; }
    [[nodiscard]] ITextInputClient* as_text_input_client() noexcept override { return this; }

    [[nodiscard]] const std::string& text() const noexcept { return m_data.text; }
    void set_text(std::string text) {
        if (m_data.text != text) {
            m_data.text = std::move(text);
            m_cursor_index = std::min(m_cursor_index, m_data.text.length());
            m_selection_anchor = m_cursor_index;
            ensure_cursor_visible();
            mark_needs_paint();
        }
    }

    void set_input_focused(bool focused) {
        if (m_state.is_focused != focused) {
            m_state.is_focused = focused;
            trigger_focus_animations(focused);
            mark_needs_paint();
        }
    }

    void append_chars(const std::string& str) {
        if (str.empty()) return;
        m_cursor_index = m_data.text.size();
        m_selection_anchor = m_cursor_index;
        insert_text_internal(str);
    }

    [[nodiscard]] bool can_undo() const noexcept {
        return !m_undo_stack.empty();
    }

    [[nodiscard]] bool can_redo() const noexcept {
        return !m_redo_stack.empty();
    }

    [[nodiscard]] bool is_cursor_blink_visible() const noexcept {
        return m_state.is_focused && m_blink_visible;
    }

    [[nodiscard]] std::pair<size_t, size_t> find_word_boundaries(size_t index) const {
        const std::string& str = m_data.text;
        if (str.empty()) return {0, 0};
        if (index >= str.length()) index = str.length() - 1;

        char target = str[index];
        if (target == ' ' || target == '\t') {
            size_t s = index;
            while (s > 0 && (str[s - 1] == ' ' || str[s - 1] == '\t')) s--;
            size_t e = index;
            while (e < str.length() && (str[e] == ' ' || str[e] == '\t')) e++;
            return {s, e};
        }

        if (is_word_char(target)) {
            size_t s = index;
            while (s > 0 && is_word_char(str[s - 1])) s--;
            size_t e = index;
            while (e < str.length() && is_word_char(str[e])) e++;
            return {s, e};
        }

        size_t s = prev_utf8_char_index(str, index + 1);
        size_t e = next_utf8_char_index(str, index);
        return {s, e};
    }

    [[nodiscard]] const std::string& placeholder() const noexcept { return m_data.placeholder; }
    void set_placeholder(std::string ph) {
        if (m_data.placeholder != ph) {
            m_data.placeholder = std::move(ph);
            mark_needs_paint();
        }
    }

    [[nodiscard]] size_t cursor_index() const noexcept { return m_cursor_index; }
    void set_cursor_index(size_t idx) noexcept {
        m_cursor_index = std::min(idx, m_data.text.length());
        m_selection_anchor = m_cursor_index;
        ensure_cursor_visible();
        mark_needs_paint();
    }

    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    void set_disabled(bool disabled) {
        if (m_state.is_disabled != disabled) {
            m_state.is_disabled = disabled;
            m_data.disabled = disabled;
            is_focusable = !m_state.is_disabled;
            cursor_shape = m_state.is_disabled ? CursorShape::Forbidden : CursorShape::IBeam;
            if (m_state.is_disabled) {
                m_state.is_focused = false;
                trigger_focus_animations(false);
            }
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool is_read_only() const noexcept { return m_state.is_read_only; }
    void set_read_only(bool ro) noexcept {
        m_state.is_read_only = ro;
        m_data.is_read_only = ro;
        mark_needs_paint();
    }

    [[nodiscard]] bool is_password() const noexcept { return m_data.is_password; }
    void set_is_password(bool pass) noexcept {
        m_data.is_password = pass;
        mark_needs_paint();
    }

    [[nodiscard]] bool is_password_revealed() const noexcept { return m_data.is_password_revealed; }
    void set_password_revealed(bool revealed) noexcept {
        m_data.is_password_revealed = revealed;
        mark_needs_paint();
    }

    // ==================== 选区 (Selection) 管理 ====================

    [[nodiscard]] bool has_selection() const noexcept {
        return m_selection_anchor != m_cursor_index;
    }

    [[nodiscard]] size_t selection_start() const noexcept {
        return std::min(m_selection_anchor, m_cursor_index);
    }

    [[nodiscard]] size_t selection_end() const noexcept {
        return std::max(m_selection_anchor, m_cursor_index);
    }

    void select_all() noexcept {
        m_selection_anchor = 0;
        m_cursor_index = m_data.text.length();
        ensure_cursor_visible();
        mark_needs_paint();
    }

    void clear_selection() noexcept {
        m_selection_anchor = m_cursor_index;
        mark_needs_paint();
    }

    [[nodiscard]] std::string selected_text() const {
        if (!has_selection()) return "";
        size_t s = selection_start();
        size_t len = selection_end() - s;
        return m_data.text.substr(s, len);
    }

    // ==================== 撤销 / 重做 (Undo / Redo) 栈 ====================

    void undo() {
        if (m_undo_stack.empty() || m_state.is_read_only || m_state.is_disabled) return;
        m_is_undoing = true;
        m_redo_stack.push_back({m_data.text, m_cursor_index, m_selection_anchor});
        auto state = m_undo_stack.back();
        m_undo_stack.pop_back();

        m_data.text = state.text;
        m_cursor_index = std::min(state.cursor_pos, m_data.text.length());
        m_selection_anchor = std::min(state.selection_anchor, m_data.text.length());
        m_is_undoing = false;

        notify_changed();
        ensure_cursor_visible();
        mark_needs_paint();
    }

    void redo() {
        if (m_redo_stack.empty() || m_state.is_read_only || m_state.is_disabled) return;
        m_is_undoing = true;
        m_undo_stack.push_back({m_data.text, m_cursor_index, m_selection_anchor});
        auto state = m_redo_stack.back();
        m_redo_stack.pop_back();

        m_data.text = state.text;
        m_cursor_index = std::min(state.cursor_pos, m_data.text.length());
        m_selection_anchor = std::min(state.selection_anchor, m_data.text.length());
        m_is_undoing = false;

        notify_changed();
        ensure_cursor_visible();
        mark_needs_paint();
    }

    // ==================== 样式解析辅助 ====================

    [[nodiscard]] ResolvedTextBoxStyle resolve_current_style() const {
        const Theme& theme = ThemeContext::current();
        return TextBoxStyleResolver::resolve(
            m_data, m_state, theme,
            m_custom_bg, m_custom_border, m_custom_focused_border,
            m_custom_text, m_custom_placeholder,
            m_custom_height, m_custom_radius, m_custom_padding, m_custom_font_size
        );
    }

    // ==================== 布局度量 (Layout & Measure) ====================

    void perform_layout() override {
        auto style = resolve_current_style();
        float desired_w = m_preferred_width; // 默认紧凑最小宽度 120px，派生控件可按需指定
        float desired_h = style.height;
        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
        ensure_cursor_visible();
    }

    // ==================== 绘制管线 (Painting Pipeline) ====================

    void paint(PaintContext& context) override;

    // ==================== ITextInputClient 接口实现 (IME 支持) ====================

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        auto style = resolve_current_style();
        const std::string display_text = composed_display_text();
        float font_sz = style.font_size;
        float start_x = bounds().min_x + style.padding.left - m_scroll_offset_x;
        const size_t cursor = to_display_offset(m_cursor_index) + std::min(m_ime_preedit_cursor, m_ime_preedit.size());
        float cursor_x = start_x + TextMeasure::cursor_x(display_text, font_sz, cursor);
        float caret_h = font_sz + 2.0f;
        float caret_y = bounds().min_y + (bounds().height() - caret_h) * 0.5f;
        return Rect::from_xywh(cursor_x, caret_y, style.caret_width, caret_h);
    }

    void on_ime_preedit(const std::string& text, size_t cursor_byte_pos) override {
        if (m_state.is_read_only || m_state.is_disabled) return;
        m_ime_preedit = text;
        m_ime_preedit_cursor = cursor_byte_pos;
        ensure_cursor_visible();
        mark_needs_paint();
    }

    void on_ime_commit(const std::string& utf8_text) override {
        if (m_state.is_read_only || m_state.is_disabled || utf8_text.empty()) return;
        m_ime_preedit.clear();
        m_ime_preedit_cursor = 0;
        insert_text_internal(utf8_text);
    }

    // 外部或测试按键处理注入 (完整支持选区、快捷键、撤销、Shift导航)
    void handle_text_key(const KeyEvent& ev);

    void apply_custom_overrides(std::optional<Color> bg,
                                std::optional<Color> border,
                                std::optional<Color> focus_border,
                                std::optional<Color> txt,
                                std::optional<Color> ph,
                                std::optional<float> h,
                                std::optional<float> radius,
                                std::optional<EdgeInsets> pad,
                                std::optional<float> font_sz) {
        m_custom_bg = bg;
        m_custom_border = border;
        m_custom_focused_border = focus_border;
        m_custom_text = txt;
        m_custom_placeholder = ph;
        m_custom_height = h;
        m_custom_radius = radius;
        m_custom_padding = pad;
        m_custom_font_size = font_sz;
    }

private:
    void setup_behavior();

    bool tick_cursor_blink() override {
        if (!m_state.is_focused) {
            if (m_blink_visible) {
                m_blink_visible = false;
                mark_needs_paint();
                return true;
            }
            return false;
        }
        auto style = resolve_current_style();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_blink_time).count();
        if (elapsed >= style.caret_blink_ms) {
            m_blink_visible = !m_blink_visible;
            m_last_blink_time = now;
            mark_needs_paint();
            return true;
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept override {
        if (!m_state.is_focused) return 0;
        auto style = resolve_current_style();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_blink_time).count();
        if (elapsed >= style.caret_blink_ms) return 0;
        return static_cast<uint32_t>(style.caret_blink_ms - elapsed);
    }

    void update_blink_state(uint32_t /*blink_rate_ms*/) {
        tick_cursor_blink();
    }

    [[nodiscard]] std::string_view get_display_text() const {
        if (m_data.is_password && !m_data.is_password_revealed) {
            update_password_mask_cache();
            return m_cached_password_mask;
        }
        return m_data.text;
    }

    size_t to_display_offset(size_t offset) const {
        if (!m_data.is_password || m_data.is_password_revealed) return offset;
        const auto stops = kernetile::text::grapheme_boundaries(m_data.text);
        return 3 * (std::upper_bound(stops.begin(), stops.end(), offset) - stops.begin() - 1);
    }

    std::string composed_display_text() const {
        std::string result(get_display_text());
        result.insert(std::min(to_display_offset(m_cursor_index), result.size()), m_ime_preedit);
        return result;
    }

    void update_password_mask_cache() const {
        size_t count = kernetile::text::grapheme_boundaries(m_data.text).size() - 1;
        if (m_cached_password_mask.size() != count * 3) {
            m_cached_password_mask.clear();
            m_cached_password_mask.reserve(count * 3);
            for (size_t i = 0; i < count; ++i) {
                m_cached_password_mask += "\xE2\x80\xA2";
            }
        }
    }

    void insert_text_internal(const std::string& str) {
        push_undo_snapshot();
        delete_selection_internal();
        m_data.text.insert(m_cursor_index, str);
        m_cursor_index += str.length();
        m_selection_anchor = m_cursor_index;
        notify_changed();
        ensure_cursor_visible();
        mark_needs_paint();
    }

    void delete_selection_internal() {
        if (!has_selection()) return;
        size_t s = selection_start();
        size_t len = selection_end() - s;
        m_data.text.erase(s, len);
        m_cursor_index = s;
        m_selection_anchor = s;
    }

    void push_undo_snapshot() {
        if (m_is_undoing) return;
        m_undo_stack.push_back({m_data.text, m_cursor_index, m_selection_anchor});
        m_redo_stack.clear();
        if (m_undo_stack.size() > 100) {
            m_undo_stack.erase(m_undo_stack.begin());
        }
    }

    void notify_changed() {
        if (m_data.on_change) {
            m_data.on_change(m_data.text);
        }
    }

    void ensure_cursor_visible() {
        auto style = resolve_current_style();
        if (m_size.x <= 1.0f) { m_scroll_offset_x = 0.0f; return; }
        float font_sz = style.font_size;
        float reveal_btn_w = (m_data.is_password && m_data.show_reveal_button) ? 22.0f : 0.0f;
        float avail_w = std::max(10.0f, m_size.x - style.padding.horizontal() - reveal_btn_w);

        const auto display_text = composed_display_text();
        const size_t cursor = to_display_offset(m_cursor_index) + std::min(m_ime_preedit_cursor, m_ime_preedit.size());
        const float max_scroll = std::max(0.0f, TextMeasure::measure_width(display_text, font_sz) - avail_w + 12.0f);
        m_scroll_offset_x = std::clamp(m_scroll_offset_x, 0.0f, max_scroll);
        float cursor_rel_x = TextMeasure::cursor_x(display_text, font_sz, cursor);
        if (cursor_rel_x - m_scroll_offset_x > avail_w - 4.0f) {
            m_scroll_offset_x = cursor_rel_x - avail_w + 12.0f;
        } else if (cursor_rel_x - m_scroll_offset_x < 0.0f) {
            m_scroll_offset_x = std::max(0.0f, cursor_rel_x - 12.0f);
        }
    }

    void select_word_at(float click_x, std::string_view str, float font_sz) {
        size_t idx = get_char_index_at(click_x, str, font_sz);
        str = m_data.text;
        if (str.empty()) return;

        size_t start = idx;
        while (start > 0 && is_word_char(str[start - 1])) {
            start--;
        }
        size_t end = idx;
        while (end < str.length() && is_word_char(str[end])) {
            end++;
        }
        if (start == end && idx < str.length()) {
            end = next_utf8_char_index(str, idx);
        }
        m_selection_anchor = start;
        m_cursor_index = end;
    }

    [[nodiscard]] size_t get_char_index_at(float x, std::string_view str, float font_sz) const {
        const auto offset = TextMeasure::hit_test_cursor(str, font_sz, x);
        if (!m_data.is_password || m_data.is_password_revealed) return offset;
        const auto stops = kernetile::text::grapheme_boundaries(m_data.text);
        return stops[std::min(offset / 3, stops.size() - 1)];
    }

    static bool is_word_char(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }

    static size_t prev_utf8_char_index(std::string_view str, size_t index) {
        const auto stops = kernetile::text::grapheme_boundaries(str);
        const auto stop = std::lower_bound(stops.begin(), stops.end(), index);
        return stop == stops.begin() ? 0 : *std::prev(stop);
    }

    static size_t next_utf8_char_index(std::string_view str, size_t index) {
        const auto stops = kernetile::text::grapheme_boundaries(str);
        const auto stop = std::upper_bound(stops.begin(), stops.end(), index);
        return stop == stops.end() ? str.size() : *stop;
    }

    mutable std::string m_cached_password_mask{};
    TextBoxData m_data;
    TextBoxInteractiveState m_state;

    // 状态与测量
    size_t m_cursor_index{0};
    size_t m_selection_anchor{0};
    std::string m_ime_preedit{""};
    size_t m_ime_preedit_cursor{0};
    float m_scroll_offset_x{0.0f};

    std::chrono::steady_clock::time_point m_last_blink_time;
    bool m_blink_visible{true};

    std::chrono::steady_clock::time_point m_last_click_time;
    int m_click_count{0};

    std::vector<TextBoxUndoState> m_undo_stack;
    std::vector<TextBoxUndoState> m_redo_stack;
    bool m_is_undoing{false};

    static Color lerp_color(const Color& a, const Color& b, float t) noexcept {
        float c = std::clamp(t, 0.0f, 1.0f);
        return Color{
            a.r + (b.r - a.r) * c,
            a.g + (b.g - a.g) * c,
            a.b + (b.b - a.b) * c,
            a.a + (b.a - a.a) * c
        };
    }

    void trigger_focus_animations(bool focused) {
        if (focused) {
            m_blink_visible = true;
            m_last_blink_time = std::chrono::steady_clock::now();
            m_focus_line_tween.start(m_focus_line_tween.current_value(), 1.0f, kAfterDurationSec, Easing::EaseOutCubic);
            m_before_from = m_before_progress;
            m_before_to = 1.0f;
            m_before_elapsed = 0.0f;
            m_after_from = m_after_scale;
            m_after_to = 1.0f;
            m_after_elapsed = 0.0f;
            m_placeholder_from = m_placeholder_opacity;
            m_placeholder_to = 0.0f;
            m_placeholder_elapsed = 0.0f;
        } else {
            m_blink_visible = false;
            m_ime_preedit.clear();
            m_focus_line_tween.start(m_focus_line_tween.current_value(), 0.0f, kBeforeDurationSec, Easing::EaseOutCubic);
            m_before_from = m_before_progress;
            m_before_to = 0.0f;
            m_before_elapsed = 0.0f;
            m_after_from = m_after_scale;
            m_after_to = 0.0f;
            m_after_elapsed = 0.0f;
            m_placeholder_from = m_placeholder_opacity;
            m_placeholder_to = 1.0f;
            m_placeholder_elapsed = 0.0f;
        }
        m_last_tick_time = std::chrono::steady_clock::now();
    }

    // Uiverse (WhiteNervosa/bad-mole-33) 动画曲线与时长常量
    static constexpr float kBeforeDurationSec = 0.20f;      // 200ms
    static constexpr float kAfterDurationSec = 0.25f;       // 250ms
    static constexpr float kPlaceholderDurationSec = 0.25f; // 250ms
    static constexpr float kBgDurationSec = 0.20f;          // 200ms
    static constexpr CubicBezier kCurveBefore{0.4f, 0.0f, 0.2f, 1.0f};
    static constexpr CubicBezier kCurveAfter{0.0f, 0.0f, 0.2f, 1.0f};
    static constexpr CubicBezier kCurveBg{0.25f, 0.8f, 0.25f, 1.0f};

    // 动效管理器托管的底部活动高亮线驱动器 + Uiverse 四路平滑过渡状态
    MotionTween<float> m_focus_line_tween;
    std::chrono::steady_clock::time_point m_last_tick_time{};

    float m_before_from{0.0f};
    float m_before_to{0.0f};
    float m_before_progress{0.0f};
    float m_before_elapsed{kBeforeDurationSec};

    float m_after_from{0.0f};
    float m_after_to{0.0f};
    float m_after_scale{0.0f};
    float m_after_elapsed{kAfterDurationSec};

    float m_placeholder_from{1.0f};
    float m_placeholder_to{1.0f};
    float m_placeholder_opacity{1.0f};
    float m_placeholder_elapsed{kPlaceholderDurationSec};

    bool m_bg_initialized{false};
    Color m_bg_from{};
    Color m_bg_to{};
    Color m_bg_current{};
    float m_bg_elapsed{kBgDurationSec};

    // 自定义覆盖属性
    std::optional<Color> m_custom_bg;
    std::optional<Color> m_custom_border;
    std::optional<Color> m_custom_focused_border;
    std::optional<Color> m_custom_text;
    std::optional<Color> m_custom_placeholder;
    std::optional<float> m_custom_height;
    std::optional<float> m_custom_radius;
    void open_context_menu(Vec2 client_pos, bool is_touch) {
        m_click_count = 0;
        m_last_click_time = {};
        const bool allow_copy_secret = (!m_data.is_password || m_data.is_password_revealed);
        TextContextMenuActions actions;
        actions.can_undo = can_undo() && !m_state.is_read_only;
        actions.can_redo = can_redo() && !m_state.is_read_only;
        actions.can_cut = has_selection() && !m_state.is_read_only && allow_copy_secret;
        actions.can_copy = has_selection() && allow_copy_secret;
        actions.can_paste = !m_state.is_read_only && !Clipboard::get_text().empty();
        actions.can_delete = has_selection() && !m_state.is_read_only;
        actions.can_select_all = !m_data.text.empty();
        actions.on_undo = [this]() { undo(); };
        actions.on_redo = [this]() { redo(); };
        actions.on_cut = [this]() {
            if (has_selection() && !m_state.is_read_only) {
                push_undo_snapshot();
                Clipboard::set_text(selected_text());
                delete_selection_internal();
                notify_changed();
                ensure_cursor_visible();
                mark_needs_paint();
            }
        };
        actions.on_copy = [this]() {
            if (has_selection()) Clipboard::set_text(selected_text());
        };
        actions.on_paste = [this]() {
            if (!m_state.is_read_only) {
                std::string clip = Clipboard::get_text();
                std::string filtered;
                for (char c : clip) {
                    if (c != '\r' && c != '\n') filtered += c;
                }
                if (!filtered.empty()) insert_text_internal(filtered);
            }
        };
        actions.on_delete = [this]() {
            if (has_selection() && !m_state.is_read_only) {
                push_undo_snapshot();
                delete_selection_internal();
                notify_changed();
                ensure_cursor_visible();
                mark_needs_paint();
            }
        };
        actions.on_select_all = [this, client_pos, is_touch]() {
            select_all();
            // Telegram 风格：移动端点击“全选”后直接刷新弹出菜单（此时剪切/复制/删除全部可用），无需再次长按
            if (is_touch && has_selection()) {
                open_context_menu(client_pos, true);
            }
        };
        show_winui_text_context_menu(client_pos, actions);
    }

    bool m_pending_touch_collapse{false};
    size_t m_pending_touch_collapse_index{0};
    std::optional<EdgeInsets> m_custom_padding;
    std::optional<float> m_custom_font_size;

public:
    void set_custom_background(Color c) { m_custom_bg = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_border_color(Color c) { m_custom_border = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_focused_border_color(Color c) { m_custom_focused_border = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_text_color(Color c) { m_custom_text = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_placeholder_color(Color c) { m_custom_placeholder = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_height(float h) { m_custom_height = (h > 0.0f) ? std::optional<float>(h) : std::nullopt; mark_needs_layout(); }
    void set_custom_corner_radius(float r) { m_custom_radius = (r > 0.0f) ? std::optional<float>(r) : std::nullopt; mark_needs_paint(); }
    void set_custom_padding(EdgeInsets pad) { m_custom_padding = (pad.left >= 0.0f) ? std::optional<EdgeInsets>(pad) : std::nullopt; mark_needs_layout(); }
    void set_custom_font_size(float fs) { m_custom_font_size = (fs > 0.0f) ? std::optional<float>(fs) : std::nullopt; mark_needs_layout(); }
    void set_show_reveal_button(bool show) { m_data.show_reveal_button = show; mark_needs_paint(); }
    void set_on_change(Fn<void(const std::string&)> cb) { m_data.on_change = std::move(cb); }
    void set_preferred_width(float w) { m_preferred_width = std::max(40.0f, w); mark_needs_layout(); }
    void set_ghost_suffix(std::string suffix) { if (m_ghost_suffix != suffix) { m_ghost_suffix = std::move(suffix); mark_needs_paint(); } }
    void set_ghost_badge(std::string badge) { if (m_ghost_badge != badge) { m_ghost_badge = std::move(badge); mark_needs_paint(); } }
    void set_status_dot(bool show, Color color = Color{0.10f, 0.72f, 0.45f, 1.0f}) {
        m_show_status_dot = show;
        m_status_dot_color = color;
        mark_needs_paint();
    }
    void set_key_intercept(Fn<bool(KeyEvent&)> cb) { m_key_intercept = std::move(cb); }
    void set_borderless_inline(bool v) { if (m_borderless_inline != v) { m_borderless_inline = v; mark_needs_paint(); } }
    void set_smooth_caret(bool enabled) noexcept {
        if (m_smooth_caret != enabled) {
            m_smooth_caret = enabled;
            if (!enabled) m_spring_caret.reset();
            mark_needs_paint();
        }
    }
    void set_inset_shadow(bool enabled) noexcept {
        if (m_inset_shadow != enabled) {
            m_inset_shadow = enabled;
            mark_needs_paint();
        }
    }
    void jump_caret_to(size_t index) noexcept {
        m_cursor_index = std::min(index, m_data.text.size());
        m_selection_anchor = m_cursor_index;
        m_state.is_focused = true;
        m_blink_visible = true;
        m_spring_caret.notify_activity();
        ensure_cursor_visible();
        mark_needs_paint();
    }

private:
    bool m_borderless_inline{false};
    bool m_smooth_caret{true};
    bool m_inset_shadow{false};
    SpringCaretSolver m_spring_caret;
    float m_preferred_width{120.0f};
    std::string m_ghost_suffix;
    std::string m_ghost_badge;
    bool m_show_status_dot{false};
    Color m_status_dot_color{0.10f, 0.72f, 0.45f, 1.0f};
    Fn<bool(KeyEvent&)> m_key_intercept;
};

} // namespace ktui

#include "textbox_paint.hpp"
#include "textbox_behavior.hpp"
#include "textbox_widget.hpp"
