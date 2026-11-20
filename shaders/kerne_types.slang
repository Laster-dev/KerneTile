// KerneTile: Shading Types & Struct Definitions
// Binary mirror of kernetile::core types

#ifndef KERNETILE_TYPES_SLANG
#define KERNETILE_TYPES_SLANG

// 80 字节 UiPrimitive 结构体 (GPU 端的二进制镜像)
struct UiPrimitive {
    // 00-15: 几何包围盒 (物理像素坐标)
    float min_x;
    float min_y;
    float max_x;
    float max_y;

    // 16-31: 四角独立圆角半径 (顺时针: 左上、右上、右下、左下)
    float radius_top_left;
    float radius_top_right;
    float radius_bottom_right;
    float radius_bottom_left;

    // 32-47: 线性光空间预乘颜色 (RGBA 各 32-bit float)
    float color_r;
    float color_g;
    float color_b;
    float color_a;

    // 48-63: SDF 描边与解析阴影控制参数
    float border_width;       // 等宽边框宽度 (<= 0 为实心填充)
    float shadow_blur;        // 高斯阴影模糊半径 sigma
    float shadow_spread;      // 阴影外扩距离
    float shadow_offset_x;    // 阴影水平偏移

    // 64-79: 动态图集 UV 映射与管线元标志
    uint atlas_u0_v0;         // 定点化 (u0: 低16位, v0: 高16位)
    uint atlas_u1_v1;         // 定点化 (u1: 低16位, v1: 高16位)
    uint flags_clip_index;    // flags: 低16位, clip_index: 高16位
    float shadow_offset_y;    // 阴影垂直偏移
};

// 32 字节 ClipRect 结构体
struct ClipRect {
    float min_x;
    float min_y;
    float max_x;
    float max_y;

    float corner_radius;
    uint parent_index;
    uint flags;
    uint reserved;
};

// 8 字节 TileHeader 结构体
struct TileHeader {
    uint primitive_offset;
    uint count_and_flags; // count: 低16位, flags: 高16位
};

// 图元标志位掩码常量
static const uint PRIMITIVE_FLAG_HAS_COLOR       = 0x0001;
static const uint PRIMITIVE_FLAG_HAS_GRADIENT    = 0x0002;
static const uint PRIMITIVE_FLAG_HAS_BORDER      = 0x0004;
static const uint PRIMITIVE_FLAG_HAS_SHADOW      = 0x0008;
static const uint PRIMITIVE_FLAG_IS_GLYPH        = 0x0010;
static const uint PRIMITIVE_FLAG_IS_ICON         = 0x0020;
static const uint PRIMITIVE_FLAG_IS_BACKDROP     = 0x0040;
static const uint PRIMITIVE_FLAG_IS_SOLID_OPAQUE = 0x0080;
static const uint PRIMITIVE_FLAG_STEM_DARKENING  = 0x0100;

#endif // KERNETILE_TYPES_SLANG
