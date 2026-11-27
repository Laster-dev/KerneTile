#pragma once

namespace ktui {

inline RenderNavigationView::ChromeButton RenderNavigationView::hit_test_chrome(PointF p, NavigationPaneDisplayMode eff) const noexcept {
    if (eff == NavigationPaneDisplayMode::Top) {
        if (m_data.is_back_button_visible && Rect::from_xywh(6.0f, 4.0f, 38.0f, 36.0f).contains(p)) {
            return ChromeButton::Back;
        }
        return ChromeButton::None;
    }
    if (eff == NavigationPaneDisplayMode::LeftMinimal && m_pane_open_anim <= 0.02f) {
        float bx = 4.0f;
        if (m_data.is_back_button_visible) {
            if (Rect::from_xywh(bx, 4.0f, 40.0f, 36.0f).contains(p)) return ChromeButton::Back;
            bx += 42.0f;
        }
        if (m_data.is_pane_toggle_button_visible && Rect::from_xywh(bx, 4.0f, 40.0f, 36.0f).contains(p)) {
            return ChromeButton::Toggle;
        }
        return ChromeButton::None;
    }
    if (m_data.is_back_button_visible && m_data.is_pane_toggle_button_visible) {
        if (m_pane_open_anim > 0.35f && Rect::from_xywh(4.0f, 4.0f, 36.0f, 36.0f).contains(p)) {
            return ChromeButton::Back;
        }
        float tx = 4.0f + 38.0f * m_pane_open_anim;
        float tw = 40.0f - 4.0f * m_pane_open_anim;
        if (Rect::from_xywh(tx, 4.0f, tw, 36.0f).contains(p)) {
            return ChromeButton::Toggle;
        }
        return ChromeButton::None;
    }
    if (m_data.is_back_button_visible && Rect::from_xywh(4.0f, 4.0f, 40.0f, 36.0f).contains(p)) {
        return ChromeButton::Back;
    }
    if (m_data.is_pane_toggle_button_visible && Rect::from_xywh(4.0f, 4.0f, 40.0f, 36.0f).contains(p)) {
        return ChromeButton::Toggle;
    }
    return ChromeButton::None;
}

inline void RenderNavigationView::activate_item(const NavigationViewItem& item, NavigationPaneDisplayMode eff,
                       bool is_settings, PointF flyout_anchor, bool flyout_below) {
    if (item.disabled || item.kind != NavigationItemKind::Item) return;

    if (is_settings) {
        // WinUI 3 AnimatedSettingsVisualSource: 每次点击顺滑旋转 90° (π/2 rad)
        m_settings_spin_base += 1.5707963f;
        m_last_anim_time = std::chrono::steady_clock::now();
    }

    // 含子项时:
    // - Compact / Top 模式下直接弹出 WinUI 3 浮层子菜单，绝不强行展开侧边栏！
    // - Expanded 模式下在侧边栏内就地展开/折叠子项
    if (!item.children.empty()) {
        if (eff == NavigationPaneDisplayMode::Top || m_pane_open_anim < 0.5f) {
            auto now = std::chrono::steady_clock::now();
            if (m_last_submenu_dismiss.time_since_epoch().count() != 0 &&
                std::chrono::duration<float>(now - m_last_submenu_dismiss).count() < 0.06f) {
                return;
            }
            open_submenu_flyout(item, flyout_anchor, flyout_below);
            return;
        }
        for (auto& mi : m_data.menu_items) {
            if (mi.effective_id() == item.effective_id()) {
                mi.expanded = !mi.expanded;
                sync_expand_anims();
                m_last_anim_time = std::chrono::steady_clock::now();
                mark_needs_paint();
                return;
            }
        }
    }

    close_submenu_flyout();
    set_selected_id(item.effective_id(), true, true);
    if (is_overlay_pane(eff) && m_data.is_pane_open) {
        set_pane_open(false, true);
    }
}

inline void RenderNavigationView::setup_behavior() {
    // 隧道预览：识别 Android / Windows / Linux 触控边缘右滑展开、左滑收起与侧边栏列表触控滚动
    on_pointer_preview = [this](PointerEvent& e) {
        if (e.device_kind != PointerDeviceKind::Touch) return;
        const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);
        if (eff == NavigationPaneDisplayMode::Top) return;

        if (e.type == PlatformEventType::PointerDown) {
            m_touch_tracking = true;
            m_touch_mode = TouchGestureMode::None;
            m_touch_start_local = e.local_position;
            m_touch_last_local = e.local_position;
            if (m_menu_scroll.max_scroll() > 0.0f) {
                m_menu_scroll.begin_touch_drag();
            }
        } else if (e.type == PlatformEventType::PointerMove && m_touch_tracking && m_touch_mode == TouchGestureMode::None) {
            float dx = e.local_position.x - m_touch_start_local.x;
            float dy = e.local_position.y - m_touch_start_local.y;
            const float pane_w = current_pane_width(eff);

            // 1. 左边缘右滑直接展开侧边栏 (任意模式关闭态从左侧 <= 36px 右滑 > 20px)
            if (!m_data.is_pane_open && m_touch_start_local.x <= 36.0f &&
                dx > 20.0f && std::abs(dx) > std::abs(dy) * 1.2f) {
                m_touch_mode = TouchGestureMode::EdgeSwipe;
                set_pane_open(true, true);
                e.gesture_capture_target = this;
                e.stop_propagation();
                return;
            }
            // 2. 侧边栏展开态向左滑直接收起 (dx < -28px)
            if (m_data.is_pane_open && m_touch_start_local.x <= pane_w + 24.0f &&
                dx < -28.0f && std::abs(dx) > std::abs(dy) * 1.2f) {
                m_touch_mode = TouchGestureMode::EdgeSwipe;
                set_pane_open(false, true);
                e.gesture_capture_target = this;
                e.stop_propagation();
                return;
            }
            // 3. 侧边栏菜单列表垂直触控拖拽滚动 (避开滚动条区域)
            Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
            auto sb_m = m_menu_scrollbar.compute_metrics(menu_vp, compute_left_menu_total_height(), m_menu_scroll.offset());
            bool on_sb = (m_menu_scrollbar.hit_test(m_touch_start_local, sb_m) != WinUIScrollBar::HitPart::None);
            if (!on_sb && !m_menu_scrollbar.is_dragging() &&
                menu_vp.contains(m_touch_start_local) && m_menu_scroll.max_scroll() > 0.0f &&
                std::abs(dy) > 8.0f && std::abs(dy) > std::abs(dx) * 1.15f) {
                m_touch_mode = TouchGestureMode::MenuScroll;
                m_menu_scroll.apply_touch_drag(e.local_position.y - m_touch_last_local.y);
                m_touch_last_local = e.local_position;
                m_menu_scrollbar.notify_activity();
                mark_needs_paint();
                e.gesture_capture_target = this;
                e.stop_propagation();
                return;
            }
        } else if (e.type == PlatformEventType::PointerUp) {
            if (m_touch_mode == TouchGestureMode::None && m_touch_tracking) {
                m_touch_tracking = false;
                if (std::abs(m_menu_scroll.overscroll_amount()) > 0.05f) {
                    m_menu_scroll.end_touch_drag();
                    mark_needs_paint();
                }
            }
        }
    };

    on_pointer_move = [this](PointerEvent& e) {
        if (e.device_kind == PointerDeviceKind::Touch) {
            if (m_has_reveal_cursor) {
                m_has_reveal_cursor = false;
                mark_needs_paint();
            }
        }
        if (e.device_kind == PointerDeviceKind::Touch && m_touch_mode != TouchGestureMode::None) {
            if (m_touch_mode == TouchGestureMode::MenuScroll) {
                float dy = e.local_position.y - m_touch_last_local.y;
                m_touch_last_local = e.local_position;
                m_menu_scroll.apply_touch_drag(dy);
                m_menu_scrollbar.notify_activity();
                mark_needs_paint();
            }
            e.stop_propagation();
            return;
        }
        const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

        if (e.type == PlatformEventType::PointerScroll && eff != NavigationPaneDisplayMode::Top) {
            Rect menu_vp = compute_left_menu_viewport(size().y, current_pane_width(eff));
            m_menu_scroll.set_max_scroll(std::max(0.0f, compute_left_menu_total_height() - menu_vp.height()));
            if (menu_vp.contains(e.local_position) && m_menu_scroll.max_scroll() > 0.0f) {
                m_menu_scroll.add_wheel_delta(e.delta.y);
                m_menu_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
            }
            return;
        }

        if (eff == NavigationPaneDisplayMode::Top) {
            ChromeButton cb = hit_test_chrome(e.local_position, eff);
            int top_idx = -1;
            bool hov_settings = false;
            if (cb == ChromeButton::None) {
                auto layouts = compute_top_layouts();
                for (int i = 0; i < static_cast<int>(layouts.size()); ++i) {
                    if (layouts[static_cast<size_t>(i)].rect.contains(e.local_position)) {
                        top_idx = i;
                        hov_settings = layouts[static_cast<size_t>(i)].is_settings;
                        break;
                    }
                }
            }
            bool need_paint = (cb != m_hovered_chrome || top_idx != m_hovered_top || hov_settings != m_settings_hovered);
            if (need_paint) {
                m_hovered_chrome = cb;
                m_hovered_top = top_idx;
                m_settings_hovered = hov_settings;
                m_last_anim_time = std::chrono::steady_clock::now();
            }
            if (ThemeContext::current().resolve_pointer_reveal(m_data.enable_pointer_reveal) && e.device_kind != PointerDeviceKind::Touch &&
                e.type == PlatformEventType::PointerMove) {
                const bool over_nav = (e.local_position.y <= 44.0f || cb != ChromeButton::None || top_idx >= 0);
                if (over_nav) {
                    if (!m_has_reveal_cursor ||
                        std::abs(m_reveal_cursor.x - e.local_position.x) > 0.01f ||
                        std::abs(m_reveal_cursor.y - e.local_position.y) > 0.01f) {
                        m_has_reveal_cursor = true;
                        m_reveal_cursor = e.local_position;
                        need_paint = true;
                    }
                } else if (m_has_reveal_cursor) {
                    m_has_reveal_cursor = false;
                    need_paint = true;
                }
            }
            if (need_paint) {
                mark_needs_paint();
            }
            return;
        }

        const float pane_w = current_pane_width(eff);
        Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
        bool sb_consumed = false;
        if (m_menu_scrollbar.handle_pointer_move(e.local_position, menu_vp, compute_left_menu_total_height(),
                                                 m_menu_scroll, sb_consumed)) {
            mark_needs_paint();
        }
        if (sb_consumed) {
            if (m_has_reveal_cursor) {
                m_has_reveal_cursor = false;
                mark_needs_paint();
            }
            e.stop_propagation();
            return;
        }

        ChromeButton cb = hit_test_chrome(e.local_position, eff);
        int r_idx = -1;
        bool r_footer = false;
        if (cb == ChromeButton::None) {
            hit_test_left_row(e.local_position, eff, r_idx, r_footer);
        }
        bool hov_settings = (r_footer && r_idx >= 0 && r_idx < static_cast<int>(m_footer_rows.size()) &&
                             m_footer_rows[static_cast<size_t>(r_idx)].is_settings);
        bool need_paint = (cb != m_hovered_chrome || r_idx != m_hovered_row ||
                           r_footer != m_hovered_in_footer || hov_settings != m_settings_hovered);
        if (need_paint) {
            m_hovered_chrome = cb;
            m_hovered_row = r_idx;
            m_hovered_in_footer = r_footer;
            m_settings_hovered = hov_settings;
            m_last_anim_time = std::chrono::steady_clock::now();
        }
        if (ThemeContext::current().resolve_pointer_reveal(m_data.enable_pointer_reveal) && e.device_kind != PointerDeviceKind::Touch &&
            e.type == PlatformEventType::PointerMove) {
            const bool over_nav = (e.local_position.x <= pane_w || cb != ChromeButton::None || r_idx >= 0);
            if (over_nav) {
                if (!m_has_reveal_cursor ||
                    std::abs(m_reveal_cursor.x - e.local_position.x) > 0.01f ||
                    std::abs(m_reveal_cursor.y - e.local_position.y) > 0.01f) {
                    m_has_reveal_cursor = true;
                    m_reveal_cursor = e.local_position;
                    need_paint = true;
                }
            } else if (m_has_reveal_cursor) {
                m_has_reveal_cursor = false;
                need_paint = true;
            }
        }
        if (need_paint) {
            mark_needs_paint();
        }
    };

    on_pointer_exit = [this](PointerEvent&) {
        m_menu_scrollbar.handle_pointer_exit();
        const bool was_reveal = m_has_reveal_cursor;
        m_has_reveal_cursor = false;
        const bool was_hover = (m_hovered_chrome != ChromeButton::None || m_hovered_row != -1 || m_hovered_top != -1 || m_settings_hovered);
        if (was_hover) {
            m_hovered_chrome = ChromeButton::None;
            m_hovered_row = -1;
            m_hovered_top = -1;
            m_settings_hovered = false;
            m_last_anim_time = std::chrono::steady_clock::now();
        }
        if (was_hover || was_reveal) {
            mark_needs_paint();
        }
    };

    on_pointer_down = [this](PointerEvent& e) {
        const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

        if (eff == NavigationPaneDisplayMode::Top) {
            ChromeButton cb = hit_test_chrome(e.local_position, eff);
            if (cb != ChromeButton::None) {
                m_pressed_chrome = cb;
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            auto layouts = compute_top_layouts();
            for (int i = 0; i < static_cast<int>(layouts.size()); ++i) {
                if (layouts[static_cast<size_t>(i)].rect.contains(e.local_position)) {
                    m_pressed_top = i;
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
            }
            return;
        }

        const float pane_w = current_pane_width(eff);
        if (is_overlay_pane(eff) && m_data.is_pane_open && e.local_position.x > pane_w) {
            set_pane_open(false, true);
            e.stop_propagation();
            return;
        }

        Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
        if (m_menu_scrollbar.handle_pointer_down(e.local_position, menu_vp, compute_left_menu_total_height(),
                                                 m_menu_scroll, 36.0f)) {
            mark_needs_paint();
            e.stop_propagation();
            return;
        }

        ChromeButton cb = hit_test_chrome(e.local_position, eff);
        if (cb != ChromeButton::None) {
            m_pressed_chrome = cb;
            mark_needs_paint();
            e.stop_propagation();
            return;
        }

        int r_idx = -1;
        bool r_footer = false;
        hit_test_left_row(e.local_position, eff, r_idx, r_footer);
        if (r_idx >= 0) {
            m_pressed_row = r_idx;
            m_pressed_in_footer = r_footer;
            mark_needs_paint();
            e.stop_propagation();
        }
    };

    on_pointer_up = [this](PointerEvent& e) {
        bool sb_released = m_menu_scrollbar.handle_pointer_up();
        if (m_touch_mode != TouchGestureMode::None) {
            if (m_touch_mode == TouchGestureMode::MenuScroll) {
                m_menu_scroll.end_touch_drag();
                m_menu_scrollbar.notify_activity();
                mark_needs_paint();
            }
            m_touch_mode = TouchGestureMode::None;
            m_touch_tracking = false;
            e.stop_propagation();
            return;
        }
        m_touch_tracking = false;

        if (sb_released) {
            mark_needs_paint();
            e.stop_propagation();
            return;
        }

        const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

        if (m_pressed_chrome != ChromeButton::None) {
            ChromeButton pressed = m_pressed_chrome;
            m_pressed_chrome = ChromeButton::None;
            if (hit_test_chrome(e.local_position, eff) == pressed) {
                if (pressed == ChromeButton::Back && can_go_back()) {
                    go_back();
                } else if (pressed == ChromeButton::Toggle) {
                    toggle_pane();
                }
            }
            mark_needs_paint();
            e.stop_propagation();
            return;
        }

        if (eff == NavigationPaneDisplayMode::Top && m_pressed_top >= 0) {
            int pressed = m_pressed_top;
            m_pressed_top = -1;
            auto layouts = compute_top_layouts();
            if (pressed < static_cast<int>(layouts.size()) &&
                layouts[static_cast<size_t>(pressed)].rect.contains(e.local_position)) {
                const auto& tl = layouts[static_cast<size_t>(pressed)];
                activate_item(*tl.item, eff, tl.is_settings,
                              PointF{tl.rect.min_x, tl.rect.max_y + 4.0f}, true);
            }
            mark_needs_paint();
            e.stop_propagation();
            return;
        }

        if (m_pressed_row >= 0) {
            int pr = m_pressed_row;
            bool pf = m_pressed_in_footer;
            m_pressed_row = -1;
            int hr = -1;
            bool hf = false;
            float row_y = 0.0f;
            hit_test_left_row(e.local_position, eff, hr, hf, &row_y);
            if (hr == pr && hf == pf) {
                const auto& list = pf ? m_footer_rows : m_main_rows;
                if (pr >= 0 && pr < static_cast<int>(list.size()) && list[static_cast<size_t>(pr)].item) {
                    const auto& row = list[static_cast<size_t>(pr)];
                    activate_item(*row.item, eff, row.is_settings,
                                  PointF{current_pane_width(eff) + 4.0f, row_y}, false);
                }
            }
            mark_needs_paint();
            e.stop_propagation();
        }
    };

    on_key = [this](KeyEvent& e) {
        if (e.type != PlatformEventType::KeyDown) return;
        const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);
        if (e.key_code == 27) { // Esc 收起浮层面板
            if (is_overlay_pane(eff) && m_data.is_pane_open) {
                set_pane_open(false, true);
                e.stop_propagation();
            }
        }
    };
}

inline void RenderNavigationView::update_animations() {
    auto now = std::chrono::steady_clock::now();
    if (m_last_anim_time.time_since_epoch().count() == 0) {
        m_last_anim_time = now;
    }
    float dt = std::clamp(std::chrono::duration<float>(now - m_last_anim_time).count(), 0.0f, 0.05f);
    m_last_anim_time = now;

    bool animating = false;

    // 1. 面板展开/收起动画 (200ms)
    float target_open = m_data.is_pane_open ? 1.0f : 0.0f;
    if (std::abs(m_pane_open_anim - target_open) > 0.004f) {
        m_pane_open_anim += (target_open - m_pane_open_anim) * std::min(1.0f, dt * 18.0f);
        if (std::abs(m_pane_open_anim - target_open) <= 0.004f) {
            m_pane_open_anim = target_open;
        }
        NavigationPaneDisplayMode eff = compute_effective_mode(size().x);
        if (eff == NavigationPaneDisplayMode::Left && m_content_box) {
            Rect body = compute_content_body_rect(size().x, size().y, eff);
            m_content_box->layout(BoxConstraints::tight(Vec2{std::max(0.0f, body.width()), std::max(0.0f, body.height())}));
            m_content_box->set_offset(Vec2{body.min_x, body.min_y});
        }
        animating = true;
    } else {
        m_pane_open_anim = target_open;
    }

    // 1.5 先推进菜单列表滚动并将滚动位移直接同步至小蓝条，确保滚动时零拖尾、零延迟
    if (m_menu_scroll.update()) animating = true;
    if (m_menu_scrollbar.update_animations(m_hovered_row >= 0)) animating = true;
    {
        float cur_scroll = m_menu_scroll.offset();
        float dscroll = cur_scroll - m_pill_last_scroll_y;
        if (m_pill_initialized && m_pill_in_menu && std::abs(dscroll) > 0.0001f) {
            m_pill_head -= dscroll;
            m_pill_tail -= dscroll;
            m_pill_target_pos -= dscroll;
        }
        m_pill_last_scroll_y = cur_scroll;
    }

    // 2. WinUI 3 弹性拉伸滑动强调条 (仅在切换选中项时生效: head 30x 先行、tail 15x 追赶)
    if (m_pill_initialized) {
        if (std::abs(m_pill_head - m_pill_target_pos) > 0.25f) {
            m_pill_head += (m_pill_target_pos - m_pill_head) * std::min(1.0f, dt * 30.0f);
            animating = true;
        } else {
            m_pill_head = m_pill_target_pos;
        }
        if (std::abs(m_pill_tail - m_pill_target_pos) > 0.25f) {
            m_pill_tail += (m_pill_target_pos - m_pill_tail) * std::min(1.0f, dt * 15.0f);
            animating = true;
        } else {
            m_pill_tail = m_pill_target_pos;
        }
        if (std::abs(m_pill_size - m_pill_target_size) > 0.25f) {
            m_pill_size += (m_pill_target_size - m_pill_size) * std::min(1.0f, dt * 22.0f);
            animating = true;
        } else {
            m_pill_size = m_pill_target_size;
        }
    }

    // 3. WinUI 3 设置齿轮旋转动效 (悬停微转 +18°, 点击顺滑旋转 +90°)
    float gear_target = m_settings_spin_base + (m_settings_hovered ? 0.314f : 0.0f);
    if (std::abs(m_settings_gear_angle - gear_target) > 0.005f) {
        m_settings_gear_angle += (gear_target - m_settings_gear_angle) * std::min(1.0f, dt * 12.0f);
        animating = true;
    } else {
        m_settings_gear_angle = gear_target;
    }

    // 4. 页面切换进场动画 (140ms)
    if (m_page_enter_progress < 1.0f) {
        m_page_enter_progress = std::min(1.0f, m_page_enter_progress + dt / 0.14f);
        animating = true;
    }

    // 5. 左侧可折叠父级菜单手风琴展开/收起平滑动画 + 箭头 180° 旋转
    for (const auto& mi : m_data.menu_items) {
        if (mi.kind != NavigationItemKind::Item || mi.children.empty()) continue;
        const std::string& id = mi.effective_id();
        float target = mi.expanded ? 1.0f : 0.0f;
        bool found = false;
        for (auto& kv : m_expand_anims) {
            if (kv.first == id) {
                found = true;
                if (std::abs(kv.second - target) > 0.004f) {
                    kv.second += (target - kv.second) * (1.0f - std::exp(-dt * 18.0f));
                    if (std::abs(kv.second - target) <= 0.004f) kv.second = target;
                    animating = true;
                } else {
                    kv.second = target;
                }
                break;
            }
        }
        if (!found) {
            m_expand_anims.emplace_back(id, target);
        }
    }

    if (animating) {
        mark_needs_paint();
    }
}

} // namespace ktui
