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
