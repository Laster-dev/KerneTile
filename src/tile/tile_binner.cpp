#include "kernetile/tile/tile_binner.hpp"
#include <algorithm>
#include <cstring>

namespace kernetile::tile {

const BinningResult& TileBinner::bin_primitives(
    const TileGrid& grid,
    std::span<const core::UiPrimitive> primitives,
    bool enable_early_occlusion_culling,
    std::span<const core::ClipRect> clips)
{
    const uint32_t total_tiles = grid.get_total_tiles();
    const uint32_t prim_count = static_cast<uint32_t>(primitives.size());
    const uint32_t clip_count = static_cast<uint32_t>(clips.size());
    const float vp_w = static_cast<float>(grid.get_width());
    const float vp_h = static_cast<float>(grid.get_height());

    m_tile_counts.assign(total_tiles, 0u);
    m_tile_occluded_from.assign(total_tiles, 0u);
    m_prim_meta.resize(prim_count);

    // 1. 预计算图元有效瓦片范围 (包含阴影外扩、裁剪链求交剔除与前端实心遮挡标记)
    for (uint32_t prim_id = 0; prim_id < prim_count; ++prim_id) {
        const auto& prim = primitives[prim_id];
        PrimBinMeta meta{};

        float effective_min_x = prim.min_x;
        float effective_min_y = prim.min_y;
        float effective_max_x = prim.max_x;
        float effective_max_y = prim.max_y;

        // 若包含软阴影，外扩 3 倍 sigma + spread
        if (prim.shadow_blur > 0.0f || prim.shadow_spread > 0.0f) {
            float pad = prim.shadow_blur * 3.0f + prim.shadow_spread;
            effective_min_x = std::min(effective_min_x, prim.min_x + prim.shadow_offset_x - pad);
            effective_min_y = std::min(effective_min_y, prim.min_y + prim.shadow_offset_y - pad);
            effective_max_x = std::max(effective_max_x, prim.max_x + prim.shadow_offset_x + pad);
            effective_max_y = std::max(effective_max_y, prim.max_y + prim.shadow_offset_y + pad);
        }

        // 与关联裁剪链求交，彻底剔除滚动容器或视口外不可见图元
        if (prim.clip_index != core::INVALID_INDEX_U16 && prim.clip_index < clip_count) {
            uint32_t cur = prim.clip_index;
            for (uint32_t depth = 0; depth < 64 && cur != core::INVALID_INDEX_U32 && cur < clip_count; ++depth) {
                const auto& cl = clips[cur];
                effective_min_x = std::max(effective_min_x, cl.min_x);
                effective_min_y = std::max(effective_min_y, cl.min_y);
                effective_max_x = std::min(effective_max_x, cl.max_x);
                effective_max_y = std::min(effective_max_y, cl.max_y);
                cur = cl.parent_index;
            }
        }

        // 视口边界快速裁剪
        if (effective_max_x <= 0.0f || effective_max_y <= 0.0f ||
            effective_min_x >= vp_w || effective_min_y >= vp_h ||
            effective_min_x >= effective_max_x || effective_min_y >= effective_max_y)
        {
            m_prim_meta[prim_id] = meta;
            continue;
        }

        uint32_t tx0, ty0, tx1, ty1;
        grid.get_intersecting_tiles(effective_min_x, effective_min_y, effective_max_x, effective_max_y, tx0, ty0, tx1, ty1);

        const bool is_solid_box =
            enable_early_occlusion_culling &&
            !(prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph)) &&
            (prim.clip_index == core::INVALID_INDEX_U16) &&
            prim.color_a >= 0.999f &&
            prim.border_width <= 0.0f &&
            prim.shadow_blur <= 0.0f &&
            prim.radius_top_left <= 0.0f && prim.radius_top_right <= 0.0f &&
            prim.radius_bottom_right <= 0.0f && prim.radius_bottom_left <= 0.0f;

        meta.tx0 = static_cast<uint16_t>(tx0);
        meta.ty0 = static_cast<uint16_t>(ty0);
        meta.tx1 = static_cast<uint16_t>(tx1);
        meta.ty1 = static_cast<uint16_t>(ty1);
        meta.visible = 1;
        meta.solid = is_solid_box ? 1 : 0;
        m_prim_meta[prim_id] = meta;

        // 记录完全遮盖瓦片的最高层实心矩形索引，后续计数与分散阶段直接跳过其下层图元
        if (is_solid_box) {
            for (uint32_t ty = ty0; ty <= ty1; ++ty) {
                const float tile_pixel_y0 = static_cast<float>(ty * TileGrid::TILE_SIZE);
                const float tile_pixel_y1 = tile_pixel_y0 + static_cast<float>(TileGrid::TILE_SIZE);
                if (prim.min_y > tile_pixel_y0 || prim.max_y < tile_pixel_y1) continue;
                for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                    const float tile_pixel_x0 = static_cast<float>(tx * TileGrid::TILE_SIZE);
                    const float tile_pixel_x1 = tile_pixel_x0 + static_cast<float>(TileGrid::TILE_SIZE);
                    if (prim.min_x <= tile_pixel_x0 && prim.max_x >= tile_pixel_x1) {
                        m_tile_occluded_from[grid.get_tile_id(tx, ty)] = prim_id;
                    }
                }
            }
        }
    }

    // 2. 第一遍扁平计数 (零每瓦片堆分配)
    for (uint32_t prim_id = 0; prim_id < prim_count; ++prim_id) {
        const auto& meta = m_prim_meta[prim_id];
        if (!meta.visible) continue;
        for (uint32_t ty = meta.ty0; ty <= meta.ty1; ++ty) {
            const uint32_t row_base = ty * grid.get_num_tiles_x();
            for (uint32_t tx = meta.tx0; tx <= meta.tx1; ++tx) {
                const uint32_t tile_id = row_base + tx;
                if (prim_id >= m_tile_occluded_from[tile_id]) {
                    m_tile_counts[tile_id]++;
                }
            }
        }
    }

    // 3. 前缀和构建 TileHeader 与单次连续索引缓冲
    m_result.headers.resize(total_tiles);
    m_write_cursors.resize(total_tiles);

    size_t running_offset = 0;
    uint32_t max_count = 0;
    for (uint32_t t = 0; t < total_tiles; ++t) {
        const uint32_t cnt = m_tile_counts[t];
        const uint16_t clamped_cnt = static_cast<uint16_t>(std::min<uint32_t>(cnt, 65535u));
        m_result.headers[t] = core::TileHeader{
            .primitive_offset = static_cast<uint32_t>(running_offset),
            .primitive_count = clamped_cnt,
            .flags = 0
        };
        m_write_cursors[t] = static_cast<uint32_t>(running_offset);
        if (clamped_cnt > max_count) max_count = clamped_cnt;
        running_offset += clamped_cnt;
    }

    m_result.indices.resize(running_offset);

    // 4. 第二遍顺序分散写入 (严格保持图元绘制顺序)
    for (uint32_t prim_id = 0; prim_id < prim_count; ++prim_id) {
        const auto& meta = m_prim_meta[prim_id];
        if (!meta.visible) continue;
        for (uint32_t ty = meta.ty0; ty <= meta.ty1; ++ty) {
            const uint32_t row_base = ty * grid.get_num_tiles_x();
            for (uint32_t tx = meta.tx0; tx <= meta.tx1; ++tx) {
                const uint32_t tile_id = row_base + tx;
                if (prim_id >= m_tile_occluded_from[tile_id]) {
                    const uint32_t end_off = m_result.headers[tile_id].primitive_offset + m_result.headers[tile_id].primitive_count;
                    if (m_write_cursors[tile_id] < end_off) {
                        m_result.indices[m_write_cursors[tile_id]++] = prim_id;
                    }
                }
            }
        }
    }

    m_result.max_primitives_per_tile = max_count;
    m_result.total_indices_count = running_offset;
    return m_result;
}

} // namespace kernetile::tile
