#pragma once

#include "vstack.hpp" // 复用 RenderFlexBox

namespace ktui {

class HStackWidget;

// C++20 指定初始化器水平弹性容器 (HStack)
// 零开销原生字段 + 链式 .bind(...) 按需绑定任意属性
struct HStack : BindableMixin<HStack, RenderFlexBox> {
    JustifyContent justify{JustifyContent::FlexStart};
    AlignItems align_items{AlignItems::Center}; // 水平栏桌面默认垂直居中
    float spacing{6.0f}; // 桌面紧凑标准间距 6px
    EdgeInsets padding{EdgeInsets::zero()};
    std::vector<WidgetPtr> content{};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

// HStackWidget 包装实现
class HStackWidget : public Widget {
public:
    explicit HStackWidget(const HStack& desc) : m_desc(desc) {}
    explicit HStackWidget(HStack&& desc) noexcept : m_desc(std::move(desc)) {}
    ~HStackWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderFlexBox>(
            FlexDirection::Row,
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
            flex_box->set_direction(FlexDirection::Row);
            flex_box->set_justify(m_desc.justify);
            flex_box->set_align_items(m_desc.align_items);
            flex_box->set_gap(m_desc.spacing);
            flex_box->set_padding(m_desc.padding);

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

    [[nodiscard]] const HStack& desc() const noexcept { return m_desc; }

private:
    HStack m_desc;
    std::vector<std::shared_ptr<RenderBox>> m_child_render_boxes;
};

inline HStack::operator WidgetPtr() const& {
    return make_widget<HStackWidget>(*this);
}
inline HStack::operator WidgetPtr() && {
    return make_widget<HStackWidget>(std::move(*this));
}

} // namespace ktui
