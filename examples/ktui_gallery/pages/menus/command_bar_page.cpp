#include "../gallery_pages.hpp"

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
            app.make_page_title(title, 22.0f),
            Text { .text = subtitle, .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
        }
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
                    app.make_accent(app.commandbar_status, 12.0f)
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

} // namespace gallery::pages
