#include "gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_svg_showcase_card(GalleryApp& app) {
    static constexpr const char* kShieldPath =
        "M12 2.25c.3 0 .59.07.86.2l6.5 3.1c.54.26.89.8.89 1.4v4.73c0 4.65-3.07 8.79-7.53 10.12a2.5 2.5 0 0 1-1.44 0C6.82 20.47 3.75 16.33 3.75 11.68V6.95c0-.6.35-1.14.89-1.4l6.5-3.1c.27-.13.56-.2.86-.2Zm0 1.66-6.5 3.1v4.67c0 3.94 2.58 7.45 6.37 8.63.08.02.18.02.26 0 3.79-1.18 6.37-4.69 6.37-8.63V7.01L12 3.91Zm3.53 5.56a.75.75 0 0 1 0 1.06l-4.25 4.25a.75.75 0 0 1-1.06 0l-2-2a.75.75 0 1 1 1.06-1.06l1.47 1.47 3.72-3.72a.75.75 0 0 1 1.06 0Z";
    static constexpr const char* kRocketPath =
        "M12 2c3.87 0 7 3.13 7 7 0 2.57-1.39 5.05-3.5 6.82V19a1 1 0 0 1-1 1h-5a1 1 0 0 1-1-1v-3.18C6.39 14.05 5 11.57 5 9c0-3.87 3.13-7 7-7Zm0 4a3 3 0 1 0 0 6 3 3 0 0 0 0-6Zm-2 15h4a1 1 0 0 1 0 2h-4a1 1 0 0 1 0-2Z";
    static constexpr const char* kHeartPath =
        "M12 20.75a.75.75 0 0 1-.48-.17C6.2 16.22 2.5 12.69 2.5 8.5 2.5 5.42 4.92 3 8 3c1.74 0 3.41.81 4 2.09C12.59 3.81 14.26 3 16 3c3.08 0 5.5 2.42 5.5 5.5 0 4.19-3.7 7.72-9.02 12.08a.75.75 0 0 1-.48.17Z";

    return app.make_card(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 10.0f,
        .content = {
            Text { .text = "NanoSVG 硬件图集矢量渲染 (支持动态换色与 HiDPI 抗锯齿)", .font_size = 14.0f },
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 24.0f,
                .content = {
                    VStack {
                        .align_items = AlignItems::Center,
                        .spacing = 6.0f,
                        .content = {
                            Svg { .path = kShieldPath, .viewbox_size = 24.0f, .width = 32.0f, .height = 32.0f, .tint = Bind(app.accent_text) },
                            Text { .text = "32px 主题强调色", .font_size = 11.0f, .color = Bind(app.muted_text) }
                        }
                    },
                    VStack {
                        .align_items = AlignItems::Center,
                        .spacing = 6.0f,
                        .content = {
                            Svg { .path = kRocketPath, .viewbox_size = 24.0f, .width = 32.0f, .height = 32.0f },
                            Text { .text = "32px 主题前景", .font_size = 11.0f, .color = Bind(app.muted_text) }
                        }
                    },
                    VStack {
                        .align_items = AlignItems::Center,
                        .spacing = 6.0f,
                        .content = {
                            Svg { .path = kHeartPath, .viewbox_size = 24.0f, .width = 32.0f, .height = 32.0f, .tint = Color{0.95f, 0.32f, 0.42f, 1.0f} },
                            Text { .text = "32px 语义红", .font_size = 11.0f, .color = Bind(app.muted_text) }
                        }
                    }
                }
            }
        }
    });
}

WidgetPtr build_svg_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "NanoSVG 矢量图形", .font_size = 20.0f },
            Text { .text = "由 NanoSVG 解析光栅化并缓存至 KerneTile DynamicAtlas，全后端单图元硬件加速渲染。", .font_size = 12.5f, .color = Bind(app.muted_text) },
            build_svg_showcase_card(app)
        }
    });
}

WidgetPtr build_home_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "KTUI Gallery", .font_size = 22.0f },
            Text { .text = "Windows 11 / WinUI 3 Fluent 2 控件标准陈列室 · NanoSVG 矢量图标 · Chromium 平滑滚动", .font_size = 12.5f, .color = Bind(app.muted_text) },
            build_buttons_card(app),
            build_toggles_card(app),
            build_combobox_card(app),
            build_radio_button_card(app),
            build_radio_card_group_card(app),
            build_slider_progress_card(app),
            build_progress_ring_card(app),
            build_svg_showcase_card(app),
            build_text_input_card(app)
        }
    });
}

} // namespace gallery::pages
