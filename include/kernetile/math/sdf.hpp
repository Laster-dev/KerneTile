#pragma once

#include "kernetile/math/vec.hpp"
#include <cmath>
#include <algorithm>

namespace kernetile::math {

// 四角独立圆角半径参数
struct CornerRadii {
    float top_left{0.0f};
    float top_right{0.0f};
    float bottom_right{0.0f};
    float bottom_left{0.0f};

    constexpr CornerRadii() noexcept = default;
    constexpr CornerRadii(float uniform_r) noexcept
        : top_left(uniform_r), top_right(uniform_r), bottom_right(uniform_r), bottom_left(uniform_r) {}
    constexpr CornerRadii(float tl, float tr, float br, float bl) noexcept
        : top_left(tl), top_right(tr), bottom_right(br), bottom_left(bl) {}
};

// Inigo Quilez 无分支 2D 精确圆角矩形有向距离场 (SDF)
// p: 待求值像素物理坐标
// center: 矩形中心 (min + max) * 0.5
// half_size: 矩形半宽高 (max - min) * 0.5
// radii: 四角独立圆角
[[nodiscard]] inline float sd_rounded_box(
    const Vec2& p, 
    const Vec2& center, 
    const Vec2& half_size, 
    const CornerRadii& radii) noexcept
{
    Vec2 s = p - center;

    // 依象限选取对应圆角半径
    float r = (s.x >= 0.0f) ?
              ((s.y >= 0.0f) ? radii.bottom_right : radii.top_right) :
              ((s.y >= 0.0f) ? radii.bottom_left : radii.top_left);

    r = std::min(r, std::min(half_size.x, half_size.y));

    // 局部象限折叠并平移至内缩角点
    Vec2 q{
        std::abs(s.x) - (half_size.x - r),
        std::abs(s.y) - (half_size.y - r)
    };

    // 无分支欧氏有向距离
    float len = Vec2{std::max(q.x, 0.0f), std::max(q.y, 0.0f)}.length();
    float inner = std::min(std::max(q.x, q.y), 0.0f);
    return len + inner - r;
}

// Apple 连续曲率超椭圆圆角矩形 SDF (G2 Continuous Superellipse / Squircle)
// - 常规圆角 (r/min_half <= 0.65)：采用归一化 L4 超椭圆 (1.35x 过渡延伸 + 一阶梯度校正)，直边到圆角曲率为 0，彻底消除折痕且描边等宽
// - 圆形/胶囊 (r/min_half >= 0.90)：平滑回退至标准 L2 正圆，确保 RadioButton / Slider 滑块 / 胶囊按钮保持正圆不变形
[[nodiscard]] inline float sd_squircle_box(
    const Vec2& p,
    const Vec2& center,
    const Vec2& half_size,
    const CornerRadii& radii) noexcept
{
    const Vec2 s = p - center;
    const float raw_r = (s.x >= 0.0f) ?
                        ((s.y >= 0.0f) ? radii.bottom_right : radii.top_right) :
                        ((s.y >= 0.0f) ? radii.bottom_left : radii.top_left);

    const float abs_dx = std::abs(s.x);
    const float abs_dy = std::abs(s.y);
    const float min_half = std::min(half_size.x, half_size.y);
    const float r = std::clamp(raw_r, 0.0f, min_half);
    if (r <= 0.001f) {
        const float qx = abs_dx - half_size.x;
        const float qy = abs_dy - half_size.y;
        const float out_len = (qx > 0.0f && qy > 0.0f)
            ? std::sqrt(qx * qx + qy * qy)
            : std::max(0.0f, std::max(qx, qy));
        return std::min(std::max(qx, qy), 0.0f) + out_len;
    }

    const float ratio = r / std::max(min_half, 1e-4f);
    const float squircle_t = std::clamp((0.90f - ratio) * 4.0f, 0.0f, 1.0f);

    const float qx2 = abs_dx - half_size.x + r;
    const float qy2 = abs_dy - half_size.y + r;
    const float out2 = (qx2 > 0.0f && qy2 > 0.0f)
        ? std::sqrt(qx2 * qx2 + qy2 * qy2)
        : std::max(0.0f, std::max(qx2, qy2));
    const float d_circle = std::min(std::max(qx2, qy2), 0.0f) + out2 - r;

    if (squircle_t <= 0.001f) {
        return d_circle;
    }

    const float r_eff = std::min(min_half, r * 1.35f);
    const float qx4 = abs_dx - half_size.x + r_eff;
    const float qy4 = abs_dy - half_size.y + r_eff;
    float d_squircle;
    if (qx4 > 0.0f && qy4 > 0.0f) {
        const float inv_r = 1.0f / r_eff;
        const float u = qx4 * inv_r;
        const float v = qy4 * inv_r;
        const float u2 = u * u, v2 = v * v;
        const float u4 = u2 * u2, v4 = v2 * v2;
        const float s4 = u4 + v4;
        const float s2 = std::sqrt(s4);
        const float d4 = std::sqrt(s2);
        const float denom = s2 * d4;
        const float grad = (denom > 1e-6f) ? (std::sqrt(u4 * u2 + v4 * v2) / denom) : 1.0f;
        d_squircle = (d4 - 1.0f) * r_eff / std::max(grad, 0.5f);
    } else {
        d_squircle = std::max(qx4, qy4) - r_eff;
    }

    return d_circle + (d_squircle - d_circle) * squircle_t;
}

// 环形折叠描边算子 (等宽边框, t = stroke_width * 0.5)
[[nodiscard]] inline float op_ring(float d, float stroke_width) noexcept {
    return std::abs(d) - stroke_width * 0.5f;
}

// 多项式平滑最小值算子
[[nodiscard]] inline float smooth_min(float a, float b, float k) noexcept {
    float h = std::max(k - std::abs(a - b), 0.0f) / k;
    return std::min(a, b) - h * h * k * 0.25f;
}

// 亚像素抗锯齿覆盖率计算 (1 像素过渡带)
[[nodiscard]] inline float evaluate_coverage(float d) noexcept {
    return std::clamp(0.5f - d, 0.0f, 1.0f);
}

} // namespace kernetile::math
