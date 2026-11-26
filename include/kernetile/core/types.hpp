#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace kernetile::core {

// KerneTile 引擎版本常量 (与 CMakeLists.txt 保持一致)
inline constexpr uint32_t kVersionMajor = 1;
inline constexpr uint32_t kVersionMinor = 0;
inline constexpr uint32_t kVersionPatch = 0;
inline constexpr const char* kVersionString = "1.0.0";

// 无效图元/裁剪索引哨兵值
constexpr uint32_t INVALID_INDEX_U32 = 0xFFFFFFFFu;
constexpr uint16_t INVALID_INDEX_U16 = 0xFFFFu;

// 图元标志位枚举定义 (16-bit)
enum class PrimitiveFlags : uint16_t {
    None              = 0x0000,
    HasLinearColor    = 0x0001, // 纯色填充
    HasLinearGradient = 0x0002, // 线性渐变
    HasBorder         = 0x0004, // SDF 等宽描边
    HasShadow         = 0x0008, // 解析高斯软阴影
    IsGlyph           = 0x0010, // 动态图集字形
    IsIcon            = 0x0020, // 动态图集彩色图标
    IsBackdropFilter  = 0x0040, // 背景毛玻璃滤镜
    IsSolidOpaque     = 0x0080, // 完全不透明实心瓦片 (可用于前端遮挡剔除)
    StemDarkening     = 0x0100  // 小字号笔划加粗补偿
};

inline constexpr PrimitiveFlags operator|(PrimitiveFlags a, PrimitiveFlags b) noexcept {
    return static_cast<PrimitiveFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline constexpr PrimitiveFlags operator&(PrimitiveFlags a, PrimitiveFlags b) noexcept {
    return static_cast<PrimitiveFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline constexpr bool has_flag(PrimitiveFlags val, PrimitiveFlags flag) noexcept {
    return (val & flag) == flag;
}

// 瓦片状态标志位枚举 (16-bit)
enum class TileFlags : uint16_t {
    None         = 0x0000,
    IsDirty      = 0x0001, // 脏瓦片 (需重绘)
    IsSolidCover = 0x0002, // 被不透明图元完全遮挡 (可跳过底层)
    HasFilter    = 0x0004  // 包含毛玻璃非局域滤镜
};

inline constexpr TileFlags operator|(TileFlags a, TileFlags b) noexcept {
    return static_cast<TileFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline constexpr TileFlags operator&(TileFlags a, TileFlags b) noexcept {
    return static_cast<TileFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline constexpr bool has_flag(TileFlags val, TileFlags flag) noexcept {
    return (val & flag) == flag;
}

} // namespace kernetile::core
