#include <ktui/widgets/slider.hpp>
#include <ktui/widgets/progress_bar.hpp>
#include <ktui/widgets/progress_ring.hpp>
#include <ktui/theme/theme.hpp>
#include <iostream>
#include <cassert>
#include <cmath>

using namespace ktui;

void test_slider() {
    std::cout << "[Test 1] Slider 规范度量、步进吸附、归一化与数据绑定测试..." << std::endl;

    SliderData data;
    data.min_value = 0.0f;
    data.max_value = 100.0f;
    data.step = 5.0f;
    data.value = 42.0f;
    assert(data.min_value == 0.0f);
    assert(data.max_value == 100.0f);
    assert(data.step == 5.0f);

    SliderInteractiveState state;
    Theme dark_theme = Theme::dark();
    Theme light_theme = Theme::light();
    ResolvedSliderStyle style = SliderStyleResolver::resolve(data, state, dark_theme, 0.0f, 0.0f);
    assert(style.track_height == 4.0f);
    assert(style.thumb_radius == 10.0f);
    assert(style.thumb_scale == 1.0f);
    // WinUI 3 Dark 外环为 #454545 深灰表面，Light 外环为 #FFFFFF 纯白表面
    assert(style.thumb_fill.r < 0.35f && style.thumb_fill.g < 0.35f && style.thumb_fill.b < 0.35f);
    ResolvedSliderStyle light_style = SliderStyleResolver::resolve(data, state, light_theme, 0.0f, 0.0f);
    assert(light_style.thumb_fill.r == 1.0f && light_style.thumb_fill.g == 1.0f && light_style.thumb_fill.b == 1.0f);

    // WinUI 3 核心微交互: 悬停内圆扩张、按下内圆收缩，外环尺寸保持恒定
    ResolvedSliderStyle hover_style = SliderStyleResolver::resolve(data, state, dark_theme, 1.0f, 0.0f);
    ResolvedSliderStyle press_style = SliderStyleResolver::resolve(data, state, dark_theme, 1.0f, 1.0f);
    assert(hover_style.thumb_scale == 1.0f && press_style.thumb_scale == 1.0f);
    assert(hover_style.core_radius > style.core_radius);
    assert(press_style.core_radius < style.core_radius);

    RenderSlider render_slider;
    render_slider.layout(BoxConstraints::tight(Vec2{200.0f, 32.0f}));
    render_slider.set_range(0.0f, 100.0f, 5.0f);
    render_slider.set_value(42.0f, false);
    // 42 应该吸附到最近的步长 40
    assert(std::abs(render_slider.value() - 40.0f) < 0.001f);
    assert(std::abs(render_slider.normalized_value() - 0.40f) < 0.001f);

    // 鼠标点击拖动测试 (基于 local_position)
    PointerEvent down_ev;
    down_ev.type = PlatformEventType::PointerDown;
    down_ev.position = Vec2{500.0f, 500.0f};       // 模拟全局绝对坐标 (不应干扰拖动)
    down_ev.local_position = Vec2{10.0f, 16.0f};    // 局部起始端 (thumb_r = 10.0)
    render_slider.on_pointer_down(down_ev);
    assert(std::abs(render_slider.value() - 0.0f) < 0.001f);

    PointerEvent move_ev;
    move_ev.type = PlatformEventType::PointerMove;
    move_ev.position = Vec2{999.0f, 999.0f};
    move_ev.local_position = Vec2{200.0f, 13.0f};   // 局部末尾端
    render_slider.on_pointer_move(move_ev);
    assert(std::abs(render_slider.value() - 100.0f) < 0.001f);

    PointerEvent up_ev;
    up_ev.type = PlatformEventType::PointerUp;
    render_slider.on_pointer_up(up_ev);
    assert(!render_slider.state().is_dragging);

    // 边界钳制测试
    render_slider.set_value(150.0f, false);
    assert(std::abs(render_slider.value() - 100.0f) < 0.001f);
    assert(std::abs(render_slider.normalized_value() - 1.0f) < 0.001f);

    render_slider.set_value(-20.0f, false);
    assert(std::abs(render_slider.value() - 0.0f) < 0.001f);
    assert(std::abs(render_slider.normalized_value() - 0.0f) < 0.001f);

    // 响应式数据绑定测试
    Property<float> prop{30.0f};
    WidgetPtr ptr = Slider{
        .min_value = 0.0f,
        .max_value = 100.0f,
        .step = 1.0f
    }.bind_two_way(prop);
    auto node = std::static_pointer_cast<RenderSlider>(ptr->create_render_node());
    assert(node != nullptr);
    assert(std::abs(node->value() - 30.0f) < 0.001f);

    prop.set(75.0f);
    assert(std::abs(node->value() - 75.0f) < 0.001f);

    std::cout << "  -> Slider 功能与双向绑定全部通过！" << std::endl;
}

void test_progress_bar() {
    std::cout << "[Test 2] ProgressBar 确定/不确定模式与约束布局测试..." << std::endl;

    ProgressBarData data;
    data.value = 0.65f;
    data.indeterminate = false;

    ProgressBarInteractiveState state;
    ResolvedProgressBarStyle style = ProgressBarStyleResolver::resolve(data, state, Theme::dark());
    assert(style.track_height == 3.0f);
    assert(style.rail_height == 1.0f);
    assert(style.corner_radius == 1.5f);
    assert(style.show_rail);

    // WinUI 3 不确定模式隐藏 1px 底线
    data.indeterminate = true;
    ResolvedProgressBarStyle ind_style = ProgressBarStyleResolver::resolve(data, state, Theme::dark());
    assert(!ind_style.show_rail);

    // WinUI 3 Paused / Error 语义色
    data.indeterminate = false;
    data.state = ProgressBarState::Error;
    ResolvedProgressBarStyle err_style = ProgressBarStyleResolver::resolve(data, state, Theme::light());
    assert(err_style.indicator_color.r > 0.7f && err_style.indicator_color.b < 0.2f);

    RenderProgressBar render_bar;
    render_bar.set_value(0.65f, false);
    assert(std::abs(render_bar.value() - 0.65f) < 0.001f);
    assert(!render_bar.indeterminate());

    // 布局与尺寸约束
    render_bar.layout(BoxConstraints::tight(Vec2{200.0f, 20.0f}));
    assert(render_bar.size().x == 200.0f);
    assert(render_bar.size().y == 20.0f);

    // 切换到不确定模式
    render_bar.set_indeterminate(true);
    assert(render_bar.indeterminate());

    std::cout << "  -> ProgressBar 确定/不确定模式全部通过！" << std::endl;
}

void test_progress_ring() {
    std::cout << "[Test 3] ProgressRing 环形尺寸、确定百分比与旋转模式测试..." << std::endl;

    ProgressRingData data;
    data.size = 32.0f;
    data.stroke_width = 3.5f;
    data.indeterminate = true;
    data.show_track = true;

    ProgressRingInteractiveState state;
    ResolvedProgressRingStyle style = ProgressRingStyleResolver::resolve(data, state, Theme::dark());
    assert(style.size == 32.0f);
    assert(style.stroke_width == 3.5f);
    assert(style.show_track);

    RenderProgressRing render_ring;
    render_ring.set_ring_size(32.0f);
    render_ring.layout(BoxConstraints::loose(Vec2{40.0f, 40.0f}));
    assert(render_ring.size().x == 32.0f);
    assert(render_ring.size().y == 32.0f);

    render_ring.set_indeterminate(false);
    render_ring.set_value(0.80f, false);
    assert(std::abs(render_ring.value() - 0.80f) < 0.001f);

    // WinUI 3 官方规范属性与模式测试
    render_ring.set_indeterminate(true);
    assert(render_ring.indeterminate());
    assert(style.indicator_color.a > 0.9f);

    std::cout << "  -> ProgressRing 功能验证全部通过！" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " KTUI Slider, ProgressBar & ProgressRing Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_slider();
    test_progress_bar();
    test_progress_ring();

    std::cout << "\nAll 3 widget tests passed successfully!" << std::endl;
    return 0;
}
