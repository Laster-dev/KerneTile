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

} // namespace gallery::pages
