#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/layout/box_constraints.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <memory>

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
        Vec2 pad_size{m_padding.horizontal(), m_padding.vertical()};
        Vec2 child_size{0.0f, 0.0f};

        if (m_child_box) {
            BoxConstraints child_constraints = m_constraints.deflate(m_padding);
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
            m_child_box->set_offset(Vec2{m_padding.left, m_padding.top});
        }

        float desired_w = (m_width >= 0.0f) ? m_width : (child_size.x + pad_size.x);
        float desired_h = (m_height >= 0.0f) ? m_height : (child_size.y + pad_size.y);

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
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
        // 1. 绘制自身实心背景
        if (bg.a > 0.001f) {
            context.draw_rect(bounds(), bg, m_corner_radius, 0.0f);
        }
        // 2. 绘制 1px 细边框
        if (m_border_width > 0.0f && bd.a > 0.001f) {
            context.draw_rect(bounds(), bd, m_corner_radius, m_border_width);
        }
        // 3. 递归绘制子节点 (默认施加硬件级圆角矩形裁剪，严防内容溢出容器)
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

private:
    float m_width{-1.0f};
    float m_height{-1.0f};
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
// 零开销原生字段 + 链式 .bind(...) 按需绑定任意属性
struct Box : BindableMixin<Box, RenderDecoratedBox> {
    float width{-1.0f};
    float height{-1.0f};
    Color background_color{Color::transparent()};
    Color border_color{Color::transparent()};
    float border_width{0.0f};
    float corner_radius{0.0f}; // 桌面级克制圆角 (0px 直角或 2~4px 微圆角)
    EdgeInsets padding{EdgeInsets::zero()};
    bool theme_card{false};              // 自动跟随 ThemeContext::current() 渲染卡片底色与描边 (支持涟漪主题无缝过渡)
    WidgetPtr child{nullptr};
    bool clip_content{true};   // 是否对子节点施加硬件视口裁剪

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

// BoxWidget 多态包装实现
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
        dec_box->set_theme_card(m_desc.theme_card);
    }

    Box m_desc;
    std::shared_ptr<RenderBox> m_child_render_box{nullptr};
};

inline Box::operator WidgetPtr() const& {
    return make_widget<BoxWidget>(*this);
}
inline Box::operator WidgetPtr() && {
    return make_widget<BoxWidget>(std::move(*this));
}

} // namespace ktui
