#pragma once

#include "vstack.hpp" // 复用 RenderFlexBox

namespace ktui {

class HStackWidget;

// C++20 指定初始化器水平弹性容器 (HStack)
// 桌面专业标准：默认间距 spacing = 6.0f (紧凑高信息密度)，全属性支持 Bind(...)
struct HStack {
    Bindable<JustifyContent> justify{JustifyContent::FlexStart};
    Bindable<AlignItems> align_items{AlignItems::Center}; // 水平栏桌面默认垂直居中
    Bindable<float> spacing{6.0f}; // 桌面紧凑标准间距 6px
    Bindable<EdgeInsets> padding{EdgeInsets::zero()};
    std::vector<WidgetPtr> content{};

    operator WidgetPtr() const;
};

// HStackWidget 包装实现
class HStackWidget : public Widget {
public:
    explicit HStackWidget(const HStack& desc) : m_desc(desc) {}
    ~HStackWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderFlexBox>(
            FlexDirection::Row,
            m_desc.justify.get(),
            m_desc.align_items.get(),
            m_desc.spacing.get(),
            m_desc.padding.get()
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
        bind_all(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto flex_box = std::dynamic_pointer_cast<RenderFlexBox>(node)) {
            flex_box->set_direction(FlexDirection::Row);
            flex_box->set_justify(m_desc.justify.get());
            flex_box->set_align_items(m_desc.align_items.get());
            flex_box->set_gap(m_desc.spacing.get());
            flex_box->set_padding(m_desc.padding.get());

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
            bind_all(flex_box);
        }
    }

    [[nodiscard]] const HStack& desc() const noexcept { return m_desc; }

private:
    void bind_all(const std::shared_ptr<RenderFlexBox>& flex_box) {
        m_bindings.clear();
        m_bindings.attach(m_desc.justify, flex_box, [](RenderFlexBox& n, JustifyContent v) { n.set_justify(v); });
        m_bindings.attach(m_desc.align_items, flex_box, [](RenderFlexBox& n, AlignItems v) { n.set_align_items(v); });
        m_bindings.attach(m_desc.spacing, flex_box, [](RenderFlexBox& n, float v) { n.set_gap(v); });
        m_bindings.attach(m_desc.padding, flex_box, [](RenderFlexBox& n, EdgeInsets v) { n.set_padding(v); });
    }

    HStack m_desc;
    std::vector<std::shared_ptr<RenderBox>> m_child_render_boxes;
    BindingGroup m_bindings;
};

inline HStack::operator WidgetPtr() const {
    return std::make_shared<HStackWidget>(*this);
}

} // namespace ktui
