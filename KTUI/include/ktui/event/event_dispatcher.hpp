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
            (event.type == PlatformEventType::PointerMove ||
             event.type == PlatformEventType::PointerUp ||
             event.type == PlatformEventType::PointerCancel)) {
            Vec2 abs_off = m_active_target->global_offset();
            event.local_position = event.position - abs_off;
            if (event.type == PlatformEventType::PointerMove && m_active_target->on_pointer_move) {
                m_active_target->on_pointer_move(event);
            } else if (event.type == PlatformEventType::PointerCancel) {
                dispatch_cancel_to_node(m_active_target, event);
                m_active_target = nullptr;
                m_gesture_captured = false;
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

        if (event.type == PlatformEventType::PointerCancel) {
            cancel_active_interaction();
            return;
        }

        // 触控按下时清理任何残留的鼠标 :hover 状态，彻底杜绝触控屏「点完一直高亮悬停」
        if (event.type == PlatformEventType::PointerDown && event.device_kind == PointerDeviceKind::Touch && !m_hovered_nodes.empty()) {
            std::vector<RenderBox*> prev_hovered(m_hovered_nodes.begin(), m_hovered_nodes.end());
            m_hovered_nodes.clear();
            for (RenderBox* old_node : prev_hovered) {
                old_node->is_hovered = false;
                if (old_node->on_pointer_exit) {
                    PointerEvent exit_ev = event;
                    exit_ev.type = PlatformEventType::PointerMove;
                    old_node->on_pointer_exit(exit_ev);
                }
            }
        }

        HitTestResult result;
        if (event.device_kind == PointerDeviceKind::Touch) {
            root->hit_test_touch(result, event.position);
        } else {
            root->hit_test(result, event.position);
        }

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
                    // 取消先前被手指按下的底层子控件 (例如 Button/Toggle)，派发正式 PointerCancel 防止滚动后误触发点击
                    if (m_active_target && m_active_target != capturer) {
                        dispatch_cancel_to_node(m_active_target, event);
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

        // ==================== 2. Active 捕获与拖拽候选记录 ====================
        if (event.type == PlatformEventType::PointerDown) {
            m_active_target = result.first_target();
            if (m_active_target) {
                m_active_target->is_active = true;
            }
            m_drag_candidate = nullptr;
            if (event.button == PointerButton::Primary) {
                for (const auto& entry : result.path()) {
                    if (entry.target && entry.target->is_draggable && entry.target->on_drag_start) {
                        m_drag_candidate = entry.target;
                        m_drag_start_pos = event.position;
                        break;
                    }
                }
            }
        }

        // ==================== 2.5 窗口内控件拖拽会话 ====================
        if (event.type == PlatformEventType::PointerMove) {
            if (!m_is_dragging && m_drag_candidate && m_drag_candidate->on_drag_start) {
                float dx = event.position.x - m_drag_start_pos.x;
                float dy = event.position.y - m_drag_start_pos.y;
                const float drag_slop_sq = (event.device_kind == PointerDeviceKind::Touch)
                    ? (touch::kTouchSlopPx * touch::kTouchSlopPx)
                    : 25.0f;
                if (dx * dx + dy * dy >= drag_slop_sq) { // 鼠标 5px / 触控 10px 拖拽启动阈值
                    DragPayload payload;
                    payload.source = m_drag_candidate;
                    DragEvent start_ev(DragEventType::DragStart, event.position, std::move(payload), event.modifiers);
                    start_ev.local_position = event.position - m_drag_candidate->global_offset();
                    m_drag_candidate->on_drag_start(start_ev);
                    if (start_ev.accepted || !start_ev.payload.empty()) {
                        m_is_dragging = true;
                        m_drag_source = m_drag_candidate;
                        m_drag_payload = std::move(start_ev.payload);
                        m_drag_payload.source = m_drag_source;
                        m_drop_target = nullptr;
                    } else {
                        m_drag_candidate = nullptr;
                    }
                }
            }
            if (m_is_dragging) {
                RenderBox* new_drop = find_drop_target(result);
                if (new_drop != m_drop_target) {
                    if (m_drop_target && m_drop_target->on_drag_leave) {
                        DragEvent leave_ev(DragEventType::DragLeave, event.position, m_drag_payload, event.modifiers);
                        leave_ev.local_position = event.position - m_drop_target->global_offset();
                        m_drop_target->on_drag_leave(leave_ev);
                    }
                    m_drop_target = new_drop;
                    if (m_drop_target && m_drop_target->on_drag_enter) {
                        DragEvent enter_ev(DragEventType::DragEnter, event.position, m_drag_payload, event.modifiers);
                        enter_ev.local_position = event.position - m_drop_target->global_offset();
                        m_drop_target->on_drag_enter(enter_ev);
                    }
                }
                if (m_drop_target && m_drop_target->on_drag_over) {
                    DragEvent over_ev(DragEventType::DragOver, event.position, m_drag_payload, event.modifiers);
                    over_ev.local_position = event.position - m_drop_target->global_offset();
                    m_drop_target->on_drag_over(over_ev);
                }
                return;
            }
        }

        if (event.type == PlatformEventType::PointerUp && m_is_dragging) {
            RenderBox* drop_node = find_drop_target(result);
            if (!drop_node) drop_node = m_drop_target;
            bool dropped = false;
            if (drop_node && drop_node->on_drop) {
                DragEvent drop_ev(DragEventType::Drop, event.position, m_drag_payload, event.modifiers);
                drop_ev.local_position = event.position - drop_node->global_offset();
                drop_node->on_drop(drop_ev);
                dropped = drop_ev.accepted;
            }
            if (m_drag_source && m_drag_source->on_drag_end) {
                DragEvent end_ev(DragEventType::DragEnd, event.position, m_drag_payload, event.modifiers);
                end_ev.local_position = event.position - m_drag_source->global_offset();
                end_ev.accepted = dropped;
                m_drag_source->on_drag_end(end_ev);
            }
            end_drag_session();
            if (m_active_target) {
                m_active_target->is_active = false;
                m_active_target = nullptr;
            }
            m_gesture_captured = false;
            return;
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
            m_drag_candidate = nullptr;
            m_gesture_captured = false;
        }
    }

    // 分发操作系统外部文件拖放事件 (Hit-Test 命中目标并沿渲染树冒泡至支持 on_drop 的节点)
    bool dispatch_file_drop(RenderBox* root, PointF position,
                            const std::vector<std::string>& files,
                            KeyModifiers mods = {}) {
        if (!root || files.empty()) return false;
        HitTestResult result;
        root->hit_test(result, position);

        DragPayload payload;
        payload.mime_type = "text/uri-list";
        payload.files = files;
        payload.source = nullptr;

        DragEvent ev(DragEventType::Drop, position, std::move(payload), mods);
        m_tree_mutated = false;
        for (const auto& entry : result.path()) {
            RenderBox* target = entry.target;
            if (!target || !target->on_drop) continue;
            ev.local_position = entry.local_position;
            target->on_drop(ev);
            if (ev.accepted || ev.is_propagation_stopped() || m_tree_mutated) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool is_dragging() const noexcept { return m_is_dragging; }
    [[nodiscard]] const DragPayload& active_drag_payload() const noexcept { return m_drag_payload; }

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
        if (m_drag_candidate == node) m_drag_candidate = nullptr;
        if (m_drag_source == node) m_drag_source = nullptr;
        if (m_drop_target == node) m_drop_target = nullptr;
    }

    // 取消当前正在进行的按压/拖拽交互（例如触控长按触发右键菜单或祖先抢占滚动时，彻底取消底层控件的点击态）
    void cancel_active_interaction() {
        if (m_active_target) {
            RenderBox* prev = m_active_target;
            m_active_target = nullptr;
            PointerEvent cancel_ev(PlatformEventType::PointerCancel, Vec2{-10000.0f, -10000.0f}, PointerButton::None);
            cancel_ev.device_kind = touch::InputModeTracker::current();
            dispatch_cancel_to_node(prev, cancel_ev);
        }
        end_drag_session();
        m_gesture_captured = false;
    }

private:
    static void dispatch_cancel_to_node(RenderBox* prev, const PointerEvent& src_ev) {
        if (!prev) return;
        prev->is_active = false;
        prev->is_hovered = false;
        PointerEvent cancel_ev = src_ev;
        cancel_ev.type = PlatformEventType::PointerCancel;
        cancel_ev.local_position = Vec2{-10000.0f, -10000.0f};
        if (prev->on_pointer_cancel) {
            prev->on_pointer_cancel(cancel_ev);
        } else if (prev->on_pointer_exit) {
            cancel_ev.type = PlatformEventType::PointerMove;
            prev->on_pointer_exit(cancel_ev);
        }
        prev->mark_needs_paint();
    }

public:

    void clear() {
        m_hovered_nodes.clear();
        m_active_target = nullptr;
        end_drag_session();
        m_gesture_captured = false;
    }

private:
    static RenderBox* find_drop_target(const HitTestResult& result) noexcept {
        for (const auto& entry : result.path()) {
            if (entry.target && (entry.target->is_drop_target || entry.target->on_drop || entry.target->on_drag_over)) {
                return entry.target;
            }
        }
        return nullptr;
    }

    void end_drag_session() noexcept {
        m_is_dragging = false;
        m_drag_candidate = nullptr;
        m_drag_source = nullptr;
        m_drop_target = nullptr;
        m_drag_payload = DragPayload{};
    }

    std::unordered_set<RenderBox*> m_hovered_nodes;
    RenderBox* m_active_target{nullptr};
    bool m_gesture_captured{false};
    bool m_tree_mutated{false};

    // 窗口内控件拖拽会话状态
    bool m_is_dragging{false};
    PointF m_drag_start_pos{0.0f, 0.0f};
    RenderBox* m_drag_candidate{nullptr};
    RenderBox* m_drag_source{nullptr};
    RenderBox* m_drop_target{nullptr};
    DragPayload m_drag_payload{};
};

} // namespace ktui
