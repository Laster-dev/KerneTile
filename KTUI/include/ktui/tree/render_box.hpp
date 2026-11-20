#pragma once

#include "render_node.hpp"
#include <ktui/layout/box_constraints.hpp>
#include <ktui/layout/flex_types.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/hit_test_tree.hpp>
#include <functional>

namespace ktui {

class PaintContext;

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

    // ==================== 命中测试 (Hit Testing) ====================
    // position 为相对于当前节点左上角的局部坐标
    virtual bool hit_test(HitTestResult& result, PointF position) {
        if (!bounds().contains(position)) {
            return false;
        }

        // 后序逆向遍历子节点 (后添加的、处于视觉顶层的子节点优先测试)
        for (RenderNode* child = last_child(); child != nullptr; child = child->prev_sibling()) {
            if (auto* child_box = dynamic_cast<RenderBox*>(child)) {
                PointF child_pos = PointF{position.x - child_box->offset().x, position.y - child_box->offset().y};
                if (child_box->hit_test(result, child_pos)) {
                    result.add(this, position);
                    return true;
                }
            }
        }

        // 若子节点未命中，但点击点处于自身区域内，则自身命中
        result.add(this, position);
        return true;
    }

    // ==================== 绘制管线 (Painting Pipeline) ====================
    virtual void paint(PaintContext& context);

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

    // ==================== 交互与事件回调 ====================
    bool is_hovered{false};
    bool is_active{false};
    bool is_focusable{false};
    CursorShape cursor_shape{CursorShape::Arrow};

    std::function<void(PointerEvent&)> on_pointer_down;
    std::function<void(PointerEvent&)> on_pointer_up;
    std::function<void(PointerEvent&)> on_pointer_move;
    std::function<void(PointerEvent&)> on_pointer_enter;
    std::function<void(PointerEvent&)> on_pointer_exit;
    std::function<void(KeyEvent&)> on_key;
    std::function<void(FocusEvent&)> on_focus;

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

#include <ktui/presentation/paint_context.hpp>

namespace ktui {

inline void RenderBox::paint(PaintContext& context) {
    // 默认遍历所有子节点，平移到子节点的局部 offset 并递归绘制
    for (RenderNode* child = first_child(); child != nullptr; child = child->next_sibling()) {
        if (auto* child_box = dynamic_cast<RenderBox*>(child)) {
            PaintContext::OffsetScope scope(context, child_box->offset());
            child_box->paint(context);
        }
    }
}

} // namespace ktui
