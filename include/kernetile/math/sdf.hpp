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
