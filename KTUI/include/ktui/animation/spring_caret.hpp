#pragma once

#include <ktui/animation/spring_curve.hpp>
#include <ktui/pal/platform_types.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

// ============================================================================
// 高响应平滑弹簧输入光标 + 呼吸渐变闪烁求解器 (Dual-Spring Gliding & Breathing Caret)
// 参考 smooth-caret 设计：
// - 前缘 (lead) 使用高频临界阻尼弹簧 (4200 N/m, ~55ms) 紧贴高速打字输入零滞后
// - 后缘 (tail) 使用略缓临界阻尼弹簧 (1800 N/m, ~95ms) 形成克制的流畅拖尾伸缩胶囊
// - 静止 340ms 后进入 1.08s 周期柔和余弦呼吸闪烁
// ============================================================================
class SpringCaretSolver {
public:
    struct FrameState {
        bool visible{false};
        bool animating{false};
        Rect rect{};
        float opacity{1.0f};
    };

    SpringCaretSolver() noexcept
        : m_x_lead(0.0f, SpringConfig::with_damping_ratio(4200.0f, 1.0f, 1.0f, 0.05f, 0.25f)),
          m_x_tail(0.0f, SpringConfig::with_damping_ratio(1800.0f, 1.0f, 1.0f, 0.05f, 0.25f)),
          m_y_spring(0.0f, SpringConfig::with_damping_ratio(3200.0f, 1.0f, 1.0f, 0.05f, 0.25f)) {}

    void set_enabled(bool enabled) noexcept {
        if (m_enabled != enabled) {
            m_enabled = enabled;
            if (!enabled) {
                reset();
            }
        }
    }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    void reset() noexcept {
        m_initialized = false;
        m_has_tick = false;
        m_x_lead.reset(0.0f);
        m_x_tail.reset(0.0f);
        m_y_spring.reset(0.0f);
        m_last_activity = std::chrono::steady_clock::now();
    }

    void notify_activity() noexcept {
        m_last_activity = std::chrono::steady_clock::now();
    }

    FrameState evaluate(float target_x, float target_y, float width, float height,
                        bool focused, bool disabled) noexcept {
        FrameState out{};
        if (!m_enabled || !focused || disabled) {
            m_initialized = false;
            m_has_tick = false;
            return out;
        }

        const auto now = std::chrono::steady_clock::now();
        float dt = 1.0f / 60.0f;
        if (m_has_tick) {
            dt = std::clamp(std::chrono::duration<float>(now - m_last_tick).count(), 0.001f, 0.05f);
        }
        m_last_tick = now;
        m_has_tick = true;

        step_internal(target_x, target_y, dt, now, out, width, height);
        return out;
    }

    FrameState step_for_test(float target_x, float target_y, float width, float height,
                             float dt, float idle_sec = 0.0f) noexcept {
        FrameState out{};
        if (!m_enabled) return out;
        const float clamped_dt = std::clamp(dt, 0.001f, 0.05f);
        const auto base_now = std::chrono::steady_clock::now();
        m_last_activity = base_now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float>(std::max(0.0f, idle_sec)));
        step_internal(target_x, target_y, clamped_dt, base_now, out, width, height);
        return out;
    }

    [[nodiscard]] float current_x() const noexcept { return m_x_lead.value(); }
    [[nodiscard]] float tail_x() const noexcept { return m_x_tail.value(); }
    [[nodiscard]] float current_y() const noexcept { return m_y_spring.value(); }

private:
    void step_internal(float target_x, float target_y, float dt,
                       std::chrono::steady_clock::time_point now,
                       FrameState& out, float width, float height) noexcept {
        if (!m_initialized) {
            m_initialized = true;
            m_x_lead.reset(target_x);
            m_x_tail.reset(target_x);
            m_y_spring.reset(target_y);
            m_target_x = target_x;
            m_target_y = target_y;
            m_last_activity = now;
        } else if (std::abs(target_x - m_target_x) > 0.25f || std::abs(target_y - m_target_y) > 0.25f) {
            const float prev_ty = m_target_y;
            m_target_x = target_x;
            m_target_y = target_y;
            if (std::abs(target_y - prev_ty) > 2.0f) {
                // 跨换行时快速收紧水平拖尾避免横扫整行造成干扰
                m_x_tail.rebind(target_x, 0.0f, target_x);
            } else {
                m_x_tail.rebind(m_x_tail.value(), m_x_tail.velocity(), target_x);
            }
            m_x_lead.rebind(m_x_lead.value(), m_x_lead.velocity(), target_x);
            m_y_spring.rebind(m_y_spring.value(), m_y_spring.velocity(), target_y);
            m_last_activity = now;
        }

        m_x_lead.update(dt);
        m_x_tail.update(dt);
        m_y_spring.update(dt);

        const float base_w = std::max(1.5f, width);
        const float lx = m_x_lead.value();
        const float tx = m_x_tail.value();
        // 计算双弹簧前后缘间距并上限钳制，防止长跳转时过度拉长
        const float raw_span = std::abs(lx - tx);
        const float span = std::min(raw_span, 18.0f);
        const float x0 = (lx >= tx) ? (lx - span) : lx;
        const float eff_w = base_w + span;

        const float idle_s = std::max(0.0f, std::chrono::duration<float>(now - m_last_activity).count());
        float alpha = 1.0f;
        if (idle_s > 0.34f) {
            const float phase = std::fmod(idle_s - 0.34f, 1.08f) / 1.08f;
            const float wave = 0.5f + 0.5f * std::cos(phase * 6.2831853f);
            alpha = 0.16f + 0.84f * wave;
        }

        out.visible = true;
        out.animating = true;
        out.opacity = std::clamp(alpha, 0.0f, 1.0f);
        out.rect = Rect::from_xywh(x0, m_y_spring.value(), eff_w, std::max(4.0f, height));
    }

    bool m_enabled{true};
    bool m_initialized{false};
    bool m_has_tick{false};
    float m_target_x{0.0f};
    float m_target_y{0.0f};
    SpringSolver m_x_lead;
    SpringSolver m_x_tail;
    SpringSolver m_y_spring;
    std::chrono::steady_clock::time_point m_last_tick{};
    std::chrono::steady_clock::time_point m_last_activity{};
};

} // namespace ktui
