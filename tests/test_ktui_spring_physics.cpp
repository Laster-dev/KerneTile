#include <ktui/animation/animation.hpp>
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>

using namespace ktui;

void test_critically_damped_zero_overshoot() {
    std::cout << "[Test 1] 临界阻尼绝对零超调与解析收敛测试..." << std::endl;

    SpringConfig config = SpringConfig::with_damping_ratio(250.0f, 1.0f);
    SpringSolver solver(0.0f, config);

    assert(solver.is_settled());
    assert(solver.value() == 0.0f);

    solver.rebind(0.0f, 0.0f, 100.0f);
    assert(!solver.is_settled());

    float prev_val = 0.0f;
    float max_val = 0.0f;
    float dt = 1.0f / 60.0f; // 60 FPS
    int frame_count = 0;

    while (!solver.is_settled() && frame_count < 300) {
        solver.update(dt);
        float current = solver.value();

        // 临界阻尼单调递增，且绝不允许超调超过 100.0001f
        assert(current >= prev_val - 1e-4f);
        assert(current <= 100.0001f);

        if (current > max_val) {
            max_val = current;
        }

        prev_val = current;
        frame_count++;
    }

    assert(solver.is_settled());
    assert(std::abs(solver.value() - 100.0f) < 1e-3f);
    assert(solver.velocity() == 0.0f);
    assert(max_val <= 100.0001f); // 严格零超调

    std::cout << "  -> 临界阻尼在第 " << frame_count << " 帧完美收敛，最大位移: " << max_val << " (无超调)!" << std::endl;
}

void test_velocity_preservation_interruption() {
    std::cout << "[Test 2] 运动中高速打断速度保真 (C^1 连续性) 测试..." << std::endl;

    SpringSolver solver(0.0f, Springs::Snappy);
    solver.rebind(0.0f, 0.0f, 200.0f);

    float dt = 1.0f / 120.0f; // 120 FPS 高刷新率

    // 运行 10 帧，使物体加速到较高速度
    for (int i = 0; i < 10; ++i) {
        solver.update(dt);
    }

    float pos_before = solver.value();
    float vel_before = solver.velocity();

    // 此时应当具有可观的前进正速度
    assert(vel_before > 10.0f);

    // 突发事件：用户反向打断，将目标重置为 0.0f
    solver.rebind(pos_before, vel_before, 0.0f);

    // 检查打断瞬间的状态：位置与瞬时速度完全保真，连续无突变
    assert(std::abs(solver.value() - pos_before) < 1e-5f);
    assert(std::abs(solver.velocity() - vel_before) < 1e-5f);

    // 步进下一帧，由于惯性存在，下一帧位置应当继续短暂增加 (冲头)，随后在阻尼和反向拉力下减速回弹
    solver.update(dt);
    float pos_after = solver.value();
    assert(pos_after > pos_before); // 惯性继续向前滑行

    // 继续演化直到反向回到 0.0f 收敛
    int frames = 0;
    while (!solver.is_settled() && frames < 500) {
        solver.update(dt);
        frames++;
    }

    assert(solver.is_settled());
    assert(std::abs(solver.value() - 0.0f) < 1e-3f);
    std::cout << "  -> 高速打断保真验证成功！初速度: " << vel_before << " px/s，无缝反向收敛于 " << frames << " 帧。" << std::endl;
}

void test_underdamped_bouncy_oscillation() {
    std::cout << "[Test 3] 欠阻尼弹性震荡 (Bouncy) 测试..." << std::endl;

    // 欠阻尼 ζ = 0.4
    SpringConfig bouncy_config = SpringConfig::with_damping_ratio(150.0f, 0.4f);
    SpringSolver solver(0.0f, bouncy_config);
    solver.rebind(0.0f, 0.0f, 100.0f);

    float dt = 1.0f / 60.0f;
    float max_overshoot = 0.0f;
    bool had_overshoot = false;
    int frames = 0;

    while (!solver.is_settled() && frames < 500) {
        solver.update(dt);
        float val = solver.value();
        if (val > 100.0f) {
            had_overshoot = true;
            if (val > max_overshoot) max_overshoot = val;
        }
        frames++;
    }

    assert(had_overshoot);
    assert(max_overshoot > 105.0f); // 存在明显的弹性回弹超调
    assert(solver.is_settled());
    assert(std::abs(solver.value() - 100.0f) < 1e-3f);

    std::cout << "  -> 欠阻尼弹性震荡验证成功，峰值超调到达 " << max_overshoot << "，最终稳定归于 100.0f。" << std::endl;
}

void test_overdamped_decay() {
    std::cout << "[Test 4] 过阻尼平缓衰减测试..." << std::endl;

    SpringConfig overdamped_config = SpringConfig::with_damping_ratio(100.0f, 2.0f);
    SpringSolver solver(0.0f, overdamped_config);
    solver.rebind(0.0f, 0.0f, 100.0f);

    float dt = 1.0f / 60.0f;
    int frames = 0;
    while (!solver.is_settled() && frames < 800) {
        solver.update(dt);
        // 过阻尼绝不能超调
        assert(solver.value() <= 100.001f);
        frames++;
    }

    assert(solver.is_settled());
    assert(std::abs(solver.value() - 100.0f) < 1e-3f);
    std::cout << "  -> 过阻尼无震荡单调衰减验证成功！" << std::endl;
}

void test_multidimensional_animatables() {
    std::cout << "[Test 5] 多维复合类型 (Vec2, Color, Rect) 物理演算测试..." << std::endl;

    // 1. Vec2 动画
    AnimatedValue<Vec2> pos(Vec2{0.0f, 0.0f}, Springs::Snappy);
    pos = Vec2{100.0f, 200.0f};
    assert(!pos.is_settled());

    for (int i = 0; i < 200 && !pos.is_settled(); ++i) {
        pos.update(1.0f / 60.0f);
    }
    assert(pos.is_settled());
    assert(std::abs(pos.get().x - 100.0f) < 1e-2f);
    assert(std::abs(pos.get().y - 200.0f) < 1e-2f);

    // 2. Color 动画
    AnimatedValue<Color> color(Color{0.0f, 0.0f, 0.0f, 1.0f}, Springs::Smooth);
    color = Color{1.0f, 0.5f, 0.2f, 0.8f};
    for (int i = 0; i < 300 && !color.is_settled(); ++i) {
        color.update(1.0f / 60.0f);
    }
    assert(color.is_settled());
    assert(std::abs(color.get().r - 1.0f) < 1e-2f);
    assert(std::abs(color.get().g - 0.5f) < 1e-2f);
    assert(std::abs(color.get().b - 0.2f) < 1e-2f);
    assert(std::abs(color.get().a - 0.8f) < 1e-2f);

    // 3. Rect 动画
    AnimatedValue<Rect> rect(Rect::from_xywh(0.0f, 0.0f, 50.0f, 50.0f), Springs::Default);
    rect = Rect::from_xywh(10.0f, 20.0f, 300.0f, 400.0f);
    for (int i = 0; i < 300 && !rect.is_settled(); ++i) {
        rect.update(1.0f / 60.0f);
    }
    assert(rect.is_settled());
    assert(std::abs(rect.get().min_x - 10.0f) < 1e-2f);
    assert(std::abs(rect.get().width() - 300.0f) < 1e-2f);

    std::cout << "  -> 多维类型 Vec2 / Color / Rect 多通道物理独立求解全部通过！" << std::endl;
}

void test_animation_transaction_scope() {
    std::cout << "[Test 6] 事务级隐式动画 with_animation 测试..." << std::endl;

    AnimatedValue<float> width(100.0f, Springs::Smooth);
    assert(width.config() == Springs::Smooth);

    with_animation(Springs::Bouncy, [&] {
        width = 250.0f;
    });

    // 检查在 with_animation 内设置后，采用了 Bouncy 配置
    assert(width.config() == Springs::Bouncy);
    assert(!width.is_settled());

    // 离开事务范围后，AnimationScope::is_active 应为 false
    assert(!AnimationScope::is_active());

    std::cout << "  -> with_animation 事务作用域传播与嵌套恢复正常！" << std::endl;
}

void test_explicit_animation_controller() {
    std::cout << "[Test 7] 显式时间线控制器 AnimationController 测试..." << std::endl;

    AnimationController controller(0.5f); // 0.5 秒
    assert(controller.status() == AnimationStatus::Dismissed);
    assert(controller.value() == 0.0f);

    // 正向播放
    controller.forward();
    assert(controller.status() == AnimationStatus::Forward);
    assert(controller.is_animating());

    // 推进 0.25 秒 -> 应处于 0.5 左右
    controller.update(0.25f);
    assert(std::abs(controller.value() - 0.5f) < 1e-3f);
    assert(controller.status() == AnimationStatus::Forward);

    // 推进 0.3 秒 -> 到达终点 1.0f，状态 Completed
    controller.update(0.3f);
    assert(std::abs(controller.value() - 1.0f) < 1e-3f);
    assert(controller.status() == AnimationStatus::Completed);
    assert(!controller.is_animating());

    // 往返循环测试
    controller.repeat(true /* reverse_cycle */);
    assert(controller.is_animating());
    controller.update(0.5f); // 运行到反向
    assert(controller.status() == AnimationStatus::Reverse);

    // 停止测试
    controller.stop();
    assert(!controller.is_animating());

    // 重置测试
    controller.reset();
    assert(controller.status() == AnimationStatus::Dismissed);
    assert(controller.value() == 0.0f);

    std::cout << "  -> AnimationController 正向/反向/循环/停止/重置全部通过！" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "   KTUI 阶段 3: SOTA 物理弹簧解析动画系统测试    " << std::endl;
    std::cout << "==================================================" << std::endl;

    test_critically_damped_zero_overshoot();
    test_velocity_preservation_interruption();
    test_underdamped_bouncy_oscillation();
    test_overdamped_decay();
    test_multidimensional_animatables();
    test_animation_transaction_scope();
    test_explicit_animation_controller();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 3 物理弹簧动画全测试用例 100% 通过!   " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
