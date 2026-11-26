#pragma once

#include "flex_types.hpp"
#include "box_constraints.hpp"
#include <ktui/tree/render_box.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

namespace ktui {

// 自研高性能纯 C++20 双通道 Flexbox 布局求解器 (FlexEngine)
// Pass 1: 测量固定子项并分配剩余空间给弹性子项 (flex_grow / flex_shrink)
// Pass 2: 根据 JustifyContent 与 AlignItems 计算子节点的局部原点并精确定位
class FlexEngine {
public:
    struct Params {
        FlexDirection direction = FlexDirection::Row;
        JustifyContent justify = JustifyContent::FlexStart;
        AlignItems align_items = AlignItems::FlexStart;
        float gap = 0.0f;
    };

    static void layout_flex(RenderBox* container, const BoxConstraints& constraints, const Params& params) {
        if (!container) return;

        // 栈上小缓冲收集子节点 (<= 32 子项零堆分配，超出自动回退 vector)
        constexpr size_t kStackCap = 32;
        RenderBox* stack_children[kStackCap];
        std::vector<RenderBox*> heap_children;
        size_t N = 0;

        for (RenderNode* child = container->first_child(); child != nullptr; child = child->next_sibling()) {
            if (auto* box = child->as_render_box()) {
                if (N < kStackCap && heap_children.empty()) {
                    stack_children[N++] = box;
                } else {
                    if (heap_children.empty()) {
                        heap_children.assign(stack_children, stack_children + N);
                    }
                    heap_children.push_back(box);
                    ++N;
                }
            }
        }
        RenderBox** children = heap_children.empty() ? stack_children : heap_children.data();

        const bool is_horizontal = (params.direction == FlexDirection::Row || params.direction == FlexDirection::RowReverse);

        auto get_main = [is_horizontal](Vec2 v) noexcept -> float { return is_horizontal ? v.x : v.y; };
        auto get_cross = [is_horizontal](Vec2 v) noexcept -> float { return is_horizontal ? v.y : v.x; };
        auto make_vec = [is_horizontal](float m, float c) noexcept -> Vec2 {
            return is_horizontal ? Vec2{m, c} : Vec2{c, m};
        };

        const float max_main_constraint = is_horizontal ? constraints.max_width : constraints.max_height;
        const float min_main_constraint = is_horizontal ? constraints.min_width : constraints.min_height;
        const float max_cross_constraint = is_horizontal ? constraints.max_height : constraints.max_width;
        const float min_cross_constraint = is_horizontal ? constraints.min_height : constraints.min_width;

        if (N == 0) {
            container->set_size(constraints.constrain(Vec2{0.0f, 0.0f}));
            return;
        }

        const float total_gap = (N > 1) ? params.gap * static_cast<float>(N - 1) : 0.0f;

        // ==================== Pass 1: 测量、flex_shrink 压缩与 flex_grow 分配 ====================
        float total_grow = 0.0f;
        float fixed_main_used = total_gap;
        float max_child_cross = 0.0f;
        const bool cross_bounded = (max_cross_constraint < BoxConstraints::kInfinity);
        const bool cross_tight = cross_bounded && (std::abs(max_cross_constraint - min_cross_constraint) < 0.01f);
        const bool main_bounded = (max_main_constraint < BoxConstraints::kInfinity);

        // 第一遍：测量固定尺寸 (非 flex_grow) 子项
        // 注意：仅当容器自身交叉轴为严格紧约束 (cross_tight) 时才在 Pass 1 强制拉伸子项；
        // 若容器交叉轴是松散上限 (如 VStack 内的 HStack，min_h=0, max_h=视口高)，须先按固有尺寸度量再二次对齐！
        for (size_t i = 0; i < N; ++i) {
            RenderBox* child = children[i];
            const auto& flex = child->flex_data();
            if (flex.flex_grow > 0.0f) {
                total_grow += flex.flex_grow;
            } else {
                AlignItems align = flex.align_self.value_or(params.align_items);
                float cross_min = (align == AlignItems::Stretch && cross_tight) ? max_cross_constraint : 0.0f;

                BoxConstraints child_constraints = is_horizontal
                    ? BoxConstraints(0.0f, max_main_constraint, cross_min, max_cross_constraint)
                    : BoxConstraints(cross_min, max_cross_constraint, 0.0f, max_main_constraint);

                child->layout(child_constraints, true);
                fixed_main_used += get_main(child->size());
                max_child_cross = std::max(max_child_cross, get_cross(child->size()));
            }
        }

        // 若固定子项总和超出主轴上限，按 flex_shrink * base_main 比例执行弹性收缩
        if (main_bounded && fixed_main_used > max_main_constraint) {
            const float overflow = fixed_main_used - max_main_constraint;
            float total_shrink_weight = 0.0f;
            for (size_t i = 0; i < N; ++i) {
                RenderBox* child = children[i];
                const auto& flex = child->flex_data();
                if (flex.flex_grow <= 0.0f && flex.flex_shrink > 0.0f) {
                    total_shrink_weight += flex.flex_shrink * get_main(child->size());
                }
            }
            if (total_shrink_weight > 0.001f) {
                fixed_main_used = total_gap;
                max_child_cross = 0.0f;
                for (size_t i = 0; i < N; ++i) {
                    RenderBox* child = children[i];
                    const auto& flex = child->flex_data();
                    if (flex.flex_grow > 0.0f) continue;

                    float base_main = get_main(child->size());
                    if (flex.flex_shrink > 0.0f && base_main > 0.0f) {
                        float shrink_amount = overflow * ((flex.flex_shrink * base_main) / total_shrink_weight);
                        float target_main = std::max(0.0f, base_main - shrink_amount);
                        AlignItems align = flex.align_self.value_or(params.align_items);
                        float cross_min = (align == AlignItems::Stretch && cross_tight) ? max_cross_constraint : 0.0f;
                        BoxConstraints shrunk_constraints = is_horizontal
                            ? BoxConstraints(0.0f, target_main, cross_min, max_cross_constraint)
                            : BoxConstraints(cross_min, max_cross_constraint, 0.0f, target_main);
                        child->layout(shrunk_constraints, true);
                    }
                    fixed_main_used += get_main(child->size());
                    max_child_cross = std::max(max_child_cross, get_cross(child->size()));
                }
            }
        }

        // 确定可供弹性分配的总主轴基准长度
        float target_container_main = (total_grow > 0.0f && main_bounded) ? max_main_constraint : fixed_main_used;
        float remaining_free_space = std::max(0.0f, target_container_main - fixed_main_used);

        // 第二遍：为 flex_grow 子项分配剩余空间并排版
        if (total_grow > 0.0f) {
            float space_per_grow = remaining_free_space / total_grow;
            for (size_t i = 0; i < N; ++i) {
                RenderBox* child = children[i];
                const auto& flex = child->flex_data();
                if (flex.flex_grow > 0.0f) {
                    float allocated_main = flex.flex_grow * space_per_grow;
                    AlignItems align = flex.align_self.value_or(params.align_items);
                    float cross_min = (align == AlignItems::Stretch && cross_tight) ? max_cross_constraint : 0.0f;

                    float child_main_min = (flex.fit == FlexFit::Tight) ? allocated_main : 0.0f;
                    float child_main_max = allocated_main;

                    BoxConstraints child_constraints = is_horizontal
                        ? BoxConstraints(child_main_min, child_main_max, cross_min, max_cross_constraint)
                        : BoxConstraints(cross_min, max_cross_constraint, child_main_min, child_main_max);

                    child->layout(child_constraints, true);
                    max_child_cross = std::max(max_child_cross, get_cross(child->size()));
                }
            }
        }

        // 计算所有子项加间距的实际主轴占用总长
        float actual_main_used = total_gap;
        for (size_t i = 0; i < N; ++i) {
            actual_main_used += get_main(children[i]->size());
        }

        // 确定容器自身的最终尺寸
        float target_main = (total_grow > 0.0f || min_main_constraint == max_main_constraint)
            ? (main_bounded ? max_main_constraint : actual_main_used)
            : actual_main_used;
        float final_container_main = std::clamp(
            target_main,
            min_main_constraint,
            max_main_constraint
        );
        float final_container_cross = std::clamp(
            max_child_cross,
            min_cross_constraint,
            max_cross_constraint
        );

        // 交叉轴非严格紧约束场景下补齐 AlignItems::Stretch 二次拉伸（确保矮子项拉伸至容器实际交叉轴尺寸而非 max_cross_constraint）
        if (!cross_tight && final_container_cross > 0.0f) {
            for (size_t i = 0; i < N; ++i) {
                RenderBox* child = children[i];
                AlignItems align = child->flex_data().align_self.value_or(params.align_items);
                if (align == AlignItems::Stretch && get_cross(child->size()) + 0.5f < final_container_cross) {
                    float child_main = get_main(child->size());
                    BoxConstraints stretch_constraints = is_horizontal
                        ? BoxConstraints(child_main, child_main, final_container_cross, final_container_cross)
                        : BoxConstraints(final_container_cross, final_container_cross, child_main, child_main);
                    child->layout(stretch_constraints, true);
                }
            }
        }

        container->set_size(make_vec(final_container_main, final_container_cross));

        // ==================== Pass 2: 对齐与几何定位 ====================
        float free_main = std::max(0.0f, final_container_main - actual_main_used);
        float current_main = 0.0f;
        float between_space = params.gap;

        switch (params.justify) {
            case JustifyContent::FlexStart:
                current_main = 0.0f;
                between_space = params.gap;
                break;
            case JustifyContent::FlexEnd:
                current_main = free_main;
                between_space = params.gap;
                break;
            case JustifyContent::Center:
                current_main = free_main * 0.5f;
                between_space = params.gap;
                break;
            case JustifyContent::SpaceBetween:
                current_main = 0.0f;
                between_space = (N > 1) ? (params.gap + free_main / static_cast<float>(N - 1)) : params.gap;
                break;
            case JustifyContent::SpaceAround: {
                float unit = free_main / (2.0f * static_cast<float>(N));
                current_main = unit;
                between_space = params.gap + unit * 2.0f;
                break;
            }
            case JustifyContent::SpaceEvenly: {
                float unit = free_main / static_cast<float>(N + 1);
                current_main = unit;
                between_space = params.gap + unit;
                break;
            }
        }

        // 是否为反向排布
        bool is_reverse = (params.direction == FlexDirection::RowReverse || params.direction == FlexDirection::ColumnReverse);
        size_t start_idx = is_reverse ? (N - 1) : 0;

        for (size_t i = 0; i < N; ++i) {
            size_t idx = is_reverse ? (start_idx - i) : (start_idx + i);
            RenderBox* child = children[idx];

            // 交叉轴对齐原点计算
            AlignItems align = child->flex_data().align_self.value_or(params.align_items);
            float child_cross = 0.0f;
            float child_cross_size = get_cross(child->size());

            switch (align) {
                case AlignItems::FlexStart:
                case AlignItems::Stretch:
                    child_cross = 0.0f;
                    break;
                case AlignItems::FlexEnd:
                    child_cross = final_container_cross - child_cross_size;
                    break;
                case AlignItems::Center:
                    child_cross = (final_container_cross - child_cross_size) * 0.5f;
                    break;
            }

            child->set_offset(make_vec(current_main, child_cross));
            current_main += get_main(child->size()) + between_space;
        }
    }
};

} // namespace ktui
