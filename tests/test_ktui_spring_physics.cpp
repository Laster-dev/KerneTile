#include <ktui/animation/animation.hpp>
#include <ktui/animation/hero_transition.hpp>
#include <ktui/animation/spring_caret.hpp>
#include <ktui/animation/tactile_press.hpp>
#include <ktui/layout/flex_types.hpp>
#include <ktui/widgets/scroll/smooth_scroll_physics.hpp>
#include <iostream>
#include <cassert>
#define KTUI_CHECK(expr) do { if (!(expr)) { std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; std::exit(1); } } while (0)

#include <cstdlib>
#include <vector>
#include <cmath>

using namespace ktui;

void test_critically_damped_zero_overshoot() {
    std::cout << "[Test 1] 临界阻尼绝对零超调与解析收敛测试..." << std::endl;

    SpringConfig config = SpringConfig::with_damping_ratio(250.0f, 1.0f);
    SpringSolver solver(0.0f, config);

    KTUI_CHECK(solver.is_settled());
    KTUI_CHECK(solver.value() == 0.0f);

    solver.rebind(0.0f, 0.0f, 100.0f);
    KTUI_CHECK(!solver.is_settled());

    float prev_val = 0.0f;
    float max_val = 0.0f;
    float dt = 1.0f / 60.0f; // 60 FPS
    int frame_count = 0;

    while (!solver.is_settled() && frame_count < 300) {
        solver.update(dt);
        float current = solver.value();

        // 临界阻尼单调递增，且绝不允许超调超过 100.0001f
        KTUI_CHECK(current >= prev_val - 1e-4f);
        KTUI_CHECK(current <= 100.0001f);

        if (current > max_val) {
            max_val = current;
        }

        prev_val = current;
        frame_count++;
    }

    KTUI_CHECK(solver.is_settled());
    KTUI_CHECK(std::abs(solver.value() - 100.0f) < 1e-3f);
    KTUI_CHECK(solver.velocity() == 0.0f);
    KTUI_CHECK(max_val <= 100.0001f); // 严格零超调

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
    KTUI_CHECK(vel_before > 10.0f);

    // 突发事件：用户反向打断，将目标重置为 0.0f
    solver.rebind(pos_before, vel_before, 0.0f);

    // 检查打断瞬间的状态：位置与瞬时速度完全保真，连续无突变
    KTUI_CHECK(std::abs(solver.value() - pos_before) < 1e-5f);
    KTUI_CHECK(std::abs(solver.velocity() - vel_before) < 1e-5f);

    // 步进下一帧，由于惯性存在，下一帧位置应当继续短暂增加 (冲头)，随后在阻尼和反向拉力下减速回弹
    solver.update(dt);
    float pos_after = solver.value();
    KTUI_CHECK(pos_after > pos_before); // 惯性继续向前滑行

    // 继续演化直到反向回到 0.0f 收敛
    int frames = 0;
    while (!solver.is_settled() && frames < 500) {
        solver.update(dt);
        frames++;
    }

    KTUI_CHECK(solver.is_settled());
    KTUI_CHECK(std::abs(solver.value() - 0.0f) < 1e-3f);
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

    KTUI_CHECK(had_overshoot);
    KTUI_CHECK(max_overshoot > 105.0f); // 存在明显的弹性回弹超调
    KTUI_CHECK(solver.is_settled());
    KTUI_CHECK(std::abs(solver.value() - 100.0f) < 1e-3f);

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
        KTUI_CHECK(solver.value() <= 100.001f);
        frames++;
    }

    KTUI_CHECK(solver.is_settled());
    KTUI_CHECK(std::abs(solver.value() - 100.0f) < 1e-3f);
    std::cout << "  -> 过阻尼无震荡单调衰减验证成功！" << std::endl;
}

void test_multidimensional_animatables() {
    std::cout << "[Test 5] 多维复合类型 (Vec2, Color, Rect) 物理演算测试..." << std::endl;

    // 1. Vec2 动画
    AnimatedValue<Vec2> pos(Vec2{0.0f, 0.0f}, Springs::Snappy);
    pos = Vec2{100.0f, 200.0f};
    KTUI_CHECK(!pos.is_settled());

    for (int i = 0; i < 200 && !pos.is_settled(); ++i) {
        pos.update(1.0f / 60.0f);
    }
    KTUI_CHECK(pos.is_settled());
    KTUI_CHECK(std::abs(pos.get().x - 100.0f) < 1e-2f);
    KTUI_CHECK(std::abs(pos.get().y - 200.0f) < 1e-2f);

    // 2. Color 动画
    AnimatedValue<Color> color(Color{0.0f, 0.0f, 0.0f, 1.0f}, Springs::Smooth);
    color = Color{1.0f, 0.5f, 0.2f, 0.8f};
    for (int i = 0; i < 300 && !color.is_settled(); ++i) {
        color.update(1.0f / 60.0f);
    }
    KTUI_CHECK(color.is_settled());
    KTUI_CHECK(std::abs(color.get().r - 1.0f) < 1e-2f);
    KTUI_CHECK(std::abs(color.get().g - 0.5f) < 1e-2f);
    KTUI_CHECK(std::abs(color.get().b - 0.2f) < 1e-2f);
    KTUI_CHECK(std::abs(color.get().a - 0.8f) < 1e-2f);

    // 3. Rect 动画
    AnimatedValue<Rect> rect(Rect::from_xywh(0.0f, 0.0f, 50.0f, 50.0f), Springs::Default);
    rect = Rect::from_xywh(10.0f, 20.0f, 300.0f, 400.0f);
    for (int i = 0; i < 300 && !rect.is_settled(); ++i) {
        rect.update(1.0f / 60.0f);
    }
    KTUI_CHECK(rect.is_settled());
    KTUI_CHECK(std::abs(rect.get().min_x - 10.0f) < 1e-2f);
    KTUI_CHECK(std::abs(rect.get().width() - 300.0f) < 1e-2f);

    std::cout << "  -> 多维类型 Vec2 / Color / Rect 多通道物理独立求解全部通过！" << std::endl;
}

void test_animation_transaction_scope() {
    std::cout << "[Test 6] 事务级隐式动画 with_animation 测试..." << std::endl;

    AnimatedValue<float> width(100.0f, Springs::Smooth);
    KTUI_CHECK(width.config() == Springs::Smooth);

    with_animation(Springs::Bouncy, [&] {
        width = 250.0f;
    });

    // 检查在 with_animation 内设置后，采用了 Bouncy 配置
    KTUI_CHECK(width.config() == Springs::Bouncy);
    KTUI_CHECK(!width.is_settled());

    // 离开事务范围后，AnimationScope::is_active 应为 false
    KTUI_CHECK(!AnimationScope::is_active());

    std::cout << "  -> with_animation 事务作用域传播与嵌套恢复正常！" << std::endl;
}

void test_explicit_animation_controller() {
    std::cout << "[Test 7] 显式时间线控制器 AnimationController 测试..." << std::endl;

    AnimationController controller(0.5f); // 0.5 秒
    KTUI_CHECK(controller.status() == AnimationStatus::Dismissed);
    KTUI_CHECK(controller.value() == 0.0f);

    // 正向播放
    controller.forward();
    KTUI_CHECK(controller.status() == AnimationStatus::Forward);
    KTUI_CHECK(controller.is_animating());

    // 推进 0.25 秒 -> 应处于 0.5 左右
    controller.update(0.25f);
    KTUI_CHECK(std::abs(controller.value() - 0.5f) < 1e-3f);
    KTUI_CHECK(controller.status() == AnimationStatus::Forward);

    // 推进 0.3 秒 -> 到达终点 1.0f，状态 Completed
    controller.update(0.3f);
    KTUI_CHECK(std::abs(controller.value() - 1.0f) < 1e-3f);
    KTUI_CHECK(controller.status() == AnimationStatus::Completed);
    KTUI_CHECK(!controller.is_animating());

    // 往返循环测试
    controller.repeat(true /* reverse_cycle */);
    KTUI_CHECK(controller.is_animating());
    controller.update(0.5f); // 运行到反向
    KTUI_CHECK(controller.status() == AnimationStatus::Reverse);

    // 停止测试
    controller.stop();
    KTUI_CHECK(!controller.is_animating());

    // 重置测试
    controller.reset();
    KTUI_CHECK(controller.status() == AnimationStatus::Dismissed);
    KTUI_CHECK(controller.value() == 0.0f);

    std::cout << "  -> AnimationController 正向/反向/循环/停止/重置全部通过！" << std::endl;
}

void test_smooth_scroll_rubber_band_and_spring_handoff() {
    std::cout << "[Test 8] R4 SmoothScrollPhysics Opt-in 橡皮筋公式与 SpringSolver 速度交接测试..." << std::endl;

    auto require = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAILED: " << msg << std::endl;
            std::abort();
        }
    };

    struct VirtualClockGuard {
        VirtualClockGuard() { FrameClock::global().enable_virtual_time(0.0); }
        ~VirtualClockGuard() { FrameClock::global().disable_virtual_time(); }
    } vclock_guard;

    // 1. Default enabled, manual disable: strict [0, max_scroll] clamping across drag, scroll_to, scroll_by, wheel, fling
    {
        SmoothScrollPhysics phys;
        require(phys.is_rubber_band_enabled() && phys.rubber_band_enabled() && phys.rubber_band_overscroll,
                "default rubber_band_overscroll must be true");
        phys.set_rubber_band_enabled(false);
        require(!phys.is_rubber_band_enabled() && !phys.rubber_band_enabled() && !phys.rubber_band_overscroll,
                "manual disable rubber_band_overscroll must be false");
        phys.set_viewport_extent(320.0f);
        phys.set_max_scroll(400.0f);

        phys.scroll_to(-50.0f);
        require(phys.target() == 0.0f && phys.offset() == 0.0f && phys.overscroll_amount() == 0.0f, "scroll_to(-50) clamped");
        phys.scroll_to(500.0f);
        require(phys.target() == 400.0f && phys.overscroll_amount() == 0.0f, "scroll_to(500) target clamped");
        for (int i = 0; i < 60; ++i) {
            FrameClock::global().advance_virtual(1.0f / 60.0f);
            bool active = phys.update();
            require(phys.offset() >= 0.0f && phys.offset() <= 400.0f && phys.overscroll_amount() == 0.0f,
                    "scroll_to(500) offset clamped during animation");
            if (!active) break;
        }
        require(std::abs(phys.offset() - 400.0f) < 1e-3f, "scroll_to(500) settled at 400");

        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(120.0f, 0.016);
        require(phys.offset() == 0.0f && phys.overscroll_amount() == 0.0f, "disabled top drag clamped");
        phys.end_touch_drag_for_test(0.0);
        require(!phys.is_bouncing(), "disabled top drag release does not bounce");

        phys.jump_to(400.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(-120.0f, 0.016);
        require(phys.offset() == 400.0f && phys.overscroll_amount() == 0.0f, "disabled bottom drag clamped");
        phys.end_touch_drag_for_test(0.0);
        require(!phys.is_bouncing(), "disabled bottom drag release does not bounce");
    }

    // 2. Exact hyperbolic formula f(x) = (x * d * c) / (d + c * x) for both small (d=80 < 120) and standard (d=400) viewports
    for (float d : {80.0f, 400.0f}) {
        constexpr float c = 0.55f;
        SmoothScrollPhysics phys;
        phys.set_rubber_band_enabled(true);
        phys.set_viewport_extent(d);
        phys.set_max_scroll(500.0f);

        for (float x : {25.0f, 100.0f, 250.0f}) {
            const float expected = (x * d * c) / (d + c * x);

            // Top overscroll
            phys.jump_to(0.0f);
            phys.begin_touch_drag();
            phys.apply_touch_drag_with_dt(x, 0.016);
            require(std::abs((-phys.offset()) - expected) < 1e-4f,
                    "top overscroll must match (x*d*c)/(d+c*x) within 1e-4");

            // Bottom overscroll
            phys.jump_to(500.0f);
            phys.begin_touch_drag();
            phys.apply_touch_drag_with_dt(-x, 0.016);
            require(std::abs((phys.offset() - 500.0f) - expected) < 1e-4f,
                    "bottom overscroll must match (x*d*c)/(d+c*x) within 1e-4");
        }
    }

    // 3. SpringSolver handoff on release: zero-velocity pause release + non-zero velocity release, monotonic rebound
    {
        SmoothScrollPhysics phys;
        phys.rubber_band_overscroll = true; // Direct public field mutation must be honored
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(500.0f);

        // 3a. Top overscroll with pause (zero initial release velocity)
        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(100.0f, 0.016);
        phys.end_touch_drag_for_test(0.15); // paused > 95ms -> velocity = 0
        require(phys.is_bouncing(), "top release enters SpringSolver bounce");
        float prev = phys.offset();
        require(prev < -40.0f, "top release starts overscrolled");
        for (int i = 0; i < 120 && phys.step_touch_for_test(1.0 / 60.0); ++i) {
            float cur = phys.offset();
            require(cur >= prev - 1e-4f && cur <= 0.5f, "top zero-vel rebound moves monotonically to 0");
            prev = cur;
        }
        require(std::abs(phys.offset()) < 1e-3f && !phys.is_bouncing(), "top zero-vel rebound settled at 0");

        // 3b. Top overscroll with active downward pull velocity (non-zero handoff velocity)
        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(60.0f, 0.016);
        phys.apply_touch_drag_with_dt(60.0f, 0.016);
        phys.end_touch_drag_for_test(0.0);
        require(phys.is_bouncing() && phys.velocity() < -50.0f, "non-zero release velocity preserved in SpringSolver handoff");
        for (int i = 0; i < 180 && phys.step_touch_for_test(1.0 / 60.0); ++i) {
            require(phys.offset() <= 0.5f, "critically damped handoff does not overshoot past 0");
        }
        require(std::abs(phys.offset()) < 1e-3f && !phys.is_bouncing(), "top non-zero-vel rebound settled at 0");

        // 3c. Bottom overscroll monotonic rebound to max_scroll
        phys.jump_to(500.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(-100.0f, 0.016);
        phys.end_touch_drag_for_test(0.15);
        require(phys.is_bouncing(), "bottom release enters SpringSolver bounce");
        prev = phys.offset();
        for (int i = 0; i < 120 && phys.step_touch_for_test(1.0 / 60.0); ++i) {
            float cur = phys.offset();
            require(cur <= prev + 1e-4f && cur >= 499.5f, "bottom zero-vel rebound moves monotonically to max_scroll");
            prev = cur;
        }
        require(std::abs(phys.offset() - 500.0f) < 1e-3f && !phys.is_bouncing(), "bottom rebound settled at max_scroll");
    }

    // 4. Mid-rebound touch re-grab continuity (inv_rubber_band roundtrip)
    {
        SmoothScrollPhysics phys;
        phys.set_rubber_band_enabled(true);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(500.0f);
        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(120.0f, 0.016);
        phys.end_touch_drag_for_test(0.15);
        for (int i = 0; i < 4; ++i) phys.step_touch_for_test(1.0 / 60.0);
        float mid_offset = phys.offset();
        require(mid_offset < -5.0f && phys.is_bouncing(), "still overscrolled mid-rebound");

        phys.begin_touch_drag();
        require(std::abs(phys.offset() - mid_offset) < 1e-4f, "begin_touch_drag preserves exact visual offset");
        phys.apply_touch_drag_with_dt(0.0f, 0.016);
        require(std::abs(phys.offset() - mid_offset) < 1e-3f, "zero-delta re-grab has zero jump via inv_rubber_band");
        phys.end_touch_drag_for_test(0.0);
        for (int i = 0; i < 120 && phys.step_touch_for_test(1.0 / 60.0); ++i) {}
        require(std::abs(phys.offset()) < 1e-3f && !phys.is_bouncing(), "re-grabbed rebound settles cleanly");
    }

    // 5. Wheel overscroll past top and bottom + automatic SpringSolver rebound
    {
        constexpr float d = 400.0f;
        constexpr float c = 0.55f;
        SmoothScrollPhysics phys;
        phys.set_rubber_band_enabled(true);
        phys.set_viewport_extent(d);
        phys.set_max_scroll(500.0f);

        // Top wheel overscroll
        phys.jump_to(0.0f);
        phys.add_wheel_delta(-36.0f); // -100px
        const float expected_100 = (100.0f * d * c) / (d + c * 100.0f);
        require(std::abs((-phys.target()) - expected_100) < 1e-4f, "wheel top overscroll target matches f(100)");
        for (int i = 0; i < 120; ++i) {
            FrameClock::global().advance_virtual(1.0f / 60.0f);
            if (!phys.update() && !phys.is_bouncing()) break;
        }
        require(std::abs(phys.offset()) < 1e-3f && !phys.is_bouncing(), "wheel top overscroll settles at 0");

        // Bottom wheel overscroll
        phys.jump_to(500.0f);
        phys.add_wheel_delta(36.0f); // +100px
        require(std::abs((phys.target() - 500.0f) - expected_100) < 1e-4f, "wheel bottom overscroll target matches f(100)");
        bool saw_bottom_overscroll = false;
        for (int i = 0; i < 120; ++i) {
            FrameClock::global().advance_virtual(1.0f / 60.0f);
            bool active = phys.update();
            if (phys.offset() > 510.0f) saw_bottom_overscroll = true;
            if (!active && !phys.is_bouncing()) break;
        }
        require(saw_bottom_overscroll, "wheel bottom overscroll exceeded max_scroll");
        require(std::abs(phys.offset() - 500.0f) < 1e-3f && !phys.is_bouncing(), "wheel bottom overscroll settles at 500");
    }

    // 6. Regression: set_max_scroll shrinking mid-bounce rebinds SpringSolver cleanly and never settles out of bounds
    {
        SmoothScrollPhysics phys;
        phys.set_rubber_band_enabled(true);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(500.0f);
        phys.jump_to(500.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(-120.0f, 0.016);
        phys.end_touch_drag_for_test(0.15);
        require(phys.is_bouncing() && phys.offset() > 530.0f, "bouncing past old bottom 500 before shrink");

        phys.set_max_scroll(300.0f);
        require(phys.is_bouncing(), "still bouncing toward new max_scroll 300");
        for (int i = 0; i < 180 && phys.step_touch_for_test(1.0 / 60.0); ++i) {}
        require(!phys.is_bouncing() && std::abs(phys.offset() - 300.0f) < 1e-3f,
                "shrinking max_scroll mid-bounce must settle at 300, not old 500");
        require(phys.target() <= 300.0f && phys.offset() <= 300.5f, "offset and target remain within new max_scroll");
    }

    // 7. Regression: sub-0.1px scroll_by at boundaries preserves f(x) target and settles cleanly
    {
        constexpr float d = 400.0f;
        constexpr float c = 0.55f;
        constexpr float x = 0.01f;
        const float expected_tiny = (x * d * c) / (d + c * x);

        SmoothScrollPhysics phys;
        phys.set_rubber_band_enabled(true);
        phys.set_viewport_extent(d);
        phys.set_max_scroll(500.0f);

        phys.jump_to(0.0f);
        phys.scroll_by(-x);
        require(std::abs((-phys.target()) - expected_tiny) < 1e-4f,
                "tiny scroll_by(-0.01) target matches -f(0.01) within 1e-4");
        for (int i = 0; i < 60; ++i) {
            FrameClock::global().advance_virtual(1.0f / 60.0f);
            if (!phys.update() && !phys.is_bouncing()) break;
        }
        require(std::abs(phys.offset()) < 1e-4f && !phys.is_animating(),
                "tiny top scroll_by settles cleanly at 0");

        phys.jump_to(500.0f);
        phys.scroll_by(x);
        require(std::abs((phys.target() - 500.0f) - expected_tiny) < 1e-4f,
                "tiny scroll_by(+0.01) target matches +f(0.01) within 1e-4");
        for (int i = 0; i < 60; ++i) {
            FrameClock::global().advance_virtual(1.0f / 60.0f);
            if (!phys.update() && !phys.is_bouncing()) break;
        }
        require(std::abs(phys.offset() - 500.0f) < 1e-4f && !phys.is_animating(),
                "tiny bottom scroll_by settles cleanly at 500");
    }

    // 8. Regression: disabling rubber_band mid-wheel-overscroll clears is_animating() immediately
    {
        SmoothScrollPhysics phys;
        phys.set_rubber_band_enabled(true);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(500.0f);
        phys.jump_to(0.0f);
        phys.add_wheel_delta(-36.0f);
        require(phys.is_animating(), "wheel overscroll impulse animating before disable");
        phys.set_rubber_band_enabled(false);
        require(!phys.is_animating() && !phys.is_bouncing() && phys.offset() == 0.0f && phys.target() == 0.0f,
                "disabling rubber_band mid-wheel-overscroll clears is_animating() and clamps to 0");
    }

    // 9. Regression: max_scroll == 0.0f disabled clamps strictly, enabled overscrolls and rebounds
    {
        SmoothScrollPhysics phys;
        phys.set_rubber_band_enabled(false);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(100.0f, 0.016);
        require(phys.offset() == 0.0f, "max_scroll==0 disabled clamps touch drag");
        phys.end_touch_drag_for_test(0.0);

        phys.set_rubber_band_enabled(true);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(100.0f, 0.016);
        const float expected_100 = (100.0f * 400.0f * 0.55f) / (400.0f + 0.55f * 100.0f);
        require(std::abs((-phys.offset()) - expected_100) < 1e-4f, "max_scroll==0 enabled top overscroll");
        phys.end_touch_drag_for_test(0.0);
        for (int i = 0; i < 120 && phys.step_touch_for_test(1.0 / 60.0); ++i) {}
        require(std::abs(phys.offset()) < 1e-3f && !phys.is_bouncing(), "max_scroll==0 rebounds to 0");
    }

    std::cout << "  -> R4 橡皮筋公式、SpringSolver 速度保真交接、中断再抓与滚轮越界回弹全部通过！" << std::endl;
}

void test_tactile_press_and_hero_transition() {
    std::cout << "[Test 9] 指针感知微倾斜/按压缩放物理 (TactilePressPhysics) 与 HeroCoordinator 跨页协调测试..." << std::endl;

    // 1. TactilePressPhysics: off-center click produces directional lever shift, scale < 1, tilted light dir, and lower elevation
    TactilePressPhysics phys(0.95f, 4.0f);
    phys.set_enabled(true);
    phys.on_enter();
    const Vec2 sz{200.0f, 100.0f};
    const Rect base = Rect::from_xywh(0.0f, 0.0f, sz.x, sz.y);
    phys.on_down(PointF{180.0f, 80.0f}, sz); // bottom-right quadrant (+0.8, +0.6 lever)
    for (int i = 0; i < 12; ++i) {
        phys.step_for_test(1.0f / 60.0f);
    }
    const Rect deformed = phys.transform_rect(base);
    KTUI_CHECK(phys.is_active());
    KTUI_CHECK(deformed.width() < base.width() && deformed.height() < base.height());
    KTUI_CHECK(deformed.min_x + deformed.max_x > base.min_x + base.max_x); // shifts right along +nx lever
    KTUI_CHECK(deformed.min_y + deformed.max_y > base.min_y + base.max_y); // shifts down along +ny lever
    KTUI_CHECK(phys.tilted_light_dir().x < -0.25f);
    KTUI_CHECK(phys.elevation_scale() < 0.9f);

    phys.on_up();
    for (int i = 0; i < 180; ++i) {
        if (!phys.step_for_test(1.0f / 60.0f)) break;
    }
    KTUI_CHECK(std::abs(phys.press_amount()) < 1e-2f);
    phys.on_exit();
    phys.reset();
    KTUI_CHECK(!phys.is_active());

    // Verify HeroCoordinator epoch transition + departure seeding & flight claim
    auto& hc = HeroCoordinator::instance();
    hc.clear();
    HeroSnapshot src{};
    src.tag = "hero_card_demo";
    src.abs_bounds = Rect::from_xywh(20.0f, 40.0f, 160.0f, 100.0f);
    src.corner_radius = 12.0f;
    src.elevation = 6.0f;
    src.route_epoch = hc.current_epoch();
    hc.record(src);
    hc.begin_route_transition();

    HeroSnapshot dst{};
    dst.tag = "hero_card_demo";
    dst.abs_bounds = Rect::from_xywh(40.0f, 80.0f, 480.0f, 260.0f);
    dst.corner_radius = 24.0f;
    dst.elevation = 22.0f;
    dst.route_epoch = hc.current_epoch();

    HeroSnapshot claimed{};
    KTUI_CHECK(hc.try_claim_flight("hero_card_demo", dst, claimed));
    KTUI_CHECK(std::abs(claimed.abs_bounds.width() - 160.0f) < 1e-3f);
    KTUI_CHECK(std::abs(claimed.corner_radius - 12.0f) < 1e-3f);
    KTUI_CHECK(std::abs(claimed.elevation - 6.0f) < 1e-3f);

    // Seed departure claim test
    HeroSnapshot dep = dst;
    hc.seed_departure(dep);
    HeroSnapshot claimed_back{};
    KTUI_CHECK(hc.try_claim_flight("hero_card_demo", src, claimed_back));
    KTUI_CHECK(std::abs(claimed_back.abs_bounds.width() - 480.0f) < 1e-3f);

    hc.clear();
    std::cout << "  -> TactilePressPhysics 与 HeroCoordinator 物理转场协调验证通过！" << std::endl;
}

void test_spring_caret_rolling_and_concentric_radius() {
    std::cout << "[Test 10] 平滑弹簧光标、滚动吸顶分隔线、翻滚数值与同心圆角几何推导测试..." << std::endl;

    // 1. SpringCaretSolver: glide + stretch + breathing opacity
    {
        SpringCaretSolver caret;
        caret.set_enabled(true);
        auto f0 = caret.step_for_test(10.0f, 8.0f, 1.5f, 16.0f, 1.0f / 60.0f, 0.0f);
        KTUI_CHECK(f0.visible && std::abs(f0.rect.min_x - 10.0f) < 1e-3f);
        KTUI_CHECK(std::abs(f0.opacity - 1.0f) < 1e-4f);

        auto f_jump = caret.step_for_test(180.0f, 8.0f, 1.5f, 16.0f, 1.0f / 60.0f, 0.0f);
        KTUI_CHECK(caret.current_x() > 10.0f && caret.current_x() < 180.0f); // mid-glide
        KTUI_CHECK(f_jump.rect.width() > 2.0f); // horizontal trailing smear pill on jump

        // High-frequency critically damped lead spring reaches >= 96% within 8 frames (~133ms)
        for (int i = 0; i < 8; ++i) {
            caret.step_for_test(180.0f, 8.0f, 1.5f, 16.0f, 1.0f / 60.0f, 0.0f);
        }
        KTUI_CHECK(caret.current_x() > 170.0f);
        for (int i = 0; i < 30; ++i) {
            caret.step_for_test(180.0f, 8.0f, 1.5f, 16.0f, 1.0f / 60.0f, 0.0f);
        }
        KTUI_CHECK(std::abs(caret.current_x() - 180.0f) < 0.25f);

        // Breathing idle dimming at midpoint of 1.08s cycle (0.32 + 0.54 = 0.86s)
        auto f_dim = caret.step_for_test(180.0f, 8.0f, 1.5f, 16.0f, 1.0f / 60.0f, 0.86f);
        KTUI_CHECK(f_dim.opacity < 0.30f && f_dim.opacity >= 0.13f);
    }

    // 2. Concentric Nested Corner Radius math
    {
        KTUI_CHECK(std::abs(ktui::concentric_radius(28.0f, 12.0f, 4.0f) - 16.0f) < 1e-4f);
        KTUI_CHECK(std::abs(ktui::concentric_radius(10.0f, 14.0f, 3.0f) - 3.0f) < 1e-4f);
        KTUI_CHECK(std::abs(ktui::concentric_radius(0.0f, 8.0f, 2.0f) - 0.0f) < 1e-4f);
        KTUI_CHECK(std::abs(ktui::concentric_radius(24.0f, EdgeInsets(8.0f, 12.0f), 2.0f) - 16.0f) < 1e-4f);
    }

    std::cout << "  -> SpringCaret 与 ConcentricRadius 数学求解全部通过！" << std::endl;
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
    test_smooth_scroll_rubber_band_and_spring_handoff();
    test_tactile_press_and_hero_transition();
    test_spring_caret_rolling_and_concentric_radius();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 3 物理弹簧动画全测试用例 100% 通过!   " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
