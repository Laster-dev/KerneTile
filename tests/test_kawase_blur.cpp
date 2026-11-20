#include "test_common.hpp"
#include "kernetile/fx/dual_kawase_blur.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <chrono>

using namespace kernetile::fx;

void test_pass_planning() {
    std::cout << "[Test 1] Testing Dual Kawase Pass Planning & Pyramid VRAM..." << std::endl;

    // 4K 分辨率 (3840 x 2160)
    const uint32_t w_4k = 3840;
    const uint32_t h_4k = 2160;

    // 1. Low: 1 级降采样 + 1 级升采样 = 2 passes
    {
        BlurParams params{};
        params.quality = BlurQuality::Low;
        auto passes = DualKawaseBlur::build_pass_plan(w_4k, h_4k, params);
        assert(passes.size() == 2);

        // Pass 0: 3840x2160 -> 1920x1080 (Down)
        assert(passes[0].is_downsample == true);
        assert(passes[0].input_width == 3840 && passes[0].input_height == 2160);
        assert(passes[0].output_width == 1920 && passes[0].output_height == 1080);
        assert(passes[0].pyramid_level == 0);

        // Pass 1: 1920x1080 -> 3840x2160 (Up)
        assert(passes[1].is_downsample == false);
        assert(passes[1].input_width == 1920 && passes[1].input_height == 1080);
        assert(passes[1].output_width == 3840 && passes[1].output_height == 2160);
        assert(passes[1].pyramid_level == 0);

        size_t vram = DualKawaseBlur::calculate_pyramid_vram_bytes(w_4k, h_4k, params);
        // Level 1: 1920 * 1080 * 4 = 8,294,400 字节
        assert(vram == 1920 * 1080 * 4);
    }

    // 2. Medium: 2 级降采样 + 2 级升采样 = 4 passes
    {
        BlurParams params{};
        params.quality = BlurQuality::Medium;
        auto passes = DualKawaseBlur::build_pass_plan(w_4k, h_4k, params);
        assert(passes.size() == 4);

        // Pass 0: 3840x2160 -> 1920x1080 (Down)
        // Pass 1: 1920x1080 -> 960x540 (Down)
        // Pass 2: 960x540 -> 1920x1080 (Up)
        // Pass 3: 1920x1080 -> 3840x2160 (Up)
        assert(passes[0].output_width == 1920 && passes[0].output_height == 1080);
        assert(passes[1].output_width == 960 && passes[1].output_height == 540);
        assert(passes[2].output_width == 1920 && passes[2].output_height == 1080);
        assert(passes[3].output_width == 3840 && passes[3].output_height == 2160);

        size_t vram = DualKawaseBlur::calculate_pyramid_vram_bytes(w_4k, h_4k, params);
        size_t expected_vram = (1920 * 1080 + 960 * 540) * 4;
        assert(vram == expected_vram);
    }

    // 3. High: 3 级降采样 + 3 级升采样 = 6 passes
    {
        BlurParams params{};
        params.quality = BlurQuality::High;
        auto passes = DualKawaseBlur::build_pass_plan(w_4k, h_4k, params);
        assert(passes.size() == 6);

        // 验证中间金字塔最小分辨率降至 480x270
        assert(passes[2].output_width == 480 && passes[2].output_height == 270);
        assert(passes[3].input_width == 480 && passes[3].input_height == 270);
        assert(passes[5].output_width == 3840 && passes[5].output_height == 2160);

        size_t vram = DualKawaseBlur::calculate_pyramid_vram_bytes(w_4k, h_4k, params);
        size_t expected_vram = (1920 * 1080 + 960 * 540 + 480 * 270) * 4; // 10,886,400 字节
        assert(vram == expected_vram);
        double mb = static_cast<double>(vram) / (1024.0 * 1024.0);
        std::cout << "  4K High Blur Pyramid VRAM: " << mb << " MiB (Expected ~10.38 MiB)" << std::endl;
    }

    std::cout << "  -> Pass planning & VRAM calculation PASSED." << std::endl;
}

void test_energy_conservation() {
    std::cout << "[Test 2] Testing Dual Kawase Energy Conservation..." << std::endl;

    const uint32_t width = 128;
    const uint32_t height = 128;
    std::vector<uint8_t> src(width * height * 4);
    std::vector<uint8_t> dst(width * height * 4, 0);

    const uint8_t ref_r = 120;
    const uint8_t ref_g = 80;
    const uint8_t ref_b = 210;
    const uint8_t ref_a = 255;

    for (size_t i = 0; i < width * height; ++i) {
        src[i * 4 + 0] = ref_r;
        src[i * 4 + 1] = ref_g;
        src[i * 4 + 2] = ref_b;
        src[i * 4 + 3] = ref_a;
    }

    // 针对 Low, Medium, High 档位均验证中心区域能量漂移 <= 1 LSB
    BlurQuality qualities[] = {BlurQuality::Low, BlurQuality::Medium, BlurQuality::High};
    for (auto q : qualities) {
        BlurParams params{};
        params.quality = q;
        DualKawaseBlur::blur_cpu(src.data(), width, height, dst.data(), params);

        // 检查中心 64x64 核心区域
        int max_err_r = 0;
        int max_err_g = 0;
        int max_err_b = 0;
        int max_err_a = 0;

        for (uint32_t y = 32; y < 96; ++y) {
            for (uint32_t x = 32; x < 96; ++x) {
                size_t idx = (y * width + x) * 4;
                max_err_r = std::max(max_err_r, std::abs(static_cast<int>(dst[idx + 0]) - ref_r));
                max_err_g = std::max(max_err_g, std::abs(static_cast<int>(dst[idx + 1]) - ref_g));
                max_err_b = std::max(max_err_b, std::abs(static_cast<int>(dst[idx + 2]) - ref_b));
                max_err_a = std::max(max_err_a, std::abs(static_cast<int>(dst[idx + 3]) - ref_a));
            }
        }

        std::cout << "  Quality " << static_cast<int>(q) 
                  << " Center Max LSB Drift: R=" << max_err_r 
                  << " G=" << max_err_g << " B=" << max_err_b << " A=" << max_err_a << std::endl;

        // 能量守恒断言：整数归一化舍入误差绝对不超过 1 个 LSB！
        assert(max_err_r <= 1);
        assert(max_err_g <= 1);
        assert(max_err_b <= 1);
        assert(max_err_a <= 1);
    }

    std::cout << "  -> Energy conservation PASSED." << std::endl;
}

void test_boundary_clamping() {
    std::cout << "[Test 3] Testing Dual Kawase Boundary Clamping (No Dark Borders)..." << std::endl;

    const uint32_t width = 64;
    const uint32_t height = 64;
    std::vector<uint8_t> src(width * height * 4, 255); // 全白
    std::vector<uint8_t> dst(width * height * 4, 0);

    BlurParams params{};
    params.quality = BlurQuality::High; // 3 passes 降采样+升采样
    DualKawaseBlur::blur_cpu(src.data(), width, height, dst.data(), params);

    // 检查四个最极端角点和边界
    size_t corner_indices[] = {
        0,                                   // (0, 0)
        (width - 1) * 4,                     // (w-1, 0)
        ((height - 1) * width) * 4,          // (0, h-1)
        (height * width - 1) * 4             // (w-1, h-1)
    };

    for (size_t idx : corner_indices) {
        assert(dst[idx + 0] >= 254);
        assert(dst[idx + 1] >= 254);
        assert(dst[idx + 2] >= 254);
        assert(dst[idx + 3] >= 254);
    }

    // 检查边界所有像素均无发暗渗色
    for (uint32_t x = 0; x < width; ++x) {
        size_t top = x * 4;
        size_t bot = ((height - 1) * width + x) * 4;
        assert(dst[top + 0] >= 254 && dst[bot + 0] >= 254);
    }

    std::cout << "  -> Boundary Clamping (Zero Dark Border Bleeding) PASSED." << std::endl;
}

void test_smooth_step_blur() {
    std::cout << "[Test 4] Testing Smooth Step Blur Gradient..." << std::endl;

    const uint32_t width = 128;
    const uint32_t height = 64;
    std::vector<uint8_t> src(width * height * 4, 0);
    std::vector<uint8_t> dst(width * height * 4, 0);

    // 左半边全黑 (0), 右半边全白 (255)
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = (y * width + x) * 4;
            uint8_t val = (x >= 64) ? 255 : 0;
            src[idx + 0] = val;
            src[idx + 1] = val;
            src[idx + 2] = val;
            src[idx + 3] = 255;
        }
    }

    BlurParams params{};
    params.quality = BlurQuality::Medium;
    DualKawaseBlur::blur_cpu(src.data(), width, height, dst.data(), params);

    // 检查第 32 行（水平中轴线）的横向渐变平滑度
    uint32_t mid_y = 32;
    int prev_val = -1;
    for (uint32_t x = 16; x < 112; ++x) {
        size_t idx = (mid_y * width + x) * 4;
        int cur_val = dst[idx + 0];
        // 断言单调非递减性 (无负值震荡和伪影)
        assert(cur_val >= prev_val);
        prev_val = cur_val;
    }

    // 检查阶跃边界处的中心过渡值 (应当在 127 附近)
    size_t mid_idx = (mid_y * width + 64) * 4;
    int mid_val = dst[mid_idx + 0];
    std::cout << "  Step transition midpoint value: " << mid_val << " (Target ~127)" << std::endl;
    assert(mid_val >= 110 && mid_val <= 145);

    std::cout << "  -> Smooth step blur & monotonic transition PASSED." << std::endl;
}

void test_planning_perf() {
    std::cout << "[Test 5] Testing Pass Planning Throughput..." << std::endl;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        BlurParams p{};
        p.quality = BlurQuality::High;
        auto passes = DualKawaseBlur::build_pass_plan(3840, 2160, p);
        (void)passes;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double total_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double avg_us = total_us / 10000.0;
    std::cout << "  10,000 4K Pass Plan Generations: " << total_us << " us (Avg: " << avg_us << " us/plan)" << std::endl;
    assert(avg_us < 5.0); // 必须在 5 微秒内完成规划

    std::cout << "  -> Pass planning throughput PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 7 Dual Kawase Blur Unit Tests ===" << std::endl;

    test_pass_planning();
    test_energy_conservation();
    test_boundary_clamping();
    test_smooth_step_blur();
    test_planning_perf();

    std::cout << "=== All Dual Kawase Blur Tests PASSED (100%) ===" << std::endl;
    return 0;
}
