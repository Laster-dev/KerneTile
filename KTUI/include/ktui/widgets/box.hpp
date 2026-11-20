#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/layout/box_constraints.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <memory>

namespace ktui {

// 万能装饰与尺寸修饰渲染盒 (RenderDecoratedBox)
class RenderDecoratedBox : public RenderBox {
public:
    RenderDecoratedBox() noexcept = default;
    ~RenderDecoratedBox() override = default;

    void set_properties(float w, float h, Color bg, Color border, float border_w, float radius, EdgeInsets pad) {
        bool needs_relayout = (m_width != w || m_height != h || !(m_padding == pad));
        bool needs_repainting = (m_background_color != bg || m_border_color != border ||
                                 m_border_width != border_w || m_corner_radius != radius);

        m_width = w;
        m_height = h;
        m_background_color = bg;
        m_border_color = border;
        m_border_width = border_w;
        m_corner_radius = radius;
        m_padding = pad;

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
            m_child_box->layout(child_constraints, true);
            child_size = m_child_box->size();
            m_child_box->set_offset(Vec2{m_padding.left, m_padding.top});
        }

        float desired_w = (m_width >= 0.0f) ? m_width : (child_size.x + pad_size.x);
        float desired_h = (m_height >= 0.0f) ? m_height : (child_size.y + pad_size.y);

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
    }

    void paint(PaintContext& context) override {
        // 绘制自身背景与边框
        if (m_background_color.a > 0.001f || m_border_width > 0.0f) {
            context.draw_rect(bounds(), m_background_color, m_corner_radius, m_border_width);
        }
        // 递归绘制子节点
        RenderBox::paint(context);
    }

private:
    float m_width{-1.0f};
    float m_height{-1.0f};
    Color m_background_color{Color::transparent()};
    Color m_border_color{Color::transparent()};
    float m_border_width{0.0f};
    float m_corner_radius{0.0f};
    EdgeInsets m_padding{EdgeInsets::zero()};
    std::shared_ptr<RenderBox> m_child_box{nullptr};
};

class BoxWidget;

// C++20 指定初始化器声明式聚合体 (Box)
// 桌面专业标准：默认微圆角 0.0f~4.0f，绝不大圆角
struct Box {
    float width{-1.0f};
    float height{-1.0f};
    Color background_color{Color::transparent()};
    Color border_color{Color::transparent()};
    float border_width{0.0f};
    float corner_radius{0.0f}; // 桌面级克制圆角 (0px 直角或 2~4px 微圆角)
    EdgeInsets padding{EdgeInsets::zero()};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const;
};

// BoxWidget 多态包装实现
class BoxWidget : public Widget {
public:
    explicit BoxWidget(const Box& desc) : m_desc(desc) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderDecoratedBox>();
        m_child_render_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_render_box) {
            m_desc.child->set_render_node(m_child_render_box);
        }
        node->set_child_box(m_child_render_box);
        node->set_properties(m_desc.width, m_desc.height, m_desc.background_color,
                             m_desc.border_color, m_desc.border_width,
                             m_desc.corner_radius, m_desc.padding);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto dec_box = std::dynamic_pointer_cast<RenderDecoratedBox>(node)) {
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

            dec_box->set_properties(m_desc.width, m_desc.height, m_desc.background_color,
                                 m_desc.border_color, m_desc.border_width,
                                 m_desc.corner_radius, m_desc.padding);
        }
    }

    [[nodiscard]] const Box& desc() const noexcept { return m_desc; }

private:
    Box m_desc;
    std::shared_ptr<RenderBox> m_child_render_box{nullptr};
};

inline Box::operator WidgetPtr() const {
    return std::make_shared<BoxWidget>(*this);
}

} // namespace ktui
