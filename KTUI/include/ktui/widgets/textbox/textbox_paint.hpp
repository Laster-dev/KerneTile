#pragma once

namespace ktui {

inline void RenderTextBox::paint(PaintContext& context) {
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
    if (!m_borderless_inline && before_col.a > 0.001f && style.underline_height > 0.0f) {
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

    // 7.5 增量式幽灵补全后缀 (紧跟真实文本之后，复用原生 TextBox 排版与裁剪)
    if (!m_ghost_suffix.empty() && m_ime_preedit.empty() && !display_text.empty()) {
        float ghost_x = start_x + TextMeasure::measure_width(display_text, font_sz);
        Color ghost_col = style.placeholder;
        ghost_col.a = std::min(ghost_col.a, 0.45f);
        context.draw_text(m_ghost_suffix, PointF{ghost_x, text_y}, font_sz, ghost_col);
    }

    // 8. 绘制 IME 输入法预编辑组合态下划线 (如拼音 "nihao")
    if (!m_ime_preedit.empty()) {
        float preedit_start_x = start_x + TextMeasure::cursor_x(display_text, font_sz, preedit_start);
        float preedit_end_x = start_x + TextMeasure::cursor_x(display_text, font_sz, preedit_start + m_ime_preedit.size());
        float underline_y = bounds().max_y - style.padding.bottom - 1.0f;
        context.draw_rect(Rect::from_xywh(std::min(preedit_start_x, preedit_end_x), underline_y, std::abs(preedit_end_x - preedit_start_x), 1.5f), style.focus_indicator, 0.0f, 0.0f);
    }

    // 9. 绘制精致 1.5px 闪烁光标 (默认启用平滑弹簧光标 + 呼吸渐变闪烁，支持手动关闭)
    if (m_state.is_focused && !m_state.is_disabled) {
        size_t visual_cursor = preedit_start + std::min(m_ime_preedit_cursor, m_ime_preedit.size());
        float cursor_x = start_x + TextMeasure::cursor_x(display_text, font_sz, visual_cursor);
        float caret_h = font_sz + 2.0f;
        float caret_y = bounds().min_y + (bounds().height() - caret_h) * 0.5f;
        if (m_smooth_caret) {
            auto frame = m_spring_caret.evaluate(cursor_x, caret_y, style.caret_width, caret_h, true, m_state.is_disabled);
            if (frame.visible) {
                Color caret_col = style.caret;
                caret_col.a *= frame.opacity;
                if (caret_col.a > 0.01f) {
                    context.draw_rect(frame.rect, caret_col, style.caret_width * 0.5f, 0.0f);
                }
            }
            if (frame.animating) any_anim_active = true;
        } else if (m_blink_visible) {
            context.draw_rect(Rect::from_xywh(cursor_x, caret_y, style.caret_width, caret_h), style.caret, 0.0f, 0.0f);
        }
    } else if (m_smooth_caret) {
        m_spring_caret.reset();
    }

    context.pop_clip();

    // 10. 底部 2px 动态聚焦展开条 (:after, transform: scaleX(0) -> scaleX(1), 250ms)
    if (!m_borderless_inline && m_after_scale > 0.001f && !m_state.is_disabled) {
        float active_w = bounds().width() * m_after_scale;
        float active_x = bounds().min_x + (bounds().width() - active_w) * 0.5f;
        float line_y = bounds().max_y - style.focus_line_height;
        context.draw_rect(Rect::from_xywh(active_x, line_y, active_w, style.focus_line_height), style.focus_indicator, 0.0f, 0.0f);
    }

    // 10.5 幽灵补全右侧按键提示胶囊 (如 "Tab 补词") 或状态指示圆点 (如 IP 校验状态)
    if (!m_ghost_suffix.empty() && !m_ghost_badge.empty()) {
        const Theme& t = ThemeContext::current();
        Vec2 bs = TextMeasure::measure(m_ghost_badge, 10.5f, FontWeight::Medium);
        float bw = bs.x + 12.0f;
        Rect br = Rect::from_xywh(bounds().max_x - bw - 6.0f, (bounds().height() - 20.0f) * 0.5f, bw, 20.0f);
        context.draw_rect(br, t.is_dark ? Color{1, 1, 1, 0.08f} : Color{0, 0, 0, 0.05f}, 4.0f, 0.0f);
        context.draw_rect(br, t.is_dark ? Color{1, 1, 1, 0.14f} : Color{0, 0, 0, 0.12f}, 4.0f, 1.0f);
        context.draw_text(m_ghost_badge,
                          PointF{std::round(br.min_x + 6.0f), std::round(br.min_y + (20.0f - bs.y) * 0.5f)},
                          10.5f, style.placeholder, FontWeight::Medium);
    } else if (m_show_status_dot) {
        float r = 3.5f;
        float cx = bounds().max_x - 11.0f;
        float cy = bounds().min_y + bounds().height() * 0.5f;
        context.draw_rect(Rect::from_xywh(cx - r, cy - r, r * 2.0f, r * 2.0f), m_status_dot_color, r, 0.0f);
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

} // namespace ktui
