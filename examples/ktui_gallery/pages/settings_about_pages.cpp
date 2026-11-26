#include "gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

namespace {
// Fluent 2 常用设置图标 (24x24 viewBox)
static constexpr const char* kIconPalette =
    "M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10c1.1 0 2-.9 2-2 0-.52-.2-.99-.52-1.35-.31-.35-.5-.81-.5-1.31 0-1.1.9-2 2-2h2.36c3.13 0 5.66-2.54 5.66-5.67C23 5.5 18.08 2 12 2Zm-5.5 9a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3Zm3-4a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3Zm5 0a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3Zm3 4a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3Z";
static constexpr const char* kIconLayers =
    "M12 2.5 2.5 7.5 12 12.5 21.5 7.5 12 2.5Zm-9.5 9.5L12 17l9.5-5M2.5 16.5 12 21.5l9.5-5";
static constexpr const char* kIconNav =
    "M4 5.5A1.5 1.5 0 0 1 5.5 4h13A1.5 1.5 0 0 1 20 5.5v13a1.5 1.5 0 0 1-1.5 1.5h-13A1.5 1.5 0 0 1 4 18.5v-13ZM9 5.5v13h9.5v-13H9Z";
static constexpr const char* kIconWindow =
    "M4.5 4h15A1.5 1.5 0 0 1 21 5.5v13a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18.5v-13A1.5 1.5 0 0 1 4.5 4Zm0 4v10.5h15V8h-15Z";
static constexpr const char* kIconInfo =
    "M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20Zm0 5a1.25 1.25 0 1 1 0 2.5A1.25 1.25 0 0 1 12 7Zm-1 4.5h2v6h-2v-6Z";
static constexpr const char* kIconSparkle =
    "M12 2l2.2 6.2L20.5 10l-6.3 1.8L12 18l-2.2-6.2L3.5 10l6.3-1.8L12 2Z";
} // namespace

WidgetPtr build_settings_page(GalleryApp& app) {
    auto always_on_top = std::make_shared<Property<bool>>(app.window ? app.window->is_always_on_top() : false);
    int cur_backdrop_idx = 1;
    if (app.window) {
        if (app.window->backdrop() == WindowBackdrop::None) cur_backdrop_idx = 0;
        else if (app.window->backdrop() == WindowBackdrop::MicaAlt) cur_backdrop_idx = 2;
    }

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 4.0f,
        .content = {
            Text { .text = "设置", .font_size = 24.0f, .font_weight = FontWeight::SemiBold },
            Box { .height = 8.0f },

            // 分组一：外观与个性化
            Text { .text = "外观与个性化", .font_size = 13.5f, .font_weight = FontWeight::SemiBold },
            Box { .height = 4.0f },
            SettingsCard {
                .header = "应用主题",
                .description = "选择深色或浅色 Fluent 2 主题，切换时触发物理径向波纹过渡",
                .header_icon_svg = kIconPalette,
                .content = ComboBox {
                    .items = {"深色 (Dark)", "浅色 (Light)"},
                    .selected_index = app.app->current_theme().is_dark ? 0 : 1,
                    .width = 160.0f,
                    .on_change = [&app](int idx, const std::string&) {
                        app.app->set_theme(idx == 0 ? Theme::dark() : Theme::light());
                    }
                }
            },
            SettingsCard {
                .header = "窗口背景材质 (Backdrop)",
                .description = "实时切换 Windows 11 自算云母 (Mica / Mica Alt) 或无材质纯底",
                .header_icon_svg = kIconLayers,
                .content = ComboBox {
                    .items = {"无材质 (None)", "云母 (Mica)", "深层云母 (Mica Alt)"},
                    .selected_index = cur_backdrop_idx,
                    .width = 180.0f,
                    .on_change = [&app](int idx, const std::string&) {
                        if (!app.window) return;
                        if (idx == 0) app.window->set_backdrop(WindowBackdrop::None);
                        else if (idx == 1) app.window->set_backdrop(WindowBackdrop::Mica);
                        else app.window->set_backdrop(WindowBackdrop::MicaAlt);
                    }
                }
            },

            Box { .height = 14.0f },

            // 分组二：导航与窗口
            Text { .text = "导航与窗口", .font_size = 13.5f, .font_weight = FontWeight::SemiBold },
            Box { .height = 4.0f },
            SettingsExpander {
                .header = "NavigationView 导航栏布局",
                .description = "调整侧边栏展开模式、紧凑态或顶部水平导航",
                .header_icon_svg = kIconNav,
                .expanded = true,
                .header_content = ComboBox {
                    .items = {
                        "Auto (响应式自动)",
                        "Left (左侧固定)",
                        "LeftCompact (左侧紧凑 48px)",
                        "LeftMinimal (极简浮层)",
                        "Top (顶部水平导航)"
                    },
                    .width = 210.0f,
                    .on_change = [&app](int idx, const std::string&) {
                        app.nav_mode.set(static_cast<NavigationPaneDisplayMode>(idx));
                    }
                }.bind_two_way(app.nav_mode_idx),
                .items = {
                    SettingsCard {
                        .header = "窗口始终置顶 (Always on Top)",
                        .description = "将 KTUI Gallery 主窗口固定在所有其他桌面窗口上方",
                        .is_nested = true,
                        .content = ToggleSwitch {
                            .checked = always_on_top->get(),
                            .on_change = [&app, always_on_top](bool on) {
                                always_on_top->set(on);
                                if (app.window) app.window->set_always_on_top(on);
                            }
                        }
                    },
                    SettingsCard {
                        .header = "新建独立工具窗口",
                        .description = "打开一个共享全局主题与响应式状态的多窗口实例",
                        .is_click_enabled = true,
                        .is_nested = true,
                        .on_click = [&app]() { app.spawn_child_window(); }
                    }
                }
            },

            Box { .height = 14.0f },

            // 分组三：关于
            Text { .text = "关于", .font_size = 13.5f, .font_weight = FontWeight::SemiBold },
            Box { .height = 4.0f },
            SettingsExpander {
                .header = "KerneTile KTUI Gallery",
                .description = "2D 瓦片计算着色器渲染引擎 + C++20 声明式桌面 UI 框架",
                .header_icon_svg = kIconInfo,
                .expanded = false,
                .header_content = Text { .text = "v1.0.0", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text),
                .items = {
                    SettingsCard {
                        .header = "渲染管线",
                        .description = "Direct3D 12 Compute 16x16 瓦片分箱 + DirectWrite 文本图集 + NanoSVG 矢量光栅化",
                        .is_nested = true
                    },
                    SettingsCard {
                        .header = "复制仓库地址",
                        .description = "https://github.com/Laster-dev/KerneTile",
                        .is_click_enabled = true,
                        .is_nested = true,
                        .on_click = []() {
                            Clipboard::set_text("https://github.com/Laster-dev/KerneTile");
                            Toast::show(ToastDesc{ .title = "已复制仓库链接", .message = "https://github.com/Laster-dev/KerneTile", .severity = ToastSeverity::Success });
                        }
                    }
                }
            }
        }
    });
}

WidgetPtr build_settings_card_page(GalleryApp& app) {
    auto status = std::make_shared<Property<std::string>>("点击任意设置卡片、开关、下拉框或展开器体验 Windows 11 原生设置交互");
    auto auto_update = std::make_shared<Property<bool>>(true);
    auto hw_accel = std::make_shared<Property<bool>>(true);
    auto font_scale = std::make_shared<Property<float>>(100.0f);

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 6.0f,
        .content = {
            app.make_page_title("SettingsCard & SettingsExpander"),
            app.make_muted("对标 Windows 11 设置应用与 CommunityToolkit.WinUI.Controls：支持前置矢量图标、主副标题、右侧任意真实控件嵌套、可点击导航卡片与可展开设置卡片组。"),
            Box { .height = 4.0f },

            SettingsCard {
                .header = "硬件加速合成",
                .description = "启用 D3D12 Compute Shader 瓦片渲染，显著降低高刷屏 CPU 占用",
                .header_icon_svg = kIconSparkle,
                .content = ToggleSwitch {
                    .checked = hw_accel->get(),
                    .on_change = [hw_accel, status](bool on) {
                        hw_accel->set(on);
                        status->set(on ? "硬件加速合成已开启" : "硬件加速合成已关闭");
                    }
                }
            },
            SettingsCard {
                .header = "可点击导航设置卡片",
                .description = "设置 is_click_enabled = true 后整张卡片可悬停点击，右侧自动呈现跳转箭头",
                .header_icon_svg = kIconWindow,
                .is_click_enabled = true,
                .on_click = [status]() {
                    status->set("触发了可点击 SettingsCard 导航事件！");
                    Toast::show(ToastDesc{ .title = "SettingsCard 点击", .message = "可用于跳转子设置页或打开对话框", .severity = ToastSeverity::Informational });
                }
            },
            SettingsExpander {
                .header = "高级更新与显示选项 (SettingsExpander)",
                .description = "头部可直接放置右侧控件，点击整行平滑展开子设置卡片列表",
                .header_icon_svg = kIconLayers,
                .expanded = true,
                .header_content = ToggleSwitch {
                    .checked = auto_update->get(),
                    .on_change = [auto_update, status](bool on) {
                        auto_update->set(on);
                        status->set(on ? "自动更新已开启" : "自动更新已关闭");
                    }
                },
                .items = {
                    SettingsCard {
                        .header = "更新通道",
                        .description = "选择稳定版、预览版或每日构建通道",
                        .is_nested = true,
                        .content = ComboBox {
                            .items = {"Stable 稳定版", "Beta 预览版", "Canary 每日构建"},
                            .selected_index = 0,
                            .width = 160.0f,
                            .on_change = [status](int, const std::string& label) {
                                status->set("已切换更新通道: " + label);
                            }
                        }
                    },
                    SettingsCard {
                        .header = "界面缩放比例",
                        .description = "拖动右侧 Slider 实时调整缩放百分比",
                        .is_nested = true,
                        .content = Slider {
                            .value = font_scale->get(),
                            .min_value = 75.0f,
                            .max_value = 150.0f,
                            .step = 5.0f,
                            .width = 160.0f,
                            .on_change = [font_scale, status](float v) {
                                font_scale->set(v);
                                status->set("界面缩放比例: " + std::to_string(static_cast<int>(v)) + "%");
                            }
                        }
                    },
                    SettingsCard {
                        .header = "重置全部高级设置",
                        .description = "将所有配置恢复到出厂默认值",
                        .is_nested = true,
                        .content = Button {
                            .text = "重置",
                            .on_click = [status]() { status->set("已重置所有高级设置"); }
                        }
                    }
                }
            },
            Box { .height = 6.0f },
            app.make_accent(*status, 12.5f, FontWeight::Medium)
        }
    });
}

WidgetPtr build_about_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 6.0f,
        .content = {
            Text { .text = "关于 KerneTile KTUI Gallery", .font_size = 24.0f, .font_weight = FontWeight::SemiBold },
            Box { .height = 6.0f },
            SettingsCard {
                .header = "KerneTile 引擎与 KTUI 框架",
                .description = "2D 瓦片计算渲染引擎 + C++20 声明式桌面 UI 框架，零异常零 RTTI 极致体积",
                .header_icon_svg = kIconInfo,
                .content = Text { .text = "v1.0.0", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
            },
            SettingsCard {
                .header = "开源仓库",
                .description = "https://github.com/Laster-dev/KerneTile",
                .header_icon_svg = kIconWindow,
                .is_click_enabled = true,
                .on_click = []() {
                    Clipboard::set_text("https://github.com/Laster-dev/KerneTile");
                    Toast::show(ToastDesc{ .title = "已复制仓库链接", .message = "https://github.com/Laster-dev/KerneTile", .severity = ToastSeverity::Success });
                }
            }
        }
    });
}

} // namespace gallery::pages
