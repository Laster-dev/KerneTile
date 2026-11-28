#pragma once

namespace ktui {

inline void RenderTextArea::paint(PaintContext& ctx) {
    const Theme& theme = ThemeContext::current();
    bool anim_f = FluentInputChrome::step_focus_anim(m_focus_anim, m_focused && !m_disabled);
    bool anim_s = m_physics.update();
    bool anim_b = m_scrollbar.update_animations(m_hovered);
    if (anim_f || anim_s || anim_b) mark_needs_paint();

    Rect box = box_rect();
    // WinUI 3 TextBox / RichEditBox 原生风格底板
    Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f}
                                 : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
    Color ph_c   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.54f} : Color{0.0f, 0.0f, 0.0f, 0.54f};
    Color winui_accent = theme.is_dark
        ? Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}  // WinUI 3 Dark #60CDFF
        : Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};            // WinUI 3 Light #005FB8

    if (!m_header.empty()) {
        ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, text_c);
    }

    Color bg, border, bottom_line;
    if (theme.is_dark) {
        bg = m_disabled ? Color{1.0f, 1.0f, 1.0f, 0.03f}
           : (m_focused ? Color{30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 0.96f}
                        : (m_hovered ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{1.0f, 1.0f, 1.0f, 0.055f}));
        border      = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.05f : 0.08f};
        bottom_line = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.06f : 0.54f};
    } else {
        bg = m_disabled ? Color{0.96f, 0.96f, 0.96f, 0.55f}
           : (m_focused ? Color{1.0f, 1.0f, 1.0f, 1.0f}
                        : (m_hovered ? Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.65f}
                                     : Color{1.0f, 1.0f, 1.0f, 0.72f}));
        border      = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.05f : 0.08f};
        bottom_line = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.06f : 0.45f};
    }

    ctx.draw_rect(box, bg, 4.0f, 0.0f);
    ctx.draw_rect(box, border, 4.0f, 1.0f);
    ctx.push_clip(box, 4.0f);
    ctx.draw_rect(Rect::from_xywh(box.min_x, box.max_y - 1.0f, box.width(), 1.0f), bottom_line, 0.0f, 0.0f);
    if (m_focus_anim > 0.01f && !m_disabled) {
        float half_w = box.width() * 0.5f * m_focus_anim;
        float cx = (box.min_x + box.max_x) * 0.5f;
        ctx.draw_rect(Rect::from_xywh(cx - half_w, box.max_y - 2.0f, half_w * 2.0f, 2.0f), winui_accent, 0.0f, 0.0f);
    }
    ctx.pop_clip();

    Rect clip = Rect::from_xywh(box.min_x + 2.0f, box.min_y + 2.0f,
                                std::max(0.0f, box.width() - 4.0f),
                                std::max(0.0f, box.height() - 4.0f));
    ctx.push_clip(clip, 3.0f);

    float scroll_y = m_physics.offset();
    const bool show_sel = has_selection() && m_ime_preedit.empty();
    const size_t s_start = sel_start();
    const size_t s_end = sel_end();
    Color sel_col = FluentInputChrome::selection_bg(theme, m_focused);
    Color sel_txt_col = FluentInputChrome::selection_text(theme);

    if (m_text.empty() && m_ime_preedit.empty()) {
        if (!m_placeholder.empty()) {
            ctx.draw_text(m_placeholder, PointF{box.min_x + 11.0f, box.min_y + 8.0f}, kFontSize, ph_c);
        }
    } else {
        const size_t pre_start = m_caret;
        const size_t pre_end = m_caret + m_ime_preedit.size();
        for (size_t i = 0; i < m_lines.size(); ++i) {
            float ly = box.min_y + 8.0f + static_cast<float>(i) * kLineH - scroll_y;
            if (ly + kLineH < box.min_y || ly > box.max_y) continue;
            const auto& line = m_lines[i];

            // 先绘制常规行文本
            if (!line.text.empty()) {
                ctx.draw_text(line.text, PointF{box.min_x + 11.0f, ly}, kFontSize, text_c);
            }

            // WinUI 3 原生逐行选区：聚焦时实心强调色底块 + 裁剪反白前景文字 (#FFFFFF)
            if (show_sel) {
                size_t ov_s = std::max(s_start, line.start);
                size_t ov_e = std::min(s_end, line.end);
                bool spans_newline = (s_start <= line.end && s_end > line.end);
                if (ov_s < ov_e || spans_newline) {
                    size_t rel_s = (ov_s > line.start) ? std::min(ov_s - line.start, line.text.size()) : 0;
                    size_t rel_e = (ov_e > line.start) ? std::min(ov_e - line.start, line.text.size()) : 0;
                    float x0 = box.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel_s), kFontSize);
                    float x1 = box.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel_e), kFontSize);
                    if (spans_newline) x1 += 6.0f;
                    if (x1 > x0) {
                        Rect sel_r = Rect::from_xywh(x0, ly - 1.0f, x1 - x0, kLineH);
                        ctx.draw_rect(sel_r, sel_col, 0.0f, 0.0f);
                        if (m_focused && !line.text.empty()) {
                            ctx.push_clip(sel_r, 0.0f);
                            ctx.draw_text(line.text, PointF{box.min_x + 11.0f, ly}, kFontSize, sel_txt_col);
                            ctx.pop_clip();
                        }
                    }
                }
            }

            // IME 预编辑下划线
            if (!m_ime_preedit.empty()) {
                size_t ps = std::max(pre_start, line.start);
                size_t pe = std::min(pre_end, line.end);
                if (ps < pe) {
                    size_t rel_s = std::min(ps - line.start, line.text.size());
                    size_t rel_e = std::min(pe - line.start, line.text.size());
                    float ux0 = box.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel_s), kFontSize);
                    float ux1 = box.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel_e), kFontSize);
                    ctx.draw_rect(Rect::from_xywh(ux0, ly + kLineH - 3.0f, std::max(2.0f, ux1 - ux0), 1.5f),
                                  winui_accent, 0.0f, 0.0f);
                }
            }
        }
    }

    if (m_focused && !m_disabled) {
        PointF cp = caret_local_pos();
        if (m_smooth_caret) {
            auto frame = m_spring_caret.evaluate(std::round(cp.x), std::round(cp.y), 1.5f, kLineH - 3.0f, true, false);
            if (frame.visible) {
                Color c = text_c;
                c.a *= frame.opacity;
                if (c.a > 0.01f) {
                    ctx.draw_rect(frame.rect, c, 0.75f, 0.0f);
                }
            }
            if (frame.animating) mark_needs_paint();
        } else if (m_caret_visible) {
            ctx.draw_rect(Rect::from_xywh(std::round(cp.x), std::round(cp.y), 1.5f, kLineH - 3.0f),
                          text_c, 0.5f, 0.0f);
        }
    } else if (m_smooth_caret) {
        m_spring_caret.reset();
    }

    float content_h = static_cast<float>(std::max<size_t>(1, m_lines.size())) * kLineH + 16.0f;
    m_scrollbar.paint(ctx, box, content_h, scroll_y, theme);
    ctx.pop_clip();
}

inline void RenderTextArea::setup_behavior() {
    on_pointer_enter = [this](PointerEvent&) {
        if (!m_disabled) { m_hovered = true; mark_needs_paint(); }
    };
    on_pointer_exit = [this](PointerEvent&) {
        m_hovered = false;
        m_scrollbar.handle_pointer_exit();
        mark_needs_paint();
    };
    on_pointer_move = [this](PointerEvent& e) {
        if (e.type == PlatformEventType::PointerScroll) {
            if (m_physics.max_scroll() > 0.0f) {
                m_physics.add_wheel_delta(e.delta.y);
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
            }
            return;
        }
        Rect b = box_rect();
        float content_h = static_cast<float>(std::max<size_t>(1, m_lines.size())) * kLineH + 16.0f;
        bool consumed = false;
        if (m_scrollbar.handle_pointer_move(e.local_position, b, content_h, m_physics, consumed)) {
            mark_needs_paint();
        }
        if (consumed) {
            e.stop_propagation();
            return;
        }
        if (m_dragging && !m_disabled) {
            if (!m_drag_threshold_passed) {
                float d = std::hypot(e.local_position.x - m_drag_start_local.x,
                                     e.local_position.y - m_drag_start_local.y);
                if (d < 3.5f) {
                    e.stop_propagation();
                    return;
                }
                m_drag_threshold_passed = true;
            }
            m_caret = byte_index_at_point(e.local_position);
            ensure_caret_visible();
            reset_blink();
            mark_needs_paint();
            e.stop_propagation();
        }
    };
    on_pointer_down = [this](PointerEvent& e) {
        if (m_disabled) return;
        if (e.button == PointerButton::Secondary) {
            m_pending_touch_collapse = false;
            m_dragging = false;
            m_drag_threshold_passed = false;
            const bool is_touch = (e.device_kind == PointerDeviceKind::Touch);
            if (is_touch && !has_selection() && !m_text.empty()) {
                select_word_at(byte_index_at_point(e.local_position));
            }
            if (is_touch) {
                m_show_touch_handles = true;
                sync_touch_handles();
            }
            open_context_menu(e.position, is_touch);
            e.stop_propagation();
            return;
        }
        if (e.button != PointerButton::Primary) return;

        Rect b = box_rect();
        float content_h = static_cast<float>(std::max<size_t>(1, m_lines.size())) * kLineH + 16.0f;
        if (m_scrollbar.handle_pointer_down(e.local_position, b, content_h, m_physics, 32.0f, e.device_kind)) {
            mark_needs_paint();
            e.stop_propagation();
            return;
        }

        const bool is_touch = (e.device_kind == PointerDeviceKind::Touch);
        if (!is_touch) {
            m_show_touch_handles = false;
            m_touch_handles.hide();
        }

        auto now = std::chrono::steady_clock::now();
        const bool has_prev = (m_last_click_time.time_since_epoch().count() != 0);
        const float dt_ms = has_prev
            ? std::chrono::duration<float, std::milli>(now - m_last_click_time).count()
            : 1e6f;
        const float click_dist = std::hypot(e.local_position.x - m_last_click_pos.x,
                                            e.local_position.y - m_last_click_pos.y);
        const float max_multi_dist = is_touch ? 18.0f : 6.0f;
        if (has_prev && dt_ms < 400.0f && click_dist <= max_multi_dist) {
            m_click_count = (m_click_count % 3) + 1;
        } else {
            m_click_count = 1;
        }
        m_last_click_time = now;
        m_last_click_pos = e.local_position;

        size_t idx = byte_index_at_point(e.local_position);
        if (m_click_count == 2) {
            m_pending_touch_collapse = false;
            m_dragging = false;
            m_drag_threshold_passed = false;
            select_word_at(idx);
            if (is_touch && has_selection()) {
                m_show_touch_handles = true;
                sync_touch_handles();
                open_context_menu(e.position, true);
            }
        } else if (m_click_count == 3) {
            m_pending_touch_collapse = false;
            m_dragging = false;
            m_drag_threshold_passed = false;
            select_line_at(idx);
            if (is_touch && has_selection()) {
                m_show_touch_handles = true;
                sync_touch_handles();
                open_context_menu(e.position, true);
            }
        } else {
            if (is_touch) {
                if (has_selection()) {
                    m_pending_touch_collapse = true;
                    m_pending_touch_collapse_idx = idx;
                } else {
                    m_pending_touch_collapse = false;
                    m_caret = idx;
                    m_sel_anchor = m_caret;
                }
                m_dragging = false;
                m_drag_threshold_passed = false;
            } else {
                m_pending_touch_collapse = false;
                m_caret = idx;
                if (!e.modifiers.shift) m_sel_anchor = m_caret;
                m_dragging = true;
                m_drag_start_local = e.local_position;
                m_drag_threshold_passed = false;
            }
        }
        reset_blink();
        mark_needs_paint();
        e.stop_propagation();
    };
    on_pointer_up = [this](PointerEvent& e) {
        const bool was_dragging = m_dragging;
        m_dragging = false;
        m_drag_threshold_passed = false;
        if (m_pending_touch_collapse) {
            m_pending_touch_collapse = false;
            m_caret = std::min(m_pending_touch_collapse_idx, m_text.size());
            m_sel_anchor = m_caret;
            reset_blink();
            mark_needs_paint();
        }
        if (e.device_kind == PointerDeviceKind::Touch && m_focused && !m_disabled) {
            if (!m_text.empty()) {
                m_show_touch_handles = true;
                sync_touch_handles();
            } else {
                m_show_touch_handles = false;
                m_touch_handles.hide();
            }
        }
        if (m_scrollbar.handle_pointer_up()) mark_needs_paint();
        if (was_dragging) e.stop_propagation();
    };
    on_pointer_cancel = [this](PointerEvent&) {
        m_hovered = false;
        m_pending_touch_collapse = false;
        m_dragging = false;
        m_drag_threshold_passed = false;
        m_scrollbar.handle_pointer_up();
        m_scrollbar.handle_pointer_exit();
        mark_needs_paint();
    };
    on_focus = [this](FocusEvent& e) {
        if (m_disabled) return;
        m_focused = e.gained_focus;
        if (m_focused) {
            reset_blink();
        } else {
            m_dragging = false;
            m_drag_threshold_passed = false;
            m_pending_touch_collapse = false;
            m_click_count = 0;
            m_last_click_time = {};
            m_show_touch_handles = false;
            m_touch_handles.hide();
            m_ime_preedit.clear();
            m_ime_preedit_cursor = 0;
        }
        mark_needs_paint();
    };
    on_key = [this](KeyEvent& e) {
        if (m_disabled) return;
        if (e.type == PlatformEventType::ImeCommit) {
            if (!e.text.empty()) {
                on_ime_commit(e.text);
                e.stop_propagation();
            }
            return;
        }
        if (e.type != PlatformEventType::KeyDown) return;
        if (m_show_touch_handles) {
            m_show_touch_handles = false;
            m_touch_handles.hide();
        }

        if (e.modifiers.ctrl) {
            uint32_t kc = e.key_code;
            if (kc == 'A' || kc == 'a') {
                select_all();
                e.stop_propagation();
                return;
            }
            if (kc == 'C' || kc == 'c') {
                if (has_selection()) Clipboard::set_text(selected_text());
                e.stop_propagation();
                return;
            }
            if (kc == 'X' || kc == 'x') {
                if (has_selection()) {
                    Clipboard::set_text(selected_text());
                    delete_selection();
                }
                e.stop_propagation();
                return;
            }
            if (kc == 'V' || kc == 'v') {
                std::string clip = Clipboard::get_text();
                if (!clip.empty()) insert_text(clip);
                e.stop_propagation();
                return;
            }
            if (kc == 'Z' || kc == 'z') {
                if (e.modifiers.shift) redo();
                else undo();
                e.stop_propagation();
                return;
            }
            if (kc == 'Y' || kc == 'y') {
                redo();
                e.stop_propagation();
                return;
            }
            if (kc == 36) { // Ctrl+Home -> 文档首
                m_caret = 0;
                if (!e.modifiers.shift) m_sel_anchor = 0;
                ensure_caret_visible();
                reset_blink();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (kc == 35) { // Ctrl+End -> 文档尾
                m_caret = m_text.size();
                if (!e.modifiers.shift) m_sel_anchor = m_caret;
                ensure_caret_visible();
                reset_blink();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
        }

        switch (e.key_code) {
            case 13: // Enter -> 换行
                insert_text("\n");
                e.stop_propagation();
                return;
            case 9: // Tab -> 4 空格缩进
                insert_text("    ");
                e.stop_propagation();
                return;
            case 38: // Up
                move_vertical(-1, e.modifiers.shift);
                e.stop_propagation();
                return;
            case 40: // Down
                move_vertical(1, e.modifiers.shift);
                e.stop_propagation();
                return;
            case 33: // PageUp
                move_vertical(-5, e.modifiers.shift);
                e.stop_propagation();
                return;
            case 34: // PageDown
                move_vertical(5, e.modifiers.shift);
                e.stop_propagation();
                return;
            case 8: // Backspace
                if (has_selection()) {
                    delete_selection();
                } else if (m_caret > 0) {
                    push_undo_snapshot();
                    size_t p = SingleLineTextEditCore::prev_utf8(m_text, m_caret);
                    m_text.erase(p, m_caret - p);
                    m_caret = p;
                    m_sel_anchor = p;
                    notify_edited();
                }
                e.stop_propagation();
                return;
            case 46: // Delete
                if (has_selection()) {
                    delete_selection();
                } else if (m_caret < m_text.size()) {
                    push_undo_snapshot();
                    size_t n = SingleLineTextEditCore::next_utf8(m_text, m_caret);
                    m_text.erase(m_caret, n - m_caret);
                    notify_edited();
                }
                e.stop_propagation();
                return;
            case 37: // Left
                if (e.modifiers.shift) {
                    m_caret = SingleLineTextEditCore::prev_utf8(m_text, m_caret);
                } else if (has_selection()) {
                    m_caret = sel_start();
                    m_sel_anchor = m_caret;
                } else {
                    m_caret = SingleLineTextEditCore::prev_utf8(m_text, m_caret);
                    m_sel_anchor = m_caret;
                }
                ensure_caret_visible();
                reset_blink();
                mark_needs_paint();
                e.stop_propagation();
                return;
            case 39: // Right
                if (e.modifiers.shift) {
                    m_caret = SingleLineTextEditCore::next_utf8(m_text, m_caret);
                } else if (has_selection()) {
                    m_caret = sel_end();
                    m_sel_anchor = m_caret;
                } else {
                    m_caret = SingleLineTextEditCore::next_utf8(m_text, m_caret);
                    m_sel_anchor = m_caret;
                }
                ensure_caret_visible();
                reset_blink();
                mark_needs_paint();
                e.stop_propagation();
                return;
            case 36: { // Home -> 行首
                size_t li = caret_line_index();
                if (li < m_lines.size()) m_caret = std::min(m_lines[li].start, m_text.size());
                if (!e.modifiers.shift) m_sel_anchor = m_caret;
                reset_blink();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            case 35: { // End -> 行尾
                size_t li = caret_line_index();
                if (li < m_lines.size()) m_caret = std::min(m_lines[li].end, m_text.size());
                if (!e.modifiers.shift) m_sel_anchor = m_caret;
                reset_blink();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            default:
                if (!e.text.empty()) {
                    insert_text(e.text);
                    e.stop_propagation();
                }
                break;
        }
    };
}

} // namespace ktui
