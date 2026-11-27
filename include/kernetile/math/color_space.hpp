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

// ============================================================================
// OkLab 感知均匀色彩空间 (Bjorn Ottosson 2020 标准矩阵)
// 消除线性 RGB / sRGB 插值时高饱和对比色之间的浑浊灰暗带与亮度塌陷
// ============================================================================
struct OkLab {
    float L{0.0f};
    float a{0.0f};
    float b{0.0f};

    bool operator==(const OkLab&) const noexcept = default;
};

[[nodiscard]] inline OkLab linear_srgb_to_oklab(float r, float g, float b) noexcept {
    const float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
    const float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
    const float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;

    const float l_ = std::cbrt(std::max(l, 0.0f));
    const float m_ = std::cbrt(std::max(m, 0.0f));
    const float s_ = std::cbrt(std::max(s, 0.0f));

    return OkLab{
        0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
        1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
        0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_
    };
}

inline void oklab_to_linear_srgb(const OkLab& lab, float& out_r, float& out_g, float& out_b) noexcept {
    const float l_ = lab.L + 0.3963377774f * lab.a + 0.2158037573f * lab.b;
    const float m_ = lab.L - 0.1055613458f * lab.a - 0.0638541728f * lab.b;
    const float s_ = lab.L - 0.0894841775f * lab.a - 1.2914855480f * lab.b;

    const float l = l_ * l_ * l_;
    const float m = m_ * m_ * m_;
    const float s = s_ * s_ * s_;

    out_r = std::clamp(+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s, 0.0f, 1.0f);
    out_g = std::clamp(-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s, 0.0f, 1.0f);
    out_b = std::clamp(-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s, 0.0f, 1.0f);
}

[[nodiscard]] inline OkLab srgb_to_oklab(const Color& srgb) noexcept {
    return linear_srgb_to_oklab(
        srgb_to_linear(srgb.r),
        srgb_to_linear(srgb.g),
        srgb_to_linear(srgb.b));
}

[[nodiscard]] inline Color oklab_to_srgb(const OkLab& lab, float alpha = 1.0f) noexcept {
    float lr = 0.0f, lg = 0.0f, lb = 0.0f;
    oklab_to_linear_srgb(lab, lr, lg, lb);
    return Color{
        linear_to_srgb(lr),
        linear_to_srgb(lg),
        linear_to_srgb(lb),
        std::clamp(alpha, 0.0f, 1.0f)
    };
}

// 直通 sRGB 颜色在 OkLab 感知均匀空间中的插值
// 当任一端接近全透明时沿用另一端色相，避免向透明黑过渡时出现暗灰边缘
[[nodiscard]] inline Color lerp_oklab(const Color& c0, const Color& c1, float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= 0.0f) return c0;
    if (t >= 1.0f) return c1;
    const float a0 = std::clamp(c0.a, 0.0f, 1.0f);
    const float a1 = std::clamp(c1.a, 0.0f, 1.0f);
    const float a = a0 + (a1 - a0) * t;
    if (a0 <= 1e-4f && a1 <= 1e-4f) {
        return Color{c0.r + (c1.r - c0.r) * t, c0.g + (c1.g - c0.g) * t, c0.b + (c1.b - c0.b) * t, a};
    }
    const Color eff0 = (a0 <= 1e-4f) ? c1 : c0;
    const Color eff1 = (a1 <= 1e-4f) ? c0 : c1;
    const OkLab lab0 = srgb_to_oklab(eff0);
    const OkLab lab1 = srgb_to_oklab(eff1);
    const OkLab mid{
        lab0.L + (lab1.L - lab0.L) * t,
        lab0.a + (lab1.a - lab0.a) * t,
        lab0.b + (lab1.b - lab0.b) * t
    };
    return oklab_to_srgb(mid, a);
}

// 预乘 sRGB 四元组在 OkLab 感知均匀空间中的插值 (用于着色器/光栅器渐变链路)
[[nodiscard]] inline ColorPremul lerp_oklab_premul(const ColorPremul& p0, const ColorPremul& p1, float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    const Color s0 = (p0.a > 1e-4f)
        ? Color{std::clamp(p0.r / p0.a, 0.0f, 1.0f), std::clamp(p0.g / p0.a, 0.0f, 1.0f), std::clamp(p0.b / p0.a, 0.0f, 1.0f), p0.a}
        : Color{0.0f, 0.0f, 0.0f, 0.0f};
    const Color s1 = (p1.a > 1e-4f)
        ? Color{std::clamp(p1.r / p1.a, 0.0f, 1.0f), std::clamp(p1.g / p1.a, 0.0f, 1.0f), std::clamp(p1.b / p1.a, 0.0f, 1.0f), p1.a}
        : Color{0.0f, 0.0f, 0.0f, 0.0f};
    const Color res = lerp_oklab(s0, s1, t);
    return clamp_premul(res.r * res.a, res.g * res.a, res.b * res.a, res.a);
}

} // namespace kernetile::math
