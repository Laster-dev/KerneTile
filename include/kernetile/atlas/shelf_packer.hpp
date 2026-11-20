#pragma once

#include "kernetile/atlas/atlas_types.hpp"
#include <cstdint>
#include <vector>
#include <optional>
#include <algorithm>

namespace kernetile::atlas {

struct Shelf {
    uint32_t current_x{0};
    uint32_t current_y{0};
    uint32_t height{0};
};

class ShelfAtlasPacker {
public:
    static constexpr uint32_t MAX_HEIGHT_TOLERANCE = 4; // 货架垂直高度聚类容差 (像素)

    explicit ShelfAtlasPacker(uint32_t width = 2048, uint32_t height = 2048)
        : m_width(width), m_height(height) {}

    ~ShelfAtlasPacker() = default;

    // 申请分配一个尺寸为 (w, h) 的图元包围盒 (已含 1px 保护边距)
    [[nodiscard]] std::optional<RectU32> allocate(uint32_t w, uint32_t h) {
        if (w == 0 || h == 0 || w > m_width || h > m_height) {
            return std::nullopt;
        }

        // 1. Shelf Best Height Fit (BHF): 优先在垂直聚类高度契合 (waste <= MAX_HEIGHT_TOLERANCE) 的货架中推进
        int best_shelf_idx = -1;
        uint32_t min_height_waste = UINT32_MAX;

        for (size_t i = 0; i < m_shelves.size(); ++i) {
            const auto& shelf = m_shelves[i];
            if (shelf.current_x + w <= m_width && shelf.height >= h) {
                uint32_t waste = shelf.height - h;
                if (waste <= MAX_HEIGHT_TOLERANCE && waste < min_height_waste) {
                    min_height_waste = waste;
                    best_shelf_idx = static_cast<int>(i);
                    if (waste == 0) break; // 0 废料完美契合
                }
            }
        }

        // 2. 找到高度契合的货架，水平推进
        if (best_shelf_idx != -1) {
            auto& shelf = m_shelves[best_shelf_idx];
            RectU32 alloc_rect{shelf.current_x, shelf.current_y, w, h};
            shelf.current_x += w;
            m_allocated_area += static_cast<uint64_t>(w) * h;
            return alloc_rect;
        }

        // 3. 否则尝试开启高度精准匹配的新货架
        uint32_t new_shelf_y = m_shelves.empty() ? 0 : 
            (m_shelves.back().current_y + m_shelves.back().height);

        if (new_shelf_y + h <= m_height) {
            m_shelves.push_back(Shelf{
                .current_x = w,
                .current_y = new_shelf_y,
                .height = h
            });
            m_allocated_area += static_cast<uint64_t>(w) * h;
            return RectU32{0, new_shelf_y, w, h};
        }

        // 4. 图集底部空间已不足以开启新货架，进行二次尽力装箱 (允许放入任何有剩余空间的现存货架)
        int fallback_shelf_idx = -1;
        uint32_t fallback_min_waste = UINT32_MAX;
        for (size_t i = 0; i < m_shelves.size(); ++i) {
            const auto& shelf = m_shelves[i];
            if (shelf.current_x + w <= m_width && shelf.height >= h) {
                uint32_t waste = shelf.height - h;
                if (waste < fallback_min_waste) {
                    fallback_min_waste = waste;
                    fallback_shelf_idx = static_cast<int>(i);
                }
            }
        }

        if (fallback_shelf_idx != -1) {
            auto& shelf = m_shelves[fallback_shelf_idx];
            RectU32 alloc_rect{shelf.current_x, shelf.current_y, w, h};
            shelf.current_x += w;
            m_allocated_area += static_cast<uint64_t>(w) * h;
            return alloc_rect;
        }

        // 图集彻底装满
        return std::nullopt;
    }

    void reset() noexcept {
        m_shelves.clear();
        m_allocated_area = 0;
    }

    [[nodiscard]] uint32_t get_width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t get_height() const noexcept { return m_height; }
    [[nodiscard]] uint64_t get_allocated_area() const noexcept { return m_allocated_area; }
    [[nodiscard]] uint64_t get_total_area() const noexcept { 
        return static_cast<uint64_t>(m_width) * m_height; 
    }

    // 计算实际空间利用率 (已分配像素 / 总容量)
    [[nodiscard]] double get_utilization() const noexcept {
        return static_cast<double>(m_allocated_area) / static_cast<double>(get_total_area());
    }

    [[nodiscard]] size_t get_shelf_count() const noexcept { return m_shelves.size(); }
    [[nodiscard]] size_t get_free_rect_count() const noexcept { return 0; }

private:
    uint32_t m_width;
    uint32_t m_height;
    uint64_t m_allocated_area{0};
    std::vector<Shelf> m_shelves;
};

} // namespace kernetile::atlas
