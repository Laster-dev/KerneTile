#include "test_common.hpp"
#include "kernetile/platform/presentation_sync.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace kernetile::platform;

void test_modal_size_move_sync() {
    std::cout << "[Test 1] Testing Win32 WM_ENTERSIZEMOVE Modal Loop Refresh..." << std::endl;

    PresentationSync sync;
    uint32_t rendered_frames = 0;
    uint32_t last_w = 0;
    uint32_t last_h = 0;

    sync.set_render_callback([&](uint32_t w, uint32_t h) {
        rendered_frames++;
        last_w = w;
        last_h = h;
    });

    // 1. 模拟开始模态拖拽 (WM_ENTERSIZEMOVE)
    sync.on_enter_size_move();
    assert(sync.is_in_modal_size_move() == true);

    // 2. 模拟在模态拉伸中连续发送 60 次 WM_SIZING 调整大小
    for (uint32_t i = 1; i <= 60; ++i) {
        sync.on_sizing(800 + i * 10, 600 + i * 5);
    }

    assert(rendered_frames == 60);
    assert(last_w == 800 + 60 * 10);
    assert(last_h == 600 + 60 * 5);
    assert(sync.get_modal_frame_count() == 60);

    // 3. 模拟 WM_PAINT 补帧
    sync.on_paint(last_w, last_h);
    assert(rendered_frames == 61);
    assert(sync.get_modal_frame_count() == 61);

    // 4. 模拟释放鼠标结束模态拉伸 (WM_EXITSIZEMOVE)
    sync.on_exit_size_move();
    assert(sync.is_in_modal_size_move() == false);
    std::cout << "  Modal resize frames rendered: " << rendered_frames 
              << ", Elapsed time: " << sync.get_total_modal_time_ms() << " ms" << std::endl;

    std::cout << "  -> Modal size/move presentation sync PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 8 Platform Presentation Sync Unit Tests ===" << std::endl;

    test_modal_size_move_sync();

    std::cout << "=== All Platform Presentation Sync Tests PASSED (100%) ===" << std::endl;
    return 0;
}
