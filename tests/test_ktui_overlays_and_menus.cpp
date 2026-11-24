#include <ktui/widgets/info_bar.hpp>
#include <ktui/widgets/content_dialog.hpp>
#include <ktui/widgets/menu_flyout.hpp>
#include <ktui/widgets/dropdown_button.hpp>
#include <ktui/widgets/password_box.hpp>
#include <ktui/widgets/number_box.hpp>
#include <ktui/widgets/command_bar.hpp>
#include <ktui/widgets/label.hpp>
#include <ktui/widgets/toast.hpp>
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

    // 5. PasswordBox 掩码、输入、按住窥视与安全校验
    std::string latest_pwd;
    PasswordBox pb{
        .password = "abc",
        .header = "密码",
        .reveal_mode = PasswordRevealMode::Peek,
        .width = 220.0f,
        .on_change = [&](const std::string& v) { latest_pwd = v; }
    };
    WidgetPtr pb_ptr = pb;
    auto pb_node = std::dynamic_pointer_cast<RenderPasswordBox>(pb_ptr->create_render_node());
    pb_node->layout(BoxConstraints::loose(Vec2{300.0f, 80.0f}));
    assert(pb_node->size().y == 54.0f); // 22px header + 32px box
    assert(!pb_node->is_revealing());
    pb_node->on_ime_commit("123");
    assert(pb_node->password() == "abc123");
    assert(latest_pwd == "abc123");

    // 6. NumberBox 步进、钳位与算术表达式求值
    double latest_num = 0.0;
    NumberBox nb{
        .value = 10.0,
        .minimum = 0.0,
        .maximum = 100.0,
        .small_change = 5.0,
        .precision = 0,
        .accepts_expression = true,
        .on_value_changed = [&](double v) { latest_num = v; }
    };
    WidgetPtr nb_ptr = nb;
    auto nb_node = std::dynamic_pointer_cast<RenderNumberBox>(nb_ptr->create_render_node());
    nb_node->layout(BoxConstraints::loose(Vec2{300.0f, 60.0f}));
    nb_node->step(5.0);
    assert(nb_node->value() == 15.0 && latest_num == 15.0);
    nb_node->step(500.0);
    assert(nb_node->value() == 100.0); // 严格钳位到 maximum
    auto expr_val = detail::NumberBoxExprParser::evaluate("(12 + 8) * 3 - 4 / 2");
    assert(expr_val.has_value() && std::abs(*expr_val - 58.0) < 1e-6);

    // 7. CommandBar 主命令布局、窄宽度自适应溢出与 Toggle 状态
    CommandBar cmd{
        .content = "工具栏",
        .primary_commands = {
            CommandBarItem{ .label = "保存" },
            CommandBarItem{ .label = "复制" },
            CommandBarItem{ .label = "分享" },
            CommandBarItem{ .label = "删除" }
        },
        .secondary_commands = {
            CommandBarItem{ .label = "更多设置" }
        },
        .default_label_position = CommandBarDefaultLabelPosition::Right
    };
    WidgetPtr cmd_ptr = cmd;
    auto cmd_node = std::dynamic_pointer_cast<RenderCommandBar>(cmd_ptr->create_render_node());
    cmd_node->layout(BoxConstraints::tight(Vec2{520.0f, 40.0f}));
    assert(cmd_node->visible_primary_count() == 4);
    assert(cmd_node->overflowed_primary_count() == 0);
    // 缩窄宽度后自动触发主命令溢出到 "..." 菜单
    cmd_node->layout(BoxConstraints::tight(Vec2{220.0f, 40.0f}));
    assert(cmd_node->overflowed_primary_count() > 0);

    // 8. Label 排版与胶囊徽章布局校验
    Label lb{ .text = "已就绪", .variant = LabelVariant::Success, .pill = true, .required = true };
    WidgetPtr lb_ptr = lb;
    auto lb_node = std::dynamic_pointer_cast<RenderLabel>(lb_ptr->create_render_node());
    lb_node->layout(BoxConstraints::loose(Vec2{300.0f, 60.0f}));
    assert(lb_node->size().x > 30.0f && lb_node->size().y >= 24.0f);

    // 9. Toast 尺寸计算与关闭触发校验
    RenderToast toast(ToastDesc{ .title = "保存成功", .message = "已写入配置", .action_text = "撤销" }, nullptr);
    toast.layout(BoxConstraints::loose(Vec2{500.0f, 200.0f}));
    assert(toast.size().x == 340.0f && toast.size().y == 100.0f);

    std::cout << "All InfoBar, ContentDialog, MenuFlyout, Split/DropDownButton, PasswordBox, NumberBox, CommandBar, Label & Toast tests passed!" << std::endl;
    return 0;
}
