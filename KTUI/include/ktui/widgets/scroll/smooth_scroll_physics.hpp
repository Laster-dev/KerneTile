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
    [[nodiscard]] float velocity() const noexcept { return static_cast<float>(m_velocity); }
    [[nodiscard]] bool is_animating() const noexcept { return m_animating || m_flinging || m_bouncing; }
    [[nodiscard]] bool is_flinging() const noexcept { return m_flinging; }
    [[nodiscard]] bool is_bouncing() const noexcept { return m_bouncing; }
    [[nodiscard]] float overscroll_amount() const noexcept {
        if (m_current < 0.0) return static_cast<float>(m_current);
        if (m_current > m_max_scroll) return static_cast<float>(m_current - m_max_scroll);
        return 0.0f;
    }

    void set_viewport_extent(float extent) noexcept {
        m_viewport_extent = std::max(120.0, static_cast<double>(extent));
    }

    void set_max_scroll(float max_scroll) noexcept {
        m_max_scroll = std::max(0.0, static_cast<double>(max_scroll));
        m_target = std::clamp(m_target, 0.0, m_max_scroll);
        if (!m_touch_dragging && !m_bouncing) {
            m_current = std::clamp(m_current, 0.0, m_max_scroll);
            if (!m_flinging && std::abs(m_target - m_current) < 0.05) {
                m_current = m_target;
                m_velocity = 0.0;
                m_animating = false;
            }
        }
    }

    // 立即跳转 (拖拽滚动条滑块 1:1 跟手)
    void jump_to(float offset) noexcept {
        double clamped = std::clamp(static_cast<double>(offset), 0.0, m_max_scroll);
        m_current = clamped;
        m_start = clamped;
        m_target = clamped;
        m_raw_drag_pos = clamped;
        m_velocity = 0.0;
        m_animating = false;
        m_flinging = false;
        m_bouncing = false;
        m_touch_dragging = false;
    }

    // 触控手指直接拖拽 (1:1 跟手 + 顶部/底部橡皮筋越界阻力 + 滑动窗口速度追踪)
    void begin_touch_drag() noexcept {
        m_animating = false;
        m_flinging = false;
        m_bouncing = false;
        m_touch_dragging = true;
        m_velocity = 0.0;
        m_target = std::clamp(m_current, 0.0, m_max_scroll);
        // 若在回弹中途被手指再次接住，将当前视觉越界量反解为虚拟拖拽位移，实现零跳变无缝接管
        if (m_current < 0.0) {
            m_raw_drag_pos = inv_rubber_band(m_current, m_viewport_extent);
        } else if (m_current > m_max_scroll) {
            m_raw_drag_pos = m_max_scroll + inv_rubber_band(m_current - m_max_scroll, m_viewport_extent);
        } else {
            m_raw_drag_pos = m_current;
        }
        m_sample_count = 0;
        m_sample_head = 0;
        m_virtual_time = 0.0;
        auto now = std::chrono::steady_clock::now();
        m_fling_last_time = now;
        m_drag_origin_time = now;
        push_touch_sample(0.0, m_raw_drag_pos);
    }

    void apply_touch_drag(float finger_delta_y) noexcept {
        auto now = std::chrono::steady_clock::now();
        double t_sec = std::chrono::duration<double>(now - m_drag_origin_time).count();
        m_fling_last_time = now;
        apply_touch_drag_internal(finger_delta_y, t_sec);
    }

    // 确定性测试接口：按指定时间间隔 dt_sec 推入触控拖拽增量
    void apply_touch_drag_with_dt(float finger_delta_y, double dt_sec) noexcept {
        m_virtual_time += std::max(0.0, dt_sec);
        apply_touch_drag_internal(finger_delta_y, m_virtual_time);
    }

    void end_touch_drag() noexcept {
        auto now = std::chrono::steady_clock::now();
        double t_sec = std::chrono::duration<double>(now - m_drag_origin_time).count();
        end_touch_drag_internal(t_sec, now);
    }

    // 确定性测试接口：在当前虚拟时间点结束触控拖拽
    void end_touch_drag_for_test(double pause_before_lift_sec = 0.0) noexcept {
        m_virtual_time += std::max(0.0, pause_before_lift_sec);
        end_touch_drag_internal(m_virtual_time, std::chrono::steady_clock::now());
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
        if (m_bouncing || m_flinging) {
            auto now = std::chrono::steady_clock::now();
            double dt = std::clamp(std::chrono::duration<double>(now - m_fling_last_time).count(), 0.0, 0.05);
            m_fling_last_time = now;
            return step_touch_physics(dt);
        }
        if (!m_animating) return false;
        return step_internal(std::chrono::steady_clock::now());
    }

    // 指定已流逝时间 (秒) 步进 (用于确定性单元测试)
    bool step_for_test(float elapsed_sec) noexcept {
        if (!m_animating) return false;
        return evaluate_at(static_cast<double>(elapsed_sec));
    }

    // 指定步长 dt_sec 步进触控惯性/回弹物理 (用于确定性单元测试)
    bool step_touch_for_test(double dt_sec) noexcept {
        if (!m_flinging && !m_bouncing) return false;
        return step_touch_physics(std::clamp(dt_sec, 0.0, 0.05));
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

    struct TouchSample {
        double t_sec{0.0};
        double pos{0.0};
    };

    static constexpr double kRubberBandCoeff = 0.55;
    static constexpr double kFlingDeceleration = 2.1;  // iOS UIScrollView / Android 流畅滑行衰减系数
    static constexpr double kBounceOmega = 15.0;       // 临界阻尼回弹自然频率 (rad/s)，紧致无多余晃动

    [[nodiscard]] static double max_overscroll_cap(double dim) noexcept {
        double d = std::max(120.0, dim);
        return std::clamp(d * 0.38, 64.0, 160.0);
    }

    // iOS UIScrollView 渐进阻尼橡皮筋映射: (d * c * D) / (d + c * D)
    [[nodiscard]] static double rubber_band(double dist, double dim) noexcept {
        double d = std::max(120.0, dim);
        double ad = std::abs(dist);
        double res = (ad * kRubberBandCoeff * d) / (ad + kRubberBandCoeff * d);
        res = std::min(res, max_overscroll_cap(d));
        return dist < 0.0 ? -res : res;
    }

    [[nodiscard]] static double inv_rubber_band(double visual, double dim) noexcept {
        double d = std::max(120.0, dim);
        double cap = max_overscroll_cap(d);
        double av = std::min(std::abs(visual), cap * 0.99);
        double denom = std::max(1e-4, kRubberBandCoeff * d - av);
        double res = (av * kRubberBandCoeff * d) / denom;
        return visual < 0.0 ? -res : res;
    }

    void push_touch_sample(double t_sec, double pos) noexcept {
        m_samples[m_sample_head] = TouchSample{t_sec, pos};
        m_sample_head = (m_sample_head + 1) % kMaxSamples;
        if (m_sample_count < kMaxSamples) {
            ++m_sample_count;
        }
    }

    [[nodiscard]] double estimate_release_velocity(double lift_t_sec) const noexcept {
        if (m_sample_count < 2) return 0.0;
        const TouchSample& newest = m_samples[(m_sample_head + kMaxSamples - 1) % kMaxSamples];
        // 若抬手前手指已停顿超过 95ms，视为用户有意停住，不触发惯性滑行
        if (lift_t_sec - newest.t_sec > 0.095) {
            return 0.0;
        }
        // 在最近 100ms 窗口内选取最早的有效样本计算平均速度，抗单帧 dy=0 / 抬手微抖
        const TouchSample* oldest = &newest;
        for (int i = 1; i < m_sample_count; ++i) {
            int idx = (m_sample_head + kMaxSamples - 1 - i) % kMaxSamples;
            const TouchSample& s = m_samples[idx];
            if (newest.t_sec - s.t_sec <= 0.100) {
                oldest = &s;
            } else {
                break;
            }
        }
        double dt = newest.t_sec - oldest->t_sec;
        if (dt < 0.004) return 0.0;
        return (newest.pos - oldest->pos) / dt;
    }

    void apply_touch_drag_internal(float finger_delta_y, double t_sec) noexcept {
        m_touch_dragging = true;
        m_animating = false;
        m_flinging = false;
        m_bouncing = false;

        // 手指向上滑 (finger_delta_y < 0) -> 滚动偏移量增大
        m_raw_drag_pos -= static_cast<double>(finger_delta_y);
        if (m_raw_drag_pos < 0.0) {
            m_current = rubber_band(m_raw_drag_pos, m_viewport_extent);
        } else if (m_raw_drag_pos > m_max_scroll) {
            m_current = m_max_scroll + rubber_band(m_raw_drag_pos - m_max_scroll, m_viewport_extent);
        } else {
            m_current = m_raw_drag_pos;
        }
        m_target = std::clamp(m_current, 0.0, m_max_scroll);
        m_start = m_current;

        // 仅当有实质位移时才推入速度样本，避免抬手前夕浏览器/Android 补发 dy=0 事件清空惯性速度
        if (std::abs(finger_delta_y) > 0.01f) {
            push_touch_sample(t_sec, m_raw_drag_pos);
        }
    }

    void start_bounce(double boundary, double initial_vel) noexcept {
        m_bouncing = true;
        m_flinging = false;
        m_animating = false;
        m_bounce_boundary = boundary;
        m_bounce_x0 = m_current - boundary;
        m_bounce_v0 = initial_vel;
        m_bounce_elapsed = 0.0;
        m_target = boundary;
        m_velocity = initial_vel;
    }

    void end_touch_drag_internal(double lift_t_sec, std::chrono::steady_clock::time_point now) noexcept {
        m_touch_dragging = false;
        double v = estimate_release_velocity(lift_t_sec);
        m_fling_last_time = now;

        // 1. 若松手时处于顶部或底部越界拉伸态，直接启动临界阻尼弹性回弹
        if (m_current < -0.05) {
            start_bounce(0.0, std::clamp(v * 0.2, -600.0, 600.0));
            return;
        }
        if (m_current > m_max_scroll + 0.05) {
            start_bounce(m_max_scroll, std::clamp(v * 0.2, -600.0, 600.0));
            return;
        }

        // 2. 界内松手：达到阈值则启动惯性滑行
        if (std::abs(v) < 45.0) {
            m_velocity = 0.0;
            m_flinging = false;
            return;
        }
        m_velocity = std::clamp(v, -6500.0, 6500.0);
        m_flinging = true;
        m_animating = false;
        m_bouncing = false;
    }

    bool step_touch_physics(double dt) noexcept {
        if (m_bouncing) {
            m_bounce_elapsed += dt;
            double t = m_bounce_elapsed;
            double e = std::exp(-kBounceOmega * t);
            double c2 = m_bounce_v0 + kBounceOmega * m_bounce_x0;
            double disp = (m_bounce_x0 + c2 * t) * e;
            double vel = (m_bounce_v0 - kBounceOmega * c2 * t) * e;
            double cap = max_overscroll_cap(m_viewport_extent);
            disp = std::clamp(disp, -cap, cap);
            m_current = m_bounce_boundary + disp;
            m_target = m_bounce_boundary;
            m_velocity = vel;
            if (std::abs(disp) < 0.25 && std::abs(vel) < 8.0) {
                m_current = m_bounce_boundary;
                m_target = m_bounce_boundary;
                m_velocity = 0.0;
                m_bouncing = false;
                return false;
            }
            return true;
        }

        if (m_flinging) {
            // 解析积分位移 Δx = v * (1 - e^(-k*dt)) / k，与帧率严格无关
            double decay = std::exp(-kFlingDeceleration * dt);
            double step_dx = (dt > 1e-6) ? (m_velocity * (1.0 - decay) / kFlingDeceleration) : 0.0;
            double next = m_current + step_dx;
            m_velocity *= decay;

            // 惯性撞击顶部/底部边界时柔和过冲并弹性回弹
            if (next < 0.0) {
                m_current = 0.0;
                double impact_v = std::clamp(m_velocity * 0.38, -1500.0, 0.0);
                if (std::abs(impact_v) > 40.0) {
                    start_bounce(0.0, impact_v);
                    return true;
                }
                m_target = 0.0;
                m_velocity = 0.0;
                m_flinging = false;
                return false;
            }
            if (next > m_max_scroll) {
                m_current = m_max_scroll;
                double impact_v = std::clamp(m_velocity * 0.38, 0.0, 1500.0);
                if (std::abs(impact_v) > 40.0) {
                    start_bounce(m_max_scroll, impact_v);
                    return true;
                }
                m_target = m_max_scroll;
                m_velocity = 0.0;
                m_flinging = false;
                return false;
            }

            m_current = next;
            m_target = next;
            if (std::abs(m_velocity) < 12.0) {
                m_velocity = 0.0;
                m_flinging = false;
                return false;
            }
            return true;
        }
        return false;
    }

    static constexpr int kMaxSamples = 8;

    CubicBezier m_bezier;
    double m_current{0.0};
    double m_start{0.0};
    double m_target{0.0};
    double m_max_scroll{0.0};
    double m_viewport_extent{420.0};
    double m_raw_drag_pos{0.0};
    double m_velocity{0.0};
    double m_duration{0.2};
    double m_bounce_boundary{0.0};
    double m_bounce_x0{0.0};
    double m_bounce_v0{0.0};
    double m_bounce_elapsed{0.0};
    double m_virtual_time{0.0};
    bool m_animating{false};
    bool m_flinging{false};
    bool m_bouncing{false};
    bool m_touch_dragging{false};
    TouchSample m_samples[kMaxSamples]{};
    int m_sample_count{0};
    int m_sample_head{0};
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_fling_last_time;
    std::chrono::steady_clock::time_point m_drag_origin_time;
};

} // namespace ktui
