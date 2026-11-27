#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

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

} // namespace gallery::pages
