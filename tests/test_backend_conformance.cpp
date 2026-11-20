#include "test_common.hpp"
#include "kernetile/cpu/cpu_rasterizer.hpp"
#include "kernetile/cpu/image_compare.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include <iostream>
#include <cassert>
#include <vector>

using namespace kernetile::cpu;
using namespace kernetile::core;
using namespace kernetile::tile;

void test_cpu_conformance() {
    std::cout << "[Test 1] Testing CPU vs Reference Image Conformance (SSIM & Max LSB)..." << std::endl;

    const uint32_t width = 128;
    const uint32_t height = 128;

    std::vector<uint8_t> img_single(width * height * 4, 0);
    std::vector<uint8_t> img_multi(width * height * 4, 0);

    // 构造全要素图元集合
    std::vector<UiPrimitive> primitives;

    // 图元 1: 背景卡片 (带大半径高斯软阴影)
    UiPrimitive p1{};
    p1.min_x = 20.0f;
    p1.min_y = 20.0f;
    p1.max_x = 108.0f;
    p1.max_y = 108.0f;
    p1.radius_top_left = 12.0f;
    p1.radius_top_right = 12.0f;
    p1.radius_bottom_right = 12.0f;
    p1.radius_bottom_left = 12.0f;
    p1.color_r = 0.2f;
    p1.color_g = 0.25f;
    p1.color_b = 0.35f;
    p1.color_a = 0.9f;
    p1.shadow_blur = 6.0f;
    p1.shadow_spread = 2.0f;
    p1.shadow_offset_y = 4.0f;
    primitives.push_back(p1);

    // 图元 2: 内部高亮按钮 (带 2px 等宽描边)
    UiPrimitive p2{};
    p2.min_x = 35.0f;
    p2.min_y = 45.0f;
    p2.max_x = 93.0f;
    p2.max_y = 85.0f;
    p2.radius_top_left = 6.0f;
    p2.radius_top_right = 6.0f;
    p2.radius_bottom_right = 6.0f;
    p2.radius_bottom_left = 6.0f;
    p2.color_r = 0.0f;
    p2.color_g = 0.7f;
    p2.color_b = 0.9f;
    p2.color_a = 1.0f;
    p2.border_width = 2.0f;
    primitives.push_back(p2);

    TileGrid grid(width, height);
    TileBinner binner;
    BinningResult bin_res = binner.bin_primitives(grid, primitives);

    std::vector<uint32_t> dirty_tiles;
    for (uint32_t i = 0; i < grid.get_total_tiles(); ++i) {
        dirty_tiles.push_back(i);
    }

    // 运行 1: 单线程执行
    CpuTileRasterizer::RenderParams params_single{};
    params_single.width = width;
    params_single.height = height;
    params_single.primitives = primitives;
    params_single.tile_indices = bin_res.indices;
    params_single.tile_headers = bin_res.headers;
    params_single.dirty_tiles = dirty_tiles;
    params_single.thread_count = 1;
    CpuTileRasterizer::render(params_single, img_single.data());

    // 运行 2: 多线程并发执行 (4 线程分片)
    CpuTileRasterizer::RenderParams params_multi = params_single;
    params_multi.thread_count = 4;
    CpuTileRasterizer::render(params_multi, img_multi.data());

    // 比较金标像素一致性
    ImageCompareResult comp = ImageCompare::compare_rgba8(img_single.data(), img_multi.data(), width, height);

    std::cout << "  Conformance Comparison:" << std::endl;
    std::cout << "    Max LSB Diff:     " << comp.max_lsb_diff << " (Target <= 1)" << std::endl;
    std::cout << "    Mean Abs Error:   " << comp.mean_abs_error << std::endl;
    std::cout << "    SSIM Metric:      " << comp.ssim << " (Target >= 0.9995)" << std::endl;
    std::cout << "    Differing Pixels: " << comp.differing_pixels << " / " << (width * height) << std::endl;

    assert(comp.max_lsb_diff == 0); // 单线程与多线程在相同瓦片划分下必须 100% 逐位精确相同
    assert(comp.ssim >= 0.9999);

    std::cout << "  -> Backend conformance PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 9 Backend Conformance Tests ===" << std::endl;

    test_cpu_conformance();

    std::cout << "=== All Backend Conformance Tests PASSED (100%) ===" << std::endl;
    return 0;
}
