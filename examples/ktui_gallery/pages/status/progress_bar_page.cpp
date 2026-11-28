#include "../gallery_pages.hpp"
#include <cmath>

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

// ==================== ProgressBar ====================
WidgetPtr build_progress_bar_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "ProgressBar", "WinUI 3 进度条，1px 居中轨道与 3px 药丸进度条，支持确定进度与不确定流光模式。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "确定进度 (Determinate)", .font_size = 14.0f },
                    ProgressBar { .width = 264.0f }.bind(app.progress_norm, [](RenderProgressBar& n, float v) { n.set_value(v, true); }),
                    Slider {
                        .min_value = 0.0f,
                        .max_value = 100.0f,
                        .step = 1.0f,
                        .on_change = [&app](float v) {
                            app.slider_val.set(v);
                            app.slider_text.set("调节数值 (滑块): " + std::to_string(static_cast<int>(std::round(v))) + "%");
                            app.progress_norm.set(v / 100.0f);
                        }
                    }.bind_two_way(app.slider_val)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "不确定流光进度 (Indeterminate)", .font_size = 14.0f },
                    ProgressBar { .indeterminate = true, .width = 264.0f }
                }
            })
        }
    });
}

} // namespace gallery::pages
