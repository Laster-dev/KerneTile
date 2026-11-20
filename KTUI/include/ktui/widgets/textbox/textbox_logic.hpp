#pragma once

#include "textbox_data.hpp"
#include "textbox_style.hpp"
#include <ktui/animation/animation.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/theme/theme.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/pal/clipboard.hpp>
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
                        std::function<void(const std::string&)> on_ch,
                        std::function<void(const std::string&)> on_sub) {
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
            if (focused) {
                m_blink_visible = true;
                m_last_blink_time = std::chrono::steady_clock::now();
                m_focus_line_tween.start(0.0f, 1.0f, 0.25f, Easing::EaseOutCubic);
            } else {
                m_blink_visible = false;
                m_ime_preedit.clear();
                m_focus_line_tween.start(1.0f, 0.0f, 0.20f, Easing::EaseOutCubic);
            }
            m_last_tick_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        }
    }

    void append_chars(const std::string& str) {
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
                m_focus_line_tween.start(1.0f, 0.0f, 0.15f, Easing::EaseOutCubic);
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
        float desired_w = 120.0f; // 默认紧凑最小宽度
        float desired_h = style.height;
        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
        ensure_cursor_visible();
    }

    // ==================== 绘制管线 (Painting Pipeline) ====================

    void paint(PaintContext& context) override {
        // 1. 如果底部活动高亮线补间正处于运行中，推进动效管理器步进
        if (m_focus_line_tween.is_active()) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - m_last_tick_time).count();
            m_last_tick_time = now;
            m_focus_line_tween.tick(dt);
        }

        auto style = resolve_current_style();

        // 2. 检查光标闪烁状态 (530ms 闪烁驱动)
        update_blink_state(style.caret_blink_ms);

        // 3. 绘制微圆角实心底色卡片
        if (style.background.a > 0.001f) {
            context.draw_rect(bounds(), style.background, style.corner_radius, 0.0f);
        }

        // 4. 绘制四周描边 (仅当外部显式指定 custom_border 且 border_width > 0 时才绘制，默认 0px 零描边)
        if (style.border_width > 0.0f && style.border.a > 0.001f) {
            context.draw_rect(bounds(), style.border, style.corner_radius, style.border_width);
        }

        // 4.1 CUI / Fluent 底部 1px 静止底线 (ThemeTokenId::InputBorder，静止时提供下划视觉锚点)
        const float corner_inset = std::min(style.corner_radius * 0.75f, 4.0f);
        float line_start_x = bounds().min_x + corner_inset;
        float line_w = std::max(0.0f, bounds().width() - corner_inset * 2.0f);
        float resting_line_y = bounds().max_y - style.underline_height;
        if (style.underline.a > 0.001f && style.underline_height > 0.0f) {
            context.draw_rect(Rect::from_xywh(line_start_x, resting_line_y, line_w, style.underline_height), style.underline, 0.0f, 0.0f);
        }

        // 5. 计算文本内容区域并施加硬件裁剪保护 (严防内容溢出文本框外部)
        float reveal_btn_w = (m_data.is_password && m_data.show_reveal_button) ? 22.0f : 0.0f;
        Rect content_rect = Rect::from_xywh(
            bounds().min_x + style.padding.left,
            bounds().min_y + style.padding.top,
            std::max(0.0f, bounds().width() - style.padding.horizontal() - reveal_btn_w),
            std::max(0.0f, bounds().height() - style.padding.vertical())
        );

        context.push_clip(content_rect, 0.0f);

        std::string display_text = get_display_text();
        float font_sz = style.font_size;
        float text_y = bounds().min_y + (bounds().height() - font_sz) * 0.5f - 1.0f;
        float start_x = content_rect.min_x - m_scroll_offset_x;

        // 6. 绘制选区高亮色块 (半透明 Fluent Accent)
        if (m_state.is_focused && has_selection() && !display_text.empty()) {
            size_t sel_s = selection_start();
            size_t sel_e = selection_end();
            float sel_x1 = start_x + TextMeasure::measure_width(std::string_view(display_text).substr(0, sel_s), font_sz);
            float sel_x2 = start_x + TextMeasure::measure_width(std::string_view(display_text).substr(0, sel_e), font_sz);
            Rect sel_rect = Rect::from_xywh(
                sel_x1,
                content_rect.min_y,
                sel_x2 - sel_x1,
                content_rect.height()
            );
            context.draw_rect(sel_rect, style.selection, 0.0f, 0.0f);
        }

        // 7. 绘制主体文字或占位符
        if (!display_text.empty()) {
            context.draw_text(display_text, PointF{start_x, text_y}, font_sz, style.text);
        } else if (!m_data.placeholder.empty() && m_ime_preedit.empty()) {
            context.draw_text(m_data.placeholder, PointF{start_x, text_y}, font_sz, style.placeholder);
        }

        // 8. 绘制 IME 输入法预编辑组合态下划线 (如拼音 "nihao")
        if (!m_ime_preedit.empty()) {
            float preedit_start_x = start_x + TextMeasure::measure_width(std::string_view(display_text).substr(0, m_cursor_index), font_sz);
            float preedit_w = TextMeasure::measure_width(m_ime_preedit, font_sz);
            float underline_y = bounds().max_y - style.padding.bottom - 1.0f;
            context.draw_rect(Rect::from_xywh(preedit_start_x, underline_y, preedit_w, 1.5f), style.focus_indicator, 0.0f, 0.0f);
        }

        // 9. 绘制精致 1.5px 闪烁光标
        if (m_state.is_focused && m_blink_visible && !m_state.is_disabled) {
            size_t visual_cursor = m_cursor_index;
            float cursor_x = start_x + TextMeasure::measure_width(std::string_view(display_text).substr(0, visual_cursor), font_sz);
            if (!m_ime_preedit.empty()) {
                cursor_x += TextMeasure::measure_width(std::string_view(m_ime_preedit).substr(0, m_ime_preedit_cursor), font_sz);
            }
            float caret_h = font_sz + 2.0f;
            float caret_y = bounds().min_y + (bounds().height() - caret_h) * 0.5f;
            context.draw_rect(Rect::from_xywh(cursor_x, caret_y, style.caret_width, caret_h), style.caret, 0.0f, 0.0f);
        }

        context.pop_clip();

        // 10. Fluent 底部动态聚焦扩展高光线 (Focus Indicator Line)
        float focus_progress = m_focus_line_tween.current_value();
        if (focus_progress > 0.001f && !m_state.is_disabled) {
            float max_active_w = bounds().width() - corner_inset * 2.0f;
            float active_w = max_active_w * focus_progress;
            float active_x = bounds().min_x + (bounds().width() - active_w) * 0.5f;
            float line_y = bounds().max_y - style.focus_line_height;
            context.draw_rect(Rect::from_xywh(active_x, line_y, active_w, style.focus_line_height), style.focus_indicator, 0.0f, 0.0f);
        }

        // 11. 密码模式显隐切换眼睛图标
        if (m_data.is_password && m_data.show_reveal_button) {
            float eye_cx = bounds().max_x - 12.0f;
            float eye_cy = bounds().min_y + bounds().height() * 0.5f;
            Color eye_col = style.placeholder;
            Rect eye_rect = Rect::from_xywh(eye_cx - 6.0f, eye_cy - 4.0f, 12.0f, 8.0f);
            context.draw_rect(eye_rect, eye_col, 4.0f, 1.0f);
            context.draw_rect(Rect::from_xywh(eye_cx - 2.0f, eye_cy - 2.0f, 4.0f, 4.0f), eye_col, 2.0f, 0.0f);
            if (!m_data.is_password_revealed) {
                // 绘制斜线表示隐藏
                context.draw_rect(Rect::from_xywh(eye_cx - 5.0f, eye_cy - 1.0f, 10.0f, 1.2f), eye_col, 0.0f, 0.0f);
            }
        }
    }

    // ==================== ITextInputClient 接口实现 (IME 支持) ====================

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        auto style = resolve_current_style();
        std::string display_text = get_display_text();
        float font_sz = style.font_size;
        float start_x = bounds().min_x + style.padding.left - m_scroll_offset_x;
        float cursor_x = start_x + TextMeasure::measure_width(std::string_view(display_text).substr(0, m_cursor_index), font_sz);
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
    void handle_text_key(const KeyEvent& ev) {
        if (!m_state.is_focused || m_state.is_disabled) return;

        if (ev.type == PlatformEventType::KeyDown) {
            constexpr uint32_t kKeyBackspace = 8;
            constexpr uint32_t kKeyEnter = 13;
            constexpr uint32_t kKeyDelete = 46;
            constexpr uint32_t kKeyLeft = 37;
            constexpr uint32_t kKeyRight = 39;
            constexpr uint32_t kKeyHome = 36;
            constexpr uint32_t kKeyEnd = 35;

            // Ctrl 快捷键
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
                    if (has_selection() && !m_state.is_read_only) {
                        push_undo_snapshot();
                        Clipboard::set_text(selected_text());
                        delete_selection_internal();
                        notify_changed();
                    }
                    return;
                } else if (kc == 'V' || kc == 'v') {
                    if (!m_state.is_read_only) {
                        std::string clip = Clipboard::get_text();
                        std::string filtered;
                        for (char c : clip) {
                            if (c != '\r' && c != '\n') filtered += c;
                        }
                        if (!filtered.empty()) {
                            insert_text_internal(filtered);
                        }
                    }
                    return;
                } else if (kc == 'Z' || kc == 'z') {
                    if (ev.modifiers.shift) redo();
                    else undo();
                    return;
                } else if (kc == 'Y' || kc == 'y') {
                    redo();
                    return;
                }
            }

            if (ev.key_code == kKeyBackspace) {
                if (m_state.is_read_only) return;
                if (has_selection()) {
                    push_undo_snapshot();
                    delete_selection_internal();
                    notify_changed();
                } else if (m_cursor_index > 0) {
                    push_undo_snapshot();
                    size_t prev = prev_utf8_char_index(m_data.text, m_cursor_index);
                    m_data.text.erase(prev, m_cursor_index - prev);
                    m_cursor_index = prev;
                    m_selection_anchor = m_cursor_index;
                    notify_changed();
                }
            } else if (ev.key_code == kKeyDelete) {
                if (m_state.is_read_only) return;
                if (has_selection()) {
                    push_undo_snapshot();
                    delete_selection_internal();
                    notify_changed();
                } else if (m_cursor_index < m_data.text.length()) {
                    push_undo_snapshot();
                    size_t next = next_utf8_char_index(m_data.text, m_cursor_index);
                    m_data.text.erase(m_cursor_index, next - m_cursor_index);
                    notify_changed();
                }
            } else if (ev.key_code == kKeyLeft) {
                if (ev.modifiers.shift) {
                    if (m_cursor_index > 0) {
                        m_cursor_index = prev_utf8_char_index(m_data.text, m_cursor_index);
                    }
                } else {
                    if (has_selection()) {
                        m_cursor_index = selection_start();
                        m_selection_anchor = m_cursor_index;
                    } else if (m_cursor_index > 0) {
                        m_cursor_index = prev_utf8_char_index(m_data.text, m_cursor_index);
                        m_selection_anchor = m_cursor_index;
                    }
                }
            } else if (ev.key_code == kKeyRight) {
                if (ev.modifiers.shift) {
                    if (m_cursor_index < m_data.text.length()) {
                        m_cursor_index = next_utf8_char_index(m_data.text, m_cursor_index);
                    }
                } else {
                    if (has_selection()) {
                        m_cursor_index = selection_end();
                        m_selection_anchor = m_cursor_index;
                    } else if (m_cursor_index < m_data.text.length()) {
                        m_cursor_index = next_utf8_char_index(m_data.text, m_cursor_index);
                        m_selection_anchor = m_cursor_index;
                    }
                }
            } else if (ev.key_code == kKeyHome) {
                m_cursor_index = 0;
                if (!ev.modifiers.shift) m_selection_anchor = 0;
            } else if (ev.key_code == kKeyEnd) {
                m_cursor_index = m_data.text.length();
                if (!ev.modifiers.shift) m_selection_anchor = m_cursor_index;
            } else if (ev.key_code == kKeyEnter) {
                if (m_data.on_submit) m_data.on_submit(m_data.text);
            }

            m_blink_visible = true;
            m_last_blink_time = std::chrono::steady_clock::now();
            ensure_cursor_visible();
            mark_needs_paint();
        } else if (ev.type == PlatformEventType::ImeCommit) {
            if (!m_state.is_read_only && !ev.text.empty()) {
                insert_text_internal(ev.text);
            }
        }
    }

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
    void setup_behavior() {
        on_focus = [this](FocusEvent& ev) {
            m_state.is_focused = ev.gained_focus;
            if (ev.gained_focus) {
                m_blink_visible = true;
                m_last_blink_time = std::chrono::steady_clock::now();
                // 委托动效管理器：底部活动聚焦线使用 EaseOutCubic 平滑自中心展开
                m_focus_line_tween.start(0.0f, 1.0f, 0.25f, Easing::EaseOutCubic);
            } else {
                m_blink_visible = false;
                m_ime_preedit.clear();
                m_focus_line_tween.start(1.0f, 0.0f, 0.20f, Easing::EaseOutCubic);
            }
            m_last_tick_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        };

        on_pointer_enter = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = true;
                is_hovered = true;
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = false;
                is_hovered = false;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (m_state.is_disabled || ev.button != PointerButton::Primary) return;

            auto now = std::chrono::steady_clock::now();
            float dt_ms = std::chrono::duration<float, std::milli>(now - m_last_click_time).count();
            if (dt_ms < 400.0f) {
                m_click_count++;
            } else {
                m_click_count = 1;
            }
            m_last_click_time = now;

            // 密码显示/隐藏眼睛按钮命中测试
            if (m_data.is_password && m_data.show_reveal_button) {
                float eye_x = bounds().max_x - 24.0f;
                if (ev.local_position.x >= eye_x) {
                    m_data.is_password_revealed = !m_data.is_password_revealed;
                    mark_needs_paint();
                    return;
                }
            }

            auto style = resolve_current_style();
            float start_x = style.padding.left - m_scroll_offset_x;
            float click_x = ev.local_position.x - start_x;
            std::string display_text = get_display_text();

            if (m_click_count == 2) {
                select_word_at(click_x, display_text, style.font_size);
            } else if (m_click_count >= 3) {
                select_all();
            } else {
                size_t new_index = get_char_index_at(click_x, display_text, style.font_size);
                m_cursor_index = new_index;
                if (!ev.modifiers.shift) {
                    m_selection_anchor = m_cursor_index;
                }
                m_state.is_dragging_selection = true;
            }

            m_blink_visible = true;
            m_last_blink_time = std::chrono::steady_clock::now();
            ensure_cursor_visible();
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& ev) {
            if (m_state.is_dragging_selection && !m_state.is_disabled) {
                auto style = resolve_current_style();
                float start_x = style.padding.left - m_scroll_offset_x;
                float click_x = ev.local_position.x - start_x;
                std::string display_text = get_display_text();
                m_cursor_index = get_char_index_at(click_x, display_text, style.font_size);
                ensure_cursor_visible();
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent&) {
            m_state.is_dragging_selection = false;
        };

        on_key = [this](KeyEvent& ev) {
            handle_text_key(ev);
        };
    }

    void update_blink_state(uint32_t blink_rate_ms) {
        if (!m_state.is_focused) {
            m_blink_visible = false;
            return;
        }
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_blink_time).count();
        if (elapsed >= blink_rate_ms) {
            m_blink_visible = !m_blink_visible;
            m_last_blink_time = now;
            mark_needs_paint();
        }
    }

    [[nodiscard]] std::string get_display_text() const {
        if (m_data.is_password && !m_data.is_password_revealed) {
            // 密码掩码: 每个字符转换为黑圆点 • (U+2022 UTF-8: E2 80 A2)
            std::string masked;
            size_t count = count_utf8_chars(m_data.text);
            for (size_t i = 0; i < count; ++i) {
                masked += "\xE2\x80\xA2";
            }
            return masked;
        }
        return m_data.text;
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
        float font_sz = style.font_size;
        float reveal_btn_w = (m_data.is_password && m_data.show_reveal_button) ? 22.0f : 0.0f;
        float avail_w = std::max(10.0f, m_size.x - style.padding.horizontal() - reveal_btn_w);

        std::string display_text = get_display_text();
        float cursor_rel_x = TextMeasure::measure_width(std::string_view(display_text).substr(0, m_cursor_index), font_sz);
        if (cursor_rel_x - m_scroll_offset_x > avail_w - 4.0f) {
            m_scroll_offset_x = cursor_rel_x - avail_w + 12.0f;
        } else if (cursor_rel_x - m_scroll_offset_x < 0.0f) {
            m_scroll_offset_x = std::max(0.0f, cursor_rel_x - 12.0f);
        }
    }

    void select_word_at(float click_x, const std::string& str, float font_sz) {
        size_t idx = get_char_index_at(click_x, str, font_sz);
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

    [[nodiscard]] size_t get_char_index_at(float x, const std::string& str, float font_sz) const {
        if (x <= 0.0f || str.empty()) return 0;
        size_t byte_pos = 0;
        float prev_w = 0.0f;

        while (byte_pos < str.length()) {
            size_t next_pos = next_utf8_char_index(str, byte_pos);
            float curr_w = TextMeasure::measure_width(std::string_view(str).substr(0, next_pos), font_sz);
            float mid_x = (prev_w + curr_w) * 0.5f;
            if (x < mid_x) {
                return byte_pos;
            }
            prev_w = curr_w;
            byte_pos = next_pos;
        }
        return str.length();
    }

    static bool is_word_char(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }

    static size_t prev_utf8_char_index(const std::string& str, size_t index) noexcept {
        if (index == 0) return 0;
        size_t pos = index - 1;
        while (pos > 0 && (static_cast<unsigned char>(str[pos]) & 0xC0) == 0x80) {
            pos--;
        }
        return pos;
    }

    static size_t next_utf8_char_index(const std::string& str, size_t index) noexcept {
        if (index >= str.length()) return str.length();
        size_t pos = index + 1;
        while (pos < str.length() && (static_cast<unsigned char>(str[pos]) & 0xC0) == 0x80) {
            pos++;
        }
        return pos;
    }

    static size_t count_utf8_chars(const std::string& str) noexcept {
        size_t count = 0;
        size_t pos = 0;
        while (pos < str.length()) {
            pos = next_utf8_char_index(str, pos);
            count++;
        }
        return count;
    }

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

    // 动效管理器托管的 Fluent 底部活动高亮线驱动器
    MotionTween<float> m_focus_line_tween;
    std::chrono::steady_clock::time_point m_last_tick_time;

    // 自定义覆盖属性
    std::optional<Color> m_custom_bg;
    std::optional<Color> m_custom_border;
    std::optional<Color> m_custom_focused_border;
    std::optional<Color> m_custom_text;
    std::optional<Color> m_custom_placeholder;
    std::optional<float> m_custom_height;
    std::optional<float> m_custom_radius;
    std::optional<EdgeInsets> m_custom_padding;
    std::optional<float> m_custom_font_size;
};

class TextBoxWidget;

// C++20 指定初始化器声明式单行文本框 (TextBox DSL Wrapper)
// 桌面专业标准：默认 28px 高度，3px 硬朗微圆角，支持双向绑定响应式同步
struct TextBox {
    std::string text{""};
    std::string placeholder{""};
    BindRef<std::string> bind_text{nullptr};
    BindRef<bool> bind_disabled{nullptr};
    BindRef<bool> bind_read_only{nullptr};

    float height{0.0f};           // 0 表示按主题解析默认值 28px (支持显式传递 26px 向下兼容)
    float corner_radius{0.0f};    // 0 表示按主题解析默认值 3px
    EdgeInsets padding{EdgeInsets(-1.0f, -1.0f)}; // 负值代表采用主题解析默认值
    Color background_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color focused_border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color text_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color placeholder_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    float font_size{0.0f};

    bool is_password{false};
    bool is_read_only{false};
    bool disabled{false};
    bool show_reveal_button{false};

    std::function<void(const std::string&)> on_change{nullptr};
    std::function<void(const std::string&)> on_submit{nullptr};

    operator WidgetPtr() const;
};

// TextBoxWidget 声明式包装组件
class TextBoxWidget : public Widget {
public:
    explicit TextBoxWidget(const TextBox& desc) : m_desc(desc) {}

    ~TextBoxWidget() override {
        unsubscribe();
    }

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderTextBox>();
        apply_desc_to_node(node);
        setup_bindings(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto box = std::dynamic_pointer_cast<RenderTextBox>(node)) {
            apply_desc_to_node(box);
            setup_bindings(box);
        }
    }

    [[nodiscard]] const TextBox& desc() const noexcept { return m_desc; }

private:
    void apply_desc_to_node(std::shared_ptr<RenderTextBox> node) {
        std::string init_text = m_desc.bind_text ? m_desc.bind_text->get() : m_desc.text;
        node->set_text(init_text);
        node->set_placeholder(m_desc.placeholder);
        node->set_is_password(m_desc.is_password);
        node->set_read_only(m_desc.is_read_only);

        bool is_dis = m_desc.bind_disabled ? m_desc.bind_disabled->get() : m_desc.disabled;
        node->set_disabled(is_dis);

        // 应用显式覆盖样式
        std::optional<Color> bg = (m_desc.background_color.r >= 0.0f) ? std::optional<Color>(m_desc.background_color) : std::nullopt;
        std::optional<Color> border = (m_desc.border_color.r >= 0.0f) ? std::optional<Color>(m_desc.border_color) : std::nullopt;
        std::optional<Color> focus_border = (m_desc.focused_border_color.r >= 0.0f) ? std::optional<Color>(m_desc.focused_border_color) : std::nullopt;
        std::optional<Color> txt = (m_desc.text_color.r >= 0.0f) ? std::optional<Color>(m_desc.text_color) : std::nullopt;
        std::optional<Color> ph = (m_desc.placeholder_color.r >= 0.0f) ? std::optional<Color>(m_desc.placeholder_color) : std::nullopt;
        std::optional<float> h = (m_desc.height > 0.0f) ? std::optional<float>(m_desc.height) : std::nullopt;
        std::optional<float> radius = (m_desc.corner_radius > 0.0f) ? std::optional<float>(m_desc.corner_radius) : std::nullopt;
        std::optional<EdgeInsets> pad = (m_desc.padding.left >= 0.0f) ? std::optional<EdgeInsets>(m_desc.padding) : std::nullopt;
        std::optional<float> font_sz = (m_desc.font_size > 0.0f) ? std::optional<float>(m_desc.font_size) : std::nullopt;

        node->apply_custom_overrides(bg, border, focus_border, txt, ph, h, radius, pad, font_sz);

        // 挂接双向通知代理
        auto on_ch = [this, weak_node = std::weak_ptr<RenderTextBox>(node)](const std::string& val) {
            m_is_internal_update = true;
            if (m_desc.bind_text) {
                m_desc.bind_text->set(val);
            }
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
            m_is_internal_update = false;
        };

        node->set_properties(init_text, m_desc.placeholder,
                             h.value_or(28.0f), radius.value_or(3.0f),
                             pad.value_or(EdgeInsets(4.0f, 8.0f)),
                             m_desc.background_color, m_desc.border_color, m_desc.focused_border_color,
                             m_desc.text_color, m_desc.placeholder_color,
                             font_sz.value_or(13.0f), on_ch, m_desc.on_submit);
    }

    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_text) {
            m_desc.bind_text->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_bindings(std::shared_ptr<RenderTextBox> node) {
        unsubscribe();
        if (m_desc.bind_text) {
            std::weak_ptr<RenderTextBox> weak_box = node;
            m_binding_token = m_desc.bind_text->subscribe([this, weak_box](const std::string& new_val) {
                if (m_is_internal_update) return; // 避免双向死循环
                if (auto box = weak_box.lock()) {
                    box->set_text(new_val);
                }
            });
        }
    }

    TextBox m_desc;
    uint64_t m_binding_token{0};
    bool m_is_internal_update{false};
};

inline TextBox::operator WidgetPtr() const {
    return std::make_shared<TextBoxWidget>(*this);
}

// ==================== 100% 向后兼容别名 (Type Aliases) ====================
using TextInput = TextBox;
using RenderTextInput = RenderTextBox;
using TextInputWidget = TextBoxWidget;

} // namespace ktui
