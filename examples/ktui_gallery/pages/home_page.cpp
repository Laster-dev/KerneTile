#include "gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

namespace {
WidgetPtr nav_chip(GalleryApp& app, const std::string& label, const std::string& target_id, bool accent = false) {
    return Button {
        .text = label,
        .variant = accent ? ButtonVariant::Accent : ButtonVariant::Standard,
        .on_click = [&app, target_id]() {
            app.nav_selected.set(target_id);
        }
    };
}
} // namespace

WidgetPtr build_svg_page(GalleryApp& app) {
    static constexpr const char* kShieldPath =
        "M12 2.25c.3 0 .59.07.86.2l6.5 3.1c.54.26.89.8.89 1.4v4.73c0 4.65-3.07 8.79-7.53 10.12a2.5 2.5 0 0 1-1.44 0C6.82 20.47 3.75 16.33 3.75 11.68V6.95c0-.6.35-1.14.89-1.4l6.5-3.1c.27-.13.56-.2.86-.2Zm0 1.66-6.5 3.1v4.67c0 3.94 2.58 7.45 6.37 8.63.08.02.18.02.26 0 3.79-1.18 6.37-4.69 6.37-8.63V7.01L12 3.91Zm3.53 5.56a.75.75 0 0 1 0 1.06l-4.25 4.25a.75.75 0 0 1-1.06 0l-2-2a.75.75 0 1 1 1.06-1.06l1.47 1.47 3.72-3.72a.75.75 0 0 1 1.06 0Z";
    static constexpr const char* kRocketPath =
        "M12 2c3.87 0 7 3.13 7 7 0 2.57-1.39 5.05-3.5 6.82V19a1 1 0 0 1-1 1h-5a1 1 0 0 1-1-1v-3.18C6.39 14.05 5 11.57 5 9c0-3.87 3.13-7 7-7Zm0 4a3 3 0 1 0 0 6 3 3 0 0 0 0-6Zm-2 15h4a1 1 0 0 1 0 2h-4a1 1 0 0 1 0-2Z";
    static constexpr const char* kHeartPath =
        "M12 20.75a.75.75 0 0 1-.48-.17C6.2 16.22 2.5 12.69 2.5 8.5 2.5 5.42 4.92 3 8 3c1.74 0 3.41.81 4 2.09C12.59 3.81 14.26 3 16 3c3.08 0 5.5 2.42 5.5 5.5 0 4.19-3.7 7.72-9.02 12.08a.75.75 0 0 1-.48.17Z";

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("NanoSVG 矢量图形", 22.0f),
            app.make_muted("由 NanoSVG 解析光栅化并缓存至 KerneTile DynamicAtlas，全后端单图元 80B 硬件加速渲染。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "动态主题换色与 HiDPI 矢量图标", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 28.0f,
                        .content = {
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Svg { .path = kShieldPath, .viewbox_size = 24.0f, .width = 36.0f, .height = 36.0f }.bind(&RenderSvg::set_tint, app.accent_text),
                                    app.make_muted("36px 强调色", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Svg { .path = kRocketPath, .viewbox_size = 24.0f, .width = 36.0f, .height = 36.0f },
                                    app.make_muted("36px 主题前景", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Svg { .path = kHeartPath, .viewbox_size = 24.0f, .width = 36.0f, .height = 36.0f, .tint = Color{0.95f, 0.32f, 0.42f, 1.0f} },
                                    app.make_muted("36px 语义红", 11.5f)
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

WidgetPtr build_home_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("KTUI Gallery", 24.0f),
            app.make_muted("对标 Windows 11 WinUI 3 Gallery · 49 个独立控件与布局展厅 · 云母平滑聚焦 · 圆形涟漪主题切换 · 跨平台触控手势", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "布局系统 (Layout)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Wrap {
                        .spacing = 8.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            nav_chip(app, "Flex (HStack/VStack)", "layout_flex"),
                            nav_chip(app, "Wrap 流式换行", "layout_wrap"),
                            nav_chip(app, "Grid 二维网格", "layout_grid"),
                            nav_chip(app, "ZStack & Positioned", "layout_zstack"),
                            nav_chip(app, "Align & AspectRatio", "layout_align"),
                            nav_chip(app, "SplitView 可拖拽分隔", "layout_split_view"),
                            nav_chip(app, "DockPanel 拖拽停靠", "layout_dock_panel", true),
                            nav_chip(app, "Visibility 显隐", "layout_visibility")
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "基础输入 (Basic Input)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Wrap {
                        .spacing = 8.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            nav_chip(app, "Button", "button", true),
                            nav_chip(app, "DropDown / SplitButton", "dropdown_button"),
                            nav_chip(app, "CheckBox", "checkbox"),
                            nav_chip(app, "ToggleSwitch", "toggle_switch"),
                            nav_chip(app, "RadioButton", "radio_button"),
                            nav_chip(app, "RadioCard", "radio_card"),
                            nav_chip(app, "ComboBox", "combobox"),
                            nav_chip(app, "Slider", "slider"),
                            nav_chip(app, "CalendarDatePicker", "calendar_date_picker"),
                            nav_chip(app, "TimePicker", "time_picker"),
                            nav_chip(app, "ColorPicker", "color_picker"),
                            nav_chip(app, "RatingControl", "rating_control"),
                            nav_chip(app, "ToggleButton", "toggle_button", true),
                            nav_chip(app, "SegmentedControl", "segmented_control", true)
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "文本与数值 (Text & Numbers)", .font_size = 14.0f },
                    Wrap {
                        .spacing = 8.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            nav_chip(app, "Label", "label"),
                            nav_chip(app, "TextBox", "textbox"),
                            nav_chip(app, "TextArea", "text_area", true),
                            nav_chip(app, "AutoSuggestBox", "auto_suggest_box"),
                            nav_chip(app, "PasswordBox", "password_box"),
                            nav_chip(app, "NumberBox", "number_box")
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "导航、容器与工具栏 (Navigation, Containers & Toolbars)", .font_size = 14.0f },
                    Wrap {
                        .spacing = 8.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            nav_chip(app, "NavigationView", "navigation_view"),
                            nav_chip(app, "TabView", "tab_view"),
                            nav_chip(app, "BreadcrumbBar", "breadcrumb_bar"),
                            nav_chip(app, "Expander", "expander"),
                            nav_chip(app, "TreeView", "tree_view"),
                            nav_chip(app, "VirtualList", "virtual_list"),
                            nav_chip(app, "DataGrid", "data_grid"),
                            nav_chip(app, "FlipView", "flip_view", true),
                            nav_chip(app, "PipsPager", "pips_pager", true),
                            nav_chip(app, "CommandBar", "command_bar"),
                            nav_chip(app, "MenuFlyout / ContextMenu", "menu_flyout")
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "弹窗、状态与图形 (Dialogs, Status & Vector)", .font_size = 14.0f },
                    Wrap {
                        .spacing = 8.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            nav_chip(app, "ContentDialog", "content_dialog"),
                            nav_chip(app, "TeachingTip", "teaching_tip"),
                            nav_chip(app, "Tooltip", "tooltip", true),
                            nav_chip(app, "InfoBar", "info_bar"),
                            nav_chip(app, "InfoBadge", "info_badge", true),
                            nav_chip(app, "PersonPicture", "person_picture", true),
                            nav_chip(app, "Toast", "toast", true),
                            nav_chip(app, "ProgressBar", "progress_bar"),
                            nav_chip(app, "ProgressRing", "progress_ring"),
                            nav_chip(app, "NanoSVG", "svg")
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "质感交互 (Optical & Perceptual Polish)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Wrap {
                        .spacing = 8.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            nav_chip(app, "文字光渗补偿 (Font Gamma)", "tactile_font_gamma", true),
                            nav_chip(app, "方向性玻璃高光 (Directional Rim)", "tactile_directional_rim", true),
                            nav_chip(app, "OkLab 色彩插值", "tactile_oklab", true),
                            nav_chip(app, "零纹理亚克力微颗粒", "tactile_acrylic_grain", true),
                            nav_chip(app, "解析高斯内阴影", "tactile_inset_shadow", true),
                            nav_chip(app, "灵动岛液态融合", "tactile_metaballs", true),
                            nav_chip(app, "列表边缘渐隐羽化", "tactile_scroll_feather", true),
                            nav_chip(app, "跨页面共享元素转场", "tactile_hero_transition", true),
                            nav_chip(app, "指针感知微倾斜与按压", "tactile_micro_tilt", true),
                            nav_chip(app, "弹簧光滑输入光标", "tactile_spring_caret", true),
                            nav_chip(app, "滚动吸顶与阻尼视差", "tactile_scroll_parallax", true),
                            nav_chip(app, "翻滚数值与交叉切换", "tactile_rolling_counter", true),
                            nav_chip(app, "同心嵌套圆角几何", "tactile_concentric_radius", true)
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages
