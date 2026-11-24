#pragma once

#include "widget.hpp"
#include <vector>
#include <memory>
#include <algorithm>

namespace ktui {

// 层叠容器渲染盒 (RenderZStack)
class RenderZStack : public RenderBox {
public:
    RenderZStack() noexcept = default;
    ~RenderZStack() override = default;

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
        float max_w = 0.0f;
        float max_h = 0.0f;

        // 对所有子节点传递松约束并对齐在 (0, 0)
        for (auto& child : m_child_boxes) {
            if (child) {
                child->layout(m_constraints.loosen(), true);
                child->set_offset(Vec2{0.0f, 0.0f});
                max_w = std::max(max_w, child->size().x);
                max_h = std::max(max_h, child->size().y);
            }
        }

        m_size = m_constraints.constrain(Vec2{max_w, max_h});
    }

private:
    std::vector<std::shared_ptr<RenderBox>> m_child_boxes;
};

class ZStackWidget;

// C++20 指定初始化器层叠容器 (ZStack)
struct ZStack {
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
        auto node = make_render_node<RenderZStack>();
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
