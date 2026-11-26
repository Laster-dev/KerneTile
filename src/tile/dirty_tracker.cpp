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
    std::span<const core::UiPrimitive> primitives,
    std::span<const core::ClipRect> clips)
{
    uint32_t total_tiles = grid.get_total_tiles();
    if (m_prev_signatures.size() < total_tiles) {
        m_prev_signatures.resize(total_tiles, 0);
    }

    m_result.dirty_coords.clear();
    m_result.dirty_tiles.clear();

    // 0a. O(1) 增量预计算裁剪链内容签名 (父裁剪先于子裁剪入栈，直接复用父签名避免 O(depth) 重复遍历)
    const size_t clip_count = clips.size();
    if (m_clip_signatures.size() < clip_count) {
        m_clip_signatures.resize(clip_count);
    }
    for (size_t c = 0; c < clip_count; ++c) {
        const auto& cl = clips[c];
        uint64_t h = (cl.parent_index != core::INVALID_INDEX_U32 && cl.parent_index < c)
            ? m_clip_signatures[cl.parent_index]
            : 0xcbf29ce484222325ULL;
        uint64_t w0 = 0, w1 = 0, w2 = 0;
        std::memcpy(&w0, &cl.min_x, sizeof(uint64_t));          // min_x, min_y
        std::memcpy(&w1, &cl.max_x, sizeof(uint64_t));          // max_x, max_y
        uint32_t rad_bits = 0;
        std::memcpy(&rad_bits, &cl.corner_radius, sizeof(uint32_t));
        w2 = (static_cast<uint64_t>(cl.flags) << 32u) | static_cast<uint64_t>(rad_bits);
        h = fnv1a_64_step(h, w0);
        h = fnv1a_64_step(h, w1);
        h = fnv1a_64_step(h, w2);
        m_clip_signatures[c] = h;
    }

    // 0b. 预计算每个独立图元的 64-bit 内容签名 (不混入数组索引 p，避免前序图元增删引发雪崩失效)
    size_t prim_count = primitives.size();
    if (m_prim_signatures.size() < prim_count) {
        m_prim_signatures.resize(prim_count);
    }
    for (size_t p = 0; p < prim_count; ++p) {
        core::UiPrimitive normalized = primitives[p];
        const uint16_t clip_idx = normalized.clip_index;
        // 剥离瞬时 clip_index 偏移，改为混入实际裁剪几何链签名
        normalized.clip_index = core::INVALID_INDEX_U16;

        uint64_t h = 0xcbf29ce484222325ULL;
        const auto* bytes = reinterpret_cast<const unsigned char*>(&normalized);
        for (size_t offset = 0; offset < sizeof(normalized); offset += sizeof(uint64_t)) {
            uint64_t field;
            std::memcpy(&field, bytes + offset, sizeof(field));
            h = fnv1a_64_step(h, field);
        }
        if (clip_idx != core::INVALID_INDEX_U16 && clip_idx < clip_count) {
            h = fnv1a_64_step(h, m_clip_signatures[clip_idx]);
        }
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

        // 与上一帧签名比对 (若触发了 reset/全量失效，空背景瓦片 count==0 也必须标脏重绘以清除残影)
        if (m_force_all_dirty || current_hash != m_prev_signatures[t]) {
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

    m_force_all_dirty = false;

    m_result.dirty_count = static_cast<uint32_t>(m_result.dirty_coords.size());
    m_result.culling_ratio = 1.0 - (static_cast<double>(m_result.dirty_count) / static_cast<double>(total_tiles));
    m_result.dispatch_group_count_x = m_result.dirty_count;
    m_result.dispatch_group_count_y = 1;
    m_result.dispatch_group_count_z = 1;

    return m_result;
}

} // namespace kernetile::tile
