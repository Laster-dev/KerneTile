#pragma once

#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// Fluent 2 风格带时间轴缩略图的范围选择器 (Brush / Minimap Timeline Range Selector)
// - 内嵌柔和圆角迷你密度/波峰波形图，异常峰值以 Fluent 琥珀橙红 + 顶端圆点标识
// - 悬停垂直准星与实时采样值浮标；Fluent 2 双层浮雕滑块手柄，支持缩放与整体平移选区
class RenderMinimapTimelineSlider : public RenderBox {
public:
    RenderMinimapTimelineSlider(std::vector<float> samples = {},
                                float start_ratio = 0.25f,
                                float end_ratio = 0.70f,
                                float anomaly_threshold = 0.78f,
                                std::vector<std::string> axis_labels = {},
                                Fn<void(float, float)> on_range_changed = nullptr)
        : m_samples(std::move(samples)),
          m_start(std::clamp(start_ratio, 0.0f, 0.95f)),
          m_end(std::clamp(end_ratio, m_start + 0.05f, 1.0f)),
          m_anomaly_threshold(std::clamp(anomaly_threshold, 0.1f, 1.0f)),
          m_axis_labels(std::move(axis_labels)),
          m_on_range_changed(std::move(on_range_changed)) {
        is_focusable = true;
        setup_behavior();
    }

    [[nodiscard]] float start_ratio() const noexcept { return m_start; }
    [[nodiscard]] float end_ratio() const noexcept { return m_end; }

    void set_range(float s, float e, bool notify = false) {
        constexpr float kMinSpan = 0.05f;
        s = std::clamp(s, 0.0f, 1.0f - kMinSpan);
        e = std::clamp(e, s + kMinSpan, 1.0f);
        if (std::abs(m_start - s) > 1e-4f || std::abs(m_end - e) > 1e-4f) {
            m_start = s;
            m_end = e;
            mark_needs_paint();
            if (notify && m_on_range_changed) m_on_range_changed(m_start, m_end);
        }
    }

    void set_samples(std::vector<float> s) {
        m_samples = std::move(s);
        mark_needs_paint();
    }
    void set_on_range_changed(Fn<void(float, float)> cb) { m_on_range_changed = std::move(cb); }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(m_constraints.min_width, 480.0f);
        float h = m_constraints.has_bounded_height() ? std::min(m_constraints.max_height, 92.0f) : 92.0f;
        m_size = m_constraints.constrain(Vec2{w, std::max(72.0f, h)});
        m_track = Rect::from_xywh(10.0f, 8.0f, std::max(24.0f, m_size.x - 20.0f), std::max(42.0f, m_size.y - 30.0f));
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        const float sx = m_track.min_x + m_track.width() * m_start;
        const float ex = m_track.min_x + m_track.width() * m_end;
        if (std::abs(position.x - sx) <= 8.0f || std::abs(position.x - ex) <= 8.0f) {
            cursor_shape = CursorShape::ResizeEW;
        } else if (position.x > sx && position.x < ex && m_track.contains(position)) {
            cursor_shape = CursorShape::PointingHand;
        } else {
            cursor_shape = CursorShape::Arrow;
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const Color accent = t.colors.primary;
        const Color anomaly = Color{0xF7 / 255.0f, 0x63 / 255.0f, 0x0C / 255.0f, 1.0f};

        // Fluent 2 柔和凹槽底板
        Color track_bg = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.032f} : Color{0.0f, 0.0f, 0.0f, 0.028f};
        Color track_bd = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.09f}  : Color{0.0f, 0.0f, 0.0f, 0.09f};
        context.draw_rect(m_track, track_bg, 8.0f, 0.0f);

        context.push_clip(m_track, 8.0f);

        // 水平参考微细网格线 (50%)
        Color grid_c = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.045f} : Color{0.0f, 0.0f, 0.0f, 0.045f};
        context.draw_rect(Rect::from_xywh(m_track.min_x, std::round(m_track.min_y + m_track.height() * 0.5f),
                                          m_track.width(), 1.0f), grid_c, 0.0f, 0.0f);

        // 1. 内嵌迷你密度波形柱状图 + 波峰圆点
        float max_v = 0.001f;
        for (float v : m_samples) max_v = std::max(max_v, v);
        if (!m_samples.empty()) {
            const size_t n = m_samples.size();
            const float step_w = m_track.width() / static_cast<float>(n);
            const float bar_w = std::max(2.0f, step_w - 2.0f);
            const float max_bar_h = std::max(10.0f, m_track.height() - 14.0f);

            for (size_t i = 0; i < n; ++i) {
                float norm = std::clamp(m_samples[i] / max_v, 0.05f, 1.0f);
                float ratio = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
                bool in_sel = (ratio >= m_start && ratio <= m_end);
                bool is_peak = (norm >= m_anomaly_threshold);
                float bh = max_bar_h * norm;
                float bx = m_track.min_x + static_cast<float>(i) * step_w + (step_w - bar_w) * 0.5f;
                float by = m_track.max_y - bh - 2.0f;

                Color col = is_peak
                    ? Color{anomaly.r, anomaly.g, anomaly.b, in_sel ? 0.92f : 0.38f}
                    : (in_sel ? Color{accent.r, accent.g, accent.b, 0.72f}
                              : (is_dark ? Color{1.0f, 1.0f, 1.0f, 0.16f} : Color{0.0f, 0.0f, 0.0f, 0.15f}));
                context.draw_rect(Rect::from_xywh(bx, by, bar_w, bh), col, std::min(2.0f, bar_w * 0.5f), 0.0f);

                if (is_peak && in_sel) {
                    context.draw_rect(Rect::from_xywh(bx + bar_w * 0.5f - 2.0f, by - 5.0f, 4.0f, 4.0f),
                                      anomaly, 2.0f, 0.0f);
                }
            }
        }

        // 2. 非选区柔和毛玻璃压暗 + Fluent 2 选区高亮窗口
        const float sx = m_track.min_x + m_track.width() * m_start;
        const float ex = m_track.min_x + m_track.width() * m_end;
        Color dim = is_dark ? Color{0.0f, 0.0f, 0.0f, 0.36f} : Color{0.92f, 0.94f, 0.96f, 0.52f};
        if (sx > m_track.min_x) {
            context.draw_rect(Rect::from_xywh(m_track.min_x, m_track.min_y, sx - m_track.min_x, m_track.height()), dim, 0.0f, 0.0f);
        }
        if (ex < m_track.max_x) {
            context.draw_rect(Rect::from_xywh(ex, m_track.min_y, m_track.max_x - ex, m_track.height()), dim, 0.0f, 0.0f);
        }

        Rect sel_rect = Rect::from_xywh(sx, m_track.min_y, std::max(4.0f, ex - sx), m_track.height());
        context.draw_rect(sel_rect, Color{accent.r, accent.g, accent.b, m_drag_mode == DragMode::MoveWindow ? 0.14f : 0.07f}, 0.0f, 0.0f);
        // 顶部与底部 2px 主题色强调边条（Fluent 2 Brush 质感）
        context.draw_rect(Rect::from_xywh(sx, m_track.min_y, std::max(4.0f, ex - sx), 2.0f), accent, 0.0f, 0.0f);
        context.draw_rect(Rect::from_xywh(sx, m_track.max_y - 2.0f, std::max(4.0f, ex - sx), 2.0f), Color{accent.r, accent.g, accent.b, 0.65f}, 0.0f, 0.0f);
        // 左右 1.5px 边界线
        context.draw_rect(Rect::from_xywh(sx, m_track.min_y, 1.5f, m_track.height()), accent, 0.0f, 0.0f);
        context.draw_rect(Rect::from_xywh(ex - 1.5f, m_track.min_y, 1.5f, m_track.height()), accent, 0.0f, 0.0f);

        // 3. 悬停垂直参考准星与实时读数胶囊
        if (m_hovered && m_track.contains(m_hover_pos) && !m_samples.empty() && m_drag_mode == DragMode::None) {
            float hr = std::clamp((m_hover_pos.x - m_track.min_x) / std::max(1.0f, m_track.width()), 0.0f, 0.9999f);
            size_t idx = static_cast<size_t>(hr * static_cast<float>(m_samples.size()));
            context.draw_rect(Rect::from_xywh(std::round(m_hover_pos.x), m_track.min_y, 1.0f, m_track.height()),
                              is_dark ? Color{1, 1, 1, 0.28f} : Color{0, 0, 0, 0.22f}, 0.0f, 0.0f);
            char tip[32];
            std::snprintf(tip, sizeof(tip), "%.0f", m_samples[idx]);
            Vec2 ts = TextMeasure::measure(tip, 10.5f, FontWeight::SemiBold);
            float tw = ts.x + 12.0f;
            float tx = std::clamp(m_hover_pos.x - tw * 0.5f, m_track.min_x + 4.0f, m_track.max_x - tw - 4.0f);
            Rect pill = Rect::from_xywh(std::round(tx), m_track.min_y + 4.0f, tw, 18.0f);
            context.draw_rect(pill, is_dark ? Color{0x28 / 255.0f, 0x2A / 255.0f, 0x30 / 255.0f, 0.94f}
                                            : Color{1.0f, 1.0f, 1.0f, 0.95f}, 9.0f, 0.0f);
            context.draw_rect(pill, track_bd, 9.0f, 1.0f);
            context.draw_text(tip, PointF{std::round(pill.min_x + 6.0f), std::round(pill.min_y + (18.0f - ts.y) * 0.5f)},
                              10.5f, t.colors.text_primary, FontWeight::SemiBold);
        }

        context.pop_clip();
        context.draw_rect(m_track, track_bd, 8.0f, 1.0f);

        // 4. Fluent 2 双层浮雕圆形滑块手柄（覆盖在轨道边缘之上，立体感更强）
        auto draw_fluent_thumb = [&](float hx, bool active, bool hov) {
            float r = active ? 7.5f : (hov ? 7.0f : 6.0f);
            float cy = m_track.min_y + m_track.height() * 0.5f;
            Rect outer = Rect::from_xywh(hx - r, cy - r, r * 2.0f, r * 2.0f);
            Color ring_bg = is_dark ? Color{0x2D / 255.0f, 0x2F / 255.0f, 0x36 / 255.0f, 1.0f} : Color::white();
            context.draw_rect(outer, ring_bg, r, 0.0f);
            context.draw_rect(outer, is_dark ? Color{1, 1, 1, 0.16f} : Color{0, 0, 0, 0.14f}, r, 1.0f);
            float ir = active ? 4.2f : (hov ? 3.8f : 3.2f);
            context.draw_rect(Rect::from_xywh(hx - ir, cy - ir, ir * 2.0f, ir * 2.0f), accent, ir, 0.0f);
        };
        draw_fluent_thumb(sx, m_drag_mode == DragMode::LeftHandle, m_hover_handle == 1);
        draw_fluent_thumb(ex, m_drag_mode == DragMode::RightHandle, m_hover_handle == 2);

        // 5. 底部时间刻度标签
        if (!m_axis_labels.empty()) {
            const float ly = m_track.max_y + 6.0f;
            const size_t cnt = m_axis_labels.size();
            for (size_t i = 0; i < cnt; ++i) {
                float frac = (cnt == 1) ? 0.5f : static_cast<float>(i) / static_cast<float>(cnt - 1);
                Vec2 ls = TextMeasure::measure(m_axis_labels[i], 10.5f);
                float lx = m_track.min_x + m_track.width() * frac - ls.x * frac;
                context.draw_text(m_axis_labels[i], PointF{std::round(lx), ly}, 10.5f, t.colors.text_secondary);
            }
        }
    }

private:
    enum class DragMode : uint8_t { None, LeftHandle, RightHandle, MoveWindow };

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) { m_hovered = true; mark_needs_paint(); };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_hover_handle = 0;
            mark_needs_paint();
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            const float px = ev.local_position.x;
            const float sx = m_track.min_x + m_track.width() * m_start;
            const float ex = m_track.min_x + m_track.width() * m_end;

            if (std::abs(px - sx) <= 9.0f) {
                m_drag_mode = DragMode::LeftHandle;
            } else if (std::abs(px - ex) <= 9.0f) {
                m_drag_mode = DragMode::RightHandle;
            } else if (px > sx && px < ex) {
                m_drag_mode = DragMode::MoveWindow;
                m_drag_anchor_x = px;
                m_drag_init_start = m_start;
                m_drag_init_end = m_end;
            } else if (m_track.contains(ev.local_position)) {
                float span = m_end - m_start;
                float center = std::clamp((px - m_track.min_x) / std::max(1.0f, m_track.width()), 0.0f, 1.0f);
                float ns = std::clamp(center - span * 0.5f, 0.0f, 1.0f - span);
                set_range(ns, ns + span, true);
                m_drag_mode = DragMode::MoveWindow;
                m_drag_anchor_x = px;
                m_drag_init_start = m_start;
                m_drag_init_end = m_end;
            }
            mark_needs_paint();
            ev.stop_propagation();
        };

        on_pointer_move = [this](PointerEvent& ev) {
            m_hover_pos = ev.local_position;
            const float tw = std::max(1.0f, m_track.width());
            if (m_drag_mode != DragMode::None) {
                const float r = std::clamp((ev.local_position.x - m_track.min_x) / tw, 0.0f, 1.0f);
                if (m_drag_mode == DragMode::LeftHandle) {
                    set_range(std::min(r, m_end - 0.05f), m_end, true);
                } else if (m_drag_mode == DragMode::RightHandle) {
                    set_range(m_start, std::max(r, m_start + 0.05f), true);
                } else if (m_drag_mode == DragMode::MoveWindow) {
                    float delta = (ev.local_position.x - m_drag_anchor_x) / tw;
                    float span = m_drag_init_end - m_drag_init_start;
                    float ns = std::clamp(m_drag_init_start + delta, 0.0f, 1.0f - span);
                    set_range(ns, ns + span, true);
                }
                ev.stop_propagation();
                return;
            }

            const float sx = m_track.min_x + tw * m_start;
            const float ex = m_track.min_x + tw * m_end;
            int hh = 0;
            if (std::abs(ev.local_position.x - sx) <= 9.0f) hh = 1;
            else if (std::abs(ev.local_position.x - ex) <= 9.0f) hh = 2;
            m_hover_handle = hh;
            mark_needs_paint();
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (m_drag_mode != DragMode::None) {
                m_drag_mode = DragMode::None;
                mark_needs_paint();
                ev.stop_propagation();
            }
        };
    }

    std::vector<float> m_samples;
    float m_start{0.25f};
    float m_end{0.70f};
    float m_anomaly_threshold{0.78f};
    std::vector<std::string> m_axis_labels;
    Fn<void(float, float)> m_on_range_changed;

    Rect m_track{};
    DragMode m_drag_mode{DragMode::None};
    float m_drag_anchor_x{0.0f};
    float m_drag_init_start{0.0f};
    float m_drag_init_end{0.0f};
    bool m_hovered{false};
    int m_hover_handle{0};
    PointF m_hover_pos{};
};

struct MinimapTimelineSlider : BindableMixin<MinimapTimelineSlider, RenderMinimapTimelineSlider> {
    std::vector<float> samples{};
    float start_ratio{0.25f};
    float end_ratio{0.70f};
    float anomaly_threshold{0.78f};
    std::vector<std::string> axis_labels{};
    Fn<void(float, float)> on_range_changed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class MinimapTimelineSliderWidget : public Widget {
public:
    explicit MinimapTimelineSliderWidget(const MinimapTimelineSlider& desc) : m_desc(desc) {}
    explicit MinimapTimelineSliderWidget(MinimapTimelineSlider&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderMinimapTimelineSlider>(
            m_desc.samples, m_desc.start_ratio, m_desc.end_ratio,
            m_desc.anomaly_threshold, m_desc.axis_labels, m_desc.on_range_changed);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderMinimapTimelineSlider>(node)) {
            n->set_samples(m_desc.samples);
            n->set_on_range_changed(m_desc.on_range_changed);
        }
    }
private:
    MinimapTimelineSlider m_desc;
};

inline MinimapTimelineSlider::operator WidgetPtr() const& { return make_widget<MinimapTimelineSliderWidget>(*this); }
inline MinimapTimelineSlider::operator WidgetPtr() && { return make_widget<MinimapTimelineSliderWidget>(std::move(*this)); }

} // namespace ktui
