#pragma once

#include "widget.hpp"
#include <ktui/layout/flex_types.hpp>
#include <ktui/binding/binding.hpp>
#include <vector>
#include <memory>
#include <algorithm>

namespace ktui {

// 层叠容器渲染盒 (RenderZStack)
// 支持九宫格对齐 (Alignment)、Padding 与 Loose/Expand 约束模式
class RenderZStack : public RenderBox {
public:
    RenderZStack(Alignment alignment = Alignment::top_left(),
                 StackFit fit = StackFit::Loose,
                 EdgeInsets padding = EdgeInsets::zero()) noexcept
        : m_alignment(alignment), m_fit(fit), m_padding(padding) {}
    ~RenderZStack() override = default;

    void set_alignment(Alignment a) noexcept {
        if (!(m_alignment == a)) {
            m_alignment = a;
            mark_needs_layout();
        }
    }
    [[nodiscard]] Alignment alignment() const noexcept { return m_alignment; }

    void set_fit(StackFit f) noexcept {
        if (m_fit != f) {
            m_fit = f;
            mark_needs_layout();
        }
    }
    [[nodiscard]] StackFit fit() const noexcept { return m_fit; }

    void set_padding(EdgeInsets p) noexcept {
        if (!(m_padding == p)) {
            m_padding = p;
            mark_needs_layout();
        }
    }
    [[nodiscard]] EdgeInsets padding() const noexcept { return m_padding; }

    void set_child_boxes(std::vector<std::shared_ptr<RenderBox>> boxes) {
        while (first_child()) {
            remove_child(first_child());
        }
        m_child_boxes = std::move(boxes);
        for (auto& child : m_child_boxes) {
            if (child) {
                append_child(child.get());
            }
        }
        mark_needs_layout();
    }

    void perform_layout() override {
        BoxConstraints inner = m_constraints.deflate(m_padding);
        BoxConstraints child_constraints = inner.loosen();
        if (m_fit == StackFit::Expand && inner.has_bounded_width() && inner.has_bounded_height()) {
            child_constraints = BoxConstraints::tight(Vec2{inner.max_width, inner.max_height});
        }

        float max_w = 0.0f;
        float max_h = 0.0f;

        for (auto& child : m_child_boxes) {
            if (child) {
                child->layout(child_constraints, true);
                max_w = std::max(max_w, child->size().x);
                max_h = std::max(max_h, child->size().y);
            }
        }

        Vec2 pad_sz{m_padding.horizontal(), m_padding.vertical()};
        m_size = m_constraints.constrain(Vec2{max_w + pad_sz.x, max_h + pad_sz.y});
        Vec2 inner_size{
            std::max(0.0f, m_size.x - pad_sz.x),
            std::max(0.0f, m_size.y - pad_sz.y)
        };

        for (auto& child : m_child_boxes) {
            if (child) {
                Vec2 aligned = m_alignment.compute_offset(inner_size, child->size());
                child->set_offset(Vec2{m_padding.left + aligned.x, m_padding.top + aligned.y});
            }
        }
    }

private:
    Alignment m_alignment{Alignment::top_left()};
    StackFit m_fit{StackFit::Loose};
    EdgeInsets m_padding{EdgeInsets::zero()};
    std::vector<std::shared_ptr<RenderBox>> m_child_boxes;
};

class ZStackWidget;

// C++20 指定初始化器层叠容器 (ZStack)
struct ZStack : BindableMixin<ZStack, RenderZStack> {
    Alignment alignment{Alignment::top_left()};
    StackFit fit{StackFit::Loose};
    EdgeInsets padding{EdgeInsets::zero()};
    std::vector<WidgetPtr> content{};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

// ZStackWidget 包装实现
class ZStackWidget : public Widget {
public:
    explicit ZStackWidget(const ZStack& desc) : m_desc(desc) {}
    explicit ZStackWidget(ZStack&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderZStack>(m_desc.alignment, m_desc.fit, m_desc.padding);
        m_child_render_boxes.clear();
        m_child_render_boxes.reserve(m_desc.content.size());

        for (auto& w : m_desc.content) {
            if (w) {
                auto child_box = w->create_render_node();
                w->set_render_node(child_box);
                m_child_render_boxes.push_back(child_box);
            }
        }
        node->set_child_boxes(m_child_render_boxes);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto z_box = std::static_pointer_cast<RenderZStack>(node)) {
            z_box->set_alignment(m_desc.alignment);
            z_box->set_fit(m_desc.fit);
            z_box->set_padding(m_desc.padding);
            m_child_render_boxes.clear();
            m_child_render_boxes.reserve(m_desc.content.size());
            for (auto& w : m_desc.content) {
                if (w) {
                    auto existing = w->render_node();
                    if (!existing) {
                        existing = w->create_render_node();
                        w->set_render_node(existing);
                    } else {
                        w->update_render_node(existing);
                    }
                    m_child_render_boxes.push_back(existing);
                }
            }
            z_box->set_child_boxes(m_child_render_boxes);
        }
    }

    [[nodiscard]] const ZStack& desc() const noexcept { return m_desc; }

private:
    ZStack m_desc;
    std::vector<std::shared_ptr<RenderBox>> m_child_render_boxes;
};

inline ZStack::operator WidgetPtr() const& {
    return make_widget<ZStackWidget>(*this);
}
inline ZStack::operator WidgetPtr() && {
    return make_widget<ZStackWidget>(std::move(*this));
}

} // namespace ktui
