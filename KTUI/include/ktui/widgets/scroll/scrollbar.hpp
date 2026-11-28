#pragma once

#include "smooth_scroll_physics.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
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
        float base_thumb_h = std::clamp(lane_h * ratio, std::min(kMinThumbHeight, lane_h), lane_h);
        float max_scroll = std::max(1.0f, content_height - view_h);
        // 顶部/底部越界拉动时，滑块按越界比例弹性压缩并紧贴边缘 (iOS / Android 移动端手感)
        float overscroll = 0.0f;
        if (scroll_offset < 0.0f) overscroll = -scroll_offset;
        else if (scroll_offset > max_scroll) overscroll = scroll_offset - max_scroll;
        float min_squish_h = std::min(kMinThumbHeight * 0.45f, base_thumb_h);
        float thumb_h = std::max(min_squish_h, base_thumb_h - overscroll * 0.55f);
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

    [[nodiscard]] HitPart hit_test(PointF local_pos, const Metrics& m, PointerDeviceKind kind = PointerDeviceKind::Mouse) const noexcept {
        if (!m.visible) {
            return HitPart::None;
        }
        Rect track_hit = m.track_rect;
        if (kind == PointerDeviceKind::Touch) {
            // 触控下向左侧扩宽命中带至 24px，方便手指直接抓握滚动条
            track_hit.min_x = std::min(track_hit.min_x, track_hit.max_x - 24.0f);
        }
        if (!track_hit.contains(local_pos)) {
            return HitPart::None;
        }
        if (m.top_arrow_rect.height() > 0.0f && local_pos.y <= m.top_arrow_rect.max_y) {
            return HitPart::TopArrow;
        }
        if (m.bottom_arrow_rect.height() > 0.0f && local_pos.y >= m.bottom_arrow_rect.min_y) {
            return HitPart::BottomArrow;
        }
        if (local_pos.y >= m.thumb_rect.min_y && local_pos.y <= m.thumb_rect.max_y) {
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
                             float line_step = 32.0f,
                             PointerDeviceKind device_kind = PointerDeviceKind::Mouse) noexcept {
        Metrics m = compute_metrics(viewport_rect, content_height, physics.offset());
        HitPart part = hit_test(local_pos, m, device_kind);
        if (part == HitPart::None) {
            return false;
        }

        m_pressed_part = part;
        notify_activity();
        m_last_anim_time = std::chrono::steady_clock::now();

        switch (part) {
            case HitPart::Thumb:
                physics.jump_to(physics.offset());
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

    // 零动画直连滚动接口 (供 HexViewer / InspectorGrid 等追求零延迟瞬时滚动的高性能控件复用)
    bool handle_pointer_move_direct(PointF local_pos,
                                    Rect viewport_rect,
                                    float content_height,
                                    float& scroll_offset,
                                    float max_scroll,
                                    bool& out_consumed) noexcept {
        out_consumed = false;
        Metrics m = compute_metrics(viewport_rect, content_height, scroll_offset);
        if (!m.visible) {
            m_bar_hovered = false;
            m_hover_part = HitPart::None;
            m_dragging = false;
            return false;
        }
        if (m_dragging) {
            float lane_span = std::max(1.0f, m.thumb_lane_rect.height() - m.thumb_rect.height());
            float dy = local_pos.y - m_drag_start_y;
            scroll_offset = std::clamp(m_drag_start_offset + (dy / lane_span) * max_scroll, 0.0f, max_scroll);
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
        if (changed) m_last_anim_time = std::chrono::steady_clock::now();
        return changed || now_hovered;
    }

    bool handle_pointer_down_direct(PointF local_pos,
                                    Rect viewport_rect,
                                    float content_height,
                                    float& scroll_offset,
                                    float max_scroll,
                                    float line_step = 32.0f,
                                    PointerDeviceKind device_kind = PointerDeviceKind::Mouse) noexcept {
        Metrics m = compute_metrics(viewport_rect, content_height, scroll_offset);
        HitPart part = hit_test(local_pos, m, device_kind);
        if (part == HitPart::None) return false;
        m_pressed_part = part;
        notify_activity();
        m_last_anim_time = std::chrono::steady_clock::now();
        switch (part) {
            case HitPart::Thumb:
                m_dragging = true;
                m_drag_start_y = local_pos.y;
                m_drag_start_offset = scroll_offset;
                break;
            case HitPart::TopArrow:
                scroll_offset = std::clamp(scroll_offset - line_step, 0.0f, max_scroll);
                break;
            case HitPart::BottomArrow:
                scroll_offset = std::clamp(scroll_offset + line_step, 0.0f, max_scroll);
                break;
            case HitPart::TrackAbove:
                scroll_offset = std::clamp(scroll_offset - std::max(line_step, viewport_rect.height() * 0.85f), 0.0f, max_scroll);
                break;
            case HitPart::TrackBelow:
                scroll_offset = std::clamp(scroll_offset + std::max(line_step, viewport_rect.height() * 0.85f), 0.0f, max_scroll);
                break;
            default:
                break;
        }
        return true;
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
                ? Color{44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 0.94f * m_expand_progress * eff_alpha}
                : Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.94f * m_expand_progress * eff_alpha};
            Color track_border = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, 0.06f * m_expand_progress * eff_alpha}
                : Color{0.0f, 0.0f, 0.0f, 0.06f * m_expand_progress * eff_alpha};

            ctx.draw_rect(m.track_rect, track_bg, kTrackWidth * 0.5f, 0.0f);
            ctx.draw_rect(m.track_rect, track_border, kTrackWidth * 0.5f, 1.0f);
        }

        // 2. 顶部与底部 WinUI 3 实心圆角小三角箭头 (Segoe Fluent Icons 像素级对齐)
        if (m_expand_progress > 0.05f && m.top_arrow_rect.height() > 0.0f) {
            static constexpr const char* kArrowUpSvg =
                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 12 12'>"
                "<path fill='#FFFFFF' d='M6 3.05c.26 0 .5.13.64.35l3.05 4.55a.68.68 0 0 1-.57 1.05H2.88a.68.68 0 0 1-.57-1.05l3.05-4.55c.14-.22.38-.35.64-.35z'/>"
                "</svg>";
            static constexpr const char* kArrowDownSvg =
                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 12 12'>"
                "<path fill='#FFFFFF' d='M6 8.95a.76.76 0 0 1-.64-.35L2.31 4.05A.68.68 0 0 1 2.88 3h6.24a.68.68 0 0 1 .57 1.05L6.64 8.6a.76.76 0 0 1-.64.35z'/>"
                "</svg>";

            auto draw_arrow = [&](Rect r, HitPart part, const char* svg, float y_nudge) {
                bool pressed = (m_pressed_part == part);
                bool hovered = (m_hover_part == part);
                float base_a = pressed ? 0.88f : (hovered ? 0.78f : 0.54f);
                Color col = theme.is_dark
                    ? Color{1.0f, 1.0f, 1.0f, base_a * m_expand_progress * eff_alpha}
                    : Color{0.0f, 0.0f, 0.0f, (base_a * 0.90f) * m_expand_progress * eff_alpha};
                float sz = pressed ? 6.0f : 7.0f;
                Rect ir = Rect::from_xywh(std::round(r.min_x + (r.width() - sz) * 0.5f),
                                          std::round(r.min_y + (r.height() - sz) * 0.5f + y_nudge),
                                          sz, sz);
                SvgEngine::draw_svg_xml(ctx, svg, ir, col);
            };

            draw_arrow(m.top_arrow_rect, HitPart::TopArrow, kArrowUpSvg, 1.0f);
            draw_arrow(m.bottom_arrow_rect, HitPart::BottomArrow, kArrowDownSvg, -1.0f);
        }

        // 3. 胶囊滑块 (收起态 2px -> 展开态 6px，圆角严格等于 width * 0.5)
        float thumb_a = 0.50f;
        if (m_dragging || m_pressed_part == HitPart::Thumb) {
            thumb_a = 0.64f;
        } else if (m_hover_part == HitPart::Thumb) {
            thumb_a = 0.78f;
        } else if (m_expand_progress > 0.01f) {
            thumb_a = 0.50f + 0.08f * m_expand_progress;
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
