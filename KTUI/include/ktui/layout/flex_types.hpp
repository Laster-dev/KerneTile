#pragma once

#include <ktui/pal/platform_types.hpp>
#include <optional>
#include <cstdint>

namespace ktui {

// 二维归一化对齐锚点 (x, y ∈ [-1, 1]: -1 为起点, 0 为居中, +1 为终点)
struct Alignment {
    float x{-1.0f};
    float y{-1.0f};

    constexpr Alignment() noexcept = default;
    constexpr Alignment(float ax, float ay) noexcept : x(ax), y(ay) {}

    [[nodiscard]] static constexpr Alignment top_left() noexcept      { return {-1.0f, -1.0f}; }
    [[nodiscard]] static constexpr Alignment top_center() noexcept    { return { 0.0f, -1.0f}; }
    [[nodiscard]] static constexpr Alignment top_right() noexcept     { return { 1.0f, -1.0f}; }
    [[nodiscard]] static constexpr Alignment center_left() noexcept   { return {-1.0f,  0.0f}; }
    [[nodiscard]] static constexpr Alignment center() noexcept        { return { 0.0f,  0.0f}; }
    [[nodiscard]] static constexpr Alignment center_right() noexcept  { return { 1.0f,  0.0f}; }
    [[nodiscard]] static constexpr Alignment bottom_left() noexcept   { return {-1.0f,  1.0f}; }
    [[nodiscard]] static constexpr Alignment bottom_center() noexcept { return { 0.0f,  1.0f}; }
    [[nodiscard]] static constexpr Alignment bottom_right() noexcept  { return { 1.0f,  1.0f}; }

    [[nodiscard]] constexpr Vec2 compute_offset(Vec2 parent_size, Vec2 child_size) const noexcept {
        float dx = (parent_size.x - child_size.x) * (x + 1.0f) * 0.5f;
        float dy = (parent_size.y - child_size.y) * (y + 1.0f) * 0.5f;
        return Vec2{dx, dy};
    }

    bool operator==(const Alignment&) const noexcept = default;
};

// ZStack 子项约束适配模式
enum class StackFit : uint8_t {
    Loose,  // 向子节点传递松约束，按子项固有尺寸呈现
    Expand  // 强制子节点扩展填满容器尺寸
};

// Flex 主轴排布方向
enum class FlexDirection : uint8_t {
    Row,            // 水平排列 (从左往右)
    Column,         // 垂直排列 (从上往下)
    RowReverse,     // 水平反向排列 (从右往左)
    ColumnReverse   // 垂直反向排列 (从下往上)
};

// 主轴对齐与空间分配方式 (Justify Content)
enum class JustifyContent : uint8_t {
    FlexStart,      // 紧靠主轴起点
    FlexEnd,        // 紧靠主轴终点
    Center,         // 沿主轴居中排列
    SpaceBetween,   // 首尾贴边，剩余空间在子项之间均分
    SpaceAround,    // 每个子项两侧各分配一半等宽间距
    SpaceEvenly     // 首尾与子项之间的所有间隙绝对严格均等
};

// 交叉轴对齐方式 (Align Items / Cross Axis Alignment)
enum class AlignItems : uint8_t {
    FlexStart,      // 紧靠交叉轴起点
    FlexEnd,        // 紧靠交叉轴终点
    Center,         // 沿交叉轴居中对齐
    Stretch         // 拉伸以填满交叉轴可用空间
};

// 弹性子项适配模式
enum class FlexFit : uint8_t {
    Tight,          // 紧凑：强制子项完全扩展并充满分配到的弹性空间
    Loose           // 松散：允许子项小于分配空间，按子项 Intrinsic 尺寸显示
};

// 单个 Flex 子项的父级布局参数 (Flex Item Parent Data)
struct FlexItemData {
    float flex_grow{0.0f};          // 弹性放大权重 (0 表示不参与分配剩余空间)
    float flex_shrink{1.0f};        // 弹性收缩权重 (空间不足时按比例压缩)
    FlexFit fit{FlexFit::Tight};    // 填充模式
    std::optional<AlignItems> align_self{std::nullopt}; // 单项交叉轴对齐覆盖
    bool explicit_shrink{false};    // 是否由 Flexible 显式指定了溢出收缩语义

    bool operator==(const FlexItemData&) const noexcept = default;
};

// 同心嵌套圆角几何推导 (Concentric Nested Corner Radius)
// outer_radius > 0 时保证内层圆角与外层圆角共用圆心中心点：inner_r = max(min_radius, outer_radius - inset)
[[nodiscard]] constexpr float concentric_radius(float outer_radius, float inset, float min_radius = 2.0f) noexcept {
    if (outer_radius <= 0.0f) return 0.0f;
    const float clamped_inset = (inset > 0.0f) ? inset : 0.0f;
    const float raw = outer_radius - clamped_inset;
    return (raw > min_radius) ? raw : min_radius;
}

[[nodiscard]] constexpr float concentric_radius(float outer_radius, const EdgeInsets& pad, float min_radius = 2.0f) noexcept {
    const float h = (pad.left < pad.right) ? pad.left : pad.right;
    const float v = (pad.top < pad.bottom) ? pad.top : pad.bottom;
    return concentric_radius(outer_radius, (h < v) ? h : v, min_radius);
}

} // namespace ktui
