#include "gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_settings_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "设置", .font_size = 20.0f },
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "外观主题", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Button {
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    app.app->set_theme(app.app->current_theme().is_dark ? Theme::light() : Theme::dark());
                                }
                            }.bind(&RenderButton::set_text, app.theme_btn_text)
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "NavigationView 导航栏布局模式 (PaneDisplayMode)", .font_size = 14.0f },
                    ComboBox {
                        .items = {
                            "Auto (响应式自动)",
                            "Left (左侧固定)",
                            "LeftCompact (左侧紧凑 48px)",
                            "LeftMinimal (极简浮层)",
                            "Top (顶部水平导航)"
                        },
                        .width = 240.0f,
                        .on_change = [&app](int idx, const std::string&) {
                            app.nav_mode.set(static_cast<NavigationPaneDisplayMode>(idx));
                        }
                    }.bind_two_way(app.nav_mode_idx)
                }
            })
        }
    });
}

WidgetPtr build_about_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "关于 KerneTile KTUI Gallery", .font_size = 20.0f },
            app.make_card(VStack {
                .spacing = 6.0f,
                .content = {
                    Text { .text = "KerneTile 2D 瓦片计算渲染引擎 + C++20 声明式桌面 UI 框架", .font_size = 13.5f },
                    Text { .text = "控件视觉全面对标 Windows 11 / WinUI 3 Fluent 2，内置 NanoSVG 矢量图集管线与 Chromium 原版 Impulse 平滑滚动。", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
                }
            })
        }
    });
}

} // namespace gallery::pages
