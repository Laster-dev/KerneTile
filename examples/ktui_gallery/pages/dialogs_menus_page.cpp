#include "gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

namespace {
constexpr const char* kCopySvg =
    "M5.5 4.625V16.5a2.5 2.5 0 0 0 2.5 2.5h7.375a2.25 2.25 0 0 1-2.125 1.5H8a4 4 0 0 1-4-4V6.75A2.25 2.25 0 0 1 5.5 4.625ZM9.25 2A2.25 2.25 0 0 0 7 4.25v11.5A2.25 2.25 0 0 0 9.25 18h7.5A2.25 2.25 0 0 0 19 15.75V4.25A2.25 2.25 0 0 0 16.75 2h-7.5ZM8.5 4.25a.75.75 0 0 1 .75-.75h7.5a.75.75 0 0 1 .75.75v11.5a.75.75 0 0 1-.75.75h-7.5a.75.75 0 0 1-.75-.75V4.25Z";
constexpr const char* kSaveSvg =
    "M5.25 3A2.25 2.25 0 0 0 3 5.25v13.5A2.25 2.25 0 0 0 5.25 21h13.5A2.25 2.25 0 0 0 21 18.75V7.96a2.25 2.25 0 0 0-.659-1.591l-2.71-2.71A2.25 2.25 0 0 0 16.04 3H5.25ZM4.5 5.25a.75.75 0 0 1 .75-.75H7v4.25A1.75 1.75 0 0 0 8.75 10.5h5.5A1.75 1.75 0 0 0 16 8.75V4.5h.04a.75.75 0 0 1 .53.22l2.71 2.71a.75.75 0 0 1 .22.53v10.79a.75.75 0 0 1-.75.75H17v-5.25A1.75 1.75 0 0 0 15.25 12.5h-6.5A1.75 1.75 0 0 0 7 14.25v5.25H5.25a.75.75 0 0 1-.75-.75V5.25Zm4 9a.25.25 0 0 1 .25-.25h6.5a.25.25 0 0 1 .25.25v5.25h-7v-5.25ZM8.5 4.5h6v4.25a.25.25 0 0 1-.25.25h-5.5a.25.25 0 0 1-.25-.25V4.5Z";
constexpr const char* kShareSvg =
    "M12 2.25a.75.75 0 0 1 .53.22l4.25 4.25a.75.75 0 0 1-1.06 1.06l-2.97-2.97V15a.75.75 0 0 1-1.5 0V4.81L8.28 7.78a.75.75 0 0 1-1.06-1.06l4.25-4.25a.75.75 0 0 1 .53-.22ZM4.75 13a.75.75 0 0 1 .75.75v4.5c0 .69.56 1.25 1.25 1.25h10.5c.69 0 1.25-.56 1.25-1.25v-4.5a.75.75 0 0 1 1.5 0v4.5A2.75 2.75 0 0 1 17.25 21H6.75A2.75 2.75 0 0 1 4 18.25v-4.5a.75.75 0 0 1 .75-.75Z";
constexpr const char* kDeleteSvg =
    "M10 3a2 2 0 0 0-2 2H4.75a.75.75 0 0 0 0 1.5h.58l.92 12.15A2.25 2.25 0 0 0 8.5 20.75h7a2.25 2.25 0 0 0 2.24-2.1l.92-12.15h.59a.75.75 0 0 0 0-1.5H16a2 2 0 0 0-2-2h-4Zm4.5 2h-5a.5.5 0 0 1 .5-.5h4a.5.5 0 0 1 .5.5Zm-7.66 1.5h10.32l-.91 12.04a.75.75 0 0 1-.75.71h-7a.75.75 0 0 1-.75-.71L6.84 6.5Z";

WidgetPtr page_header(GalleryApp& app, const std::string& title, const std::string& subtitle) {
    return VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 4.0f,
        .content = {
            Text { .text = title, .font_size = 22.0f },
            Text { .text = subtitle, .font_size = 12.5f, .color = Bind(app.muted_text) }
        }
    };
}

std::vector<MenuFlyoutItem> make_sample_menu_items(GalleryApp& app) {
    return {
        MenuFlyoutItem{ .label = "保存文件", .shortcut = "Ctrl+S", .svg_path = kSaveSvg,
                        .on_click = [&app]() { app.menu_status.set("已触发菜单: 保存文件 (Ctrl+S)"); } },
        MenuFlyoutItem{ .label = "复制内容", .shortcut = "Ctrl+C", .svg_path = kCopySvg,
                        .on_click = [&app]() { app.menu_status.set("已触发菜单: 复制内容 (Ctrl+C)"); } },
        MenuFlyoutItem{ .label = "导出并分享", .shortcut = "Ctrl+Shift+E", .svg_path = kShareSvg,
                        .on_click = [&app]() { app.menu_status.set("已触发菜单: 导出并分享"); } },
        MenuFlyoutItem::separator(),
        MenuFlyoutItem{ .label = "自动保存", .is_checkable = true, .checked = app.menu_auto_save.get(),
                        .on_click = [&app]() {
                            bool next = !app.menu_auto_save.get();
                            app.menu_auto_save.set(next);
                            app.menu_status.set(next ? "已勾选: 自动保存 (开启)" : "已取消勾选: 自动保存 (关闭)");
                        } },
        MenuFlyoutItem{ .label = "只读保护 (已禁用)", .disabled = true },
        MenuFlyoutItem::separator(),
        MenuFlyoutItem{ .label = "删除项目", .shortcut = "Del", .svg_path = kDeleteSvg,
                        .on_click = [&app]() { app.menu_status.set("已触发菜单: 删除项目"); } }
    };
}
} // namespace

// ==================== CommandBar ====================
WidgetPtr build_command_bar_page(GalleryApp& app) {
    auto primary_cmds = [&app]() -> std::vector<CommandBarItem> {
        return {
            CommandBarItem{ .label = "保存", .svg_path = kSaveSvg, .shortcut = "Ctrl+S",
                            .on_click = [&app]() { app.commandbar_status.set("CommandBar: 已点击「保存」"); } },
            CommandBarItem{ .label = "复制", .svg_path = kCopySvg, .shortcut = "Ctrl+C",
                            .on_click = [&app]() { app.commandbar_status.set("CommandBar: 已点击「复制」"); } },
            CommandBarItem{ .label = "分享", .svg_path = kShareSvg,
                            .on_click = [&app]() { app.commandbar_status.set("CommandBar: 已点击「分享」"); } },
            CommandBarItem::separator(),
            CommandBarItem{ .label = "自动保存", .svg_path = kSaveSvg, .is_toggle = true, .checked = true,
                            .on_toggle = [&app](bool on) {
                                app.commandbar_status.set(on ? "CommandBar Toggle: 自动保存已开启" : "CommandBar Toggle: 自动保存已关闭");
                            } },
            CommandBarItem{ .label = "删除", .svg_path = kDeleteSvg, .shortcut = "Del",
                            .on_click = [&app]() { app.commandbar_status.set("CommandBar: 已点击「删除」"); } }
        };
    };
    auto secondary_cmds = [&app]() -> std::vector<CommandBarItem> {
        return {
            CommandBarItem{ .label = "导出为 PDF", .svg_path = kShareSvg, .shortcut = "Ctrl+P",
                            .on_click = [&app]() { app.commandbar_status.set("二级命令: 导出为 PDF"); } },
            CommandBarItem::separator(),
            CommandBarItem{ .label = "清空缓存", .svg_path = kDeleteSvg,
                            .on_click = [&app]() { app.commandbar_status.set("二级命令: 已清空缓存"); } }
        };
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "CommandBar", "WinUI 3 命令工具栏，支持 Toggle 按钮、分隔线、窄窗口自适应溢出与 ... 二级菜单。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "紧凑标签模式 (DefaultLabelPosition::Right)", .font_size = 14.0f },
                    CommandBar {
                        .content = "文档操作",
                        .primary_commands = primary_cmds(),
                        .secondary_commands = secondary_cmds(),
                        .default_label_position = CommandBarDefaultLabelPosition::Right
                    },
                    Text { .text = Bind(app.commandbar_status), .font_size = 12.0f, .color = Bind(app.accent_text) }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "经典底部标签模式 (DefaultLabelPosition::Bottom)", .font_size = 14.0f },
                    CommandBar {
                        .content = "经典视图",
                        .primary_commands = primary_cmds(),
                        .secondary_commands = secondary_cmds(),
                        .default_label_position = CommandBarDefaultLabelPosition::Bottom
                    }
                }
            })
        }
    });
}

// ==================== DropDownButton & SplitButton ====================
WidgetPtr build_dropdown_button_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "DropDownButton & SplitButton", "带下拉箭头的菜单触发按钮，以及左右独立响应的主操作 + 下拉菜单拆分按钮。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "DropDownButton 与 SplitButton (Standard / Accent)", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            DropDownButton {
                                .text = "文件操作",
                                .svg_path = kShareSvg,
                                .items = make_sample_menu_items(app)
                            },
                            SplitButton {
                                .text = "保存",
                                .svg_path = kSaveSvg,
                                .accent = true,
                                .items = make_sample_menu_items(app),
                                .on_click = [&app]() { app.menu_status.set("已点击 SplitButton 左侧主操作: 立即保存"); }
                            },
                            SplitButton {
                                .text = "复制",
                                .svg_path = kCopySvg,
                                .accent = false,
                                .items = make_sample_menu_items(app),
                                .on_click = [&app]() { app.menu_status.set("已点击 SplitButton 左侧主操作: 复制"); }
                            }
                        }
                    },
                    Text { .text = Bind(app.menu_status), .font_size = 12.0f, .color = Bind(app.accent_text) }
                }
            })
        }
    });
}

// ==================== MenuFlyout & ContextMenu ====================
WidgetPtr build_menu_flyout_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "MenuFlyout & ContextMenu", "WinUI 3 悬浮上下文菜单，支持图标、快捷键、勾选状态、分隔线、高斯软阴影与触控长按弹出。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "右键菜单 / 触控长按 (450ms) 响应区", .font_size = 14.0f },
                    ContextMenuArea {
                        .items = make_sample_menu_items(app),
                        .child = Box {
                            .border_width = 1.0f,
                            .corner_radius = 6.0f,
                            .padding = EdgeInsets(24.0f, 16.0f),
                            .theme_card = true,
                            .child = Text {
                                .text = "在此区域任意位置点击鼠标右键（或触控屏长按）弹出 WinUI 3 上下文菜单",
                                .font_size = 13.0f,
                                .color = Bind(app.muted_text)
                            }
                        }
                    },
                    Text { .text = Bind(app.menu_status), .font_size = 12.0f, .color = Bind(app.accent_text) }
                }
            })
        }
    });
}

// ==================== ContentDialog ====================
WidgetPtr build_content_dialog_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "ContentDialog", "WinUI 3 模态对话框，具备 1.05x->1.00x 缩放弹出动效、64dp 高斯软阴影与平滑 Smoke 遮罩层。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "模态对话框演示", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Button {
                                .text = "弹出保存确认框 (3 按钮)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    ContentDialog::show(ContentDialogDesc{
                                        .title = "保存更改?",
                                        .content = "您对当前 KTUI Gallery 配置进行了修改，是否在离开前保存更改？按 Esc 或点击取消可关闭弹窗。",
                                        .primary_button_text = "保存",
                                        .secondary_button_text = "不保存",
                                        .close_button_text = "取消",
                                        .width = 430.0f,
                                        .on_result = [&app](ContentDialogResult res) {
                                            switch (res) {
                                                case ContentDialogResult::Primary:   app.dialog_status.set("对话框返回: Primary (已保存)"); break;
                                                case ContentDialogResult::Secondary: app.dialog_status.set("对话框返回: Secondary (放弃更改)"); break;
                                                case ContentDialogResult::Close:     app.dialog_status.set("对话框返回: Close (取消)"); break;
                                                default:                             app.dialog_status.set("对话框返回: Esc 取消"); break;
                                            }
                                        }
                                    });
                                }
                            },
                            Button {
                                .text = "弹出信息提示框 (单按钮)",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    ContentDialog::show(ContentDialogDesc{
                                        .title = "渲染引擎就绪",
                                        .content = "KerneTile GPU 计算着色器、NanoSVG 矢量管线与跨平台触控手势已全部启用。",
                                        .primary_button_text = "知道了",
                                        .width = 390.0f,
                                        .on_result = [&app](ContentDialogResult) {
                                            app.dialog_status.set("信息对话框已确认关闭");
                                        }
                                    });
                                }
                            }
                        }
                    },
                    Text { .text = Bind(app.dialog_status), .font_size = 12.0f, .color = Bind(app.accent_text) }
                }
            })
        }
    });
}

// ==================== InfoBar ====================
WidgetPtr build_info_bar_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "InfoBar", "WinUI 3 内联状态通知条，支持 4 种语义级别、操作按钮与平滑可关闭交互。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Text { .text = "可交互 InfoBar (支持关闭与切换语义):", .font_size = 13.5f },
                            Button {
                                .text = "切换级别 / 重新显示",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    int next = (static_cast<int>(app.infobar_severity.get()) + 1) % 4;
                                    app.infobar_severity.set(static_cast<InfoBarSeverity>(next));
                                    app.infobar_open.set(true);
                                }
                            }
                        }
                    },
                    InfoBar {
                        .title = "系统通知",
                        .message = "此 InfoBar 支持关闭按钮与自定义操作按钮，点击上方按钮可切换 4 种语义级别。",
                        .severity = Bind(app.infobar_severity),
                        .is_open = Bind(app.infobar_open),
                        .is_closable = true,
                        .action_text = "查看详情",
                        .on_action = [&app]() { app.dialog_status.set("InfoBar 操作按钮已点击"); },
                        .on_close = [&app]() { app.infobar_open.set(false); }
                    },
                    InfoBar {
                        .title = "成功",
                        .message = "NanoSVG 矢量图集、Chromium 平滑滚动与跨平台触控手势已就绪。",
                        .severity = InfoBarSeverity::Success,
                        .is_closable = false
                    },
                    InfoBar {
                        .title = "警告",
                        .message = "检测到高刷新率显示器，已自动启用 120 FPS 高精度定时器。",
                        .severity = InfoBarSeverity::Warning,
                        .is_closable = false
                    },
                    InfoBar {
                        .title = "错误",
                        .message = "连接远程渲染节点超时，已自动回退至本地 GPU 计算后端。",
                        .severity = InfoBarSeverity::Error,
                        .is_closable = false
                    }
                }
            })
        }
    });
}

// ==================== Toast ====================
WidgetPtr build_toast_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Toast", "Windows 11 悬浮通知吐司，支持高斯软阴影、滑入/滑出动效、底部倒计时进度条（悬停自动暂停）与多条自动堆叠。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "触发不同语义与方位的 Toast 通知", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "信息通知 (右下角)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    Toast::show(ToastDesc{
                                        .title = "系统消息",
                                        .message = "KerneTile 云母与触控手势已启用。",
                                        .severity = ToastSeverity::Informational,
                                        .placement = ToastPlacement::BottomRight,
                                        .duration_sec = 4.0f,
                                        .action_text = "查看",
                                        .on_action = [&app]() { app.toast_status.set("已点击 Toast 操作按钮: 查看"); }
                                    });
                                    app.toast_status.set("已弹出 Informational Toast (右下角)");
                                }
                            },
                            Button {
                                .text = "成功通知",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    Toast::show(ToastDesc{
                                        .title = "保存成功",
                                        .message = "KTUI Gallery 配置已写入本地缓存。",
                                        .severity = ToastSeverity::Success,
                                        .placement = ToastPlacement::BottomRight,
                                        .duration_sec = 3.5f
                                    });
                                    app.toast_status.set("已弹出 Success Toast");
                                }
                            },
                            Button {
                                .text = "警告通知 (右上角)",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    Toast::show(ToastDesc{
                                        .title = "性能提示",
                                        .message = "当前窗口正在以 120 FPS 高刷运行。",
                                        .severity = ToastSeverity::Warning,
                                        .placement = ToastPlacement::TopRight,
                                        .duration_sec = 4.0f
                                    });
                                    app.toast_status.set("已弹出 Warning Toast (右上角)");
                                }
                            },
                            Button {
                                .text = "错误通知 (底部居中)",
                                .variant = ButtonVariant::Danger,
                                .on_click = [&app]() {
                                    Toast::show(ToastDesc{
                                        .title = "连接超时",
                                        .message = "远端同步服务未响应，请稍后重试。",
                                        .severity = ToastSeverity::Error,
                                        .placement = ToastPlacement::BottomCenter,
                                        .duration_sec = 4.5f,
                                        .action_text = "重试",
                                        .on_action = [&app]() { app.toast_status.set("已点击 Toast 操作按钮: 重试"); }
                                    });
                                    app.toast_status.set("已弹出 Error Toast (底部居中)");
                                }
                            }
                        }
                    },
                    Text { .text = Bind(app.toast_status), .font_size = 12.0f, .color = Bind(app.accent_text) }
                }
            })
        }
    });
}

// ==================== NavigationView ====================
WidgetPtr build_navigation_view_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "NavigationView", "WinUI 3 完整导航视图：支持 Left / LeftCompact / LeftMinimal / Top 四种布局模式、弹性拉伸强调条、折叠态子菜单浮层、左滑手势展开与旋转设置齿轮。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Text { .text = "切换内嵌 NavigationView 布局模式:", .font_size = 13.0f },
                            ComboBox {
                                .items = {
                                    ComboBoxItem("Left (展开侧边栏)"),
                                    ComboBoxItem("LeftCompact (紧凑图标栏)"),
                                    ComboBoxItem("LeftMinimal (极简汉堡菜单)"),
                                    ComboBoxItem("Top (顶部水平导航)")
                                },
                                .selected_index = Bind(app.demo_nav_mode_idx),
                                .width = 220.0f,
                                .on_change = [&app](int idx, const std::string&) {
                                    app.demo_nav_mode_idx.set(idx);
                                    switch (idx) {
                                        case 1: app.demo_nav_mode.set(NavigationPaneDisplayMode::LeftCompact); break;
                                        case 2: app.demo_nav_mode.set(NavigationPaneDisplayMode::LeftMinimal); break;
                                        case 3: app.demo_nav_mode.set(NavigationPaneDisplayMode::Top); break;
                                        default: app.demo_nav_mode.set(NavigationPaneDisplayMode::Left); break;
                                    }
                                }
                            }
                        }
                    },
                    Box {
                        .height = 340.0f,
                        .border_width = 1.0f,
                        .corner_radius = 8.0f,
                        .theme_card = true,
                        .child = NavigationView {
                            .menu_items = {
                                NavigationViewItem{ .id = "overview", .label = "概览", .icon = NavigationIcon::Home },
                                NavigationViewItem{
                                    .id = "workspace", .label = "工作区", .icon = NavigationIcon::Controls, .badge = "2", .expanded = true,
                                    .children = {
                                        NavigationViewItem{ .id = "files", .label = "项目文件" },
                                        NavigationViewItem{ .id = "tasks", .label = "构建任务" }
                                    }
                                },
                                NavigationViewItem{ .id = "analytics", .label = "性能遥测", .icon = NavigationIcon::Progress }
                            },
                            .selected_id = Bind(app.demo_nav_selected),
                            .pane_title = "示例导航",
                            .pane_display_mode = Bind(app.demo_nav_mode),
                            .is_pane_open = true,
                            .is_pane_toggle_button_visible = true,
                            .is_back_button_visible = true,
                            .is_settings_visible = true,
                            .open_pane_length = 190.0f,
                            .page_builder = [&app](const std::string& pid) -> WidgetPtr {
                                std::string desc = "当前内嵌页面 ID: " + pid + " · 可切换上方布局模式或在折叠态点击「工作区」测试浮层子菜单。";
                                return Box {
                                    .padding = EdgeInsets(18.0f, 20.0f),
                                    .child = VStack {
                                        .spacing = 8.0f,
                                        .content = {
                                            Label { .text = "页面: " + pid, .variant = LabelVariant::Subtitle },
                                            Text { .text = desc, .font_size = 13.0f, .color = Bind(app.muted_text) }
                                        }
                                    }
                                };
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages
