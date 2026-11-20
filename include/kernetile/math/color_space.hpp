#pragma once

#include "kernetile/math/color.hpp"
#include <cmath>
#include <algorithm>

namespace kernetile::math {

// IEC 61966-2-1 分段阈值常量
constexpr float SRGB_THRESHOLD_LIN = 0.0031308f;
constexpr float SRGB_THRESHOLD_ENC = 0.04045f;

// 非线性 sRGB -> 线性光空间 (单通道)
[[nodiscard]] inline float srgb_to_linear(float c) noexcept {
    c = std::clamp(c, 0.0f, 1.0f);
    if (c <= SRGB_THRESHOLD_ENC) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// 线性光空间 -> 非线性 sRGB (单通道)
[[nodiscard]] inline float linear_to_srgb(float c) noexcept {
    c = std::clamp(c, 0.0f, 1.0f);
    if (c <= SRGB_THRESHOLD_LIN) {
        return c * 12.92f;
    }
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// 直通 sRGB 颜色转换至线性预乘四元组 (RGBA 各 32-bit float)
[[nodiscard]] inline ColorPremul srgb_to_linear_premul(const Color& srgb) noexcept {
    float a = std::clamp(srgb.a, 0.0f, 1.0f);
    float lin_r = srgb_to_linear(srgb.r) * a;
    float lin_g = srgb_to_linear(srgb.g) * a;
    float lin_b = srgb_to_linear(srgb.b) * a;
    return clamp_premul(lin_r, lin_g, lin_b, a);
}

// 线性预乘四元组转换回直通 sRGB 颜色 (用于调试与屏幕呈现)
[[nodiscard]] inline Color linear_premul_to_srgb(const ColorPremul& premul) noexcept {
    if (premul.a <= 1e-6f) {
        return Color::transparent();
    }
    float inv_a = 1.0f / premul.a;
    float straight_lin_r = std::clamp(premul.r * inv_a, 0.0f, 1.0f);
    float straight_lin_g = std::clamp(premul.g * inv_a, 0.0f, 1.0f);
    float straight_lin_b = std::clamp(premul.b * inv_a, 0.0f, 1.0f);
    return Color{
        linear_to_srgb(straight_lin_r),
        linear_to_srgb(straight_lin_g),
        linear_to_srgb(straight_lin_b),
        premul.a
    };
}

} // namespace kernetile::math
