#include "kernetile/tile/tile_grid.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include "test_common.hpp"
#include <iostream>
#include <cassert>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_tile_binning..." << std::endl;

    using namespace kernetile::tile;
    using namespace kernetile::core;

    // 1. 验证三种标准分辨率瓦片网格参数化 (对齐论文表 2)
    TileGrid grid_1080p(1920, 1080);
    assert(grid_1080p.get_num_tiles_x() == 120);
    assert(grid_1080p.get_num_tiles_y() == 68);
    assert(grid_1080p.get_total_tiles() == 8160);

    TileGrid grid_1440p(2560, 1440);
    assert(grid_1440p.get_num_tiles_x() == 160);
    assert(grid_1440p.get_num_tiles_y() == 90);
    assert(grid_1440p.get_total_tiles() == 14400);

    TileGrid grid_4k(3840, 2160);
    assert(grid_4k.get_num_tiles_x() == 240);
    assert(grid_4k.get_num_tiles_y() == 135);
    assert(grid_4k.get_total_tiles() == 32400);
    std::cout << "  -> Tile grid parameterization (1080p, 1440p, 4K): PASS" << std::endl;

    // 2. 验证 AABB 瓦片投影
    TileBinner binner;
    std::vector<UiPrimitive> primitives;

    // 图元 0: 覆盖坐标 (0, 0) 至 (32, 32)，跨越 2x2 = 4 个瓦片
    UiPrimitive p0{};
    p0.min_x = 0.0f; p0.min_y = 0.0f;
    p0.max_x = 32.0f; p0.max_y = 32.0f;
    p0.color_a = 0.5f; // 半透明
    primitives.push_back(p0);

    auto result = binner.bin_primitives(grid_4k, primitives, false);
    assert(result.headers[grid_4k.get_tile_id(0, 0)].primitive_count == 1);
    assert(result.headers[grid_4k.get_tile_id(1, 0)].primitive_count == 1);
    assert(result.headers[grid_4k.get_tile_id(0, 1)].primitive_count == 1);
    assert(result.headers[grid_4k.get_tile_id(1, 1)].primitive_count == 1);
    assert(result.headers[grid_4k.get_tile_id(2, 2)].primitive_count == 0);
    std::cout << "  -> AABB tile projection: PASS" << std::endl;

    // 3. 验证实心不透明矩形前端遮挡剔除 (Early Solid Occlusion Culling)
    // 图元 1: 完全不透明实心矩形，完整覆盖瓦片 (0, 0)，即 (0..16, 0..16)
    UiPrimitive p1_solid{};
    p1_solid.min_x = 0.0f; p1_solid.min_y = 0.0f;
    p1_solid.max_x = 16.0f; p1_solid.max_y = 16.0f;
    p1_solid.color_a = 1.0f;
    p1_solid.border_width = 0.0f;
    p1_solid.shadow_blur = 0.0f;
    primitives.push_back(p1_solid);

    auto result_cull = binner.bin_primitives(grid_4k, primitives, true);
    
    // 瓦片 (0, 0) 中的底层图元 0 应当被成功遮挡剔除，仅保留图元 1
    uint32_t tile_0_0 = grid_4k.get_tile_id(0, 0);
    assert(result_cull.headers[tile_0_0].primitive_count == 1);
    uint32_t prim_in_tile_0 = result_cull.indices[result_cull.headers[tile_0_0].primitive_offset];
    assert(prim_in_tile_0 == 1 && "Primitive 0 should be culled by solid primitive 1");

    // 瓦片 (1, 1) 未被完全遮挡，仍保留图元 0
    uint32_t tile_1_1 = grid_4k.get_tile_id(1, 1);
    assert(result_cull.headers[tile_1_1].primitive_count == 1);
    assert(result_cull.indices[result_cull.headers[tile_1_1].primitive_offset] == 0);

    std::cout << "  -> Early solid occlusion culling: PASS" << std::endl;
    std::cout << "[TEST] test_tile_binning PASSED successfully!" << std::endl;
    return 0;
}
