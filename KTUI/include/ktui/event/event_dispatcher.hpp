#pragma once

#include "event_types.hpp"
#include "hit_test_tree.hpp"
#include <ktui/tree/render_box.hpp>
#include <unordered_set>
#include <vector>
#include <algorithm>

namespace ktui {

// UI 统一事件双向分发器 (Event Dispatcher)
// 管理 :hover 与 :active 自动流转、深层命中测试与叶到根事件冒泡/阻断
class EventDispatcher {
public:
    EventDispatcher() = default;

    // 分发指针输入事件
    void dispatch_pointer_event(RenderBox* root, PointerEvent& event) {
        if (!root) return;

        // ==================== 0. 独占触控手势捕获快速通道 ====================
        if (m_gesture_captured && m_active_target &&
            (event.type == PlatformEventType::PointerMove || event.type == PlatformEventType::PointerUp)) {
            Vec2 abs_off = m_active_target->global_offset();
            event.local_position = event.position - abs_off;
            if (event.type == PlatformEventType::PointerMove && m_active_target->on_pointer_move) {
                m_active_target->on_pointer_move(event);
            } else if (event.type == PlatformEventType::PointerUp) {
                if (m_active_target->on_pointer_up) {
                    m_active_target->on_pointer_up(event);
                }
                m_active_target->is_active = false;
                m_active_target = nullptr;
                m_gesture_captured = false;
            }
            return;
        }

        HitTestResult result;
        root->hit_test(result, event.position);

        // ==================== 0.5 隧道预览阶段 (Root -> Leaf，供触控滚动与边缘滑动手势识别) ====================
        if (event.type == PlatformEventType::PointerDown ||
            event.type == PlatformEventType::PointerMove ||
            event.type == PlatformEventType::PointerUp) {
            const auto& path = result.path();
            for (auto it = path.rbegin(); it != path.rend(); ++it) {
                RenderBox* target = it->target;
                if (!target || !target->on_pointer_preview) continue;
                event.local_position = it->local_position;
                target->on_pointer_preview(event);
                if (event.gesture_capture_target != nullptr) {
                    RenderBox* capturer = event.gesture_capture_target;
                    // 取消先前被手指按下的底层子控件 (例如 Button/Toggle)，防止滚动后误触发点击
                    if (m_active_target && m_active_target != capturer) {
                        RenderBox* prev = m_active_target;
                        prev->is_active = false;
                        prev->is_hovered = false;
                        if (prev->on_pointer_exit) {
                            PointerEvent cancel_ev = event;
                            cancel_ev.type = PlatformEventType::PointerMove;
                            prev->on_pointer_exit(cancel_ev);
                        }
                    }
                    m_active_target = capturer;
                    m_active_target->is_active = true;
                    m_gesture_captured = true;
                    return;
                }
                if (event.is_propagation_stopped()) {
                    return;
                }
            }
        }

        // ==================== 1. Hover 状态流转管理 ====================
        if (event.type == PlatformEventType::PointerMove && event.device_kind != PointerDeviceKind::Touch) {
            std::unordered_set<RenderBox*> current_targets;
            for (const auto& entry : result.path()) {
                current_targets.insert(entry.target);
            }

            // 处理 Exit: 存在于旧列表但不存在于新列表的节点 (先拷贝防止回调期间销毁/修改集合导致迭代器失效)
            std::vector<RenderBox*> exited_nodes;
            for (RenderBox* old_node : m_hovered_nodes) {
                if (current_targets.find(old_node) == current_targets.end()) {
                    exited_nodes.push_back(old_node);
                }
            }
            m_hovered_nodes = std::move(current_targets);

            for (RenderBox* old_node : exited_nodes) {
                old_node->is_hovered = false;
                if (old_node->on_pointer_exit) {
                    PointerEvent exit_event = event;
                    exit_event.type = PlatformEventType::PointerMove;
                    old_node->on_pointer_exit(exit_event);
                }
            }

            // 处理 Enter: 存在于新列表但不存在于旧列表的节点
            std::vector<RenderBox*> entered_nodes;
            for (RenderBox* new_node : m_hovered_nodes) {
                if (!new_node->is_hovered) {
                    entered_nodes.push_back(new_node);
                }
            }
            for (RenderBox* new_node : entered_nodes) {
                new_node->is_hovered = true;
                if (new_node->on_pointer_enter) {
                    PointerEvent enter_event = event;
                    enter_event.type = PlatformEventType::PointerMove;
                    new_node->on_pointer_enter(enter_event);
                }
            }
        }

        // ==================== 2. Active 捕获 ====================
        if (event.type == PlatformEventType::PointerDown) {
            m_active_target = result.first_target();
            if (m_active_target) {
                m_active_target->is_active = true;
            }
        }

        // ==================== 3. Bubble 阶段冒泡派发 ====================
        // 若当前存在拖拽捕获目标 (m_active_target) 且鼠标移出了目标区域，仍需向捕获目标转发 Move/Up
        bool active_hit = false;
        m_tree_mutated = false;
        for (const auto& entry : result.path()) {
            RenderBox* target = entry.target;
            if (!target) continue;
            if (target == m_active_target) active_hit = true;

            // 设置目标局部坐标
            event.local_position = entry.local_position;

            switch (event.type) {
                case PlatformEventType::PointerDown:
                    if (target->on_pointer_down) target->on_pointer_down(event);
                    break;
                case PlatformEventType::PointerUp:
                    if (target->on_pointer_up) target->on_pointer_up(event);
                    break;
                case PlatformEventType::PointerMove:
                case PlatformEventType::PointerScroll:
                    if (target->on_pointer_move) target->on_pointer_move(event);
                    break;
                default:
                    break;
            }

            // 若回调中调用了 stop_propagation() 或销毁了当前渲染树节点，立即阻断冒泡避免悬空指针！
            if (event.is_propagation_stopped() || m_tree_mutated) {
                break;
            }
        }

        if (m_active_target && !active_hit &&
            (event.type == PlatformEventType::PointerMove || event.type == PlatformEventType::PointerUp)) {
            Vec2 abs_off = m_active_target->global_offset();
            event.local_position = event.position - abs_off;
            if (event.type == PlatformEventType::PointerMove && m_active_target->on_pointer_move) {
                m_active_target->on_pointer_move(event);
            } else if (event.type == PlatformEventType::PointerUp && m_active_target->on_pointer_up) {
                m_active_target->on_pointer_up(event);
            }
        }

        // ==================== 4. Active 释放（在 PointerUp 回调执行后） ====================
        if (event.type == PlatformEventType::PointerUp) {
            if (m_active_target) {
                m_active_target->is_active = false;
                m_active_target = nullptr;
            }
            m_gesture_captured = false;
        }
    }

    // 分发键盘事件
    void dispatch_key_event(RenderBox* focused_node, RenderBox* root, KeyEvent& event) {
        RenderBox* current = focused_node ? focused_node : root;

        // 沿父节点链冒泡
        while (current) {
            if (current->on_key) {
                current->on_key(event);
            }

            if (event.is_propagation_stopped()) {
                break;
            }

            current = (current->parent() ? current->parent()->as_render_box() : nullptr);
        }
    }

    [[nodiscard]] RenderBox* active_target() const noexcept { return m_active_target; }
    [[nodiscard]] const std::unordered_set<RenderBox*>& hovered_nodes() const noexcept { return m_hovered_nodes; }

    void forget_node(RenderBox* node) noexcept {
        if (!node) return;
        m_tree_mutated = true;
        m_hovered_nodes.erase(node);
        if (m_active_target == node) {
            m_active_target = nullptr;
            m_gesture_captured = false;
        }
    }

    // 取消当前正在进行的按压/拖拽交互（例如触控长按触发右键菜单时，取消底层控件的点击态）
    void cancel_active_interaction() {
        if (m_active_target) {
            RenderBox* prev = m_active_target;
            m_active_target = nullptr;
            prev->is_active = false;
            prev->is_hovered = false;
            if (prev->on_pointer_exit) {
                PointerEvent exit_ev(PlatformEventType::PointerMove, Vec2{-10000.0f, -10000.0f}, PointerButton::None);
                exit_ev.local_position = Vec2{-10000.0f, -10000.0f};
                prev->on_pointer_exit(exit_ev);
            }
            prev->mark_needs_paint();
        }
        m_gesture_captured = false;
    }

    void clear() {
        m_hovered_nodes.clear();
        m_active_target = nullptr;
        m_gesture_captured = false;
    }

private:
    std::unordered_set<RenderBox*> m_hovered_nodes;
    RenderBox* m_active_target{nullptr};
    bool m_gesture_captured{false};
    bool m_tree_mutated{false};
};

} // namespace ktui
