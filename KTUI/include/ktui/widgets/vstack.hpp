#pragma once

#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/tree/render_flex.hpp>
#include <vector>
#include <memory>

namespace ktui {

// 支持 Padding 的弹性容器渲染盒
class RenderFlexBox : public RenderFlex {
public:
    RenderFlexBox(FlexDirection direction,
                  JustifyContent justify = JustifyContent::FlexStart,
                  AlignItems align_items = AlignItems::FlexStart,
                  float gap = 6.0f,
                  EdgeInsets padding = EdgeInsets::zero()) noexcept
        : RenderFlex(direction, justify, align_items, gap), m_padding(padding) {}

    ~RenderFlexBox() override = default;

    void set_padding(EdgeInsets pad) noexcept {
        if (!(m_padding == pad)) {
            m_padding = pad;
            mark_needs_layout();
        }
    }

    [[nodiscard]] EdgeInsets padding() const noexcept { return m_padding; }

    void perform_layout() override {
        BoxConstraints outer_constraints = m_constraints;
        // 扣除 padding
        BoxConstraints inner_constraints = outer_constraints.deflate(m_padding);

        // 临时设置约束并排版
        m_constraints = inner_constraints;
        RenderFlex::perform_layout();

        // 恢复外部约束，尺寸累加 padding
        m_constraints = outer_constraints;
        Vec2 pad_sz{m_padding.horizontal(), m_padding.vertical()};
        m_size = m_constraints.constrain(m_size + pad_sz);

        // 为所有子节点叠加 padding 偏移
        if (m_padding.left != 0.0f || m_padding.top != 0.0f) {
            for (RenderNode* child = first_child(); child != nullptr; child = child->next_sibling()) {
                if (auto* box = (child ? (child)->as_render_box() : nullptr)) {
                    box->set_offset(box->offset() + Vec2{m_padding.left, m_padding.top});
                }
            }
        }
    }

    void set_child_boxes(std::vector<std::shared_ptr<RenderBox>> boxes) {
        // 解除旧子节点
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

    [[nodiscard]] const std::vector<std::shared_ptr<RenderBox>>& child_boxes() const noexcept {
        return m_child_boxes;
    }

private:
    EdgeInsets m_padding{EdgeInsets::zero()};
    std::vector<std::shared_ptr<RenderBox>> m_child_boxes;
};

class VStackWidget;

// C++20 指定初始化器垂直弹性容器 (VStack)
// 零开销原生字段 + 链式 .bind(...) 按需绑定任意属性
struct VStack : BindableMixin<VStack, RenderFlexBox> {
    JustifyContent justify{JustifyContent::FlexStart};
    AlignItems align_items{AlignItems::FlexStart};
    float spacing{6.0f}; // 桌面紧凑标准间距 6px，绝不 16px/24px
    EdgeInsets padding{EdgeInsets::zero()};
    bool reverse{false};
    std::vector<WidgetPtr> content{};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

// VStackWidget 包装实现
class VStackWidget : public Widget {
public:
    explicit VStackWidget(const VStack& desc) : m_desc(desc) {}
    explicit VStackWidget(VStack&& desc) noexcept : m_desc(std::move(desc)) {}
    ~VStackWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderFlexBox>(
            m_desc.reverse ? FlexDirection::ColumnReverse : FlexDirection::Column,
            m_desc.justify,
            m_desc.align_items,
            m_desc.spacing,
            m_desc.padding
        );

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
        if (auto flex_box = std::static_pointer_cast<RenderFlexBox>(node)) {
            flex_box->set_direction(m_desc.reverse ? FlexDirection::ColumnReverse : FlexDirection::Column);
            flex_box->set_justify(m_desc.justify);
            flex_box->set_align_items(m_desc.align_items);
            flex_box->set_gap(m_desc.spacing);
            flex_box->set_padding(m_desc.padding);

            // 增量同步子节点
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
            flex_box->set_child_boxes(m_child_render_boxes);
        }
    }

    [[nodiscard]] const VStack& desc() const noexcept { return m_desc; }

private:
    VStack m_desc;
    std::vector<std::shared_ptr<RenderBox>> m_child_render_boxes;
};

KTUI_NOINLINE inline VStack::operator WidgetPtr() const& {
    return make_widget<VStackWidget>(*this);
}
KTUI_NOINLINE inline VStack::operator WidgetPtr() && {
    return make_widget<VStackWidget>(std::move(*this));
}

} // namespace ktui
