#include <ktui/widgets/info_bar.hpp>
#include <ktui/widgets/content_dialog.hpp>
#include <ktui/widgets/menu_flyout.hpp>
#include <ktui/widgets/dropdown_button.hpp>
#include <ktui/theme/theme.hpp>
#include <cassert>
#include <iostream>

using namespace ktui;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " KTUI InfoBar, Dialog & MenuFlyout Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. InfoBar 开关与尺寸测试
    Property<bool> ib_open{true};
    Property<InfoBarSeverity> ib_sev{InfoBarSeverity::Success};
    InfoBar ib{
        .title = "完成",
        .message = "测试消息",
        .severity = Bind(ib_sev),
        .is_open = Bind(ib_open)
    };
    WidgetPtr ib_ptr = ib;
    auto ib_node = std::dynamic_pointer_cast<RenderInfoBar>(ib_ptr->create_render_node());
    assert(ib_node != nullptr);
    ib_node->layout(BoxConstraints::loose(Vec2{500.0f, 100.0f}));
    assert(ib_node->is_open());
    assert(ib_node->size().y == 48.0f);

    ib_open.set(false);
    ib_node->layout(BoxConstraints::loose(Vec2{500.0f, 100.0f}));
    assert(!ib_node->is_open());
    assert(ib_node->size().y == 0.0f);

    // 2. ContentDialog 尺寸计算测试
    ContentDialogDesc dlg_desc{
        .title = "确认操作",
        .content = "是否继续执行？",
        .primary_button_text = "确定",
        .close_button_text = "取消",
        .width = 400.0f
    };
    RenderContentDialog dlg(dlg_desc, nullptr);
    dlg.layout(BoxConstraints::loose(Vec2{800.0f, 600.0f}));
    assert(dlg.size().x == 400.0f);
    assert(dlg.size().y > 120.0f);

    // 3. MenuFlyout 布局与尺寸测试
    RenderMenuFlyout menu({
        MenuFlyoutItem{ .label = "复制", .shortcut = "Ctrl+C" },
        MenuFlyoutItem::separator(),
        MenuFlyoutItem{ .label = "删除", .shortcut = "Del" }
    }, nullptr);
    menu.layout(BoxConstraints::loose(Vec2{400.0f, 400.0f}));
    assert(menu.size().x >= 160.0f);
    assert(menu.size().y == 8.0f + 32.0f + 9.0f + 32.0f);

    // 4. DropDownButton & SplitButton 布局与点击测试
    int split_clicks = 0;
    SplitButton sb{
        .text = "保存",
        .accent = true,
        .items = { MenuFlyoutItem{ .label = "另存为" } },
        .on_click = [&]() { split_clicks++; }
    };
    WidgetPtr sb_ptr = sb;
    auto sb_node = std::dynamic_pointer_cast<RenderSplitButton>(sb_ptr->create_render_node());
    sb_node->layout(BoxConstraints::loose(Vec2{300.0f, 60.0f}));
    assert(sb_node->size().y == 32.0f);

    PointerEvent down(PlatformEventType::PointerDown, Vec2{15.0f, 16.0f}, PointerButton::Primary);
    down.local_position = Vec2{15.0f, 16.0f};
    sb_node->on_pointer_down(down);
    PointerEvent up(PlatformEventType::PointerUp, Vec2{15.0f, 16.0f}, PointerButton::Primary);
    up.local_position = Vec2{15.0f, 16.0f};
    sb_node->on_pointer_up(up);
    assert(split_clicks == 1);

    DropDownButton ddb{ .text = "操作", .items = { MenuFlyoutItem{ .label = "选项 1" } } };
    WidgetPtr ddb_ptr = ddb;
    auto ddb_node = std::dynamic_pointer_cast<RenderDropDownButton>(ddb_ptr->create_render_node());
    ddb_node->layout(BoxConstraints::loose(Vec2{300.0f, 60.0f}));
    assert(ddb_node->size().y == 32.0f);

    std::cout << "All InfoBar, ContentDialog, MenuFlyout & Split/DropDownButton tests passed!" << std::endl;
    return 0;
}
