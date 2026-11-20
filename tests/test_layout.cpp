#include "kernetile/core/primitives.hpp"
#include "kernetile/core/tile_header.hpp"
#include <iostream>
#include <cassert>
#include <cstddef>

int main() {
    using namespace kernetile::core;

    std::cout << "[TEST] Running test_layout..." << std::endl;

    // 1. 结构体大小与对齐断言
    static_assert(sizeof(UiPrimitive) == 80, "UiPrimitive size mismatch!");
    static_assert(alignof(UiPrimitive) == 16, "UiPrimitive align mismatch!");
    static_assert(sizeof(ClipRect) == 32, "ClipRect size mismatch!");
    static_assert(alignof(ClipRect) == 16, "ClipRect align mismatch!");
    static_assert(sizeof(TileHeader) == 8, "TileHeader size mismatch!");
    static_assert(alignof(TileHeader) == 4, "TileHeader align mismatch!");

    // 2. UiPrimitive 字段偏移量严格检验
    assert(offsetof(UiPrimitive, min_x) == 0);
    assert(offsetof(UiPrimitive, min_y) == 4);
    assert(offsetof(UiPrimitive, max_x) == 8);
    assert(offsetof(UiPrimitive, max_y) == 12);

    assert(offsetof(UiPrimitive, radius_top_left) == 16);
    assert(offsetof(UiPrimitive, radius_top_right) == 20);
    assert(offsetof(UiPrimitive, radius_bottom_right) == 24);
    assert(offsetof(UiPrimitive, radius_bottom_left) == 28);

    assert(offsetof(UiPrimitive, color_r) == 32);
    assert(offsetof(UiPrimitive, color_g) == 36);
    assert(offsetof(UiPrimitive, color_b) == 40);
    assert(offsetof(UiPrimitive, color_a) == 44);

    assert(offsetof(UiPrimitive, border_width) == 48);
    assert(offsetof(UiPrimitive, shadow_blur) == 52);
    assert(offsetof(UiPrimitive, shadow_spread) == 56);
    assert(offsetof(UiPrimitive, shadow_offset_x) == 60);

    assert(offsetof(UiPrimitive, atlas_u0) == 64);
    assert(offsetof(UiPrimitive, atlas_v0) == 66);
    assert(offsetof(UiPrimitive, atlas_u1) == 68);
    assert(offsetof(UiPrimitive, atlas_v1) == 70);
    assert(offsetof(UiPrimitive, flags) == 72);
    assert(offsetof(UiPrimitive, clip_index) == 74);
    assert(offsetof(UiPrimitive, shadow_offset_y) == 76);

    // 3. ClipRect 字段偏移量严格检验
    assert(offsetof(ClipRect, min_x) == 0);
    assert(offsetof(ClipRect, min_y) == 4);
    assert(offsetof(ClipRect, max_x) == 8);
    assert(offsetof(ClipRect, max_y) == 12);
    assert(offsetof(ClipRect, corner_radius) == 16);
    assert(offsetof(ClipRect, parent_index) == 20);
    assert(offsetof(ClipRect, flags) == 24);
    assert(offsetof(ClipRect, reserved) == 28);

    // 4. TileHeader 字段偏移量严格检验
    assert(offsetof(TileHeader, primitive_offset) == 0);
    assert(offsetof(TileHeader, primitive_count) == 4);
    assert(offsetof(TileHeader, flags) == 6);

    std::cout << "  -> UiPrimitive (80B, align 16): PASS" << std::endl;
    std::cout << "  -> ClipRect (32B, align 16): PASS" << std::endl;
    std::cout << "  -> TileHeader (8B, align 4): PASS" << std::endl;
    std::cout << "[TEST] test_layout PASSED successfully!" << std::endl;
    return 0;
}
