#include "kernetile/tile/tile_grid.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include "kernetile/tile/dirty_tracker.hpp"
#include "test_common.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_dirty_tracking..." << std::endl;

    using namespace kernetile::tile;
    using namespace kernetile::core;

    TileGrid grid_4k(3840, 2160); // 32,400 瓦片
    TileBinner binner;
    DirtyTracker tracker;

    // --- 帧 0: 初始空白状态 ---
    std::vector<UiPrimitive> prims_frame0;
    auto bin0 = binner.bin_primitives(grid_4k, prims_frame0);
    auto dirty0 = tracker.update(grid_4k, bin0.headers, bin0.indices, prims_frame0);
    std::cout << "  -> Frame 0 (initial clean): dirty=" << dirty0.dirty_count << std::endl;
    assert(dirty0.dirty_count == 0);

    // --- 帧 1 (场景 A: 光标闪烁): 在 (100, 100) 处插入 2x20 像素光标 ---
    std::vector<UiPrimitive> prims_cursor;
    UiPrimitive cursor{};
    cursor.min_x = 100.0f; cursor.min_y = 100.0f;
    cursor.max_x = 102.0f; cursor.max_y = 120.0f; // 2x20 像素，y 从 100 跨到 120 (tile 6 到 7)
    cursor.color_a = 1.0f;
    prims_cursor.push_back(cursor);

    auto bin_cursor = binner.bin_primitives(grid_4k, prims_cursor);
    auto dirty_cursor = tracker.update(grid_4k, bin_cursor.headers, bin_cursor.indices, prims_cursor);

    std::cout << "  -> Scenario A (Cursor Blink): dirty_tiles=" << dirty_cursor.dirty_count 
              << ", culling_ratio=" << std::setprecision(5) << (dirty_cursor.culling_ratio * 100.0) << "%" << std::endl;

    // 严格检验论文表 2 指标: 瓦片数 <= 2, 剔除率 >= 99.99% (理论值 99.994%)
    assert(dirty_cursor.dirty_count <= 2);
    assert(dirty_cursor.culling_ratio >= 0.9999);

    // --- 帧 2 (场景 B: 按钮悬停交互): 在 (200, 200) 处出现 120x40 像素悬停卡片 ---
    std::vector<UiPrimitive> prims_button = prims_cursor;
    UiPrimitive btn{};
    btn.min_x = 200.0f; btn.min_y = 200.0f;
    btn.max_x = 320.0f; btn.max_y = 240.0f; // 120x40 像素: 9x4 瓦片
    btn.color_a = 1.0f;
    prims_button.push_back(btn);

    auto bin_button = binner.bin_primitives(grid_4k, prims_button);
    auto dirty_button = tracker.update(grid_4k, bin_button.headers, bin_button.indices, prims_button);

    std::cout << "  -> Scenario B (Button Hover): dirty_tiles=" << dirty_button.dirty_count 
              << ", culling_ratio=" << std::setprecision(5) << (dirty_button.culling_ratio * 100.0) << "%" << std::endl;

    // 严格检验论文表 2 指标: 瓦片数 <= 36, 剔除率 >= 99.85% (理论值 99.89%)
    assert(dirty_button.dirty_count <= 36);
    assert(dirty_button.culling_ratio >= 0.9985);

    // --- 帧 3 (静态帧): 无状态变动 ---
    auto bin_static = binner.bin_primitives(grid_4k, prims_button);
    auto dirty_static = tracker.update(grid_4k, bin_static.headers, bin_static.indices, prims_button);
    std::cout << "  -> Scenario C (Static Frame): dirty_tiles=" << dirty_static.dirty_count << std::endl;
    assert(dirty_static.dirty_count == 0);
    assert(dirty_static.culling_ratio == 1.0);

    // Appearance-only transitions must still produce a final frame.
    auto rounded = prims_button;
    rounded.back().radius_top_left = 4.0f;
    auto bin_rounded = binner.bin_primitives(grid_4k, rounded);
    const auto d_r1 = tracker.update(grid_4k, bin_rounded.headers, bin_rounded.indices, rounded).dirty_count;
    const auto d_r2 = tracker.update(grid_4k, bin_rounded.headers, bin_rounded.indices, rounded).dirty_count;
    if (d_r1 == 0 || d_r2 != 0) {
        std::cerr << "FAIL: rounded transition expected >0 then 0, got " << d_r1 << ", " << d_r2 << std::endl;
        return 1;
    }

    // Scenario D (Index Shift Immunity): Inserting a new primitive at index 0 must ONLY dirty its own tiles,
    // not invalidate unrelated downstream tiles whose array indices shifted.
    std::vector<UiPrimitive> shifted;
    UiPrimitive badge{};
    badge.min_x = 0.0f; badge.min_y = 0.0f;
    badge.max_x = 12.0f; badge.max_y = 12.0f; // 仅占 tile (0,0)
    badge.color_a = 1.0f;
    shifted.push_back(badge);
    shifted.insert(shifted.end(), rounded.begin(), rounded.end());
    auto bin_shifted = binner.bin_primitives(grid_4k, shifted);
    auto dirty_shifted = tracker.update(grid_4k, bin_shifted.headers, bin_shifted.indices, shifted);
    std::cout << "  -> Scenario D (Front Insert / Index Shift): dirty_tiles=" << dirty_shifted.dirty_count << std::endl;
    if (dirty_shifted.dirty_count != 1) {
        std::cerr << "FAIL: expected 1 dirty tile on front insert, got " << dirty_shifted.dirty_count << std::endl;
        return 1;
    }

    std::cout << "[TEST] test_dirty_tracking PASSED successfully!" << std::endl;
    return 0;
}
