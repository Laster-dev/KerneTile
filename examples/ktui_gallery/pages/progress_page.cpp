#include "gallery_pages.hpp"
#include <cmath>

using namespace ktui;

namespace gallery::pages {

namespace {
WidgetPtr page_header(GalleryApp& app, const std::string& title, const std::string& subtitle) {
    return VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 4.0f,
        .content = {
            Text { .text = title, .font_size = 22.0f },
            Text { .text = subtitle, .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
        }
    };
}
} // namespace

// ==================== Slider ====================
WidgetPtr build_slider_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Slider", "WinUI 3 滑块，具备 20px 外环手柄、悬停扩张/按压缩小的同心强调色内圆与平滑拖拽。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .font_size = 14.0f }.bind(&RenderText::set_text, app.slider_text),
                    Slider {
                        .min_value = 0.0f,
                        .max_value = 100.0f,
                        .step = 1.0f,
                        .on_change = [&app](float v) {
                            app.slider_val.set(v);
                            app.slider_text.set("调节数值 (滑块): " + std::to_string(static_cast<int>(std::round(v))) + "%");
                            app.progress_norm.set(v / 100.0f);
                        }
                    }.bind_two_way(app.slider_val),
                    Text { .text = "拖动滑块将同步更新 ProgressBar 与 ProgressRing 页面的确定进度值。", .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text)
                }
            })
        }
    });
}

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
                    ProgressBar { .width = 320.0f }.bind(app.progress_norm, [](RenderProgressBar& n, float v) { n.set_value(v, true); }),
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
                    ProgressBar { .indeterminate = true, .width = 320.0f }
                }
            })
        }
    });
}

// ==================== ProgressRing ====================
WidgetPtr build_progress_ring_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "ProgressRing", "WinUI 3 环形加载指示器，带 Round Cap 药丸圆头、零跳变周期衔接与多尺寸规范。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "不确定旋转加载 (Indeterminate)", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 32.0f,
                        .content = {
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    ProgressRing { .indeterminate = true, .size = 18.0f },
                                    Text { .text = "18px 紧凑", .font_size = 11.5f }.bind(&RenderText::set_color, app.muted_text)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    ProgressRing { .indeterminate = true, .size = 32.0f },
                                    Text { .text = "32px 标准", .font_size = 11.5f }.bind(&RenderText::set_color, app.muted_text)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    ProgressRing { .indeterminate = true, .size = 54.0f },
                                    Text { .text = "54px 大号", .font_size = 11.5f }.bind(&RenderText::set_color, app.muted_text)
                                }
                            }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "确定环形进度 (Determinate)", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 18.0f,
                        .content = {
                            ProgressRing { .indeterminate = false, .size = 36.0f, .stroke_width = 3.5f }.bind(app.progress_norm, [](RenderProgressRing& n, float v) { n.set_value(v, true); }),
                            Text { .font_size = 13.0f }.bind(&RenderText::set_text, app.slider_text).bind(&RenderText::set_color, app.muted_text)
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages
