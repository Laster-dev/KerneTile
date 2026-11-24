#pragma once

#include <cstdint>
#include <algorithm>

namespace kernetile::tile {

struct TileCoord {
    uint32_t x{0};
    uint32_t y{0};
};

class TileGrid {
public:
    static constexpr uint32_t TILE_SIZE = 16;

    constexpr TileGrid(uint32_t width, uint32_t height) noexcept
        : m_width(width),
          m_height(height),
          m_num_tiles_x((width + TILE_SIZE - 1) / TILE_SIZE),
          m_num_tiles_y((height + TILE_SIZE - 1) / TILE_SIZE),
          m_total_tiles(m_num_tiles_x * m_num_tiles_y) {}

    [[nodiscard]] constexpr uint32_t get_width() const noexcept { return m_width; }
    [[nodiscard]] constexpr uint32_t get_height() const noexcept { return m_height; }
    [[nodiscard]] constexpr uint32_t get_num_tiles_x() const noexcept { return m_num_tiles_x; }
    [[nodiscard]] constexpr uint32_t get_num_tiles_y() const noexcept { return m_num_tiles_y; }
    [[nodiscard]] constexpr uint32_t get_total_tiles() const noexcept { return m_total_tiles; }

    [[nodiscard]] constexpr uint32_t get_tile_id(uint32_t tx, uint32_t ty) const noexcept {
        return ty * m_num_tiles_x + tx;
    }

    [[nodiscard]] constexpr TileCoord get_tile_coord(uint32_t tile_id) const noexcept {
        return TileCoord{
            .x = tile_id % m_num_tiles_x,
            .y = tile_id / m_num_tiles_x
        };
    }

    // 将浮点坐标映射至瓦片索引区间 (半开区间 [min, max))
    constexpr void get_intersecting_tiles(
        float min_x, float min_y, float max_x, float max_y,
        uint32_t& out_tx0, uint32_t& out_ty0, uint32_t& out_tx1, uint32_t& out_ty1) const noexcept
    {
        float r_x = (max_x > min_x) ? (max_x - 0.001f) : max_x;
        float r_y = (max_y > min_y) ? (max_y - 0.001f) : max_y;

        int32_t x0 = static_cast<int32_t>(min_x) / static_cast<int32_t>(TILE_SIZE);
        int32_t y0 = static_cast<int32_t>(min_y) / static_cast<int32_t>(TILE_SIZE);
        int32_t x1 = static_cast<int32_t>(r_x) / static_cast<int32_t>(TILE_SIZE);
        int32_t y1 = static_cast<int32_t>(r_y) / static_cast<int32_t>(TILE_SIZE);

        out_tx0 = static_cast<uint32_t>(std::clamp(x0, 0, static_cast<int32_t>(m_num_tiles_x - 1)));
        out_ty0 = static_cast<uint32_t>(std::clamp(y0, 0, static_cast<int32_t>(m_num_tiles_y - 1)));
        out_tx1 = static_cast<uint32_t>(std::clamp(x1, 0, static_cast<int32_t>(m_num_tiles_x - 1)));
        out_ty1 = static_cast<uint32_t>(std::clamp(y1, 0, static_cast<int32_t>(m_num_tiles_y - 1)));
    }

private:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_num_tiles_x;
    uint32_t m_num_tiles_y;
    uint32_t m_total_tiles;
};

} // namespace kernetile::tile
