#include "gallery_pages.hpp"
#include <cmath>

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_slider_progress_card(GalleryApp& app) {
    return app.make_card(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 10.0f,
        .content = {
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 16.0f,
                .content = {
                    Text { .text = Bind(app.slider_text), .color = Bind(app.muted_text) },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Text { .text = "旋转加载:", .font_size = 11.5f, .color = Bind(app.muted_text) },
                            ProgressRing { .indeterminate = true, .size = 18.0f, .stroke_width = 2.5f }
                        }
                    }
                }
            },
            Slider {
                .value = Bind(app.slider_val),
                .min_value = 0.0f,
                .max_value = 100.0f,
                .step = 1.0f,
                .on_change = [&app](float v) {
                    app.slider_val.set(v);
                    app.slider_text.set("调节数值 (滑块): " + std::to_string(static_cast<int>(std::round(v))) + "%");
                    app.progress_norm.set(v / 100.0f);
                }
            },
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "同步进度:", .font_size = 11.5f, .color = Bind(app.muted_text) },
                    ProgressBar { .value = Bind(app.progress_norm), .width = 120.0f },
                    ProgressRing { .value = Bind(app.progress_norm), .indeterminate = false, .size = 18.0f, .stroke_width = 2.5f },
                    Text { .text = "流光:", .font_size = 11.5f, .color = Bind(app.muted_text) },
                    ProgressBar { .indeterminate = true, .width = 96.0f }
                }
            }
        }
    });
}

WidgetPtr build_progress_ring_card(GalleryApp& app) {
    return app.make_card(VStack {
        .align_items = AlignItems::Center,
        .spacing = 10.0f,
        .content = {
            Text { .text = "WinUI 3 官方规范 ProgressRing (带 Round Cap 药丸圆头):", .font_size = 12.0f, .color = Bind(app.muted_text) },
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 28.0f,
                .content = {
                    VStack {
                        .align_items = AlignItems::Center,
                        .spacing = 6.0f,
                        .content = {
                            ProgressRing { .indeterminate = true, .size = 18.0f },
                            Text { .text = "18px 紧凑", .font_size = 11.0f, .color = Bind(app.muted_text) }
                        }
                    },
                    VStack {
                        .align_items = AlignItems::Center,
                        .spacing = 6.0f,
                        .content = {
                            ProgressRing { .indeterminate = true, .size = 32.0f },
                            Text { .text = "32px 标准", .font_size = 11.0f, .color = Bind(app.muted_text) }
                        }
                    },
                    VStack {
                        .align_items = AlignItems::Center,
                        .spacing = 6.0f,
                        .content = {
                            ProgressRing { .indeterminate = true, .size = 54.0f },
                            Text { .text = "54px 大号", .font_size = 11.0f, .color = Bind(app.muted_text) }
                        }
                    }
                }
            }
        }
    });
}

WidgetPtr build_progress_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "WinUI 3 滑块与进度指示", .font_size = 20.0f },
            build_slider_progress_card(app),
            build_progress_ring_card(app)
        }
    });
}

} // namespace gallery::pages
