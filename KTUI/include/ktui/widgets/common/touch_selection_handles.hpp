#pragma once

#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace ktui {

enum class TouchHandleKind : uint8_t {
    None = 0,
    Start = 1,
    End = 2,
    Caret = 3
};

struct TouchSelectionHandlesState {
    bool active{false};
    bool has_selection{false};
    bool show_start{false};
    bool show_end{false};
    bool show_caret{false};
    PointF start_top{};
    PointF start_bottom{};
    PointF end_top{};
    PointF end_bottom{};
    PointF caret_top{};
    PointF caret_bottom{};
};

// 判断控件内局部点是否处于祖先容器（如 ScrollView）与窗口可见区域内
inline bool is_local_point_visible_in_ancestors(const RenderBox* node, PointF local_pt) noexcept {
    if (!node) return false;
    PointF pt = local_pt;
    const RenderBox* cur = node;
    while (cur && cur->parent()) {
        RenderBox* p = cur->parent()->as_render_box();
        if (!p) break;
        pt = PointF{pt.x + cur->offset().x, pt.y + cur->offset().y};
        const Vec2 psz = p->size();
        if (psz.x > 0.0f && psz.y > 0.0f) {
            if (pt.x < -4.0f || pt.x > psz.x + 4.0f || pt.y < -4.0f || pt.y > psz.y + 4.0f) {
                return false;
            }
        }
        cur = p;
    }
    return true;
}

// 顶层透明穿透浮层：绘制与拖拽交互水滴形触控选区/光标调整手柄 (Teardrop Touch Handles)
class RenderTouchSelectionHandles : public RenderBox {
public:
    static constexpr float kHandleSize = 16.0f;
    static constexpr float kDragHandleSize = 18.0f;
    static constexpr float kHitRadius = 22.0f; // 44x44px 触控最小命中区

    static constexpr const char* kCaretTeardropSvg =
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
        "<path fill='#FFFFFF' d='M12 1.5 C15.2 6.0 20 10.2 20 14.5 C20 18.92 16.42 22.5 12 22.5 "
        "C7.58 22.5 4 18.92 4 14.5 C4 10.2 8.8 6.0 12 1.5 Z'/>"
        "</svg>";

    RenderTouchSelectionHandles(std::shared_ptr<bool> owner_alive,
                                Fn<TouchSelectionHandlesState()> query,
                                Fn<void(TouchHandleKind, PointF)> on_drag_begin,
                                Fn<void(TouchHandleKind, PointF)> on_drag_update,
                                Fn<void(TouchHandleKind)> on_drag_end)
        : m_owner_alive(std::move(owner_alive)),
          m_query(std::move(query)),
          m_on_drag_begin(std::move(on_drag_begin)),
          m_on_drag_update(std::move(on_drag_update)),
          m_on_drag_end(std::move(on_drag_end)) {
        setup_behavior();
    }

    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 0.0f;
        float h = std::isfinite(m_constraints.max_height) ? m_constraints.max_height : 0.0f;
        set_size(Vec2{w, h});
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        return hit_test_internal(result, local_position);
    }

    bool hit_test_touch(HitTestResult& result, PointF local_position) override {
        return hit_test_internal(result, local_position);
    }

    void paint(PaintContext& ctx) override {
        const TouchSelectionHandlesState st = query_safe();
        if (!st.active) return;

        const Theme& theme = ThemeContext::current();
        const Color accent = theme.is_dark
            ? Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}
            : Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
        const Color rim = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.36f}
            : Color{1.0f, 1.0f, 1.0f, 0.90f};

        if (st.has_selection) {
            if (st.show_start) {
                paint_side_teardrop(ctx, st.start_top, st.start_bottom, true,
                                    m_dragging == TouchHandleKind::Start, accent, rim, theme.is_dark);
            }
            if (st.show_end) {
                paint_side_teardrop(ctx, st.end_top, st.end_bottom, false,
                                    m_dragging == TouchHandleKind::End, accent, rim, theme.is_dark);
            }
        } else if (st.show_caret) {
            paint_caret_teardrop(ctx, st.caret_bottom, m_dragging == TouchHandleKind::Caret, accent, theme.is_dark);
        }
    }

    [[nodiscard]] bool is_dragging() const noexcept { return m_dragging != TouchHandleKind::None; }

private:
    [[nodiscard]] TouchSelectionHandlesState query_safe() const {
        if (!m_owner_alive || !(*m_owner_alive) || !m_query) return {};
        return m_query();
    }

    [[nodiscard]] static Rect handle_visual_rect(TouchHandleKind kind, const TouchSelectionHandlesState& st, bool dragging) noexcept {
        const float d = dragging ? kDragHandleSize : kHandleSize;
        if (kind == TouchHandleKind::Start) {
            return Rect::from_xywh(st.start_bottom.x - d, st.start_bottom.y, d, d);
        }
        if (kind == TouchHandleKind::End) {
            return Rect::from_xywh(st.end_bottom.x, st.end_bottom.y, d, d);
        }
        if (kind == TouchHandleKind::Caret) {
            return Rect::from_xywh(st.caret_bottom.x - d * 0.5f, st.caret_bottom.y, d, d * 1.15f);
        }
        return {};
    }

    [[nodiscard]] static PointF handle_hit_center(TouchHandleKind kind, const TouchSelectionHandlesState& st) noexcept {
        const float half = kHandleSize * 0.5f;
        if (kind == TouchHandleKind::Start) {
            return PointF{st.start_bottom.x - half, st.start_bottom.y + 14.0f};
        }
        if (kind == TouchHandleKind::End) {
            return PointF{st.end_bottom.x + half, st.end_bottom.y + 14.0f};
        }
        if (kind == TouchHandleKind::Caret) {
            return PointF{st.caret_bottom.x, st.caret_bottom.y + 14.0f};
        }
        return {};
    }

    [[nodiscard]] static float handle_anchor_bottom_y(TouchHandleKind kind, const TouchSelectionHandlesState& st) noexcept {
        if (kind == TouchHandleKind::Start) return st.start_bottom.y;
        if (kind == TouchHandleKind::End) return st.end_bottom.y;
        if (kind == TouchHandleKind::Caret) return st.caret_bottom.y;
        return 0.0f;
    }

    [[nodiscard]] TouchHandleKind pick_handle(PointF p, const TouchSelectionHandlesState& st) const noexcept {
        if (!st.active) return TouchHandleKind::None;
        const float r2 = kHitRadius * kHitRadius;

        if (st.has_selection) {
            TouchHandleKind best = TouchHandleKind::None;
            float best_d2 = r2 + 1.0f;
            auto consider = [&](TouchHandleKind k, bool visible) {
                if (!visible) return;
                // 严禁水滴热区向上侵占正文行，避免拦截双击选词/重定位点击
                if (p.y < handle_anchor_bottom_y(k, st) - 2.0f) return;
                PointF c = handle_hit_center(k, st);
                float dx = p.x - c.x;
                float dy = p.y - c.y;
                float d2 = dx * dx + dy * dy;
                if (d2 <= r2 && d2 < best_d2) {
                    best_d2 = d2;
                    best = k;
                }
            };
            consider(TouchHandleKind::Start, st.show_start);
            consider(TouchHandleKind::End, st.show_end);
            return best;
        }

        if (st.show_caret) {
            if (p.y < st.caret_bottom.y - 2.0f) return TouchHandleKind::None;
            PointF c = handle_hit_center(TouchHandleKind::Caret, st);
            float dx = p.x - c.x;
            float dy = p.y - c.y;
            if (dx * dx + dy * dy <= r2) {
                return TouchHandleKind::Caret;
            }
        }
        return TouchHandleKind::None;
    }

    bool hit_test_internal(HitTestResult& result, PointF local_position) {
        const TouchSelectionHandlesState st = query_safe();
        if (m_dragging != TouchHandleKind::None || pick_handle(local_position, st) != TouchHandleKind::None) {
            result.add(this, local_position);
            return true;
        }
        return false;
    }

    static void paint_side_teardrop(PaintContext& ctx, PointF top, PointF bottom, bool is_start,
                                    bool dragging, Color accent, Color rim, bool is_dark) {
        const float d = dragging ? kDragHandleSize : kHandleSize;
        const float rad = d * 0.5f;
        const float ax = std::round(bottom.x);
        const float ay = std::round(bottom.y);
        const float ty = std::round(top.y);

        // 连接选区边缘的 2px 细竖杆
        if (ay > ty) {
            ctx.draw_rect(Rect::from_xywh(ax - 1.0f, ty, 2.0f, (ay - ty) + 1.5f), accent, 1.0f, 0.0f);
        }

        // 水滴形主体：圆润球体 + 指向选区端点的锐利尖角 (左上或右上 1.5px 小圆角)
        Rect drop = is_start
            ? Rect::from_xywh(ax - d, ay, d, d)
            : Rect::from_xywh(ax, ay, d, d);
        ctx.draw_elevation_shadow(drop, rad, is_dark, dragging ? 10.0f : 6.0f, 0.85f);
        if (is_start) {
            ctx.draw_rounded_rect(drop, accent, rad, 1.5f, rad, rad);
            ctx.draw_rounded_rect(drop, rim, rad, 1.5f, rad, rad, 0.0f, Vec2{0.0f, 0.0f}, 1.0f);
        } else {
            ctx.draw_rounded_rect(drop, accent, 1.5f, rad, rad, rad);
            ctx.draw_rounded_rect(drop, rim, 1.5f, rad, rad, rad, 0.0f, Vec2{0.0f, 0.0f}, 1.0f);
        }
    }

    static void paint_caret_teardrop(PaintContext& ctx, PointF bottom, bool dragging, Color accent, bool is_dark) {
        const float d = dragging ? kDragHandleSize : kHandleSize;
        const float h = std::round(d * 1.15f);
        const float cx = std::round(bottom.x);
        const float cy = std::round(bottom.y);
        Rect bulb_shadow = Rect::from_xywh(cx - d * 0.42f, cy + h * 0.25f, d * 0.84f, d * 0.84f);
        ctx.draw_elevation_shadow(bulb_shadow, d * 0.42f, is_dark, dragging ? 9.0f : 5.0f, 0.75f);
        Rect svg_r = Rect::from_xywh(std::round(cx - d * 0.5f), cy, d, h);
        SvgEngine::draw_svg_xml(ctx, kCaretTeardropSvg, svg_r, accent);
    }

    void setup_behavior() {
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            const TouchSelectionHandlesState st = query_safe();
            TouchHandleKind hit = pick_handle(ev.local_position, st);
            if (hit == TouchHandleKind::None) return;

            m_dragging = hit;
            PointF anchor_mid{};
            if (hit == TouchHandleKind::Start) {
                anchor_mid = PointF{st.start_bottom.x, (st.start_top.y + st.start_bottom.y) * 0.5f};
            } else if (hit == TouchHandleKind::End) {
                anchor_mid = PointF{st.end_bottom.x, (st.end_top.y + st.end_bottom.y) * 0.5f};
            } else {
                anchor_mid = PointF{st.caret_bottom.x, (st.caret_top.y + st.caret_bottom.y) * 0.5f};
            }
            m_grab_offset = PointF{ev.position.x - anchor_mid.x, ev.position.y - anchor_mid.y};
            if (m_owner_alive && *m_owner_alive && m_on_drag_begin) {
                m_on_drag_begin(hit, anchor_mid);
            }
            mark_needs_paint();
            ev.stop_propagation();
        };

        on_pointer_move = [this](PointerEvent& ev) {
            if (m_dragging == TouchHandleKind::None) return;
            if (!m_owner_alive || !(*m_owner_alive)) {
                m_dragging = TouchHandleKind::None;
                return;
            }
            PointF target{ev.position.x - m_grab_offset.x, ev.position.y - m_grab_offset.y};
            if (m_on_drag_update) {
                m_on_drag_update(m_dragging, target);
            }
            mark_needs_paint();
            ev.stop_propagation();
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (m_dragging == TouchHandleKind::None) return;
            TouchHandleKind finished = m_dragging;
            m_dragging = TouchHandleKind::None;
            if (m_owner_alive && *m_owner_alive && m_on_drag_end) {
                m_on_drag_end(finished);
            }
            mark_needs_paint();
            ev.stop_propagation();
        };

        on_pointer_cancel = [this](PointerEvent&) {
            if (m_dragging == TouchHandleKind::None) return;
            TouchHandleKind finished = m_dragging;
            m_dragging = TouchHandleKind::None;
            if (m_owner_alive && *m_owner_alive && m_on_drag_end) {
                m_on_drag_end(finished);
            }
            mark_needs_paint();
        };
    }

    std::shared_ptr<bool> m_owner_alive;
    Fn<TouchSelectionHandlesState()> m_query;
    Fn<void(TouchHandleKind, PointF)> m_on_drag_begin;
    Fn<void(TouchHandleKind, PointF)> m_on_drag_update;
    Fn<void(TouchHandleKind)> m_on_drag_end;
    TouchHandleKind m_dragging{TouchHandleKind::None};
    PointF m_grab_offset{0.0f, 0.0f};
};

// 生命周期托管器：供 RenderTextBox / RenderTextArea 自动挂载、刷新与销毁水滴形触控手柄浮层
class TouchSelectionHandleController {
public:
    TouchSelectionHandleController() = default;
    ~TouchSelectionHandleController() {
        hide();
    }

    TouchSelectionHandleController(const TouchSelectionHandleController&) = delete;
    TouchSelectionHandleController& operator=(const TouchSelectionHandleController&) = delete;

    void ensure_shown(Fn<TouchSelectionHandlesState()> query,
                      Fn<void(TouchHandleKind, PointF)> on_drag_begin,
                      Fn<void(TouchHandleKind, PointF)> on_drag_update,
                      Fn<void(TouchHandleKind)> on_drag_end) {
        Window* win = Window::active_window();
        if (!win) return;
        if (m_overlay_id != 0) {
            if (m_node) m_node->mark_needs_paint();
            return;
        }
        m_alive = std::make_shared<bool>(true);
        m_node = make_render_node<RenderTouchSelectionHandles>(
            m_alive, std::move(query),
            std::move(on_drag_begin), std::move(on_drag_update), std::move(on_drag_end));
        m_node->set_size(win->size());

        OverlayEntry entry;
        entry.content = m_node;
        entry.global_pos = Vec2{0.0f, 0.0f};
        entry.size = Vec2{0.0f, 0.0f};
        entry.dismiss_on_outside_click = false;
        entry.is_modal = false;
        entry.allow_hit_passthrough = true;
        auto alive = m_alive;
        entry.on_dismiss = [this, alive]() {
            if (alive && *alive) {
                m_overlay_id = 0;
                m_node.reset();
            }
        };
        m_overlay_id = win->show_overlay(std::move(entry));
    }

    void hide() {
        if (m_alive) {
            *m_alive = false;
            m_alive.reset();
        }
        uint64_t id = m_overlay_id;
        m_overlay_id = 0;
        m_node.reset();
        if (id != 0) {
            if (Window* win = Window::active_window()) {
                win->hide_overlay(id);
            }
        }
    }

    void mark_dirty() {
        if (m_node) m_node->mark_needs_paint();
    }

    [[nodiscard]] bool is_shown() const noexcept { return m_overlay_id != 0; }
    [[nodiscard]] bool is_dragging() const noexcept { return m_node && m_node->is_dragging(); }

private:
    uint64_t m_overlay_id{0};
    std::shared_ptr<bool> m_alive;
    std::shared_ptr<RenderTouchSelectionHandles> m_node;
};

} // namespace ktui
