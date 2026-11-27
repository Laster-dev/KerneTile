#pragma once

#include "toast_data.hpp"
#include "toast_style.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

// WinUI 3 / Windows 11 悬浮通知吐司 (RenderToast)
// 具备原生高斯软阴影、滑入/滑出动效、底部倒计时进度条 (悬停自动暂停)、操作按钮与多条自动堆叠
class RenderToast : public RenderBox {
public:
    RenderToast(ToastDesc desc, Fn<void()> on_finish_close)
        : m_desc(std::move(desc)), m_on_finish_close(std::move(on_finish_close)) {
        m_last_tick = std::chrono::steady_clock::now();
        m_open_start = m_last_tick;
        setup_behavior();
    }

    ~RenderToast() override = default;

    [[nodiscard]] Vec2 compute_size() const {
        float w = std::clamp(m_desc.width > 0.0f ? m_desc.width : 340.0f, 260.0f, 460.0f);
        bool has_both = !m_desc.title.empty() && !m_desc.message.empty();
        float h = has_both ? 70.0f : 52.0f;
        if (!m_desc.action_text.empty()) h += 30.0f;
        return Vec2{w, h};
    }

    void set_overlay_tracking(uint64_t overlay_id, Vec2 initial_pos, Fn<void()> on_begin_dismiss) {
        m_overlay_id = overlay_id;
        m_current_x = initial_pos.x;
        m_current_y = initial_pos.y;
        m_target_y = initial_pos.y;
        m_on_begin_dismiss = std::move(on_begin_dismiss);
    }

    void set_target_y(float target_y) noexcept {
        if (std::abs(m_target_y - target_y) > 0.25f) {
            m_target_y = target_y;
            m_pos_animating = true;
            mark_needs_paint();
        }
    }

    void begin_dismiss() {
        if (m_closing) return;
        m_closing = true;
        m_close_start = std::chrono::steady_clock::now();
        if (auto cb = std::move(m_on_begin_dismiss)) {
            cb();
        }
        mark_needs_paint();
    }

    void perform_layout() override {
        set_size(compute_size());
    }

    void paint(PaintContext& ctx) override {
        auto now = std::chrono::steady_clock::now();
        float dt = std::clamp(std::chrono::duration<float>(now - m_last_tick).count(), 0.0f, 0.05f);
        m_last_tick = now;

        float alpha = 1.0f;
        float slide_x = 0.0f;
        float slide_y = 0.0f;

        if (m_closing) {
            float t = std::clamp(std::chrono::duration<float>(now - m_close_start).count() / 0.16f, 0.0f, 1.0f);
            float accel = t * t;
            alpha = 1.0f - t;
            if (m_desc.placement == ToastPlacement::BottomCenter) slide_y = 16.0f * accel;
            else slide_x = 28.0f * accel;
            if (t < 1.0f) {
                mark_needs_paint();
            } else if (!m_close_dispatched) {
                m_close_dispatched = true;
                if (m_on_finish_close) m_on_finish_close();
                return;
            }
        } else {
            float t = std::clamp(std::chrono::duration<float>(now - m_open_start).count() / 0.22f, 0.0f, 1.0f);
            float ease = 1.0f - std::pow(1.0f - t, 3.0f);
            alpha = std::clamp(t * 1.4f, 0.0f, 1.0f);
            if (m_desc.placement == ToastPlacement::BottomCenter) slide_y = (1.0f - ease) * 18.0f;
            else slide_x = (1.0f - ease) * 32.0f;
            if (t < 1.0f) mark_needs_paint();

            // 自动倒计时 (悬停时暂停，鼠标离开继续)
            if (m_desc.duration_sec > 0.0f) {
                if (!m_hovered) {
                    m_elapsed_life += dt;
                    if (m_elapsed_life >= m_desc.duration_sec) {
                        begin_dismiss();
                    }
                }
                mark_needs_paint();
            }
        }

        if (m_pos_animating) {
            float diff = m_target_y - m_current_y;
            if (std::abs(diff) < 0.35f) {
                m_current_y = m_target_y;
                m_pos_animating = false;
            } else {
                float k = 1.0f - std::exp(-dt * 16.0f);
                m_current_y += diff * k;
                mark_needs_paint();
            }
            if (Window* w = Window::active_window(); w && m_overlay_id != 0) {
                w->set_overlay_position(m_overlay_id, Vec2{std::round(m_current_x), std::round(m_current_y)});
            }
        }

        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;
        Rect panel = Rect::from_xywh(slide_x, slide_y, w, h);

        // 超高模糊半透磨砂亚克力底板 (对齐 Fluent 2 Acrylic Flyout)
        Color bg = theme.is_dark
            ? Color{36.0f / 255.0f, 38.0f / 255.0f, 44.0f / 255.0f, 0.78f * alpha}
            : Color{248.0f / 255.0f, 249.0f / 255.0f, 252.0f / 255.0f, 0.82f * alpha};
        Color border = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.14f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.12f * alpha};
        Color title_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.96f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.92f * alpha};
        Color msg_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.78f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.70f * alpha};
        Color accent = severity_color(theme.is_dark);
        accent.a *= alpha;

        // 1. WinUI 3 Flyout 32dp 双层真实高斯软阴影
        ctx.draw_elevation_shadow(panel, 8.0f, theme.is_dark, 32.0f, alpha);

        // 2. 超高模糊半透亚克力卡片底板 + 细微噪点 + 方向性玻璃高光描边
        ctx.draw_acrylic_panel(panel, bg, 8.0f, 0.026f, 1.0f);
        ctx.draw_rect(panel, border, 8.0f, 1.0f);
        {
            const Color top_hi = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.24f * alpha} : Color{1.0f, 1.0f, 1.0f, 0.72f * alpha};
            const Color bot_sh = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.30f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.12f * alpha};
            ctx.draw_directional_rim(panel, top_hi, bot_sh, 8.0f, 1.0f, Vec2{-0.18f, -0.98f}, 0.28f);
        }

        // 3. 左侧语义徽标圆点 + 图标
        Rect badge_r = Rect::from_xywh(std::round(panel.min_x + 14.0f), std::round(panel.min_y + 15.0f), 18.0f, 18.0f);
        ctx.draw_rect(badge_r, accent, 9.0f, 0.0f);
        Color glyph_fg = theme.is_dark ? Color{0.06f, 0.08f, 0.12f, alpha} : Color{1.0f, 1.0f, 1.0f, alpha};
        paint_severity_glyph(ctx, badge_r, glyph_fg);

        // 4. 标题与正文
        float text_x = panel.min_x + 42.0f;
        float text_w = std::max(40.0f, w - 42.0f - 36.0f);
        ctx.push_clip(Rect::from_xywh(text_x, panel.min_y + 8.0f, text_w, h - 16.0f), 0.0f);
        float cur_y = panel.min_y + 13.0f;
        if (!m_desc.title.empty()) {
            ctx.draw_text(m_desc.title, PointF{std::round(text_x), std::round(cur_y)}, 13.5f, title_col);
            cur_y += 20.0f;
        }
        if (!m_desc.message.empty()) {
            ctx.draw_text(m_desc.message, PointF{std::round(text_x), std::round(cur_y)}, 12.5f, msg_col);
            cur_y += 20.0f;
        }
        ctx.pop_clip();

        // 5. 可选操作按钮
        m_action_rect = Rect::from_xywh(0, 0, 0, 0);
        if (!m_desc.action_text.empty()) {
            Vec2 asz = TextMeasure::measure(m_desc.action_text, 12.0f);
            float bw = std::ceil(asz.x + 18.0f);
            float bh = 24.0f;
            m_action_rect = Rect::from_xywh(std::round(text_x), std::round(panel.max_y - bh - 10.0f), bw, bh);
            Color abg = m_action_hovered
                ? (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.12f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.08f * alpha})
                : (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.07f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.045f * alpha});
            ctx.draw_rect(m_action_rect, abg, 4.0f, 0.0f);
            ctx.draw_rect(m_action_rect, border, 4.0f, 1.0f);
            ctx.draw_text(m_desc.action_text,
                          PointF{std::round(m_action_rect.min_x + (bw - asz.x) * 0.5f),
                                 std::round(m_action_rect.min_y + (bh - asz.y) * 0.5f)},
                          12.0f, title_col);
        }

        // 6. 右上角关闭按钮 (×)
        m_close_rect = Rect::from_xywh(panel.max_x - 32.0f, panel.min_y + 10.0f, 24.0f, 24.0f);
        if (m_close_hovered) {
            Color cbg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.05f * alpha};
            ctx.draw_rect(m_close_rect, cbg, 4.0f, 0.0f);
        }
        Color xc = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.72f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.64f * alpha};
        float xcx = m_close_rect.min_x + 12.0f;
        float xcy = m_close_rect.min_y + 12.0f;
        ctx.draw_line(PointF{xcx - 4.0f, xcy - 4.0f}, PointF{xcx + 4.0f, xcy + 4.0f}, xc, 1.4f);
        ctx.draw_line(PointF{xcx + 4.0f, xcy - 4.0f}, PointF{xcx - 4.0f, xcy + 4.0f}, xc, 1.4f);

        // 7. 底部倒计时细进度条
        if (m_desc.duration_sec > 0.0f && !m_closing) {
            float remain = std::clamp(1.0f - m_elapsed_life / m_desc.duration_sec, 0.0f, 1.0f);
            float bar_w = std::max(0.0f, (w - 16.0f) * remain);
            Color bar_col = accent;
            bar_col.a *= 0.85f;
            ctx.draw_rect(Rect::from_xywh(panel.min_x + 8.0f, panel.max_y - 3.0f, bar_w, 2.0f), bar_col, 1.0f, 0.0f);
        }
    }

private:
    [[nodiscard]] Color severity_color(bool is_dark) const noexcept {
        if (is_dark) {
            switch (m_desc.severity) {
                case ToastSeverity::Success: return Color{108.0f / 255.0f, 203.0f / 255.0f, 95.0f / 255.0f, 1.0f};
                case ToastSeverity::Warning: return Color{252.0f / 255.0f, 225.0f / 255.0f, 0.0f, 1.0f};
                case ToastSeverity::Error:   return Color{255.0f / 255.0f, 153.0f / 255.0f, 164.0f / 255.0f, 1.0f};
                default:                     return Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};
            }
        }
        switch (m_desc.severity) {
            case ToastSeverity::Success: return Color{15.0f / 255.0f, 123.0f / 255.0f, 15.0f / 255.0f, 1.0f};
            case ToastSeverity::Warning: return Color{157.0f / 255.0f, 93.0f / 255.0f, 0.0f, 1.0f};
            case ToastSeverity::Error:   return Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 1.0f};
            default:                     return Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
        }
    }

    void paint_severity_glyph(PaintContext& ctx, Rect r, Color col) const {
        float cx = r.min_x + r.width() * 0.5f;
        float cy = r.min_y + r.height() * 0.5f;
        switch (m_desc.severity) {
            case ToastSeverity::Success:
                ctx.draw_line(PointF{cx - 3.5f, cy + 0.2f}, PointF{cx - 0.8f, cy + 3.0f}, col, 1.6f);
                ctx.draw_line(PointF{cx - 0.8f, cy + 3.0f}, PointF{cx + 4.0f, cy - 2.8f}, col, 1.6f);
                break;
            case ToastSeverity::Warning:
                ctx.draw_line(PointF{cx, cy - 3.8f}, PointF{cx, cy + 1.0f}, col, 1.6f);
                ctx.draw_rect(Rect::from_xywh(cx - 0.9f, cy + 2.6f, 1.8f, 1.8f), col, 0.9f, 0.0f);
                break;
            case ToastSeverity::Error:
                ctx.draw_line(PointF{cx - 3.0f, cy - 3.0f}, PointF{cx + 3.0f, cy + 3.0f}, col, 1.6f);
                ctx.draw_line(PointF{cx + 3.0f, cy - 3.0f}, PointF{cx - 3.0f, cy + 3.0f}, col, 1.6f);
                break;
            default:
                ctx.draw_rect(Rect::from_xywh(cx - 0.9f, cy - 4.0f, 1.8f, 1.8f), col, 0.9f, 0.0f);
                ctx.draw_line(PointF{cx, cy - 1.0f}, PointF{cx, cy + 4.0f}, col, 1.6f);
                break;
        }
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            if (e.is_touch()) {
                if (m_touch_swiping) {
                    m_touch_dx = e.local_position.x - m_touch_start.x;
                    m_touch_dy = e.local_position.y - m_touch_start.y;
                    mark_needs_paint();
                }
                e.stop_propagation();
                return;
            }
            m_hovered = true;
            bool ch = m_close_rect.contains(e.local_position);
            bool ah = m_action_rect.width() > 0.0f && m_action_rect.contains(e.local_position);
            if (ch != m_close_hovered || ah != m_action_hovered) {
                m_close_hovered = ch;
                m_action_hovered = ah;
                mark_needs_paint();
            }
            e.stop_propagation();
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_close_hovered = false;
            m_action_hovered = false;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            Rect close_hit = e.is_touch() ? touch::expand_hit_rect(m_close_rect, 40.0f) : m_close_rect;
            Rect act_hit = (m_action_rect.width() > 0.0f)
                ? (e.is_touch() ? touch::expand_hit_rect(m_action_rect, 40.0f) : m_action_rect)
                : Rect{};
            if (close_hit.contains(e.local_position)) {
                begin_dismiss();
                e.stop_propagation();
                return;
            } else if (act_hit.width() > 0.0f && act_hit.contains(e.local_position)) {
                if (m_desc.on_action) m_desc.on_action();
                begin_dismiss();
                e.stop_propagation();
                return;
            }
            if (e.is_touch()) {
                m_hovered = true; // 按住暂停倒计时
                m_touch_swiping = true;
                m_touch_start = e.local_position;
                m_touch_dx = 0.0f;
                m_touch_dy = 0.0f;
            }
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (e.is_touch() && m_touch_swiping) {
                m_touch_swiping = false;
                m_hovered = false;
                float dx = e.local_position.x - m_touch_start.x;
                float dy = e.local_position.y - m_touch_start.y;
                m_touch_dx = 0.0f;
                m_touch_dy = 0.0f;
                bool dismiss = (m_desc.placement == ToastPlacement::BottomCenter)
                    ? (dy > 36.0f)
                    : (std::abs(dx) > size().x * 0.35f);
                if (dismiss) {
                    begin_dismiss();
                } else {
                    mark_needs_paint();
                }
                e.stop_propagation();
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_touch_swiping = false;
            m_touch_dx = 0.0f;
            m_touch_dy = 0.0f;
            m_hovered = false;
            m_close_hovered = false;
            m_action_hovered = false;
            mark_needs_paint();
        };
    }

    ToastDesc m_desc;
    bool m_touch_swiping{false};
    PointF m_touch_start{};
    float m_touch_dx{0.0f};
    float m_touch_dy{0.0f};
    Fn<void()> m_on_finish_close;
    Fn<void()> m_on_begin_dismiss;
    uint64_t m_overlay_id{0};
    float m_current_x{0.0f};
    float m_current_y{0.0f};
    float m_target_y{0.0f};
    bool m_pos_animating{false};
    Rect m_close_rect;
    Rect m_action_rect;
    bool m_hovered{false};
    bool m_close_hovered{false};
    bool m_action_hovered{false};
    bool m_closing{false};
    bool m_close_dispatched{false};
    float m_elapsed_life{0.0f};
    std::chrono::steady_clock::time_point m_open_start{};
    std::chrono::steady_clock::time_point m_close_start{};
    std::chrono::steady_clock::time_point m_last_tick{};
};

// 静态便捷弹出器 (Toast::show)，支持多条通知自动垂直错位堆叠与关闭后平滑沉降补位动画
struct Toast {
    static void reflow_placement(ToastPlacement placement) {
        Window* win = Window::active_window();
        if (!win) return;
        const Vec2 win_sz = win->size();
        const float margin = 16.0f;
        const float tb = win->titlebar_height();
        float accumulated_h = 0.0f;
        auto& slots = live_slots();
        for (const auto& s : slots) {
            if (s.placement != placement || s.closing) continue;
            if (auto node = s.node.lock()) {
                float target_y = (placement == ToastPlacement::TopRight)
                    ? (tb + margin + accumulated_h)
                    : std::max(tb + margin, win_sz.y - s.height - margin - accumulated_h);
                node->set_target_y(std::round(target_y));
                accumulated_h += s.height + 10.0f;
            }
        }
    }

    static uint64_t show(const ToastDesc& desc) {
        Window* win = Window::active_window();
        if (!win) return 0;

        auto overlay_id = std::make_shared<uint64_t>(0);
        auto on_close = desc.on_close;
        const ToastPlacement placement = desc.placement;
        auto& active_slots = live_slots();

        // 按同方位未关闭 Toast 的实际高度逐条累加偏移，彻底避免不同高度堆叠重叠
        float accumulated_h = 0.0f;
        int same_place_cnt = 0;
        for (const auto& s : active_slots) {
            if (s.placement == placement && !s.closing) {
                accumulated_h += s.height + 10.0f;
                ++same_place_cnt;
            }
        }
        if (same_place_cnt >= 5) {
            accumulated_h = std::min(accumulated_h, 420.0f);
        }

        auto toast = make_render_node<RenderToast>(
            desc,
            [overlay_id, on_close, placement]() {
                uint64_t id = *overlay_id;
                *overlay_id = 0;
                auto user_cb = on_close; // 先拷贝至栈上，防止 hide_overlay 销毁 RenderToast 与当前 lambda 后悬空
                auto& slots = live_slots();
                slots.erase(std::remove_if(slots.begin(), slots.end(),
                    [id](const Slot& s) { return s.overlay_id == id; }), slots.end());
                if (Window* w = Window::active_window(); w && id != 0) {
                    w->hide_overlay(id);
                }
                reflow_placement(placement);
                if (user_cb) user_cb();
            });

        Vec2 sz = toast->compute_size();
        Vec2 win_sz = win->size();
        const float margin = 16.0f;
        float gx = margin;
        float gy = margin;

        switch (placement) {
            case ToastPlacement::TopRight:
                gx = std::max(margin, win_sz.x - sz.x - margin);
                gy = win->titlebar_height() + margin + accumulated_h;
                break;
            case ToastPlacement::BottomCenter:
                gx = std::max(margin, std::round((win_sz.x - sz.x) * 0.5f));
                gy = std::max(win->titlebar_height() + margin, win_sz.y - sz.y - margin - accumulated_h);
                break;
            case ToastPlacement::BottomRight:
            default:
                gx = std::max(margin, win_sz.x - sz.x - margin);
                gy = std::max(win->titlebar_height() + margin, win_sz.y - sz.y - margin - accumulated_h);
                break;
        }

        Vec2 init_pos{std::round(gx), std::round(gy)};
        OverlayEntry entry;
        entry.content = toast;
        entry.global_pos = init_pos;
        entry.size = sz;
        entry.dismiss_on_outside_click = false;
        entry.is_modal = false;

        uint64_t id = win->show_overlay(std::move(entry));
        *overlay_id = id;
        active_slots.push_back(Slot{id, placement, sz.y, toast, false});
        toast->set_overlay_tracking(id, init_pos, [overlay_id, placement]() {
            uint64_t id_now = *overlay_id;
            for (auto& s : live_slots()) {
                if (s.overlay_id == id_now) {
                    s.closing = true;
                    break;
                }
            }
            reflow_placement(placement);
        });
        return id;
    }

private:
    struct Slot {
        uint64_t overlay_id{0};
        ToastPlacement placement{ToastPlacement::BottomRight};
        float height{72.0f};
        std::weak_ptr<RenderToast> node;
        bool closing{false};
    };
    static std::vector<Slot>& live_slots() {
        static std::vector<Slot> s;
        return s;
    }
};
} // namespace ktui
 // namespace ktui
