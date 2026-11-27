#pragma once

namespace ktui {

// WinUI 3 Compact / Top 模式子菜单浮层 (RenderNavigationSubmenuFlyout)
// 挂载于 OverlayManager 顶层，折叠状态下点击含子项的父导航项直接弹出选择子项，无需强行展开侧边栏
class RenderNavigationSubmenuFlyout : public RenderBox {
public:
    RenderNavigationSubmenuFlyout(std::vector<NavigationViewItem> children,
                                  std::string selected_id,
                                  Fn<void(const std::string&)> on_select)
        : m_children(std::move(children)),
          m_selected_id(std::move(selected_id)),
          m_on_select(std::move(on_select)) {
        cursor_shape = CursorShape::Arrow;
        m_open_start = std::chrono::steady_clock::now();
        setup_behavior();
    }

    ~RenderNavigationSubmenuFlyout() override = default;

    [[nodiscard]] Vec2 compute_ideal_size() const {
        float max_w = 160.0f;
        for (const auto& item : m_children) {
            Vec2 ts = TextMeasure::measure(item.label, 13.0f);
            float icon_w = (item.icon != NavigationIcon::None || item.icon_painter) ? 26.0f : 0.0f;
            max_w = std::max(max_w, ts.x + icon_w + 38.0f);
        }
        float h = static_cast<float>(std::max<size_t>(1, m_children.size())) * kItemH + 8.0f;
        return Vec2{std::ceil(std::min(max_w, 280.0f)), h};
    }

    void perform_layout() override {
        Vec2 ideal = compute_ideal_size();
        float w = std::isfinite(m_constraints.max_width) ? std::min(ideal.x, m_constraints.max_width) : ideal.x;
        float h = std::isfinite(m_constraints.max_height) ? std::min(ideal.y, m_constraints.max_height) : ideal.y;
        set_size(Vec2{w, h});
    }

    void paint(PaintContext& ctx) override {
        if (m_open_progress < 1.0f) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - m_open_start).count();
            m_open_progress = std::clamp(elapsed / 0.13f, 0.0f, 1.0f);
            if (m_open_progress < 1.0f) mark_needs_paint();
        }

        const Theme& theme = ThemeContext::current();
        const auto style = NavigationViewStyleResolver::resolve(theme);

        float ease = 1.0f - std::pow(1.0f - m_open_progress, 3.0f);
        float anim_h = std::max(16.0f, size().y * (0.76f + 0.24f * ease));
        float alpha_mul = std::clamp(ease * 1.15f, 0.0f, 1.0f);

        Rect panel = Rect::from_xywh(0.0f, 0.0f, size().x, anim_h);

        // 1. WinUI 3 Flyout 32dp 双层真实高斯软阴影
        ctx.draw_elevation_shadow(panel, 8.0f, theme.is_dark, 32.0f, alpha_mul);

        // 2. 8px 圆角浮层底板与 1px 描边
        Color bg = style.pane_overlay_bg;
        bg.a *= alpha_mul;
        Color bd = style.pane_overlay_border;
        bd.a *= alpha_mul;
        ctx.draw_rect(panel, bg, 8.0f, 0.0f);
        ctx.draw_rect(panel, bd, 8.0f, 1.0f);

        // 3. 子项列表
        ctx.push_clip(Rect::from_xywh(4.0f, 4.0f, std::max(0.0f, size().x - 8.0f), std::max(0.0f, anim_h - 8.0f)), 4.0f);
        for (int i = 0; i < static_cast<int>(m_children.size()); ++i) {
            const auto& item = m_children[static_cast<size_t>(i)];
            float iy = 4.0f + static_cast<float>(i) * kItemH;
            Rect r = Rect::from_xywh(4.0f, iy + 1.0f, std::max(0.0f, size().x - 8.0f), kItemH - 2.0f);

            bool is_sel = (item.effective_id() == m_selected_id);
            bool is_hov = (i == m_hovered_index && !item.disabled);
            bool is_prs = (i == m_pressed_index && !item.disabled);

            if (is_prs) {
                Color c = style.item_pressed_bg; c.a *= alpha_mul;
                ctx.draw_rect(r, c, 4.0f, 0.0f);
            } else if (is_hov) {
                Color c = style.item_hover_bg; c.a *= alpha_mul;
                ctx.draw_rect(r, c, 4.0f, 0.0f);
            } else if (is_sel) {
                Color c = style.item_selected_bg; c.a *= alpha_mul;
                ctx.draw_rect(r, c, 4.0f, 0.0f);
            }

            if (is_sel) {
                Color pill = style.accent_pill; pill.a *= alpha_mul;
                ctx.draw_rect(Rect::from_xywh(5.0f, iy + (kItemH - 16.0f) * 0.5f, 3.0f, 16.0f), pill, 1.5f, 0.0f);
            }

            float tx = r.min_x + 12.0f;
            Color ic = item.disabled ? style.item_disabled_text : (is_sel ? style.icon_selected_color : style.icon_color);
            ic.a *= alpha_mul;
            if (item.icon != NavigationIcon::None || item.icon_painter) {
                Rect ir = Rect::from_xywh(tx, std::round(iy + (kItemH - 16.0f) * 0.5f), 16.0f, 16.0f);
                if (item.icon_painter) item.icon_painter(ctx, ir, ic);
                else NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, ir, ic);
                tx += 26.0f;
            }

            Color tc = item.disabled ? style.item_disabled_text : (is_sel ? style.item_selected_text : style.item_text);
            tc.a *= alpha_mul;
            Vec2 ts = TextMeasure::measure(item.label, 13.0f);
            ctx.draw_text(item.label, PointF{std::round(tx), std::round(iy + (kItemH - ts.y) * 0.5f)}, 13.0f, tc);
        }
        ctx.pop_clip();
    }

private:
    static constexpr float kItemH = 34.0f;

    int hit_index(float ly) const noexcept {
        float y = ly - 4.0f;
        if (y < 0.0f) return -1;
        int idx = static_cast<int>(std::floor(y / kItemH));
        if (idx < 0 || idx >= static_cast<int>(m_children.size())) return -1;
        return idx;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int idx = hit_index(e.local_position.y);
            if (idx != m_hovered_index) {
                m_hovered_index = idx;
                mark_needs_paint();
            }
            e.stop_propagation();
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_index != -1 || m_pressed_index != -1) {
                m_hovered_index = -1;
                m_pressed_index = -1;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            int idx = hit_index(e.local_position.y);
            if (idx >= 0 && !m_children[static_cast<size_t>(idx)].disabled) {
                m_pressed_index = idx;
                mark_needs_paint();
            }
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& e) {
            int idx = hit_index(e.local_position.y);
            int pressed = m_pressed_index;
            m_pressed_index = -1;
            if (idx >= 0 && idx == pressed && !m_children[static_cast<size_t>(idx)].disabled) {
                if (m_on_select) m_on_select(m_children[static_cast<size_t>(idx)].effective_id());
            }
            mark_needs_paint();
            e.stop_propagation();
        };
    }

    std::vector<NavigationViewItem> m_children;
    std::string m_selected_id;
    Fn<void(const std::string&)> m_on_select;
    int m_hovered_index{-1};
    int m_pressed_index{-1};
    float m_open_progress{0.0f};
    std::chrono::steady_clock::time_point m_open_start;
};

// =============================================================================
// WinUI 3 NavigationView 核心渲染与交互盒
// =============================================================================

} // namespace ktui
