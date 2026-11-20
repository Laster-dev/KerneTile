#include "kernetile/math/color_space.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

int main() {
    using namespace kernetile::math;

    std::cout << "[TEST] Running test_color..." << std::endl;

    // 1. 验证线性阈值 C_lin = 0.0031308 处的两端连续性
    float lin_threshold = 0.0031308f;
    float linear_segment = 12.92f * lin_threshold;
    float power_segment = 1.055f * std::pow(lin_threshold, 1.0f / 2.4f) - 0.055f;
    float diff_threshold = std::abs(linear_segment - power_segment);

    std::cout << "  -> Linear segment at threshold: " << std::setprecision(9) << linear_segment << std::endl;
    std::cout << "  -> Power segment at threshold:  " << std::setprecision(9) << power_segment << std::endl;
    std::cout << "  -> Absolute deviation at boundary: " << diff_threshold << std::endl;
    assert(diff_threshold < 1e-7f);

    // 2. 验证非线性阈值 C_srgb = 0.04045 处的两端连续性
    float enc_threshold = 0.04045f;
    float srgb_linear_segment = enc_threshold / 12.92f;
    float srgb_power_segment = std::pow((enc_threshold + 0.055f) / 1.055f, 2.4f);
    float diff_enc_threshold = std::abs(srgb_linear_segment - srgb_power_segment);

    std::cout << "  -> sRGB linear segment: " << srgb_linear_segment << std::endl;
    std::cout << "  -> sRGB power segment:  " << srgb_power_segment << std::endl;
    std::cout << "  -> sRGB boundary deviation: " << diff_enc_threshold << std::endl;
    assert(diff_enc_threshold < 1e-7f);

    // 3. 边界值与极端值测试
    assert(srgb_to_linear(0.0f) == 0.0f);
    assert(linear_to_srgb(0.0f) == 0.0f);
    assert(std::abs(srgb_to_linear(1.0f) - 1.0f) < 1e-6f);
    assert(std::abs(linear_to_srgb(1.0f) - 1.0f) < 1e-6f);

    // 4. 100,000 点均匀采样往返往复精度与单调递增性验证
    constexpr int NUM_SAMPLES = 100000;
    float max_roundtrip_err = 0.0f;
    float prev_linear = -1.0f;

    for (int i = 0; i <= NUM_SAMPLES; ++i) {
        float srgb = static_cast<float>(i) / static_cast<float>(NUM_SAMPLES);
        float lin = srgb_to_linear(srgb);

        // 验证单调递增性
        assert(lin >= prev_linear);
        prev_linear = lin;

        // 验证往返精度
        float recovered_srgb = linear_to_srgb(lin);
        float err = std::abs(recovered_srgb - srgb);
        if (err > max_roundtrip_err) {
            max_roundtrip_err = err;
        }
    }

    std::cout << "  -> 100,000 samples max roundtrip error: " << max_roundtrip_err << std::endl;
    assert(max_roundtrip_err < 5e-4f); // 单精度浮点下的往返误差在正常量化容差内

    std::cout << "[TEST] test_color PASSED successfully!" << std::endl;
    return 0;
}
