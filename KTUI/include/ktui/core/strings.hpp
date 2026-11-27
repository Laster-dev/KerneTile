#pragma once

#include <array>

namespace ktui {

// ============================================================================
// KTUI 轻量级框架内置本地化字符串表 (Zero-Allocation Framework i18n Strings)
// 替代控件头文件中的硬编码文案，支持内置 zh_CN / en_US 预设与运行期按需替换。
// ============================================================================
struct FrameworkStrings {
    // 通用输入与按钮
    const char* search_placeholder{"搜索..."};
    const char* no_matching_results{"无匹配结果"};
    const char* select_placeholder{"请选择..."};
    const char* select_date_placeholder{"选择日期"};
    const char* enter_password_placeholder{"输入密码..."};
    const char* enter_number_placeholder{"输入数字或表达式..."};
    const char* multiline_placeholder{"输入多行文本..."};
    const char* find_placeholder{"查找..."};
    const char* cancel{"取消"};
    const char* action{"操作"};
    const char* save{"保存"};
    const char* undo{"撤销"};
    const char* redo{"重做"};
    const char* cut{"剪切"};
    const char* copy{"复制"};
    const char* paste{"粘贴"};
    const char* del{"删除"};
    const char* select_all{"全选"};
    const char* copy_selection{"复制选区"};
    const char* copy_all_text{"复制全部文本"};
    const char* clear_selection{"清除选区"};
    const char* got_it{"知道了"};
    const char* prev_step{"上一步"};
    const char* next_step{"下一步"};
    const char* finish{"完成"};

    // 导航视图
    const char* nav_search_placeholder{"搜索控件"};
    const char* nav_settings_label{"设置"};

    // 日历日期格式
    const char* calendar_year_month_fmt{"%d年%d月"};
    const char* calendar_year_fmt{"%d年"};
    const char* calendar_month_fmt{"%d月"};
    std::array<const char*, 7> weekdays{"日", "一", "二", "三", "四", "五", "六"};

    // DockPanel 停靠面板菜单
    const char* dock_back_to_workspace{"停靠回主工作区"};
    const char* dock_left{"停靠到左侧 (Left)"};
    const char* dock_right{"停靠到右侧 (Right)"};
    const char* dock_top{"停靠到顶部 (Top)"};
    const char* dock_bottom{"停靠到底部 (Bottom)"};
    const char* dock_center{"停靠到中心 (Center)"};
    const char* dock_float_window{"浮动为独立窗口"};
    const char* dock_restore_window{"还原窗口"};
    const char* dock_maximize_window{"最大化窗口"};
    const char* dock_close_and_redock{"关闭并停靠回工作区"};
    const char* dock_close_pane{"关闭面板"};
    const char* dock_release_to_float_suffix{"  ↗ 释放弹出独立窗口"};

    // 专业控件默认文案
    const char* ghost_placeholder{"输入命令或语句，按 Tab 逐词接受幽灵补全..."};
    const char* ghost_tab_badge{"Tab 补词"};
    const char* hex_viewer_hint{"点击或拖动选择字节实时解码 (支持 0-9 / A-F 编辑)"};
    const char* hold_confirm_default{"长按 1.5s 确认删除"};
    const char* hold_confirm_holding{"继续按住确认..."};
    const char* hold_confirm_done{"已执行"};
    const char* query_filter_placeholder{"输入 status: 或点击选择筛选条件..."};
    const char* query_filter_add_more{"继续添加筛选..."};
    const char* query_filter_pills_mode{"胶囊"};
    const char* spotlight_action_hint{"可直接点击高亮区域内的真实按钮继续"};
    const char* syntax_filter_placeholder{"输入过滤表达式，如 ip == 10.0.0.1 && port > 1024 ..."};
    const char* syntax_filter_tab_badge{"Tab 补全"};
    const char* syntax_filter_ready{"就绪：输入字段与比较表达式"};
    const char* syntax_filter_err_paren{"语法错误：括号不匹配或存在非法字符"};
    const char* syntax_filter_err_expr{"语法错误：期望格式 <field> <op> <value> (支持 && / ||)"};
    const char* syntax_filter_warn_prefix{"警告：未注册字段 \""};
    const char* syntax_filter_warn_suffix{"\"（语法合法）"};
    const char* syntax_filter_valid{"✓ 语法合法，表达式可直接应用"};

    // Terminal 终端菜单与状态文案
    const char* term_select_all_buffer{"全选缓冲区"};
    const char* term_clear_buffer{"清空缓冲区"};
    const char* term_cursor_style{"光标样式"};
    const char* term_cursor_bar{"竖线 (Bar |)"};
    const char* term_cursor_filled_box{"实心方块 (FilledBox)"};
    const char* term_cursor_vintage{"下划线 (Vintage _)"};
    const char* term_cursor_empty_box{"空心方块 (EmptyBox)"};
    const char* term_cursor_double_underscore{"双下划线 (DoubleUnderscore)"};
    const char* term_color_scheme{"配色方案"};
    const char* term_scheme_campbell{"Campbell (WinTerminal 默认)"};
    const char* term_zoom_in{"放大字号"};
    const char* term_zoom_out{"缩小字号"};
    const char* term_zoom_reset{"重置缩放"};
    const char* term_conpty_unsupported{"\x1b[31m[ConPTY] 当前 Windows 版本不支持 CreatePseudoConsole (需 Win10 1809+)\x1b[0m\r\n"};
    const char* term_conpty_create_failed{"\x1b[31m[ConPTY] CreatePseudoConsole 失败\x1b[0m\r\n"};
    const char* term_conpty_spawn_failed_prefix{"\x1b[31m[ConPTY] 启动子进程失败: "};

    [[nodiscard]] static constexpr FrameworkStrings zh_CN() noexcept {
        return FrameworkStrings{};
    }

    [[nodiscard]] static constexpr FrameworkStrings en_US() noexcept {
        FrameworkStrings s{};
        s.search_placeholder = "Search...";
        s.no_matching_results = "No results found";
        s.select_placeholder = "Select...";
        s.select_date_placeholder = "Select a date";
        s.enter_password_placeholder = "Password...";
        s.enter_number_placeholder = "Enter number or expression...";
        s.multiline_placeholder = "Enter multiline text...";
        s.find_placeholder = "Find...";
        s.cancel = "Cancel";
        s.action = "Action";
        s.save = "Save";
        s.undo = "Undo";
        s.redo = "Redo";
        s.cut = "Cut";
        s.copy = "Copy";
        s.paste = "Paste";
        s.del = "Delete";
        s.select_all = "Select All";
        s.copy_selection = "Copy Selection";
        s.copy_all_text = "Copy All Text";
        s.clear_selection = "Clear Selection";
        s.got_it = "Got it";
        s.prev_step = "Previous";
        s.next_step = "Next";
        s.finish = "Finish";
        s.nav_search_placeholder = "Search controls";
        s.nav_settings_label = "Settings";
        s.calendar_year_month_fmt = "%d / %02d";
        s.calendar_year_fmt = "%d";
        s.calendar_month_fmt = "M%d";
        s.weekdays = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        s.dock_back_to_workspace = "Dock to Workspace";
        s.dock_left = "Dock Left";
        s.dock_right = "Dock Right";
        s.dock_top = "Dock Top";
        s.dock_bottom = "Dock Bottom";
        s.dock_center = "Dock Center";
        s.dock_float_window = "Float as Window";
        s.dock_restore_window = "Restore Window";
        s.dock_maximize_window = "Maximize Window";
        s.dock_close_and_redock = "Close & Redock";
        s.dock_close_pane = "Close Pane";
        s.dock_release_to_float_suffix = "  ↗ Release to float";
        s.ghost_placeholder = "Type a command, press Tab to accept ghost completion...";
        s.ghost_tab_badge = "Tab";
        s.hex_viewer_hint = "Click or drag bytes to inspect (0-9 / A-F to edit)";
        s.hold_confirm_default = "Hold 1.5s to confirm";
        s.hold_confirm_holding = "Keep holding...";
        s.hold_confirm_done = "Confirmed";
        s.query_filter_placeholder = "Type status: or pick a filter...";
        s.query_filter_add_more = "Add another filter...";
        s.query_filter_pills_mode = "Pills";
        s.spotlight_action_hint = "Click the highlighted control directly to continue";
        s.syntax_filter_placeholder = "Enter filter expression, e.g. ip == 10.0.0.1 && port > 1024 ...";
        s.syntax_filter_tab_badge = "Tab";
        s.syntax_filter_ready = "Ready: enter field and comparison expression";
        s.syntax_filter_err_paren = "Syntax error: unmatched parentheses or invalid character";
        s.syntax_filter_err_expr = "Syntax error: expected <field> <op> <value> (supports && / ||)";
        s.syntax_filter_warn_prefix = "Warning: unknown field \"";
        s.syntax_filter_warn_suffix = "\" (valid syntax)";
        s.syntax_filter_valid = "✓ Valid syntax";
        s.term_select_all_buffer = "Select All Buffer";
        s.term_clear_buffer = "Clear Buffer";
        s.term_cursor_style = "Cursor Style";
        s.term_cursor_bar = "Bar (|)";
        s.term_cursor_filled_box = "Filled Box";
        s.term_cursor_vintage = "Vintage (_)";
        s.term_cursor_empty_box = "Empty Box";
        s.term_cursor_double_underscore = "Double Underscore";
        s.term_color_scheme = "Color Scheme";
        s.term_scheme_campbell = "Campbell (Default)";
        s.term_zoom_in = "Zoom In";
        s.term_zoom_out = "Zoom Out";
        s.term_zoom_reset = "Reset Zoom";
        s.term_conpty_unsupported = "\x1b[31m[ConPTY] CreatePseudoConsole requires Windows 10 1809+\x1b[0m\r\n";
        s.term_conpty_create_failed = "\x1b[31m[ConPTY] CreatePseudoConsole failed\x1b[0m\r\n";
        s.term_conpty_spawn_failed_prefix = "\x1b[31m[ConPTY] Failed to launch process: ";
        return s;
    }

    [[nodiscard]] static const FrameworkStrings& current() noexcept {
        return storage();
    }

    static void set_current(const FrameworkStrings& strings) noexcept {
        storage() = strings;
    }

private:
    static FrameworkStrings& storage() noexcept {
        static FrameworkStrings s_current = zh_CN();
        return s_current;
    }
};

} // namespace ktui
