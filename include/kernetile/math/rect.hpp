#pragma once

#include "kernetile/math/vec.hpp"
#include <algorithm>

namespace kernetile::math {

struct Rect {
    float min_x{0.0f};
    float min_y{0.0f};
    float max_x{0.0f};
    float max_y{0.0f};

    constexpr Rect() noexcept = default;
    constexpr Rect(float in_min_x, float in_min_y, float in_max_x, float in_max_y) noexcept
        : min_x(in_min_x), min_y(in_min_y), max_x(in_max_x), max_y(in_max_y) {}

    [[nodiscard]] static constexpr Rect from_xywh(float x, float y, float w, float h) noexcept {
        return {x, y, x + w, y + h};
    }

    [[nodiscard]] constexpr float width() const noexcept { return max_x - min_x; }
    [[nodiscard]] constexpr float height() const noexcept { return max_y - min_y; }
    [[nodiscard]] constexpr Vec2 size() const noexcept { return {width(), height()}; }
    [[nodiscard]] constexpr Vec2 center() const noexcept { return {(min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f}; }
    [[nodiscard]] constexpr Vec2 half_extents() const noexcept { return {width() * 0.5f, height() * 0.5f}; }

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return min_x >= max_x || min_y >= max_y;
    }

    [[nodiscard]] constexpr bool contains(const Vec2& point) const noexcept {
        return point.x >= min_x && point.x <= max_x && point.y >= min_y && point.y <= max_y;
    }

    [[nodiscard]] constexpr bool intersects(const Rect& other) const noexcept {
        return min_x < other.max_x && max_x > other.min_x &&
               min_y < other.max_y && max_y > other.min_y;
    }

    [[nodiscard]] constexpr Rect intersection(const Rect& other) const noexcept {
        float x0 = std::max(min_x, other.min_x);
        float y0 = std::max(min_y, other.min_y);
        float x1 = std::min(max_x, other.max_x);
        float y1 = std::min(max_y, other.max_y);
        if (x0 < x1 && y0 < y1) {
            return {x0, y0, x1, y1};
        }
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    [[nodiscard]] constexpr Rect union_with(const Rect& other) const noexcept {
        if (is_empty()) return other;
        if (other.is_empty()) return *this;
        return {
            std::min(min_x, other.min_x),
            std::min(min_y, other.min_y),
            std::max(max_x, other.max_x),
            std::max(max_y, other.max_y)
        };
    }

    [[nodiscard]] constexpr Rect outset(float amount) const noexcept {
        return {min_x - amount, min_y - amount, max_x + amount, max_y + amount};
    }

    [[nodiscard]] constexpr Rect inset(float amount) const noexcept {
        return outset(-amount);
    }

    bool operator==(const Rect&) const noexcept = default;
};

} // namespace kernetile::math
