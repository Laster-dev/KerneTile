#include "kernetile/text/stem_darkening.hpp"
#include "test_common.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cassert>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_stem_darkening..." << std::endl;

    using namespace kernetile::text;

    // 1. 验证字号反比等效 Gamma 模型
    float gamma_11 = StemDarkener::compute_gamma(11.0f);
    float gamma_16 = StemDarkener::compute_gamma(16.0f);
    float gamma_24 = StemDarkener::compute_gamma(24.0f);
    float gamma_48 = StemDarkener::compute_gamma(48.0f);

    std::cout << "  -> Gamma at 11px: " << std::fixed << std::setprecision(3) << gamma_11 << " (Target: ~1.50)" << std::endl;
    std::cout << "  -> Gamma at 16px: " << gamma_16 << " (Target: ~1.30)" << std::endl;
    std::cout << "  -> Gamma at 24px: " << gamma_24 << " (Target: 1.00)" << std::endl;
    std::cout << "  -> Gamma at 48px: " << gamma_48 << " (Target: 1.00)" << std::endl;

    assert(gamma_11 >= 1.40f && gamma_11 <= 1.60f);
    assert(gamma_16 >= 1.20f && gamma_16 <= 1.40f);
    assert(gamma_24 == 1.0f && "Font sizes >= 24px must not apply stem darkening");
    assert(gamma_48 == 1.0f);

    // 2. 验证 12px 小字号下的半透明边缘亚像素覆盖率提升
    auto lut_12 = StemDarkener::build_lut(12.0f);

    // 纯白 (255) 与纯黑 (0) 必须绝对保真，不能发生色阶漂移
    assert(lut_12[0] == 0 && "Zero alpha must remain zero");
    assert(lut_12[255] == 255 && "Full alpha must remain 255");

    // 边缘亚像素采样点 (原值 51/255 即 20% 覆盖率)
    uint8_t orig_val = 51;
    uint8_t dark_val = lut_12[orig_val];
    double boost_ratio = (static_cast<double>(dark_val) - orig_val) / orig_val;

    std::cout << "  -> Edge coverage at 12px (orig=" << (int)orig_val << "/255): darkened=" 
              << (int)dark_val << "/255 (Boost: +" << std::setprecision(1) << (boost_ratio * 100.0) 
              << "%, Target: >= 25.0%)" << std::endl;

    assert(boost_ratio >= 0.25 && "Edge coverage boost must be at least +25%");

    // 3. 验证真实字形点阵应用
    std::vector<uint8_t> sample_bitmap = { 0, 30, 80, 150, 220, 255 };
    std::vector<uint8_t> orig_copy = sample_bitmap;
    StemDarkener::apply(sample_bitmap, 12.0f);

    assert(sample_bitmap[0] == 0);
    assert(sample_bitmap[5] == 255);
    for (size_t i = 1; i < 5; ++i) {
        assert(sample_bitmap[i] > orig_copy[i] && "Intermediate coverage must be boosted");
    }

    std::cout << "[TEST] test_stem_darkening PASSED successfully!" << std::endl;
    return 0;
}
