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
            app.make_page_title(title, 22.0f),
            Text { .text = subtitle, .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
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
                    app.make_accent(app.menu_status, 12.0f)
                }
            })
        }
    });
}

// ==================== MenuFlyout & ContextMenu ====================
WidgetPtr build_menu_flyout_page(GalleryApp& app) {
    struct MenuDemoState {
        bool show_minimap{true};
        bool word_wrap{false};
        bool gpu_overlay{true};
        int zoom_idx{1};      // 0: 75%, 1: 100%, 2: 125%, 3: 150%
        int sort_idx{0};      // 0: 名称, 1: 修改日期, 2: 体积
        int density_idx{1};   // 0: 紧凑, 1: 标准, 2: 宽松
        double font_pt{14.0}; // 10..24
        float opacity{85.0f}; // 0..100
        double rating{4.0};   // 1..5
    };
    auto st = std::make_shared<MenuDemoState>();
    auto rich_status = std::make_shared<Property<std::string>>(
        "状态: 小地图=开 | 自动换行=关 | 缩放=100% | 字号=14pt | 不透明度=85%");
    auto refresh_st = [st, rich_status]() {
        static constexpr const char* kZooms[4] = {"75%", "100%", "125%", "150%"};
        static constexpr const char* kSorts[3] = {"名称", "修改日期", "体积"};
        static constexpr const char* kDens[3]  = {"紧凑", "标准", "宽松"};
        rich_status->set(std::string("状态: 小地图=") + (st->show_minimap ? "开" : "关") +
                         " | 自动换行=" + (st->word_wrap ? "开" : "关") +
                         " | 缩放=" + kZooms[st->zoom_idx] +
                         " | 密度=" + kDens[st->density_idx] +
                         " | 排序=" + kSorts[st->sort_idx] +
                         " | 字号=" + std::to_string(static_cast<int>(std::round(st->font_pt))) + "pt" +
                         " | 不透明度=" + std::to_string(static_cast<int>(std::round(st->opacity))) + "%");
    };

    auto make_cascading_menu = [&app, st, refresh_st]() -> std::vector<MenuFlyoutItem> {
        return {
            MenuFlyoutItem{ .label = "新建文档", .shortcut = "Ctrl+N", .on_click = [&app]() { app.menu_status.set("已触发: 新建文档"); } },
            MenuFlyoutItem{
                .label = "导出为",
                .sub_items = {
                    MenuFlyoutItem{
                        .label = "位图与矢量图像",
                        .sub_items = {
                            MenuFlyoutItem{ .label = "PNG 无损图像", .shortcut = "32-bit", .on_click = [&app]() { app.menu_status.set("已导出: PNG 无损图像"); } },
                            MenuFlyoutItem{ .label = "WebP 压缩图像", .shortcut = "Lossy",  .on_click = [&app]() { app.menu_status.set("已导出: WebP 压缩图像"); } },
                            MenuFlyoutItem{ .label = "SVG 矢量路径",  .shortcut = "Vector", .on_click = [&app]() { app.menu_status.set("已导出: SVG 矢量路径"); } }
                        }
                    },
                    MenuFlyoutItem{ .label = "PDF 排版文档", .on_click = [&app]() { app.menu_status.set("已导出: PDF 排版文档"); } },
                    MenuFlyoutItem{ .label = "JSON 布局树",  .on_click = [&app]() { app.menu_status.set("已导出: JSON 布局树"); } }
                }
            },
            MenuFlyoutItem::separator(),
            MenuFlyoutItem{
                .label = "排序方式 (单选分组)",
                .sub_items = {
                    MenuFlyoutItem{ .label = "按名称排序",     .checked = (st->sort_idx == 0), .on_click = [&app, st, refresh_st]() { st->sort_idx = 0; refresh_st(); app.menu_status.set("排序方式: 按名称"); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "按修改日期排序", .checked = (st->sort_idx == 1), .on_click = [&app, st, refresh_st]() { st->sort_idx = 1; refresh_st(); app.menu_status.set("排序方式: 按修改日期"); }, .is_radio = true },
                    MenuFlyoutItem{ .label = "按文件体积排序", .checked = (st->sort_idx == 2), .on_click = [&app, st, refresh_st]() { st->sort_idx = 2; refresh_st(); app.menu_status.set("排序方式: 按体积"); }, .is_radio = true }
                }
            },
            MenuFlyoutItem::separator(),
            // 直接复用真实 ToggleSwitch 控件作为 trailing_widget，完整保留弹簧手感与原生动效！
            MenuFlyoutItem{
                .label = "显示代码小地图",
                .keep_open_on_click = true,
                .trailing_widget = ToggleSwitch {
                    .checked = st->show_minimap,
                    .on_change = [&app, st, refresh_st](bool on) {
                        st->show_minimap = on;
                        refresh_st();
                        app.menu_status.set(on ? "真实 ToggleSwitch: 开启代码小地图" : "真实 ToggleSwitch: 关闭代码小地图");
                    }
                }
            },
            MenuFlyoutItem{
                .label = "自动换行 (Word Wrap)",
                .keep_open_on_click = true,
                .trailing_widget = ToggleSwitch {
                    .checked = st->word_wrap,
                    .on_change = [&app, st, refresh_st](bool on) {
                        st->word_wrap = on;
                        refresh_st();
                        app.menu_status.set(on ? "真实 ToggleSwitch: 开启自动换行" : "真实 ToggleSwitch: 关闭自动换行");
                    }
                }
            },
            MenuFlyoutItem{
                .label = "字号大小 (pt)",
                .keep_open_on_click = true,
                .trailing_widget = NumberBox {
                    .value = st->font_pt,
                    .minimum = 10.0,
                    .maximum = 24.0,
                    .small_change = 1.0,
                    .width = 92.0f,
                    .on_value_changed = [st, refresh_st](double v) {
                        st->font_pt = v;
                        refresh_st();
                    }
                }
            }
        };
    };

    auto make_widget_embedded_menu = [&app, st, refresh_st]() -> std::vector<MenuFlyoutItem> {
        auto op_label = std::make_shared<Property<std::string>>(
            "图层不透明度: " + std::to_string(static_cast<int>(std::round(st->opacity))) + "%");
        return {
            // 任意嵌套真实控件 1：快捷按钮栏 (HStack + Button)
            MenuFlyoutItem::widget(HStack {
                .align_items = AlignItems::Center,
                .spacing = 6.0f,
                .content = {
                    Button { .text = "剪切", .height = 26.0f, .on_click = [&app]() { app.menu_status.set("快捷工具栏: 剪切"); } },
                    Button { .text = "复制", .height = 26.0f, .on_click = [&app]() { app.menu_status.set("快捷工具栏: 复制"); } },
                    Button { .text = "粘贴", .height = 26.0f, .on_click = [&app]() { app.menu_status.set("快捷工具栏: 粘贴"); } }
                }
            }),
            MenuFlyoutItem::separator(),
            // 任意嵌套真实控件 2：界面密度切换 (SegmentedControl)
            MenuFlyoutItem::widget(SegmentedControl {
                .items = {
                    SegmentedItem{ .label = "紧凑" },
                    SegmentedItem{ .label = "标准" },
                    SegmentedItem{ .label = "宽松" }
                },
                .selected_index = st->density_idx,
                .width = 228.0f,
                .on_selected_index_changed = [st, refresh_st](int idx) {
                    st->density_idx = idx;
                    refresh_st();
                }
            }),
            // 任意嵌套真实控件 3：不透明度滑块 (VStack + Text + Slider)
            MenuFlyoutItem::widget(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 4.0f,
                .content = {
                    Text { .font_size = 11.5f }.bind(&RenderText::set_text, *op_label).bind(&RenderText::set_color, app.muted_text),
                    Slider {
                        .value = st->opacity,
                        .min_value = 0.0f,
                        .max_value = 100.0f,
                        .step = 1.0f,
                        .width = 228.0f,
                        .on_change = [st, op_label, refresh_st](float v) {
                            st->opacity = v;
                            op_label->set("图层不透明度: " + std::to_string(static_cast<int>(std::round(v))) + "%");
                            refresh_st();
                        }
                    }
                }
            }),
            MenuFlyoutItem::separator(),
            // 任意嵌套真实控件 4：子菜单中继续嵌套真实 RatingControl 评分控件
            MenuFlyoutItem{
                .label = "为图层打分",
                .sub_items = {
                    MenuFlyoutItem::widget(RatingControl {
                        .value = static_cast<float>(st->rating),
                        .on_value_changed = [&app, st](float r) {
                            st->rating = static_cast<double>(r);
                            app.menu_status.set("子菜单内嵌 RatingControl 评分: " + std::to_string(static_cast<int>(r)) + " 星");
                        }
                    })
                }
            },
            // 任意嵌套真实控件 5：右侧伴随真实 ToggleSwitch
            MenuFlyoutItem{
                .label = "硬件加速合成",
                .keep_open_on_click = true,
                .trailing_widget = ToggleSwitch {
                    .checked = st->gpu_overlay,
                    .on_change = [st, refresh_st](bool on) { st->gpu_overlay = on; refresh_st(); }
                }
            },
            MenuFlyoutItem::separator(),
            MenuFlyoutItem{ .label = "重置图层参数", .shortcut = "Ctrl+0", .on_click = [&app, st, refresh_st]() {
                st->opacity = 100.0f; st->zoom_idx = 1; st->density_idx = 1; st->font_pt = 14.0; refresh_st();
                app.menu_status.set("已重置图层参数为默认值");
            }}
        };
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "MenuFlyout & ContextMenu", "WinUI 3 悬浮菜单与上下文菜单：支持顶部菜单栏、多级级联子菜单、内联 ToggleSwitch 开关、Radio 单选分组与内嵌自定义控件。"),
            // 1. 顶部菜单栏 (WinUI 3 MenuBar：无边框透明横条 + 悬停平滑滑块 + 展开态 Hot-Track 无缝切换)
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "顶部菜单栏 (WinUI 3 MenuBar，展开后滑动即可无缝切换)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    MenuBar {
                        .items = {
                            MenuBarItem{ .title = "文件", .items_builder = make_cascading_menu },
                            MenuBarItem{
                                .title = "编辑",
                                .items = {
                                    MenuFlyoutItem{ .label = "撤销", .shortcut = "Ctrl+Z", .on_click = [&app]() { app.menu_status.set("顶部菜单: 撤销"); } },
                                    MenuFlyoutItem{ .label = "重做", .shortcut = "Ctrl+Y", .on_click = [&app]() { app.menu_status.set("顶部菜单: 重做"); } },
                                    MenuFlyoutItem::separator(),
                                    MenuFlyoutItem{ .label = "查找与替换", .shortcut = "Ctrl+H", .on_click = [&app]() { app.menu_status.set("顶部菜单: 查找与替换"); } }
                                }
                            },
                            MenuBarItem{ .title = "视图", .items_builder = make_widget_embedded_menu },
                            MenuBarItem{
                                .title = "帮助",
                                .items = {
                                    MenuFlyoutItem{ .label = "快捷键速查", .shortcut = "F1", .on_click = [&app]() { app.menu_status.set("顶部菜单: 打开快捷键速查"); } },
                                    MenuFlyoutItem{ .label = "关于 KerneTile", .on_click = [&app]() { app.menu_status.set("KerneTile 2D GPU Compute Engine"); } }
                                }
                            }
                        }
                    }
                }
            }),
            // 2. 多级级联 + 开关型 + 单选型右键菜单
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "多级级联 & 开关型 / 单选分组右键菜单", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    ContextMenuArea {
                        .child = Box {
                            .border_width = 1.0f,
                            .corner_radius = 6.0f,
                            .padding = EdgeInsets(22.0f, 16.0f),
                            .theme_card = true,
                            .child = app.make_muted("右键点击此处：体验 3 级嵌套子菜单（导出为 ▸ 位图与矢量图像 ▸ PNG/WebP/SVG）、Radio 单选与原地切换不关闭的 ToggleSwitch 开关项", 13.0f)
                        },
                        .items_builder = make_cascading_menu
                    }
                }
            }),
            // 3. 内嵌自定义控件的超级右键菜单
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "内嵌自定义控件右键菜单 (内置按钮栏 + Slider 滑块 + 开关)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    ContextMenuArea {
                        .child = Box {
                            .border_width = 1.0f,
                            .corner_radius = 6.0f,
                            .padding = EdgeInsets(22.0f, 16.0f),
                            .theme_card = true,
                            .child = app.make_muted("右键点击此处：在菜单内直接拖动 Slider 调节不透明度、点击顶部快捷按钮栏、以及切换内联开关", 13.0f)
                        },
                        .items_builder = make_widget_embedded_menu
                    },
                    Text { .font_size = 12.5f }.bind(&RenderText::set_text, *rich_status).bind(&RenderText::set_color, app.accent_text),
                    app.make_accent(app.menu_status, 12.0f)
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
                    app.make_accent(app.dialog_status, 12.0f)
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
                        .is_closable = true,
                        .action_text = "查看详情",
                        .on_action = [&app]() { app.dialog_status.set("InfoBar 操作按钮已点击"); },
                        .on_close = [&app]() { app.infobar_open.set(false); }
                    }.bind(&RenderInfoBar::set_severity, app.infobar_severity)
                     .bind_two_way(app.infobar_open),
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
                    app.make_accent(app.toast_status, 12.0f)
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
                            }.bind_two_way(app.demo_nav_mode_idx)
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
                            .pane_title = "示例导航",
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
                                            Text { .text = desc, .font_size = 13.0f }.bind(&RenderText::set_color, app.muted_text)
                                        }
                                    }
                                };
                            }
                        }.bind_two_way(app.demo_nav_selected)
                         .bind(&RenderNavigationView::set_pane_display_mode, app.demo_nav_mode)
                    }
                }
            })
        }
    });
}

// ==================== TabView ====================
WidgetPtr build_tab_view_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "TabView", "WinUI 3 多标签页控件 (Windows Terminal / Edge 风格)：选中标签无缝融底、单标签关闭 (×) 与动态新建 (+)。"),
            TabView {
                .is_add_tab_button_visible = true,
                .height = 280.0f,
                .content_builder = [&app](const std::string& tab_id) -> WidgetPtr {
                    return Box {
                        .padding = EdgeInsets(18.0f, 20.0f),
                        .child = VStack {
                            .align_items = AlignItems::Stretch,
                            .spacing = 10.0f,
                            .content = {
                                Label { .text = "当前标签页: " + tab_id, .variant = LabelVariant::Subtitle },
                                app.make_muted("点击顶部「+」可动态新建标签页，点击标签右侧「×」可关闭可关闭标签页；首标签「欢迎页」设置为不可关闭 (is_closable = false)。", 13.0f),
                                HStack {
                                    .align_items = AlignItems::Center,
                                    .spacing = 8.0f,
                                    .content = {
                                        Label { .text = "Tab ID: " + tab_id, .variant = LabelVariant::Accent, .pill = true },
                                        Label { .text = "GPU 硬件合成", .variant = LabelVariant::Success, .pill = true }
                                    }
                                }
                            }
                        }
                    };
                },
                .on_selection_changed = [&app](int idx) {
                    app.tab_selected.set(idx);
                },
                .on_tab_close = [&app](int idx) {
                    auto list = app.tab_items.get();
                    if (idx >= 0 && idx < static_cast<int>(list.size())) {
                        list.erase(list.begin() + idx);
                        int next_sel = std::min(app.tab_selected.get(), static_cast<int>(list.size()) - 1);
                        app.tab_items.set(std::move(list));
                        app.tab_selected.set(std::max(0, next_sel));
                    }
                },
                .on_add_tab = [&app]() {
                    auto list = app.tab_items.get();
                    int n = app.tab_seq++;
                    list.push_back(TabViewItem{
                        .id = "tab_" + std::to_string(n),
                        .header = "文档 #" + std::to_string(n),
                        .svg_path = kSaveSvg,
                        .is_closable = true
                    });
                    int new_idx = static_cast<int>(list.size()) - 1;
                    app.tab_items.set(std::move(list));
                    app.tab_selected.set(new_idx);
                }
            }.bind(&RenderTabView::set_tabs_value, app.tab_items)
             .bind_two_way(app.tab_selected)
        }
    });
}

// ==================== BreadcrumbBar ====================
WidgetPtr build_breadcrumb_bar_page(GalleryApp& app) {
    auto sibling_path = std::make_shared<Property<std::string>>("当前路径: root / var / log / app（点击段名回退，点击段右侧 › / ▾ 弹出同级兄弟节点横跳）");
    auto sibling_items = std::make_shared<Property<std::vector<BreadcrumbItem>>>(std::vector<BreadcrumbItem>{
        BreadcrumbItem{ .id = "root", .label = "root", .siblings = {"root", "mnt", "proc"} },
        BreadcrumbItem{ .id = "var",  .label = "var",  .siblings = {"var", "etc", "usr", "opt"} },
        BreadcrumbItem{ .id = "log",  .label = "log",  .siblings = {"log", "cache", "lib", "run"} },
        BreadcrumbItem{ .id = "app",  .label = "app",  .siblings = {"app", "nginx", "systemd", "kernel"} }
    });
    auto format_path = [](const std::vector<BreadcrumbItem>& v) {
        std::string out;
        for (size_t i = 0; i < v.size(); ++i) { if (i > 0) out += " / "; out += v[i].label; }
        return out;
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "BreadcrumbBar", "WinUI 3 面包屑路径导航条：支持点击前段路径回溯、窄宽度自动折叠「...」浮层菜单，以及路径段 › / ▾ 同级兄弟节点下拉横跳。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "交互式文件系统路径导航", .font_size = 14.0f },
                    BreadcrumbBar {
                        .font_size = 14.5f,
                        .on_item_clicked = [&app](size_t idx, const BreadcrumbItem& item) {
                            auto list = app.breadcrumb_items.get();
                            if (idx + 1 < list.size()) {
                                list.resize(idx + 1);
                                app.breadcrumb_items.set(std::move(list));
                            }
                            app.breadcrumb_status.set("已跳转至目录: " + item.label);
                        }
                    }.bind(&RenderBreadcrumbBar::set_items_value, app.breadcrumb_items),
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "进入子目录 (+)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    auto list = app.breadcrumb_items.get();
                                    std::string name = "folder_" + std::to_string(list.size());
                                    list.push_back(BreadcrumbItem{ .id = name, .label = name });
                                    app.breadcrumb_items.set(std::move(list));
                                    app.breadcrumb_status.set("已进入子目录: " + name);
                                }
                            },
                            Button {
                                .text = "重置完整路径",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    app.breadcrumb_items.set({
                                        BreadcrumbItem{ .id = "root", .label = "此电脑" },
                                        BreadcrumbItem{ .id = "d", .label = "D:\\" },
                                        BreadcrumbItem{ .id = "kt", .label = "KerneTile" },
                                        BreadcrumbItem{ .id = "ktui", .label = "KTUI" },
                                        BreadcrumbItem{ .id = "widgets", .label = "widgets" }
                                    });
                                    app.breadcrumb_status.set("已恢复默认 5 级路径");
                                }
                            }
                        }
                    },
                    app.make_accent(app.breadcrumb_status, 12.0f)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "路径段同级兄弟下拉横跳 (Segment Sibling Jump)", .font_size = 14.0f },
                    BreadcrumbBar {
                        .font_size = 13.5f,
                        .on_item_clicked = [sibling_items, sibling_path, format_path](size_t idx, const BreadcrumbItem&) {
                            auto list = sibling_items->get();
                            if (idx + 1 < list.size()) {
                                list.resize(idx + 1);
                                sibling_items->set(list);
                            }
                            sibling_path->set("已回退至路径: " + format_path(list));
                        },
                        .on_sibling_selected = [sibling_items, sibling_path, format_path](size_t idx, const std::string& chosen) {
                            auto list = sibling_items->get();
                            if (idx < list.size()) {
                                list[idx].id = chosen;
                                list[idx].label = chosen;
                                list.resize(idx + 1);
                                sibling_items->set(list);
                            }
                            sibling_path->set("已横跳至兄弟路径: " + format_path(list));
                        }
                    }.bind(&RenderBreadcrumbBar::set_items_value, *sibling_items),
                    HStack {
                        .justify = JustifyContent::SpaceBetween,
                        .align_items = AlignItems::Center,
                        .content = {
                            app.make_accent(*sibling_path, 12.0f),
                            Button {
                                .text = "重置路径",
                                .on_click = [sibling_items, sibling_path]() {
                                    sibling_items->set({
                                        BreadcrumbItem{ .id = "root", .label = "root", .siblings = {"root", "mnt", "proc"} },
                                        BreadcrumbItem{ .id = "var",  .label = "var",  .siblings = {"var", "etc", "usr", "opt"} },
                                        BreadcrumbItem{ .id = "log",  .label = "log",  .siblings = {"log", "cache", "lib", "run"} },
                                        BreadcrumbItem{ .id = "app",  .label = "app",  .siblings = {"app", "nginx", "systemd", "kernel"} }
                                    });
                                    sibling_path->set("当前路径: root / var / log / app");
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

// ==================== Expander ====================
WidgetPtr build_expander_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Expander", "WinUI 3 折叠展开卡片：标题栏带旋转 180° 箭头与平滑高度过渡动效，适合设置项与高级选项分组。"),
            Expander {
                .header = "渲染管线与帧率策略",
                .subtitle = "配置 GPU 计算着色器、高刷定时器与脏瓦片合批选项",
                .svg_path = kSaveSvg,
                .content = VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 10.0f,
                    .content = {
                        app.make_muted("展开后支持内嵌任意交互控件（开关、按钮、输入框等）：", 12.5f),
                        ToggleSwitch { .label = "启用 120 FPS 高精度等待定时器" }.bind_two_way(app.step_ten),
                        HStack {
                            .align_items = AlignItems::Center,
                            .spacing = 8.0f,
                            .content = {
                                Label { .text = "D3D12 Compute", .variant = LabelVariant::Accent, .pill = true },
                                Label { .text = "16×16 Tile Binning", .variant = LabelVariant::Success, .pill = true }
                            }
                        }
                    }
                }
            }.bind_two_way(app.expander_open_1),
            Expander {
                .header = "高级开发者诊断选项",
                .subtitle = "默认折叠状态，点击整行标题栏即可平滑展开",
                .svg_path = kCopySvg,
                .content = VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 8.0f,
                    .content = {
                        app.make_muted("这里可以放置低频诊断参数、日志级别或实验性功能开关。", 12.5f),
                        Button {
                            .text = "触发一条诊断 Toast",
                            .variant = ButtonVariant::Standard,
                            .on_click = []() {
                                Toast::show(ToastDesc{
                                    .title = "诊断就绪",
                                    .message = "Expander 内部按钮交互响应正常。",
                                    .severity = ToastSeverity::Success,
                                    .duration_sec = 3.0f
                                });
                            }
                        }
                    }
                }
            }.bind_two_way(app.expander_open_2)
        }
    });
}

WidgetPtr build_auto_suggest_box_page(GalleryApp& app) {
    static const std::vector<std::string> kAllControls = {
        "AutoSuggestBox", "BreadcrumbBar", "Button", "CalendarDatePicker",
        "CheckBox", "ComboBox", "CommandBar", "ContentDialog",
        "DropDownButton", "Expander", "InfoBar", "Label",
        "MenuFlyout", "NavigationView", "NumberBox", "PasswordBox",
        "ProgressBar", "ProgressRing", "RadioButton", "RadioCard",
        "Slider", "TabView", "TextBox", "Toast", "ToggleSwitch", "TreeView"
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("AutoSuggestBox", 22.0f),
            app.make_muted("WinUI 3 自动补全搜索框，内置清除与搜索按钮、真高斯阴影悬浮候选列表，支持 ↑/↓/Enter/Esc 键盘导航。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "快速检索 KTUI 控件", .font_size = 14.0f },
                    AutoSuggestBox {
                        .placeholder = "输入控件名 (如 Tab, Box, Bar, Tree)...",
                        .header = "控件搜索",
                        .items = kAllControls,
                        .width = 340.0f,
                        .on_text_changed = [&app](const std::string& q) {
                            app.suggest_status.set(q.empty() ? "已清空搜索词，点击输入框可浏览全部候选" : ("正在过滤关键字: " + q));
                        },
                        .on_suggestion_chosen = [&app](const std::string& chosen) {
                            app.suggest_status.set("已选择建议项: " + chosen);
                        },
                        .on_query_submitted = [&app](const std::string& q) {
                            app.suggest_status.set("已提交搜索查询: " + (q.empty() ? "(空)" : q));
                        }
                    }.bind_two_way(app.suggest_query),
                    app.make_accent(app.suggest_status, 12.5f)
                }
            })
        }
    });
}

WidgetPtr build_tree_view_page(GalleryApp& app) {
    static constexpr const char* kFolderSvg =
        "M3 6.75A2.75 2.75 0 0 1 5.75 4h3.58c.7 0 1.37.28 1.86.78l1.03 1.03c.2.2.48.31.76.31h5.27A2.75 2.75 0 0 1 21 8.87v8.38A2.75 2.75 0 0 1 18.25 20H5.75A2.75 2.75 0 0 1 3 17.25V6.75z";
    static constexpr const char* kFileSvg =
        "M6.75 3h6.5L19 8.75v10.5A1.75 1.75 0 0 1 17.25 21H6.75A1.75 1.75 0 0 1 5 19.25V4.75C5 3.78 5.78 3 6.75 3zm5.75 1.5v4.25h4.25L12.5 4.5z";

    std::vector<TreeViewNode> project_tree = {
        TreeViewNode{
            .id = "root_kt", .label = "KerneTile", .svg_path = kFolderSvg, .is_expanded = true,
            .children = {
                TreeViewNode{
                    .id = "src_ktui", .label = "KTUI", .svg_path = kFolderSvg, .is_expanded = true,
                    .children = {
                        TreeViewNode{ .id = "f_auto", .label = "auto_suggest_box.hpp", .svg_path = kFileSvg },
                        TreeViewNode{ .id = "f_tree", .label = "tree_view.hpp", .svg_path = kFileSvg },
                        TreeViewNode{ .id = "f_cal",  .label = "calendar_date_picker.hpp", .svg_path = kFileSvg }
                    }
                },
                TreeViewNode{
                    .id = "src_hal", .label = "HAL 后端", .svg_path = kFolderSvg, .is_expanded = false,
                    .children = {
                        TreeViewNode{ .id = "hal_d3d12",  .label = "d3d12_backend.cpp", .svg_path = kFileSvg },
                        TreeViewNode{ .id = "hal_vulkan", .label = "vulkan_backend.cpp", .svg_path = kFileSvg },
                        TreeViewNode{ .id = "hal_cpu",    .label = "cpu_rasterizer.cpp", .svg_path = kFileSvg }
                    }
                },
                TreeViewNode{ .id = "f_cmake", .label = "CMakeLists.txt", .svg_path = kFileSvg }
            }
        }
    };

    std::vector<TreeViewNode> feature_tree = {
        TreeViewNode{
            .id = "pkg_core", .label = "渲染与合成核心", .is_expanded = true,
            .children = {
                TreeViewNode{ .id = "feat_tile", .label = "16×16 瓦片分箱引擎", .is_checked = true },
                TreeViewNode{ .id = "feat_mica", .label = "云母动态聚焦背景", .is_checked = true },
                TreeViewNode{ .id = "feat_hdr",  .label = "广色域 HDR 合成 (实验)", .is_checked = false }
            }
        },
        TreeViewNode{
            .id = "pkg_platform", .label = "跨平台目标", .is_expanded = true,
            .children = {
                TreeViewNode{ .id = "plat_win", .label = "Windows 11 (Win32 + D3D12)", .is_checked = true },
                TreeViewNode{ .id = "plat_lin", .label = "Linux (X11 + XInput2)", .is_checked = false },
                TreeViewNode{ .id = "plat_web", .label = "WebOS Desktop (WASM)", .is_checked = false }
            }
        }
    };

    std::vector<TreeViewNode> tree_list_nodes = {
        TreeViewNode{
            .id = "tl_engine", .label = "KerneTile.Core", .svg_path = kFolderSvg, .is_expanded = true,
            .children = {
                TreeViewNode{ .id = "tl_bin", .label = "tile_binner.cpp", .svg_path = kFileSvg, .sub_columns = {"C++20", "18.4 KB", "0.12 ms"} },
                TreeViewNode{ .id = "tl_sdf", .label = "sdf_rasterizer.hlsl", .svg_path = kFileSvg, .sub_columns = {"HLSL SM6", "11.2 KB", "0.28 ms"} },
                TreeViewNode{ .id = "tl_atl", .label = "dynamic_atlas.cpp", .svg_path = kFileSvg, .sub_columns = {"C++20", "9.6 KB", "0.04 ms"} }
            },
            .sub_columns = {"模块", "39.2 KB", "0.44 ms"}
        },
        TreeViewNode{
            .id = "tl_ui", .label = "KTUI.Widgets", .svg_path = kFolderSvg, .is_expanded = true,
            .children = {
                TreeViewNode{ .id = "tl_tv", .label = "tree_view.hpp", .svg_path = kFileSvg, .sub_columns = {"Header", "22.1 KB", "120 FPS"} },
                TreeViewNode{ .id = "tl_dg", .label = "data_grid.hpp", .svg_path = kFileSvg, .sub_columns = {"Header", "19.8 KB", "120 FPS"} }
            },
            .sub_columns = {"模块", "41.9 KB", "120 FPS"}
        }
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("TreeView", 22.0f),
            app.make_muted("WinUI 3 层级树形视图：平滑展开/折叠高度与箭头旋转动画、单选/多选三态联动、以及多列可调宽 TreeListView 模式。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "多列树形表格 (TreeListView，支持表头拖拽调列宽 + 平滑折叠)", .font_size = 14.0f },
                    TreeView {
                        .nodes = std::move(tree_list_nodes),
                        .selection_mode = TreeViewSelectionMode::Single,
                        .selected_id = "tl_bin",
                        .height = 240.0f,
                        .columns = {
                            TreeListColumn{ .title = "名称", .width = 240.0f },
                            TreeListColumn{ .title = "类型", .width = 110.0f },
                            TreeListColumn{ .title = "体积", .width = 100.0f },
                            TreeListColumn{ .title = "耗时 / 帧率", .width = 0.0f }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "单选文件目录树 (SelectionMode::Single)", .font_size = 14.0f },
                    TreeView {
                        .nodes = std::move(project_tree),
                        .selection_mode = TreeViewSelectionMode::Single,
                        .height = 220.0f,
                        .on_selection_changed = [&app](const std::string& id, const std::string& label) {
                            app.tree_single_status.set("当前选中节点: " + label + " (" + id + ")");
                        }
                    }.bind_two_way(app.tree_single_selected),
                    app.make_accent(app.tree_single_status, 12.5f)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "多选三态勾选树 (SelectionMode::Multiple)", .font_size = 14.0f },
                    TreeView {
                        .nodes = std::move(feature_tree),
                        .selection_mode = TreeViewSelectionMode::Multiple,
                        .height = 220.0f,
                        .on_checked_changed = [&app](const std::vector<std::string>& ids) {
                            app.tree_multi_status.set("当前已勾选 " + std::to_string(ids.size()) + " 项");
                        }
                    },
                    app.make_accent(app.tree_multi_status, 12.5f)
                }
            })
        }
    });
}

WidgetPtr build_calendar_date_picker_page(GalleryApp& app) {
    CalendarDate today = CalendarDate::today();
    int end_day = std::min(today.day + 5, CalendarDate::days_in_month(today.year, today.month));
    CalendarDate init_end{today.year, today.month, end_day};
    auto range_status = std::make_shared<Property<std::string>>(
        "已选区间: " + today.format_iso() + " ~ " + init_end.format_iso());

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("CalendarDatePicker", 22.0f),
            app.make_muted("WinUI 3 日历日期选择器：带滑入弹出动画、滚轮/翻页平滑切月、单日与日期区间选择（含悬停实时预览连带）。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "双向绑定单日选择", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::FlexEnd,
                        .spacing = 12.0f,
                        .content = {
                            CalendarDatePicker {
                                .placeholder = "请选择日期",
                                .header = "发布日期",
                                .width = 240.0f,
                                .on_date_changed = [&app](const CalendarDate& d) {
                                    app.date_status.set("当前选中日期: " + d.format_iso());
                                }
                            }.bind_two_way(app.picked_date),
                            Button {
                                .text = "回到今天",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app, today]() {
                                    app.picked_date.set(today);
                                    app.date_status.set("当前选中日期: " + today.format_iso());
                                }
                            }
                        }
                    },
                    app.make_accent(app.date_status, 12.5f)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "日期区间选择 (Date Range)", .font_size = 14.0f },
                    CalendarDatePicker {
                        .placeholder = "请选择起止日期",
                        .header = "迭代冲刺周期 (点击起止两日，支持滚轮切月)",
                        .width = 280.0f,
                        .range_mode = true,
                        .start_date = today,
                        .end_date = init_end,
                        .on_range_changed = [range_status](const CalendarDate& s, const CalendarDate& e) {
                            range_status->set("已选区间: " + s.format_iso() + " ~ " + e.format_iso());
                        }
                    },
                    Text { .font_size = 12.5f }.bind(&RenderText::set_text, *range_status).bind(&RenderText::set_color, app.accent_text)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "限定可选范围 (仅限本月内)", .font_size = 14.0f },
                    CalendarDatePicker {
                        .date = today,
                        .header = "本月排期窗口",
                        .min_date = CalendarDate{today.year, today.month, 1},
                        .max_date = CalendarDate{today.year, today.month, CalendarDate::days_in_month(today.year, today.month)},
                        .width = 240.0f
                    }
                }
            })
        }
    });
}

WidgetPtr build_virtual_list_page(GalleryApp& app) {
    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("VirtualList", 22.0f),
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 10.0f,
                .content = {
                    app.make_muted("视口虚拟化高性能列表：100,000 条记录仍保持 O(1) 内存与 120 FPS 丝滑滚动。", 12.5f),
                    Label { .text = "O(1) 视口裁剪", .variant = LabelVariant::Accent, .pill = true }
                }
            },
            Expanded {
                .flex = 1.0f,
                .child = VirtualList {
                    .item_count = 100000,
                    .item_height = 38.0f,
                    .height = 0.0f,
                    .overscan = 3,
                    .item_builder = [&app](size_t idx) -> WidgetPtr {
                        bool even = (idx % 2 == 0);
                        return Box {
                            .background_color = even ? Color::transparent() : Color{1.0f, 1.0f, 1.0f, 0.02f},
                            .padding = EdgeInsets(0.0f, 12.0f),
                            .child = HStack {
                                .align_items = AlignItems::Center,
                                .spacing = 12.0f,
                                .content = {
                                    Label {
                                        .text = "#" + std::to_string(idx + 1),
                                        .variant = (idx % 5 == 0) ? LabelVariant::Accent : LabelVariant::Caption,
                                        .pill = true
                                    },
                                    Text {
                                        .text = "KerneTile 虚拟化数据条目 #" + std::to_string(idx + 1) + "  ·  Tile Binning 16×16",
                                        .font_size = 13.0f
                                    },
                                    Text {
                                        .text = (idx % 3 == 0) ? "GPU Ready" : "Active",
                                        .font_size = 11.5f
                                    }.bind(&RenderText::set_color, app.muted_text)
                                }
                            }
                        };
                    }
                }
            }
        }
    });
}

WidgetPtr build_color_picker_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("ColorPicker", 22.0f),
            app.make_muted("WinUI 3 弹出式颜色选择器，点击色块按钮弹出固定尺寸（324px）真高斯阴影色盘，支持 SV 色盘拖拽、色相彩虹条与透明度滑条。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "弹出式固定尺寸 RGBA 选色器", .font_size = 14.0f },
                    ColorPicker {
                        .header = "主题强调色",
                        .is_alpha_enabled = true,
                        .width = 210.0f,
                        .on_color_changed = [&app](const Color& c) {
                            app.color_status.set("当前颜色: " + RenderColorPicker::format_hex(c, true));
                        }
                    }.bind_two_way(app.picked_color),
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "Windows 蓝",
                                .on_click = [&app]() {
                                    Color c{0.0f, 120.0f / 255.0f, 212.0f / 255.0f, 1.0f};
                                    app.picked_color.set(c);
                                    app.color_status.set("当前颜色: " + RenderColorPicker::format_hex(c, true));
                                }
                            },
                            Button {
                                .text = "翡翠绿",
                                .on_click = [&app]() {
                                    Color c{16.0f / 255.0f, 185.0f / 255.0f, 129.0f / 255.0f, 1.0f};
                                    app.picked_color.set(c);
                                    app.color_status.set("当前颜色: " + RenderColorPicker::format_hex(c, true));
                                }
                            },
                            Button {
                                .text = "珊瑚红 80%",
                                .on_click = [&app]() {
                                    Color c{239.0f / 255.0f, 68.0f / 255.0f, 68.0f / 255.0f, 0.8f};
                                    app.picked_color.set(c);
                                    app.color_status.set("当前颜色: " + RenderColorPicker::format_hex(c, true));
                                }
                            }
                        }
                    },
                    app.make_accent(app.color_status, 12.5f)
                }
            })
        }
    });
}

WidgetPtr build_rating_control_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("RatingControl", 22.0f),
            app.make_muted("WinUI 3 星级评分控件，支持 Hover 实时预览高亮、整星/半星精度、再次点击清零与只读展示模式。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "交互式整星评分 (再次点击当前星级可清零)", .font_size = 14.0f },
                    RatingControl {
                        .max_rating = 5,
                        .is_clear_enabled = true,
                        .allow_half_stars = false,
                        .star_size = 24.0f,
                        .on_value_changed = [&app](float v) {
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "%.1f / 5.0", v);
                            app.rating_caption.set(buf);
                        }
                    }.bind_two_way(app.rating_val)
                     .bind(&RenderRatingControl::set_caption, app.rating_caption)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "半星精度评分 (AllowHalfStars = true)", .font_size = 14.0f },
                    RatingControl {
                        .max_rating = 5,
                        .allow_half_stars = true,
                        .star_size = 24.0f,
                        .on_value_changed = [&app](float v) {
                            char buf[48];
                            std::snprintf(buf, sizeof(buf), "%.1f / 5.0 (半星模式)", v);
                            app.rating_half_caption.set(buf);
                        }
                    }.bind_two_way(app.rating_half_val)
                     .bind(&RenderRatingControl::set_caption, app.rating_half_caption)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "只读展示 (ReadOnly = true)", .font_size = 14.0f },
                    RatingControl {
                        .value = 4.7f,
                        .max_rating = 5,
                        .is_read_only = true,
                        .star_size = 20.0f,
                        .caption = "4.7 (1,284 条评价)"
                    }
                }
            })
        }
    });
}

WidgetPtr build_teaching_tip_page(GalleryApp& app) {
    static constexpr const char* kBulbSvg =
        "M12 2a7 7 0 0 0-4 12.74V17a2 2 0 0 0 2 2h4a2 2 0 0 0 2-2v-2.26A7 7 0 0 0 12 2zm-2 19a1 1 0 0 0 1 1h2a1 1 0 1 0 0-2h-2a1 1 0 0 0-1 1z";
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("TeachingTip", 22.0f),
            app.make_muted("WinUI 3 功能引导气泡，复用 FluentPopupSurface 真高斯阴影与 130ms 缓出动效，带指向目标锚点的矢量三角尾标。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "触发引导气泡", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Button {
                                .text = "弹出带尾标引导气泡",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    Vec2 anchor{420.0f, 185.0f};
                                    if (Window* w = Window::active_window()) {
                                        anchor = Vec2{w->size().x * 0.52f, w->titlebar_height() + 140.0f};
                                    }
                                    TeachingTip::show(TeachingTipDesc{
                                        .title = "试试新的主题涟漪切换",
                                        .subtitle = "点击右上角标题栏主题按钮，即可体验仿 CUI 圆形 SDF 涟漪展开式深浅色平滑过渡效果。",
                                        .svg_path = kBulbSvg,
                                        .action_text = "立即了解",
                                        .close_text = "知道了",
                                        .preferred_placement = TeachingTipPlacement::Bottom,
                                        .on_action = [&app]() { app.tip_status.set("TeachingTip: 用户点击了主操作「立即了解」"); },
                                        .on_close  = [&app]() { app.tip_status.set("TeachingTip: 用户关闭了引导气泡"); }
                                    }, anchor);
                                }
                            },
                            Button {
                                .text = "弹出居中无尾标气泡",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    TeachingTip::show(TeachingTipDesc{
                                        .title = "KTUI Gallery 31 个展厅已就绪",
                                        .subtitle = "所有浮层均统一接入 FluentPopupSurface 真高斯阴影与 FrameClock 帧时钟调度。",
                                        .svg_path = kBulbSvg,
                                        .action_text = "查看 VirtualList",
                                        .close_text = "关闭",
                                        .on_action = [&app]() {
                                            app.tip_status.set("TeachingTip: 已跳转至 VirtualList 展厅");
                                            app.nav_selected.set("virtual_list");
                                        },
                                        .on_close = [&app]() { app.tip_status.set("TeachingTip: 已关闭居中引导气泡"); }
                                    });
                                }
                            }
                        }
                    },
                    app.make_accent(app.tip_status, 12.5f)
                }
            })
        }
    });
}

WidgetPtr build_data_grid_page(GalleryApp& app) {
    static const std::vector<DataGridColumn> kCols = {
        DataGridColumn{ .title = "ID", .width = 76.0f, .sortable = true, .numeric = true },
        DataGridColumn{ .title = "模块名称", .width = 180.0f, .sortable = true, .numeric = false },
        DataGridColumn{ .title = "渲染后端", .width = 140.0f, .sortable = true, .numeric = false },
        DataGridColumn{ .title = "耗时 (ms)", .width = 100.0f, .sortable = true, .numeric = true },
        DataGridColumn{ .title = "状态", .width = 0.0f, .sortable = true, .numeric = false }
    };

    static const std::vector<std::vector<std::string>> kRows = []() {
        static constexpr const char* kMods[] = {"TileBinning", "GaussianShadow", "MicaBackdrop", "ThemeRipple", "NanoSVG", "FlexLayout", "TextShaper", "Compositor"};
        static constexpr const char* kBackends[] = {"D3D12 Compute", "Vulkan Compute", "CPU AVX2", "WebGPU / WASM"};
        static constexpr const char* kStatus[] = {"120 FPS 就绪", "GPU 加速", "缓存命中", "活跃"};
        std::vector<std::vector<std::string>> r;
        r.reserve(5000);
        for (int i = 0; i < 5000; ++i) {
            char ms_buf[16];
            std::snprintf(ms_buf, sizeof(ms_buf), "%.2f", 0.15 + static_cast<double>((i * 37) % 450) / 100.0);
            r.push_back({
                std::to_string(i + 1),
                std::string(kMods[i % 8]) + " #" + std::to_string((i / 8) + 1),
                kBackends[i % 4],
                ms_buf,
                kStatus[i % 4]
            });
        }
        return r;
    }();

    auto dg_overflow = std::make_shared<Property<DataGridCellOverflow>>(DataGridCellOverflow::Ellipsis);

    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("DataGrid", 22.0f),
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 12.0f,
                .content = {
                    Expanded {
                        .flex = 1.0f,
                        .child = app.make_muted("高性能虚拟化数据表格：支持拖拽调列宽、平滑行选中、Win32 拉框多选 (Ctrl/Shift) 及省略号/裁剪溢出模式切换。", 12.5f)
                    },
                    SegmentedControl {
                        .items = {
                            SegmentedItem{ .label = "省略号 (...)" },
                            SegmentedItem{ .label = "直接裁剪" }
                        },
                        .selected_index = 0,
                        .on_selected_index_changed = [dg_overflow](int idx) {
                            dg_overflow->set(idx == 0 ? DataGridCellOverflow::Ellipsis : DataGridCellOverflow::Clip);
                        }
                    }
                }
            },
            Expanded {
                .flex = 1.0f,
                .child = DataGrid {
                    .columns = kCols,
                    .rows = kRows,
                    .row_height = 34.0f,
                    .height = 0.0f,
                    .on_row_selected = [&app](size_t orig_idx) {
                        app.datagrid_status.set("已选中原始行 #" + std::to_string(orig_idx + 1) +
                                                " (" + kRows[orig_idx][1] + " · " + kRows[orig_idx][3] + " ms)");
                    }
                }.bind(&RenderDataGrid::set_cell_overflow, *dg_overflow)
            },
            app.make_accent(app.datagrid_status, 12.5f)
        }
    });
}

WidgetPtr build_time_picker_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("TimePicker", 22.0f),
            app.make_muted("WinUI 3 时间选择器，点击弹出固定尺寸滚轮选时浮层，支持 24h / 12h AM·PM 模式、滚轮滚动步进与分钟步长配置。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "24 小时制与 12 小时制对比", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::FlexEnd,
                        .spacing = 16.0f,
                        .content = {
                            TimePicker {
                                .header = "会议开始时间 (24h)",
                                .use_24h = true,
                                .minute_increment = 5,
                                .width = 200.0f,
                                .on_time_changed = [&app](const TimeOfDay& t) {
                                    app.time_status.set("当前选择: " + t.format(true) + " (24h) / " + app.picked_time_12.get().format(false) + " (12h)");
                                }
                            }.bind_two_way(app.picked_time_24),
                            TimePicker {
                                .header = "提醒时间 (12h AM/PM)",
                                .use_24h = false,
                                .minute_increment = 15,
                                .width = 210.0f,
                                .on_time_changed = [&app](const TimeOfDay& t) {
                                    app.time_status.set("当前选择: " + app.picked_time_24.get().format(true) + " (24h) / " + t.format(false) + " (12h)");
                                }
                            }.bind_two_way(app.picked_time_12)
                        }
                    },
                    app.make_accent(app.time_status, 12.5f)
                }
            })
        }
    });
}

WidgetPtr build_info_badge_page(GalleryApp& app) {
    static constexpr const char* kStarSvg =
        "M12 2.5l2.84 5.75 6.35.92-4.6 4.48 1.09 6.32L12 16.98l-5.68 2.99 1.09-6.32-4.6-4.48 6.35-.92L12 2.5z";
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("InfoBadge", 22.0f),
            app.make_muted("WinUI 3 状态角标，提供数字（支持 99+ 溢出）、圆点与矢量图标三种形态，覆盖 5 种 Fluent 2 语义色。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "动态数字角标与 5 种语义形态", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            InfoBadge { .severity = InfoBadgeSeverity::Attention }.bind(&RenderInfoBadge::set_value, app.badge_count),
                            InfoBadge { .value = 128, .severity = InfoBadgeSeverity::Critical },
                            InfoBadge { .value = -1, .severity = InfoBadgeSeverity::Success },
                            InfoBadge { .value = -1, .severity = InfoBadgeSeverity::Caution, .svg_path = kStarSvg },
                            InfoBadge { .value = 7, .severity = InfoBadgeSeverity::Informational }
                        }
                    },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "未读数 +1",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() { app.badge_count.set(app.badge_count.get() + 1); }
                            },
                            Button {
                                .text = "设为 105 (触发 99+)",
                                .on_click = [&app]() { app.badge_count.set(105); }
                            },
                            Button {
                                .text = "清零",
                                .on_click = [&app]() { app.badge_count.set(0); }
                            }
                        }
                    }
                }
            })
        }
    });
}

WidgetPtr build_person_picture_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("PersonPicture", 22.0f),
            app.make_muted("WinUI 3 用户头像组件，支持中英文姓名首字母自动提取、确定性 Fluent 配色、多尺寸规范与在线状态指示圆点。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "多尺寸头像与在线状态指示点", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 18.0f,
                        .content = {
                            PersonPicture { .display_name = "Jane Doe", .diameter = 64.0f, .presence = PersonPresence::Available },
                            PersonPicture { .display_name = "林思远", .diameter = 52.0f, .presence = PersonPresence::Busy },
                            PersonPicture { .display_name = "Alex Chen", .diameter = 44.0f, .presence = PersonPresence::Away },
                            PersonPicture { .display_name = "KerneTile", .diameter = 36.0f, .presence = PersonPresence::Offline }
                        }
                    }
                }
            })
        }
    });
}

WidgetPtr build_text_area_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("TextArea", 22.0f),
            app.make_muted("WinUI 3 多行文本编辑器，支持回车换行、自动折行、多行光标上下左右定位、剪贴板、中文 IME 与内置垂直滚动条。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    TextArea {
                        .placeholder = "在此输入多行内容...",
                        .header = "文档备注",
                        .height = 140.0f,
                        .on_change = [&app](const std::string& v) {
                            app.textarea_status.set("当前字符数: " + std::to_string(v.size()) + " 字节");
                        }
                    }.bind_two_way(app.textarea_val),
                    app.make_accent(app.textarea_status, 12.5f)
                }
            })
        }
    });
}

WidgetPtr build_flip_view_page(GalleryApp& app) {
    auto make_slide = [](const std::string& title, const std::string& desc, Color bg) -> WidgetPtr {
        return Box {
            .background_color = bg,
            .padding = EdgeInsets(24.0f, 36.0f),
            .child = VStack {
                .align_items = AlignItems::Center,
                .spacing = 8.0f,
                .content = {
                    Text { .text = title, .font_size = 20.0f, .color = Color{1.0f, 1.0f, 1.0f, 0.96f} },
                    Text { .text = desc, .font_size = 13.0f, .color = Color{1.0f, 1.0f, 1.0f, 0.80f} }
                }
            }
        };
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("FlipView", 22.0f),
            app.make_muted("WinUI 3 轮播视图，支持悬停左右翻页按钮、触控滑动切页与平滑位移过渡，下方与 PipsPager 双向绑定联动。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Center,
                .spacing = 12.0f,
                .content = {
                    FlipView {
                        .items = {
                            make_slide("01 · KerneTile 16×16 瓦片渲染", "纯 C++20 零依赖 2D 计算着色器与 AVX2 软光栅双引擎", Color{0.08f, 0.34f, 0.62f, 0.85f}),
                            make_slide("02 · WinUI 3 Fluent 2 设计体系", "40 个独立控件展厅 · 真高斯阴影 · 云母动态聚焦 · 主题涟漪", Color{0.10f, 0.45f, 0.38f, 0.85f}),
                            make_slide("03 · 全平台原生与 WebOS Desktop", "Windows / Linux / Android / Emscripten WASM 桌面多开", Color{0.38f, 0.22f, 0.58f, 0.85f}),
                            make_slide("04 · 100,000 行虚拟化 120 FPS", "VirtualList & DataGrid 恒定 O(1) 视口开销", Color{0.62f, 0.24f, 0.32f, 0.85f})
                        },
                        .width = 520.0f,
                        .height = 170.0f
                    }.bind_two_way(app.flip_page),
                    PipsPager {
                        .number_of_pages = 4
                    }.bind_two_way(app.flip_page)
                }
            })
        }
    });
}

WidgetPtr build_pips_pager_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("PipsPager", 22.0f),
            app.make_muted("WinUI 3 圆点分页指示器，切换时当前圆点平滑放大并显示页码数字，1 秒后数字淡出、圆点平滑缩回。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Center,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "12 页滑动窗口演示 (MaxVisiblePips = 7)", .font_size = 14.0f },
                    PipsPager {
                        .number_of_pages = 12,
                        .max_visible_pips = 7,
                        .show_nav_buttons = true
                    }.bind_two_way(app.pips_page)
                }
            })
        }
    });
}

WidgetPtr build_segmented_control_page(GalleryApp& app) {
    static const char* kModes[] = {"列表视图", "网格视图", "紧凑详情"};
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("SegmentedControl", 22.0f),
            app.make_muted("Fluent 2 分段选择器，内置 FrameClock 驱动的平滑滑动激活胶囊底板与底部主题色微指示条。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "视图切换分段栏", .font_size = 14.0f },
                    SegmentedControl {
                        .items = {
                            SegmentedItem{ .label = "列表" },
                            SegmentedItem{ .label = "网格" },
                            SegmentedItem{ .label = "详情" }
                        },
                        .width = 280.0f,
                        .on_selected_index_changed = [&app](int idx) {
                            if (idx >= 0 && idx < 3) {
                                app.seg_status.set(std::string("当前视图模式: ") + kModes[idx]);
                            }
                        }
                    }.bind_two_way(app.seg_idx),
                    app.make_accent(app.seg_status, 12.5f)
                }
            })
        }
    });
}

WidgetPtr build_toggle_button_page(GalleryApp& app) {
    auto refresh_fmt = [&app]() {
        std::string s;
        if (app.tb_bold.get()) s += "粗体 ";
        if (app.tb_italic.get()) s += "斜体 ";
        if (app.tb_underline.get()) s += "下划线 ";
        app.tb_status.set("当前格式: " + (s.empty() ? "无" : s));
    };
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("ToggleButton", 22.0f),
            app.make_muted("WinUI 3 切换按钮，可保持选中主题色高亮态，适用于富文本格式或工具栏模式开关。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "文本格式切换组", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            ToggleButton { .text = "B 粗体", .on_change = [refresh_fmt](bool) { refresh_fmt(); } }.bind_two_way(app.tb_bold),
                            ToggleButton { .text = "I 斜体", .on_change = [refresh_fmt](bool) { refresh_fmt(); } }.bind_two_way(app.tb_italic),
                            ToggleButton { .text = "U 下划线", .on_change = [refresh_fmt](bool) { refresh_fmt(); } }.bind_two_way(app.tb_underline),
                            ToggleButton { .checked = false, .text = "已禁用", .disabled = true }
                        }
                    },
                    app.make_accent(app.tb_status, 12.5f)
                }
            })
        }
    });
}

WidgetPtr build_tooltip_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Tooltip", 22.0f),
            app.make_muted("WinUI 3 悬浮提示浮层，悬停后经可配置延迟平滑淡入，自动上下防溢出翻转，且点击穿透不阻断交互。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "基础按钮与不同延迟配置 (将鼠标悬停于控件上方)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Tooltip {
                                .text = "标准 WinUI 3 ToolTip (默认 400ms 延迟)",
                                .child = Button { .text = "标准提示 (400ms)", .variant = ButtonVariant::Accent }
                            },
                            Tooltip {
                                .text = "极速触发提示 (120ms 延迟)",
                                .delay_sec = 0.12f,
                                .child = Button { .text = "快速提示 (120ms)", .variant = ButtonVariant::Standard }
                            },
                            Tooltip {
                                .text = "慢速长悬停提示 (800ms 延迟)",
                                .delay_sec = 0.80f,
                                .child = Button { .text = "慢速提示 (800ms)", .variant = ButtonVariant::Standard }
                            }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "为标签、切换按钮与表单项挂载语义提示", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 12.0f,
                        .content = {
                            Tooltip {
                                .text = "当前 GPU 渲染管线已开启 16×16 瓦片脏区剔除",
                                .delay_sec = 0.25f,
                                .child = Label { .text = "渲染状态: 正常", .variant = LabelVariant::Success, .pill = true }
                            },
                            Tooltip {
                                .text = "快捷键 Ctrl+B 切换粗体格式",
                                .delay_sec = 0.25f,
                                .child = ToggleButton { .text = "B 粗体" }.bind_two_way(app.tb_bold)
                            },
                            Tooltip {
                                .text = "提示文本支持响应式 Bind(...) 动态更新",
                                .delay_sec = 0.25f,
                                .child = Button { .text = "动态绑定提示", .variant = ButtonVariant::Subtle }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages
