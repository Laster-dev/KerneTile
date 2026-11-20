#pragma once

#include <algorithm>
#include <cstdint>

namespace kernetile::math {

// 直通颜色 (Straight Alpha RGBA)
struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    constexpr Color() noexcept = default;
    constexpr Color(float in_r, float in_g, float in_b, float in_a = 1.0f) noexcept
        : r(in_r), g(in_g), b(in_b), a(in_a) {}

    [[nodiscard]] static constexpr Color transparent() noexcept { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Color white() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Color black() noexcept { return {0.0f, 0.0f, 0.0f, 1.0f}; }
};

// 预乘颜色 (Premultiplied Alpha RGBA)
// 不变量契约: 全程满足 0 <= c <= a <= 1 (防跨后端超色域异常)
struct ColorPremul {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    constexpr ColorPremul() noexcept = default;
    constexpr ColorPremul(float in_r, float in_g, float in_b, float in_a) noexcept
        : r(in_r), g(in_g), b(in_b), a(in_a) {}

    [[nodiscard]] static constexpr ColorPremul transparent() noexcept { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr ColorPremul white() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    [[nodiscard]] static constexpr ColorPremul black() noexcept { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    // 缩放整个预乘颜色 (含 Alpha)
    [[nodiscard]] constexpr ColorPremul operator*(float scalar) const noexcept {
        return {r * scalar, g * scalar, b * scalar, a * scalar};
    }
};

// 严格合法预乘域自动安全钳制
inline constexpr ColorPremul clamp_premul(float r, float g, float b, float a) noexcept {
    a = std::clamp(a, 0.0f, 1.0f);
    r = std::clamp(r, 0.0f, a);
    g = std::clamp(g, 0.0f, a);
    b = std::clamp(b, 0.0f, a);
    return ColorPremul{r, g, b, a};
}

inline constexpr ColorPremul clamp_premul(const ColorPremul& c) noexcept {
    return clamp_premul(c.r, c.g, c.b, c.a);
}

// Porter-Duff Source-Over 预乘合成代数:
// Co = Ca + Cb * (1 - Aa)
// Ao = Aa + Ab * (1 - Aa)
inline constexpr ColorPremul porter_duff_over(const ColorPremul& src, const ColorPremul& dst) noexcept {
    float one_minus_src_a = 1.0f - src.a;
    return ColorPremul{
        src.r + dst.r * one_minus_src_a,
        src.g + dst.g * one_minus_src_a,
        src.b + dst.b * one_minus_src_a,
        src.a + dst.a * one_minus_src_a
    };
}

} // namespace kernetile::math
