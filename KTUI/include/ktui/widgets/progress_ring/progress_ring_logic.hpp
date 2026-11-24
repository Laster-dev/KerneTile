#pragma once

#include "progress_ring_data.hpp"
#include "progress_ring_style.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

namespace ktui {

// 环形加载圈渲染盒 (RenderProgressRing - Logic Layer)
// 像素级对齐 Windows 11 WinUI 3 官方 ProgressRing (流动伸缩高亮弧 + 圆润 Round Cap 端点)
class RenderProgressRing : public RenderBox {
public:
    RenderProgressRing() noexcept {
        setup_behavior();
    }

    ~RenderProgressRing() override = default;

    [[nodiscard]] float value() const noexcept { return m_data.value; }
    [[nodiscard]] bool indeterminate() const noexcept { return m_data.indeterminate; }
    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    [[nodiscard]] const ProgressRingData& data() const noexcept { return m_data; }

    void set_value(float val, bool animate = true) {
        float clamped = std::clamp(val, 0.0f, 1.0f);
        if (std::abs(m_data.value - clamped) > 0.0001f) {
            m_data.value = clamped;
            if (!animate) {
                m_displayed_val = clamped;
            } else {
                m_last_anim_time = std::chrono::steady_clock::now();
            }
            mark_needs_paint();
        }
    }

    void set_indeterminate(bool ind) {
        if (m_data.indeterminate != ind) {
            m_data.indeterminate = ind;
            m_last_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        }
    }

    void set_ring_size(float sz) {
        if (sz > 0.0f && std::abs(m_data.size - sz) > 0.0001f) {
            m_data.size = sz;
            mark_needs_layout();
        }
    }

    void set_stroke_width(float sw) {
        if (std::abs(m_data.stroke_width - sw) > 0.0001f) {
            m_data.stroke_width = sw;
            mark_needs_paint();
        }
    }

    void set_show_track(bool st) {
        if (m_data.show_track != st) {
            m_data.show_track = st;
            mark_needs_paint();
        }
    }

    void set_disabled(bool disabled) {
        if (m_state.is_disabled != disabled) {
            m_state.is_disabled = disabled;
            m_data.disabled = disabled;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float d = m_constraints.constrain_width(m_data.size > 0.0f ? m_data.size : 32.0f);
        d = m_constraints.constrain_height(d);
        set_size(Vec2{d, d});
    }

    void paint(PaintContext& ctx) override {
        update_animations();

        const Theme& theme = ThemeContext::current();
        ResolvedProgressRingStyle style = ProgressRingStyleResolver::resolve(m_data, m_state, theme);

        const float cx = size().x * 0.5f;
        const float cy = size().y * 0.5f;
        const float stroke_w = style.stroke_width;
        const float r = std::max(1.0f, (std::min(size().x, size().y) - stroke_w) * 0.5f);
        constexpr float kPi = 3.14159265358979323846f;

        // 1. 底槽环 (ProgressRingTrackBackground)
        if (style.show_track && style.track_color.a > 0.001f) {
            Rect track_rect = Rect::from_xywh(cx - r, cy - r, r * 2.0f, r * 2.0f);
            ctx.draw_rect(track_rect, style.track_color, r, stroke_w);
        }

        if (m_data.indeterminate) {
            // =========================================================================
            // Windows 11 WinUI 3 官方标准 Indeterminate 动效:
            // 匀速公转 (2.0s 周期) + 累计推进伸缩弧 (1.5s 周期，每轮无缝累计推进 270° = 0.75 圈)
            // 跨周期头尾角度严格连续 (cycle * 0.75 + 0.75 == (cycle + 1) * 0.75 + 0.0)，彻底消除回跳
            // =========================================================================
            constexpr float kDashPeriod = 1.5f;
            constexpr float kCycleAdvance = 0.75f; // 每周期推进 270° (4 个周期 = 6.0s 恰好 3 整圈)

            float cycle_idx = std::floor(m_time / kDashPeriod);
            float t_dash = (m_time - cycle_idx * kDashPeriod) / kDashPeriod;

            // 整体均匀公转旋转角 (2.0s 周期，6.0s 恰好 3 整圈)
            float rot_ang = (m_time / 2.0f) * (2.0f * kPi);

            auto ease = [](float t) noexcept -> float {
                t = std::clamp(t, 0.0f, 1.0f);
                return (t < 0.5f) ? (4.0f * t * t * t)
                                  : (1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f);
            };

            // 前半周期头部快速展开至 0.75 圈，后半周期尾部追上至 0.75 圈，首尾完全重合衔接下一轮
            float head_frac = 0.0f;
            float tail_frac = 0.0f;
            if (t_dash < 0.5f) {
                float p = ease(t_dash * 2.0f);
                tail_frac = p * 0.12f;
                head_frac = p * kCycleAdvance;
            } else {
                float p = ease((t_dash - 0.5f) * 2.0f);
                tail_frac = 0.12f + p * (kCycleAdvance - 0.12f);
                head_frac = kCycleAdvance;
            }

            float base_cycle_ang = cycle_idx * kCycleAdvance * (2.0f * kPi);
            float start_ang = rot_ang + base_cycle_ang + tail_frac * (2.0f * kPi) - kPi * 0.5f;
            float end_ang = rot_ang + base_cycle_ang + head_frac * (2.0f * kPi) - kPi * 0.5f;
            float span = std::max(0.08f, end_ang - start_ang);

            int seg_count = std::clamp(static_cast<int>(span * 16.0f), 6, 42);
            for (int i = 0; i < seg_count; ++i) {
                float a1 = start_ang + span * (static_cast<float>(i) / static_cast<float>(seg_count));
                float a2 = start_ang + span * (static_cast<float>(i + 1) / static_cast<float>(seg_count));
                PointF p1{cx + r * std::cos(a1), cy + r * std::sin(a1)};
                PointF p2{cx + r * std::cos(a2), cy + r * std::sin(a2)};
                ctx.draw_line(p1, p2, style.indicator_color, stroke_w);
            }
        } else {
            // =========================================================================
            // WinUI 3 确定性百分比进度环 (带 Round Cap 圆头)
            // =========================================================================
            if (m_displayed_val > 0.001f) {
                float start_ang = -kPi * 0.5f; // 12点钟方向
                float span_rad = 2.0f * kPi * m_displayed_val;
                int seg_count = std::clamp(static_cast<int>(span_rad * 14.0f), 4, 48);
                for (int i = 0; i < seg_count; ++i) {
                    float a1 = start_ang + span_rad * (static_cast<float>(i) / static_cast<float>(seg_count));
                    float a2 = start_ang + span_rad * (static_cast<float>(i + 1) / static_cast<float>(seg_count));
                    PointF p1{cx + r * std::cos(a1), cy + r * std::sin(a1)};
                    PointF p2{cx + r * std::cos(a2), cy + r * std::sin(a2)};
                    ctx.draw_line(p1, p2, style.indicator_color, stroke_w);
                }
            }
        }
    }

private:
    void setup_behavior() {
        m_displayed_val = m_data.value;
    }

    void update_animations() {
        auto now = std::chrono::steady_clock::now();
        if (m_last_anim_time.time_since_epoch().count() == 0) {
            m_last_anim_time = now;
            if (m_data.indeterminate || std::abs(m_displayed_val - m_data.value) > 0.001f) {
                mark_needs_paint();
            }
            return;
        }
        float dt = std::chrono::duration<float>(now - m_last_anim_time).count();
        m_last_anim_time = now;
        dt = std::clamp(dt, 0.0f, 0.05f);

        bool animating = false;

        if (m_data.indeterminate) {
            m_time += dt;
            if (m_time >= 3000.0f) m_time = std::fmod(m_time, 6.0f); // 保持精度
            animating = true;
        } else {
            float diff = m_data.value - m_displayed_val;
            if (std::abs(diff) > 0.001f) {
                m_displayed_val += diff * std::min(1.0f, dt * 26.0f);
                if (std::abs(m_data.value - m_displayed_val) <= 0.001f) {
                    m_displayed_val = m_data.value;
                }
                animating = true;
            } else {
                m_displayed_val = m_data.value;
            }
        }

        if (animating) {
            mark_needs_paint();
        }
    }

    ProgressRingData m_data;
    ProgressRingInteractiveState m_state;
    float m_displayed_val{0.0f};
    float m_time{0.0f};
    std::chrono::steady_clock::time_point m_last_anim_time;
};

// C++20 指定初始化器声明式环形加载圈控件 (ProgressRing DSL Wrapper)
struct ProgressRing : BindableMixin<ProgressRing, RenderProgressRing> {
    float value{0.0f};
    bool indeterminate{true};
    float size{32.0f};
    float stroke_width{-1.0f};
    bool show_track{false};
    bool disabled{false};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class ProgressRingWidget : public Widget {
public:
    explicit ProgressRingWidget(const ProgressRing& desc) : m_desc(desc) {}
    explicit ProgressRingWidget(ProgressRing&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ProgressRingWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderProgressRing>();
        apply_desc_to_node(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto pr = std::static_pointer_cast<RenderProgressRing>(node)) {
            apply_desc_to_node(pr);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderProgressRing>& node) {
        node->set_value(m_desc.value, false);
        node->set_indeterminate(m_desc.indeterminate);
        node->set_ring_size(m_desc.size);
        node->set_stroke_width(m_desc.stroke_width);
        node->set_show_track(m_desc.show_track);
        node->set_disabled(m_desc.disabled);
    }

    ProgressRing m_desc;
};

inline ProgressRing::operator WidgetPtr() const& {
    return make_widget<ProgressRingWidget>(*this);
}
inline ProgressRing::operator WidgetPtr() && {
    return make_widget<ProgressRingWidget>(std::move(*this));
}

} // namespace ktui
