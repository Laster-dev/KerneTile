#pragma once

namespace ktui {

inline bool RenderDockPanel::detach_pane(const std::string& id) {
    if (!can_detach()) return false;
    for (size_t i = 0; i < m_panes.size(); ++i) {
        auto& p = m_panes[i];
        if (p.id != id || p.closed || p.detached) continue;

        std::string pid = p.id;
        std::string ptitle = p.title;
        DockZone prev_zone = p.zone;
        std::weak_ptr<bool> alive = m_alive;

        // 先安全清理悬停/焦点引用并从当前工作区摘除该面板的 RenderBox
        if (p.content_box) {
            detail::forget_render_subtree(p.content_box.get());
            if (p.content_box->parent() == this) {
                remove_child(p.content_box.get());
            }
        }
        p.detached = true;
        mark_needs_layout();
        mark_needs_paint();

        auto on_redock = [this, alive, pid, prev_zone]() {
            if (auto g = alive.lock(); g && *g) {
                dock_pane(pid, prev_zone);
            }
        };

        if (m_on_detach_pane) {
            bool ok = m_on_detach_pane(pid, ptitle, p.content_box, std::move(on_redock));
            if (!ok) {
                dock_pane(pid, prev_zone);
                return false;
            }
            if (m_on_detach_changed) m_on_detach_changed(pid, true);
            return true;
        }

        // 内置开箱即用的跨平台独立窗口撕出 (严格通过 post_task 异步创建，杜绝 WndProc 重入崩溃)
        auto* app = Application::current();
        if (!app) {
            dock_pane(pid, prev_zone);
            return false;
        }

        // 捕获当前所属父窗口，使浮动工具窗始终位于父窗口之上并支持按住标题栏拖回吸附
        std::weak_ptr<Window> parent_win_weak;
        Window* active_win = Window::active_window();
        for (const auto& w : app->windows()) {
            if (w && w.get() == active_win) {
                parent_win_weak = w;
                break;
            }
        }
        if (parent_win_weak.expired()) {
            parent_win_weak = app->main_window();
        }

        auto content_ref = p.content_box;
        PointF launch_hint = m_drag_pos;
        app->dispatcher().post_task([this, alive, pid, ptitle, prev_zone, content_ref, parent_win_weak, launch_hint, on_redock = std::move(on_redock)]() mutable {
            auto guard = alive.lock();
            if (!guard || !*guard) return;
            auto* cur_app = Application::current();
            if (!cur_app) return;

            // 防止在 post_task 排队期间面板已被重置或重新停靠，避免弹出幽灵窗口
            auto it = std::find_if(m_panes.begin(), m_panes.end(), [&](const LivePane& lp) { return lp.id == pid; });
            if (it == m_panes.end() || !it->detached || it->closed) return;

            WindowDesc wd;
            wd.title = ptitle;
            wd.size = Vec2{420.0f, 300.0f};
            wd.min_size = Vec2{260.0f, 160.0f};
            wd.resizable = true;
            wd.decorated = false;
            wd.custom_titlebar = false; // 关闭系统与默认大标题栏，由 26px Visual Studio 风格极简标题栏接管
            // 使用 Normal + parent_handle 归属窗口（而非 WS_EX_TOOLWINDOW）：既不占任务栏且保持在父窗口之上，
            // 又完整支持系统原生最大化/还原动画与 Windows 11 悬停最大化按钮的分屏布局 (Snap Layouts)
            wd.type = WindowType::Normal;

            if (auto pw = parent_win_weak.lock(); pw && !pw->is_closed()) {
                if (pw->platform_window()) {
                    wd.parent_handle = pw->platform_window()->get_native_handle();
                }
                const Vec2 p_pos = pw->position();
                const float p_dpi = std::max(0.1f, pw->dpi_scale());
                const Vec2 dock_org = global_offset();
                float lx = (launch_hint.x != 0.0f || launch_hint.y != 0.0f) ? launch_hint.x : m_size.x * 0.5f;
                float ly = (launch_hint.x != 0.0f || launch_hint.y != 0.0f) ? launch_hint.y : m_size.y * 0.5f;
                wd.position = Vec2{
                    std::max(20.0f, std::round(p_pos.x + (dock_org.x + lx - wd.size.x * 0.4f) * p_dpi)),
                    std::max(20.0f, std::round(p_pos.y + (pw->titlebar_height() + dock_org.y + ly - 14.0f) * p_dpi))
                };
            }

            auto detached_win = cur_app->create_window(wd);
            if (!detached_win) {
                dock_pane(pid, prev_zone);
                return;
            }

            auto redocked = std::make_shared<bool>(false);
            auto schedule_dock_to = [this, alive, redocked, pid](DockZone target) {
                if (*redocked) return;
                *redocked = true;
                if (auto* a = Application::current()) {
                    a->dispatcher().post_task([this, alive, pid, target]() {
                        if (auto g = alive.lock(); g && *g) {
                            m_external_drag_active = false;
                            m_drop_target.reset();
                            dock_pane(pid, target);
                        }
                    });
                }
            };
            auto schedule_redock = [schedule_dock_to, prev_zone]() {
                schedule_dock_to(prev_zone);
            };

            detail::RenderDetachedPaneShell::Callbacks cbs;
            cbs.on_redock = schedule_redock;
            cbs.on_dock_to = schedule_dock_to;
            cbs.on_drag_move = [this, alive, parent_win_weak](Vec2 screen_phys) {
                if (auto g = alive.lock(); !g || !*g) return;
                if (auto pw = parent_win_weak.lock(); pw && !pw->is_closed()) {
                    Vec2 p_pos = pw->position();
                    float p_dpi = std::max(0.1f, pw->dpi_scale());
                    Vec2 dock_org = global_offset();
                    PointF lp{
                        (screen_phys.x - p_pos.x) / p_dpi - dock_org.x,
                        (screen_phys.y - p_pos.y) / p_dpi - pw->titlebar_height() - dock_org.y
                    };
                    update_external_drag(lp, pw.get());
                }
            };
            cbs.on_drag_end = [this, alive, parent_win_weak, schedule_dock_to](Vec2 screen_phys) {
                if (auto g = alive.lock(); !g || !*g) return;
                std::optional<DockZone> target;
                if (auto pw = parent_win_weak.lock(); pw && !pw->is_closed()) {
                    Vec2 p_pos = pw->position();
                    float p_dpi = std::max(0.1f, pw->dpi_scale());
                    Vec2 dock_org = global_offset();
                    PointF lp{
                        (screen_phys.x - p_pos.x) / p_dpi - dock_org.x,
                        (screen_phys.y - p_pos.y) / p_dpi - pw->titlebar_height() - dock_org.y
                    };
                    target = finish_external_drag(lp, pw.get());
                } else {
                    m_external_drag_active = false;
                    m_drop_target.reset();
                }
                if (target.has_value()) {
                    schedule_dock_to(*target);
                }
            };

            auto shell = make_render_node<detail::RenderDetachedPaneShell>(
                ptitle, content_ref, std::weak_ptr<Window>(detached_win), std::move(cbs));
            for (auto& pane : m_panes) {
                if (pane.id == pid) {
                    pane.detached_window = detached_win;
                    pane.detached_shell = shell;
                    break;
                }
            }

            detached_win->set_custom_hit_test([sh_weak = std::weak_ptr<detail::RenderDetachedPaneShell>(shell)](PointF p) -> TitlebarHitTest {
                if (auto sh = sh_weak.lock()) return sh->titlebar_hit_test(p);
                return TitlebarHitTest::Client;
            });
            detached_win->on_close([schedule_redock]() -> bool {
                schedule_redock();
                return true;
            });
            detached_win->set_content(make_widget<detail::DetachedPaneHostWidget>(shell));
            detached_win->show();
            if (m_on_detach_changed) m_on_detach_changed(pid, true);
        });
        return true;
    }
    return false;
}

inline void RenderDockPanel::perform_layout() {
    BoxConstraints inner = m_constraints.deflate(m_padding);
    float w = inner.has_bounded_width() ? inner.max_width : std::max(inner.min_width, 560.0f);
    float h = inner.has_bounded_height() ? inner.max_height : std::max(inner.min_height, 360.0f);
    m_size = m_constraints.constrain(Vec2{w + m_padding.horizontal(), h + m_padding.vertical()});

    const float ox = m_padding.left + 1.0f;
    const float oy = m_padding.top + 1.0f;
    const float iw = std::max(0.0f, m_size.x - m_padding.horizontal() - 2.0f);
    const float ih = std::max(0.0f, m_size.y - m_padding.vertical() - 2.0f);
    constexpr float kHairline = 1.0f;

    m_zone_rects.fill(Rect{});
    m_splitters.clear();
    m_tab_hits.clear();

    const bool has_top    = !panes_in_zone(DockZone::Top).empty();
    const bool has_bottom = !panes_in_zone(DockZone::Bottom).empty();
    const bool has_left   = !panes_in_zone(DockZone::Left).empty();
    const bool has_right  = !panes_in_zone(DockZone::Right).empty();
    const bool has_center = !panes_in_zone(DockZone::Center).empty();
    const bool has_mid    = has_left || has_right || has_center;

    float top_h = 0.0f, bot_h = 0.0f;
    float v_gaps = 0.0f;
    if (has_top && (has_mid || has_bottom)) v_gaps += kHairline;
    if (has_bottom && has_mid) v_gaps += kHairline;
    float avail_h = std::max(0.0f, ih - v_gaps);

    if (has_top) {
        float max_t = (has_mid || has_bottom) ? std::max(kMinPaneSize, avail_h - kMinPaneSize) : avail_h;
        top_h = std::clamp(m_top_h, std::min(kMinPaneSize, max_t), max_t);
        if (!has_mid && !has_bottom) top_h = avail_h;
    }
    if (has_bottom) {
        float rem = std::max(0.0f, avail_h - top_h);
        float max_b = has_mid ? std::max(kMinPaneSize, rem - kMinPaneSize) : rem;
        bot_h = std::clamp(m_bottom_h, std::min(kMinPaneSize, max_b), max_b);
        if (!has_mid) bot_h = rem;
    }
    float mid_y = oy + top_h + (has_top && (has_mid || has_bottom) ? kHairline : 0.0f);
    float mid_h = std::max(0.0f, ih - top_h - bot_h - v_gaps);

    if (has_top) {
        m_zone_rects[static_cast<size_t>(DockZone::Top)] = Rect::from_xywh(ox, oy, iw, top_h);
        if (has_mid || has_bottom) {
            float sy = oy + top_h;
            m_splitters.push_back(Splitter{SplitterKind::Top,
                Rect::from_xywh(ox, sy - kSplitterHit * 0.5f, iw, kSplitterHit),
                Rect::from_xywh(ox, sy, iw, kHairline)});
        }
    }
    if (has_bottom) {
        float by = oy + ih - bot_h;
        m_zone_rects[static_cast<size_t>(DockZone::Bottom)] = Rect::from_xywh(ox, by, iw, bot_h);
        if (has_mid) {
            float sy = by - kHairline;
            m_splitters.push_back(Splitter{SplitterKind::Bottom,
                Rect::from_xywh(ox, sy - kSplitterHit * 0.5f, iw, kSplitterHit),
                Rect::from_xywh(ox, sy, iw, kHairline)});
        }
    }

    if (has_mid) {
        float h_gaps = 0.0f;
        if (has_left && (has_center || has_right)) h_gaps += kHairline;
        if (has_right && has_center) h_gaps += kHairline;
        float avail_w = std::max(0.0f, iw - h_gaps);

        float left_w = 0.0f, right_w = 0.0f;
        if (has_left) {
            float max_l = (has_center || has_right) ? std::max(kMinPaneSize, avail_w - kMinPaneSize) : avail_w;
            left_w = std::clamp(m_left_w, std::min(kMinPaneSize, max_l), max_l);
            if (!has_center && !has_right) left_w = avail_w;
        }
        if (has_right) {
            float rem = std::max(0.0f, avail_w - left_w);
            float max_r = has_center ? std::max(kMinPaneSize, rem - kMinPaneSize) : rem;
            right_w = std::clamp(m_right_w, std::min(kMinPaneSize, max_r), max_r);
            if (!has_center) right_w = rem;
        }
        float center_x = ox + left_w + (has_left && (has_center || has_right) ? kHairline : 0.0f);
        float center_w = std::max(0.0f, iw - left_w - right_w - h_gaps);

        if (has_left) {
            m_zone_rects[static_cast<size_t>(DockZone::Left)] = Rect::from_xywh(ox, mid_y, left_w, mid_h);
            if (has_center || has_right) {
                float sx = ox + left_w;
                m_splitters.push_back(Splitter{SplitterKind::Left,
                    Rect::from_xywh(sx - kSplitterHit * 0.5f, mid_y, kSplitterHit, mid_h),
                    Rect::from_xywh(sx, mid_y, kHairline, mid_h)});
            }
        }
        if (has_center) {
            m_zone_rects[static_cast<size_t>(DockZone::Center)] = Rect::from_xywh(center_x, mid_y, center_w, mid_h);
        }
        if (has_right) {
            float rx = ox + iw - right_w;
            m_zone_rects[static_cast<size_t>(DockZone::Right)] = Rect::from_xywh(rx, mid_y, right_w, mid_h);
            if (has_center) {
                float sx = rx - kHairline;
                m_splitters.push_back(Splitter{SplitterKind::Right,
                    Rect::from_xywh(sx - kSplitterHit * 0.5f, mid_y, kSplitterHit, mid_h),
                    Rect::from_xywh(sx, mid_y, kHairline, mid_h)});
            }
        }
    }

    for (auto& pane : m_panes) {
        if (pane.closed && pane.content_box && pane.content_box->parent() == this) {
            pane.content_box->layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}), true);
            pane.content_box->set_offset(Vec2{-10000.0f, -10000.0f});
        }
    }

    for (size_t z = 0; z < 5; ++z) {
        DockZone dz = static_cast<DockZone>(z);
        auto list = panes_in_zone(dz);
        if (list.empty()) continue;
        if (m_active_tab[z] >= list.size()) m_active_tab[z] = 0;

        Rect zr = m_zone_rects[z];
        float content_h = std::max(0.0f, zr.height() - kHeaderHeight);
        float tab_x = zr.min_x + 4.0f;
        float right_reserve = can_detach() ? 28.0f : 4.0f;
        float max_bar_x = zr.max_x - right_reserve;

        for (size_t k = 0; k < list.size(); ++k) {
            auto& pane = m_panes[list[k]];
            float label_w = TextMeasure::measure(pane.title, 12.0f, FontWeight::SemiBold).x;
            float ideal_w = 12.0f + label_w + (pane.closable ? 24.0f : 12.0f);
            float tab_w = std::min(ideal_w, std::max(56.0f, max_bar_x - tab_x));
            if (tab_x + tab_w > max_bar_x) tab_w = std::max(40.0f, max_bar_x - tab_x);

            Rect tr = Rect::from_xywh(tab_x, zr.min_y, tab_w, kHeaderHeight);
            Rect close_r{};
            if (pane.closable && tab_w >= 52.0f) {
                close_r = Rect::from_xywh(tr.max_x - 20.0f, tr.min_y + (kHeaderHeight - 18.0f) * 0.5f, 18.0f, 18.0f);
            }
            m_tab_hits.push_back(TabHit{dz, k, list[k], tr, close_r});
            tab_x += tab_w + 2.0f;

            if (pane.content_box && pane.content_box->parent() == this) {
                if (k == m_active_tab[z]) {
                    pane.content_box->layout(BoxConstraints::tight(Vec2{zr.width(), content_h}), true);
                    pane.content_box->set_offset(Vec2{zr.min_x, zr.min_y + kHeaderHeight});
                } else {
                    pane.content_box->layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}), true);
                    pane.content_box->set_offset(Vec2{-10000.0f, -10000.0f});
                }
            }
        }
    }
}

inline void RenderDockPanel::open_pane_context_menu(size_t pane_index, PointF event_window_pos) {
    if (pane_index >= m_panes.size()) return;
    const auto& pane = m_panes[pane_index];
    std::string pid = pane.id;
    DockZone cur_zone = pane.zone;
    bool closable = pane.closable;
    std::weak_ptr<bool> alive = m_alive;

    std::vector<MenuFlyoutItem> items;
    if (can_detach()) {
        items.push_back(MenuFlyoutItem{
            .label = FrameworkStrings::current().dock_float_window,
            .on_click = [this, alive, pid]() {
                if (auto g = alive.lock(); g && *g) detach_pane(pid);
            }
        });
        items.push_back(MenuFlyoutItem::separator());
    }

    auto add_zone = [&](const char* label, DockZone z) {
        items.push_back(MenuFlyoutItem{
            .label = label,
            .is_checkable = true,
            .checked = (cur_zone == z),
            .on_click = [this, alive, pid, z]() {
                if (auto g = alive.lock(); g && *g) dock_pane(pid, z);
            }
        });
    };
    add_zone("停靠到左侧 (Left)",   DockZone::Left);
    add_zone("停靠到右侧 (Right)",  DockZone::Right);
    add_zone("停靠到顶部 (Top)",    DockZone::Top);
    add_zone("停靠到底部 (Bottom)", DockZone::Bottom);
    add_zone("停靠到中心 (Center)", DockZone::Center);

    if (closable) {
        items.push_back(MenuFlyoutItem::separator());
        items.push_back(MenuFlyoutItem{
            .label = FrameworkStrings::current().dock_close_pane,
            .on_click = [this, alive, pid]() {
                if (auto g = alive.lock(); !g || !*g) return;
                for (auto& lp : m_panes) {
                    if (lp.id == pid) {
                        if (lp.content_box) detail::forget_render_subtree(lp.content_box.get());
                        lp.closed = true;
                        mark_needs_layout();
                        mark_needs_paint();
                        break;
                    }
                }
            }
        });
    }

    Vec2 win_pos = event_window_pos;
    if (Window* w = Window::active_window()) win_pos.y += w->titlebar_height();
    MenuFlyout::show_at(std::move(items), win_pos);
}

inline void RenderDockPanel::setup_behavior() {
    on_pointer_exit = [this](PointerEvent&) {
        if (!m_dragging_pane && !m_dragging_splitter) {
            m_hovered_splitter.reset();
            m_hovered_detach_zone.reset();
            m_hovered_pane_index = SIZE_MAX;
            mark_needs_paint();
        }
    };

    on_pointer_down = [this](PointerEvent& ev) {
        PointF p = ev.local_position;

        // 右键点击停靠面板标签页或标题栏：弹出 Visual Studio 风格停靠上下文菜单
        if (ev.button == PointerButton::Secondary) {
            for (const auto& th : m_tab_hits) {
                if (th.rect.contains(p)) {
                    const size_t z_idx = static_cast<size_t>(th.zone);
                    if (m_active_tab[z_idx] != th.tab_index) {
                        m_active_tab[z_idx] = th.tab_index;
                        m_tab_anim_t[z_idx] = 0.0f;
                        mark_needs_layout();
                        mark_needs_paint();
                    }
                    open_pane_context_menu(th.pane_index, ev.position);
                    ev.stop_propagation();
                    return;
                }
            }
            for (size_t z = 0; z < 5; ++z) {
                Rect zr = m_zone_rects[z];
                if (zr.is_empty()) continue;
                if (Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight).contains(p)) {
                    auto list = panes_in_zone(static_cast<DockZone>(z));
                    if (!list.empty()) {
                        size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
                        open_pane_context_menu(list[active_idx], ev.position);
                        ev.stop_propagation();
                        return;
                    }
                }
            }
            return;
        }

        if (ev.button != PointerButton::Primary) return;

        for (const auto& sp : m_splitters) {
            if (sp.hit_rect.contains(p)) {
                m_dragging_splitter = sp.kind;
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
        }

        if (can_detach()) {
            for (size_t z = 0; z < 5; ++z) {
                Rect zr = m_zone_rects[z];
                if (zr.is_empty()) continue;
                if (detach_btn_rect(zr).contains(p)) {
                    auto list = panes_in_zone(static_cast<DockZone>(z));
                    if (!list.empty()) {
                        size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
                        detach_pane(m_panes[list[active_idx]].id);
                    }
                    ev.stop_propagation();
                    return;
                }
            }
        }

        for (const auto& th : m_tab_hits) {
            if (th.rect.contains(p)) {
                auto& pane = m_panes[th.pane_index];
                if (pane.closable && !th.close_rect.is_empty() && th.close_rect.contains(p)) {
                    if (pane.content_box) detail::forget_render_subtree(pane.content_box.get());
                    pane.closed = true;
                    mark_needs_layout();
                    mark_needs_paint();
                    ev.stop_propagation();
                    return;
                }
                const size_t z_idx = static_cast<size_t>(th.zone);
                if (m_active_tab[z_idx] != th.tab_index) {
                    auto list = panes_in_zone(th.zone);
                    if (m_active_tab[z_idx] < list.size()) {
                        auto& prev_pane = m_panes[list[m_active_tab[z_idx]]];
                        if (prev_pane.content_box) detail::forget_render_subtree(prev_pane.content_box.get());
                    }
                    m_active_tab[z_idx] = th.tab_index;
                    m_tab_anim_t[z_idx] = 0.0f;
                }
                m_dragging_pane = th.pane_index;
                m_drag_origin = p;
                m_drag_pos = p;
                m_drag_started = false;
                m_will_detach_on_drop = false;
                m_drop_target.reset();
                mark_needs_layout();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
        }
    };

    on_pointer_move = [this](PointerEvent& ev) {
        PointF p = ev.local_position;

        if (m_dragging_splitter.has_value()) {
            const float iw = std::max(1.0f, m_size.x - m_padding.horizontal());
            const float ih = std::max(1.0f, m_size.y - m_padding.vertical());
            switch (*m_dragging_splitter) {
                case SplitterKind::Left:
                    m_left_w = std::clamp(p.x - m_padding.left, kMinPaneSize, std::max(kMinPaneSize, iw - kMinPaneSize));
                    break;
                case SplitterKind::Right:
                    m_right_w = std::clamp(m_size.x - m_padding.right - p.x, kMinPaneSize, std::max(kMinPaneSize, iw - kMinPaneSize));
                    break;
                case SplitterKind::Top:
                    m_top_h = std::clamp(p.y - m_padding.top, kMinPaneSize, std::max(kMinPaneSize, ih - kMinPaneSize));
                    break;
                case SplitterKind::Bottom:
                    m_bottom_h = std::clamp(m_size.y - m_padding.bottom - p.y, kMinPaneSize, std::max(kMinPaneSize, ih - kMinPaneSize));
                    break;
            }
            mark_needs_layout();
            mark_needs_paint();
            ev.stop_propagation();
            return;
        }

        if (m_dragging_pane.has_value()) {
            m_drag_pos = p;
            if (!m_drag_started && std::hypot(p.x - m_drag_origin.x, p.y - m_drag_origin.y) >= 8.0f) {
                m_drag_started = true;
            }
            if (m_drag_started) {
                m_drop_target = hit_test_drop_target(p, m_will_detach_on_drop);
                mark_needs_paint();
            }
            ev.stop_propagation();
            return;
        }

        std::optional<SplitterKind> next_sp;
        for (const auto& sp : m_splitters) {
            if (sp.hit_rect.contains(p)) { next_sp = sp.kind; break; }
        }
        size_t next_pane = SIZE_MAX;
        for (const auto& th : m_tab_hits) {
            if (th.rect.contains(p)) { next_pane = th.pane_index; break; }
        }
        std::optional<DockZone> next_detach;
        if (can_detach()) {
            for (size_t z = 0; z < 5; ++z) {
                if (!m_zone_rects[z].is_empty() && detach_btn_rect(m_zone_rects[z]).contains(p)) {
                    next_detach = static_cast<DockZone>(z);
                    break;
                }
            }
        }
        if (next_sp != m_hovered_splitter || next_pane != m_hovered_pane_index || next_detach != m_hovered_detach_zone) {
            m_hovered_splitter = next_sp;
            m_hovered_pane_index = next_pane;
            m_hovered_detach_zone = next_detach;
            mark_needs_paint();
        }
    };

    on_pointer_up = [this](PointerEvent& ev) {
        if (m_dragging_splitter.has_value()) {
            m_dragging_splitter.reset();
            mark_needs_paint();
            ev.stop_propagation();
            return;
        }
        if (m_dragging_pane.has_value()) {
            size_t pane_idx = *m_dragging_pane;
            auto target = m_drop_target;
            bool should_detach = m_will_detach_on_drop;
            bool was_dragging = m_drag_started;
            m_dragging_pane.reset();
            m_drop_target.reset();
            m_drag_started = false;
            m_will_detach_on_drop = false;
            if (was_dragging && pane_idx < m_panes.size()) {
                if (should_detach) {
                    detach_pane(m_panes[pane_idx].id);
                } else if (target.has_value()) {
                    dock_pane(m_panes[pane_idx].id, *target);
                } else {
                    mark_needs_paint();
                }
            } else {
                mark_needs_paint();
            }
            ev.stop_propagation();
        }
    };
}

} // namespace ktui
