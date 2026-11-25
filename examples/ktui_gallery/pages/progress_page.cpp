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
            app.make_page_title(title, 22.0f),
            Text { .text = subtitle, .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
        }
    };
}
} // namespace

// ==================== Slider ====================
WidgetPtr build_slider_page(GalleryApp& app) {
    auto tick_val_text = std::make_shared<Property<std::string>>("带刻度步进滑块 (Step=10): 40");
    auto range_text    = std::make_shared<Property<std::string>>("双滑块区间选择 (Range): 20 - 75");
    auto vert_text     = std::make_shared<Property<std::string>>("竖向音量: 65%");
    auto vert_rng_text = std::make_shared<Property<std::string>>("竖向频段: 25 - 80 Hz");

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Slider", "WinUI 3 滑块家族：连续滑块、刻度步进、双滑块区间 (RangeSlider) 与竖向滑块。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .font_size = 14.0f }.bind(&RenderText::set_text, app.slider_text),
                    Slider {
                        .min_value = 0.0f,
                        .max_value = 100.0f,
                        .step = 1.0f,
                        .width = 320.0f,
                        .on_change = [&app](float v) {
                            app.slider_val.set(v);
                            app.slider_text.set("调节数值 (滑块): " + std::to_string(static_cast<int>(std::round(v))) + "%");
                            app.progress_norm.set(v / 100.0f);
                        }
                    }.bind_two_way(app.slider_val),
                    app.make_muted("拖动滑块将同步更新 ProgressBar 与 ProgressRing 页面的确定进度值。", 12.0f)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .font_size = 14.0f }.bind(&RenderText::set_text, *tick_val_text),
                    Slider {
                        .value = 40.0f,
                        .min_value = 0.0f,
                        .max_value = 100.0f,
                        .step = 10.0f,
                        .width = 320.0f,
                        .on_change = [tick_val_text](float v) {
                            tick_val_text->set("带刻度步进滑块 (Step=10): " + std::to_string(static_cast<int>(std::round(v))));
                        },
                        .tick_frequency = 10.0f,
                        .show_ticks = true
                    },
                    app.make_muted("支持独立 TickFrequency 刻度标尺与步进吸附，以及禁用态展示。", 12.0f),
                    Slider { .value = 50.0f, .width = 240.0f, .disabled = true }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .font_size = 14.0f }.bind(&RenderText::set_text, *range_text),
                    Slider {
                        .min_value = 0.0f,
                        .max_value = 100.0f,
                        .step = 1.0f,
                        .width = 320.0f,
                        .tick_frequency = 20.0f,
                        .show_ticks = true,
                        .range_mode = true,
                        .range_start = 20.0f,
                        .range_end = 75.0f,
                        .on_range_changed = [range_text](float s, float e) {
                            range_text->set("双滑块区间选择 (Range): " +
                                std::to_string(static_cast<int>(std::round(s))) + " - " +
                                std::to_string(static_cast<int>(std::round(e))));
                        }
                    },
                    app.make_muted("左右双独立同心手柄，自动就近拾取并防交叉限位。", 12.0f)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "竖向滑块 (Vertical Slider & Vertical Range)", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 42.0f,
                        .content = {
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 8.0f,
                                .content = {
                                    Slider {
                                        .value = 65.0f,
                                        .min_value = 0.0f,
                                        .max_value = 100.0f,
                                        .step = 1.0f,
                                        .on_change = [vert_text](float v) {
                                            vert_text->set("竖向音量: " + std::to_string(static_cast<int>(std::round(v))) + "%");
                                        },
                                        .orientation = SliderOrientation::Vertical,
                                        .height = 150.0f
                                    },
                                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, *vert_text).bind(&RenderText::set_color, app.muted_text)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 8.0f,
                                .content = {
                                    Slider {
                                        .value = 50.0f,
                                        .min_value = 0.0f,
                                        .max_value = 100.0f,
                                        .step = 10.0f,
                                        .orientation = SliderOrientation::Vertical,
                                        .height = 150.0f,
                                        .tick_frequency = 20.0f,
                                        .show_ticks = true
                                    },
                                    app.make_muted("竖向带刻度", 12.0f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 8.0f,
                                .content = {
                                    Slider {
                                        .min_value = 0.0f,
                                        .max_value = 100.0f,
                                        .step = 5.0f,
                                        .orientation = SliderOrientation::Vertical,
                                        .height = 150.0f,
                                        .range_mode = true,
                                        .range_start = 25.0f,
                                        .range_end = 80.0f,
                                        .on_range_changed = [vert_rng_text](float s, float e) {
                                            vert_rng_text->set("竖向频段: " +
                                                std::to_string(static_cast<int>(std::round(s))) + " - " +
                                                std::to_string(static_cast<int>(std::round(e))) + " Hz");
                                        }
                                    },
                                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, *vert_rng_text).bind(&RenderText::set_color, app.muted_text)
                                }
                            }
                        }
                    }
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
                                    app.make_muted("18px 紧凑", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    ProgressRing { .indeterminate = true, .size = 32.0f },
                                    app.make_muted("32px 标准", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    ProgressRing { .indeterminate = true, .size = 54.0f },
                                    app.make_muted("54px 大号", 11.5f)
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
