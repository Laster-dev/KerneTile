#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/hit_test_tree.hpp>
#include <ktui/layout/box_constraints.hpp>
#include <memory>
#include <vector>
#include <functional>
#include <algorithm>

namespace ktui {

class Window;

// 浮层条目 (Overlay Entry)
// 脱离父级容器 hardware clip 限制，在最顶层独立布局、渲染与捕获交互
struct OverlayEntry {
    uint64_t id{0};
    std::shared_ptr<RenderBox> content{nullptr};
    Vec2 global_pos{0.0f, 0.0f};
    Vec2 size{0.0f, 0.0f};
    bool dismiss_on_outside_click{true};
    bool is_modal{false};
    bool allow_hit_passthrough{false}; // true 时若 content->hit_test 返回 false，则放行事件穿透到底层真实控件（如聚光灯镂空区）
    Color barrier_color{Color{0.0f, 0.0f, 0.0f, 0.50f}};
    Fn<void()> on_dismiss{nullptr};
};

// 浮层与弹出层管理器 (OverlayManager)
class OverlayManager {
public:
    OverlayManager() = default;
    ~OverlayManager() = default;

    // 显示一个浮层条目，返回其全局唯一 ID
    uint64_t show(OverlayEntry entry) {
        if (entry.id == 0) {
            entry.id = m_next_id++;
        }
        if (entry.content) {
            entry.content->set_offset(entry.global_pos);
        }
        m_entries.push_back(std::move(entry));
        return m_entries.back().id;
    }

    // 按 ID 关闭指定浮层
    void hide(uint64_t id) {
        if (m_captured_id == id) {
            m_captured_id = 0;
            m_captured_target = nullptr;
        }
        if (m_hovered_id == id) {
            m_hovered_id = 0;
            m_hovered_target = nullptr;
        }
        auto it = std::find_if(m_entries.begin(), m_entries.end(), [id](const OverlayEntry& e) {
            return e.id == id;
        });
        if (it != m_entries.end()) {
            auto cb = std::move(it->on_dismiss);
            m_entries.erase(it);
            if (cb) {
                cb();
            }
        }
    }

    // 关闭最顶层浮层
    void dismiss_top() {
        if (!m_entries.empty()) {
            auto top = std::move(m_entries.back());
            if (m_captured_id == top.id) {
                m_captured_id = 0;
                m_captured_target = nullptr;
            }
            if (m_hovered_id == top.id) {
                m_hovered_id = 0;
                m_hovered_target = nullptr;
            }
            m_entries.pop_back();
            if (top.on_dismiss) {
                top.on_dismiss();
            }
        }
    }

    // 关闭所有浮层
    void dismiss_all() {
        while (!m_entries.empty()) {
            dismiss_top();
        }
    }

    // 关闭所有非模态锚定浮层 (窗口缩放/父容器大幅滚动时调用，避免浮层悬空错位)
    void dismiss_non_modal() {
        std::vector<Fn<void()>> cbs;
        auto it = m_entries.begin();
        while (it != m_entries.end()) {
            if (!it->is_modal && it->dismiss_on_outside_click) {
                if (m_captured_id == it->id) {
                    m_captured_id = 0;
                    m_captured_target = nullptr;
                }
                if (m_hovered_id == it->id) {
                    m_hovered_id = 0;
                    m_hovered_target = nullptr;
                }
                if (it->on_dismiss) cbs.push_back(std::move(it->on_dismiss));
                it = m_entries.erase(it);
            } else {
                ++it;
            }
        }
        for (auto& cb : cbs) cb();
    }

    [[nodiscard]] bool has_active_overlay() const noexcept {
        return !m_entries.empty();
    }

    [[nodiscard]] bool has_modal_overlay() const noexcept {
        return std::any_of(m_entries.begin(), m_entries.end(), [](const OverlayEntry& e) {
            return e.is_modal;
        });
    }

    // 返回最顶层模态浮层的内容根节点 (用于 ContentDialog 等模态对话框的 Tab 焦点陷阱)
    [[nodiscard]] RenderBox* top_modal_content() const noexcept {
        for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
            if (it->is_modal && it->content) return it->content.get();
        }
        return nullptr;
    }

    [[nodiscard]] size_t overlay_count() const noexcept {
        return m_entries.size();
    }

    [[nodiscard]] const std::vector<OverlayEntry>& entries() const noexcept {
        return m_entries;
    }

    [[nodiscard]] bool needs_redraw() const noexcept {
        return std::any_of(m_entries.begin(), m_entries.end(), [](const OverlayEntry& entry) {
            const auto& content = entry.content;
            return content && (content->needs_paint() || content->needs_layout() || content->child_needs_layout());
        });
    }

    // 绘制所有激活的浮层（在主场景树绘制完成且弹出所有 clip 后在最顶层绘制）
    void layout_and_paint(PaintContext& ctx, Vec2 window_size) {
        if (m_entries.empty()) return;

        // 1. 若存在模态弹窗，先全屏绘制半透明阻断遮罩
        for (const auto& entry : m_entries) {
            if (entry.is_modal) {
                ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, window_size.x, window_size.y),
                              entry.barrier_color, 0.0f, 0.0f);
                break;
            }
        }

        // 2. 快照后依次在最顶层绘制各个浮层（防止 Toast 等在 paint 中调用 hide_overlay 导致迭代器失效与野指针）
        std::vector<OverlayEntry> snapshot = m_entries;
        for (auto& entry : snapshot) {
            if (!entry.content) continue;

            // 布局浮层内容
            BoxConstraints constraints = (entry.size.x > 0.0f && entry.size.y > 0.0f)
                ? BoxConstraints::tight(entry.size)
                : BoxConstraints::loose(window_size);
            entry.content->layout(constraints);
            entry.content->clear_dirty_recursive();

            // 更新浮层节点的全局绝对偏移
            entry.content->set_offset(entry.global_pos);

            // 绘制浮层（使用 OffsetScope 平移到浮层全局坐标）
            PaintContext::OffsetScope scope(ctx, entry.global_pos);
            entry.content->paint(ctx);
        }
    }

    // 处理鼠标按下事件（由 Window::handle_platform_event 优先调用）
    // 返回 true 表示事件被浮层或阻断遮罩消费
    bool handle_pointer_down(Vec2 pos, PointerButton button, PointerDeviceKind device_kind = PointerDeviceKind::Mouse) {
        if (m_entries.empty()) return false;

        // 逆序从最顶层浮层向下测试
        for (size_t i = m_entries.size(); i > 0; --i) {
            auto& entry = m_entries[i - 1];
            if (!entry.content) continue;

            Vec2 sz = (entry.size.x > 0.0f && entry.size.y > 0.0f) ? entry.size : entry.content->size();
            Rect entry_rect = Rect::from_xywh(entry.global_pos.x, entry.global_pos.y, sz.x, sz.y);

            if (entry_rect.contains(pos)) {
                auto content_guard = entry.content; // 持有强引用，防止回调内 hide_overlay 导致野指针
                bool passthrough = entry.allow_hit_passthrough;
                uint64_t eid = entry.id;
                HitTestResult hit_res;
                PointF local_p{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                if (content_guard->hit_test(hit_res, local_p)) {
                    m_captured_id = eid;
                    m_captured_target = nullptr;
                    PointerEvent ev(PlatformEventType::PointerDown, pos, button);
                    ev.device_kind = device_kind;
                    for (const auto& h : hit_res.path()) {
                        if (h.target && h.target->on_pointer_down) {
                            PointerEvent tev = ev;
                            tev.local_position = h.local_position;
                            m_captured_target = h.target;
                            h.target->on_pointer_down(tev);
                            if (m_captured_id != eid || tev.is_propagation_stopped()) break;
                        }
                    }
                    return true;
                }
                if (passthrough) continue;
                m_captured_id = eid;
                m_captured_target = nullptr;
                return true;
            } else {
                // 点击在浮层外部
                if (entry.dismiss_on_outside_click) {
                    uint64_t id_to_dismiss = entry.id;
                    bool was_modal = entry.is_modal;
                    hide(id_to_dismiss);
                    return was_modal; // 模态弹窗点击外部关闭时阻断底层点击穿透
                }
                if (entry.is_modal) {
                    return true; // 模态弹窗阻断外部点击
                }
            }
        }
        return false;
    }

    // 处理鼠标/触控移动事件
    bool handle_pointer_move(Vec2 pos, PointerDeviceKind device_kind = PointerDeviceKind::Mouse) {
        if (m_entries.empty()) return false;

        // 若当前存在拖拽捕获浮层（如拖拽浮层内的 Slider/滚动条滑块或触控拖拽），即使指针移出浮层边界仍持续转发 Move 给实际捕获节点
        if (m_captured_id != 0) {
            for (auto& entry : m_entries) {
                if (entry.id == m_captured_id && entry.content) {
                    auto content_guard = entry.content;
                    RenderBox* target = m_captured_target ? m_captured_target : content_guard.get();
                    if (target && target->on_pointer_move) {
                        PointerEvent ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                        ev.device_kind = device_kind;
                        Vec2 tg = target->global_offset();
                        ev.local_position = PointF{pos.x - tg.x, pos.y - tg.y};
                        target->on_pointer_move(ev);
                        return true;
                    }
                    if (content_guard->on_pointer_move) {
                        PointerEvent ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                        ev.device_kind = device_kind;
                        ev.local_position = PointF{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                        content_guard->on_pointer_move(ev);
                        return true;
                    }
                }
            }
        }

        for (size_t i = m_entries.size(); i > 0; --i) {
            auto& entry = m_entries[i - 1];
            if (!entry.content) continue;

            Vec2 sz = (entry.size.x > 0.0f && entry.size.y > 0.0f) ? entry.size : entry.content->size();
            Rect entry_rect = Rect::from_xywh(entry.global_pos.x, entry.global_pos.y, sz.x, sz.y);

            if (entry_rect.contains(pos)) {
                auto content_guard = entry.content;
                uint64_t eid = entry.id;
                HitTestResult hit_res;
                PointF local_p{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                if (content_guard->hit_test(hit_res, local_p)) {
                    // 维护浮层内深层子控件的 PointerEnter / PointerExit 悬停态
                    RenderBox* top_hit = hit_res.path().empty() ? nullptr : hit_res.path().front().target;
                    if (top_hit != m_hovered_target) {
                        if (m_hovered_target && m_hovered_target->on_pointer_exit) {
                            PointerEvent exit_ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                            m_hovered_target->on_pointer_exit(exit_ev);
                        }
                        m_hovered_id = eid;
                        m_hovered_target = top_hit;
                        if (m_hovered_target && m_hovered_target->on_pointer_enter) {
                            PointerEvent enter_ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                            enter_ev.local_position = hit_res.path().front().local_position;
                            m_hovered_target->on_pointer_enter(enter_ev);
                        }
                    }
                    PointerEvent ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                    ev.device_kind = device_kind;
                    for (const auto& h : hit_res.path()) {
                        if (h.target && h.target->on_pointer_move) {
                            PointerEvent tev = ev;
                            tev.local_position = h.local_position;
                            h.target->on_pointer_move(tev);
                            if (tev.is_propagation_stopped()) break;
                        }
                    }
                    return true;
                }
                if (entry.allow_hit_passthrough) continue;
                return true;
            } else {
                if (m_hovered_id == entry.id && m_hovered_target) {
                    RenderBox* prev = m_hovered_target;
                    m_hovered_id = 0;
                    m_hovered_target = nullptr;
                    if (prev->on_pointer_exit) {
                        PointerEvent exit_ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                        prev->on_pointer_exit(exit_ev);
                    }
                }
                if (entry.content->on_pointer_exit) {
                    PointerEvent exit_ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                    entry.content->on_pointer_exit(exit_ev);
                }
            }
            if (entry.is_modal) return true;
        }
        return false;
    }

    // 处理鼠标滚轮事件
    bool handle_pointer_scroll(Vec2 pos, Vec2 delta) {
        if (m_entries.empty()) return false;

        for (size_t i = m_entries.size(); i > 0; --i) {
            auto& entry = m_entries[i - 1];
            if (!entry.content) continue;

            Vec2 sz = (entry.size.x > 0.0f && entry.size.y > 0.0f) ? entry.size : entry.content->size();
            Rect entry_rect = Rect::from_xywh(entry.global_pos.x, entry.global_pos.y, sz.x, sz.y);

            if (entry_rect.contains(pos)) {
                HitTestResult hit_res;
                PointF local_p{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                if (entry.content->hit_test(hit_res, local_p)) {
                    PointerEvent ev(PlatformEventType::PointerScroll, pos, PointerButton::None);
                    ev.delta = delta;
                    for (const auto& h : hit_res.path()) {
                        if (h.target && h.target->on_pointer_move) {
                            PointerEvent tev = ev;
                            tev.local_position = h.local_position;
                            h.target->on_pointer_move(tev);
                            if (tev.is_propagation_stopped()) break;
                        }
                    }
                }
                return true;
            }
            if (entry.is_modal) return true;
        }
        return false;
    }

    // 处理鼠标释放事件
    bool handle_pointer_up(Vec2 pos, PointerButton button) {
        if (m_entries.empty()) return false;

        uint64_t captured = m_captured_id;
        RenderBox* captured_target = m_captured_target;
        m_captured_id = 0;
        m_captured_target = nullptr;

        for (size_t i = m_entries.size(); i > 0; --i) {
            auto& entry = m_entries[i - 1];
            if (!entry.content) continue;

            Vec2 sz = (entry.size.x > 0.0f && entry.size.y > 0.0f) ? entry.size : entry.content->size();
            Rect entry_rect = Rect::from_xywh(entry.global_pos.x, entry.global_pos.y, sz.x, sz.y);

            if (entry_rect.contains(pos) || entry.id == captured) {
                auto content_guard = entry.content; // 持有强引用，防止回调内 hide_overlay 导致野指针
                PointF local_p{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                bool passthrough = entry.allow_hit_passthrough && entry.id != captured;

                // 若按下时已捕获具体子控件（如 Slider / ToggleSwitch / Button），释放事件优先发回该捕获节点（支持拖出边界后释放）
                if (entry.id == captured && captured_target && captured_target != content_guard.get()) {
                    if (captured_target->on_pointer_up) {
                        PointerEvent ev(PlatformEventType::PointerUp, pos, button);
                        Vec2 tg = captured_target->global_offset();
                        ev.local_position = PointF{pos.x - tg.x, pos.y - tg.y};
                        captured_target->on_pointer_up(ev);
                        if (ev.is_propagation_stopped()) return true;
                    }
                }

                HitTestResult hit_res;
                if (entry_rect.contains(pos) && content_guard->hit_test(hit_res, local_p)) {
                    PointerEvent ev(PlatformEventType::PointerUp, pos, button);
                    for (const auto& h : hit_res.path()) {
                        if (h.target == captured_target && entry.id == captured && captured_target != content_guard.get()) {
                            continue; // 已在上方分发过，避免重复触发
                        }
                        if (h.target && h.target->on_pointer_up) {
                            PointerEvent tev = ev;
                            tev.local_position = h.local_position;
                            h.target->on_pointer_up(tev);
                            if (tev.is_propagation_stopped()) break;
                        }
                    }
                    return true;
                }
                if (passthrough) continue;
                if (content_guard->on_pointer_up) {
                    PointerEvent ev(PlatformEventType::PointerUp, pos, button);
                    ev.local_position = local_p;
                    content_guard->on_pointer_up(ev);
                }
                return true;
            }
            if (entry.is_modal) return true;
        }
        return false;
    }

private:
    std::vector<OverlayEntry> m_entries;
    uint64_t m_next_id{1};
    uint64_t m_captured_id{0};
    RenderBox* m_captured_target{nullptr};
    uint64_t m_hovered_id{0};
    RenderBox* m_hovered_target{nullptr};
};

} // namespace ktui
