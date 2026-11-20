#pragma once

#include "render_node.hpp"
#include <ktui/layout/box_constraints.hpp>
#include <ktui/layout/flex_types.hpp>

namespace ktui {

// 2D 盒模型渲染对象 (RenderBox)
// 统一实现自顶向下传递 BoxConstraints，自底向上决定 Size，并推导 Relayout Boundary
class RenderBox : public RenderNode {
public:
    RenderBox() noexcept = default;
    ~RenderBox() override = default;

    // 核心入口：执行排版布局
    // parent_uses_size 表示父节点是否依赖当前节点的最终计算尺寸
    void layout(const BoxConstraints& constraints, bool parent_uses_size = true) {
        // 自动判定重排边界：
        // 1. 显式指定了强行重排边界 (m_force_relayout_boundary)；
        // 2. 父级给入的是紧约束 (尺寸由父级硬性规定)；
        // 3. 父级排版并不依赖子级的计算尺寸 (!parent_uses_size)；
        // 4. 根节点 (无父级)。
        bool is_boundary = m_force_relayout_boundary || !parent_uses_size || constraints.is_tight() || (m_parent == nullptr);
        m_is_relayout_boundary = is_boundary;

        if (!m_needs_layout && constraints == m_constraints) {
            return; // 约束未变且非脏状态，跳过重复布局
        }

        m_constraints = constraints;
        perform_layout();
        m_needs_layout = false;
        mark_needs_paint();
    }

    // 子类必须实现具体排版算法 (例如 Flex 求解或文本折行)
    virtual void perform_layout() = 0;

    // 测量固有内在尺寸 (Intrinsic Size)
    [[nodiscard]] virtual Vec2 compute_intrinsic_size(const BoxConstraints& constraints) const {
        return constraints.constrain(Vec2{0.0f, 0.0f});
    }

    // ==================== 几何信息 ====================
    [[nodiscard]] const BoxConstraints& constraints() const noexcept { return m_constraints; }
    [[nodiscard]] Vec2 size() const noexcept { return m_size; }
    [[nodiscard]] Vec2 offset() const noexcept { return m_offset; }

    void set_size(Vec2 sz) noexcept { m_size = sz; }
    void set_offset(Vec2 off) noexcept { m_offset = off; }

    // 本地包围盒 (原点为 0,0)
    [[nodiscard]] Rect bounds() const noexcept {
        return Rect::from_xywh(0.0f, 0.0f, m_size.x, m_size.y);
    }

    // 相对于父节点的局部坐标包围盒
    [[nodiscard]] Rect frame() const noexcept {
        return Rect::from_xywh(m_offset.x, m_offset.y, m_size.x, m_size.y);
    }

    // 计算相对于根视口的全局绝对坐标
    [[nodiscard]] Vec2 global_offset() const noexcept {
        Vec2 pt = m_offset;
        RenderNode* p = m_parent;
        while (p) {
            if (auto* box = dynamic_cast<RenderBox*>(p)) {
                pt = pt + box->offset();
            }
            p = p->parent();
        }
        return pt;
    }

    // ==================== Flex Item 附加属性 ====================
    [[nodiscard]] const FlexItemData& flex_data() const noexcept { return m_flex_data; }
    FlexItemData& flex_data() noexcept { return m_flex_data; }
    void set_flex_data(const FlexItemData& data) noexcept { m_flex_data = data; }

protected:
    BoxConstraints m_constraints;
    Vec2 m_size{0.0f, 0.0f};
    Vec2 m_offset{0.0f, 0.0f};
    FlexItemData m_flex_data;
};

} // namespace ktui
