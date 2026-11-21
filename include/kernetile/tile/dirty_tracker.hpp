#pragma once

#include "kernetile/core/primitives.hpp"
#include "kernetile/core/tile_header.hpp"
#include "kernetile/tile/tile_grid.hpp"
#include <span>
#include <vector>

namespace kernetile::tile {

struct DirtyTileInfo {
    uint32_t tx{0};
    uint32_t ty{0};
    uint32_t tile_id{0};
    uint32_t pad{0};
};

struct DirtyTrackingResult {
    std::vector<TileCoord> dirty_coords;    // 脏瓦片二维坐标 (tx, ty) 列表
    std::vector<DirtyTileInfo> dirty_tiles; // GPU 消费的 16 字节对齐结构化元数据列表
    uint32_t dirty_count{0};                // 脏瓦片数量
    double culling_ratio{0.0};              // 脏块剔除率 (0.0 ~ 1.0)
    
    // 间接派发参数 (D3D12_DISPATCH_ARGUMENTS / VkDispatchIndirectCommand 镜像)
    uint32_t dispatch_group_count_x{0};
    uint32_t dispatch_group_count_y{1};
    uint32_t dispatch_group_count_z{1};
};

class DirtyTracker {
public:
    explicit DirtyTracker(size_t initial_tile_capacity = 32400) {
        m_prev_signatures.resize(initial_tile_capacity, 0);
    }

    // 更新帧图元并检测脏瓦片
    [[nodiscard]] const DirtyTrackingResult& update(
        const TileGrid& grid,
        std::span<const core::TileHeader> headers,
        std::span<const uint32_t> indices,
        std::span<const core::UiPrimitive> primitives);

    [[nodiscard]] const DirtyTrackingResult& get_result() const noexcept {
        return m_result;
    }

    void reset() {
        std::fill(m_prev_signatures.begin(), m_prev_signatures.end(), 0);
    }

private:
    std::vector<uint64_t> m_prev_signatures;
    std::vector<uint64_t> m_prim_signatures;
    DirtyTrackingResult m_result;
};

} // namespace kernetile::tile
