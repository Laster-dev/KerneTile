#pragma once

#include <ktui/animation/frame_clock.hpp>
#include <ktui/app/window.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace ktui {

// WinUI 3 / Fluent 2 输入框外框复用绘制器 (基于语义主题令牌 + 统一 FrameClock 步进)
struct FluentInputChrome {
    static bool step_focus_anim(float& focus_anim, bool focused) noexcept {
        float dt = std::clamp(FrameClock::global().dt(), 1.0f / 240.0f, 0.05f);
        float speed = 11.0f * dt; // ~90ms 完成中心展开
        if (focused && focus_anim < 1.0f) {
            focus_anim = std::min(1.0f, focus_anim + speed);
            return focus_anim < 1.0f;
        }
        if (!focused && focus_anim > 0.0f) {
            focus_anim = std::max(0.0f, focus_anim - speed);
            return focus_anim > 0.0f;
        }
        return false;
    }

    static void paint_box(PaintContext& ctx,
                          const Theme& theme,
                          Rect box,
                          const std::string& header,
                          bool disabled,
                          bool hovered,
                          bool focused,
                          float focus_anim) {
        Color text_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, disabled ? 0.36f : 0.96f}
            : Color{0.0f, 0.0f, 0.0f, disabled ? 0.36f : 0.92f};

        if (!header.empty()) {
            ctx.draw_text(header, PointF{0.0f, 2.0f}, 13.0f, text_col);
        }

        Color bg = disabled ? theme.colors.control_fill_disabled
                 : (focused ? theme.colors.control_fill_input_active
                            : (hovered ? theme.colors.control_fill_secondary
                                       : theme.colors.control_fill_default));

        ctx.draw_rect(box, bg, 4.0f, 0.0f);
        ctx.draw_rect(box, theme.colors.control_stroke_default, 4.0f, 1.0f);

        ctx.draw_rect(Rect::from_xywh(box.min_x + 2.0f, box.max_y - 1.0f,
                                      std::max(0.0f, box.width() - 4.0f), 1.0f),
                      theme.colors.control_stroke_bottom, 0.0f, 0.0f);

        if (focus_anim > 0.01f && !disabled) {
            float half_w = (box.width() - 2.0f) * 0.5f * focus_anim;
            float cx = (box.min_x + box.max_x) * 0.5f;
            ctx.draw_rect(Rect::from_xywh(cx - half_w, box.max_y - 2.0f, half_w * 2.0f, 2.0f),
                          theme.colors.primary, 1.0f, 0.0f);
        }
    }
};

// WinUI 3 / Fluent 2 弹出浮层公共底板与锚点定位器 (语义令牌 + 统一 FrameClock 驱动)
class FluentPopupSurface {
public:
    struct FrameState {
        float ease{1.0f};
        float alpha{1.0f};
        bool animating{false};
    };

    FluentPopupSurface() noexcept
        : m_open_start_sec(FrameClock::global().frame_time_sec()) {}

    void reset_open_animation() noexcept {
        m_open_progress = 0.0f;
        m_open_start_sec = FrameClock::global().frame_time_sec();
    }

    FrameState tick(float duration_sec = 0.13f) noexcept {
        if (m_open_progress < 1.0f) {
            double elapsed = FrameClock::global().frame_time_sec() - m_open_start_sec;
            if (elapsed <= 0.0) elapsed = static_cast<double>(FrameClock::global().dt());
            m_open_progress = std::clamp(static_cast<float>(elapsed) / duration_sec, 0.0f, 1.0f);
        }
        float ease = 1.0f - std::pow(1.0f - m_open_progress, 3.0f);
        float alpha = std::clamp(ease * 1.15f, 0.0f, 1.0f);
        return FrameState{ease, alpha, m_open_progress < 1.0f};
    }

    static void paint_panel(PaintContext& ctx, const Theme& theme, Rect panel, float alpha, float radius = 8.0f) {
        Color bg = theme.colors.flyout_surface;
        bg.a *= alpha;
        Color border = theme.colors.flyout_stroke;
        border.a *= alpha;
        ctx.draw_elevation_shadow(panel, radius, theme.is_dark, 32.0f, alpha);
        ctx.draw_rect(panel, bg, radius, 0.0f);
        ctx.draw_rect(panel, border, radius, 1.0f);
    }

    [[nodiscard]] static Vec2 compute_anchored_pos(const Window& win,
                                                   Vec2 anchor_global,
                                                   Rect anchor_box,
                                                   Vec2 popup_size,
                                                   float gap = 4.0f) noexcept {
        Vec2 win_sz = win.size();
        float tb = win.titlebar_height();
        float gx = std::clamp(anchor_global.x, 6.0f, std::max(6.0f, win_sz.x - popup_size.x - 8.0f));
        float gy = anchor_global.y + anchor_box.max_y + tb + gap;
        if (gy + popup_size.y > win_sz.y - 8.0f) {
            gy = std::max(tb + 6.0f, anchor_global.y + anchor_box.min_y + tb - popup_size.y - gap);
        }
        return Vec2{std::round(gx), std::round(gy)};
    }

private:
    float m_open_progress{0.0f};
    double m_open_start_sec{0.0};
};

} // namespace ktui
