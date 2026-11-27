#pragma once

#include "render_node.hpp"
#include <ktui/layout/box_constraints.hpp>
#include <ktui/layout/flex_types.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/hit_test_tree.hpp>
#include <ktui/event/touch_metrics.hpp>
#include <functional>

namespace ktui {

class PaintContext;

// 2D 盒模型渲染对象 (RenderBox)
// 统一实现自顶向下传递 BoxConstraints，自底向上决定 Size，并推导 Relayout Boundary
class RenderBox : public RenderNode {
public:
    using DestroyHook = void(*)(RenderBox*) noexcept;
    inline static DestroyHook s_destroy_hook = nullptr;

    RenderBox() noexcept = default;
    ~RenderBox() override {
        if (s_destroy_hook) {
            s_destroy_hook(this);
        }
    }

    [[nodiscard]] RenderBox* as_render_box() noexcept override { return this; }
    [[nodiscard]] const RenderBox* as_render_box() const noexcept override { return this; }

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
            if (m_child_needs_layout) {
                for (RenderNode* child = first_child(); child != nullptr; child = child->next_sibling()) {
                    if (auto* child_box = (child ? (child)->as_render_box() : nullptr)) {
                        if (child_box->needs_layout() || child_box->child_needs_layout()) {
                            child_box->layout(child_box->constraints(), !child_box->is_relayout_boundary());
                        }
                    }
                }
                m_child_needs_layout = false;
            }
            return; // 约束未变且非脏状态，跳过重复布局
        }

        m_constraints = constraints;
        perform_layout();
        m_needs_layout = false;
        m_child_needs_layout = false;
        mark_needs_paint();
    }

    // 子类必须实现具体排版算法 (例如 Flex 求解或文本折行)
    virtual void perform_layout() = 0;

    // 测量固有内在尺寸 (Intrinsic Size)
    [[nodiscard]] virtual Vec2 compute_intrinsic_size(const BoxConstraints& constraints) const {
        return constraints.constrain(Vec2{0.0f, 0.0f});
    }

    // 返回当前装饰容器的物理圆角半径 (< 0 表示非装饰圆角表面，用于无 RTTI 的同心圆角推导)
    [[nodiscard]] virtual float surface_corner_radius() const noexcept {
        return -1.0f;
    }

    // ==================== 命中测试 (Hit Testing) ====================
    // position 为相对于当前节点左上角的局部坐标
    virtual bool hit_test(HitTestResult& result, PointF position) {
        if (!bounds().contains(position)) {
            return false;
        }

        // 后序逆向遍历子节点 (后添加的、处于视觉顶层的子节点优先测试)
        for (RenderNode* child = last_child(); child != nullptr; child = child->prev_sibling()) {
            if (auto* child_box = (child ? (child)->as_render_box() : nullptr)) {
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

    // 触控命中测试：自动沿渲染树透传并允许小尺寸叶子控件覆写隐式扩张 44px 触控热区
    // 容器节点在子节点略微溢出自身 bounds（因触控热区外扩）时也能正确命中子控件
    virtual bool hit_test_touch(HitTestResult& result, PointF position) {
        Rect expanded = touch::expand_hit_rect_for_touch(bounds(), touch::kMinTargetSize);
        if (!expanded.contains(position)) {
            return false;
        }
        for (RenderNode* child = last_child(); child != nullptr; child = child->prev_sibling()) {
            if (auto* child_box = (child ? (child)->as_render_box() : nullptr)) {
                PointF child_pos = PointF{position.x - child_box->offset().x, position.y - child_box->offset().y};
                if (child_box->hit_test_touch(result, child_pos)) {
                    result.add(this, position);
                    return true;
                }
            }
        }
        if (bounds().contains(position)) {
            result.add(this, position);
            return true;
        }
        return false;
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
            if (auto* box = (p ? (p)->as_render_box() : nullptr)) {
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
    bool is_draggable{false};
    bool is_drop_target{false};
    CursorShape cursor_shape{CursorShape::Arrow};

    Fn<void(PointerEvent&)> on_pointer_preview; // 隧道/捕获阶段预览回调 (供触控滚动与边缘滑动手势识别)
    Fn<void(PointerEvent&)> on_pointer_down;
    Fn<void(PointerEvent&)> on_pointer_up;
    Fn<void(PointerEvent&)> on_pointer_cancel;  // 触控手势被祖先抢占或系统中断时回调 (彻底取消按压态防止误触点击)
    Fn<void(PointerEvent&)> on_pointer_move;
    Fn<void(PointerEvent&)> on_pointer_enter;
    Fn<void(PointerEvent&)> on_pointer_exit;
    Fn<void(KeyEvent&)> on_key;
    Fn<void(FocusEvent&)> on_focus;

    // 拖拽与文件拖放回调 (窗口内控件拖拽 + OS 文件拖放统一协议)
    Fn<void(DragEvent&)> on_drag_start;
    Fn<void(DragEvent&)> on_drag_enter;
    Fn<void(DragEvent&)> on_drag_over;
    Fn<void(DragEvent&)> on_drag_leave;
    Fn<void(DragEvent&)> on_drop;
    Fn<void(DragEvent&)> on_drag_end;

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
    // 大页面滚动视口剔除：完全落在当前有效裁剪视口外（含 64px 阴影安全外扩）的子树直接跳过
    const bool clip_active = context.has_clip();
    for (RenderNode* child = first_child(); child != nullptr; child = child->next_sibling()) {
        if (auto* child_box = (child ? (child)->as_render_box() : nullptr)) {
            if (clip_active) {
                const Vec2 sz = child_box->size();
                if (sz.x > 0.0f && sz.y > 0.0f &&
                    !context.is_rect_visible(child_box->frame(), 64.0f)) {
                    continue;
                }
            }
            PaintContext::OffsetScope scope(context, child_box->offset());
            child_box->paint(context);
        }
    }
}

} // namespace ktui
