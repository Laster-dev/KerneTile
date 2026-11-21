#include "kernetile/tile/dirty_tracker.hpp"
#include <cstring>

namespace kernetile::tile {

static inline uint64_t fnv1a_64_step(uint64_t hash, uint64_t value) noexcept {
    constexpr uint64_t FNV_PRIME = 0x100000001b3ULL;
    hash ^= value;
    hash *= FNV_PRIME;
    return hash;
}

const DirtyTrackingResult& DirtyTracker::update(
    const TileGrid& grid,
    std::span<const core::TileHeader> headers,
    std::span<const uint32_t> indices,
    std::span<const core::UiPrimitive> primitives)
{
    uint32_t total_tiles = grid.get_total_tiles();
    if (m_prev_signatures.size() < total_tiles) {
        m_prev_signatures.resize(total_tiles, 0);
    }

    m_result.dirty_coords.clear();
    m_result.dirty_tiles.clear();

    // 0. 预计算每个独立图元的 64-bit 签名 (将 O(tiles * prims * fields) 降至 O(prims * fields))
    size_t prim_count = primitives.size();
    if (m_prim_signatures.size() < prim_count) {
        m_prim_signatures.resize(prim_count);
    }
    for (size_t p = 0; p < prim_count; ++p) {
        const auto& prim = primitives[p];
        uint64_t h = 0xcbf29ce484222325ULL;
        h = fnv1a_64_step(h, static_cast<uint64_t>(p));
        uint64_t geom_0, geom_1, col_0, col_1, extra;
        std::memcpy(&geom_0, &prim.min_x, sizeof(uint64_t)); // min_x & min_y
        std::memcpy(&geom_1, &prim.max_x, sizeof(uint64_t)); // max_x & max_y
        std::memcpy(&col_0, &prim.color_r, sizeof(uint64_t)); // color_r & color_g
        std::memcpy(&col_1, &prim.color_b, sizeof(uint64_t)); // color_b & color_a
        std::memcpy(&extra, &prim.border_width, sizeof(uint64_t)); // border_width & shadow_blur
        h = fnv1a_64_step(h, geom_0);
        h = fnv1a_64_step(h, geom_1);
        h = fnv1a_64_step(h, col_0);
        h = fnv1a_64_step(h, col_1);
        h = fnv1a_64_step(h, extra);
        m_prim_signatures[p] = h;
    }

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
                current_hash = fnv1a_64_step(current_hash, m_prim_signatures[prim_id]);
            }
        }

        // 与上一帧签名比对
        if (current_hash != m_prev_signatures[t]) {
            auto coord = grid.get_tile_coord(t);
            m_result.dirty_coords.push_back(coord);
            m_result.dirty_tiles.push_back(DirtyTileInfo{
                .tx = coord.x,
                .ty = coord.y,
                .tile_id = t,
                .pad = 0
            });
            m_prev_signatures[t] = current_hash;
        }
    }

    m_result.dirty_count = static_cast<uint32_t>(m_result.dirty_coords.size());
    m_result.culling_ratio = 1.0 - (static_cast<double>(m_result.dirty_count) / static_cast<double>(total_tiles));
    m_result.dispatch_group_count_x = m_result.dirty_count;
    m_result.dispatch_group_count_y = 1;
    m_result.dispatch_group_count_z = 1;

    return m_result;
}

} // namespace kernetile::tile
