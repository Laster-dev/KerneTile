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
    Color barrier_color{Color{0.0f, 0.0f, 0.0f, 0.50f}};
    std::function<void()> on_dismiss{nullptr};
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
        m_entries.push_back(std::move(entry));
        return m_entries.back().id;
    }

    // 按 ID 关闭指定浮层
    void hide(uint64_t id) {
        if (m_captured_id == id) {
            m_captured_id = 0;
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

    [[nodiscard]] bool has_active_overlay() const noexcept {
        return !m_entries.empty();
    }

    [[nodiscard]] bool has_modal_overlay() const noexcept {
        return std::any_of(m_entries.begin(), m_entries.end(), [](const OverlayEntry& e) {
            return e.is_modal;
        });
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

        // 2. 依次在最顶层绘制各个浮层
        for (auto& entry : m_entries) {
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
    bool handle_pointer_down(Vec2 pos, PointerButton button) {
        if (m_entries.empty()) return false;

        // 逆序从最顶层浮层向下测试
        for (size_t i = m_entries.size(); i > 0; --i) {
            auto& entry = m_entries[i - 1];
            if (!entry.content) continue;

            Vec2 sz = (entry.size.x > 0.0f && entry.size.y > 0.0f) ? entry.size : entry.content->size();
            Rect entry_rect = Rect::from_xywh(entry.global_pos.x, entry.global_pos.y, sz.x, sz.y);

            if (entry_rect.contains(pos)) {
                m_captured_id = entry.id;
                // 点击在浮层内部：进行命中测试并派发事件
                HitTestResult hit_res;
                PointF local_p{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                if (entry.content->hit_test(hit_res, local_p)) {
                    PointerEvent ev(PlatformEventType::PointerDown, pos, button);
                    for (const auto& h : hit_res.path()) {
                        if (h.target && h.target->on_pointer_down) {
                            PointerEvent tev = ev;
                            tev.local_position = h.local_position;
                            h.target->on_pointer_down(tev);
                            if (tev.is_propagation_stopped()) break;
                        }
                    }
                }
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

    // 处理鼠标移动事件
    bool handle_pointer_move(Vec2 pos) {
        if (m_entries.empty()) return false;

        // 若当前存在拖拽捕获浮层（如拖拽浮层内的滚动条滑块），即使指针移出浮层边界仍持续转发 Move
        if (m_captured_id != 0) {
            for (auto& entry : m_entries) {
                if (entry.id == m_captured_id && entry.content && entry.content->on_pointer_move) {
                    PointerEvent ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                    ev.local_position = PointF{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                    entry.content->on_pointer_move(ev);
                    return true;
                }
            }
        }

        for (size_t i = m_entries.size(); i > 0; --i) {
            auto& entry = m_entries[i - 1];
            if (!entry.content) continue;

            Vec2 sz = (entry.size.x > 0.0f && entry.size.y > 0.0f) ? entry.size : entry.content->size();
            Rect entry_rect = Rect::from_xywh(entry.global_pos.x, entry.global_pos.y, sz.x, sz.y);

            if (entry_rect.contains(pos)) {
                HitTestResult hit_res;
                PointF local_p{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                if (entry.content->hit_test(hit_res, local_p)) {
                    PointerEvent ev(PlatformEventType::PointerMove, pos, PointerButton::None);
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
            } else if (entry.content->on_pointer_exit) {
                PointerEvent exit_ev(PlatformEventType::PointerMove, pos, PointerButton::None);
                entry.content->on_pointer_exit(exit_ev);
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
        m_captured_id = 0;

        for (size_t i = m_entries.size(); i > 0; --i) {
            auto& entry = m_entries[i - 1];
            if (!entry.content) continue;

            Vec2 sz = (entry.size.x > 0.0f && entry.size.y > 0.0f) ? entry.size : entry.content->size();
            Rect entry_rect = Rect::from_xywh(entry.global_pos.x, entry.global_pos.y, sz.x, sz.y);

            if (entry_rect.contains(pos) || entry.id == captured) {
                PointF local_p{pos.x - entry.global_pos.x, pos.y - entry.global_pos.y};
                HitTestResult hit_res;
                if (entry_rect.contains(pos) && entry.content->hit_test(hit_res, local_p)) {
                    PointerEvent ev(PlatformEventType::PointerUp, pos, button);
                    for (const auto& h : hit_res.path()) {
                        if (h.target && h.target->on_pointer_up) {
                            PointerEvent tev = ev;
                            tev.local_position = h.local_position;
                            h.target->on_pointer_up(tev);
                            if (tev.is_propagation_stopped()) break;
                        }
                    }
                } else if (entry.content->on_pointer_up) {
                    PointerEvent ev(PlatformEventType::PointerUp, pos, button);
                    ev.local_position = local_p;
                    entry.content->on_pointer_up(ev);
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
};

} // namespace ktui
