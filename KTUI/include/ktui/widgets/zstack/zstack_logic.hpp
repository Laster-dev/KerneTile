#pragma once

#include "zstack_data.hpp"
#include "zstack_style.hpp"
#include <algorithm>

namespace ktui {


// ZStack 内部边角/坐标独立定位包装节点 (RenderPositioned)
class RenderPositioned : public RenderBox {
public:
    RenderPositioned(std::optional<float> left = std::nullopt,
                     std::optional<float> top = std::nullopt,
                     std::optional<float> right = std::nullopt,
                     std::optional<float> bottom = std::nullopt,
                     std::optional<float> width = std::nullopt,
                     std::optional<float> height = std::nullopt) noexcept
        : m_left(left), m_top(top), m_right(right), m_bottom(bottom),
          m_width(width), m_height(height) {}

    [[nodiscard]] RenderPositioned* as_positioned() noexcept override { return this; }
    [[nodiscard]] const RenderPositioned* as_positioned() const noexcept override { return this; }

    void set_edges(std::optional<float> left, std::optional<float> top,
                   std::optional<float> right, std::optional<float> bottom,
                   std::optional<float> width, std::optional<float> height) noexcept {
        if (m_left != left || m_top != top || m_right != right || m_bottom != bottom ||
            m_width != width || m_height != height) {
            m_left = left; m_top = top; m_right = right; m_bottom = bottom;
            m_width = width; m_height = height;
            mark_needs_layout();
        }
    }

    [[nodiscard]] std::optional<float> left() const noexcept { return m_left; }
    [[nodiscard]] std::optional<float> top() const noexcept { return m_top; }
    [[nodiscard]] std::optional<float> right() const noexcept { return m_right; }
    [[nodiscard]] std::optional<float> bottom() const noexcept { return m_bottom; }
    [[nodiscard]] std::optional<float> fixed_width() const noexcept { return m_width; }
    [[nodiscard]] std::optional<float> fixed_height() const noexcept { return m_height; }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child == child) return;
        if (m_child) remove_child(m_child.get());
        m_child = std::move(child);
        if (m_child) append_child(m_child.get());
        mark_needs_layout();
    }

    void perform_layout() override {
        if (!m_child) {
            m_size = m_constraints.constrain(Vec2{0.0f, 0.0f});
            return;
        }
        m_child->layout(m_constraints, true);
        m_child->set_offset(Vec2{0.0f, 0.0f});
        m_size = m_constraints.constrain(m_child->size());
    }

private:
    std::optional<float> m_left, m_top, m_right, m_bottom, m_width, m_height;
    std::shared_ptr<RenderBox> m_child;
};

struct Positioned : BindableMixin<Positioned, RenderPositioned> {
    std::optional<float> left{std::nullopt};
    std::optional<float> top{std::nullopt};
    std::optional<float> right{std::nullopt};
    std::optional<float> bottom{std::nullopt};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class PositionedWidget : public Widget {
public:
    explicit PositionedWidget(const Positioned& desc) : m_desc(desc) {}
    explicit PositionedWidget(Positioned&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderPositioned>(
            m_desc.left, m_desc.top, m_desc.right, m_desc.bottom, m_desc.width, m_desc.height);
        m_child_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_box) m_desc.child->set_render_node(m_child_box);
        node->set_child_box(m_child_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto p = std::static_pointer_cast<RenderPositioned>(node)) {
            p->set_edges(m_desc.left, m_desc.top, m_desc.right, m_desc.bottom, m_desc.width, m_desc.height);
            if (m_desc.child) {
                if (!m_child_box) {
                    m_child_box = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(m_child_box);
                    p->set_child_box(m_child_box);
                } else {
                    m_desc.child->update_render_node(m_child_box);
                }
            } else if (m_child_box) {
                m_child_box = nullptr;
                p->set_child_box(nullptr);
            }
        }
    }

private:
    Positioned m_desc;
    std::shared_ptr<RenderBox> m_child_box;
};

inline Positioned::operator WidgetPtr() const& { return make_widget<PositionedWidget>(*this); }
inline Positioned::operator WidgetPtr() && { return make_widget<PositionedWidget>(std::move(*this)); }

// 层叠容器渲染盒 (RenderZStack)
// 支持九宫格对齐 (Alignment)、Padding、Loose/Expand 约束模式以及 Positioned 独立边角定位
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
        bool has_positioned = false;

        // Pass 1: 先测量非 Positioned 子项确定基础容器尺寸
        for (auto& child : m_child_boxes) {
            if (!child) continue;
            if (child->as_positioned() != nullptr) {
                has_positioned = true;
                continue;
            }
            child->layout(child_constraints, true);
            max_w = std::max(max_w, child->size().x);
            max_h = std::max(max_h, child->size().y);
        }

        // 若包含 Positioned 边角定位子项（或 Expand 模式），且父容器给出有界尺寸，则自动撑满可用区域
        if (has_positioned || m_fit == StackFit::Expand) {
            if (inner.has_bounded_width())  max_w = std::max(max_w, inner.max_width);
            if (inner.has_bounded_height()) max_h = std::max(max_h, inner.max_height);
        }

        Vec2 pad_sz{m_padding.horizontal(), m_padding.vertical()};
        m_size = m_constraints.constrain(Vec2{max_w + pad_sz.x, max_h + pad_sz.y});
        Vec2 inner_size{
            std::max(0.0f, m_size.x - pad_sz.x),
            std::max(0.0f, m_size.y - pad_sz.y)
        };

        // Pass 2: 定位普通子项 & 按边角约束测量并定位 Positioned 子项
        for (auto& child : m_child_boxes) {
            if (!child) continue;
            if (auto* pos = child->as_positioned()) {
                BoxConstraints pc = BoxConstraints::loose(inner_size);
                if (pos->left().has_value() && pos->right().has_value()) {
                    float w = std::max(0.0f, inner_size.x - *pos->left() - *pos->right());
                    pc.min_width = w; pc.max_width = w;
                } else if (pos->fixed_width().has_value()) {
                    float w = std::max(0.0f, *pos->fixed_width());
                    pc.min_width = w; pc.max_width = w;
                }
                if (pos->top().has_value() && pos->bottom().has_value()) {
                    float h = std::max(0.0f, inner_size.y - *pos->top() - *pos->bottom());
                    pc.min_height = h; pc.max_height = h;
                } else if (pos->fixed_height().has_value()) {
                    float h = std::max(0.0f, *pos->fixed_height());
                    pc.min_height = h; pc.max_height = h;
                }
                pos->layout(pc, true);
                Vec2 fallback = m_alignment.compute_offset(inner_size, pos->size());
                float ox = fallback.x;
                float oy = fallback.y;
                if (pos->left().has_value())       ox = *pos->left();
                else if (pos->right().has_value()) ox = inner_size.x - *pos->right() - pos->size().x;
                if (pos->top().has_value())        oy = *pos->top();
                else if (pos->bottom().has_value()) oy = inner_size.y - *pos->bottom() - pos->size().y;
                pos->set_offset(Vec2{m_padding.left + ox, m_padding.top + oy});
            } else {
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
 // namespace ktui
