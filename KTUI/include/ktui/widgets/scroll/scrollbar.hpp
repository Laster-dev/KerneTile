#pragma once

#include "smooth_scroll_physics.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

// WinUI 3 (Windows 11 Fluent 2) 像素级对齐垂直滚动条
// 规范度量:
// - 槽位宽度: 12px (ScrollBarSize = 12px), 圆角 6px
// - 收起态 (Collapsed / AutoHide): 2px 极细圆角胶囊，距右边缘 3px，隐藏槽底与上下箭头
// - 展开态 (Expanded on Hover/Drag): 120ms 平滑展开至 12px 柔和圆角槽底 + 顶部/底部 12x14px 三角箭头按钮 + 6px 居中胶囊滑块 (最小高 24px)
class WinUIScrollBar {
public:
    static constexpr float kTrackWidth = 12.0f;
    static constexpr float kArrowHeight = 14.0f;
    static constexpr float kMinThumbHeight = 24.0f;
    static constexpr float kCollapsedThumbWidth = 2.0f;
    static constexpr float kExpandedThumbWidth = 6.0f;
    static constexpr float kRightMargin = 1.0f;

    struct Metrics {
        Rect track_rect;
        Rect top_arrow_rect;
        Rect bottom_arrow_rect;
        Rect thumb_lane_rect;
        Rect thumb_rect;
        bool visible{false};
    };

    enum class HitPart {
        None,
        TopArrow,
        BottomArrow,
        TrackAbove,
        TrackBelow,
        Thumb
    };

    [[nodiscard]] bool is_dragging() const noexcept { return m_dragging; }
    [[nodiscard]] bool is_hovered() const noexcept { return m_bar_hovered; }
    [[nodiscard]] float expand_progress() const noexcept { return m_expand_progress; }

    // 外部通知发生滚动活动 (滚轮/按键)，唤醒并重置自动淡出计时
    void notify_activity() noexcept {
        m_last_activity = std::chrono::steady_clock::now();
        m_visible_alpha = 1.0f;
    }

    [[nodiscard]] Metrics compute_metrics(Rect viewport_rect,
                                          float content_height,
                                          float scroll_offset) const noexcept {
        Metrics m{};
        float view_h = viewport_rect.height();
        if (content_height <= view_h + 0.5f || view_h <= 1.0f) {
            m.visible = false;
            return m;
        }
        m.visible = true;

        float x = viewport_rect.max_x - kTrackWidth - kRightMargin;
        float y = viewport_rect.min_y + 2.0f;
        float h = std::max(0.0f, view_h - 4.0f);
        m.track_rect = Rect::from_xywh(x, y, kTrackWidth, h);

        float arrow_h = (h >= kArrowHeight * 2.0f + kMinThumbHeight + 4.0f) ? kArrowHeight : 0.0f;
        m.top_arrow_rect = Rect::from_xywh(x, y, kTrackWidth, arrow_h);
        m.bottom_arrow_rect = Rect::from_xywh(x, y + h - arrow_h, kTrackWidth, arrow_h);

        float lane_y = y + arrow_h + 1.0f;
        float lane_h = std::max(1.0f, h - arrow_h * 2.0f - 2.0f);
        m.thumb_lane_rect = Rect::from_xywh(x, lane_y, kTrackWidth, lane_h);

        float ratio = std::clamp(view_h / content_height, 0.0f, 1.0f);
        float thumb_h = std::clamp(lane_h * ratio, std::min(kMinThumbHeight, lane_h), lane_h);
        float max_scroll = std::max(1.0f, content_height - view_h);
        float frac = std::clamp(scroll_offset / max_scroll, 0.0f, 1.0f);
        float thumb_y = lane_y + frac * (lane_h - thumb_h);

        float cur_w = kCollapsedThumbWidth + (kExpandedThumbWidth - kCollapsedThumbWidth) * m_expand_progress;
        // 收起态贴右 (距槽右侧 3px)，展开态精确居中于 12px 槽内 (两侧各 3px)
        float collapsed_x = m.track_rect.max_x - 3.0f - kCollapsedThumbWidth;
        float expanded_x = m.track_rect.min_x + (kTrackWidth - kExpandedThumbWidth) * 0.5f;
        float thumb_x = collapsed_x + (expanded_x - collapsed_x) * m_expand_progress;

        m.thumb_rect = Rect::from_xywh(thumb_x, thumb_y, cur_w, thumb_h);
        return m;
    }

    [[nodiscard]] HitPart hit_test(PointF local_pos, const Metrics& m) const noexcept {
        if (!m.visible || !m.track_rect.contains(local_pos)) {
            return HitPart::None;
        }
        if (m.top_arrow_rect.height() > 0.0f && m.top_arrow_rect.contains(local_pos)) {
            return HitPart::TopArrow;
        }
        if (m.bottom_arrow_rect.height() > 0.0f && m.bottom_arrow_rect.contains(local_pos)) {
            return HitPart::BottomArrow;
        }
        // 滑块热区横向覆盖整个 12px 轨道，方便点击拖拽
        Rect thumb_hit = Rect::from_xywh(m.track_rect.min_x, m.thumb_rect.min_y,
                                         m.track_rect.width(), m.thumb_rect.height());
        if (thumb_hit.contains(local_pos)) {
            return HitPart::Thumb;
        }
        if (local_pos.y < m.thumb_rect.min_y) {
            return HitPart::TrackAbove;
        }
        return HitPart::TrackBelow;
    }

    // 处理指针移动 (含拖拽滑块) -> 返回 true 表示消耗了事件或需要重绘
    bool handle_pointer_move(PointF local_pos,
                             Rect viewport_rect,
                             float content_height,
                             SmoothScrollPhysics& physics,
                             bool& out_consumed) noexcept {
        out_consumed = false;
        Metrics m = compute_metrics(viewport_rect, content_height, physics.offset());
        if (!m.visible) {
            m_bar_hovered = false;
            m_hover_part = HitPart::None;
            m_dragging = false;
            return false;
        }

        if (m_dragging) {
            float lane_span = std::max(1.0f, m.thumb_lane_rect.height() - m.thumb_rect.height());
            float dy = local_pos.y - m_drag_start_y;
            float new_offset = m_drag_start_offset + (dy / lane_span) * physics.max_scroll();
            physics.jump_to(new_offset);
            notify_activity();
            out_consumed = true;
            return true;
        }

        HitPart part = hit_test(local_pos, m);
        bool now_hovered = (part != HitPart::None);
        bool changed = (now_hovered != m_bar_hovered || part != m_hover_part);
        m_bar_hovered = now_hovered;
        m_hover_part = part;
        if (now_hovered) {
            notify_activity();
            out_consumed = true;
        }
        if (changed) {
            m_last_anim_time = std::chrono::steady_clock::now();
        }
        return changed || now_hovered;
    }

    void handle_pointer_exit() noexcept {
        if (!m_dragging) {
            m_bar_hovered = false;
            m_hover_part = HitPart::None;
            m_pressed_part = HitPart::None;
            m_last_anim_time = std::chrono::steady_clock::now();
        }
    }

    // 处理指针按下 -> 返回 true 表示事件被滚动条消费
    bool handle_pointer_down(PointF local_pos,
                             Rect viewport_rect,
                             float content_height,
                             SmoothScrollPhysics& physics,
                             float line_step = 32.0f) noexcept {
        Metrics m = compute_metrics(viewport_rect, content_height, physics.offset());
        HitPart part = hit_test(local_pos, m);
        if (part == HitPart::None) {
            return false;
        }

        m_pressed_part = part;
        notify_activity();
        m_last_anim_time = std::chrono::steady_clock::now();

        switch (part) {
            case HitPart::Thumb:
                m_dragging = true;
                m_drag_start_y = local_pos.y;
                m_drag_start_offset = physics.offset();
                break;
            case HitPart::TopArrow:
                physics.scroll_by(-line_step);
                break;
            case HitPart::BottomArrow:
                physics.scroll_by(line_step);
                break;
            case HitPart::TrackAbove:
                physics.scroll_by(-std::max(line_step, viewport_rect.height() * 0.85f));
                break;
            case HitPart::TrackBelow:
                physics.scroll_by(std::max(line_step, viewport_rect.height() * 0.85f));
                break;
            default:
                break;
        }
        return true;
    }

    bool handle_pointer_up() noexcept {
        bool was_active = m_dragging || (m_pressed_part != HitPart::None);
        m_dragging = false;
        m_pressed_part = HitPart::None;
        m_last_anim_time = std::chrono::steady_clock::now();
        return was_active;
    }

    // 推进展开/收起和自动淡出动画，返回 true 表示仍需重绘下一帧
    bool update_animations(bool viewport_hovered) noexcept {
        auto now = std::chrono::steady_clock::now();
        if (m_last_anim_time.time_since_epoch().count() == 0) {
            m_last_anim_time = now;
        }
        float dt = std::clamp(std::chrono::duration<float>(now - m_last_anim_time).count(), 0.0f, 0.05f);
        m_last_anim_time = now;

        const float target_expand = (m_bar_hovered || m_dragging) ? 1.0f : 0.0f;
        bool animating = false;

        if (std::abs(m_expand_progress - target_expand) > 0.004f) {
            // 120ms 快速展开 / 180ms 柔和收起
            float speed = (target_expand > m_expand_progress) ? 24.0f : 16.0f;
            m_expand_progress += (target_expand - m_expand_progress) * std::min(1.0f, dt * speed);
            animating = true;
        } else {
            m_expand_progress = target_expand;
        }

        // WinUI 3 AutoHide: 悬停视口、正在滚动或操作滚动条时保持可见，静置 1.6s 后轻柔淡出至 0.25 细线提示
        float idle_sec = (m_last_activity.time_since_epoch().count() == 0)
            ? 0.0f
            : std::chrono::duration<float>(now - m_last_activity).count();
        float target_alpha = (m_bar_hovered || m_dragging || viewport_hovered || idle_sec < 1.6f) ? 1.0f : 0.35f;
        if (std::abs(m_visible_alpha - target_alpha) > 0.005f) {
            m_visible_alpha += (target_alpha - m_visible_alpha) * std::min(1.0f, dt * 10.0f);
            animating = true;
        } else {
            m_visible_alpha = target_alpha;
        }

        return animating;
    }

    void paint(PaintContext& ctx,
               Rect viewport_rect,
               float content_height,
               float scroll_offset,
               const Theme& theme,
               float parent_alpha = 1.0f) const {
        Metrics m = compute_metrics(viewport_rect, content_height, scroll_offset);
        if (!m.visible) return;

        const float eff_alpha = m_visible_alpha * parent_alpha;
        if (eff_alpha <= 0.005f) return;

        // 1. 展开态背景槽 (WinUI 3 ScrollBarTrackFill: 12px 宽，6px 圆角胶囊)
        if (m_expand_progress > 0.01f) {
            Color track_bg = theme.is_dark
                ? Color{44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 0.88f * m_expand_progress * eff_alpha}
                : Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.90f * m_expand_progress * eff_alpha};
            Color track_border = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, 0.06f * m_expand_progress * eff_alpha}
                : Color{0.0f, 0.0f, 0.0f, 0.06f * m_expand_progress * eff_alpha};

            ctx.draw_rect(m.track_rect, track_bg, kTrackWidth * 0.5f, 0.0f);
            ctx.draw_rect(m.track_rect, track_border, kTrackWidth * 0.5f, 1.0f);
        }

        // 2. 顶部与底部 WinUI 3 三角箭头按钮 (展开态淡入)
        if (m_expand_progress > 0.05f && m.top_arrow_rect.height() > 0.0f) {
            auto arrow_color = [&](HitPart part) {
                bool pressed = (m_pressed_part == part);
                bool hovered = (m_hover_part == part);
                float base_a = pressed ? 0.85f : (hovered ? 0.78f : 0.54f);
                return theme.is_dark
                    ? Color{1.0f, 1.0f, 1.0f, base_a * m_expand_progress * eff_alpha}
                    : Color{0.0f, 0.0f, 0.0f, base_a * m_expand_progress * eff_alpha};
            };

            // 顶部向上小三角 (▲)
            {
                Color col = arrow_color(HitPart::TopArrow);
                float cx = m.top_arrow_rect.min_x + m.top_arrow_rect.width() * 0.5f;
                float cy = m.top_arrow_rect.min_y + m.top_arrow_rect.height() * 0.5f + 1.0f;
                ctx.draw_line(PointF{cx - 2.6f, cy + 1.2f}, PointF{cx, cy - 1.5f}, col, 1.3f);
                ctx.draw_line(PointF{cx, cy - 1.5f}, PointF{cx + 2.6f, cy + 1.2f}, col, 1.3f);
                ctx.draw_line(PointF{cx - 1.5f, cy + 1.0f}, PointF{cx + 1.5f, cy + 1.0f}, col, 1.2f);
            }

            // 底部向下小三角 (▼)
            {
                Color col = arrow_color(HitPart::BottomArrow);
                float cx = m.bottom_arrow_rect.min_x + m.bottom_arrow_rect.width() * 0.5f;
                float cy = m.bottom_arrow_rect.min_y + m.bottom_arrow_rect.height() * 0.5f - 1.0f;
                ctx.draw_line(PointF{cx - 2.6f, cy - 1.2f}, PointF{cx, cy + 1.5f}, col, 1.3f);
                ctx.draw_line(PointF{cx, cy + 1.5f}, PointF{cx + 2.6f, cy - 1.2f}, col, 1.3f);
                ctx.draw_line(PointF{cx - 1.5f, cy - 1.0f}, PointF{cx + 1.5f, cy - 1.0f}, col, 1.2f);
            }
        }

        // 3. 胶囊滑块 (收起态 2px -> 展开态 6px，圆角严格等于 width * 0.5)
        float thumb_a = 0.48f;
        if (m_dragging || m_pressed_part == HitPart::Thumb) {
            thumb_a = 0.85f;
        } else if (m_hover_part == HitPart::Thumb) {
            thumb_a = 0.72f;
        } else if (m_expand_progress > 0.01f) {
            thumb_a = 0.48f + 0.08f * m_expand_progress;
        }

        Color thumb_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, thumb_a * eff_alpha}
            : Color{0.0f, 0.0f, 0.0f, (thumb_a * 0.85f) * eff_alpha};

        ctx.draw_rect(m.thumb_rect, thumb_col, m.thumb_rect.width() * 0.5f, 0.0f);
    }

private:
    bool m_bar_hovered{false};
    bool m_dragging{false};
    HitPart m_hover_part{HitPart::None};
    HitPart m_pressed_part{HitPart::None};
    float m_drag_start_y{0.0f};
    float m_drag_start_offset{0.0f};
    float m_expand_progress{0.0f};
    float m_visible_alpha{1.0f};
    std::chrono::steady_clock::time_point m_last_anim_time;
    std::chrono::steady_clock::time_point m_last_activity;
};

} // namespace ktui
