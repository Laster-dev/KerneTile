#pragma once

#include <ktui/pal/platform_types.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

namespace ktui {

// UI 几何合成器与脏瓦片吸附对齐计算 (UI Compositor)
// 核心职责：
// 1. 双历史包围盒联合 (Old U New)，彻底消除位移产生的残影；
// 2. 3*sigma 阴影物理外扩计算；
// 3. 16x16 GPU 片上 LDS 瓦片网格严格对齐。
class UICompositor {
public:
    static constexpr float kTileSize = 16.0f;

    UICompositor() = default;

    // 两矩形联合包围盒
    [[nodiscard]] static Rect union_rect(const Rect& a, const Rect& b) noexcept {
        if (a.is_empty()) return b;
        if (b.is_empty()) return a;
        return Rect{
            std::min(a.min_x, b.min_x),
            std::min(a.min_y, b.min_y),
            std::max(a.max_x, b.max_x),
            std::max(a.max_y, b.max_y)
        };
    }

    // 阴影与发光物理外扩 (3 * blur_radius + spread)
    [[nodiscard]] static Rect inflate_shadow(const Rect& box, float blur_radius, float spread) noexcept {
        if (box.is_empty()) return box;
        float margin = 3.0f * blur_radius + std::max(0.0f, spread);
        return box.outset(margin);
    }

    // 16x16 像素瓦片网格向上吸附对齐
    [[nodiscard]] static Rect snap_to_tile_grid(const Rect& r, float tile_size = kTileSize) noexcept {
        if (r.is_empty()) return r;
        float min_x = std::floor(r.min_x / tile_size) * tile_size;
        float min_y = std::floor(r.min_y / tile_size) * tile_size;
        float max_x = std::ceil(r.max_x / tile_size) * tile_size;
        float max_y = std::ceil(r.max_y / tile_size) * tile_size;
        return Rect{min_x, min_y, max_x, max_y};
    }

    // 登记单块脏区域
    void mark_dirty(const Rect& rect) {
        if (!rect.is_empty()) {
            m_dirty_rects.push_back(rect);
        }
    }

    // 登记控件移动/变形产生的双历史脏区域 (包含 Old U New、阴影外扩与 16x16 瓦片吸附)
    void mark_node_dirty(const Rect& old_bounds, const Rect& new_bounds,
                         float shadow_blur = 0.0f, float shadow_spread = 0.0f) {
        // 1. Old U New 联合
        Rect combined = union_rect(old_bounds, new_bounds);

        // 2. 阴影外扩
        if (shadow_blur > 0.0f || shadow_spread > 0.0f) {
            combined = inflate_shadow(combined, shadow_blur, shadow_spread);
        }

        // 3. 16x16 瓦片对齐
        Rect snapped = snap_to_tile_grid(combined);

        mark_dirty(snapped);
    }

    // 计算融合后的整体总脏矩形
    [[nodiscard]] Rect compute_composite_dirty_rect() const noexcept {
        Rect result{};
        for (const auto& r : m_dirty_rects) {
            result = union_rect(result, r);
        }
        return result;
    }

    // 统计覆盖的 16x16 瓦片数量
    [[nodiscard]] size_t estimate_dirty_tile_count() const noexcept {
        Rect composite = compute_composite_dirty_rect();
        if (composite.is_empty()) return 0;

        float width_tiles = std::ceil(composite.width() / kTileSize);
        float height_tiles = std::ceil(composite.height() / kTileSize);
        return static_cast<size_t>(width_tiles * height_tiles);
    }

    void clear() noexcept {
        m_dirty_rects.clear();
    }

    [[nodiscard]] const std::vector<Rect>& dirty_rects() const noexcept {
        return m_dirty_rects;
    }

private:
    std::vector<Rect> m_dirty_rects;
};

} // namespace ktui
