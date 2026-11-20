#pragma once

#include <ktui/pal/platform_types.hpp>
#include <limits>
#include <algorithm>
#include <cmath>

namespace ktui {

// 盒布局约束 (Box Constraints - 借鉴 Flutter SOTA 约束模型)
// 规定了子渲染对象允许具有的最小和最大宽高范围
struct BoxConstraints {
    static constexpr float kInfinity = std::numeric_limits<float>::infinity();

    float min_width{0.0f};
    float max_width{kInfinity};
    float min_height{0.0f};
    float max_height{kInfinity};

    constexpr BoxConstraints() noexcept = default;
    constexpr BoxConstraints(float min_w, float max_w, float min_h, float max_h) noexcept
        : min_width(std::max(0.0f, min_w)),
          max_width(std::max(min_w, max_w)),
          min_height(std::max(0.0f, min_h)),
          max_height(std::max(min_h, max_h)) {}

    // 便捷工厂：创建严格紧约束 (Tight Constraints: min == max)
    [[nodiscard]] static constexpr BoxConstraints tight(Vec2 size) noexcept {
        return BoxConstraints(size.x, size.x, size.y, size.y);
    }

    // 便捷工厂：为特定宽高指定紧约束
    [[nodiscard]] static constexpr BoxConstraints tight_for(float width = -1.0f, float height = -1.0f) noexcept {
        return BoxConstraints(
            width >= 0.0f ? width : 0.0f,
            width >= 0.0f ? width : kInfinity,
            height >= 0.0f ? height : 0.0f,
            height >= 0.0f ? height : kInfinity
        );
    }

    // 便捷工厂：创建松约束 (Loose Constraints: min 为 0，max 为指定上限)
    [[nodiscard]] static constexpr BoxConstraints loose(Vec2 max_size) noexcept {
        return BoxConstraints(0.0f, max_size.x, 0.0f, max_size.y);
    }

    // 便捷工厂：尽可能扩展填满
    [[nodiscard]] static constexpr BoxConstraints expand(float width = kInfinity, float height = kInfinity) noexcept {
        return BoxConstraints(
            width == kInfinity ? kInfinity : width,
            width == kInfinity ? kInfinity : width,
            height == kInfinity ? kInfinity : height,
            height == kInfinity ? kInfinity : height
        );
    }

    // 检查是否为紧约束 (宽高均严格固定，无任何弹性伸缩空间)
    [[nodiscard]] constexpr bool is_tight() const noexcept {
        return has_tight_width() && has_tight_height();
    }

    [[nodiscard]] constexpr bool has_tight_width() const noexcept {
        return min_width >= max_width;
    }

    [[nodiscard]] constexpr bool has_tight_height() const noexcept {
        return min_height >= max_height;
    }

    // 检查是否为松约束 (最小宽高均为 0)
    [[nodiscard]] constexpr bool is_loose() const noexcept {
        return min_width == 0.0f && min_height == 0.0f;
    }

    // 检查宽高上限是否受限 (非无穷大)
    [[nodiscard]] constexpr bool has_bounded_width() const noexcept {
        return max_width < kInfinity;
    }

    [[nodiscard]] constexpr bool has_bounded_height() const noexcept {
        return max_height < kInfinity;
    }

    [[nodiscard]] constexpr bool is_unbounded() const noexcept {
        return !has_bounded_width() || !has_bounded_height();
    }

    // 尺寸安全钳制 (Clamp)
    [[nodiscard]] constexpr float constrain_width(float width) const noexcept {
        return std::clamp(width, min_width, max_width);
    }

    [[nodiscard]] constexpr float constrain_height(float height) const noexcept {
        return std::clamp(height, min_height, max_height);
    }

    [[nodiscard]] constexpr Vec2 constrain(Vec2 size) const noexcept {
        return Vec2{constrain_width(size.x), constrain_height(size.y)};
    }

    // 扣除内边距后的内部约束
    [[nodiscard]] constexpr BoxConstraints deflate(const EdgeInsets& insets) const noexcept {
        float horizontal = insets.left + insets.right;
        float vertical = insets.top + insets.bottom;

        float deflated_min_w = std::max(0.0f, min_width - horizontal);
        float deflated_max_w = (max_width == kInfinity) ? kInfinity : std::max(0.0f, max_width - horizontal);
        float deflated_min_h = std::max(0.0f, min_height - vertical);
        float deflated_max_h = (max_height == kInfinity) ? kInfinity : std::max(0.0f, max_height - vertical);

        return BoxConstraints(deflated_min_w, deflated_max_w, deflated_min_h, deflated_max_h);
    }

    bool operator==(const BoxConstraints&) const noexcept = default;
};

} // namespace ktui
