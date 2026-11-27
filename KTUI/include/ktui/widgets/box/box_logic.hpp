#pragma once

#include "box_data.hpp"
#include "box_style.hpp"
#include <ktui/animation/tactile_press.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>

namespace ktui {


// 万能装饰与尺寸修饰渲染盒 (RenderDecoratedBox)
class RenderDecoratedBox : public RenderBox {
public:
    RenderDecoratedBox() noexcept = default;
    ~RenderDecoratedBox() override = default;

    void set_properties(float w, float h, Color bg, Color border, float border_w, float radius, EdgeInsets pad, bool clip = true) {
        bool needs_relayout = (m_width != w || m_height != h || !(m_padding == pad));
        bool needs_repainting = (m_background_color != bg || m_border_color != border ||
                                 m_border_width != border_w || m_corner_radius != radius ||
                                 m_clip_content != clip);

        m_width = w;
        m_height = h;
        m_background_color = bg;
        m_border_color = border;
        m_border_width = border_w;
        m_corner_radius = radius;
        m_padding = pad;
        m_clip_content = clip;

        if (needs_relayout) {
            mark_needs_layout();
        } else if (needs_repainting) {
            mark_needs_paint();
        }
    }

    void set_layout_bounds(float min_w, float max_w, float min_h, float max_h,
                           std::optional<Alignment> alignment = std::nullopt) {
        bool changed = (m_min_width != min_w || m_max_width != max_w ||
                        m_min_height != min_h || m_max_height != max_h ||
                        m_alignment != alignment);
        m_min_width = min_w;
        m_max_width = max_w;
        m_min_height = min_h;
        m_max_height = max_h;
        m_alignment = alignment;
        if (changed) mark_needs_layout();
    }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child_box == child) return;
        if (m_child_box) {
            remove_child(m_child_box.get());
        }
        m_child_box = child;
        if (m_child_box) {
            append_child(m_child_box.get());
        }
        mark_needs_layout();
    }

    void perform_layout() override {
        BoxConstraints effective = m_constraints;
        if (m_min_width >= 0.0f) {
            effective.min_width = std::clamp(m_min_width, effective.min_width, effective.max_width);
        }
        if (m_max_width >= 0.0f) {
            effective.max_width = std::max(effective.min_width, std::min(effective.max_width, m_max_width));
        }
        if (m_min_height >= 0.0f) {
            effective.min_height = std::clamp(m_min_height, effective.min_height, effective.max_height);
        }
        if (m_max_height >= 0.0f) {
            effective.max_height = std::max(effective.min_height, std::min(effective.max_height, m_max_height));
        }

        Vec2 pad_size{m_padding.horizontal(), m_padding.vertical()};
        Vec2 child_size{0.0f, 0.0f};

        if (m_child_box) {
            BoxConstraints child_constraints = effective.deflate(m_padding);
            if (m_alignment.has_value()) {
                child_constraints = child_constraints.loosen();
            }
            if (m_width >= 0.0f) {
                float inner_w = std::max(0.0f, m_width - pad_size.x);
                child_constraints.min_width = std::min(child_constraints.min_width, inner_w);
                child_constraints.max_width = inner_w;
            }
            if (m_height >= 0.0f) {
                float inner_h = std::max(0.0f, m_height - pad_size.y);
                child_constraints.min_height = std::min(child_constraints.min_height, inner_h);
                child_constraints.max_height = inner_h;
            }
            m_child_box->layout(child_constraints, true);
            child_size = m_child_box->size();
        }

        float desired_w = (m_width >= 0.0f) ? m_width : (child_size.x + pad_size.x);
        float desired_h = (m_height >= 0.0f) ? m_height : (child_size.y + pad_size.y);

        m_size = effective.constrain(Vec2{desired_w, desired_h});

        if (m_child_box) {
            if (m_alignment.has_value()) {
                Vec2 inner_sz{
                    std::max(0.0f, m_size.x - pad_size.x),
                    std::max(0.0f, m_size.y - pad_size.y)
                };
                Vec2 aligned = m_alignment->compute_offset(inner_sz, child_size);
                m_child_box->set_offset(Vec2{m_padding.left + aligned.x, m_padding.top + aligned.y});
            } else {
                m_child_box->set_offset(Vec2{m_padding.left, m_padding.top});
            }
        }
    }

    void set_theme_card(bool v) {
        if (m_theme_card != v) { m_theme_card = v; mark_needs_paint(); }
    }

    void set_directional_rim(bool enabled, Vec2 light_dir = Vec2{-0.18f, -0.98f},
                             float intensity = 1.0f, float inner_glow = 0.35f) {
        if (m_directional_rim != enabled || m_rim_light_dir != light_dir ||
            m_rim_intensity != intensity || m_rim_inner_glow != inner_glow) {
            m_directional_rim = enabled;
            m_rim_light_dir = light_dir;
            m_rim_intensity = intensity;
            m_rim_inner_glow = inner_glow;
            mark_needs_paint();
        }
    }

    void set_acrylic_grain(bool enabled, float grain_amount = 0.028f, float grain_scale = 1.0f) {
        if (m_acrylic_grain != enabled || m_grain_amount != grain_amount || m_grain_scale != grain_scale) {
            m_acrylic_grain = enabled;
            m_grain_amount = grain_amount;
            m_grain_scale = grain_scale;
            mark_needs_paint();
        }
    }

    void set_inset_shadow(bool enabled, float blur = 6.0f, float spread = 0.0f,
                          Vec2 offset = Vec2{0.0f, 2.0f}, Color color = Color::transparent()) {
        if (m_inset_shadow != enabled || m_inset_blur != blur || m_inset_spread != spread ||
            m_inset_offset != offset || m_inset_color != color) {
            m_inset_shadow = enabled;
            m_inset_blur = blur;
            m_inset_spread = spread;
            m_inset_offset = offset;
            m_inset_color = color;
            mark_needs_paint();
        }
    }

    void set_tactile_press(bool enabled, float press_scale = 0.968f, float tilt_shift = 3.5f) {
        m_tactile_press.set_tuning(press_scale, tilt_shift);
        if (enabled && !m_tactile_handlers_ready) {
            setup_tactile_handlers();
        }
        if (m_tactile_press.enabled() != enabled) {
            m_tactile_press.set_enabled(enabled);
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool tactile_press_enabled() const noexcept { return m_tactile_press.enabled(); }

    void set_concentric_inner(bool enabled, float min_radius = 2.0f) noexcept {
        const float mr = std::max(0.0f, min_radius);
        if (m_concentric_inner != enabled || std::abs(m_concentric_min_radius - mr) > 0.01f) {
            m_concentric_inner = enabled;
            m_concentric_min_radius = mr;
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool concentric_inner_enabled() const noexcept { return m_concentric_inner; }

    [[nodiscard]] float surface_corner_radius() const noexcept override {
        return resolve_effective_corner_radius();
    }

    [[nodiscard]] float resolve_effective_corner_radius() const noexcept {
        if (!m_concentric_inner) return m_corner_radius;
        Vec2 rel = m_offset;
        const RenderNode* cur = parent();
        while (cur) {
            if (auto* rb = cur->as_render_box()) {
                const float parent_r = rb->surface_corner_radius();
                if (parent_r > 0.0f) {
                    const float right_gap = rb->size().x - (rel.x + m_size.x);
                    const float bot_gap = rb->size().y - (rel.y + m_size.y);
                    const float inset = std::max(0.0f, std::min({rel.x, rel.y, right_gap, bot_gap}));
                    return concentric_radius(parent_r, inset, m_concentric_min_radius);
                }
                rel = rel + rb->offset();
            }
            cur = cur->parent();
        }
        return m_corner_radius;
    }

    void paint(PaintContext& context) override {
        const Theme& theme = ThemeContext::current();
        if (m_tactile_press.update()) {
            mark_needs_paint();
        }
        const bool tactile_active = m_tactile_press.is_active();
        const Rect base_rect = bounds();
        const Rect paint_rect = tactile_active ? m_tactile_press.transform_rect(base_rect) : base_rect;
        const float eff_radius = resolve_effective_corner_radius() * (tactile_active ? m_tactile_press.current_scale() : 1.0f);

        Color bg = m_background_color;
        Color bd = m_border_color;
        if (m_theme_card) {
            const bool is_dark = theme.is_dark;
            bg = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.045f} : Color{1.0f, 1.0f, 1.0f, 0.65f};
            bd = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.07f}  : Color{0.0f, 0.0f, 0.0f, 0.07f};
        }
        if (m_acrylic_grain) {
            Color grain_bg = (bg.a > 0.001f) ? bg : (theme.is_dark ? Color{0.12f, 0.14f, 0.18f, 0.56f} : Color{0.96f, 0.97f, 0.99f, 0.64f});
            context.draw_acrylic_panel(paint_rect, grain_bg, eff_radius, m_grain_amount, m_grain_scale);
        } else if (bg.a > 0.001f) {
            context.draw_rect(paint_rect, bg, eff_radius, 0.0f);
        }
        if (m_inset_shadow) {
            Color scol = (m_inset_color.a > 0.001f)
                ? m_inset_color
                : (theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.45f} : Color{0.0f, 0.02f, 0.08f, 0.16f});
            context.draw_inset_shadow(paint_rect, scol, eff_radius, m_inset_blur, m_inset_spread, m_inset_offset);
        }
        if (m_directional_rim) {
            const float bw = (m_border_width > 0.0f) ? m_border_width : 1.0f;
            const float k = std::clamp(m_rim_intensity, 0.0f, 2.0f);
            if (k > 0.001f) {
                Color top_rim = theme.is_dark
                    ? Color{1.0f, 1.0f, 1.0f, std::clamp(0.32f * k, 0.0f, 1.0f)}
                    : Color{1.0f, 1.0f, 1.0f, std::clamp(0.85f * k, 0.0f, 1.0f)};
                Color bot_rim = theme.is_dark
                    ? Color{0.0f, 0.0f, 0.0f, std::clamp(0.34f * k, 0.0f, 1.0f)}
                    : Color{0.0f, 0.04f, 0.12f, std::clamp(0.14f * k, 0.0f, 1.0f)};
                const Vec2 light_dir = tactile_active ? m_tactile_press.tilted_light_dir() : m_rim_light_dir;
                context.draw_directional_rim(paint_rect, top_rim, bot_rim, eff_radius, bw, light_dir, m_rim_inner_glow);
            }
        } else if (m_border_width > 0.0f && bd.a > 0.001f) {
            context.draw_rect(paint_rect, bd, eff_radius, m_border_width);
        }
        if (m_clip_content) {
            context.push_clip(paint_rect, eff_radius);
        }
        if (tactile_active) {
            const float rs = std::max(1.0f, context.render_scale());
            const float dx = (paint_rect.min_x + paint_rect.max_x) * 0.5f - (base_rect.min_x + base_rect.max_x) * 0.5f;
            const float dy = (paint_rect.min_y + paint_rect.max_y) * 0.5f - (base_rect.min_y + base_rect.max_y) * 0.5f;
            const Vec2 shift{std::round(dx * rs) / rs, std::round(dy * rs) / rs};
            if (std::abs(shift.x) > 0.001f || std::abs(shift.y) > 0.001f) {
                PaintContext::OffsetScope tactile_offset(context, shift);
                RenderBox::paint(context);
            } else {
                RenderBox::paint(context);
            }
        } else {
            RenderBox::paint(context);
        }
        if (m_clip_content) {
            context.pop_clip();
        }
    }

    void set_width(float w) { if (m_width != w) { m_width = w; mark_needs_layout(); } }
    void set_height(float h) { if (m_height != h) { m_height = h; mark_needs_layout(); } }
    void set_background_color(Color bg) { if (m_background_color != bg) { m_background_color = bg; mark_needs_paint(); } }
    void set_border_color(Color c) { if (m_border_color != c) { m_border_color = c; mark_needs_paint(); } }
    void set_border_width(float bw) { if (m_border_width != bw) { m_border_width = bw; mark_needs_paint(); } }
    void set_corner_radius(float r) { if (m_corner_radius != r) { m_corner_radius = r; mark_needs_paint(); } }
    void set_padding(EdgeInsets pad) { if (!(m_padding == pad)) { m_padding = pad; mark_needs_layout(); } }
    void set_clip_content(bool clip) { if (m_clip_content != clip) { m_clip_content = clip; mark_needs_paint(); } }
    void set_alignment(std::optional<Alignment> a) { if (m_alignment != a) { m_alignment = a; mark_needs_layout(); } }

private:
    void setup_tactile_handlers() {
        m_tactile_handlers_ready = true;
        on_pointer_enter = [this](PointerEvent&) {
            if (m_tactile_press.enabled()) {
                m_tactile_press.on_enter();
                mark_needs_paint();
            }
        };
        on_pointer_move = [this](PointerEvent& ev) {
            if (m_tactile_press.enabled() && ev.device_kind != PointerDeviceKind::Touch) {
                m_tactile_press.on_move(ev.local_position, m_size);
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (m_tactile_press.enabled() && ev.button == PointerButton::Primary) {
                m_tactile_press.on_down(ev.local_position, m_size);
                mark_needs_paint();
            }
        };
        on_pointer_up = [this](PointerEvent& ev) {
            if (m_tactile_press.enabled() && ev.button == PointerButton::Primary) {
                m_tactile_press.on_up();
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_tactile_press.enabled()) {
                m_tactile_press.on_exit();
                mark_needs_paint();
            }
        };
    }

    float m_width{-1.0f};
    float m_height{-1.0f};
    float m_min_width{-1.0f};
    float m_max_width{-1.0f};
    float m_min_height{-1.0f};
    float m_max_height{-1.0f};
    std::optional<Alignment> m_alignment{std::nullopt};
    Color m_background_color{Color::transparent()};
    Color m_border_color{Color::transparent()};
    float m_border_width{0.0f};
    float m_corner_radius{0.0f};
    EdgeInsets m_padding{EdgeInsets::zero()};
    bool m_clip_content{true};
    bool m_theme_card{false};
    bool m_directional_rim{false};
    Vec2 m_rim_light_dir{-0.18f, -0.98f};
    float m_rim_intensity{1.0f};
    float m_rim_inner_glow{0.35f};
    bool m_acrylic_grain{false};
    float m_grain_amount{0.028f};
    float m_grain_scale{1.0f};
    bool m_inset_shadow{false};
    float m_inset_blur{6.0f};
    float m_inset_spread{0.0f};
    Vec2 m_inset_offset{0.0f, 2.0f};
    Color m_inset_color{Color::transparent()};
    TactilePressPhysics m_tactile_press;
    bool m_tactile_handlers_ready{false};
    bool m_concentric_inner{false};
    float m_concentric_min_radius{2.0f};
    std::shared_ptr<RenderBox> m_child_box{nullptr};
};

class BoxWidget;

// C++20 指定初始化器声明式聚合体 (Box)
struct Box : BindableMixin<Box, RenderDecoratedBox> {
    float width{-1.0f};
    float height{-1.0f};
    Color background_color{Color::transparent()};
    Color border_color{Color::transparent()};
    float border_width{0.0f};
    float corner_radius{0.0f};
    EdgeInsets padding{EdgeInsets::zero()};
    bool theme_card{false};
    bool directional_rim{false};
    Vec2 rim_light_dir{-0.18f, -0.98f};
    float rim_intensity{1.0f};
    float rim_inner_glow{0.35f};
    bool acrylic_grain{false};
    float grain_amount{0.028f};
    float grain_scale{1.0f};
    bool inset_shadow{false};
    float inset_shadow_blur{6.0f};
    float inset_shadow_spread{0.0f};
    Vec2 inset_shadow_offset{0.0f, 2.0f};
    Color inset_shadow_color{Color::transparent()};
    bool tactile_press{false};
    float tactile_press_scale{0.968f};
    float tactile_tilt_shift{3.5f};
    bool concentric_inner{false};
    float concentric_min_radius{2.0f};
    WidgetPtr child{nullptr};
    bool clip_content{true};
    float min_width{-1.0f};
    float max_width{-1.0f};
    float min_height{-1.0f};
    float max_height{-1.0f};
    std::optional<Alignment> alignment{std::nullopt};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class BoxWidget : public Widget {
public:
    explicit BoxWidget(const Box& desc) : m_desc(desc) {}
    explicit BoxWidget(Box&& desc) noexcept : m_desc(std::move(desc)) {}
    ~BoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderDecoratedBox>();
        m_child_render_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_render_box) {
            m_desc.child->set_render_node(m_child_render_box);
        }
        node->set_child_box(m_child_render_box);
        apply_props(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto dec_box = std::static_pointer_cast<RenderDecoratedBox>(node)) {
            if (m_desc.child) {
                if (!m_child_render_box) {
                    m_child_render_box = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(m_child_render_box);
                    dec_box->set_child_box(m_child_render_box);
                } else {
                    m_desc.child->update_render_node(m_child_render_box);
                }
            } else if (m_child_render_box) {
                m_child_render_box = nullptr;
                dec_box->set_child_box(nullptr);
            }

            apply_props(dec_box);
        }
    }

    [[nodiscard]] const Box& desc() const noexcept { return m_desc; }

private:
    void apply_props(const std::shared_ptr<RenderDecoratedBox>& dec_box) const {
        dec_box->set_properties(
            m_desc.width, m_desc.height, m_desc.background_color,
            m_desc.border_color, m_desc.border_width,
            m_desc.corner_radius, m_desc.padding, m_desc.clip_content);
        dec_box->set_layout_bounds(
            m_desc.min_width, m_desc.max_width,
            m_desc.min_height, m_desc.max_height,
            m_desc.alignment);
        dec_box->set_theme_card(m_desc.theme_card);
        dec_box->set_directional_rim(m_desc.directional_rim, m_desc.rim_light_dir,
                                     m_desc.rim_intensity, m_desc.rim_inner_glow);
        dec_box->set_acrylic_grain(m_desc.acrylic_grain, m_desc.grain_amount, m_desc.grain_scale);
        dec_box->set_inset_shadow(m_desc.inset_shadow, m_desc.inset_shadow_blur,
                                    m_desc.inset_shadow_spread, m_desc.inset_shadow_offset,
                                    m_desc.inset_shadow_color);
        dec_box->set_tactile_press(m_desc.tactile_press, m_desc.tactile_press_scale, m_desc.tactile_tilt_shift);
        dec_box->set_concentric_inner(m_desc.concentric_inner, m_desc.concentric_min_radius);
    }

    Box m_desc;
    std::shared_ptr<RenderBox> m_child_render_box{nullptr};
};

KTUI_NOINLINE inline Box::operator WidgetPtr() const& {
    return make_widget<BoxWidget>(*this);
}
KTUI_NOINLINE inline Box::operator WidgetPtr() && {
    return make_widget<BoxWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
