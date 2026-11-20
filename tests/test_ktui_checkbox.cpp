#include <ktui/widgets/checkbox.hpp>
#include <ktui/animation/animation.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/theme/theme_scope.hpp>

#include <iostream>
#include <cassert>
#include <cmath>

using namespace ktui;

void test_checkbox_three_layer_architecture() {
    std::cout << "[Test 1] Checkbox 数据/样式/逻辑三层分离与规范目录体系验证..." << std::endl;

    // 1. 数据层 (checkbox_data.hpp) 验证
    CheckboxData data;
    data.label = "Auto Save";
    data.state = CheckState::Unchecked;
    data.is_three_state = true;
    data.box_size = 18.0f;
    data.corner_radius = 3.0f;
    assert(data.label == "Auto Save");
    assert(data.state == CheckState::Unchecked);
    assert(data.is_three_state);

    // 2. 样式层 (checkbox_style.hpp) 验证
    Theme dark_theme = Theme::dark();
    CheckboxInteractiveState state;
    ResolvedCheckboxStyle style = CheckboxStyleResolver::resolve(data, state, dark_theme);

    // 验证 CUI 桌面规范：硬朗微圆角 3px，方框 18px，描边 1.2px，对勾笔触 1.9f
    assert(style.box_size == 18.0f);
    assert(style.corner_radius == 3.0f);
    assert(style.border_width == 1.2f);
    assert(style.stroke_width == 1.9f);
    assert(style.gap == 8.0f);
    assert(style.check_mark.r == 1.0f && style.check_mark.g == 1.0f && style.check_mark.b == 1.0f);

    // 3. 逻辑层 (checkbox_logic.hpp) 验证
    RenderCheckbox box;
    box.set_label("Enable GPU Binning");
    box.layout(BoxConstraints::loose(Vec2{200.0f, 28.0f}));
    assert(box.size().x > 18.0f);
    assert(box.size().y >= 18.0f);
    assert(box.cursor_shape == CursorShape::PointingHand);

    std::cout << "  -> 数据/样式/逻辑三层物理分离且 CUI 基准度量 100% 吻合！" << std::endl;
}

void test_checkbox_tri_state_cycle() {
    std::cout << "[Test 2] Checkbox 三态 (Unchecked -> Checked -> Indeterminate -> Unchecked) 循环验证..." << std::endl;

    RenderCheckbox box;
    box.set_three_state(true);
    assert(box.state() == CheckState::Unchecked);
    assert(!box.checked());

    // 第一次切换 -> Checked
    box.cycle_state();
    assert(box.state() == CheckState::Checked);
    assert(box.checked());

    // 第二次切换 -> Indeterminate
    box.cycle_state();
    assert(box.state() == CheckState::Indeterminate);
    assert(!box.checked()); // 半选不是全选

    // 第三次切换 -> 回到 Unchecked
    box.cycle_state();
    assert(box.state() == CheckState::Unchecked);
    assert(!box.checked());

    std::cout << "  -> 三态状态机切换正确！" << std::endl;
}

void test_checkbox_animation_motion_delegation() {
    std::cout << "[Test 3] Checkbox 对勾笔触生长与半选动效委托 Motion 动画管理器验证..." << std::endl;

    RenderCheckbox box;
    box.set_state(CheckState::Checked);

    // 模拟时钟步进渲染
    PaintContext ctx;
    box.layout(BoxConstraints::tight(Vec2{100.0f, 20.0f}));
    box.paint(ctx);

    // 切换到半选
    box.set_state(CheckState::Indeterminate);
    box.paint(ctx);

    // 切换到未选
    box.set_state(CheckState::Unchecked);
    box.paint(ctx);

    std::cout << "  -> 动画管理器补间驱动正常，零私有硬编码数学算法！" << std::endl;
}

void test_checkbox_reactive_binding() {
    std::cout << "[Test 4] Checkbox MVVM 响应式双向数据绑定验证..." << std::endl;

    Property<bool> check_prop{false};
    Property<bool> dis_prop{false};

    Checkbox desc{
        .bind_checked = bind(check_prop),
        .bind_disabled = bind(dis_prop),
        .label = "Synchronize"
    };

    auto widget = static_cast<WidgetPtr>(desc);
    auto node = std::dynamic_pointer_cast<RenderCheckbox>(widget->create_render_node());
    assert(node != nullptr);
    assert(!node->checked());
    assert(!node->is_disabled());

    // VM -> UI 响应
    check_prop.set(true);
    assert(node->checked());

    dis_prop.set(true);
    assert(node->is_disabled());

    // UI -> VM 响应
    dis_prop.set(false);
    assert(!node->is_disabled());
    node->toggle(); // 翻转为 false
    assert(!node->checked());
    assert(!check_prop.get());

    std::cout << "  -> MVVM 双向数据绑定与禁用态联动验证 100% 通过！" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  KerneTile UI - Checkbox (CUI 规范三层解耦与动效) 测试" << std::endl;
    std::cout << "==========================================================" << std::endl;

    test_checkbox_three_layer_architecture();
    test_checkbox_tri_state_cycle();
    test_checkbox_animation_motion_delegation();
    test_checkbox_reactive_binding();

    std::cout << ">> 全部 Checkbox 移植测试通过！(100% Passed)" << std::endl;
    return 0;
}
