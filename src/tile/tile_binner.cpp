#include "kernetile/tile/tile_binner.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace kernetile::tile {

const BinningResult& TileBinner::bin_primitives(
    const TileGrid& grid,
    std::span<const core::UiPrimitive> primitives,
    bool enable_early_occlusion_culling,
    std::span<const core::ClipRect> clips)
{
    const uint32_t total_tiles = grid.get_total_tiles();
    const uint32_t num_tiles_x = grid.get_num_tiles_x();
    const uint32_t num_tiles_y = grid.get_num_tiles_y();
    const uint32_t prim_count = static_cast<uint32_t>(primitives.size());
    const uint32_t clip_count = static_cast<uint32_t>(clips.size());
    const float vp_w = static_cast<float>(grid.get_width());
    const float vp_h = static_cast<float>(grid.get_height());

    m_tile_counts.assign(total_tiles, 0u);
    m_tile_occluded_from.assign(total_tiles, 0u);
    m_prim_meta.resize(prim_count);

    // 0. O(1) 增量预计算所有裁剪链的有效交集包围盒与圆角标志 (免除每个图元重复沿父链回溯)
    m_resolved_clips.resize(clip_count);
    for (uint32_t c = 0; c < clip_count; ++c) {
        const auto& cl = clips[c];
        ResolvedClip rc{cl.min_x, cl.min_y, cl.max_x, cl.max_y, cl.corner_radius > 0.0f ? uint8_t(1) : uint8_t(0)};
        if (cl.parent_index != core::INVALID_INDEX_U32 && cl.parent_index < c) {
            const auto& prc = m_resolved_clips[cl.parent_index];
            rc.min_x = std::max(rc.min_x, prc.min_x);
            rc.min_y = std::max(rc.min_y, prc.min_y);
            rc.max_x = std::min(rc.max_x, prc.max_x);
            rc.max_y = std::min(rc.max_y, prc.max_y);
            rc.has_radius = static_cast<uint8_t>(rc.has_radius | prc.has_radius);
        } else if (cl.parent_index != core::INVALID_INDEX_U32 && cl.parent_index < clip_count) {
            // 非拓扑顺序回退保护
            uint32_t cur = cl.parent_index;
            for (uint32_t depth = 0; depth < 64 && cur != core::INVALID_INDEX_U32 && cur < clip_count; ++depth) {
                const auto& pcl = clips[cur];
                rc.min_x = std::max(rc.min_x, pcl.min_x);
                rc.min_y = std::max(rc.min_y, pcl.min_y);
                rc.max_x = std::min(rc.max_x, pcl.max_x);
                rc.max_y = std::min(rc.max_y, pcl.max_y);
                if (pcl.corner_radius > 0.0f) rc.has_radius = 1;
                cur = pcl.parent_index;
            }
        }
        m_resolved_clips[c] = rc;
    }

    // 1. 预计算图元有效瓦片范围 (含零透明剔除、阴影外扩、O(1) 裁剪求交与圆角/裁剪不透明内核早期遮挡剔除)
    for (uint32_t prim_id = 0; prim_id < prim_count; ++prim_id) {
        const auto& prim = primitives[prim_id];
        PrimBinMeta meta{};

        // 零透明不可见图元直接剔除 (不进入任何瓦片、不参与脏块哈希与光栅化)
        const bool is_gradient = (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasLinearGradient)) != 0;
        const bool is_reveal = (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsPointerReveal)) != 0;
        const float max_alpha = is_gradient
            ? std::max(prim.color_a, static_cast<float>(prim.atlas_v1) * (1.0f / 65535.0f))
            : prim.color_a;
        if (max_alpha <= 0.0005f) {
            m_prim_meta[prim_id] = meta;
            continue;
        }

        float effective_min_x = prim.min_x;
        float effective_min_y = prim.min_y;
        float effective_max_x = prim.max_x;
        float effective_max_y = prim.max_y;

        // 若包含软阴影，外扩 3 倍 sigma + spread (跳过渐变与指针揭示图元，避免复用字段触发误扩)
        if (!is_gradient && !is_reveal && (prim.shadow_blur > 0.0f || prim.shadow_spread > 0.0f)) {
            float pad = prim.shadow_blur * 3.0f + prim.shadow_spread;
            effective_min_x = std::min(effective_min_x, prim.min_x + prim.shadow_offset_x - pad);
            effective_min_y = std::min(effective_min_y, prim.min_y + prim.shadow_offset_y - pad);
            effective_max_x = std::max(effective_max_x, prim.max_x + prim.shadow_offset_x + pad);
            effective_max_y = std::max(effective_max_y, prim.max_y + prim.shadow_offset_y + pad);
        }

        // O(1) 裁剪链求交剔除
        const bool has_clip = (prim.clip_index != core::INVALID_INDEX_U16 && prim.clip_index < clip_count);
        bool clip_has_radius = false;
        if (has_clip) {
            const auto& rc = m_resolved_clips[prim.clip_index];
            effective_min_x = std::max(effective_min_x, rc.min_x);
            effective_min_y = std::max(effective_min_y, rc.min_y);
            effective_max_x = std::min(effective_max_x, rc.max_x);
            effective_max_y = std::min(effective_max_y, rc.max_y);
            clip_has_radius = (rc.has_radius != 0);
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

        // 支持带圆角和带直角裁剪链的不透明卡片/面板内接实心区域早期遮挡剔除！
        // (Fluent 2 中 95% 卡片均有 4~8px 圆角，内接无圆角矩形内的所有瓦片 100% 被遮挡)
        const bool can_occlude =
            enable_early_occlusion_culling &&
            !(prim.flags & (static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph) |
                            static_cast<uint16_t>(core::PrimitiveFlags::IsIcon) |
                            static_cast<uint16_t>(core::PrimitiveFlags::HasLinearGradient) |
                            static_cast<uint16_t>(core::PrimitiveFlags::IsPointerReveal))) &&
            !clip_has_radius &&
            prim.color_a >= 0.999f &&
            prim.border_width <= 0.0f &&
            prim.shadow_blur <= 0.0f;

        meta.tx0 = static_cast<uint16_t>(tx0);
        meta.ty0 = static_cast<uint16_t>(ty0);
        meta.tx1 = static_cast<uint16_t>(tx1);
        meta.ty1 = static_cast<uint16_t>(ty1);
        meta.visible = 1;
        meta.solid = can_occlude ? 1 : 0;
        m_prim_meta[prim_id] = meta;

        if (can_occlude) {
            const float half_w = (prim.max_x - prim.min_x) * 0.5f;
            const float half_h = (prim.max_y - prim.min_y) * 0.5f;
            const float cap_r = std::min(half_w, half_h);
            const float max_r = std::min(cap_r, std::max({prim.radius_top_left, prim.radius_top_right,
                                                          prim.radius_bottom_right, prim.radius_bottom_left}));
            // 内接实心矩形 (内缩 max_r + 0.5px 保证边缘抗锯齿像素不漏底，并与直角裁剪区求交)
            const float inset = (max_r > 0.0f) ? (max_r + 0.5f) : 0.0f;
            float in_min_x = prim.min_x + inset;
            float in_min_y = prim.min_y + inset;
            float in_max_x = prim.max_x - inset;
            float in_max_y = prim.max_y - inset;
            if (has_clip) {
                const auto& rc = m_resolved_clips[prim.clip_index];
                in_min_x = std::max(in_min_x, rc.min_x);
                in_min_y = std::max(in_min_y, rc.min_y);
                in_max_x = std::min(in_max_x, rc.max_x);
                in_max_y = std::min(in_max_y, rc.max_y);
            }
            if (in_max_x - in_min_x >= static_cast<float>(TileGrid::TILE_SIZE) &&
                in_max_y - in_min_y >= static_cast<float>(TileGrid::TILE_SIZE))
            {
                constexpr float kInvTile = 1.0f / static_cast<float>(TileGrid::TILE_SIZE);
                int32_t oc_tx0 = std::max<int32_t>(static_cast<int32_t>(tx0), static_cast<int32_t>(std::ceil(in_min_x * kInvTile)));
                int32_t oc_ty0 = std::max<int32_t>(static_cast<int32_t>(ty0), static_cast<int32_t>(std::ceil(in_min_y * kInvTile)));
                int32_t oc_tx1 = std::min<int32_t>(static_cast<int32_t>(tx1), static_cast<int32_t>(std::floor(in_max_x * kInvTile)) - 1);
                int32_t oc_ty1 = std::min<int32_t>(static_cast<int32_t>(ty1), static_cast<int32_t>(std::floor(in_max_y * kInvTile)) - 1);
                if (oc_tx0 <= oc_tx1 && oc_ty0 <= oc_ty1 &&
                    oc_tx1 < static_cast<int32_t>(num_tiles_x) && oc_ty1 < static_cast<int32_t>(num_tiles_y))
                {
                    const uint32_t span_x = static_cast<uint32_t>(oc_tx1 - oc_tx0 + 1);
                    for (int32_t ty = oc_ty0; ty <= oc_ty1; ++ty) {
                        uint32_t* dst = m_tile_occluded_from.data() + static_cast<size_t>(ty) * num_tiles_x + oc_tx0;
                        std::fill_n(dst, span_x, prim_id);
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
            const uint32_t row_base = ty * num_tiles_x;
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
            const uint32_t row_base = ty * num_tiles_x;
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
