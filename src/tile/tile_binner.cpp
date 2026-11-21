#include "kernetile/tile/tile_binner.hpp"
#include <algorithm>
#include <cstring>

namespace kernetile::tile {

const BinningResult& TileBinner::bin_primitives(
    const TileGrid& grid,
    std::span<const core::UiPrimitive> primitives,
    bool enable_early_occlusion_culling)
{
    uint32_t total_tiles = grid.get_total_tiles();
    if (m_tile_bins.size() < total_tiles) {
        m_tile_bins.resize(total_tiles);
    }

    for (uint32_t t = 0; t < total_tiles; ++t) {
        m_tile_bins[t].clear();
    }

    // 1. 遍历图元进行 AABB 粗投影
    for (uint32_t prim_id = 0; prim_id < static_cast<uint32_t>(primitives.size()); ++prim_id) {
        const auto& prim = primitives[prim_id];

        float effective_min_x = prim.min_x;
        float effective_min_y = prim.min_y;
        float effective_max_x = prim.max_x;
        float effective_max_y = prim.max_y;

        // 若包含软阴影，外扩 3 倍 sigma
        if (prim.shadow_blur > 0.0f || prim.shadow_spread > 0.0f) {
            float pad = prim.shadow_blur * 3.0f + prim.shadow_spread;
            effective_min_x = std::min(effective_min_x, prim.min_x + prim.shadow_offset_x - pad);
            effective_min_y = std::min(effective_min_y, prim.min_y + prim.shadow_offset_y - pad);
            effective_max_x = std::max(effective_max_x, prim.max_x + prim.shadow_offset_x + pad);
            effective_max_y = std::max(effective_max_y, prim.max_y + prim.shadow_offset_y + pad);
        }

        uint32_t tx0, ty0, tx1, ty1;
        grid.get_intersecting_tiles(effective_min_x, effective_min_y, effective_max_x, effective_max_y, tx0, ty0, tx1, ty1);

        // 判断该图元是否为绝对不透明实心矩形
        bool is_solid_box = false;
        if (enable_early_occlusion_culling &&
            prim.color_a >= 0.999f &&
            prim.border_width <= 0.0f &&
            prim.shadow_blur <= 0.0f &&
            prim.radius_top_left <= 0.0f && prim.radius_top_right <= 0.0f &&
            prim.radius_bottom_right <= 0.0f && prim.radius_bottom_left <= 0.0f)
        {
            is_solid_box = true;
        }

        for (uint32_t ty = ty0; ty <= ty1; ++ty) {
            for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                uint32_t tile_id = grid.get_tile_id(tx, ty);

                // 前端实心遮挡剔除测试 (Early Solid Occlusion Culling)
                if (is_solid_box) {
                    float tile_pixel_x0 = static_cast<float>(tx * TileGrid::TILE_SIZE);
                    float tile_pixel_y0 = static_cast<float>(ty * TileGrid::TILE_SIZE);
                    float tile_pixel_x1 = tile_pixel_x0 + static_cast<float>(TileGrid::TILE_SIZE);
                    float tile_pixel_y1 = tile_pixel_y0 + static_cast<float>(TileGrid::TILE_SIZE);

                    if (prim.min_x <= tile_pixel_x0 && prim.max_x >= tile_pixel_x1 &&
                        prim.min_y <= tile_pixel_y0 && prim.max_y >= tile_pixel_y1)
                    {
                        // 当前实心矩形完全遮盖该瓦片全部 256 个像素，清空底层不可见图元
                        m_tile_bins[tile_id].clear();
                    }
                }

                m_tile_bins[tile_id].push_back(prim_id);
            }
        }
    }

    // 2. 扁平化构建 TileHeader 与 TileIndices 连续数组 (单次预分配 + 零重分配 memcpy)
    size_t total_indices = 0;
    for (uint32_t t = 0; t < total_tiles; ++t) {
        total_indices += m_tile_bins[t].size();
    }

    m_result.headers.resize(total_tiles);
    m_result.indices.resize(total_indices);

    size_t running_offset = 0;
    uint32_t max_count = 0;

    for (uint32_t t = 0; t < total_tiles; ++t) {
        const auto& bin = m_tile_bins[t];
        uint16_t count = static_cast<uint16_t>(bin.size());

        m_result.headers[t] = core::TileHeader{
            .primitive_offset = static_cast<uint32_t>(running_offset),
            .primitive_count = count,
            .flags = 0
        };

        if (count > max_count) {
            max_count = count;
        }

        if (count > 0) {
            std::memcpy(m_result.indices.data() + running_offset, bin.data(), count * sizeof(uint32_t));
            running_offset += count;
        }
    }

    m_result.max_primitives_per_tile = max_count;
    m_result.total_indices_count = total_indices;
    return m_result;
}

} // namespace kernetile::tile
