#pragma once

// From Uiverse.io by WhiteNervosa - https://uiverse.io/WhiteNervosa/bad-mole-33
// MIT License, Copyright (c) WhiteNervosa (WhiteNervosa)

#include "textbox_data.hpp"
#include "textbox_style.hpp"
#include <ktui/animation/animation.hpp>
#include <ktui/animation/cubic_bezier.hpp>
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

    [[nodiscard]] AccessibilityProperties compute_accessibility() const override {
        auto p = RenderBox::compute_accessibility();
        p.role = m_data.is_password ? AccessibilityRole::PasswordBox : AccessibilityRole::TextBox;
        if (p.name.empty()) p.name = m_data.placeholder;
        p.is_password = m_data.is_password;
        p.value = (m_data.is_password && !m_data.is_password_revealed) ? std::string{} : m_data.text;
        p.disabled = m_state.is_disabled;
        return p;
    }
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
        float desired_w = 120.0f; // 默认紧凑最小宽度
        float desired_h = style.height;
        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
        ensure_cursor_visible();
    }

    // ==================== 绘制管线 (Painting Pipeline) ====================

    void paint(PaintContext& context) override {
        auto style = resolve_current_style();

        // 1. 推进 Uiverse (WhiteNervosa/bad-mole-33) 风格四路平滑过渡动效：
        //    - :after 2px 聚焦底条 scaleX (250ms cubic-bezier(0, 0, 0.2, 1))
        //    - :before 1px 底线颜色过渡 (200ms cubic-bezier(0.4, 0, 0.2, 1))
        //    - placeholder 聚焦渐隐透明度 (250ms cubic-bezier(0, 0, 0.2, 1))
        //    - background-color 背景平滑过渡 (200ms cubic-bezier(0.25, 0.8, 0.25, 1))
        auto now = std::chrono::steady_clock::now();
        float dt = 0.0f;
        if (m_last_tick_time.time_since_epoch().count() != 0) {
            dt = std::clamp(std::chrono::duration<float>(now - m_last_tick_time).count(), 0.0f, 0.1f);
        }
        m_last_tick_time = now;

        // 检测目标背景色变更，启动 200ms cubic-bezier(0.25, 0.8, 0.25, 1) 背景过渡
        if (!m_bg_initialized) {
            m_bg_initialized = true;
            m_bg_from = style.background;
            m_bg_to = style.background;
            m_bg_current = style.background;
            m_bg_elapsed = kBgDurationSec;
        } else if (m_bg_to.r != style.background.r || m_bg_to.g != style.background.g ||
                   m_bg_to.b != style.background.b || m_bg_to.a != style.background.a) {
            m_bg_from = m_bg_current;
            m_bg_to = style.background;
            m_bg_elapsed = 0.0f;
        }

        bool any_anim_active = false;
        if (m_bg_elapsed < kBgDurationSec) {
            m_bg_elapsed = std::min(kBgDurationSec, m_bg_elapsed + dt);
            float t = kCurveBg.evaluate(m_bg_elapsed / kBgDurationSec);
            m_bg_current = lerp_color(m_bg_from, m_bg_to, t);
            if (m_bg_elapsed < kBgDurationSec) any_anim_active = true;
        } else {
            m_bg_current = m_bg_to;
        }

        if (m_focus_line_tween.is_active()) {
            if (dt > 0.0f) m_focus_line_tween.tick(dt);
            if (m_focus_line_tween.is_active()) any_anim_active = true;
        }

        if (m_before_elapsed < kBeforeDurationSec) {
            m_before_elapsed = std::min(kBeforeDurationSec, m_before_elapsed + dt);
            float t = kCurveBefore.evaluate(m_before_elapsed / kBeforeDurationSec);
            m_before_progress = m_before_from + (m_before_to - m_before_from) * t;
            if (m_before_elapsed < kBeforeDurationSec) any_anim_active = true;
        } else {
            m_before_progress = m_before_to;
        }

        if (m_after_elapsed < kAfterDurationSec) {
            m_after_elapsed = std::min(kAfterDurationSec, m_after_elapsed + dt);
            float t = kCurveAfter.evaluate(m_after_elapsed / kAfterDurationSec);
            m_after_scale = m_after_from + (m_after_to - m_after_from) * t;
            if (m_after_elapsed < kAfterDurationSec) any_anim_active = true;
        } else {
            m_after_scale = m_after_to;
        }

        if (m_placeholder_elapsed < kPlaceholderDurationSec) {
            m_placeholder_elapsed = std::min(kPlaceholderDurationSec, m_placeholder_elapsed + dt);
            float t = kCurveAfter.evaluate(m_placeholder_elapsed / kPlaceholderDurationSec);
            m_placeholder_opacity = m_placeholder_from + (m_placeholder_to - m_placeholder_from) * t;
            if (m_placeholder_elapsed < kPlaceholderDurationSec) any_anim_active = true;
        } else {
            m_placeholder_opacity = m_placeholder_to;
        }

        if (any_anim_active) {
            mark_needs_paint();
        }

        // 2. 检查光标闪烁状态 (530ms 闪烁驱动)
        update_blink_state(style.caret_blink_ms);

        // 3. 绘制半透明顶部圆角底色 (5px 5px 0px 0px，无阴影无边框)
        if (m_bg_current.a > 0.001f) {
            context.draw_rounded_rect(bounds(), m_bg_current,
                                      style.corner_radius, style.corner_radius, 0.0f, 0.0f);
        }

        // 4. 绘制四周描边 (仅当外部显式指定 custom_border 且 border_width > 0 时才绘制)
        if (style.border_width > 0.0f && style.border.a > 0.001f) {
            context.draw_rect(bounds(), style.border, style.corner_radius, style.border_width);
        }

        // 4.1 底部全宽 1px 底线 (:before)，聚焦时 200ms 平滑过渡为主题强调色
        float resting_line_y = bounds().max_y - style.underline_height;
        Color before_col = m_state.is_disabled
            ? style.underline
            : lerp_color(style.underline, style.focus_indicator, m_before_progress);
        if (before_col.a > 0.001f && style.underline_height > 0.0f) {
            context.draw_rect(Rect::from_xywh(bounds().min_x, resting_line_y, bounds().width(), style.underline_height),
                              before_col, 0.0f, 0.0f);
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

        const std::string display_text = composed_display_text();
        const size_t preedit_start = to_display_offset(m_cursor_index);
        float font_sz = style.font_size;
        float line_height = TextMeasure::measure(display_text, font_sz).y;
        float text_y = content_rect.min_y + (content_rect.height() - line_height) * 0.5f;
        float start_x = content_rect.min_x - m_scroll_offset_x;

        // 6. 绘制主体文字或占位符 (聚焦时占位符 250ms 平滑淡出至 opacity: 0)
        if (!display_text.empty()) {
            context.draw_text(display_text, PointF{start_x, text_y}, font_sz, style.text);

            // 7. WinUI 3 原生选中样式：实心强调色选区底块 + 裁剪反白前景文字 (#FFFFFF)
            if (m_state.is_focused && has_selection() && m_ime_preedit.empty()) {
                size_t sel_s = selection_start();
                size_t sel_e = selection_end();
                float sel_x1 = start_x + TextMeasure::cursor_x(display_text, font_sz, to_display_offset(sel_s));
                float sel_x2 = start_x + TextMeasure::cursor_x(display_text, font_sz, to_display_offset(sel_e));
                float sel_h = std::min(content_rect.height(), std::max(line_height + 2.0f, font_sz + 4.0f));
                float sel_y = content_rect.min_y + (content_rect.height() - sel_h) * 0.5f;
                Rect sel_rect = Rect::from_xywh(
                    std::min(sel_x1, sel_x2),
                    sel_y,
                    std::abs(sel_x2 - sel_x1),
                    sel_h
                );
                if (sel_rect.width() > 0.0f) {
                    context.draw_rect(sel_rect, style.selection, 0.0f, 0.0f);
                    context.push_clip(sel_rect, 0.0f);
                    context.draw_text(display_text, PointF{start_x, text_y}, font_sz, style.selection_text);
                    context.pop_clip();
                }
            }
        } else if (!m_data.placeholder.empty() && m_ime_preedit.empty() && m_placeholder_opacity > 0.005f) {
            Color ph_col = style.placeholder;
            ph_col.a *= m_placeholder_opacity;
            context.draw_text(m_data.placeholder, PointF{start_x, text_y}, font_sz, ph_col);
        }

        // 8. 绘制 IME 输入法预编辑组合态下划线 (如拼音 "nihao")
        if (!m_ime_preedit.empty()) {
            float preedit_start_x = start_x + TextMeasure::cursor_x(display_text, font_sz, preedit_start);
            float preedit_end_x = start_x + TextMeasure::cursor_x(display_text, font_sz, preedit_start + m_ime_preedit.size());
            float underline_y = bounds().max_y - style.padding.bottom - 1.0f;
            context.draw_rect(Rect::from_xywh(std::min(preedit_start_x, preedit_end_x), underline_y, std::abs(preedit_end_x - preedit_start_x), 1.5f), style.focus_indicator, 0.0f, 0.0f);
        }

        // 9. 绘制精致 1.5px 闪烁光标
        if (m_state.is_focused && m_blink_visible && !m_state.is_disabled) {
            size_t visual_cursor = preedit_start + std::min(m_ime_preedit_cursor, m_ime_preedit.size());
            float cursor_x = start_x + TextMeasure::cursor_x(display_text, font_sz, visual_cursor);
            float caret_h = font_sz + 2.0f;
            float caret_y = bounds().min_y + (bounds().height() - caret_h) * 0.5f;
            context.draw_rect(Rect::from_xywh(cursor_x, caret_y, style.caret_width, caret_h), style.caret, 0.0f, 0.0f);
        }

        context.pop_clip();

        // 10. 底部 2px 动态聚焦展开条 (:after, transform: scaleX(0) -> scaleX(1), 250ms)
        if (m_after_scale > 0.001f && !m_state.is_disabled) {
            float active_w = bounds().width() * m_after_scale;
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
                    if (has_selection() && (!m_data.is_password || m_data.is_password_revealed)) {
                        Clipboard::set_text(selected_text());
                    }
                    return;
                } else if (kc == 'X' || kc == 'x') {
                    if (has_selection() && !m_state.is_read_only && (!m_data.is_password || m_data.is_password_revealed)) {
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
            trigger_focus_animations(ev.gained_focus);
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
            if (m_state.is_disabled) return;
            if (ev.button == PointerButton::Secondary) {
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
                actions.on_select_all = [this]() { select_all(); };
                show_winui_text_context_menu(ev.position, actions);
                ev.stop_propagation();
                return;
            }
            if (ev.button != PointerButton::Primary) return;

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
            std::string_view display_text = get_display_text();

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
                std::string_view display_text = get_display_text();
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
        float font_sz = style.font_size;
        float reveal_btn_w = (m_data.is_password && m_data.show_reveal_button) ? 22.0f : 0.0f;
        float avail_w = std::max(10.0f, m_size.x - style.padding.horizontal() - reveal_btn_w);

        const auto display_text = composed_display_text();
        const size_t cursor = to_display_offset(m_cursor_index) + std::min(m_ime_preedit_cursor, m_ime_preedit.size());
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
};

class TextBoxWidget;

// C++20 指定初始化器声明式单行文本框 (TextBox DSL Wrapper - 全属性支持 Bind(...))
// 桌面专业标准：默认 28px 高度，3px 硬朗微圆角，支持双向绑定响应式同步
struct TextBox {
    Bindable<std::string> text{""};
    Bindable<std::string> placeholder{""};

    Bindable<float> height{0.0f};           // 0 表示按主题解析默认值 28px
    Bindable<float> corner_radius{0.0f};    // 0 表示按主题解析默认值 3px
    Bindable<EdgeInsets> padding{EdgeInsets(-1.0f, -1.0f)}; // 负值代表采用主题解析默认值
    Bindable<Color> background_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<Color> border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<Color> focused_border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<Color> text_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<Color> placeholder_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<float> font_size{0.0f};

    Bindable<bool> is_password{false};
    Bindable<bool> is_read_only{false};
    Bindable<bool> disabled{false};
    Bindable<bool> show_reveal_button{false};

    std::function<void(const std::string&)> on_change{nullptr};
    std::function<void(const std::string&)> on_submit{nullptr};

    operator WidgetPtr() const;
};

// TextBoxWidget 声明式包装组件
class TextBoxWidget : public Widget {
public:
    explicit TextBoxWidget(const TextBox& desc) : m_desc(desc) {}
    ~TextBoxWidget() override = default;

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
    void apply_desc_to_node(const std::shared_ptr<RenderTextBox>& node) {
        std::string init_text = m_desc.text.get();
        std::string init_ph = m_desc.placeholder.get();
        node->set_text(init_text);
        node->set_placeholder(init_ph);
        node->set_is_password(m_desc.is_password.get());
        node->set_read_only(m_desc.is_read_only.get());
        node->set_disabled(m_desc.disabled.get());
        node->set_show_reveal_button(m_desc.show_reveal_button.get());

        Color bg_c = m_desc.background_color.get();
        Color brd_c = m_desc.border_color.get();
        Color fbrd_c = m_desc.focused_border_color.get();
        Color txt_c = m_desc.text_color.get();
        Color ph_c = m_desc.placeholder_color.get();
        float h_v = m_desc.height.get();
        float rad_v = m_desc.corner_radius.get();
        EdgeInsets pad_v = m_desc.padding.get();
        float fs_v = m_desc.font_size.get();

        std::optional<Color> bg = (bg_c.r >= 0.0f) ? std::optional<Color>(bg_c) : std::nullopt;
        std::optional<Color> border = (brd_c.r >= 0.0f) ? std::optional<Color>(brd_c) : std::nullopt;
        std::optional<Color> focus_border = (fbrd_c.r >= 0.0f) ? std::optional<Color>(fbrd_c) : std::nullopt;
        std::optional<Color> txt = (txt_c.r >= 0.0f) ? std::optional<Color>(txt_c) : std::nullopt;
        std::optional<Color> ph = (ph_c.r >= 0.0f) ? std::optional<Color>(ph_c) : std::nullopt;
        std::optional<float> h = (h_v > 0.0f) ? std::optional<float>(h_v) : std::nullopt;
        std::optional<float> radius = (rad_v > 0.0f) ? std::optional<float>(rad_v) : std::nullopt;
        std::optional<EdgeInsets> pad = (pad_v.left >= 0.0f) ? std::optional<EdgeInsets>(pad_v) : std::nullopt;
        std::optional<float> font_sz = (fs_v > 0.0f) ? std::optional<float>(fs_v) : std::nullopt;

        node->apply_custom_overrides(bg, border, focus_border, txt, ph, h, radius, pad, font_sz);

        // 挂接双向通知代理
        auto on_ch = [this](const std::string& val) {
            m_is_internal_update = true;
            m_desc.text.set_bound(val);
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
            m_is_internal_update = false;
        };

        node->set_properties(init_text, init_ph,
                             h.value_or(28.0f), radius.value_or(5.0f),
                             pad.value_or(EdgeInsets(4.0f, 8.0f)),
                             bg_c, brd_c, fbrd_c, txt_c, ph_c,
                             font_sz.value_or(13.0f), on_ch, m_desc.on_submit);
    }

    void setup_bindings(const std::shared_ptr<RenderTextBox>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.text, node, [this](RenderTextBox& n, const std::string& new_val) {
            if (!m_is_internal_update) n.set_text(new_val);
        });
        m_bindings.attach(m_desc.placeholder, node, [](RenderTextBox& n, const std::string& v) { n.set_placeholder(v); });
        m_bindings.attach(m_desc.height, node, [](RenderTextBox& n, float v) { n.set_custom_height(v); });
        m_bindings.attach(m_desc.corner_radius, node, [](RenderTextBox& n, float v) { n.set_custom_corner_radius(v); });
        m_bindings.attach(m_desc.padding, node, [](RenderTextBox& n, EdgeInsets v) { n.set_custom_padding(v); });
        m_bindings.attach(m_desc.background_color, node, [](RenderTextBox& n, Color v) { n.set_custom_background(v); });
        m_bindings.attach(m_desc.border_color, node, [](RenderTextBox& n, Color v) { n.set_custom_border_color(v); });
        m_bindings.attach(m_desc.focused_border_color, node, [](RenderTextBox& n, Color v) { n.set_custom_focused_border_color(v); });
        m_bindings.attach(m_desc.text_color, node, [](RenderTextBox& n, Color v) { n.set_custom_text_color(v); });
        m_bindings.attach(m_desc.placeholder_color, node, [](RenderTextBox& n, Color v) { n.set_custom_placeholder_color(v); });
        m_bindings.attach(m_desc.font_size, node, [](RenderTextBox& n, float v) { n.set_custom_font_size(v); });
        m_bindings.attach(m_desc.is_password, node, [](RenderTextBox& n, bool v) { n.set_is_password(v); });
        m_bindings.attach(m_desc.is_read_only, node, [](RenderTextBox& n, bool v) { n.set_read_only(v); });
        m_bindings.attach(m_desc.disabled, node, [](RenderTextBox& n, bool v) { n.set_disabled(v); });
        m_bindings.attach(m_desc.show_reveal_button, node, [](RenderTextBox& n, bool v) { n.set_show_reveal_button(v); });
    }

    TextBox m_desc;
    BindingGroup m_bindings;
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
