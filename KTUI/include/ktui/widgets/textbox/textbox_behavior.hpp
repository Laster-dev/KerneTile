#pragma once

namespace ktui {

inline void RenderTextBox::handle_text_key(const KeyEvent& ev) {
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

inline void RenderTextBox::setup_behavior() {
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
            m_pending_touch_collapse = false;
            m_state.is_dragging_selection = false;
            mark_needs_paint();
        }
    };

    on_pointer_down = [this](PointerEvent& ev) {
        if (m_state.is_disabled) return;
        if (ev.button == PointerButton::Secondary) {
            m_pending_touch_collapse = false;
            const bool is_touch = (ev.device_kind == PointerDeviceKind::Touch);
            if (is_touch && !has_selection() && !m_data.text.empty()) {
                auto style = resolve_current_style();
                float start_x = style.padding.left - m_scroll_offset_x;
                float click_x = ev.local_position.x - start_x;
                select_word_at(click_x, get_display_text(), style.font_size);
            }
            open_context_menu(ev.position, is_touch);
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
            float eye_x = bounds().max_x - (ev.is_touch() ? 40.0f : 24.0f);
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
            m_pending_touch_collapse = false;
            select_word_at(click_x, display_text, style.font_size);
            if (ev.device_kind == PointerDeviceKind::Touch && has_selection()) {
                open_context_menu(ev.position, true);
            }
        } else if (m_click_count >= 3) {
            m_pending_touch_collapse = false;
            select_all();
            if (ev.device_kind == PointerDeviceKind::Touch && has_selection()) {
                open_context_menu(ev.position, true);
            }
        } else {
            size_t new_index = get_char_index_at(click_x, display_text, style.font_size);
            if (ev.device_kind == PointerDeviceKind::Touch) {
                if (has_selection()) {
                    m_pending_touch_collapse = true;
                    m_pending_touch_collapse_index = new_index;
                } else {
                    m_pending_touch_collapse = false;
                    m_cursor_index = new_index;
                    m_selection_anchor = m_cursor_index;
                }
                m_state.is_dragging_selection = false;
            } else {
                m_pending_touch_collapse = false;
                m_cursor_index = new_index;
                if (!ev.modifiers.shift) {
                    m_selection_anchor = m_cursor_index;
                }
                m_state.is_dragging_selection = true;
            }
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
        if (m_pending_touch_collapse) {
            m_pending_touch_collapse = false;
            m_cursor_index = std::min(m_pending_touch_collapse_index, m_data.text.size());
            m_selection_anchor = m_cursor_index;
            ensure_cursor_visible();
            mark_needs_paint();
        }
    };

    on_pointer_cancel = [this](PointerEvent&) {
        m_state.is_hovered = false;
        is_hovered = false;
        m_pending_touch_collapse = false;
        m_state.is_dragging_selection = false;
        mark_needs_paint();
    };

    on_key = [this](KeyEvent& ev) {
        if (m_key_intercept && m_key_intercept(ev)) return;
        handle_text_key(ev);
    };
}

} // namespace ktui
