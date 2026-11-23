#include <ktui/widgets/combobox.hpp>
#include <ktui/theme/theme.hpp>
#include <cassert>
#include <iostream>
#include <string>

using namespace ktui;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " KTUI WinUI 3 ComboBox Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. 样式 Token 解析测试 (深色 & 浅色 WinUI 3 规范)
    ComboBoxData data;
    data.items = {"Direct3D 12", "Vulkan", "CPU"};
    data.selected_index = 0;

    ComboBoxInteractiveState state;
    ResolvedComboBoxStyle dark_style = ComboBoxStyleResolver::resolve(data, state, Theme::dark());
    assert(dark_style.height == 32.0f);
    assert(dark_style.corner_radius == 4.0f);
    assert(dark_style.popup_corner_radius == 8.0f);
    assert(dark_style.accent_pill.b > 0.9f); // #60CDFF

    ResolvedComboBoxStyle light_style = ComboBoxStyleResolver::resolve(data, state, Theme::light());
    assert(light_style.popup_bg.r > 0.9f);
    assert(light_style.accent_pill.b > 0.6f); // #005FB8

    // 2. 渲染节点布局、禁用项跳过、键盘上下导航测试
    RenderComboBox cb;
    cb.set_items({"Alpha", ComboBoxItem{"Beta (Disabled)", true}, "Gamma"});
    cb.layout(BoxConstraints::loose(Vec2{400.0f, 100.0f}));
    assert(cb.size().x == 180.0f);
    assert(cb.size().y == 32.0f);

    int last_idx = -1;
    std::string last_label;
    cb.set_on_change([&](int idx, const std::string& label) {
        last_idx = idx;
        last_label = label;
    });

    cb.set_selected_index(0, true);
    assert(cb.selected_index() == 0);
    assert(cb.selected_label() == "Alpha");
    assert(last_idx == 0 && last_label == "Alpha");

    // 尝试选中禁用项应被忽略
    cb.set_selected_index(1, true);
    assert(cb.selected_index() == 0);

    // 键盘向下键应自动跳过禁用项 "Beta" 选中 "Gamma"
    KeyEvent down_key(PlatformEventType::KeyDown, 40, "", 0);
    cb.on_key(down_key);
    assert(cb.selected_index() == 2);
    assert(cb.selected_label() == "Gamma");
    assert(last_idx == 2 && last_label == "Gamma");

    // 展开/收起状态切换 (Headless 无窗口下安全工作)
    cb.open_popup();
    assert(cb.is_open());
    cb.close_popup();
    assert(!cb.is_open());

    // 3. C++20 声明式 DSL 与双向绑定测试
    Property<int> bound_idx{1};
    ComboBox dsl{
        .items = {"One", "Two", "Three"},
        .selected_index = Bind(bound_idx),
        .width = 210.0f
    };
    WidgetPtr ptr = dsl;
    auto node = std::dynamic_pointer_cast<RenderComboBox>(ptr->create_render_node());
    assert(node != nullptr);
    assert(node->selected_index() == 1);
    assert(node->selected_label() == "Two");

    bound_idx.set(2);
    assert(node->selected_index() == 2);
    assert(node->selected_label() == "Three");

    node->set_selected_index(0, true);
    assert(bound_idx.get() == 0);

    std::cout << "All ComboBox tests passed successfully!" << std::endl;
    return 0;
}
