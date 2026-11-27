#pragma once

namespace ktui {

inline void RenderTerminal::ensure_find_bar() {
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
                    .placeholder = FrameworkStrings::current().find_placeholder,
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

inline void RenderTerminal::show_context_menu(PointF global_pos) {
    Window* win = Window::active_window();
    if (!win) return;
    Vec2 wp{global_pos.x, global_pos.y + win->titlebar_height()};

    std::vector<MenuFlyoutItem> items = {
        MenuFlyoutItem{
            .label = FrameworkStrings::current().copy,
            .shortcut = "Ctrl+Shift+C",
            .disabled = !m_has_selection,
            .on_click = [this]() { copy_selection(); }
        },
        MenuFlyoutItem{
            .label = FrameworkStrings::current().paste,
            .shortcut = "Ctrl+Shift+V",
            .on_click = [this]() { paste_from_clipboard(); }
        },
        MenuFlyoutItem{
            .label = FrameworkStrings::current().term_select_all_buffer,
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
            .label = FrameworkStrings::current().find_placeholder,
            .shortcut = "Ctrl+Shift+F",
            .on_click = [this]() { toggle_find(); }
        },
        MenuFlyoutItem{
            .label = FrameworkStrings::current().term_clear_buffer,
            .on_click = [this]() { clear(); }
        },
        MenuFlyoutItem::separator(),
        MenuFlyoutItem{
            .label = FrameworkStrings::current().term_cursor_style,
            .sub_items = {
                MenuFlyoutItem{ .label = FrameworkStrings::current().term_cursor_bar, .checked = (m_cursor_style == TerminalCursorStyle::Bar), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::Bar); }, .is_radio = true },
                MenuFlyoutItem{ .label = FrameworkStrings::current().term_cursor_filled_box, .checked = (m_cursor_style == TerminalCursorStyle::FilledBox), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::FilledBox); }, .is_radio = true },
                MenuFlyoutItem{ .label = FrameworkStrings::current().term_cursor_vintage, .checked = (m_cursor_style == TerminalCursorStyle::Vintage), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::Vintage); }, .is_radio = true },
                MenuFlyoutItem{ .label = FrameworkStrings::current().term_cursor_empty_box, .checked = (m_cursor_style == TerminalCursorStyle::EmptyBox), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::EmptyBox); }, .is_radio = true },
                MenuFlyoutItem{ .label = FrameworkStrings::current().term_cursor_double_underscore, .checked = (m_cursor_style == TerminalCursorStyle::DoubleUnderscore), .on_click = [this]() { set_cursor_style(TerminalCursorStyle::DoubleUnderscore); }, .is_radio = true }
            }
        },
        MenuFlyoutItem{
            .label = FrameworkStrings::current().term_color_scheme,
            .sub_items = {
                MenuFlyoutItem{ .label = FrameworkStrings::current().term_scheme_campbell, .checked = (m_scheme.name == "Campbell"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::campbell()); }, .is_radio = true },
                MenuFlyoutItem{ .label = "One Half Dark", .checked = (m_scheme.name == "One Half Dark"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::one_half_dark()); }, .is_radio = true },
                MenuFlyoutItem{ .label = "One Half Light", .checked = (m_scheme.name == "One Half Light"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::one_half_light()); }, .is_radio = true },
                MenuFlyoutItem{ .label = "Solarized Dark", .checked = (m_scheme.name == "Solarized Dark"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::solarized_dark()); }, .is_radio = true },
                MenuFlyoutItem{ .label = "Tango Dark", .checked = (m_scheme.name == "Tango Dark"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::tango_dark()); }, .is_radio = true },
                MenuFlyoutItem{ .label = "Vintage", .checked = (m_scheme.name == "Vintage"), .on_click = [this]() { set_color_scheme(TerminalColorScheme::vintage()); }, .is_radio = true }
            }
        },
        MenuFlyoutItem::separator(),
        MenuFlyoutItem{ .label = FrameworkStrings::current().term_zoom_in, .shortcut = "Ctrl++", .on_click = [this]() { set_font_size(m_font_size + 1.5f); } },
        MenuFlyoutItem{ .label = FrameworkStrings::current().term_zoom_out, .shortcut = "Ctrl+-", .on_click = [this]() { set_font_size(m_font_size - 1.5f); } },
        MenuFlyoutItem{ .label = FrameworkStrings::current().term_zoom_reset, .shortcut = "Ctrl+0", .on_click = [this]() { set_font_size(m_default_font_size); } }
    };
    MenuFlyout::show_at(std::move(items), wp);
}

inline void RenderTerminal::setup_behavior() {
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
            if (e.is_touch()) {
                m_touch_scrolling = true;
                m_touch_last_y = e.local_position.y;
                e.stop_propagation();
                return;
            }
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

        if (m_touch_scrolling && e.is_touch()) {
            float dy = e.local_position.y - m_touch_last_y;
            m_touch_last_y = e.local_position.y;
            m_scroll_y = std::clamp(m_scroll_y - dy, 0.0f, max_scroll_y());
            clamp_scroll();
            m_scrollbar.notify_activity();
            mark_needs_paint();
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
        if (m_touch_scrolling) {
            m_touch_scrolling = false;
            e.stop_propagation();
            return;
        }
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

    on_pointer_cancel = [this](PointerEvent&) {
        m_touch_scrolling = false;
        m_dragging_sel = false;
        m_vt_mouse_button = -1;
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

inline std::string RenderTerminal::encode_vt_key(uint32_t vk, bool ctrl, bool shift, bool alt, bool app_cursor) {
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

} // namespace ktui
