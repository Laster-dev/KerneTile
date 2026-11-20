#pragma once

#include "kernetile/core/types.hpp"
#include <cstdint>
#include <type_traits>

namespace kernetile::core {

#pragma pack(push, 1)

// ============================================================================
// 80 字节 UiPrimitive 结构体定义 (与 GPU 着色器端严格二进制镜像)
// 内存布局:
// [00-15]: 几何包围盒 (min_x, min_y, max_x, max_y)
// [16-31]: 四角独立圆角半径 (radius_top_left, radius_top_right, radius_bottom_right, radius_bottom_left)
// [32-47]: IEC 61966-2-1 线性光空间预乘颜色 (color_r, color_g, color_b, color_a)
// [48-63]: SDF 描边与阴影参数 (border_width, shadow_blur, shadow_spread, shadow_offset_x)
// [64-79]: 图集定点化 UV、控制标志与裁剪索引 (atlas_u0, atlas_v0, atlas_u1, atlas_v1, flags, clip_index, shadow_offset_y)
// ============================================================================
struct alignas(16) UiPrimitive {
    // 00-15: 几何包围盒 (物理像素坐标)
    float min_x{0.0f};
    float min_y{0.0f};
    float max_x{0.0f};
    float max_y{0.0f};

    // 16-31: 四角独立圆角半径 (顺时针: 左上、右上、右下、左下)
    float radius_top_left{0.0f};
    float radius_top_right{0.0f};
    float radius_bottom_right{0.0f};
    float radius_bottom_left{0.0f};

    // 32-47: IEC 61966-2-1 线性光空间预乘颜色 (RGBA 各 32-bit float, 满足 0 <= C <= A <= 1)
    float color_r{0.0f};
    float color_g{0.0f};
    float color_b{0.0f};
    float color_a{1.0f};

    // 48-63: SDF 描边与解析阴影控制参数
    float border_width{0.0f};       // 等宽边框宽度 (<= 0 为实心填充)
    float shadow_blur{0.0f};        // 高斯阴影模糊半径 sigma
    float shadow_spread{0.0f};      // 阴影外扩距离
    float shadow_offset_x{0.0f};    // 阴影水平偏移

    // 64-79: 动态图集 UV 映射与管线元标志
    uint16_t atlas_u0{0};           // 归一化 UV 坐标定点化映射 (0-65535 对应 0.0-1.0)
    uint16_t atlas_v0{0};
    uint16_t atlas_u1{0};
    uint16_t atlas_v1{0};
    uint16_t flags{0};              // PrimitiveFlags 枚举标志
    uint16_t clip_index{INVALID_INDEX_U16}; // 关联的 ClipRect 索引
    float shadow_offset_y{0.0f};    // 阴影垂直偏移
};

// ============================================================================
// 32 字节 ClipRect 结构体定义 (裁剪树节点)
// 内存布局:
// [00-15]: 裁剪绝对包围盒 (min_x, min_y, max_x, max_y)
// [16-31]: 圆角半径、父级索引、标志与保留字段
// ============================================================================
struct alignas(16) ClipRect {
    // 00-15: 裁剪绝对包围盒 (物理像素坐标)
    float min_x{0.0f};
    float min_y{0.0f};
    float max_x{0.0f};
    float max_y{0.0f};

    // 16-31: 圆角与层级元数据
    float corner_radius{0.0f};          // 裁剪圆角半径
    uint32_t parent_index{INVALID_INDEX_U32}; // 父级裁剪矩形索引 (INVALID_INDEX_U32 表示顶层)
    uint32_t flags{0};                  // 标志位 (如是否圆角、是否反向裁剪)
    uint32_t reserved{0};               // 预留对齐填充 (置0)
};

#pragma pack(pop)

// 静态编译期尺寸与对齐验证
static_assert(sizeof(UiPrimitive) == 80, "UiPrimitive must be exactly 80 bytes");
static_assert(sizeof(ClipRect) == 32, "ClipRect must be exactly 32 bytes");
static_assert(alignof(UiPrimitive) == 16, "UiPrimitive alignment must be 16 bytes");
static_assert(alignof(ClipRect) == 16, "ClipRect alignment must be 16 bytes");

} // namespace kernetile::core
