#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

namespace {

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

} // namespace gallery::pages
