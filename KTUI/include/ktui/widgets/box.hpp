#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/layout/box_constraints.hpp>
#include <ktui/layout/flex_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <memory>
#include <optional>

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

    void paint(PaintContext& context) override {
        Color bg = m_background_color;
        Color bd = m_border_color;
        if (m_theme_card) {
            const bool is_dark = ThemeContext::current().is_dark;
            bg = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.045f} : Color{1.0f, 1.0f, 1.0f, 0.65f};
            bd = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.07f}  : Color{0.0f, 0.0f, 0.0f, 0.07f};
        }
        if (bg.a > 0.001f) {
            context.draw_rect(bounds(), bg, m_corner_radius, 0.0f);
        }
        if (m_border_width > 0.0f && bd.a > 0.001f) {
            context.draw_rect(bounds(), bd, m_corner_radius, m_border_width);
        }
        if (m_clip_content) {
            context.push_clip(bounds(), m_corner_radius);
        }
        RenderBox::paint(context);
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
