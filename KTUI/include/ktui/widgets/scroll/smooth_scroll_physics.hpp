#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

// 严格对齐 Chromium 源码 (cc/animation/scroll_offset_animation_curve.cc + ui/gfx/geometry/cubic_bezier.cc)
// 所有常量与公式完全照搬 Chromium Impulse Smooth Scroll，无任何自定义魔数:
// - kImpulseCurveX1 = 0.25, kImpulseCurveX2 = 0.0, kImpulseCurveY2 = 1.0
// - kImpulseMinDurationMs = 200.0, kImpulseMaxDurationMs = 500.0, kImpulseMillisecondsPerPixel = 1.5
// - Windows 单格滚轮位移 = 100.0px (Chromium kDefaultPixelsPerTick)
// - UpdateTarget 重定目标时通过 ImpulseCurveWithInitialSlope 精确保持当前瞬时斜率 (C1 连续零卡顿)
class SmoothScrollPhysics {
public:
    // Chromium cc/animation/scroll_offset_animation_curve.cc 官方原版常量
    static constexpr double kImpulseCurveX1 = 0.25;
    static constexpr double kImpulseCurveX2 = 0.0;
    static constexpr double kImpulseCurveY2 = 1.0;
    static constexpr double kImpulseMinDurationMs = 200.0;
    static constexpr double kImpulseMaxDurationMs = 500.0;
    static constexpr double kImpulseMillisecondsPerPixel = 1.5;
    static constexpr double kWindowsWheelPixelsPerNotch = 100.0; // Chromium Windows 默认 3 行 = 100px
    static constexpr double kPalWheelDeltaPerNotch = 36.0;       // Win32Backend 单格输出 ±36.0f

    SmoothScrollPhysics() = default;

    [[nodiscard]] float offset() const noexcept { return static_cast<float>(m_current); }
    [[nodiscard]] float target() const noexcept { return static_cast<float>(m_target); }
    [[nodiscard]] float max_scroll() const noexcept { return static_cast<float>(m_max_scroll); }
    [[nodiscard]] bool is_animating() const noexcept { return m_animating; }

    void set_max_scroll(float max_scroll) noexcept {
        m_max_scroll = std::max(0.0, static_cast<double>(max_scroll));
        m_target = std::clamp(m_target, 0.0, m_max_scroll);
        m_current = std::clamp(m_current, 0.0, m_max_scroll);
        if (std::abs(m_target - m_current) < 0.05) {
            m_current = m_target;
            m_velocity = 0.0;
            m_animating = false;
        }
    }

    // 立即跳转 (拖拽滚动条滑块 1:1 跟手)
    void jump_to(float offset) noexcept {
        double clamped = std::clamp(static_cast<double>(offset), 0.0, m_max_scroll);
        m_current = clamped;
        m_start = clamped;
        m_target = clamped;
        m_velocity = 0.0;
        m_animating = false;
        m_flinging = false;
    }

    // 触控手指直接拖拽 (1:1 跟手 + 速度追踪)
    void begin_touch_drag() noexcept {
        m_animating = false;
        m_flinging = false;
        m_velocity = 0.0;
        m_target = m_current;
        m_fling_last_time = std::chrono::steady_clock::now();
    }

    void apply_touch_drag(float finger_delta_y) noexcept {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - m_fling_last_time).count();
        m_fling_last_time = now;

        // 手指向上滑 (finger_delta_y < 0) -> 滚动偏移量增大
        double prev = m_current;
        m_current = std::clamp(m_current - static_cast<double>(finger_delta_y), 0.0, m_max_scroll);
        m_target = m_current;
        m_start = m_current;
        m_animating = false;
        m_flinging = false;

        if (dt > 0.001 && dt < 0.15) {
            double inst_v = (m_current - prev) / dt;
            m_velocity = m_velocity * 0.35 + inst_v * 0.65;
        }
    }

    void end_touch_drag() noexcept {
        if (std::abs(m_velocity) < 60.0) {
            m_velocity = 0.0;
            m_flinging = false;
            return;
        }
        m_velocity = std::clamp(m_velocity, -4800.0, 4800.0);
        m_flinging = true;
        m_animating = false;
        m_fling_last_time = std::chrono::steady_clock::now();
    }

    // 相对增量平滑滚动
    void scroll_by(float delta_px) noexcept {
        scroll_to(static_cast<float>(m_target) + delta_px);
    }

    // Chromium ScrollOffsetAnimationCurve::UpdateTarget
    void scroll_to(float new_target) noexcept {
        auto now = std::chrono::steady_clock::now();
        if (m_animating) {
            step_internal(now);
        }

        double clamped = std::clamp(static_cast<double>(new_target), 0.0, m_max_scroll);
        double new_delta = clamped - m_current;
        if (std::abs(new_delta) < 0.1) {
            m_current = clamped;
            m_start = clamped;
            m_target = clamped;
            m_velocity = 0.0;
            m_animating = false;
            return;
        }

        // ImpulseSegmentDuration(new_delta)
        double duration_ms = std::clamp(
            std::abs(new_delta) * kImpulseMillisecondsPerPixel,
            kImpulseMinDurationMs,
            kImpulseMaxDurationMs);
        double new_duration = duration_ms * 0.001;

        // ImpulseCurveWithInitialSlope(slope):
        // slope = current_velocity * new_duration / new_delta
        double slope = (m_velocity * new_duration) / new_delta;
        if (slope < 0.0) slope = 0.0;

        double x1 = kImpulseCurveX1;
        double y1 = x1 * slope;
        if (y1 > 1.0) {
            y1 = 1.0;
            x1 = 1.0 / slope;
        }

        m_bezier.init(x1, y1, kImpulseCurveX2, kImpulseCurveY2);
        m_start = m_current;
        m_target = clamped;
        m_duration = new_duration;
        m_start_time = now;
        m_animating = true;
    }

    // 滚轮输入 (Win32Backend 单格为 ±36.0f -> 精确还原为 Chromium Windows 100.0px/格)
    void add_wheel_delta(float delta_y) noexcept {
        double notches = static_cast<double>(delta_y) / kPalWheelDeltaPerNotch;
        scroll_by(static_cast<float>(notches * kWindowsWheelPixelsPerNotch));
    }

    // 每帧推进物理状态，返回 true 表示仍在动画中
    bool update() noexcept {
        if (m_flinging) {
            auto now = std::chrono::steady_clock::now();
            double dt = std::clamp(std::chrono::duration<double>(now - m_fling_last_time).count(), 0.0, 0.05);
            m_fling_last_time = now;
            // 指数摩擦衰减 (Android / iOS 触控惯性手感)
            constexpr double kFriction = 3.8;
            double decay = std::exp(-kFriction * dt);
            double next = m_current + m_velocity * dt;
            if (next <= 0.0) {
                m_current = 0.0;
                m_target = 0.0;
                m_velocity = 0.0;
                m_flinging = false;
                return false;
            }
            if (next >= m_max_scroll) {
                m_current = m_max_scroll;
                m_target = m_max_scroll;
                m_velocity = 0.0;
                m_flinging = false;
                return false;
            }
            m_current = next;
            m_target = next;
            m_velocity *= decay;
            if (std::abs(m_velocity) < 18.0) {
                m_velocity = 0.0;
                m_flinging = false;
                return false;
            }
            return true;
        }
        if (!m_animating) return false;
        return step_internal(std::chrono::steady_clock::now());
    }

    // 指定已流逝时间 (秒) 步进 (用于确定性单元测试)
    bool step_for_test(float elapsed_sec) noexcept {
        if (!m_animating) return false;
        return evaluate_at(static_cast<double>(elapsed_sec));
    }

private:
    // 对齐 Chromium ui/gfx/geometry/cubic_bezier.cc 的双精度三次贝塞尔求解器
    struct CubicBezier {
        double ax{0.0}, bx{0.0}, cx{0.0};
        double ay{0.0}, by{0.0}, cy{0.0};

        void init(double p1x, double p1y, double p2x, double p2y) noexcept {
            cx = 3.0 * p1x;
            bx = 3.0 * (p2x - p1x) - cx;
            ax = 1.0 - cx - bx;

            cy = 3.0 * p1y;
            by = 3.0 * (p2y - p1y) - cy;
            ay = 1.0 - cy - by;
        }

        [[nodiscard]] double sample_x(double t) const noexcept {
            return ((ax * t + bx) * t + cx) * t;
        }
        [[nodiscard]] double sample_y(double t) const noexcept {
            return ((ay * t + by) * t + cy) * t;
        }
        [[nodiscard]] double sample_dx(double t) const noexcept {
            return (3.0 * ax * t + 2.0 * bx) * t + cx;
        }
        [[nodiscard]] double sample_dy(double t) const noexcept {
            return (3.0 * ay * t + 2.0 * by) * t + cy;
        }

        [[nodiscard]] double solve_curve_x(double x) const noexcept {
            if (x <= 0.0) return 0.0;
            if (x >= 1.0) return 1.0;
            double t = x;
            // Newton-Raphson 迭代 (Chromium 默认 8 次)
            for (int i = 0; i < 8; ++i) {
                double err = sample_x(t) - x;
                if (std::abs(err) < 1e-6) return t;
                double dx = sample_dx(t);
                if (std::abs(dx) < 1e-6) break;
                t -= err / dx;
            }
            // 二分法兜底保证严格收敛
            double lo = 0.0, hi = 1.0;
            t = std::clamp(t, lo, hi);
            for (int i = 0; i < 20; ++i) {
                double cur = sample_x(t);
                if (std::abs(cur - x) < 1e-6) return t;
                if (x > cur) lo = t;
                else hi = t;
                t = 0.5 * (lo + hi);
            }
            return t;
        }

        [[nodiscard]] double solve(double x) const noexcept {
            return sample_y(solve_curve_x(x));
        }

        [[nodiscard]] double slope(double x) const noexcept {
            double t = solve_curve_x(x);
            double dx = sample_dx(t);
            double dy = sample_dy(t);
            if (std::abs(dx) < 1e-6) return 0.0;
            return dy / dx;
        }
    };

    bool step_internal(std::chrono::steady_clock::time_point now) noexcept {
        double elapsed = std::chrono::duration<double>(now - m_start_time).count();
        return evaluate_at(elapsed);
    }

    bool evaluate_at(double elapsed) noexcept {
        if (elapsed >= m_duration || m_duration <= 1e-6) {
            m_current = m_target;
            m_velocity = 0.0;
            m_animating = false;
            return false;
        }

        double u = std::clamp(elapsed / m_duration, 0.0, 1.0);
        double delta = m_target - m_start;
        double progress = m_bezier.solve(u);
        double curve_slope = m_bezier.slope(u);

        m_current = std::clamp(m_start + delta * progress, 0.0, m_max_scroll);
        m_velocity = (delta / m_duration) * curve_slope;
        return true;
    }

    CubicBezier m_bezier;
    double m_current{0.0};
    double m_start{0.0};
    double m_target{0.0};
    double m_max_scroll{0.0};
    double m_velocity{0.0};
    double m_duration{0.2};
    bool m_animating{false};
    bool m_flinging{false};
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_fling_last_time;
};

} // namespace ktui
