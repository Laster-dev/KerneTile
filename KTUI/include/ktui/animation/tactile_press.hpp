#pragma once

#include <ktui/animation/spring_curve.hpp>
#include <ktui/pal/platform_types.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

// ============================================================================
// 指针感知微倾斜与按压缩放物理求解器 (Pointer-Aware Micro-Tilt & Press Scale)
// 结合点击光标相对控件中心的归一化力臂 (nx, ny) 驱动二阶物理弹簧：
// - 整体按压微缩放与物理回弹超调 (press_scale)
// - 按下侧轻量下沉 + 对侧高光抬升的受光向量偏移 (micro-tilt)
// ============================================================================
class TactilePressPhysics {
public:
    explicit TactilePressPhysics(float press_scale = 0.968f, float tilt_shift = 3.5f) noexcept
        : m_press_scale(press_scale),
          m_tilt_shift(tilt_shift),
          m_press_spring(0.0f, SpringConfig(460.0f, 26.0f)),
          m_nx_spring(0.0f, SpringConfig(360.0f, 28.0f)),
          m_ny_spring(0.0f, SpringConfig(360.0f, 28.0f)) {}

    void set_enabled(bool enabled) noexcept {
        if (m_enabled != enabled) {
            m_enabled = enabled;
            if (!enabled) {
                reset();
            }
        }
    }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    void set_tuning(float press_scale, float tilt_shift) noexcept {
        m_press_scale = std::clamp(press_scale, 0.85f, 1.0f);
        m_tilt_shift = std::clamp(tilt_shift, 0.0f, 16.0f);
    }

    void reset() noexcept {
        m_pressed = false;
        m_hovered = false;
        m_press_spring.reset(0.0f);
        m_nx_spring.reset(0.0f);
        m_ny_spring.reset(0.0f);
        m_has_tick = false;
    }

    void on_enter() noexcept {
        m_hovered = true;
    }

    void on_move(PointF local_pos, Vec2 size) noexcept {
        if (!m_enabled) return;
        const auto lever = compute_lever(local_pos, size);
        m_nx_spring.rebind(m_nx_spring.value(), m_nx_spring.velocity(),
                           m_pressed ? lever.x : lever.x * 0.45f);
        m_ny_spring.rebind(m_ny_spring.value(), m_ny_spring.velocity(),
                           m_pressed ? lever.y : lever.y * 0.45f);
        wake();
    }

    void on_down(PointF local_pos, Vec2 size) noexcept {
        if (!m_enabled) return;
        m_pressed = true;
        const auto lever = compute_lever(local_pos, size);
        m_press_spring.rebind(m_press_spring.value(), m_press_spring.velocity(), 1.0f);
        m_nx_spring.rebind(m_nx_spring.value(), m_nx_spring.velocity(), lever.x);
        m_ny_spring.rebind(m_ny_spring.value(), m_ny_spring.velocity(), lever.y);
        wake();
    }

    void on_up() noexcept {
        if (!m_enabled) return;
        m_pressed = false;
        m_press_spring.rebind(m_press_spring.value(), m_press_spring.velocity(), 0.0f);
        if (!m_hovered) {
            m_nx_spring.rebind(m_nx_spring.value(), m_nx_spring.velocity(), 0.0f);
            m_ny_spring.rebind(m_ny_spring.value(), m_ny_spring.velocity(), 0.0f);
        }
        wake();
    }

    void on_exit() noexcept {
        m_hovered = false;
        m_pressed = false;
        if (!m_enabled) return;
        m_press_spring.rebind(m_press_spring.value(), m_press_spring.velocity(), 0.0f);
        m_nx_spring.rebind(m_nx_spring.value(), m_nx_spring.velocity(), 0.0f);
        m_ny_spring.rebind(m_ny_spring.value(), m_ny_spring.velocity(), 0.0f);
        wake();
    }

    bool update() noexcept {
        if (!m_enabled) return false;
        const auto now = std::chrono::steady_clock::now();
        float dt = 1.0f / 60.0f;
        if (m_has_tick) {
            dt = std::clamp(std::chrono::duration<float>(now - m_last_tick).count(), 0.001f, 0.05f);
        }
        m_last_tick = now;
        m_has_tick = true;

        m_press_spring.update(dt);
        m_nx_spring.update(dt);
        m_ny_spring.update(dt);

        const bool active = !m_press_spring.is_settled() ||
                            !m_nx_spring.is_settled() ||
                            !m_ny_spring.is_settled();
        if (!active) {
            m_has_tick = false;
        }
        return active;
    }

    bool step_for_test(float dt) noexcept {
        if (!m_enabled) return false;
        const float clamped = std::clamp(dt, 0.001f, 0.05f);
        m_press_spring.update(clamped);
        m_nx_spring.update(clamped);
        m_ny_spring.update(clamped);
        return !m_press_spring.is_settled() ||
               !m_nx_spring.is_settled() ||
               !m_ny_spring.is_settled();
    }

    [[nodiscard]] bool is_active() const noexcept {
        return m_enabled && (
            m_pressed ||
            std::abs(m_press_spring.value()) > 0.001f ||
            std::abs(m_nx_spring.value()) > 0.001f ||
            std::abs(m_ny_spring.value()) > 0.001f);
    }

    [[nodiscard]] float press_amount() const noexcept { return m_press_spring.value(); }
    [[nodiscard]] float lever_x() const noexcept { return m_nx_spring.value(); }
    [[nodiscard]] float lever_y() const noexcept { return m_ny_spring.value(); }

    [[nodiscard]] float current_scale() const noexcept {
        if (!m_enabled) return 1.0f;
        return 1.0f - (1.0f - m_press_scale) * m_press_spring.value();
    }

    [[nodiscard]] Vec2 current_shift() const noexcept {
        if (!m_enabled) return Vec2{0.0f, 0.0f};
        // 仅在按下压缩时沿力臂发生物理下沉偏移；悬停态仅偏转边缘受光高光而不偏移前景文字
        const float p = std::max(0.0f, m_press_spring.value());
        if (p <= 0.001f) return Vec2{0.0f, 0.0f};
        const float w = p * 0.85f;
        return Vec2{
            m_nx_spring.value() * m_tilt_shift * w,
            m_ny_spring.value() * m_tilt_shift * w + 1.5f * p
        };
    }

    [[nodiscard]] Rect transform_rect(const Rect& r) const noexcept {
        if (!m_enabled) return r;
        const float s = current_scale();
        const Vec2 sh = current_shift();
        const float cx = (r.min_x + r.max_x) * 0.5f + sh.x;
        const float cy = (r.min_y + r.max_y) * 0.5f + sh.y;
        const float hw = r.width() * 0.5f * s;
        const float hh = r.height() * 0.5f * s;
        return Rect{cx - hw, cy - hh, cx + hw, cy + hh};
    }

    [[nodiscard]] Vec2 tilted_light_dir() const noexcept {
        const float lx = -0.18f - m_nx_spring.value() * 0.65f;
        const float ly = -0.98f - m_ny_spring.value() * 0.65f;
        const float len = std::hypot(lx, ly);
        return (len > 1e-4f) ? Vec2{lx / len, ly / len} : Vec2{0.0f, -1.0f};
    }

    [[nodiscard]] float elevation_scale() const noexcept {
        return std::clamp(1.0f - 0.55f * std::max(0.0f, m_press_spring.value()), 0.25f, 1.15f);
    }

private:
    static Vec2 compute_lever(PointF p, Vec2 sz) noexcept {
        const float hw = std::max(sz.x * 0.5f, 1.0f);
        const float hh = std::max(sz.y * 0.5f, 1.0f);
        return Vec2{
            std::clamp((p.x - hw) / hw, -1.0f, 1.0f),
            std::clamp((p.y - hh) / hh, -1.0f, 1.0f)
        };
    }

    void wake() noexcept {
        if (!m_has_tick) {
            m_last_tick = std::chrono::steady_clock::now();
            m_has_tick = true;
        }
    }

    bool m_enabled{false};
    bool m_pressed{false};
    bool m_hovered{false};
    float m_press_scale{0.968f};
    float m_tilt_shift{3.5f};

    SpringSolver m_press_spring;
    SpringSolver m_nx_spring;
    SpringSolver m_ny_spring;
    bool m_has_tick{false};
    std::chrono::steady_clock::time_point m_last_tick{};
};

} // namespace ktui
