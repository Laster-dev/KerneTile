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
} // namespace

WidgetPtr build_menus_and_buttons_card(GalleryApp& app) {
    auto menu_items = [&app]() -> std::vector<MenuFlyoutItem> {
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
    };

    return app.make_card(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "MenuFlyout / DropDownButton / SplitButton / 右键上下文菜单", .font_size = 14.0f },
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 10.0f,
                .content = {
                    DropDownButton {
                        .text = "文件操作",
                        .svg_path = kShareSvg,
                        .items = menu_items()
                    },
                    SplitButton {
                        .text = "保存",
                        .svg_path = kSaveSvg,
                        .accent = true,
                        .items = menu_items(),
                        .on_click = [&app]() { app.menu_status.set("已点击 SplitButton 左侧主操作: 立即保存"); }
                    },
                    SplitButton {
                        .text = "复制",
                        .svg_path = kCopySvg,
                        .accent = false,
                        .items = menu_items(),
                        .on_click = [&app]() { app.menu_status.set("已点击 SplitButton 左侧主操作: 快速复制"); }
                    }
                }
            },
            ContextMenuArea {
                .items = menu_items(),
                .child = Box {
                    .background_color = Bind(app.card_bg),
                    .border_color = Bind(app.card_border),
                    .border_width = 1.0f,
                    .corner_radius = 4.0f,
                    .padding = EdgeInsets(14.0f, 16.0f),
                    .child = Text {
                        .text = "在此卡片任意位置点击鼠标右键，弹出 WinUI 3 上下文菜单 (ContextMenu)",
                        .font_size = 12.5f,
                        .color = Bind(app.muted_text)
                    }
                }
            },
            Text { .text = Bind(app.menu_status), .font_size = 12.5f, .color = Bind(app.accent_text) }
        }
    });
}

WidgetPtr build_dialog_and_infobar_card(GalleryApp& app) {
    return app.make_card(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 10.0f,
        .content = {
            Text { .text = "ContentDialog (WinUI 3 模态对话框) & InfoBar (状态提示条)", .font_size = 14.0f },
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 8.0f,
                .content = {
                    Button {
                        .text = "打开确认对话框",
                        .variant = ButtonVariant::Accent,
                        .on_click = [&app]() {
                            ContentDialog::show(ContentDialogDesc{
                                .title = "保存更改？",
                                .content = "您对当前 KTUI Gallery 配置进行了修改，是否在离开前保存更改？按 Esc 或点击取消可关闭弹窗。",
                                .primary_button_text = "保存",
                                .secondary_button_text = "不保存",
                                .close_button_text = "取消",
                                .width = 430.0f,
                                .on_result = [&app](ContentDialogResult res) {
                                    switch (res) {
                                        case ContentDialogResult::Primary:
                                            app.dialog_status.set("对话框结果: Primary (已保存)");
                                            app.infobar_severity.set(InfoBarSeverity::Success);
                                            app.infobar_open.set(true);
                                            break;
                                        case ContentDialogResult::Secondary:
                                            app.dialog_status.set("对话框结果: Secondary (不保存)");
                                            app.infobar_severity.set(InfoBarSeverity::Warning);
                                            app.infobar_open.set(true);
                                            break;
                                        case ContentDialogResult::Close:
                                            app.dialog_status.set("对话框结果: Close (已取消)");
                                            break;
                                        default:
                                            app.dialog_status.set("对话框结果: None (Esc 关闭)");
                                            break;
                                    }
                                }
                            });
                        }
                    },
                    Button {
                        .text = "重新显示 InfoBar",
                        .on_click = [&app]() { app.infobar_open.set(true); }
                    },
                    Text { .text = Bind(app.dialog_status), .font_size = 12.0f, .color = Bind(app.muted_text) }
                }
            },
            InfoBar {
                .title = "状态通知",
                .message = "这是一条可交互关闭的 WinUI 3 InfoBar，点击上方对话框按钮可切换语义级别。",
                .severity = Bind(app.infobar_severity),
                .is_open = Bind(app.infobar_open),
                .is_closable = true,
                .action_text = "详情",
                .on_action = [&app]() { app.dialog_status.set("已点击 InfoBar 右侧操作按钮"); }
            },
            InfoBar {
                .title = "成功",
                .message = "NanoSVG 矢量图集与 Chromium 平滑滚动已就绪。",
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
    });
}

WidgetPtr build_dialogs_menus_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "弹窗、菜单与状态通知", .font_size = 20.0f },
            build_menus_and_buttons_card(app),
            build_dialog_and_infobar_card(app)
        }
    });
}

} // namespace gallery::pages
