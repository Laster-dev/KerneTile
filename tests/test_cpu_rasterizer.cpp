#include "test_common.hpp"
#include "kernetile/cpu/cpu_rasterizer.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include <iostream>
#include <cassert>
#include <vector>

using namespace kernetile::cpu;
using namespace kernetile::core;
using namespace kernetile::tile;

void test_adaptive_concurrency_dispatcher() {
    std::cout << "[Test 1] Testing Adaptive Concurrency Dispatcher Thread Counts..." << std::endl;

    const uint32_t host_cores = 16;

    // 1. 极小局部重绘 (<= 8 瓦片，如光标闪烁): 必须返回 1 线程，避免线程上下文切换开销
    assert(CpuTileRasterizer::calculate_worker_threads(1, host_cores) == 1);
    assert(CpuTileRasterizer::calculate_worker_threads(4, host_cores) == 1);
    assert(CpuTileRasterizer::calculate_worker_threads(8, host_cores) == 1);

    // 2. 中小交互 (<= 64 瓦片，如按钮悬停/下拉菜单): 启动 2-4 线程
    uint32_t t_medium = CpuTileRasterizer::calculate_worker_threads(32, host_cores);
    assert(t_medium >= 2 && t_medium <= 4);

    // 3. 大面积/全屏重绘 (> 64 瓦片): 启动最大 8 线程
    uint32_t t_large = CpuTileRasterizer::calculate_worker_threads(500, host_cores);
    assert(t_large >= 4 && t_large <= 8);

    std::cout << "  -> Adaptive concurrency dispatcher PASSED." << std::endl;
}

void test_cpu_tile_rasterization() {
    std::cout << "[Test 2] Testing Pure CPU Tile Rasterization Output..." << std::endl;

    const uint32_t width = 64;
    const uint32_t height = 64;
    std::vector<uint8_t> framebuffer(width * height * 4, 0);

    // 构造一个翡翠绿圆角矩形图元 [16, 16, 48, 48], 圆角 8.0, 纯翡翠绿 RGBA=(0.0, 0.8, 0.4, 1.0)
    UiPrimitive prim{};
    prim.min_x = 16.0f;
    prim.min_y = 16.0f;
    prim.max_x = 48.0f;
    prim.max_y = 48.0f;
    prim.radius_top_left = 8.0f;
    prim.radius_top_right = 8.0f;
    prim.radius_bottom_right = 8.0f;
    prim.radius_bottom_left = 8.0f;
    prim.color_r = 0.0f;
    prim.color_g = 0.8f;
    prim.color_b = 0.4f;
    prim.color_a = 1.0f;

    std::vector<UiPrimitive> primitives = {prim};

    TileGrid grid(width, height);
    TileBinner binner;
    BinningResult bin_res = binner.bin_primitives(grid, primitives);

    // 提取脏瓦片
    std::vector<uint32_t> dirty_tiles;
    for (uint32_t i = 0; i < grid.get_total_tiles(); ++i) {
        if (bin_res.headers[i].primitive_count > 0) {
            dirty_tiles.push_back(i);
        }
    }
    assert(!dirty_tiles.empty());

    CpuTileRasterizer::RenderParams params{};
    params.width = width;
    params.height = height;
    params.primitives = primitives;
    params.tile_indices = bin_res.indices;
    params.tile_headers = bin_res.headers;
    params.dirty_tiles = dirty_tiles;
    params.thread_count = 1;

    CpuTileRasterizer::render(params, framebuffer.data());

    // 校验中心点像素 (32, 32)：必须为纯翡翠绿 (R=0, G=204, B=102, A=255)
    size_t center_idx = (32 * width + 32) * 4;
    std::cout << "  Center pixel RGBA: ("
              << static_cast<int>(framebuffer[center_idx + 0]) << ", "
              << static_cast<int>(framebuffer[center_idx + 1]) << ", "
              << static_cast<int>(framebuffer[center_idx + 2]) << ", "
              << static_cast<int>(framebuffer[center_idx + 3]) << ")" << std::endl;

    assert(framebuffer[center_idx + 0] == 0);
    assert(framebuffer[center_idx + 1] == 204);
    assert(framebuffer[center_idx + 2] == 102);
    assert(framebuffer[center_idx + 3] == 255);

    // 校验外部像素 (4, 4)：必须完全透明 (0, 0, 0, 0)
    size_t out_idx = (4 * width + 4) * 4;
    assert(framebuffer[out_idx + 3] == 0);

    std::cout << "  -> Pure CPU tile rasterization PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 9 CPU Rasterizer Unit Tests ===" << std::endl;

    test_adaptive_concurrency_dispatcher();
    test_cpu_tile_rasterization();

    std::cout << "=== All CPU Rasterizer Tests PASSED (100%) ===" << std::endl;
    return 0;
}
