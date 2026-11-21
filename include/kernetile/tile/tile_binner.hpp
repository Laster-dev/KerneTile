#pragma once

#include "kernetile/core/primitives.hpp"
#include "kernetile/core/tile_header.hpp"
#include "kernetile/tile/tile_grid.hpp"
#include <span>
#include <vector>

namespace kernetile::tile {

struct BinningResult {
    std::vector<core::TileHeader> headers;
    std::vector<uint32_t> indices;
    uint32_t max_primitives_per_tile{0};
    size_t total_indices_count{0};
};

class TileBinner {
public:
    explicit TileBinner(size_t initial_tile_capacity = 32400) {
        m_tile_bins.resize(initial_tile_capacity);
    }

    [[nodiscard]] const BinningResult& bin_primitives(
        const TileGrid& grid,
        std::span<const core::UiPrimitive> primitives,
        bool enable_early_occlusion_culling = true);

    [[nodiscard]] const BinningResult& get_result() const noexcept {
        return m_result;
    }

private:
    std::vector<std::vector<uint32_t>> m_tile_bins;
    BinningResult m_result;
};

} // namespace kernetile::tile
