#include "kernetile/tile/dirty_tracker.hpp"
#include <cstring>

namespace kernetile::tile {

static inline uint64_t fnv1a_64_step(uint64_t hash, uint64_t value) noexcept {
    constexpr uint64_t FNV_PRIME = 0x100000001b3ULL;
    hash ^= value;
    hash *= FNV_PRIME;
    return hash;
}

DirtyTrackingResult DirtyTracker::update(
    const TileGrid& grid,
    std::span<const core::TileHeader> headers,
    std::span<const uint32_t> indices,
    std::span<const core::UiPrimitive> primitives)
{
    uint32_t total_tiles = grid.get_total_tiles();
    if (m_prev_signatures.size() < total_tiles) {
        m_prev_signatures.resize(total_tiles, 0);
    }

    DirtyTrackingResult result;
    result.dirty_coords.reserve(total_tiles / 10); // 典型局部重绘预分配
    result.dirty_tiles.reserve(total_tiles / 10);

    for (uint32_t t = 0; t < total_tiles; ++t) {
        const auto& header = headers[t];
        uint32_t count = header.primitive_count;

        uint64_t current_hash = 0;

        if (count > 0) {
            current_hash = 0xcbf29ce484222325ULL; // FNV-1a 64-bit offset basis
            current_hash = fnv1a_64_step(current_hash, count);
            uint32_t offset = header.primitive_offset;

            for (uint32_t i = 0; i < count; ++i) {
                uint32_t prim_id = indices[offset + i];
                current_hash = fnv1a_64_step(current_hash, prim_id);

                const auto& prim = primitives[prim_id];
                // 采样图元的核心物理几何与色彩属性作为内容签名
                uint64_t geom_bits_0;
                std::memcpy(&geom_bits_0, &prim.min_x, sizeof(uint64_t)); // min_x & min_y
                current_hash = fnv1a_64_step(current_hash, geom_bits_0);

                uint64_t color_bits;
                std::memcpy(&color_bits, &prim.color_r, sizeof(uint64_t)); // color_r & color_g
                current_hash = fnv1a_64_step(current_hash, color_bits);
            }
        }

        // 与上一帧签名比对
        if (current_hash != m_prev_signatures[t]) {
            auto coord = grid.get_tile_coord(t);
            result.dirty_coords.push_back(coord);
            result.dirty_tiles.push_back(DirtyTileInfo{
                .tx = coord.x,
                .ty = coord.y,
                .tile_id = t,
                .pad = 0
            });
            m_prev_signatures[t] = current_hash;
        }
    }

    result.dirty_count = static_cast<uint32_t>(result.dirty_coords.size());
    result.culling_ratio = 1.0 - (static_cast<double>(result.dirty_count) / static_cast<double>(total_tiles));
    result.dispatch_group_count_x = result.dirty_count;
    result.dispatch_group_count_y = 1;
    result.dispatch_group_count_z = 1;

    return result;
}

} // namespace kernetile::tile
