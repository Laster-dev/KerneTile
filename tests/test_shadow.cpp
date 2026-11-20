#include "kernetile/math/shadow.hpp"
#include "test_common.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

int main() {
    configure_test_environment();
    using namespace kernetile::math;

    std::cout << "[TEST] Running test_shadow..." << std::endl;

    // 1. Abramowitz & Stegun 误差函数逼近精度检验 ([-4.0, 4.0] 区间 10,001 点采样)
    constexpr int NUM_POINTS = 10000;
    float max_erf_err = 0.0f;
    float worst_x = 0.0f;

    for (int i = 0; i <= NUM_POINTS; ++i) {
        float x = -4.0f + 8.0f * (static_cast<float>(i) / static_cast<float>(NUM_POINTS));
        float approx = erf_approx(x);
        float exact = std::erf(x);
        float err = std::abs(approx - exact);

        if (err > max_erf_err) {
            max_erf_err = err;
            worst_x = x;
        }
    }

    float max_lsb_err = max_erf_err * 255.0f;
    std::cout << "  -> Max erf approximation error: " << std::scientific << max_erf_err 
              << " at x = " << std::fixed << worst_x << std::endl;
    std::cout << "  -> Equivalent 8-bit color error: " << std::setprecision(4) << max_lsb_err << " LSB" << std::endl;

    // 严格断言论文指标: 逼近误差必须 <= 8.2e-4，折合 <= 0.21 LSB
    assert(max_erf_err <= 8.2e-4f);
    assert(max_lsb_err <= 0.21f);

    // 2. 高斯累积分布函数 CDF 性质检验
    assert(std::abs(gaussian_cdf(0.0f, 10.0f) - 0.5f) < 1e-5f);
    assert(gaussian_cdf(-100.0f, 10.0f) < 1e-6f);
    assert(gaussian_cdf(+100.0f, 10.0f) > 0.99999f);

    // 3. Evan Wallace 解析常数时间阴影求值测试
    Vec2 box_min{50.0f, 50.0f};
    Vec2 box_max{150.0f, 110.0f};
    float radius = 10.0f;
    float sigma = 8.0f;
    float spread = 0.0f;

    // 矩形深中心点 (100, 80)
    float s_center = evaluate_analytic_shadow(Vec2{100.0f, 80.0f}, box_min, box_max, radius, sigma, spread);
    std::cout << "  -> Shadow intensity at center: " << s_center << std::endl;
    assert(s_center > 0.99f);

    // 边缘处 (150, 80)
    float s_edge = evaluate_analytic_shadow(Vec2{150.0f, 80.0f}, box_min, box_max, radius, sigma, spread);
    std::cout << "  -> Shadow intensity at boundary: " << s_edge << std::endl;
    assert(s_edge > 0.45f && s_edge < 0.55f);

    // 远离矩形处 (300, 80)
    float s_far = evaluate_analytic_shadow(Vec2{300.0f, 80.0f}, box_min, box_max, radius, sigma, spread);
    std::cout << "  -> Shadow intensity far away: " << s_far << std::endl;
    assert(s_far < 1e-4f);

    // 对称性检验: (100 + dx, 80) 与 (100 - dx, 80)
    float s_plus_dx = evaluate_analytic_shadow(Vec2{160.0f, 80.0f}, box_min, box_max, radius, sigma, spread);
    float s_minus_dx = evaluate_analytic_shadow(Vec2{40.0f, 80.0f}, box_min, box_max, radius, sigma, spread);
    assert(std::abs(s_plus_dx - s_minus_dx) < 1e-5f);

    std::cout << "[TEST] test_shadow PASSED successfully!" << std::endl;
    return 0;
}
