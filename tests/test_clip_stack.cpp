#include "test_common.hpp"
#include "kernetile/fx/clip_stack.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <chrono>

using namespace kernetile::fx;
using namespace kernetile::core;
using namespace kernetile::math;

void test_single_aabb_clip() {
    std::cout << "[Test 1] Testing Single AABB Clip..." << std::endl;

    ClipStack stack;
    Rect rect{100.0f, 100.0f, 300.0f, 200.0f};
    uint32_t clip_id = stack.push_clip(rect, 0.0f);

    assert(clip_id == 0);
    assert(stack.depth() == 1);
    assert(stack.current_clip_index() == 0);

    // 内部点：可见 (false)
    assert(stack.is_pixel_culled(Vec2{200.0f, 150.0f}, clip_id) == false);
    assert(stack.is_pixel_culled(Vec2{101.0f, 101.0f}, clip_id) == false);
    assert(stack.is_pixel_culled(Vec2{299.0f, 199.0f}, clip_id) == false);

    // 外部点：剔除 (true)
    assert(stack.is_pixel_culled(Vec2{50.0f, 150.0f}, clip_id) == true);
    assert(stack.is_pixel_culled(Vec2{350.0f, 150.0f}, clip_id) == true);
    assert(stack.is_pixel_culled(Vec2{200.0f, 50.0f}, clip_id) == true);
    assert(stack.is_pixel_culled(Vec2{200.0f, 250.0f}, clip_id) == true);

    // 无裁剪索引判定
    assert(stack.is_pixel_culled(Vec2{9999.0f, 9999.0f}, INVALID_INDEX_U32) == false);

    std::cout << "  -> Single AABB clip PASSED." << std::endl;
}

void test_rounded_corner_clip() {
    std::cout << "[Test 2] Testing Rounded Corner SDF Clip..." << std::endl;

    ClipStack stack;
    // 100x100 矩形，圆角 20.0f
    Rect rect{0.0f, 0.0f, 100.0f, 100.0f};
    uint32_t clip_id = stack.push_clip(rect, 20.0f);

    // 中心点：可见
    assert(stack.is_pixel_culled(Vec2{50.0f, 50.0f}, clip_id) == false);

    // 直角边缘中点：可见
    assert(stack.is_pixel_culled(Vec2{50.0f, 1.0f}, clip_id) == false);
    assert(stack.is_pixel_culled(Vec2{1.0f, 50.0f}, clip_id) == false);

    // 角落点 (2, 2)：位于 AABB 内部，但在圆角外部，应当被剔除 (true)
    assert(stack.is_pixel_culled(Vec2{2.0f, 2.0f}, clip_id) == true);
    assert(stack.is_pixel_culled(Vec2{98.0f, 2.0f}, clip_id) == true);
    assert(stack.is_pixel_culled(Vec2{2.0f, 98.0f}, clip_id) == true);
    assert(stack.is_pixel_culled(Vec2{98.0f, 98.0f}, clip_id) == true);

    // 刚好在圆角弧线内部的点：可见 (false)
    assert(stack.is_pixel_culled(Vec2{20.0f, 20.0f}, clip_id) == false);

    std::cout << "  -> Rounded corner SDF clip PASSED." << std::endl;
}

void test_nested_hierarchy() {
    std::cout << "[Test 3] Testing Nested Hierarchy & Push/Pop..." << std::endl;

    ClipStack stack;

    // 1. 窗口 [0, 0, 1000, 1000]
    uint32_t win_id = stack.push_clip(Rect{0.0f, 0.0f, 1000.0f, 1000.0f}, 0.0f);
    assert(win_id == 0);
    assert(stack.get_clips()[win_id].parent_index == INVALID_INDEX_U32);

    // 2. 子面板 [200, 200, 800, 800]
    uint32_t panel_id = stack.push_clip(Rect{200.0f, 200.0f, 800.0f, 800.0f}, 10.0f);
    assert(panel_id == 1);
    assert(stack.get_clips()[panel_id].parent_index == win_id);

    // 3. 滚动容器 [300, 300, 600, 600]
    uint32_t scroll_id = stack.push_clip(Rect{300.0f, 300.0f, 600.0f, 600.0f}, 0.0f);
    assert(scroll_id == 2);
    assert(stack.get_clips()[scroll_id].parent_index == panel_id);
    assert(stack.depth() == 3);
    assert(stack.current_clip_index() == scroll_id);

    // 点 (500, 500): 三层均包含 -> 可见
    assert(stack.is_pixel_culled(Vec2{500.0f, 500.0f}, scroll_id) == false);

    // 点 (250, 250): 在面板内，但在滚动容器外 -> scroll_id 剔除
    assert(stack.is_pixel_culled(Vec2{250.0f, 250.0f}, scroll_id) == true);
    // 但在 panel_id 下 -> 可见
    assert(stack.is_pixel_culled(Vec2{250.0f, 250.0f}, panel_id) == false);

    // 点 (100, 100): 在窗口内，但在面板外 -> scroll_id 与 panel_id 均剔除
    assert(stack.is_pixel_culled(Vec2{100.0f, 100.0f}, scroll_id) == true);
    assert(stack.is_pixel_culled(Vec2{100.0f, 100.0f}, panel_id) == true);
    // 但在 win_id 下 -> 可见
    assert(stack.is_pixel_culled(Vec2{100.0f, 100.0f}, win_id) == false);

    // 弹出滚动容器
    stack.pop_clip();
    assert(stack.depth() == 2);
    assert(stack.current_clip_index() == panel_id);

    // 此时当前激活裁剪为 panel_id，点 (250, 250) 可见
    assert(stack.is_pixel_culled(Vec2{250.0f, 250.0f}, stack.current_clip_index()) == false);

    // 弹出面板
    stack.pop_clip();
    assert(stack.depth() == 1);
    assert(stack.current_clip_index() == win_id);

    // 弹出窗口
    stack.pop_clip();
    assert(stack.depth() == 0);
    assert(stack.current_clip_index() == INVALID_INDEX_U32);

    std::cout << "  -> Nested hierarchy & push/pop PASSED." << std::endl;
}

void test_deep_nesting_stress() {
    std::cout << "[Test 4] Testing 16-Level Deep Nesting Stress..." << std::endl;

    ClipStack stack;
    const uint32_t levels = 16;
    for (uint32_t i = 0; i < levels; ++i) {
        float inset = static_cast<float>(i * 10);
        stack.push_clip(Rect{inset, inset, 1000.0f - inset, 1000.0f - inset}, 5.0f);
    }
    assert(stack.depth() == 16);
    uint32_t deep_id = stack.current_clip_index();

    Vec2 center_pt{500.0f, 500.0f};
    Vec2 outside_pt{10.0f, 10.0f};

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; ++i) {
        bool culled1 = stack.is_pixel_culled(center_pt, deep_id);
        bool culled2 = stack.is_pixel_culled(outside_pt, deep_id);
        (void)culled1;
        (void)culled2;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "  200,000 16-level deep clip evaluations: " << total_ms << " ms" << std::endl;
    // 20 万次 16 层遍历必须迅速完成 (Debug 模式下 < 120 ms, 平均单次 < 0.6 us)
    assert(total_ms < 120.0);

    std::cout << "  -> 16-Level deep nesting stress PASSED." << std::endl;
}

void test_binary_layout() {
    std::cout << "[Test 5] Testing ClipRect Binary Layout & GPU Mirroring..." << std::endl;

    static_assert(sizeof(ClipRect) == 32, "ClipRect must be exactly 32 bytes");
    static_assert(alignof(ClipRect) == 16, "ClipRect alignment must be 16 bytes");

    ClipStack stack;
    stack.push_clip(Rect{10.0f, 20.0f, 100.0f, 200.0f}, 8.0f, 0x1234);

    const auto& clips = stack.get_clips();
    assert(clips.size() == 1);
    assert(clips[0].min_x == 10.0f);
    assert(clips[0].min_y == 20.0f);
    assert(clips[0].max_x == 100.0f);
    assert(clips[0].max_y == 200.0f);
    assert(clips[0].corner_radius == 8.0f);
    assert(clips[0].parent_index == INVALID_INDEX_U32);
    assert(clips[0].flags == 0x1234);
    assert(clips[0].reserved == 0);

    std::cout << "  -> Binary layout & GPU mirroring PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 7 Clip Stack Unit Tests ===" << std::endl;

    test_single_aabb_clip();
    test_rounded_corner_clip();
    test_nested_hierarchy();
    test_deep_nesting_stress();
    test_binary_layout();

    std::cout << "=== All Clip Stack Tests PASSED (100%) ===" << std::endl;
    return 0;
}
