#pragma once

#include <cmath>
#include <algorithm>

namespace ktui {

// 阻尼类型
enum class DampingType {
    Underdamped,        // 欠阻尼 (ζ < 1.0): 弹性震荡自然回弹
    CriticallyDamped,   // 临界阻尼 (ζ == 1.0): 最快收敛，绝对零超调
    Overdamped          // 过阻尼 (ζ > 1.0): 平缓无震荡归位
};

// 物理弹簧参数配置
struct SpringConfig {
    float stiffness = 180.0f;           // 刚度 k (Stiffness / Tension)
    float damping = 24.0f;              // 阻尼系数 c (Damping / Friction)
    float mass = 1.0f;                  // 质量 m
    float rest_threshold = 0.001f;      // 位移静止阈值 (到达该距离内视为静止)
    float velocity_threshold = 0.001f;  // 速度静止阈值 (低于该速度视为静止)

    constexpr SpringConfig() noexcept = default;
    constexpr SpringConfig(float in_stiffness, float in_damping, float in_mass = 1.0f,
                           float in_rest_threshold = 0.001f, float in_velocity_threshold = 0.001f) noexcept
        : stiffness(in_stiffness), damping(in_damping), mass(in_mass),
          rest_threshold(in_rest_threshold), velocity_threshold(in_velocity_threshold) {}

    // 计算固有角频率 ω0 = sqrt(k / m)
    [[nodiscard]] float omega0() const noexcept {
        return (mass > 0.0f && stiffness > 0.0f) ? std::sqrt(stiffness / mass) : 0.0f;
    }

    // 计算阻尼比 ζ = c / (2 * sqrt(m * k))
    [[nodiscard]] float zeta() const noexcept {
        float mk = mass * stiffness;
        return (mk > 0.0f) ? (damping / (2.0f * std::sqrt(mk))) : 1.0f;
    }

    // 便捷工厂：根据刚度与阻尼比构造 (如 ζ=1.0 临界阻尼)
    [[nodiscard]] static SpringConfig with_damping_ratio(float stiffness, float zeta, float mass = 1.0f,
                                                         float rest_thresh = 0.001f, float vel_thresh = 0.001f) noexcept {
        float c = 2.0f * zeta * std::sqrt(mass * stiffness);
        return SpringConfig(stiffness, c, mass, rest_thresh, vel_thresh);
    }

    bool operator==(const SpringConfig&) const noexcept = default;
};

// SOTA 二阶常微分阻尼谐振子解析求解器
// 核心特征：
// 1. 基于数学解析闭式解 (Closed-Form Analytical Solution)，零累积数值积分漂移；
// 2. 任意时刻打断 100% 速度保真 (C^1 连续性)，无视觉顿挫感；
// 3. 严格双重阈值收敛判定，收敛后零 CPU 开销休眠。
class SpringSolver {
public:
    SpringSolver() noexcept = default;

    explicit SpringSolver(float initial_value, const SpringConfig& config = SpringConfig{}) noexcept
        : m_config(config),
          m_target(initial_value),
          m_current_value(initial_value),
          m_current_velocity(0.0f),
          m_elapsed_time(0.0f),
          m_settled(true) {}

    // 以当前瞬时状态为初值，平滑打断并重定向到新目标
    void rebind(float current_val, float current_vel, float new_target) noexcept {
        float y0 = current_val - new_target;
        if (std::abs(y0) <= m_config.rest_threshold && std::abs(current_vel) <= m_config.velocity_threshold) {
            m_target = new_target;
            m_current_value = new_target;
            m_current_velocity = 0.0f;
            m_settled = true;
            m_elapsed_time = 0.0f;
            return;
        }

        m_target = new_target;
        m_current_value = current_val;
        m_current_velocity = current_vel;
        m_v0 = current_vel;
        m_elapsed_time = 0.0f;
        m_settled = false;

        float k = std::max(0.0001f, m_config.stiffness);
        float m = std::max(0.0001f, m_config.mass);
        float c = std::max(0.0f, m_config.damping);

        float w0 = std::sqrt(k / m);
        float zeta = c / (2.0f * std::sqrt(m * k));

        constexpr float eps = 1e-4f;
        if (zeta < 1.0f - eps) {
            // 欠阻尼
            m_type = DampingType::Underdamped;
            m_omega_d = w0 * std::sqrt(1.0f - zeta * zeta);
            m_gamma = zeta * w0;
            m_c1 = y0;
            m_c2 = (current_vel + m_gamma * y0) / m_omega_d;
            m_v_sin_coeff = m_c1 * m_omega_d + m_gamma * m_c2;
        } else if (zeta <= 1.0f + eps) {
            // 临界阻尼
            m_type = DampingType::CriticallyDamped;
            m_omega_d = 0.0f;
            m_gamma = w0;
            m_c1 = y0;
            m_c2 = current_vel + w0 * y0;
            m_v_sin_coeff = 0.0f;
        } else {
            // 过阻尼
            m_type = DampingType::Overdamped;
            float disc = w0 * std::sqrt(zeta * zeta - 1.0f);
            m_r1 = -zeta * w0 + disc;
            m_r2 = -zeta * w0 - disc;
            m_c1 = (current_vel - y0 * m_r2) / (m_r1 - m_r2);
            m_c2 = y0 - m_c1;
            m_v_sin_coeff = 0.0f;
        }
    }

    // 重置并静止在特定数值
    void reset(float value) noexcept {
        m_target = value;
        m_current_value = value;
        m_current_velocity = 0.0f;
        m_v0 = 0.0f;
        m_elapsed_time = 0.0f;
        m_settled = true;
    }

    // 物理步进
    void update(float dt) noexcept {
        if (m_settled) return;

        m_elapsed_time += dt;
        float t = m_elapsed_time;

        float y = 0.0f;
        float v = 0.0f;

        switch (m_type) {
            case DampingType::Underdamped: {
                float decay = std::exp(-m_gamma * t);
                float sin_wt = std::sin(m_omega_d * t);
                float cos_wt = std::cos(m_omega_d * t);
                y = decay * (m_c1 * cos_wt + m_c2 * sin_wt);
                v = decay * (m_v0 * cos_wt - m_v_sin_coeff * sin_wt);
                break;
            }
            case DampingType::CriticallyDamped: {
                float decay = std::exp(-m_gamma * t);
                y = decay * (m_c1 + m_c2 * t);
                v = decay * (m_c2 - m_gamma * (m_c1 + m_c2 * t));
                break;
            }
            case DampingType::Overdamped: {
                float exp_r1 = std::exp(m_r1 * t);
                float exp_r2 = std::exp(m_r2 * t);
                y = m_c1 * exp_r1 + m_c2 * exp_r2;
                v = m_c1 * m_r1 * exp_r1 + m_c2 * m_r2 * exp_r2;
                break;
            }
        }

        float x = m_target + y;

        // 严格收敛判定：位置偏差与瞬时速度均低于阈值
        if (std::abs(y) <= m_config.rest_threshold && std::abs(v) <= m_config.velocity_threshold) {
            m_current_value = m_target;
            m_current_velocity = 0.0f;
            m_settled = true;
        } else {
            m_current_value = x;
            m_current_velocity = v;
        }
    }

    [[nodiscard]] float value() const noexcept { return m_current_value; }
    [[nodiscard]] float velocity() const noexcept { return m_current_velocity; }
    [[nodiscard]] float target() const noexcept { return m_target; }
    [[nodiscard]] bool is_settled() const noexcept { return m_settled; }
    [[nodiscard]] DampingType damping_type() const noexcept { return m_type; }
    [[nodiscard]] const SpringConfig& config() const noexcept { return m_config; }

    void set_config(const SpringConfig& cfg) noexcept {
        m_config = cfg;
    }

private:
    SpringConfig m_config;
    float m_target{0.0f};
    float m_current_value{0.0f};
    float m_current_velocity{0.0f};
    float m_v0{0.0f};
    float m_elapsed_time{0.0f};
    bool m_settled{true};

    // 预解解析系数
    DampingType m_type{DampingType::CriticallyDamped};
    float m_c1{0.0f};
    float m_c2{0.0f};
    float m_gamma{0.0f};
    float m_omega_d{0.0f};
    float m_r1{0.0f};
    float m_r2{0.0f};
    float m_v_sin_coeff{0.0f};
};

} // namespace ktui
