#include "kernetile/math/sdf.hpp"
#include "test_common.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    configure_test_environment();
    using namespace kernetile::math;

    std::cout << "[TEST] Running test_sdf..." << std::endl;

    Vec2 center{100.0f, 100.0f};
    Vec2 half_size{50.0f, 30.0f}; // 宽 100, 高 60
    CornerRadii radii{10.0f, 15.0f, 20.0f, 5.0f}; // 左上 10, 右上 15, 右下 20, 左下 5

    // 1. 中心点测试: 深度内部，距离必须为负
    float d_center = sd_rounded_box(center, center, half_size, radii);
    std::cout << "  -> Center distance: " << d_center << std::endl;
    assert(d_center < 0.0f);
    assert(std::abs(d_center - (-half_size.y)) < 1e-4f);

    // 2. 直边边界点测试: 距离必须为 0
    // 右边中点 (150, 100)
    float d_right_edge = sd_rounded_box(Vec2{150.0f, 100.0f}, center, half_size, radii);
    std::cout << "  -> Right edge distance: " << d_right_edge << std::endl;
    assert(std::abs(d_right_edge) < 1e-4f);

    // 上边中点 (100, 70)
    float d_top_edge = sd_rounded_box(Vec2{100.0f, 70.0f}, center, half_size, radii);
    std::cout << "  -> Top edge distance: " << d_top_edge << std::endl;
    assert(std::abs(d_top_edge) < 1e-4f);

    // 3. 独立圆角象限测试:
    // 右下角 (象限 x>=0, y>=0) 圆角半径为 20:
    // 圆心位于 (100 + 50 - 20, 100 + 30 - 20) = (130, 110)
    Vec2 br_corner_center{130.0f, 110.0f};
    // 距离圆心 25.0 的点，其到边界距离应为 25.0 - 20.0 = +5.0
    Vec2 test_br_out = br_corner_center + Vec2{25.0f * 0.70710678f, 25.0f * 0.70710678f};
    float d_br_out = sd_rounded_box(test_br_out, center, half_size, radii);
    std::cout << "  -> Bottom-right corner (+5px outside): " << d_br_out << std::endl;
    assert(std::abs(d_br_out - 5.0f) < 1e-3f);

    // 4. 环形描边 (op_ring) 测试
    // 边框宽度 4px，边界处 (d=0) 的描边 SDF 距离应为 -2px (处于描边最中心)
    float d_stroke_center = op_ring(0.0f, 4.0f);
    assert(std::abs(d_stroke_center - (-2.0f)) < 1e-4f);

    // 描边外边缘 (d = +2px) 处，描边 SDF 距离应恰为 0
    float d_stroke_edge = op_ring(2.0f, 4.0f);
    assert(std::abs(d_stroke_edge - 0.0f) < 1e-4f);

    // 5. 亚像素覆盖率测试
    assert(evaluate_coverage(-1.0f) == 1.0f);
    assert(evaluate_coverage(0.0f) == 0.5f);
    assert(evaluate_coverage(+1.0f) == 0.0f);
    assert(evaluate_coverage(-0.25f) == 0.75f);

    std::cout << "[TEST] test_sdf PASSED successfully!" << std::endl;
    return 0;
}
