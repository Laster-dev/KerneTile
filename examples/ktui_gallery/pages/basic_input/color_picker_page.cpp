#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

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

} // namespace gallery::pages
