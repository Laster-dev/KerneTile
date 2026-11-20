#pragma once

#include <cmath>
#include <algorithm>

namespace kernetile::math {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() noexcept = default;
    constexpr Vec2(float in_x, float in_y) noexcept : x(in_x), y(in_y) {}

    [[nodiscard]] constexpr Vec2 operator+(const Vec2& rhs) const noexcept { return {x + rhs.x, y + rhs.y}; }
    [[nodiscard]] constexpr Vec2 operator-(const Vec2& rhs) const noexcept { return {x - rhs.x, y - rhs.y}; }
    [[nodiscard]] constexpr Vec2 operator*(float scalar) const noexcept { return {x * scalar, y * scalar}; }
    [[nodiscard]] constexpr Vec2 operator/(float scalar) const noexcept { return {x / scalar, y / scalar}; }
    [[nodiscard]] constexpr Vec2 operator*(const Vec2& rhs) const noexcept { return {x * rhs.x, y * rhs.y}; }

    constexpr Vec2& operator+=(const Vec2& rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
    constexpr Vec2& operator-=(const Vec2& rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
    constexpr Vec2& operator*=(float scalar) noexcept { x *= scalar; y *= scalar; return *this; }
    constexpr Vec2& operator/=(float scalar) noexcept { x /= scalar; y /= scalar; return *this; }

    [[nodiscard]] constexpr float dot(const Vec2& rhs) const noexcept { return x * rhs.x + y * rhs.y; }
    [[nodiscard]] constexpr float length_squared() const noexcept { return dot(*this); }
    [[nodiscard]] inline float length() const noexcept { return std::sqrt(length_squared()); }

    [[nodiscard]] inline Vec2 normalized() const noexcept {
        float len = length();
        return (len > 1e-6f) ? (*this / len) : Vec2{0.0f, 0.0f};
    }
};

inline constexpr Vec2 operator*(float scalar, const Vec2& v) noexcept {
    return v * scalar;
}

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vec3() noexcept = default;
    constexpr Vec3(float in_x, float in_y, float in_z) noexcept : x(in_x), y(in_y), z(in_z) {}

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    [[nodiscard]] constexpr Vec3 operator-(const Vec3& rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    [[nodiscard]] constexpr Vec3 operator*(float scalar) const noexcept { return {x * scalar, y * scalar, z * scalar}; }
    [[nodiscard]] constexpr float dot(const Vec3& rhs) const noexcept { return x * rhs.x + y * rhs.y + z * rhs.z; }
};

struct Vec4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{0.0f};

    constexpr Vec4() noexcept = default;
    constexpr Vec4(float in_x, float in_y, float in_z, float in_w) noexcept : x(in_x), y(in_y), z(in_z), w(in_w) {}

    [[nodiscard]] constexpr Vec4 operator+(const Vec4& rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; }
    [[nodiscard]] constexpr Vec4 operator-(const Vec4& rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; }
    [[nodiscard]] constexpr Vec4 operator*(float scalar) const noexcept { return {x * scalar, y * scalar, z * scalar, w * scalar}; }
    [[nodiscard]] constexpr float dot(const Vec4& rhs) const noexcept { return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w; }
};

} // namespace kernetile::math
