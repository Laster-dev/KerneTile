#pragma once

#include "render_box.hpp"
#include <ktui/layout/flex_engine.hpp>

namespace ktui {

// Flex 弹性容器渲染盒 (RenderFlex)
// 对应上层声明式组件 HStack 与 VStack，驱动双通道 Flex 布局算法
class RenderFlex : public RenderBox {
public:
    RenderFlex(FlexDirection direction = FlexDirection::Row,
               JustifyContent justify = JustifyContent::FlexStart,
               AlignItems align_items = AlignItems::FlexStart,
               float gap = 0.0f) noexcept
        : m_direction(direction), m_justify(justify), m_align_items(align_items), m_gap(gap) {}

    ~RenderFlex() override = default;

    void perform_layout() override {
        FlexEngine::Params params{
            .direction = m_direction,
            .justify = m_justify,
            .align_items = m_align_items,
            .gap = m_gap
        };
        FlexEngine::layout_flex(this, m_constraints, params);
    }

    [[nodiscard]] Vec2 compute_intrinsic_size(const BoxConstraints& constraints) const override {
        const bool is_horizontal = (m_direction == FlexDirection::Row || m_direction == FlexDirection::RowReverse);
        float main_sum = 0.0f;
        float cross_max = 0.0f;
        size_t count = 0;
        for (const RenderNode* child = first_child(); child != nullptr; child = child->next_sibling()) {
            if (const auto* box = child->as_render_box()) {
                Vec2 sz = box->compute_intrinsic_size(constraints.loosen());
                main_sum += is_horizontal ? sz.x : sz.y;
                cross_max = std::max(cross_max, is_horizontal ? sz.y : sz.x);
                ++count;
            }
        }
        if (count > 1) {
            main_sum += m_gap * static_cast<float>(count - 1);
        }
        Vec2 desired = is_horizontal ? Vec2{main_sum, cross_max} : Vec2{cross_max, main_sum};
        return constraints.constrain(desired);
    }

    [[nodiscard]] FlexDirection direction() const noexcept { return m_direction; }
    void set_direction(FlexDirection dir) {
        if (m_direction != dir) {
            m_direction = dir;
            mark_needs_layout();
        }
    }

    [[nodiscard]] JustifyContent justify() const noexcept { return m_justify; }
    void set_justify(JustifyContent j) {
        if (m_justify != j) {
            m_justify = j;
            mark_needs_layout();
        }
    }

    [[nodiscard]] AlignItems align_items() const noexcept { return m_align_items; }
    void set_align_items(AlignItems a) {
        if (m_align_items != a) {
            m_align_items = a;
            mark_needs_layout();
        }
    }

    [[nodiscard]] float gap() const noexcept { return m_gap; }
    void set_gap(float g) {
        if (m_gap != g) {
            m_gap = g;
            mark_needs_layout();
        }
    }

private:
    FlexDirection m_direction{FlexDirection::Row};
    JustifyContent m_justify{JustifyContent::FlexStart};
    AlignItems m_align_items{AlignItems::FlexStart};
    float m_gap{0.0f};
};

// 基础固定尺寸/受限叶子节点 (用于渲染树单元测试及作为基底占位)
class RenderSizedBox : public RenderBox {
public:
    explicit RenderSizedBox(Vec2 fixed_size) noexcept
        : m_fixed_size(fixed_size) {}

    ~RenderSizedBox() override = default;

    void perform_layout() override {
        m_size = m_constraints.constrain(m_fixed_size);
        for (RenderNode* child = first_child(); child != nullptr; child = child->next_sibling()) {
            if (auto* box = (child ? (child)->as_render_box() : nullptr)) {
                box->layout(BoxConstraints::loose(m_size));
            }
        }
    }

    void set_fixed_size(Vec2 sz) {
        if (m_fixed_size != sz) {
            m_fixed_size = sz;
            mark_needs_layout();
        }
    }

    [[nodiscard]] Vec2 fixed_size() const noexcept { return m_fixed_size; }

private:
    Vec2 m_fixed_size{0.0f, 0.0f};
};

} // namespace ktui
