#pragma once

namespace ktui {

inline void RenderDockPanel::paint(PaintContext& context) {
    advance_animations();

    const Theme& t = ThemeContext::current();
    const auto resolved_style = DockPanelStyleResolver::resolve(t);
    const bool is_dark = t.is_dark;

    const Color outer_bg     = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.025f} : Color{0.0f, 0.0f, 0.0f, 0.025f};
    const Color outer_border = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.09f}  : Color{0.0f, 0.0f, 0.0f, 0.10f};
    const Color center_bg    = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.042f} : Color{1.0f, 1.0f, 1.0f, 0.88f};
    const Color tool_bg      = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.018f} : Color{0.96f, 0.97f, 0.98f, 0.80f};
    const Color bar_bg       = is_dark ? Color{0.0f, 0.0f, 0.0f, 0.14f}  : Color{0.0f, 0.0f, 0.0f, 0.03f};
    const Color hairline     = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.075f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
    const Color text_pri     = resolved_style.text_primary;
    const Color text_muted   = resolved_style.text_secondary;
    const Color accent       = resolved_style.accent;

    Rect outer = Rect::from_xywh(m_padding.left, m_padding.top,
                                 std::max(0.0f, m_size.x - m_padding.horizontal()),
                                 std::max(0.0f, m_size.y - m_padding.vertical()));
    context.draw_rect(outer, outer_bg, 8.0f, 0.0f);
    context.push_clip(outer, 8.0f);

    for (size_t z = 0; z < 5; ++z) {
        DockZone dz = static_cast<DockZone>(z);
        auto list = panes_in_zone(dz);
        if (list.empty()) continue;
        Rect target_zr = m_zone_rects[z];
        if (target_zr.is_empty()) continue;
        Rect zr = m_anim_zone_init[z] ? m_anim_zone_rects[z] : target_zr;
        const float dx = zr.min_x - target_zr.min_x;
        const float dy = zr.min_y - target_zr.min_y;

        context.push_clip(zr, 0.0f);

        const Color pane_bg = (dz == DockZone::Center) ? center_bg : tool_bg;
        context.draw_rect(zr, pane_bg, 0.0f, 0.0f);

        Rect header_r = Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight);
        context.draw_rect(header_r, bar_bg, 0.0f, 0.0f);
        context.draw_rect(Rect::from_xywh(zr.min_x, zr.min_y + kHeaderHeight - 1.0f, zr.width(), 1.0f), hairline, 0.0f, 0.0f);

        for (const auto& th : m_tab_hits) {
            if (th.zone != dz) continue;
            const bool is_active = (th.tab_index == m_active_tab[z]);
            const bool is_hovered = (m_hovered_pane_index == th.pane_index);
            const auto& pane = m_panes[th.pane_index];
            Rect tr = Rect::from_xywh(th.rect.min_x + dx, th.rect.min_y + dy, th.rect.width(), th.rect.height());

            if (is_active) {
                context.draw_rect(Rect::from_xywh(tr.min_x, tr.min_y + 2.0f, tr.width(), tr.height() - 2.0f),
                                  pane_bg, 4.0f, 0.0f);
            } else if (is_hovered) {
                Color hov = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.045f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
                context.draw_rect(Rect::from_xywh(tr.min_x, tr.min_y + 3.0f, tr.width(), tr.height() - 6.0f),
                                  hov, 4.0f, 0.0f);
            }

            const FontWeight fw = is_active ? FontWeight::SemiBold : FontWeight::Regular;
            const float text_x = tr.min_x + 10.0f;
            const bool show_close = pane.closable && !th.close_rect.is_empty() && (is_active || is_hovered);
            Rect cr = show_close
                ? Rect::from_xywh(th.close_rect.min_x + dx, th.close_rect.min_y + dy, th.close_rect.width(), th.close_rect.height())
                : Rect{};
            const float text_max_w = std::max(0.0f, (show_close ? cr.min_x - 2.0f : tr.max_x - 6.0f) - text_x);
            if (text_max_w > 4.0f) {
                Vec2 ls = TextMeasure::measure(pane.title, 12.0f, fw);
                float text_y = std::round(tr.min_y + (tr.height() - ls.y) * 0.5f);
                context.push_clip(Rect::from_xywh(text_x, tr.min_y, text_max_w, tr.height()), 0.0f);
                context.draw_text(pane.title, PointF{std::round(text_x), text_y}, 12.0f,
                                  is_active ? text_pri : text_muted, fw);
                context.pop_clip();
            }

            if (show_close) {
                Vec2 xs = TextMeasure::measure("×", 13.0f);
                context.draw_text("×", PointF{std::round(cr.min_x + (cr.width() - xs.x) * 0.5f),
                                              std::round(cr.min_y + (cr.height() - xs.y) * 0.5f - 0.5f)},
                                  13.0f, text_muted);
            }
        }

        // 平滑滑动的 Fluent 2 激活标签指示条
        if (m_tab_ind_init[z] && m_tab_ind_w[z] > 2.0f) {
            context.draw_rect(Rect::from_xywh(m_tab_ind_x[z], zr.min_y + kHeaderHeight - 2.0f,
                                              m_tab_ind_w[z], 2.0f),
                              accent, 1.0f, 0.0f);
        }

        if (can_detach()) {
            Rect dr = detach_btn_rect(zr);
            const bool d_hov = (m_hovered_detach_zone == dz);
            if (d_hov) {
                Color hov = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.07f};
                context.draw_rect(dr, hov, 4.0f, 0.0f);
            }
            Vec2 isz = TextMeasure::measure("↗", 12.0f);
            context.draw_text("↗", PointF{std::round(dr.min_x + (dr.width() - isz.x) * 0.5f),
                                          std::round(dr.min_y + (dr.height() - isz.y) * 0.5f)},
                              12.0f, d_hov ? accent : Color{text_muted.r, text_muted.g, text_muted.b, 0.65f});
        }

        size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
        auto& active_pane = m_panes[list[active_idx]];
        if (active_pane.content_box && active_pane.content_box->parent() == this) {
            Rect content_clip = Rect::from_xywh(zr.min_x, zr.min_y + kHeaderHeight,
                                                zr.width(), std::max(0.0f, zr.height() - kHeaderHeight));
            const float ease = 1.0f - std::pow(1.0f - std::clamp(m_tab_anim_t[z], 0.0f, 1.0f), 3.0f);
            const float slide_y = (1.0f - ease) * 10.0f;
            context.push_clip(content_clip, 0.0f);
            {
                Vec2 off = active_pane.content_box->offset();
                PaintContext::OffsetScope sc(context, Vec2{off.x + dx, off.y + dy + slide_y});
                active_pane.content_box->paint(context);
            }
            context.pop_clip();
        }

        context.pop_clip();
    }

    // 直接复用 SplitView 的 Fluent 2 分隔条（悬停/拖拽胶囊把手 + 主题色轨道高亮）
    for (const auto& sp : m_splitters) {
        const bool is_vertical = (sp.kind == SplitterKind::Top || sp.kind == SplitterKind::Bottom);
        const bool is_hovered  = (m_hovered_splitter == sp.kind);
        const bool is_dragging = (m_dragging_splitter == sp.kind);
        RenderSplitView::paint_splitter(context, sp.hit_rect, is_vertical, is_hovered, is_dragging, true);
    }

    const bool show_drop_overlay = (m_dragging_pane.has_value() && m_drag_started) || m_external_drag_active;
    if (show_drop_overlay) {
        if (m_drop_target.has_value()) {
            Rect preview = compute_drop_preview(*m_drop_target);
            context.draw_rect(preview, Color{accent.r, accent.g, accent.b, 0.18f}, 6.0f, 0.0f);
            context.draw_rect(preview, Color{accent.r, accent.g, accent.b, 0.85f}, 6.0f, 1.5f);
        }
        if (m_external_drag_active || !m_will_detach_on_drop) {
            paint_docking_compass(context, is_dark, accent, text_pri);
        }
    }

    context.pop_clip();
    context.draw_rect(outer, outer_border, 8.0f, 1.0f);

    if (m_dragging_pane.has_value() && m_drag_started) {
        const auto& dp = m_panes[*m_dragging_pane];
        std::string label = m_will_detach_on_drop
            ? (dp.title + FrameworkStrings::current().dock_release_to_float_suffix)
            : dp.title;
        Vec2 gs = TextMeasure::measure(label, 12.0f, FontWeight::SemiBold);
        float gw = std::max(96.0f, gs.x + 24.0f);
        Rect ghost = Rect::from_xywh(m_drag_pos.x - gw * 0.5f, m_drag_pos.y - 15.0f, gw, 30.0f);
        Color ghost_bg = is_dark ? Color{0.14f, 0.16f, 0.20f, 0.95f} : Color{1.0f, 1.0f, 1.0f, 0.96f};
        context.draw_rect(ghost, ghost_bg, 6.0f, 0.0f);
        context.draw_rect(ghost, accent, 6.0f, 1.5f);
        context.draw_text(label, PointF{std::round(ghost.min_x + (gw - gs.x) * 0.5f),
                                        std::round(ghost.min_y + (30.0f - gs.y) * 0.5f)},
                          12.0f, text_pri, FontWeight::SemiBold);
    }
}

inline void RenderDockPanel::advance_animations() {
    auto now = std::chrono::steady_clock::now();
    float dt = (m_anim_last_time.time_since_epoch().count() == 0)
        ? 0.016f
        : std::clamp(std::chrono::duration<float>(now - m_anim_last_time).count(), 0.001f, 0.05f);
    m_anim_last_time = now;

    const bool snap = m_dragging_splitter.has_value();
    const float k_zone = 1.0f - std::exp(-dt * 16.0f);
    const float k_ind  = 1.0f - std::exp(-dt * 22.0f);
    bool any_anim = false;

    for (size_t z = 0; z < 5; ++z) {
        const Rect& target = m_zone_rects[z];
        if (target.is_empty()) {
            m_anim_zone_init[z] = false;
            m_tab_ind_init[z] = false;
            continue;
        }
        if (!m_anim_zone_init[z] || snap) {
            m_anim_zone_rects[z] = target;
            m_anim_zone_init[z] = true;
        } else {
            Rect& cur = m_anim_zone_rects[z];
            cur.min_x += (target.min_x - cur.min_x) * k_zone;
            cur.min_y += (target.min_y - cur.min_y) * k_zone;
            cur.max_x += (target.max_x - cur.max_x) * k_zone;
            cur.max_y += (target.max_y - cur.max_y) * k_zone;
            float diff = std::max({std::abs(target.min_x - cur.min_x), std::abs(target.min_y - cur.min_y),
                                   std::abs(target.max_x - cur.max_x), std::abs(target.max_y - cur.max_y)});
            if (diff > 0.35f) any_anim = true;
            else cur = target;
        }

        if (m_tab_anim_t[z] < 1.0f) {
            m_tab_anim_t[z] = std::min(1.0f, m_tab_anim_t[z] + dt / 0.18f);
            if (m_tab_anim_t[z] < 1.0f) any_anim = true;
        }

        // 平滑滑动激活标签指示条
        const float dx = m_anim_zone_rects[z].min_x - target.min_x;
        for (const auto& th : m_tab_hits) {
            if (static_cast<size_t>(th.zone) == z && th.tab_index == m_active_tab[z]) {
                const float tx = th.rect.min_x + dx + 8.0f;
                const float tw = std::max(10.0f, th.rect.width() - 16.0f);
                if (!m_tab_ind_init[z] || snap) {
                    m_tab_ind_x[z] = tx;
                    m_tab_ind_w[z] = tw;
                    m_tab_ind_init[z] = true;
                } else {
                    m_tab_ind_x[z] += (tx - m_tab_ind_x[z]) * k_ind;
                    m_tab_ind_w[z] += (tw - m_tab_ind_w[z]) * k_ind;
                    if (std::abs(tx - m_tab_ind_x[z]) > 0.35f || std::abs(tw - m_tab_ind_w[z]) > 0.35f) {
                        any_anim = true;
                    } else {
                        m_tab_ind_x[z] = tx;
                        m_tab_ind_w[z] = tw;
                    }
                }
                break;
            }
        }
    }

    if (any_anim) {
        mark_needs_paint();
    }
}

} // namespace ktui
