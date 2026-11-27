#pragma once

namespace ktui {

inline void RenderNavigationView::paint(PaintContext& ctx) {
    update_animations();

    const Theme& theme = ThemeContext::current();
    const ResolvedNavigationViewStyle style = NavigationViewStyleResolver::resolve(theme);
    const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

    // 1. 内容区卡片表面与页面内容
    paint_content_area(ctx, style, eff);

    // 2. 导航栏 (盖在内容区上方)
    if (eff == NavigationPaneDisplayMode::Top) {
        paint_top_nav(ctx, style);
    } else {
        paint_left_pane(ctx, style, eff);
    }
}

inline void RenderNavigationView::paint_content_area(PaintContext& ctx, const ResolvedNavigationViewStyle& style, NavigationPaneDisplayMode eff) {
    Rect surf = compute_content_surface_rect(size().x, size().y, eff);
    float tl_rad = (eff == NavigationPaneDisplayMode::Top || eff == NavigationPaneDisplayMode::LeftMinimal)
        ? 0.0f : style.content_radius;
    ctx.draw_rounded_rect(surf, style.content_surface_bg, tl_rad, 0.0f, 0.0f, 0.0f);

    if (style.content_surface_border.a > 0.001f) {
        if (tl_rad > 0.0f) {
            // 使用带左上角独立圆角的 1px 完整描边，右/下侧外扩 4px 并裁剪到 surf，
            // 让 1px 边框顺着左上圆角弧线连续无缝绘制，绝不在圆角处截断
            ctx.push_clip(surf, 0.0f);
            ctx.draw_rounded_rect(
                Rect::from_xywh(surf.min_x, surf.min_y, surf.width() + 4.0f, surf.height() + 4.0f),
                style.content_surface_border,
                tl_rad, 0.0f, 0.0f, 0.0f,
                0.0f, Vec2{0.0f, 0.0f}, 1.0f);
            ctx.pop_clip();
        } else {
            ctx.draw_rect(Rect::from_xywh(surf.min_x, surf.min_y, surf.width(), 1.0f),
                          style.content_surface_border, 0.0f, 0.0f);
        }
    }

    if (!m_data.header.empty()) {
        ctx.draw_text(m_data.header, PointF{std::round(surf.min_x + 24.0f), std::round(surf.min_y + 14.0f)},
                      style.page_header_font_size, style.page_header_text, FontWeight::SemiBold);
    }

    if (m_content_box) {
        Rect body = compute_content_body_rect(size().x, size().y, eff);
        float ease = 1.0f - std::pow(1.0f - m_page_enter_progress, 3.0f);
        float slide_y = std::round((1.0f - ease) * 10.0f);

        ctx.push_clip(body, 0.0f);
        PaintContext::OffsetScope scope(ctx, Vec2{m_content_box->offset().x, m_content_box->offset().y + slide_y});
        m_content_box->paint(ctx);
        ctx.pop_clip();
    }
}

inline void RenderNavigationView::paint_chrome_button(PaintContext& ctx, Rect r, ChromeButton btn,
                             const ResolvedNavigationViewStyle& style, bool enabled) const {
    bool hov = (m_hovered_chrome == btn && enabled);
    bool prs = (m_pressed_chrome == btn && enabled);
    if (prs) {
        ctx.draw_rect(r, style.item_pressed_bg, style.item_radius, 0.0f);
    } else if (hov) {
        ctx.draw_rect(r, style.item_hover_bg, style.item_radius, 0.0f);
    }
    paint_item_pointer_reveal(ctx, r, style.item_radius, enabled);
    Color col = enabled ? style.icon_color : style.item_disabled_text;
    float cx = std::round(r.min_x + r.width() * 0.5f);
    float cy = std::round(r.min_y + r.height() * 0.5f);

    if (btn == ChromeButton::Back) {
        // WinUI 3 AnimatedBackVisualSource: 按下轻推向左 -2px
        float ox = prs ? -2.0f : 0.0f;
        ctx.draw_line(PointF{cx - 5.5f + ox, cy}, PointF{cx + 5.5f + ox, cy}, col, 1.3f);
        ctx.draw_line(PointF{cx - 5.5f + ox, cy}, PointF{cx - 1.2f + ox, cy - 4.2f}, col, 1.3f);
        ctx.draw_line(PointF{cx - 5.5f + ox, cy}, PointF{cx - 1.2f + ox, cy + 4.2f}, col, 1.3f);
    } else if (btn == ChromeButton::Toggle) {
        // WinUI 3 AnimatedGlobalNavigationButtonVisualSource: 按下微缩收紧
        float hw = prs ? 4.8f : 6.0f;
        ctx.draw_line(PointF{cx - hw, cy - 4.0f}, PointF{cx + hw, cy - 4.0f}, col, 1.25f);
        ctx.draw_line(PointF{cx - hw, cy},        PointF{cx + hw, cy},        col, 1.25f);
        ctx.draw_line(PointF{cx - hw, cy + 4.0f}, PointF{cx + hw, cy + 4.0f}, col, 1.25f);
    }
}

inline void RenderNavigationView::paint_compact_tooltip(PaintContext& ctx, float pane_w, float item_cy,
                               const std::string& label, const ResolvedNavigationViewStyle& style) const {
    if (label.empty()) return;
    Vec2 ts = TextMeasure::measure(label, 12.0f);
    float tw = std::ceil(ts.x + 18.0f);
    float th = 28.0f;
    float tx = pane_w + 6.0f;
    float ty = std::round(item_cy - th * 0.5f);
    Rect tr = Rect::from_xywh(tx, ty, tw, th);

    const bool is_dark = ThemeContext::current().is_dark;
    ctx.draw_elevation_shadow(tr, 4.0f, is_dark, 16.0f, 1.0f);
    ctx.draw_rect(tr, style.pane_overlay_bg, 4.0f, 0.0f);
    ctx.draw_rect(tr, style.pane_overlay_border, 4.0f, 1.0f);
    ctx.draw_text(label, PointF{std::round(tx + 9.0f), std::round(ty + (th - ts.y) * 0.5f)}, 12.0f, style.item_selected_text);
}

inline void RenderNavigationView::paint_left_pane(PaintContext& ctx, const ResolvedNavigationViewStyle& style, NavigationPaneDisplayMode eff) {
    const float pane_w = current_pane_width(eff);
    const bool overlay = is_overlay_pane(eff) && m_pane_open_anim > 0.02f;

    if (eff == NavigationPaneDisplayMode::LeftMinimal && m_pane_open_anim <= 0.02f) {
        float bx = 4.0f;
        if (m_data.is_back_button_visible) {
            paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 40.0f, 36.0f), ChromeButton::Back, style, can_go_back());
            bx += 42.0f;
        }
        if (m_data.is_pane_toggle_button_visible) {
            paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 40.0f, 36.0f), ChromeButton::Toggle, style, true);
        }
        return;
    }

    Rect pane_rect = Rect::from_xywh(0.0f, 0.0f, pane_w, size().y);

    if (overlay) {
        const bool is_dark = ThemeContext::current().is_dark;
        ctx.draw_elevation_shadow(pane_rect, 8.0f, is_dark, 48.0f, m_pane_open_anim, Vec2{1.0f, 0.0f});
        ctx.draw_rounded_rect(pane_rect, style.pane_overlay_bg, 0.0f, 8.0f, 8.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(pane_w - 1.0f, 0.0f, 1.0f, size().y), style.pane_overlay_border, 0.0f, 0.0f);
    }

    // 1. 紧凑单行顶部操作区: Back + Hamburger + PaneTitle 合并为同一行 (y = 4, h = 36)
    //    折叠态仅保留单个 40px 按钮 (优先 Toggle)，展开态 Back(4..40) + Toggle(42..78) + Title(84..)
    {
        const float cy = 4.0f;
        float title_x = 12.0f;
        if (m_data.is_back_button_visible && m_data.is_pane_toggle_button_visible) {
            if (m_pane_open_anim > 0.35f) {
                paint_chrome_button(ctx, Rect::from_xywh(4.0f, cy, 36.0f, 36.0f), ChromeButton::Back, style, can_go_back());
            }
            float tx = 4.0f + 38.0f * m_pane_open_anim;
            float tw = 40.0f - 4.0f * m_pane_open_anim;
            paint_chrome_button(ctx, Rect::from_xywh(tx, cy, tw, 36.0f), ChromeButton::Toggle, style, true);
            title_x = 84.0f;
        } else if (m_data.is_back_button_visible) {
            paint_chrome_button(ctx, Rect::from_xywh(4.0f, cy, 40.0f, 36.0f), ChromeButton::Back, style, can_go_back());
            title_x = 48.0f;
        } else if (m_data.is_pane_toggle_button_visible) {
            paint_chrome_button(ctx, Rect::from_xywh(4.0f, cy, 40.0f, 36.0f), ChromeButton::Toggle, style, true);
            title_x = 48.0f;
        }

        if (!m_data.pane_title.empty() && m_pane_open_anim > 0.45f) {
            float ta = std::clamp((m_pane_open_anim - 0.45f) / 0.55f, 0.0f, 1.0f);
            Color tc = style.item_selected_text; tc.a *= ta;
            Vec2 ts = TextMeasure::measure(m_data.pane_title, 13.5f, FontWeight::SemiBold);
            float clip_w = std::max(0.0f, pane_w - title_x - 8.0f);
            if (clip_w > 2.0f) {
                ctx.push_clip(Rect::from_xywh(title_x, cy, clip_w, 36.0f), 0.0f);
                ctx.draw_text(m_data.pane_title, PointF{title_x, std::round(cy + (36.0f - ts.y) * 0.5f)},
                              13.5f, tc, FontWeight::SemiBold);
                ctx.pop_clip();
            }
        }
    }

    // 1.5 内置左侧搜索框 (仅在展开态渲染；侧边折叠后自动隐藏且不占任何高度空间)
    if (m_data.is_search_box_visible && m_pane_open_anim > 0.55f && m_search_box) {
        float sy = compute_left_search_top();
        float sw = std::max(80.0f, pane_w - 20.0f);
        m_search_box->layout(BoxConstraints::tight(Vec2{sw, 32.0f}), false);
        m_search_box->set_offset(Vec2{10.0f, sy});
        PaintContext::OffsetScope scope(ctx, m_search_box->offset());
        m_search_box->paint(ctx);
    }

    // 2. 可滚动主菜单区域
    Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
    const float total_menu_h = compute_left_menu_total_height();
    m_menu_scroll.set_max_scroll(std::max(0.0f, total_menu_h - menu_vp.height()));
    if (m_restore_expanded_scroll && m_pane_open_anim >= 0.35f) {
        m_menu_scroll.jump_to(m_saved_expanded_scroll_y);
        m_pill_last_scroll_y = m_menu_scroll.offset();
        m_pill_initialized = false;
        m_restore_expanded_scroll = false;
    } else if (m_data.is_pane_open && m_pane_open_anim >= 0.99f) {
        m_saved_expanded_scroll_y = m_menu_scroll.offset();
    }
    const float scroll_y = m_menu_scroll.offset();
    const float label_alpha = std::clamp((m_pane_open_anim - 0.35f) / 0.65f, 0.0f, 1.0f);
    const bool is_compact = (m_pane_open_anim < 0.25f);

    // 同步菜单顶部偏移变化 (如搜索行展开/收起)，防止小蓝条出现滞后拖尾
    if (m_pill_initialized && m_pill_in_menu) {
        float dtop = menu_vp.min_y - m_pill_last_menu_top;
        if (std::abs(dtop) > 0.001f) {
            m_pill_head += dtop;
            m_pill_tail += dtop;
            m_pill_target_pos += dtop;
        }
    }
    m_pill_last_menu_top = menu_vp.min_y;

    float target_pill_y = -1.0f;
    bool target_pill_in_menu = false;
    std::string tooltip_label;
    float tooltip_cy = 0.0f;

    ctx.push_clip(menu_vp, 0.0f);
    float y = menu_vp.min_y + 2.0f - scroll_y;
    const int main_count = static_cast<int>(m_main_rows.size());
    for (int i = 0; i < main_count; ) {
        const auto& row = m_main_rows[static_cast<size_t>(i)];
        if (!row.item) { ++i; continue; }
        if (label_alpha <= 0.01f && (row.item->kind == NavigationItemKind::Header || row.depth > 0)) {
            ++i; continue;
        }

        // 子菜单手风琴分组：整组平滑裁切展开/收起 + 轻微下拉滑入 + 透明度淡入淡出
        if (row.depth > 0 && row.parent) {
            const NavigationViewItem* grp_parent = row.parent;
            int j = i;
            float full_h = 0.0f;
            while (j < main_count && m_main_rows[static_cast<size_t>(j)].depth > 0 &&
                   m_main_rows[static_cast<size_t>(j)].parent == grp_parent) {
                full_h += m_main_rows[static_cast<size_t>(j)].height + 2.0f;
                ++j;
            }
            float ef = get_expand_anim(grp_parent->effective_id(), grp_parent->expanded);
            if (ef > 0.004f) {
                float vis_h = full_h * ef;
                Rect grp_clip = Rect::from_xywh(0.0f, y, pane_w, vis_h);
                float slide_up = (1.0f - ef) * std::min(16.0f, full_h * 0.25f);
                float child_alpha = label_alpha * std::clamp(ef * 1.25f, 0.0f, 1.0f);

                ctx.push_clip(grp_clip, 0.0f);
                float cy = y - slide_up;
                for (int k = i; k < j; ++k) {
                    const auto& crow = m_main_rows[static_cast<size_t>(k)];
                    if (!crow.item) continue;
                    bool is_sel = is_item_or_child_selected(*crow.item, true);
                    if (is_sel && ef > 0.5f) {
                        target_pill_y = cy + (crow.height - 16.0f) * 0.5f;
                        target_pill_in_menu = true;
                    }
                    Rect row_rect = Rect::from_xywh(4.0f, cy, std::max(0.0f, pane_w - 8.0f), crow.height);
                    if (cy + crow.height >= std::max(menu_vp.min_y, grp_clip.min_y) &&
                        cy <= std::min(menu_vp.max_y, grp_clip.max_y)) {
                        bool is_hov = (m_hovered_row == k && !m_hovered_in_footer && ef > 0.85f);
                        bool is_prs = (m_pressed_row == k && !m_pressed_in_footer && ef > 0.85f);
                        paint_left_row(ctx, row_rect, crow, style, is_sel, is_hov, is_prs, child_alpha);
                    }
                    cy += crow.height + 2.0f;
                }
                ctx.pop_clip();
                y += vis_h;
            }
            i = j;
            continue;
        }

        float p_ef = (!row.item->children.empty())
            ? get_expand_anim(row.item->effective_id(), row.item->expanded) : 0.0f;
        bool children_vis = (label_alpha > 0.01f && p_ef > 0.5f);
        bool is_sel = is_item_or_child_selected(*row.item, children_vis);
        if (is_sel) {
            target_pill_y = y + (row.height - 16.0f) * 0.5f;
            target_pill_in_menu = true;
        }

        Rect row_rect = Rect::from_xywh(4.0f, y, std::max(0.0f, pane_w - 8.0f), row.height);
        if (y + row.height >= menu_vp.min_y && y <= menu_vp.max_y) {
            bool is_hov = (m_hovered_row == i && !m_hovered_in_footer);
            bool is_prs = (m_pressed_row == i && !m_pressed_in_footer);
            if (is_compact && is_hov && row.item->kind == NavigationItemKind::Item && m_submenu_overlay_id == 0) {
                tooltip_label = row.item->label;
                tooltip_cy = y + row.height * 0.5f;
            }
            paint_left_row(ctx, row_rect, row, style, is_sel, is_hov, is_prs, label_alpha);
        }
        y += row.height + 2.0f;
        ++i;
    }
    m_menu_scrollbar.paint(ctx, menu_vp, total_menu_h, scroll_y, ThemeContext::current());
    ctx.pop_clip();

    // 3. 底部固定 Footer 区域 (分割线 + footer_items + Settings 旋转齿轮)
    if (!m_footer_rows.empty()) {
        float fy = menu_vp.max_y + 4.0f;
        ctx.draw_rect(Rect::from_xywh(12.0f, fy, std::max(0.0f, pane_w - 24.0f), 1.0f),
                      style.separator_color, 0.0f, 0.0f);
        fy += 7.0f;

        for (int i = 0; i < static_cast<int>(m_footer_rows.size()); ++i) {
            const auto& row = m_footer_rows[static_cast<size_t>(i)];
            if (!row.item) continue;
            Rect row_rect = Rect::from_xywh(4.0f, fy, std::max(0.0f, pane_w - 8.0f), row.height);
            bool is_sel = (row.item->kind == NavigationItemKind::Item &&
                           row.item->effective_id() == m_data.selected_id);
            bool is_hov = (m_hovered_row == i && m_hovered_in_footer);
            bool is_prs = (m_pressed_row == i && m_pressed_in_footer);
            if (is_sel) {
                target_pill_y = fy + (row.height - 16.0f) * 0.5f;
                target_pill_in_menu = false;
            }
            if (is_compact && is_hov && row.item->kind == NavigationItemKind::Item && m_submenu_overlay_id == 0) {
                tooltip_label = row.item->label;
                tooltip_cy = fy + row.height * 0.5f;
            }
            paint_left_row(ctx, row_rect, row, style, is_sel, is_hov, is_prs, label_alpha);
            fy += row.height + 2.0f;
        }
    }

    // 4. WinUI 3 弹性拉伸滑动强调条: 切换选中项时弹性动画，滚动列表时严格同步不拖尾
    if (target_pill_y >= -2000.0f) {
        if (!m_pill_initialized) {
            m_pill_head = m_pill_tail = target_pill_y;
            m_pill_size = 16.0f;
            m_pill_initialized = true;
        }
        m_pill_in_menu = target_pill_in_menu;
        m_pill_target_pos = target_pill_y;
        m_pill_target_size = 16.0f;
        if (std::abs(m_pill_head - m_pill_target_pos) > 0.25f ||
            std::abs(m_pill_tail - m_pill_target_pos) > 0.25f ||
            std::abs(m_pill_size - m_pill_target_size) > 0.25f) {
            mark_needs_paint();
        }
        float pill_top = std::min(m_pill_head, m_pill_tail);
        float pill_bot = std::max(m_pill_head, m_pill_tail) + m_pill_size;
        if (target_pill_in_menu) {
            ctx.push_clip(menu_vp, 0.0f);
            ctx.draw_rect(Rect::from_xywh(4.0f, pill_top, 3.0f, std::max(16.0f, pill_bot - pill_top)),
                          style.accent_pill, 1.5f, 0.0f);
            ctx.pop_clip();
        } else {
            ctx.draw_rect(Rect::from_xywh(4.0f, pill_top, 3.0f, std::max(16.0f, pill_bot - pill_top)),
                          style.accent_pill, 1.5f, 0.0f);
        }
    }

    // 5. Compact 模式悬停 Tooltip (悬浮在内容区左侧上方)
    if (!tooltip_label.empty()) {
        paint_compact_tooltip(ctx, pane_w, tooltip_cy, tooltip_label, style);
    }
}

inline void RenderNavigationView::paint_left_row(PaintContext& ctx, Rect r, const FlatRow& row,
                        const ResolvedNavigationViewStyle& style,
                        bool is_selected, bool is_hov, bool is_prs, float label_alpha) const {
    const auto& item = *row.item;
    if (item.kind == NavigationItemKind::Separator) {
        ctx.draw_rect(Rect::from_xywh(r.min_x + 8.0f, std::round(r.min_y + r.height() * 0.5f),
                                      std::max(0.0f, r.width() - 16.0f), 1.0f),
                      style.separator_color, 0.0f, 0.0f);
        return;
    }
    if (item.kind == NavigationItemKind::Header) {
        Color hc = style.header_text;
        hc.a *= label_alpha;
        Vec2 ts = TextMeasure::measure(item.label, style.header_font_size);
        ctx.draw_text(item.label, PointF{r.min_x + 12.0f, std::round(r.min_y + (r.height() - ts.y) * 0.5f + 3.0f)},
                      style.header_font_size, hc);
        return;
    }

    if (!item.disabled) {
        if (is_prs) ctx.draw_rect(r, style.item_pressed_bg, style.item_radius, 0.0f);
        else if (is_hov) ctx.draw_rect(r, style.item_hover_bg, style.item_radius, 0.0f);
        else if (is_selected) ctx.draw_rect(r, style.item_selected_bg, style.item_radius, 0.0f);
    }
    paint_item_pointer_reveal(ctx, r, style.item_radius, !item.disabled, (row.depth > 0) ? label_alpha : 1.0f);

    const float indent = static_cast<float>(row.depth) * 24.0f;
    const float icon_cx = std::round(r.min_x + 20.0f + indent);
    const float icon_cy = std::round(r.min_y + r.height() * 0.5f);
    Rect icon_rect = Rect::from_xywh(icon_cx - 8.0f, icon_cy - 8.0f, 16.0f, 16.0f);
    Color ic = item.disabled ? style.item_disabled_text
             : (is_selected ? style.icon_selected_color : style.icon_color);

    if (row.is_settings) {
        // WinUI 3 AnimatedSettingsVisualSource: 悬停微转 + 点击顺滑旋转 90° + 按下微缩
        float gear_scale = is_prs ? 0.90f : 1.0f;
        NavigationViewStyleResolver::paint_settings_gear(ctx, icon_rect, ic, m_settings_gear_angle, gear_scale);
    } else if (item.icon_painter) {
        item.icon_painter(ctx, icon_rect, ic);
    } else if (item.icon != NavigationIcon::None) {
        NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, icon_rect, ic);
    }

    // Compact 模式下: 有子项时在右下角绘制精致小指示箭头，有徽标时在右上角绘制小圆点
    if (label_alpha < 0.3f) {
        if (!item.badge.empty()) {
            ctx.draw_rect(Rect::from_xywh(icon_cx + 4.0f, icon_cy - 8.0f, 6.0f, 6.0f),
                          style.badge_bg, 3.0f, 0.0f);
        }
        if (!item.children.empty()) {
            Color dot_c = ic; dot_c.a *= 0.6f;
            ctx.draw_line(PointF{r.max_x - 6.0f, icon_cy - 2.0f}, PointF{r.max_x - 4.0f, icon_cy}, dot_c, 1.1f);
            ctx.draw_line(PointF{r.max_x - 4.0f, icon_cy}, PointF{r.max_x - 6.0f, icon_cy + 2.0f}, dot_c, 1.1f);
        }
    }

    if (label_alpha > 0.01f) {
        float right_reserve = 8.0f;
        if (!item.children.empty()) {
            float cx = r.max_x - 14.0f;
            float cy = std::round(r.min_y + r.height() * 0.5f);
            float ef = get_expand_anim(item.effective_id(), item.expanded);
            float ang = ef * 3.14159265f; // 0 (向下 v) -> π (向上 ^) 平滑 180° 旋转
            float ca = std::cos(ang), sa = std::sin(ang);
            auto rot = [&](float dx, float dy) {
                return PointF{cx + dx * ca - dy * sa, cy + dx * sa + dy * ca};
            };
            Color ch_col = ic;
            ch_col.a *= label_alpha * 0.75f;
            PointF pL = rot(-3.2f, -1.6f);
            PointF pM = rot( 0.0f,  1.6f);
            PointF pR = rot( 3.2f, -1.6f);
            ctx.draw_line(pL, pM, ch_col, 1.2f);
            ctx.draw_line(pM, pR, ch_col, 1.2f);
            right_reserve += 20.0f;
        }

        if (!item.badge.empty()) {
            Vec2 bsz = TextMeasure::measure(item.badge, 10.5f);
            float bw = std::max(18.0f, std::ceil(bsz.x + 10.0f));
            float bh = 16.0f;
            float bx = std::round(r.max_x - right_reserve - bw);
            float by = std::round(r.min_y + (r.height() - bh) * 0.5f);
            Color bg = style.badge_bg; bg.a *= label_alpha;
            Color bt = style.badge_text; bt.a *= label_alpha;
            ctx.draw_rect(Rect::from_xywh(bx, by, bw, bh), bg, bh * 0.5f, 0.0f);
            ctx.draw_text(item.badge, PointF{std::round(bx + (bw - bsz.x) * 0.5f), std::round(by + (bh - bsz.y) * 0.5f)}, 10.5f, bt);
            right_reserve += bw + 6.0f;
        }

        float text_x = (item.icon != NavigationIcon::None || item.icon_painter || row.is_settings)
            ? (r.min_x + 40.0f + indent)
            : (row.depth > 0 ? (r.min_x + 40.0f + static_cast<float>(row.depth - 1) * 16.0f) : (r.min_x + 14.0f));
        float clip_w = std::max(0.0f, r.max_x - right_reserve - text_x);
        if (clip_w > 2.0f) {
            Color tc = item.disabled ? style.item_disabled_text
                     : (is_selected ? style.item_selected_text : style.item_text);
            tc.a *= label_alpha;
            Vec2 ts = TextMeasure::measure(item.label, style.font_size);
            ctx.push_clip(Rect::from_xywh(text_x, r.min_y, clip_w, r.height()), 0.0f);
            ctx.draw_text(item.label, PointF{std::round(text_x), std::round(r.min_y + (r.height() - ts.y) * 0.5f)}, style.font_size, tc);
            ctx.pop_clip();
        }
    }
}

inline void RenderNavigationView::paint_top_nav(PaintContext& ctx, const ResolvedNavigationViewStyle& style) {
    if (m_data.is_back_button_visible) {
        paint_chrome_button(ctx, Rect::from_xywh(6.0f, 4.0f, 38.0f, 36.0f), ChromeButton::Back, style, can_go_back());
    }

    auto layouts = compute_top_layouts();
    float target_pill_x = -1.0f;
    float target_pill_w = 16.0f;

    for (int i = 0; i < static_cast<int>(layouts.size()); ++i) {
        const auto& tl = layouts[static_cast<size_t>(i)];
        const auto& item = *tl.item;
        bool is_sel = is_item_or_child_selected(item, false);
        bool is_hov = (m_hovered_top == i && !item.disabled);
        bool is_prs = (m_pressed_top == i && !item.disabled);

        if (is_prs) ctx.draw_rect(tl.rect, style.item_pressed_bg, style.item_radius, 0.0f);
        else if (is_hov) ctx.draw_rect(tl.rect, style.item_hover_bg, style.item_radius, 0.0f);
        paint_item_pointer_reveal(ctx, tl.rect, style.item_radius, !item.disabled);

        if (is_sel) {
            target_pill_w = std::max(16.0f, tl.rect.width() - 24.0f);
            target_pill_x = tl.rect.min_x + (tl.rect.width() - target_pill_w) * 0.5f;
        }

        Color ic = item.disabled ? style.item_disabled_text
                 : (is_sel ? style.icon_selected_color : style.icon_color);
        if (tl.is_settings) {
            Rect ir = Rect::from_xywh(std::round(tl.rect.min_x + (tl.rect.width() - 16.0f) * 0.5f),
                                      std::round(tl.rect.min_y + (tl.rect.height() - 16.0f) * 0.5f), 16.0f, 16.0f);
            NavigationViewStyleResolver::paint_settings_gear(ctx, ir, ic, m_settings_gear_angle, is_prs ? 0.90f : 1.0f);
        } else {
            float tx = tl.rect.min_x + 12.0f;
            if (item.icon != NavigationIcon::None || item.icon_painter) {
                Rect ir = Rect::from_xywh(std::round(tx), std::round(tl.rect.min_y + (tl.rect.height() - 16.0f) * 0.5f), 16.0f, 16.0f);
                if (item.icon_painter) item.icon_painter(ctx, ir, ic);
                else NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, ir, ic);
                tx += 22.0f;
            }
            Color tc = item.disabled ? style.item_disabled_text
                     : (is_sel ? style.item_selected_text : style.item_text);
            Vec2 ts = TextMeasure::measure(item.label, style.font_size);
            ctx.draw_text(item.label, PointF{std::round(tx), std::round(tl.rect.min_y + (tl.rect.height() - ts.y) * 0.5f)},
                          style.font_size, tc);
            if (!item.children.empty()) {
                float cx = tl.rect.max_x - 11.0f;
                float cy = std::round(tl.rect.min_y + tl.rect.height() * 0.5f);
                ctx.draw_line(PointF{cx - 3.0f, cy - 1.5f}, PointF{cx, cy + 1.5f}, ic, 1.15f);
                ctx.draw_line(PointF{cx, cy + 1.5f}, PointF{cx + 3.0f, cy - 1.5f}, ic, 1.15f);
            }
        }
    }

    // Top 模式底部 3px 弹性拉伸水平滑动强调条
    if (target_pill_x >= 0.0f) {
        if (!m_pill_initialized) {
            m_pill_head = m_pill_tail = target_pill_x;
            m_pill_size = target_pill_w;
            m_pill_initialized = true;
        }
        m_pill_target_pos = target_pill_x;
        m_pill_target_size = target_pill_w;
        if (std::abs(m_pill_head - m_pill_target_pos) > 0.25f ||
            std::abs(m_pill_tail - m_pill_target_pos) > 0.25f ||
            std::abs(m_pill_size - m_pill_target_size) > 0.25f) {
            mark_needs_paint();
        }
        float pill_l = std::min(m_pill_head, m_pill_tail);
        float pill_r = std::max(m_pill_head, m_pill_tail) + m_pill_size;
        ctx.draw_rect(Rect::from_xywh(pill_l, 41.0f, std::max(16.0f, pill_r - pill_l), 3.0f),
                      style.accent_pill, 1.5f, 0.0f);
    }
}

} // namespace ktui
