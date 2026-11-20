#pragma once

#include "kernetile/core/types.hpp"
#include <cstdint>
#include <type_traits>

namespace kernetile::core {

#pragma pack(push, 1)

// ============================================================================
// 8 字节 TileHeader 结构体定义 (每个 16x16 瓦片的元数据头)
// 内存布局:
// [00-03]: primitive_offset (uint32_t) - 指向瓦片全局图元索引缓冲的起始偏移
// [04-05]: primitive_count  (uint16_t) - 覆盖当前瓦片的图元总数
// [06-07]: flags            (uint16_t) - TileFlags 瓦片状态标志 (如是否脏块/实心遮挡)
// ============================================================================
struct alignas(4) TileHeader {
    uint32_t primitive_offset{0}; // 瓦片图元在全局索引缓冲中的起始偏移
    uint16_t primitive_count{0};  // 瓦片内的图元数量
    uint16_t flags{0};            // TileFlags 枚举值
};

#pragma pack(pop)

// 静态编译期尺寸与对齐验证
static_assert(sizeof(TileHeader) == 8, "TileHeader must be exactly 8 bytes");
static_assert(alignof(TileHeader) == 4, "TileHeader alignment must be 4 bytes");

} // namespace kernetile::core
