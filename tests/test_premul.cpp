#include "kernetile/math/color.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

inline bool approx_equal(const kernetile::math::ColorPremul& a, const kernetile::math::ColorPremul& b, float eps = 1e-5f) {
    return std::abs(a.r - b.r) <= eps &&
           std::abs(a.g - b.g) <= eps &&
           std::abs(a.b - b.b) <= eps &&
           std::abs(a.a - b.a) <= eps;
}

int main() {
    using namespace kernetile::math;

    std::cout << "[TEST] Running test_premul..." << std::endl;

    // 1. 单位元测试 (Transparent Identity)
    ColorPremul trans = ColorPremul::transparent();
    ColorPremul red = clamp_premul(0.8f, 0.0f, 0.0f, 0.8f);

    assert(approx_equal(porter_duff_over(trans, red), red));
    assert(approx_equal(porter_duff_over(red, trans), red));

    // 2. 完全不透明覆盖测试 (Opaque Occlusion)
    ColorPremul opaque_blue = clamp_premul(0.0f, 0.0f, 1.0f, 1.0f);
    ColorPremul composite_res = porter_duff_over(opaque_blue, red);
    assert(approx_equal(composite_res, opaque_blue));

    // 3. 预乘结合律测试: Over(Over(A, B), C) == Over(A, Over(B, C))
    // 设三个不同透明度的半透明层
    ColorPremul layer_a = clamp_premul(0.4f, 0.1f, 0.1f, 0.5f); // 50% 红
    ColorPremul layer_b = clamp_premul(0.1f, 0.3f, 0.1f, 0.4f); // 40% 绿
    ColorPremul layer_c = clamp_premul(0.1f, 0.1f, 0.5f, 0.6f); // 60% 蓝

    ColorPremul ab_then_c = porter_duff_over(porter_duff_over(layer_a, layer_b), layer_c);
    ColorPremul a_then_bc = porter_duff_over(layer_a, porter_duff_over(layer_b, layer_c));

    std::cout << "  -> Over(Over(A, B), C): (" 
              << ab_then_c.r << ", " << ab_then_c.g << ", " << ab_then_c.b << ", " << ab_then_c.a << ")" << std::endl;
    std::cout << "  -> Over(A, Over(B, C)): (" 
              << a_then_bc.r << ", " << a_then_bc.g << ", " << a_then_bc.b << ", " << a_then_bc.a << ")" << std::endl;

    assert(approx_equal(ab_then_c, a_then_bc));

    // 4. 非法超色域预乘色域自动钳制防御测试 (C > A)
    // 模拟输入非法预乘色 (如 R=0.9, 但 Alpha 仅为 0.5)
    ColorPremul invalid_over_gamut = clamp_premul(0.9f, 0.8f, 0.2f, 0.5f);
    assert(invalid_over_gamut.r <= invalid_over_gamut.a);
    assert(invalid_over_gamut.g <= invalid_over_gamut.a);
    assert(invalid_over_gamut.b <= invalid_over_gamut.a);
    assert(invalid_over_gamut.r == 0.5f);
    assert(invalid_over_gamut.g == 0.5f);
    assert(invalid_over_gamut.b == 0.2f);
    assert(invalid_over_gamut.a == 0.5f);

    std::cout << "[TEST] test_premul PASSED successfully!" << std::endl;
    return 0;
}
