#pragma once

#include <cmath>
#include <algorithm>

namespace ktui {

// 三次贝塞尔曲线求解器 (Cubic Bezier Motion Curve)
// 参照 CSS Transitions / Flutter Curves / Material Design 3 / WinUI Fluent 工业实现
// 采用 8 步 Newton-Raphson 牛顿迭代法求解自变量参数 u，并在退化斜率时安全回退二分法 (Bisection)
class CubicBezier {
public:
    constexpr CubicBezier(float x1, float y1, float x2, float y2) noexcept
        : m_x1(x1), m_y1(y1), m_x2(x2), m_y2(y2) {}

    // 计算 t 对应的曲线纵坐标 (输出范围在越冲曲线下可超越 [0, 1])
    [[nodiscard]] float evaluate(float t) const noexcept {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;

        // 若为纯对角线直接恒等返回 (零开销优化)
        if (m_x1 == m_y1 && m_x2 == m_y2) {
            return t;
        }

        float u = solve_u(t);
        return sample_y(u);
    }

    [[nodiscard]] constexpr float x1() const noexcept { return m_x1; }
    [[nodiscard]] constexpr float y1() const noexcept { return m_y1; }
    [[nodiscard]] constexpr float x2() const noexcept { return m_x2; }
    [[nodiscard]] constexpr float y2() const noexcept { return m_y2; }

    bool operator==(const CubicBezier&) const noexcept = default;

private:
    [[nodiscard]] float sample_x(float u) const noexcept {
        // x(u) = 3(1-u)^2 * u * x1 + 3(1-u) * u^2 * x2 + u^3
        float inv = 1.0f - u;
        return 3.0f * inv * inv * u * m_x1 + 3.0f * inv * u * u * m_x2 + u * u * u;
    }

    [[nodiscard]] float sample_y(float u) const noexcept {
        // y(u) = 3(1-u)^2 * u * y1 + 3(1-u) * u^2 * y2 + u^3
        float inv = 1.0f - u;
        return 3.0f * inv * inv * u * m_y1 + 3.0f * inv * u * u * m_y2 + u * u * u;
    }

    [[nodiscard]] float sample_dx(float u) const noexcept {
        // dx/du = 3 * inv^2 * x1 + 6 * inv * u * (x2 - x1) + 3 * u^2 * (1 - x2)
        float inv = 1.0f - u;
        return 3.0f * inv * inv * m_x1 + 6.0f * inv * u * (m_x2 - m_x1) + 3.0f * u * u * (1.0f - m_x2);
    }

    [[nodiscard]] float solve_u(float target_x) const noexcept {
        // 1. 牛顿-拉夫逊 (Newton-Raphson) 迭代求解
        float u = target_x;
        for (int i = 0; i < 8; ++i) {
            float current_x = sample_x(u) - target_x;
            if (std::abs(current_x) < 1e-6f) {
                return u;
            }
            float dx = sample_dx(u);
            if (std::abs(dx) < 1e-6f) {
                break; // 导数过小，跳出进入二分查找
            }
            u -= current_x / dx;
        }

        // 2. 回退健壮二分法 (Bisection Search)
        float low = 0.0f;
        float high = 1.0f;
        u = target_x;
        for (int i = 0; i < 12; ++i) {
            float current_x = sample_x(u);
            if (std::abs(current_x - target_x) < 1e-6f) {
                return u;
            }
            if (target_x > current_x) {
                low = u;
            } else {
                high = u;
            }
            u = (high + low) * 0.5f;
        }
        return u;
    }

    float m_x1{0.0f};
    float m_y1{0.0f};
    float m_x2{1.0f};
    float m_y2{1.0f};
};

// ==================== 国际主流现代设计系统曲线预设 ====================
namespace Curves {

// 1. Web / CSS 工业标准
inline constexpr CubicBezier Linear{0.0f, 0.0f, 1.0f, 1.0f};
inline constexpr CubicBezier Ease{0.25f, 0.1f, 0.25f, 1.0f};
inline constexpr CubicBezier EaseIn{0.42f, 0.0f, 1.0f, 1.0f};
inline constexpr CubicBezier EaseOut{0.0f, 0.0f, 0.58f, 1.0f};
inline constexpr CubicBezier EaseInOut{0.42f, 0.0f, 0.58f, 1.0f};

// 2. Microsoft Windows 11 Fluent 桌面标准
inline constexpr CubicBezier FluentStandard{0.8f, 0.0f, 0.2f, 1.0f};
inline constexpr CubicBezier FluentDecelerate{0.1f, 0.9f, 0.2f, 1.0f};
inline constexpr CubicBezier FluentAccelerate{0.9f, 0.1f, 1.0f, 0.2f};

// 3. Google Material Design 3 标准
inline constexpr CubicBezier MaterialStandard{0.2f, 0.0f, 0.0f, 1.0f};
inline constexpr CubicBezier MaterialDecelerate{0.0f, 0.0f, 0.0f, 1.0f};
inline constexpr CubicBezier MaterialAccelerate{0.3f, 0.0f, 1.0f, 1.0f};
inline constexpr CubicBezier MaterialEmphasized{0.2f, 0.0f, 0.0f, 1.0f};
inline constexpr CubicBezier MaterialEmphasizedDecelerate{0.05f, 0.7f, 0.1f, 1.0f};
inline constexpr CubicBezier MaterialEmphasizedAccelerate{0.3f, 0.0f, 0.8f, 0.15f};

} // namespace Curves

} // namespace ktui
