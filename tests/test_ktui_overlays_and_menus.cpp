#include <ktui/widgets/box.hpp>
#include <ktui/widgets/info_bar.hpp>
#include <ktui/widgets/content_dialog.hpp>
#include <ktui/widgets/menu_flyout.hpp>
#include <ktui/widgets/dropdown_button.hpp>
#include <ktui/widgets/password_box.hpp>
#include <ktui/widgets/number_box.hpp>
#include <ktui/widgets/command_bar.hpp>
#include <ktui/widgets/label.hpp>
#include <ktui/widgets/toast.hpp>
#include <ktui/widgets/expander.hpp>
#include <ktui/widgets/breadcrumb_bar.hpp>
#include <ktui/widgets/tab_view.hpp>
#include <ktui/widgets/auto_suggest_box.hpp>
#include <ktui/widgets/tree_view.hpp>
#include <ktui/widgets/calendar_date_picker.hpp>
#include <ktui/widgets/color_picker.hpp>
#include <ktui/widgets/rating_control.hpp>
#include <ktui/widgets/teaching_tip.hpp>
#include <ktui/widgets/virtual_list.hpp>
#include <ktui/widgets/data_grid.hpp>
#include <ktui/widgets/time_picker.hpp>
#include <ktui/widgets/info_badge.hpp>
#include <ktui/widgets/person_picture.hpp>
#include <ktui/widgets/text_area.hpp>
#include <ktui/widgets/pips_pager.hpp>
#include <ktui/widgets/flip_view.hpp>
#include <ktui/widgets/segmented_control.hpp>
#include <ktui/widgets/toggle_button.hpp>
#include <ktui/widgets/tooltip.hpp>
#include <ktui/event/focus_tree.hpp>
#include <ktui/animation/frame_clock.hpp>
#include <ktui/tree/popup_handle.hpp>
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
    WidgetPtr ib_ptr = InfoBar{
        .title = "完成",
        .message = "测试消息"
    }.bind(&RenderInfoBar::set_severity, ib_sev)
     .bind_two_way(ib_open);
    auto ib_node = std::static_pointer_cast<RenderInfoBar>(ib_ptr->create_render_node());
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
    auto sb_node = std::static_pointer_cast<RenderSplitButton>(sb_ptr->create_render_node());
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
    auto ddb_node = std::static_pointer_cast<RenderDropDownButton>(ddb_ptr->create_render_node());
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
    auto pb_node = std::static_pointer_cast<RenderPasswordBox>(pb_ptr->create_render_node());
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
    auto nb_node = std::static_pointer_cast<RenderNumberBox>(nb_ptr->create_render_node());
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
    auto cmd_node = std::static_pointer_cast<RenderCommandBar>(cmd_ptr->create_render_node());
    cmd_node->layout(BoxConstraints::tight(Vec2{520.0f, 40.0f}));
    assert(cmd_node->visible_primary_count() == 4);
    assert(cmd_node->overflowed_primary_count() == 0);
    // 缩窄宽度后自动触发主命令溢出到 "..." 菜单
    cmd_node->layout(BoxConstraints::tight(Vec2{220.0f, 40.0f}));
    assert(cmd_node->overflowed_primary_count() > 0);

    // 8. Label 排版与胶囊徽章布局校验
    Label lb{ .text = "已就绪", .variant = LabelVariant::Success, .pill = true, .required = true };
    WidgetPtr lb_ptr = lb;
    auto lb_node = std::static_pointer_cast<RenderLabel>(lb_ptr->create_render_node());
    lb_node->layout(BoxConstraints::loose(Vec2{300.0f, 60.0f}));
    assert(lb_node->size().x > 30.0f && lb_node->size().y >= 24.0f);

    // 9. Toast 尺寸计算与关闭触发校验
    RenderToast toast(ToastDesc{ .title = "保存成功", .message = "已写入配置", .action_text = "撤销" }, nullptr);
    toast.layout(BoxConstraints::loose(Vec2{500.0f, 200.0f}));
    assert(toast.size().x == 340.0f && toast.size().y == 100.0f);

    // 10. Expander 折叠/展开状态与高度变化校验
    bool exp_state = false;
    Expander ex{
        .header = "高级设置",
        .is_expanded = false,
        .content = Box{ .height = 60.0f },
        .on_expanded_changed = [&](bool v) { exp_state = v; }
    };
    WidgetPtr ex_ptr = ex;
    auto ex_node = std::static_pointer_cast<RenderExpander>(ex_ptr->create_render_node());
    ex_node->layout(BoxConstraints::loose(Vec2{400.0f, 400.0f}));
    assert(!ex_node->is_expanded() && ex_node->size().y == 48.0f);
    ex_node->toggle();
    assert(ex_node->is_expanded() && exp_state == true);

    // 11. BreadcrumbBar 宽屏完整显示与窄屏自动折叠 "..." 校验
    BreadcrumbBar bb{
        .items = std::vector<BreadcrumbItem>{
            BreadcrumbItem{ .id = "1", .label = "此电脑" },
            BreadcrumbItem{ .id = "2", .label = "KerneTile" },
            BreadcrumbItem{ .id = "3", .label = "KTUI" },
            BreadcrumbItem{ .id = "4", .label = "widgets" },
            BreadcrumbItem{ .id = "5", .label = "breadcrumb_bar.hpp" }
        }
    };
    WidgetPtr bb_ptr = bb;
    auto bb_node = std::static_pointer_cast<RenderBreadcrumbBar>(bb_ptr->create_render_node());
    bb_node->layout(BoxConstraints::loose(Vec2{800.0f, 60.0f}));
    assert(bb_node->collapsed_count() == 0);
    bb_node->layout(BoxConstraints::loose(Vec2{200.0f, 60.0f}));
    assert(bb_node->collapsed_count() > 0);

    // 12. TabView 多标签切换校验
    TabView tv{
        .tabs = std::vector<TabViewItem>{
            TabViewItem{ .id = "t1", .header = "Tab 1" },
            TabViewItem{ .id = "t2", .header = "Tab 2" }
        },
        .selected_index = 0,
        .height = 240.0f
    };
    WidgetPtr tv_ptr = tv;
    auto tv_node = std::static_pointer_cast<RenderTabView>(tv_ptr->create_render_node());
    tv_node->layout(BoxConstraints::tight(Vec2{500.0f, 240.0f}));
    assert(tv_node->tab_count() == 2 && tv_node->selected_index() == 0);
    tv_node->set_selected_index_value(1);
    assert(tv_node->selected_index() == 1);

    // 13. AutoSuggestBox 实时过滤与建议选中校验
    std::string chosen_item;
    AutoSuggestBox asb{
        .items = std::vector<std::string>{"AutoSuggestBox", "Button", "BreadcrumbBar", "TabView"},
        .on_suggestion_chosen = [&](const std::string& s) { chosen_item = s; }
    };
    WidgetPtr asb_ptr = asb;
    auto asb_node = std::static_pointer_cast<RenderAutoSuggestBox>(asb_ptr->create_render_node());
    asb_node->layout(BoxConstraints::loose(Vec2{320.0f, 60.0f}));
    assert(asb_node->filtered_items().size() == 4);
    asb_node->set_text_value("bar");
    assert(asb_node->filtered_items().size() == 1 && asb_node->filtered_items()[0] == "BreadcrumbBar");
    asb_node->select_suggestion(0);
    assert(chosen_item == "BreadcrumbBar" && asb_node->text() == "BreadcrumbBar");

    // 14. TreeView 展开/折叠与父子三态复选框联动校验
    TreeView tree{
        .nodes = {
            TreeViewNode{
                .id = "root", .label = "Root", .is_expanded = false,
                .children = {
                    TreeViewNode{ .id = "c1", .label = "Child 1" },
                    TreeViewNode{ .id = "c2", .label = "Child 2" }
                }
            }
        },
        .selection_mode = TreeViewSelectionMode::Multiple,
        .height = 180.0f
    };
    WidgetPtr tree_ptr = tree;
    auto tree_node = std::static_pointer_cast<RenderTreeView>(tree_ptr->create_render_node());
    tree_node->layout(BoxConstraints::loose(Vec2{300.0f, 200.0f}));
    assert(tree_node->visible_row_count() == 1);
    assert(tree_node->toggle_expand_by_id("root"));
    assert(tree_node->visible_row_count() == 3);
    assert(tree_node->toggle_check_by_id("root"));
    assert(tree_node->checked_ids().size() == 3);

    // 15. CalendarDatePicker 日历算法与日期范围约束校验
    assert(CalendarDate::days_in_month(2024, 2) == 29);
    assert(CalendarDate::days_in_month(2026, 2) == 28);
    assert(CalendarDate::day_of_week(2026, 11, 24) == 2); // 2026-11-24 is Tuesday
    CalendarDatePicker cdp{
        .date = CalendarDate{2026, 11, 24},
        .min_date = CalendarDate{2026, 11, 1},
        .max_date = CalendarDate{2026, 11, 30}
    };
    WidgetPtr cdp_ptr = cdp;
    auto cdp_node = std::static_pointer_cast<RenderCalendarDatePicker>(cdp_ptr->create_render_node());
    cdp_node->layout(BoxConstraints::loose(Vec2{260.0f, 60.0f}));
    assert(cdp_node->date().format_iso() == "2026-11-24");
    assert(cdp_node->is_date_in_range(CalendarDate{2026, 11, 15}));
    assert(!cdp_node->is_date_in_range(CalendarDate{2026, 12, 1}));

    // 16. Fluent 2 语义令牌校验 (Dark / Light)
    Theme dark_t = Theme::dark();
    Theme light_t = Theme::light();
    assert(dark_t.colors.flyout_surface.a == 1.0f && light_t.colors.flyout_surface.a == 1.0f);
    assert(dark_t.colors.control_fill_default != light_t.colors.control_fill_default);

    // 17. FrameClock 确定性虚拟时间步进校验
    auto& clock = FrameClock::global();
    clock.enable_virtual_time(10.0);
    FluentPopupSurface surf;
    clock.advance_virtual(0.065f);
    auto st1 = surf.tick(0.13f);
    assert(st1.animating && st1.ease > 0.5f && st1.ease < 1.0f);
    clock.advance_virtual(0.10f);
    auto st2 = surf.tick(0.13f);
    assert(!st2.animating && st2.alpha == 1.0f);
    clock.disable_virtual_time();

    // 18. RenderNode 挂载/卸载生命周期钩子校验
    struct LifecycleProbe : public RenderBox {
        int mounted{0};
        int unmounted{0};
        void perform_layout() override { set_size(Vec2{10.0f, 10.0f}); }
        void on_mounted() override { ++mounted; }
        void on_unmounted() override { ++unmounted; }
    };
    {
        struct DummyBox : public RenderBox { void perform_layout() override { set_size(Vec2{100.0f, 100.0f}); } };
        DummyBox parent_b;
        LifecycleProbe probe;
        assert(!probe.is_mounted());
        parent_b.append_child(&probe);
        assert(probe.is_mounted() && probe.mounted == 1 && probe.unmounted == 0);
        parent_b.remove_child(&probe);
        assert(!probe.is_mounted() && probe.unmounted == 1);
    }

    // 19. VirtualList 100,000 行 O(1) 视口虚拟化裁剪校验
    VirtualList vlist{
        .item_count = 100000,
        .item_height = 40.0f,
        .height = 400.0f,
        .overscan = 2,
        .item_builder = [](size_t) -> WidgetPtr { return Box{ .height = 40.0f }; }
    };
    WidgetPtr vl_ptr = vlist;
    auto vl_node = std::static_pointer_cast<RenderVirtualList>(vl_ptr->create_render_node());
    vl_node->layout(BoxConstraints::tight(Vec2{400.0f, 400.0f}));
    assert(vl_node->item_count() == 100000);
    assert(vl_node->rendered_item_count() > 0 && vl_node->rendered_item_count() <= 20);
    vl_node->scroll_to(200000.0f); // 滚至第 5000 行附近
    assert(vl_node->first_visible_index() >= 4995 && vl_node->first_visible_index() <= 5000);
    assert(vl_node->rendered_item_count() <= 20);

    // 20. ColorPicker HSV <-> RGB 转换与 Hex 格式化校验
    float cr, cg, cb, ch, cs, cv;
    RenderColorPicker::hsv_to_rgb(120.0f, 1.0f, 1.0f, cr, cg, cb);
    assert(std::abs(cr) < 1e-4f && std::abs(cg - 1.0f) < 1e-4f && std::abs(cb) < 1e-4f);
    RenderColorPicker::rgb_to_hsv(cr, cg, cb, ch, cs, cv);
    assert(std::abs(ch - 120.0f) < 0.1f && std::abs(cs - 1.0f) < 1e-4f && std::abs(cv - 1.0f) < 1e-4f);
    assert(RenderColorPicker::format_hex(Color{0.0f, 1.0f, 0.0f, 1.0f}, false) == "#00FF00");

    // 21. RatingControl 评分、半星与二次点击清零校验
    float rating_out = 0.0f;
    RatingControl rc{
        .value = 3.0f,
        .max_rating = 5,
        .is_clear_enabled = true,
        .allow_half_stars = true,
        .on_value_changed = [&](float v) { rating_out = v; }
    };
    WidgetPtr rc_ptr = rc;
    auto rc_node = std::static_pointer_cast<RenderRatingControl>(rc_ptr->create_render_node());
    rc_node->layout(BoxConstraints::loose(Vec2{300.0f, 40.0f}));
    rc_node->apply_rating_click(4.5f);
    assert(std::abs(rc_node->value() - 4.5f) < 0.001f && std::abs(rating_out - 4.5f) < 0.001f);
    rc_node->apply_rating_click(4.5f); // 二次点击清零
    assert(std::abs(rc_node->value()) < 0.001f);

    // 22. TeachingTip 尺寸布局计算校验
    RenderTeachingTip tip(TeachingTipDesc{
        .title = "引导提示",
        .subtitle = "这是一段用于测试多行折行与尾标高度计算的引导说明文字。",
        .action_text = "确认",
        .close_text = "取消"
    }, true, true, 160.0f, nullptr);
    tip.layout(BoxConstraints::loose(Vec2{500.0f, 400.0f}));
    assert(tip.size().x == 320.0f && tip.size().y > 120.0f);

    // 23. DataGrid 数值列排序与状态切换校验
    DataGrid dg{
        .columns = {
            DataGridColumn{ .title = "ID", .width = 80.0f, .sortable = true, .numeric = true },
            DataGridColumn{ .title = "Name", .width = 0.0f, .sortable = true, .numeric = false }
        },
        .rows = {
            {"10", "Beta"},
            {"2", "Alpha"},
            {"30", "Gamma"}
        },
        .height = 200.0f
    };
    WidgetPtr dg_ptr = dg;
    auto dg_node = std::static_pointer_cast<RenderDataGrid>(dg_ptr->create_render_node());
    dg_node->layout(BoxConstraints::tight(Vec2{400.0f, 200.0f}));
    assert(dg_node->row_count() == 3 && dg_node->sorted_column() == -1);
    dg_node->sort_by_column(0);
    assert(dg_node->sorted_column() == 0 && dg_node->sort_direction() == DataGridSortDirection::Ascending);
    dg_node->sort_by_column(0);
    assert(dg_node->sort_direction() == DataGridSortDirection::Descending);

    // 24. TimePicker 24h/12h 格式化校验
    assert((TimeOfDay{0, 5}.format(true) == "00:05"));
    assert((TimeOfDay{0, 5}.format(false) == "12:05 AM"));
    assert((TimeOfDay{14, 30}.format(false) == "02:30 PM"));

    // 25. InfoBadge 99+ 溢出格式化校验
    InfoBadge badge{ .value = 128, .severity = InfoBadgeSeverity::Critical };
    WidgetPtr badge_ptr = badge;
    auto badge_node = std::static_pointer_cast<RenderInfoBadge>(badge_ptr->create_render_node());
    badge_node->layout(BoxConstraints::loose(Vec2{100.0f, 30.0f}));
    assert(badge_node->formatted_text() == "99+");

    // 26. PersonPicture 中英文姓名首字母提取校验
    assert(RenderPersonPicture::extract_initials("Jane Doe") == "JD");
    assert(RenderPersonPicture::extract_initials("KerneTile") == "K");
    assert(RenderPersonPicture::extract_initials("林思远") == "林");

    // 27. TextArea 多行换行统计校验
    TextArea ta{ .text = "Line 1\nLine 2\nLine 3", .height = 120.0f };
    WidgetPtr ta_ptr = ta;
    auto ta_node = std::static_pointer_cast<RenderTextArea>(ta_ptr->create_render_node());
    ta_node->layout(BoxConstraints::tight(Vec2{300.0f, 120.0f}));
    assert(ta_node->line_count() == 3);

    // 28. PipsPager 分页切换与边界夹紧校验
    PipsPager pp{ .number_of_pages = 10, .selected_index = 0 };
    WidgetPtr pp_ptr = pp;
    auto pp_node = std::static_pointer_cast<RenderPipsPager>(pp_ptr->create_render_node());
    pp_node->select_page(5);
    assert(pp_node->selected_index() == 5);
    pp_node->select_page(99);
    assert(pp_node->selected_index() == 9);

    // 29. FlipView 页面切换校验
    FlipView fv{ .items = { Box{ .height = 100.0f }, Box{ .height = 100.0f }, Box{ .height = 100.0f } }, .selected_index = 0 };
    WidgetPtr fv_ptr = fv;
    auto fv_node = std::static_pointer_cast<RenderFlipView>(fv_ptr->create_render_node());
    fv_node->go_to(2);
    assert(fv_node->item_count() == 3 && fv_node->selected_index() == 2);

    // 30. SegmentedControl 分段切换校验
    SegmentedControl sc{ .items = { SegmentedItem{ .label = "A" }, SegmentedItem{ .label = "B" } }, .selected_index = 0 };
    WidgetPtr sc_ptr = sc;
    auto sc_node = std::static_pointer_cast<RenderSegmentedControl>(sc_ptr->create_render_node());
    sc_node->select(1);
    assert(sc_node->selected_index() == 1);

    // 31. ToggleButton 状态翻转校验
    ToggleButton tb{ .checked = false, .text = "Bold" };
    WidgetPtr tb_ptr = tb;
    auto tb_node = std::static_pointer_cast<RenderToggleButton>(tb_ptr->create_render_node());
    tb_node->toggle();
    assert(tb_node->checked() == true);

    // 32. TextArea 局部选区、删除与撤销/重做校验
    ta_node->set_selection(0, 6); // "Line 1"
    assert(ta_node->has_selection() && ta_node->selected_text() == "Line 1");
    ta_node->delete_selection();
    assert(ta_node->text() == "\nLine 2\nLine 3");
    assert(ta_node->can_undo());
    ta_node->undo();
    assert(ta_node->text() == "Line 1\nLine 2\nLine 3");
    assert(ta_node->can_redo());
    ta_node->redo();
    assert(ta_node->text() == "\nLine 2\nLine 3");

    // 33. AutoSuggestBox 直接组合复用真实 RenderTextBox 子节点校验
    AutoSuggestBox asb_reuse{
        .text = "WinUI",
        .items = std::vector<std::string>{ "WinUI 3", "KerneTile", "Fluent" }
    };
    WidgetPtr asb_reuse_ptr = asb_reuse;
    auto asb_reuse_node = std::static_pointer_cast<RenderAutoSuggestBox>(asb_reuse_ptr->create_render_node());
    assert(asb_reuse_node->input_box() != nullptr);
    assert(asb_reuse_node->input_box()->text() == "WinUI");

    // 34. PipsPager 切换页码时圆点放大显示数字、1 秒后平滑缩小与淡出校验
    FrameClock::global().enable_virtual_time(10.0);
    PipsPager pp_anim{ .number_of_pages = 6, .selected_index = 0 };
    WidgetPtr pp_anim_ptr = pp_anim;
    auto pp_anim_node = std::static_pointer_cast<RenderPipsPager>(pp_anim_ptr->create_render_node());
    pp_anim_node->layout(BoxConstraints::loose(Vec2{300.0f, 40.0f}));
    pp_anim_node->select_page(2);
    assert(pp_anim_node->is_badge_active());
    for (int f = 0; f < 25; ++f) {
        FrameClock::global().advance_virtual(1.0f / 60.0f);
        pp_anim_node->step_animations();
    }
    assert(pp_anim_node->badge_alpha() > 0.85f);
    assert(pp_anim_node->active_pip_diameter() > 19.0f);
    // 推进超过 1.0 秒停留期后，数字淡出且圆点平滑缩回
    FrameClock::global().advance_virtual(0.8f);
    for (int f = 0; f < 45; ++f) {
        FrameClock::global().advance_virtual(1.0f / 60.0f);
        pp_anim_node->step_animations();
    }
    assert(!pp_anim_node->is_badge_active());
    assert(pp_anim_node->badge_alpha() < 0.05f);
    assert(pp_anim_node->active_pip_diameter() < 9.5f);
    FrameClock::global().disable_virtual_time();

    // 35. PasswordBox / NumberBox 直接组合复用真实 RenderTextBox 子节点校验
    PasswordBox pb_reuse{ .password = "secret" };
    WidgetPtr pb_reuse_ptr = pb_reuse;
    auto pb_reuse_node = std::static_pointer_cast<RenderPasswordBox>(pb_reuse_ptr->create_render_node());
    assert(pb_reuse_node->input_box() != nullptr);
    assert(pb_reuse_node->input_box()->is_password());
    assert(pb_reuse_node->input_box()->text() == "secret");

    NumberBox nb_reuse{ .value = 42.0, .minimum = 0.0, .maximum = 100.0, .small_change = 5.0 };
    WidgetPtr nb_reuse_ptr = nb_reuse;
    auto nb_reuse_node = std::static_pointer_cast<RenderNumberBox>(nb_reuse_ptr->create_render_node());
    assert(nb_reuse_node->input_box() != nullptr);
    assert(nb_reuse_node->input_box()->text() == "42");
    nb_reuse_node->step(5.0);
    assert(nb_reuse_node->value() == 47.0 && nb_reuse_node->input_box()->text() == "47");

    // 36. ToggleButton / DropDownButton / SplitButton 直接组合复用真实 RenderButton 子节点校验
    assert(tb_node->button() != nullptr);
    assert(tb_node->button()->variant() == ButtonVariant::Accent);

    DropDownButton ddb_reuse{ .text = "操作", .items = {} };
    WidgetPtr ddb_reuse_ptr = ddb_reuse;
    auto ddb_reuse_node = std::static_pointer_cast<RenderDropDownButton>(ddb_reuse_ptr->create_render_node());
    assert(ddb_reuse_node->button() != nullptr && ddb_reuse_node->button()->text() == "操作");

    SplitButton sb_reuse{ .text = "保存", .accent = true, .items = {} };
    WidgetPtr sb_reuse_ptr = sb_reuse;
    auto sb_reuse_node = std::static_pointer_cast<RenderSplitButton>(sb_reuse_ptr->create_render_node());
    assert(sb_reuse_node->main_button() != nullptr && sb_reuse_node->drop_button() != nullptr);
    assert(sb_reuse_node->main_button()->variant() == ButtonVariant::Accent);

    // 37. FontWeight (Regular / SemiBold / Bold) 排版与测量校验
    Vec2 reg_sz = TextMeasure::measure("KerneTile WinUI 3", 16.0f, FontWeight::Regular);
    Vec2 bold_sz = TextMeasure::measure("KerneTile WinUI 3", 16.0f, FontWeight::Bold);
    assert(reg_sz.x > 0.0f && bold_sz.x > 0.0f);
    assert(bold_sz.x >= reg_sz.x * 0.95f);

    // 38. Tooltip 悬浮提示尺寸计算、点击穿透与布局透传校验
    RenderTooltipPopup tip_popup("快捷键 Ctrl+S 保存");
    Vec2 tip_sz = tip_popup.compute_ideal_size();
    assert(tip_sz.x >= 32.0f && tip_sz.y >= 26.0f);
    HitTestResult tip_hit;
    assert(!tip_popup.hit_test(tip_hit, PointF{10.0f, 10.0f}));

    Tooltip tip_widget{
        .text = "操作提示",
        .child = Button { .text = "确定" }
    };
    WidgetPtr tip_ptr = tip_widget;
    auto tip_area = std::static_pointer_cast<RenderTooltipArea>(tip_ptr->create_render_node());
    tip_area->layout(BoxConstraints::loose(Vec2{200.0f, 60.0f}));
    assert(tip_area->size().x > 20.0f && tip_area->size().y == 32.0f);

    // 39. FocusManager Tab / Shift+Tab 键盘焦点流转、焦点环可见性与零尺寸节点跳过校验
    RenderDecoratedBox focus_root;
    focus_root.layout(BoxConstraints::tight(Vec2{400.0f, 200.0f}));
    RenderButton fb1, fb2, fb_hidden;
    fb1.set_text("One");
    fb2.set_text("Two");
    fb_hidden.set_text("Hidden");
    fb1.layout(BoxConstraints::tight(Vec2{80.0f, 32.0f}));
    fb2.layout(BoxConstraints::tight(Vec2{80.0f, 32.0f}));
    fb_hidden.layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}));
    focus_root.append_child(&fb1);
    focus_root.append_child(&fb_hidden);
    focus_root.append_child(&fb2);

    FocusManager fm;
    fm.request_focus(&fb1, false); // 鼠标点击不显示键盘焦点环
    assert(fm.focused_node() == &fb1 && !fm.is_keyboard_focus_visible());
    assert(fm.handle_tab_navigation(&focus_root, false)); // Tab 跳过零尺寸节点到 fb2
    assert(fm.focused_node() == &fb2 && fm.is_keyboard_focus_visible());
    assert(fm.handle_tab_navigation(&focus_root, true));  // Shift+Tab 回到 fb1
    assert(fm.focused_node() == &fb1 && fm.is_keyboard_focus_visible());

    focus_root.remove_child(&fb1);
    focus_root.remove_child(&fb_hidden);
    focus_root.remove_child(&fb2);

    // 40. Label 粗体/斜体/下划线/删除线组合校验
    Label rich_lbl{
        .text = "富文本标签",
        .variant = LabelVariant::Accent,
        .bold = true,
        .italic = true,
        .underline = true,
        .strikethrough = true
    };
    WidgetPtr rich_lbl_ptr = rich_lbl;
    auto rich_lbl_node = std::static_pointer_cast<RenderLabel>(rich_lbl_ptr->create_render_node());
    rich_lbl_node->layout(BoxConstraints::loose(Vec2{300.0f, 60.0f}));
    assert(rich_lbl_node->size().x > 20.0f && rich_lbl_node->size().y >= 16.0f);

    std::cout << "All 40 KTUI widget, overlay & architecture tests passed!" << std::endl;
    return 0;
}
