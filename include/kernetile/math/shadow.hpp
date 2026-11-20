#pragma once

#include "kernetile/math/vec.hpp"
#include <cmath>
#include <algorithm>

namespace kernetile::math {

// Abramowitz & Stegun 四阶误差函数有理逼近 (最大绝对误差 ~8.198e-4)
[[nodiscard]] inline float erf_approx(float x) noexcept {
    float ax = std::abs(x);
    float poly = 1.0f + 0.278393f * ax + 0.230389f * ax * ax + 0.078108f * (ax * ax * ax * ax);
    float poly4 = poly * poly * poly * poly;
    float result = 1.0f - 1.0f / poly4;
    return (x >= 0.0f) ? result : -result;
}

// 一维高斯累积分布函数 (CDF)
[[nodiscard]] inline float gaussian_cdf(float x, float sigma) noexcept {
    if (sigma <= 1e-4f) {
        return (x >= 0.0f) ? 1.0f : 0.0f;
    }
    // 1.0 / sqrt(2) ≈ 0.7071067811865475
    return 0.5f * (1.0f + erf_approx(x * (0.70710678f / sigma)));
}

// Evan Wallace 常数时间解析高斯圆角阴影求值
[[nodiscard]] inline float evaluate_analytic_shadow(
    const Vec2& p,
    const Vec2& in_box_min,
    const Vec2& in_box_max,
    float in_radius,
    float sigma,
    float spread) noexcept
{
    if (sigma <= 0.0f && spread <= 0.0f) {
        return 0.0f;
    }

    Vec2 box_min = in_box_min - Vec2{spread, spread};
    Vec2 box_max = in_box_max + Vec2{spread, spread};
    float radius = std::max(0.0f, in_radius + spread);

    Vec2 half_size = (box_max - box_min) * 0.5f;
    radius = std::min(radius, std::min(half_size.x, half_size.y));

    // 直角矩形解析高斯外积积分
    float fx = gaussian_cdf(box_max.x - p.x, sigma) - gaussian_cdf(box_min.x - p.x, sigma);
    float fy = gaussian_cdf(box_max.y - p.y, sigma) - gaussian_cdf(box_min.y - p.y, sigma);
    float shadow = fx * fy;

    // 圆角积分补偿与大半径 Squircle 修正
    if (radius > 0.0f) {
        Vec2 center = (box_min + box_max) * 0.5f;
        Vec2 q{
            std::abs(p.x - center.x) - (half_size.x - radius),
            std::abs(p.y - center.y) - (half_size.y - radius)
        };

        if (q.x > 0.0f && q.y > 0.0f) {
            float corner_d = q.length() - radius;
            
            // Raph Levien Squircle 修正
            float p_exp = 2.0f + 2.0f * std::clamp(sigma / std::max(1.0f, radius), 0.0f, 1.0f);
            float qx_norm = std::max(0.0f, q.x / radius);
            float qy_norm = std::max(0.0f, q.y / radius);
            float squircle_d = std::pow(std::pow(qx_norm, p_exp) + std::pow(qy_norm, p_exp), 1.0f / p_exp) * radius - radius;
            float t = std::clamp(sigma / (2.0f * radius), 0.0f, 1.0f);
            float effective_d = corner_d * (1.0f - t) + squircle_d * t;

            float corner_factor = gaussian_cdf(-effective_d, sigma);
            shadow = std::min(shadow, corner_factor);
        }
    }

    return std::clamp(shadow, 0.0f, 1.0f);
}

} // namespace kernetile::math
