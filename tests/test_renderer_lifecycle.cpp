#include "test_common.hpp"
#include "kernetile/kernetile.hpp"
#include <iostream>
#include <cassert>
#include <vector>

using namespace kernetile;
using namespace kernetile::math;

void test_renderer_api_lifecycle() {
    std::cout << "[Test 1] Testing KerneTileRenderer Lifecycle & Draw APIs..." << std::endl;

    RendererConfig config{};
    config.width = 800;
    config.height = 600;
    config.force_cpu_fallback = true; // 确保在 CI 和无显卡环境下均可 100% 运行

    auto renderer = KerneTileRenderer::create(config);
    assert(renderer != nullptr);

    // 1. 开始第一帧
    renderer->begin_frame();

    // 绘制背景矩形
    renderer->draw_rect(Rect{0.0f, 0.0f, 800.0f, 600.0f}, Color(0.1f, 0.15f, 0.2f, 1.0f));

    // 绘制带软阴影的圆角卡片
    renderer->draw_shadowed_box(
        Rect{100.0f, 100.0f, 400.0f, 300.0f},
        Color(0.2f, 0.6f, 0.9f, 1.0f),
        16.0f, // 圆角
        8.0f,  // 阴影模糊
        2.0f,  // 阴影外扩
        Vec2{0.0f, 4.0f} // 阴影偏移
    );

    // 绘制文本
    renderer->draw_text("KerneTile 2D Engine", 120.0f, 140.0f, 24.0f, Color::white());

    // 结束第一帧
    renderer->end_frame();

    const auto& stats1 = renderer->get_last_frame_stats();
    std::cout << "  Frame 1: Total Prims=" << stats1.total_primitives 
              << ", Dirty Tiles=" << stats1.dirty_tiles 
              << ", Binning Time=" << stats1.cpu_binning_time_us << " us" << std::endl;

    assert(stats1.total_primitives >= 3);
    assert(stats1.dirty_tiles > 0);

    // 2. 离屏渲染输出至内存
    std::vector<uint8_t> buffer(800 * 600 * 4, 0);
    renderer->render_to_rgba8(buffer.data());

    // 校验中心区域像素非空
    size_t center_idx = (200 * 800 + 250) * 4;
    assert(buffer[center_idx + 3] == 255); // Alpha 完全不透明

    // 3. 测试窗口动态尺寸调整 (Resize)
    renderer->resize(1920, 1080);
    renderer->begin_frame();
    renderer->draw_rect(Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, Color::black());
    renderer->end_frame();

    const auto& stats2 = renderer->get_last_frame_stats();
    assert(stats2.total_tiles == (1920 / 16) * (1080 / 16 + 1)); // 120 * 68 = 8,160

    std::cout << "  -> KerneTileRenderer Lifecycle PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 10 Renderer Lifecycle Tests ===" << std::endl;

    test_renderer_api_lifecycle();

    std::cout << "=== All Renderer Lifecycle Tests PASSED (100%) ===" << std::endl;
    return 0;
}
