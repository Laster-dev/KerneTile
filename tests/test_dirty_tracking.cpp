#include "kernetile/tile/tile_grid.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include "kernetile/tile/dirty_tracker.hpp"
#include "test_common.hpp"
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <cassert>

#define KT_CHECK(cond)                                                                         \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::cerr << "FAIL at " << __FILE__ << ":" << __LINE__ << ": " #cond << std::endl; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (false)

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

    // Scenario E (R3 PointerReveal Cursor Sweep): Moving cursor across a wide card must only dirty
    // 16x16 tiles overlapping (cursor_old +- radius) U (cursor_new +- radius)
    {
        ClipRect card_clip{};
        card_clip.min_x = 100.0f; card_clip.min_y = 100.0f;
        card_clip.max_x = 900.0f; card_clip.max_y = 220.0f;
        card_clip.corner_radius = 8.0f;
        card_clip.parent_index = INVALID_INDEX_U32;
        card_clip.flags = kClipFlagRevealBounds;
        std::vector<ClipRect> clips = {card_clip};

        UiPrimitive card{};
        card.min_x = 100.0f; card.min_y = 100.0f;
        card.max_x = 900.0f; card.max_y = 220.0f;
        card.radius_top_left = card.radius_top_right = card.radius_bottom_right = card.radius_bottom_left = 8.0f;
        card.color_r = 0.2f; card.color_g = 0.2f; card.color_b = 0.2f; card.color_a = 1.0f;

        auto make_rev = [](float cx, float cy, float r) {
            UiPrimitive rev{};
            rev.min_x = std::max(100.0f, cx - r);
            rev.min_y = std::max(100.0f, cy - r);
            rev.max_x = std::min(900.0f, cx + r);
            rev.max_y = std::min(220.0f, cy + r);
            rev.color_r = 1.0f; rev.color_g = 1.0f; rev.color_b = 1.0f; rev.color_a = 0.5f;
            rev.shadow_offset_x = cx;
            rev.shadow_offset_y = cy;
            rev.shadow_blur = r;
            rev.shadow_spread = 1.0f;
            rev.flags = static_cast<uint16_t>(PrimitiveFlags::IsPointerReveal);
            rev.clip_index = 0;
            return rev;
        };

        std::vector<UiPrimitive> frame_a = {card, make_rev(200.0f, 160.0f, 48.0f)};
        auto bin_a = binner.bin_primitives(grid_4k, frame_a, true, clips);
        (void)tracker.update(grid_4k, bin_a.headers, bin_a.indices, frame_a, clips);

        std::vector<UiPrimitive> frame_b = {card, make_rev(220.0f, 160.0f, 48.0f)};
        auto bin_b = binner.bin_primitives(grid_4k, frame_b, true, clips);
        auto dirty_b = tracker.update(grid_4k, bin_b.headers, bin_b.indices, frame_b, clips);

        std::cout << "  -> Scenario E (PointerReveal Cursor Sweep): dirty_tiles=" << dirty_b.dirty_count
                  << ", culling_ratio=" << std::setprecision(5) << (dirty_b.culling_ratio * 100.0) << "%" << std::endl;

        KT_CHECK(dirty_b.dirty_count > 0);
        KT_CHECK(dirty_b.culling_ratio >= 0.998);
        // Combined highlight union X=[152..268] -> tiles [9..16], Y=[112..208] -> tiles [7..13]
        for (const auto& dt : dirty_b.dirty_tiles) {
            KT_CHECK(dt.tx >= 9 && dt.tx <= 16);
            KT_CHECK(dt.ty >= 7 && dt.ty <= 13);
        }
    }

    std::cout << "[TEST] test_dirty_tracking PASSED successfully!" << std::endl;
    return 0;
}
