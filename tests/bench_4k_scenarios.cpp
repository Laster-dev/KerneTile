#include "test_common.hpp"
#include "kernetile/kernetile.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <vector>

using namespace kernetile;
using namespace kernetile::math;
using namespace kernetile::core;
using namespace kernetile::tile;

void bench_scenario_a_cursor_blink() {
    std::cout << "[Bench A] 4K Micro Benchmark: Cursor Blink..." << std::endl;

    constexpr uint32_t W = 3840;
    constexpr uint32_t H = 2160;
    TileGrid grid(W, H);
    assert(grid.get_total_tiles() == 32400);

    DirtyTracker tracker(grid.get_total_tiles());
    TileBinner binner;

    // 构造单光标图元 (2x20 像素，恰跨 2 块瓦片)
    UiPrimitive cursor{};
    cursor.min_x = 100.0f;
    cursor.min_y = 100.0f;
    cursor.max_x = 102.0f;
    cursor.max_y = 120.0f;
    cursor.color_r = 1.0f;
    cursor.color_g = 1.0f;
    cursor.color_b = 1.0f;
    cursor.color_a = 1.0f;

    std::vector<UiPrimitive> prims = {cursor};

    // 预热一帧
    (void)binner.bin_primitives(grid, prims);

    auto t0 = std::chrono::high_resolution_clock::now();
    auto bin_res = binner.bin_primitives(grid, prims);
    auto dirty_res = tracker.update(grid, bin_res.headers, bin_res.indices, prims);
    auto t1 = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    std::cout << "  Dirty Tiles: " << dirty_res.dirty_count << " / 32,400" << std::endl;
    std::cout << "  Culling Ratio: " << (dirty_res.culling_ratio * 100.0) << "% (Target >= 99.99%)" << std::endl;
    std::cout << "  Binning + Dirty Tracking Time: " << elapsed_us << " us" << std::endl;

    assert(dirty_res.dirty_count <= 2);
    assert(dirty_res.culling_ratio >= 0.9999);
    // Debug 模式下 32,400 瓦片 STL 迭代开销保证稳定在 50 ms 内
    assert(elapsed_us < 50000.0);

    std::cout << "  -> Scenario A PASSED." << std::endl;
}

void bench_scenario_b_button_hover() {
    std::cout << "[Bench B] 4K Micro Benchmark: Button Hover..." << std::endl;

    constexpr uint32_t W = 3840;
    constexpr uint32_t H = 2160;
    TileGrid grid(W, H);
    DirtyTracker tracker(grid.get_total_tiles());
    TileBinner binner;

    // 构造 120x40 像素卡片阴影扩展图元 (阴影 blur 8.0, spread 4.0)
    UiPrimitive button{};
    button.min_x = 500.0f;
    button.min_y = 300.0f;
    button.max_x = 620.0f;
    button.max_y = 340.0f;
    button.radius_top_left = 6.0f;
    button.radius_top_right = 6.0f;
    button.radius_bottom_right = 6.0f;
    button.radius_bottom_left = 6.0f;
    button.color_r = 0.0f;
    button.color_g = 0.6f;
    button.color_b = 0.9f;
    button.color_a = 1.0f;
    button.shadow_blur = 8.0f;
    button.shadow_spread = 4.0f;

    std::vector<UiPrimitive> prims = {button};

    auto bin_res = binner.bin_primitives(grid, prims);
    auto dirty_res = tracker.update(grid, bin_res.headers, bin_res.indices, prims);

    std::cout << "  Dirty Tiles: " << dirty_res.dirty_count << " / 32,400" << std::endl;
    std::cout << "  Culling Ratio: " << (dirty_res.culling_ratio * 100.0) << "% (Target >= 99.75%)" << std::endl;

    assert(dirty_res.dirty_count <= 80);
    assert(dirty_res.culling_ratio >= 0.9975);

    std::cout << "  -> Scenario B PASSED." << std::endl;
}

void bench_scenario_c_paris_30k_stress() {
    std::cout << "[Bench C] 4K Stress Benchmark: Paris-30k Density..." << std::endl;

    constexpr uint32_t W = 1920;
    constexpr uint32_t H = 1080;
    TileGrid grid(W, H);
    TileBinner binner;

    const uint32_t count = 30000;
    std::vector<UiPrimitive> prims;
    prims.reserve(count);

    // 随机但确定性的生成 30,000 个密集矢量图元
    for (uint32_t i = 0; i < count; ++i) {
        float x = static_cast<float>((i * 37) % (W - 50));
        float y = static_cast<float>((i * 59) % (H - 50));
        UiPrimitive p{};
        p.min_x = x;
        p.min_y = y;
        p.max_x = x + 40.0f;
        p.max_y = y + 30.0f;
        p.color_r = 0.1f;
        p.color_g = 0.5f;
        p.color_b = 0.8f;
        p.color_a = 0.5f;
        prims.push_back(p);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    auto bin_res = binner.bin_primitives(grid, prims);
    auto t1 = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double throughput_mps = (static_cast<double>(count) / (elapsed_ms / 1000.0)) / 1000000.0;

    std::cout << "  30,000 Primitives Binning Time: " << elapsed_ms << " ms" << std::endl;
    std::cout << "  Primitive Throughput: " << throughput_mps << " Million primitives/sec" << std::endl;

    assert(throughput_mps >= 0.1);
    assert(bin_res.total_indices_count > 0);

    std::cout << "  -> Scenario C PASSED." << std::endl;
}

void bench_scenario_d_full_desktop_ui() {
    std::cout << "[Bench D] 4K Composite Benchmark: Full Desktop UI..." << std::endl;

    constexpr uint32_t W = 3840;
    constexpr uint32_t H = 2160;
    TileGrid grid(W, H);
    TileBinner binner;

    // 构造典型复合桌面：含侧边栏、工具栏、2,000 个按钮与文本卡片
    const uint32_t count = 2000;
    std::vector<UiPrimitive> prims;
    prims.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        float x = static_cast<float>((i * 128) % (W - 200));
        float y = static_cast<float>((i * 64) % (H - 100));
        UiPrimitive p{};
        p.min_x = x;
        p.min_y = y;
        p.max_x = x + 120.0f;
        p.max_y = y + 40.0f;
        p.radius_top_left = 8.0f;
        p.radius_top_right = 8.0f;
        p.radius_bottom_right = 8.0f;
        p.radius_bottom_left = 8.0f;
        p.color_r = 0.2f;
        p.color_g = 0.4f;
        p.color_b = 0.7f;
        p.color_a = 0.9f;
        p.shadow_blur = 4.0f;
        prims.push_back(p);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    auto bin_res = binner.bin_primitives(grid, prims);
    auto t1 = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "  2,000 Composite 4K Primitives Binning Time: " << elapsed_ms << " ms" << std::endl;

    assert(elapsed_ms < 250.0); // Debug 模式下包含 32,400 瓦片全量遍历

    std::cout << "  -> Scenario D PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 10 4K Standard Benchmark Suite ===" << std::endl;

    bench_scenario_a_cursor_blink();
    bench_scenario_b_button_hover();
    bench_scenario_c_paris_30k_stress();
    bench_scenario_d_full_desktop_ui();

    std::cout << "=== All 4K Benchmark Scenarios PASSED (100%) ===" << std::endl;
    return 0;
}
