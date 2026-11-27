#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

namespace {

static constexpr const char* kIconLayers =
    "M12 2.5 2.5 7.5 12 12.5 21.5 7.5 12 2.5Zm-9.5 9.5L12 17l9.5-5M2.5 16.5 12 21.5l9.5-5";

static constexpr const char* kIconWindow =
    "M4.5 4h15A1.5 1.5 0 0 1 21 5.5v13a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18.5v-13A1.5 1.5 0 0 1 4.5 4Zm0 4v10.5h15V8h-15Z";

static constexpr const char* kIconSparkle =
    "M12 2l2.2 6.2L20.5 10l-6.3 1.8L12 18l-2.2-6.2L3.5 10l6.3-1.8L12 2Z";

} // namespace

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

} // namespace gallery::pages
