#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <ktui/core/fn.hpp>

namespace ktui {

class RenderBox;
class ITextInputClient;

// 保留式场景图核心基类 (RenderNode - 侵入式四指针双向链表树)
// 彻底杜绝 std::shared_ptr 带来的循环引用与弱指针锁开销，保证 120 FPS 高频遍历的极速内存局部性
class RenderNode {
public:
    RenderNode() noexcept = default;
    virtual ~RenderNode() {
        m_destroying = true;
        // 析构时安全断开所有子节点
        while (m_first_child) {
            remove_child(m_first_child);
        }
        if (m_parent) {
            m_parent->remove_child(this);
        }
    }

    // ==================== 节点挂载生命周期钩子 ====================
    virtual void on_mounted() {}
    virtual void on_unmounted() {}
    [[nodiscard]] bool is_mounted() const noexcept { return m_parent != nullptr; }

    // 零 RTTI 轻量类型转换钩子 (支持完全关闭 /GR- 与 -fno-rtti)
    [[nodiscard]] virtual RenderBox* as_render_box() noexcept { return nullptr; }
    [[nodiscard]] virtual const RenderBox* as_render_box() const noexcept { return nullptr; }
    [[nodiscard]] virtual ITextInputClient* as_text_input_client() noexcept { return nullptr; }

    RenderNode(const RenderNode&) = delete;
    RenderNode& operator=(const RenderNode&) = delete;

    // ==================== 树拓扑与指针访问 ====================
    [[nodiscard]] RenderNode* parent() const noexcept { return m_parent; }
    [[nodiscard]] RenderNode* first_child() const noexcept { return m_first_child; }
    [[nodiscard]] RenderNode* last_child() const noexcept { return m_last_child; }
    [[nodiscard]] RenderNode* next_sibling() const noexcept { return m_next_sibling; }
    [[nodiscard]] RenderNode* prev_sibling() const noexcept { return m_prev_sibling; }
    [[nodiscard]] size_t child_count() const noexcept { return m_child_count; }

    // 在末尾追加子节点
    void append_child(RenderNode* child) {
        insert_before(child, nullptr);
    }

    // 在指定兄弟节点 before 之前插入 child (若 before 为 nullptr 则插到末尾)
    void insert_before(RenderNode* child, RenderNode* before) {
        assert(child != nullptr && "Cannot insert null child!");
        assert(child != this && "Cannot insert node as child of itself!");
        assert(child->m_parent == nullptr && "Child already has a parent, remove it first!");

        child->m_parent = this;

        if (!m_first_child) {
            // 原本为空树
            m_first_child = child;
            m_last_child = child;
            child->m_prev_sibling = nullptr;
            child->m_next_sibling = nullptr;
        } else if (before == nullptr) {
            // 插到末尾
            child->m_prev_sibling = m_last_child;
            child->m_next_sibling = nullptr;
            m_last_child->m_next_sibling = child;
            m_last_child = child;
        } else if (before == m_first_child) {
            // 插到头部
            child->m_prev_sibling = nullptr;
            child->m_next_sibling = m_first_child;
            m_first_child->m_prev_sibling = child;
            m_first_child = child;
        } else {
            // 插到 before 之前
            assert(before->m_parent == this && "before node must be a child of this!");
            child->m_prev_sibling = before->m_prev_sibling;
            child->m_next_sibling = before;
            if (before->m_prev_sibling) {
                before->m_prev_sibling->m_next_sibling = child;
            }
            before->m_prev_sibling = child;
        }

        m_child_count++;
        child->on_mounted();
        mark_needs_layout();
    }

    // 移除子节点
    void remove_child(RenderNode* child) {
        assert(child != nullptr && "Cannot remove null child!");
        assert(child->m_parent == this && "Child does not belong to this parent!");

        if (child->m_prev_sibling) {
            child->m_prev_sibling->m_next_sibling = child->m_next_sibling;
        } else {
            m_first_child = child->m_next_sibling;
        }

        if (child->m_next_sibling) {
            child->m_next_sibling->m_prev_sibling = child->m_prev_sibling;
        } else {
            m_last_child = child->m_prev_sibling;
        }

        child->m_parent = nullptr;
        child->m_prev_sibling = nullptr;
        child->m_next_sibling = nullptr;
        m_child_count--;
        if (!child->m_destroying) {
            child->on_unmounted();
        }

        mark_needs_layout();
    }

    // ==================== Relayout Boundary 阻断机制 ====================
    // 标记当前节点排版已脏
    // 核心契约：当抵达重排边界 (Relayout Boundary) 时，严格阻断向上冒泡，不打扰上层祖先！
    virtual void mark_needs_layout() {
        if (m_needs_layout) return;
        m_needs_layout = true;

        if (m_is_relayout_boundary || m_parent == nullptr) {
            RenderNode* ancestor = m_parent;
            while (ancestor && !ancestor->m_child_needs_layout) {
                ancestor->m_child_needs_layout = true;
                ancestor = ancestor->m_parent;
            }
            schedule_relayout();
        } else {
            m_parent->mark_needs_layout();
        }
    }

    // 标记当前节点视觉绘制已脏 (不引发重新布局排版)
    virtual void mark_needs_paint() {
        if (m_needs_paint) return;
        m_needs_paint = true;
        if (m_parent) {
            m_parent->mark_needs_paint();
        }
    }

    [[nodiscard]] bool needs_layout() const noexcept { return m_needs_layout; }
    [[nodiscard]] bool child_needs_layout() const noexcept { return m_child_needs_layout; }
    [[nodiscard]] bool needs_paint() const noexcept { return m_needs_paint; }
    [[nodiscard]] bool is_relayout_boundary() const noexcept { return m_is_relayout_boundary; }

    void set_relayout_boundary(bool boundary) noexcept {
        m_is_relayout_boundary = boundary;
        m_force_relayout_boundary = boundary;
    }

    [[nodiscard]] bool force_relayout_boundary() const noexcept { return m_force_relayout_boundary; }
    void set_force_relayout_boundary(bool force) noexcept { m_force_relayout_boundary = force; }

    void clear_needs_layout() noexcept { m_needs_layout = false; }
    void clear_needs_paint() noexcept { m_needs_paint = false; }

    void clear_dirty_recursive() noexcept {
        m_needs_layout = false;
        m_child_needs_layout = false;
        m_needs_paint = false;
        for (RenderNode* child = m_first_child; child != nullptr; child = child->m_next_sibling) {
            child->clear_dirty_recursive();
        }
    }

protected:
    virtual void schedule_relayout() {}

    RenderNode* m_parent{nullptr};
    RenderNode* m_first_child{nullptr};
    RenderNode* m_last_child{nullptr};
    RenderNode* m_next_sibling{nullptr};
    RenderNode* m_prev_sibling{nullptr};
    size_t m_child_count{0};

    bool m_needs_layout{true};
    bool m_child_needs_layout{false};
    bool m_needs_paint{true};
    bool m_is_relayout_boundary{false};
    bool m_force_relayout_boundary{false};
    bool m_destroying{false};
    bool m_sized_by_parent{false};
};

} // namespace ktui
