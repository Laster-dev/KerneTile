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
    KeyEvent down_key(PlatformEventType::KeyDown, 40, "", KeyModifiers{});
    cb.on_key(down_key);
    assert(cb.selected_index() == 2);
    assert(cb.selected_label() == "Gamma");
    assert(last_idx == 2 && last_label == "Gamma");

    // 展开/收起状态切换 (Headless 无窗口下安全工作)
    cb.open_popup();
    assert(cb.is_open());
    cb.close_popup();
    assert(!cb.is_open());

    // 3. Chromium 原版 ScrollOffsetAnimationCurve (Impulse) 测试 (单格 100px, 200ms~500ms)
    SmoothScrollPhysics physics;
    physics.set_max_scroll(400.0f);
    physics.add_wheel_delta(36.0f); // PAL 向下滚 1 格 (+36) -> Chromium 100.0px
    assert(physics.is_animating());
    assert(std::abs(physics.target() - 100.0f) < 0.05f);
    physics.step_for_test(0.08f);
    float mid_pos = physics.offset();
    (void)mid_pos;
    assert(mid_pos > 0.0f && mid_pos < 100.0f);
    physics.step_for_test(0.25f); // 200ms 结束后精准收敛至 100.0px
    assert(!physics.is_animating());
    assert(std::abs(physics.offset() - 100.0f) < 0.05f);

    // 4. WinUI 3 滚动条度量与拖拽测试 (12px 槽位，收起 2px -> 展开 6px，最小高 24px)
    WinUIScrollBar sb;
    Rect vp = Rect::from_xywh(0.0f, 0.0f, 200.0f, 200.0f);
    auto metrics = sb.compute_metrics(vp, 600.0f, 0.0f);
    assert(metrics.visible);
    assert(metrics.track_rect.width() == 12.0f);
    assert(std::abs(metrics.thumb_rect.width() - 2.0f) < 0.01f); // 静止收起态 2px
    assert(metrics.thumb_rect.height() >= 24.0f);

    bool consumed = false;
    PointF thumb_pt{metrics.track_rect.min_x + 6.0f, metrics.thumb_rect.min_y + 10.0f};
    assert(sb.handle_pointer_down(thumb_pt, vp, 600.0f, physics, 32.0f));
    assert(sb.is_dragging());
    sb.handle_pointer_move(PointF{thumb_pt.x, thumb_pt.y + 40.0f}, vp, 600.0f, physics, consumed);
    assert(consumed);
    assert(physics.offset() > 96.0f);
    assert(sb.handle_pointer_up());
    assert(!sb.is_dragging());

    // 5. C++20 声明式 DSL 与双向绑定测试
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
