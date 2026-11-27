#pragma once

#include "scroll_linked_data.hpp"
#include "scroll_linked_style.hpp"
#include "../scroll/scrollbar.hpp"
#include "../scroll/smooth_scroll_physics.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <cmath>

namespace ktui {


// ============================================================================
// 滚动吸顶渐显分隔线与阻尼视差容器 (Sticky Header Hairline & Parallax ScrollView)
// - 顶部吸顶标题栏 (sticky_header)：offset == 0 时与下方内容无缝融合；
//   开始向下滚动后，在 hairline_fade_range 区间内平滑淡入 1px 蚀刻分隔线与柔和接触阴影
// - 可选视差横幅 (parallax_banner)：以 parallax_ratio (默认 0.42x) 随滚动平滑滞后位移
// ============================================================================
class RenderStickyParallaxScrollView final : public RenderBox {
public:
    RenderStickyParallaxScrollView() noexcept {
        setup_behavior();
    }

    ~RenderStickyParallaxScrollView() override = default;

    void set_properties(float width,
                        float height,
                        float parallax_height,
                        float parallax_ratio,
                        float hairline_fade_range,
                        bool edge_feather,
                        float feather_size) {
        bool relayout = (std::abs(m_width - width) > 0.01f ||
                         std::abs(m_height - height) > 0.01f ||
                         std::abs(m_parallax_height - parallax_height) > 0.01f);
        m_width = width;
        m_height = height;
        m_parallax_height = std::max(0.0f, parallax_height);
        m_parallax_ratio = std::clamp(parallax_ratio, 0.0f, 1.0f);
        m_hairline_fade_range = std::max(1.0f, hairline_fade_range);
        m_edge_feather = edge_feather;
        m_feather_size = std::max(0.0f, feather_size);
        if (relayout) mark_needs_layout();
        else mark_needs_paint();
    }

    void set_parallax_ratio(float r) noexcept {
        m_parallax_ratio = std::clamp(r, 0.0f, 1.0f);
        mark_needs_paint();
    }

    void set_hairline_fade_range(float r) noexcept {
        m_hairline_fade_range = std::max(1.0f, r);
        mark_needs_paint();
    }

    void set_children(std::shared_ptr<RenderBox> sticky_header,
                      std::shared_ptr<RenderBox> parallax_banner,
                      std::shared_ptr<RenderBox> body) {
        if (m_sticky == sticky_header && m_banner == parallax_banner && m_body == body) return;
        if (m_sticky) remove_child(m_sticky.get());
        if (m_banner) remove_child(m_banner.get());
        if (m_body)   remove_child(m_body.get());
        m_sticky = std::move(sticky_header);
        m_banner = std::move(parallax_banner);
        m_body   = std::move(body);
        if (m_banner) append_child(m_banner.get());
        if (m_body)   append_child(m_body.get());
        if (m_sticky) append_child(m_sticky.get());
        mark_needs_layout();
    }

    [[nodiscard]] const std::shared_ptr<RenderBox>& sticky_box() const noexcept { return m_sticky; }
    [[nodiscard]] const std::shared_ptr<RenderBox>& banner_box() const noexcept { return m_banner; }
    [[nodiscard]] const std::shared_ptr<RenderBox>& body_box() const noexcept { return m_body; }

    [[nodiscard]] float scroll_offset() const noexcept { return m_physics.offset(); }
    [[nodiscard]] float header_hairline_alpha() const noexcept {
        return std::clamp(m_physics.offset() / m_hairline_fade_range, 0.0f, 1.0f);
    }

    void scroll_to(float offset) {
        m_physics.scroll_to(offset);
        m_scrollbar.notify_activity();
        mark_needs_paint();
    }

    void jump_to(float offset) {
        m_physics.jump_to(offset);
        sync_child_offsets();
        mark_needs_paint();
    }

    void perform_layout() override {
        const float w = (m_width > 0.0f)
            ? m_constraints.constrain_width(m_width)
            : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 360.0f);
        const float h = (m_height > 0.0f)
            ? m_constraints.constrain_height(m_height)
            : (std::isfinite(m_constraints.max_height) ? m_constraints.max_height : 320.0f);

        m_sticky_h = 0.0f;
        if (m_sticky) {
            BoxConstraints hc;
            hc.min_width = w; hc.max_width = w;
            hc.min_height = 0.0f; hc.max_height = h;
            m_sticky->layout(hc, true);
            m_sticky->set_offset(Vec2{0.0f, 0.0f});
            m_sticky_h = m_sticky->size().y;
        }

        m_banner_h = 0.0f;
        if (m_banner) {
            const float bh = (m_parallax_height > 0.0f) ? m_parallax_height : 140.0f;
            BoxConstraints bc = BoxConstraints::tight(Vec2{w, bh});
            m_banner->layout(bc, true);
            m_banner_h = m_banner->size().y;
        }

        float body_h = 0.0f;
        if (m_body) {
            BoxConstraints cc;
            cc.min_width = w; cc.max_width = w;
            cc.min_height = 0.0f; cc.max_height = std::numeric_limits<float>::infinity();
            m_body->layout(cc, true);
            body_h = m_body->size().y;
        }

        m_content_height = m_banner_h + body_h;
        set_size(Vec2{w, h});
        const float vp_h = std::max(1.0f, h - m_sticky_h);
        m_physics.set_rubber_band_enabled(true);
        m_physics.set_viewport_extent(vp_h);
        m_physics.set_max_scroll(std::max(0.0f, m_content_height - vp_h));
        sync_child_offsets();
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (local_position.x < 0.0f || local_position.y < 0.0f ||
            local_position.x > size().x || local_position.y > size().y) {
            return false;
        }
        if (m_sticky && local_position.y <= m_sticky_h) {
            m_sticky->hit_test(result, local_position);
            result.add(this, local_position);
            return true;
        }
        const Rect scroll_vp = Rect::from_xywh(0.0f, m_sticky_h, size().x, std::max(0.0f, size().y - m_sticky_h));
        auto m = m_scrollbar.compute_metrics(scroll_vp, m_content_height, m_physics.offset());
        if (m_scrollbar.hit_test(local_position, m) != WinUIScrollBar::HitPart::None) {
            result.add(this, local_position);
            return true;
        }
        if (m_body && m_body->hit_test(result, local_position - m_body->offset())) {
            result.add(this, local_position);
            return true;
        }
        if (m_banner && m_banner->hit_test(result, local_position - m_banner->offset())) {
            result.add(this, local_position);
            return true;
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        bool anim_s = m_physics.update();
        bool anim_b = m_scrollbar.update_animations(m_viewport_hovered);
        if (anim_s || anim_b) {
            mark_needs_paint();
        }
        sync_child_offsets();

        const Rect vp = Rect::from_xywh(0.0f, m_sticky_h, size().x, std::max(0.0f, size().y - m_sticky_h));
        float top_f = 0.0f;
        float bot_f = 0.0f;
        if (m_edge_feather && m_feather_size > 0.5f && vp.height() > 8.0f) {
            const float cap = std::min(m_feather_size, vp.height() * 0.30f);
            top_f = std::clamp(m_physics.offset(), 0.0f, cap);
            bot_f = std::clamp(std::max(0.0f, m_physics.max_scroll() - m_physics.offset()), 0.0f, cap);
        }

        ctx.push_feathered_clip(vp, 0.0f, top_f, bot_f);
        if (m_banner) {
            PaintContext::OffsetScope bs(ctx, m_banner->offset());
            m_banner->paint(ctx);
        }
        if (m_body) {
            PaintContext::OffsetScope cs(ctx, m_body->offset());
            m_body->paint(ctx);
        }
        ctx.pop_clip();

        if (m_sticky) {
            PaintContext::OffsetScope hs(ctx, m_sticky->offset());
            m_sticky->paint(ctx);
        }

        // Scroll-Linked Header Hairline & Contact Shadow
        const float t = header_hairline_alpha();
        if (t > 0.005f) {
            const Theme& theme = ThemeContext::current();
            const Color hairline = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, 0.16f * t}
                : Color{0.0f, 0.0f, 0.0f, 0.12f * t};
            const Color sheen = theme.is_dark
                ? Color{0.0f, 0.0f, 0.0f, 0.28f * t}
                : Color{0.0f, 0.0f, 0.0f, 0.08f * t};
            ctx.draw_rect(Rect::from_xywh(0.0f, m_sticky_h, size().x, 1.0f), hairline, 0.0f, 0.0f);
            ctx.draw_linear_gradient(
                Rect::from_xywh(0.0f, m_sticky_h + 1.0f, size().x, 10.0f),
                sheen, Color{0.0f, 0.0f, 0.0f, 0.0f},
                PointF{0.0f, m_sticky_h + 1.0f},
                PointF{0.0f, m_sticky_h + 11.0f},
                0.0f, 0.0f, false);
        }

        ctx.push_clip(vp, 0.0f);
        m_scrollbar.paint(ctx, vp, m_content_height, m_physics.offset(), ThemeContext::current());
        ctx.pop_clip();
    }

private:
    void sync_child_offsets() noexcept {
        const float off = m_physics.offset();
        if (m_banner) {
            const float parallax_y = (off >= 0.0f)
                ? (-off * m_parallax_ratio)
                : (-off * 0.20f);
            m_banner->set_offset(Vec2{0.0f, m_sticky_h + parallax_y});
        }
        if (m_body) {
            m_body->set_offset(Vec2{0.0f, m_sticky_h + m_banner_h - off});
        }
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            m_viewport_hovered = true;
            m_scrollbar.notify_activity();
            mark_needs_paint();
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };
        on_pointer_move = [this](PointerEvent& e) {
            const Rect vp = Rect::from_xywh(0.0f, m_sticky_h, size().x, std::max(0.0f, size().y - m_sticky_h));
            if (e.type == PlatformEventType::PointerScroll) {
                m_physics.add_wheel_delta(e.delta.y);
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            bool consumed = false;
            if (m_scrollbar.handle_pointer_move(e.local_position, vp, m_content_height, m_physics, consumed)) {
                mark_needs_paint();
            }
            if (consumed) e.stop_propagation();
        };
        on_pointer_down = [this](PointerEvent& e) {
            const Rect vp = Rect::from_xywh(0.0f, m_sticky_h, size().x, std::max(0.0f, size().y - m_sticky_h));
            if (m_scrollbar.handle_pointer_down(e.local_position, vp, m_content_height, m_physics)) {
                mark_needs_paint();
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (m_scrollbar.handle_pointer_up()) {
                mark_needs_paint();
                e.stop_propagation();
            }
        };
    }

    float m_width{0.0f};
    float m_height{320.0f};
    float m_parallax_height{140.0f};
    float m_parallax_ratio{0.42f};
    float m_hairline_fade_range{18.0f};
    bool m_edge_feather{true};
    float m_feather_size{20.0f};
    float m_sticky_h{0.0f};
    float m_banner_h{0.0f};
    float m_content_height{0.0f};
    bool m_viewport_hovered{false};
    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;
    std::shared_ptr<RenderBox> m_sticky;
    std::shared_ptr<RenderBox> m_banner;
    std::shared_ptr<RenderBox> m_body;
};

struct StickyParallaxScrollView : BindableMixin<StickyParallaxScrollView, RenderStickyParallaxScrollView> {
    float width{0.0f};
    float height{320.0f};
    float parallax_height{140.0f};
    float parallax_ratio{0.42f};
    float hairline_fade_range{18.0f};
    bool edge_feather{true};
    float feather_size{20.0f};
    WidgetPtr sticky_header{nullptr};
    WidgetPtr parallax_banner{nullptr};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class StickyParallaxScrollViewWidget final : public Widget {
public:
    explicit StickyParallaxScrollViewWidget(const StickyParallaxScrollView& d) : m_desc(d) {}
    explicit StickyParallaxScrollViewWidget(StickyParallaxScrollView&& d) noexcept : m_desc(std::move(d)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderStickyParallaxScrollView>();
        auto sh = m_desc.sticky_header ? m_desc.sticky_header->create_render_node() : nullptr;
        auto pb = m_desc.parallax_banner ? m_desc.parallax_banner->create_render_node() : nullptr;
        auto bd = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.sticky_header && sh) m_desc.sticky_header->set_render_node(sh);
        if (m_desc.parallax_banner && pb) m_desc.parallax_banner->set_render_node(pb);
        if (m_desc.child && bd) m_desc.child->set_render_node(bd);
        n->set_children(std::move(sh), std::move(pb), std::move(bd));
        apply_props(n);
        return n;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderStickyParallaxScrollView>(node)) {
            apply_props(n);
        }
    }

private:
    void apply_props(const std::shared_ptr<RenderStickyParallaxScrollView>& n) const {
        n->set_properties(m_desc.width, m_desc.height, m_desc.parallax_height,
                          m_desc.parallax_ratio, m_desc.hairline_fade_range,
                          m_desc.edge_feather, m_desc.feather_size);
    }

    StickyParallaxScrollView m_desc;
};

inline StickyParallaxScrollView::operator WidgetPtr() const& { return make_widget<StickyParallaxScrollViewWidget>(*this); }
inline StickyParallaxScrollView::operator WidgetPtr() && { return make_widget<StickyParallaxScrollViewWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
