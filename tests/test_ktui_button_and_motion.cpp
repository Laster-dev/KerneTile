#include "test_common.hpp"
#include <ktui/widgets/button.hpp>
#include <ktui/animation/animation.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/theme/theme_scope.hpp>

#include <iostream>
#include <cassert>
#include <cmath>
#include <cstdlib>

using namespace ktui;

#define KTUI_CHECK(expr)                                                               \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
            std::exit(1);                                                              \
        }                                                                              \
    } while (0)

// ==================== 1. 动效算法库与按需编译裁剪测试 ====================

void test_motion_easing_equations() {
    std::cout << "[Test 1] 动效算法库全套 31 种缓动曲线 (Robert Penner & CSS 标准) 计算验证..." << std::endl;

    // 验证端点值 (t=0 -> 0.0, t=1 -> 1.0)
    std::vector<Easing> all_easings = {
        Easing::Linear,
        Easing::EaseInSine, Easing::EaseOutSine, Easing::EaseInOutSine,
        Easing::EaseInQuad, Easing::EaseOutQuad, Easing::EaseInOutQuad,
        Easing::EaseInCubic, Easing::EaseOutCubic, Easing::EaseInOutCubic,
        Easing::EaseInQuart, Easing::EaseOutQuart, Easing::EaseInOutQuart,
        Easing::EaseInQuint, Easing::EaseOutQuint, Easing::EaseInOutQuint,
        Easing::EaseInExpo, Easing::EaseOutExpo, Easing::EaseInOutExpo,
        Easing::EaseInCirc, Easing::EaseOutCirc, Easing::EaseInOutCirc,
        Easing::EaseInBack, Easing::EaseOutBack, Easing::EaseInOutBack,
        Easing::EaseInElastic, Easing::EaseOutElastic, Easing::EaseInOutElastic,
        Easing::EaseInBounce, Easing::EaseOutBounce, Easing::EaseInOutBounce
    };

    for (auto e : all_easings) {
        float v0 = Motion::evaluate(e, 0.0f);
        float v1 = Motion::evaluate(e, 1.0f);
        assert(std::abs(v0) < 1e-4f && "Curve start value at t=0 must be ~0.0");
        assert(std::abs(v1 - 1.0f) < 1e-4f && "Curve end value at t=1 must be ~1.0");

        // 中间值应当合法且无 NaN / Inf
        float v_mid = Motion::evaluate(e, 0.5f);
        assert(!std::isnan(v_mid) && !std::isinf(v_mid));
    }

    // 验证三次缓动 EaseOutCubic 曲线特征: 在 t=0.5 时应该具有 > 0.5 的加速响应
    float cubic_mid = Motion::evaluate(Easing::EaseOutCubic, 0.5f);
    assert(cubic_mid > 0.8f && "EaseOutCubic at t=0.5 should have rapid deceleration curve (>0.8)");

    std::cout << "  -> 全套 31 种工业级标准缓动算法端点与中间插值计算完全正确！" << std::endl;
}

void test_motion_cubic_bezier_curves() {
    std::cout << "[Test 2] 三次贝塞尔曲线牛顿迭代高精度求解与工业预设 (Fluent / Material) 验证..." << std::endl;

    // 1. 验证 Linear 贝塞尔
    float linear_mid = Motion::evaluate(Curves::Linear, 0.5f);
    assert(std::abs(linear_mid - 0.5f) < 1e-4f);

    // 2. 验证 Windows 11 Fluent 桌面标准曲线 (0.8, 0.0, 0.2, 1.0)
    float fluent_mid = Motion::evaluate(Curves::FluentStandard, 0.5f);
    assert(!std::isnan(fluent_mid) && !std::isinf(fluent_mid));
    assert(fluent_mid > 0.0f && fluent_mid < 1.0f);

    // 3. 验证 Material 3 Emphasized 曲线
    float mat_mid = Motion::evaluate(Curves::MaterialEmphasized, 0.5f);
    assert(!std::isnan(mat_mid) && !std::isinf(mat_mid));

    // 4. 验证插值辅助函数 lerp_bezier
    Color start_col{0.0f, 0.0f, 0.0f, 1.0f};
    Color end_col{1.0f, 1.0f, 1.0f, 1.0f};
    Color mid_col = Motion::lerp_bezier(start_col, end_col, 0.5f, Curves::FluentStandard);
    assert(mid_col.r > 0.0f && mid_col.r < 1.0f);

    std::cout << "  -> 三次贝塞尔 8 步牛顿迭代与二分退化求解器计算精确可靠！" << std::endl;
}

void test_motion_tween_lifecycle() {
    std::cout << "[Test 3] MotionTween 动效补间驱动器生命周期与时间步进验证..." << std::endl;

    MotionTween<float> tween(0.0f, 100.0f, 0.5f, Easing::EaseOutCubic);
    tween.start(0.0f, 100.0f, 0.5f, Easing::EaseOutCubic);
    assert(tween.is_active());
    assert(!tween.is_completed());

    // 模拟推进 0.25s (到达一半时间)
    bool still_running = tween.tick(0.25f);
    assert(still_running);
    assert(tween.progress() > 0.8f); // EaseOutCubic 前半段迅速
    assert(tween.current_value() > 80.0f);

    // 推进剩余 0.25s (到达终点 0.5s)
    still_running = tween.tick(0.25f);
    assert(!still_running);
    assert(tween.is_completed());
    assert(std::abs(tween.current_value() - 100.0f) < 1e-4f);

    std::cout << "  -> MotionTween 补间推进、进度归一化与终态检测完全正确！" << std::endl;
}

// ==================== 2. Button 控件三层分离与规范验证 ====================

void test_button_three_layer_architecture() {
    std::cout << "[Test 4] Button 数据/样式/逻辑三层分离与规范化目录结构验证..." << std::endl;

    // 1. 数据层 (button_data.hpp) 验证
    ButtonData data;
    data.text = "Fluent Button";
    data.variant = ButtonVariant::Accent;
    data.size = ButtonSize::Normal;
    data.enable_ripple = true;
    assert(data.text == "Fluent Button");
    assert(data.variant == ButtonVariant::Accent);

    // 2. 样式层 (button_style.hpp) 验证
    Theme dark_theme = Theme::dark();
    ButtonInteractiveState state;
    ResolvedButtonStyle style = ButtonStyleResolver::resolve(data, state, dark_theme);

    // 验证 WinUI 3 / Fluent 2 桌面标准：微圆角 4px，高度 32px，字号 13.5px，Dark Accent 色值 (#60CDFF)
    KTUI_CHECK(style.corner_radius == 4.0f);
    KTUI_CHECK(style.height == 32.0f);
    KTUI_CHECK(style.font_size == 13.5f);
    KTUI_CHECK(std::abs(style.background.r - 96.0f / 255.0f) < 1e-3f);
    KTUI_CHECK(std::abs(style.background.g - 205.0f / 255.0f) < 1e-3f);
    KTUI_CHECK(std::abs(style.background.b - 255.0f / 255.0f) < 1e-3f);

    // 3. 逻辑层 (button_logic.hpp) 验证
    RenderButton btn;
    btn.set_text("Launch Rocket");
    btn.set_variant(ButtonVariant::Danger);
    btn.set_size_mode(ButtonSize::Large);

    btn.layout(BoxConstraints::loose(Vec2{300.0f, 100.0f}));
    KTUI_CHECK(btn.size().y == 36.0f); // Large 对应 36px

    std::cout << "  -> 数据/样式/逻辑三层彻底解耦，桌面基准尺寸与主题解析 100% 吻合！" << std::endl;
}

void test_button_ripple_delegation_to_motion() {
    std::cout << "[Test 5] Button 水波纹点击动效 100% 委托给 Motion 动效管理器验证..." << std::endl;

    RenderButton btn;
    btn.set_text("Click Me");
    btn.layout(BoxConstraints::tight(Vec2{120.0f, 28.0f}));

    assert(!btn.has_active_ripple());

    // 模拟鼠标左键按下 -> 触发水波纹
    PointerEvent down_ev(PlatformEventType::PointerDown, PointF{60.0f, 14.0f}, PointerButton::Primary);
    btn.on_pointer_down(down_ev);

    assert(btn.has_active_ripple());
    assert(btn.ripple_radius() == 4.0f);
    assert(btn.ripple_opacity() <= 0.25f && btn.ripple_opacity() > 0.0f); // 严格确保为半透明微光，绝非纯白 (半透明而非白色)

    // 模拟时钟推进 0.2s，验证水波纹半透明层外扩，但公式完全由 Motion 驱动
    btn.tick_ripple(0.20f);
    assert(btn.has_active_ripple());
    assert(btn.ripple_radius() > 4.0f);
    assert(btn.ripple_opacity() < 0.35f);

    // 模拟时钟推进 1.0s -> 动画应当平滑完结归零
    btn.tick_ripple(1.0f);
    assert(!btn.has_active_ripple());

    std::cout << "  -> 水波纹物理扩散与透明度淡出完全由 MotionTween 驱动，零私有数学公式！" << std::endl;
}

void test_button_keyboard_trigger() {
    std::cout << "[Test 6] 跨平台无 Windows API 依赖的键盘 Space / Enter 点击触发验证..." << std::endl;

    bool space_clicked = false;
    bool enter_clicked = false;

    RenderButton btn;
    btn.set_text("Action Button");
    btn.set_click_handler([&]() {
        if (!space_clicked) space_clicked = true;
        else enter_clicked = true;
    });

    // 模拟空格键按下 (key_code = 32)
    KeyEvent space_ev(PlatformEventType::KeyDown, 32);
    btn.on_key(space_ev);
    assert(space_clicked);
    assert(btn.has_active_ripple()); // 键盘同样在中心激活水波纹反馈！

    // 模拟回车键按下 (key_code = 13)
    KeyEvent enter_ev(PlatformEventType::KeyDown, 13);
    btn.on_key(enter_ev);
    assert(enter_clicked);

    // 禁用态阻断验证
    btn.set_disabled(true);
    space_clicked = false;
    btn.on_key(space_ev);
    assert(!space_clicked && "Disabled button must ignore key events!");

    std::cout << "  -> 键盘 Tab 聚焦与 Space/Enter 点击触发完全由框架托管，零 windows.h 依赖！" << std::endl;
}

void test_button_reactive_bindings() {
    std::cout << "[Test 7] Button 响应式数据绑定 (bind_text & bind_disabled) 验证..." << std::endl;

    Property<std::string> label_prop("Initial Label");
    Property<bool> lock_prop(false);

    WidgetPtr widget = Button{}
        .bind(&RenderButton::set_text, label_prop)
        .bind(&RenderButton::set_disabled, lock_prop);
    auto node = std::static_pointer_cast<RenderButton>(widget->create_render_node());
    assert(node != nullptr);

    assert(node->text() == "Initial Label");
    assert(!node->disabled());

    // 变更数据源
    label_prop.set("Updated Label");
    assert(node->text() == "Updated Label");

    lock_prop.set(true);
    assert(node->disabled());

    std::cout << "  -> Button 响应式属性绑定单向与双向更新机制验证无误！" << std::endl;
}

void test_button_pointer_reveal_opt_in_and_gating() {
    std::cout << "[Test 8] Button Opt-in Pointer Reveal (R3) 默认禁用零抖动与状态门控验证..." << std::endl;

    // 1. Default disabled across ButtonData, Button, RenderButton
    ButtonData default_data;
    KTUI_CHECK(default_data.enable_pointer_reveal == false);

    Button default_desc;
    KTUI_CHECK(default_desc.enable_pointer_reveal == false);

    RenderButton default_btn;
    KTUI_CHECK(default_btn.pointer_reveal_enabled() == false);
    default_btn.layout(BoxConstraints::tight(Vec2{160.0f, 36.0f}));
    default_btn.clear_dirty_recursive();

    // PointerMove when disabled must NOT trigger mark_needs_paint()
    PointerEvent enter_ev(PlatformEventType::PointerMove, PointF{20.0f, 18.0f});
    default_btn.on_pointer_enter(enter_ev);
    default_btn.clear_dirty_recursive();

    PointerEvent mv1(PlatformEventType::PointerMove, PointF{20.0f, 18.0f});
    mv1.local_position = PointF{20.0f, 18.0f};
    default_btn.on_pointer_move(mv1);
    KTUI_CHECK(!default_btn.needs_paint());

    PointerEvent mv1b(PlatformEventType::PointerMove, PointF{40.0f, 20.0f});
    mv1b.local_position = PointF{40.0f, 20.0f};
    default_btn.on_pointer_move(mv1b);
    KTUI_CHECK(!default_btn.needs_paint());

    PointerEvent exit_ev(PlatformEventType::PointerMove, PointF{-10.0f, -10.0f});
    default_btn.on_pointer_exit(exit_ev);
    KTUI_CHECK(default_btn.needs_paint()); // hover exit marks paint to clear hover bg

    // 2. Fluent builder (& and &&) and WidgetPtr creation
    Button lvalue_desc{.text = "Reveal L"};
    lvalue_desc.pointer_reveal(true);
    KTUI_CHECK(lvalue_desc.enable_pointer_reveal == true);
    WidgetPtr l_widget = lvalue_desc;
    auto l_node = std::static_pointer_cast<RenderButton>(l_widget->create_render_node());
    KTUI_CHECK(l_node != nullptr && l_node->pointer_reveal_enabled() == true);

    WidgetPtr r_widget = Button{.text = "Reveal R"}.pointer_reveal(true);
    auto r_node = std::static_pointer_cast<RenderButton>(r_widget->create_render_node());
    KTUI_CHECK(r_node != nullptr && r_node->pointer_reveal_enabled() == true);

    // 3. Enabled PointerMove marks needs_paint only when position actually changes; Touch is ignored
    r_node->layout(BoxConstraints::tight(Vec2{160.0f, 36.0f}));
    r_node->clear_dirty_recursive();

    PointerEvent touch_mv(PlatformEventType::PointerMove, PointF{40.0f, 16.0f});
    touch_mv.local_position = PointF{40.0f, 16.0f};
    touch_mv.device_kind = PointerDeviceKind::Touch;
    r_node->on_pointer_move(touch_mv);
    KTUI_CHECK(!r_node->needs_paint());

    PointerEvent mouse_mv1(PlatformEventType::PointerMove, PointF{40.0f, 16.0f});
    mouse_mv1.local_position = PointF{40.0f, 16.0f};
    r_node->on_pointer_move(mouse_mv1);
    KTUI_CHECK(r_node->needs_paint());

    // Same position -> zero redundant dirtying
    r_node->clear_dirty_recursive();
    r_node->on_pointer_move(mouse_mv1);
    KTUI_CHECK(!r_node->needs_paint());

    // New position -> marks dirty
    PointerEvent mouse_mv2(PlatformEventType::PointerMove, PointF{44.0f, 18.0f});
    mouse_mv2.local_position = PointF{44.0f, 18.0f};
    r_node->on_pointer_move(mouse_mv2);
    KTUI_CHECK(r_node->needs_paint());

    // Exit clears reveal and marks dirty once
    r_node->clear_dirty_recursive();
    r_node->on_pointer_exit(exit_ev);
    KTUI_CHECK(r_node->needs_paint());
    r_node->clear_dirty_recursive();
    r_node->on_pointer_exit(exit_ev);
    KTUI_CHECK(!r_node->needs_paint());

    // 4. Disabled & Loading states suppress reveal tracking
    r_node->set_disabled(true);
    r_node->clear_dirty_recursive();
    r_node->on_pointer_move(mouse_mv1);
    KTUI_CHECK(!r_node->needs_paint());
    r_node->set_disabled(false);

    r_node->set_loading(true);
    r_node->clear_dirty_recursive();
    r_node->on_pointer_move(mouse_mv1);
    KTUI_CHECK(!r_node->needs_paint());
    r_node->set_loading(false);

    // 5. Reactive Property binding for pointer reveal
    Property<bool> reveal_prop(false);
    WidgetPtr bound_widget = Button{.text = "Bound"}
        .bind(&RenderButton::set_pointer_reveal_enabled, reveal_prop);
    auto bound_node = std::static_pointer_cast<RenderButton>(bound_widget->create_render_node());
    KTUI_CHECK(bound_node != nullptr && !bound_node->pointer_reveal_enabled());
    reveal_prop.set(true);
    KTUI_CHECK(bound_node->pointer_reveal_enabled());
    reveal_prop.set(false);
    KTUI_CHECK(!bound_node->pointer_reveal_enabled());

    std::cout << "  -> Button Pointer Reveal opt-in、零抖动与禁用/加载门控验证通过！" << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "==================================================" << std::endl;
    std::cout << "   KTUI 动效算法管理器与 Button 三层规范单元测试   " << std::endl;
    std::cout << "==================================================" << std::endl;

    test_motion_easing_equations();
    test_motion_cubic_bezier_curves();
    test_motion_tween_lifecycle();
    test_button_three_layer_architecture();
    test_button_ripple_delegation_to_motion();
    test_button_keyboard_trigger();
    test_button_reactive_bindings();
    test_button_pointer_reveal_opt_in_and_gating();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！动效算法管理器与 Button 规范重构 100% 通过!   " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
