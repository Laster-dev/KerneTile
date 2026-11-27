#include "../gallery_pages.hpp"
#include <cmath>

using namespace ktui;

namespace gallery::pages {

namespace {

WidgetPtr make_demo_tile(const std::string& label, Color bg, float w = -1.0f, float h = 42.0f) {
    return Box {
        .width = w,
        .height = h,
        .background_color = bg,
        .border_color = Color{1.0f, 1.0f, 1.0f, 0.16f},
        .border_width = 1.0f,
        .corner_radius = 6.0f,
        .padding = EdgeInsets(6.0f, 12.0f),
        .child = Text {
            .text = label,
            .font_size = 12.5f,
            .color = Color::white(),
            .font_weight = FontWeight::Medium
        },
        .alignment = Alignment::center()
    };
}

WidgetPtr make_stage_box(WidgetPtr child, float height = -1.0f) {
    return Box {
        .height = height,
        .background_color = Color{0.0f, 0.0f, 0.0f, 0.14f},
        .border_color = Color{1.0f, 1.0f, 1.0f, 0.09f},
        .border_width = 1.0f,
        .corner_radius = 6.0f,
        .padding = EdgeInsets(10.0f, 10.0f),
        .child = std::move(child)
    };
}

const Color kBlue   {0.00f, 0.47f, 0.84f, 0.88f};

const Color kTeal   {0.06f, 0.60f, 0.56f, 0.88f};

const Color kPurple {0.48f, 0.36f, 0.86f, 0.88f};

const Color kGreen  {0.16f, 0.62f, 0.34f, 0.88f};

} // namespace

// ==================== 5. Align / Center / AspectRatio / Divider 独立展示页 ====================
WidgetPtr build_layout_align_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Align / Center / AspectRatio / Divider", 22.0f),
            app.make_muted("基础单子项几何约束与装饰原语：固定比例缩放容器、单子项对齐包装以及主题自适应横/竖分隔线。", 12.5f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "AspectRatio 固定宽高比 & Divider 分隔线", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Divider {},
                    Wrap {
                        .spacing = 14.0f,
                        .run_spacing = 12.0f,
                        .cross_alignment = AlignItems::Center,
                        .content = {
                            Box {
                                .width = 176.0f,
                                .child = AspectRatio {
                                    .ratio = 16.0f / 9.0f,
                                    .child = make_demo_tile("16 : 9 (宽屏视频)", kBlue, -1.0f, -1.0f)
                                }
                            },
                            Divider { .vertical = true, .length = 80.0f },
                            Box {
                                .width = 128.0f,
                                .child = AspectRatio {
                                    .ratio = 4.0f / 3.0f,
                                    .child = make_demo_tile("4 : 3 (画布)", kTeal, -1.0f, -1.0f)
                                }
                            },
                            Divider { .vertical = true, .length = 80.0f },
                            Box {
                                .width = 96.0f,
                                .child = AspectRatio {
                                    .ratio = 1.0f,
                                    .child = make_demo_tile("1 : 1 (头像)", kPurple, -1.0f, -1.0f)
                                }
                            }
                        }
                    }
                }
            }),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = {
                    Text { .text = "Center & Padding 居中包装", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    make_stage_box(
                        Center {
                            .child = Padding {
                                .insets = EdgeInsets(10.0f, 18.0f),
                                .child = make_demo_tile("Center { Padding { ... } }", kGreen, 210.0f, 38.0f)
                            }
                        },
                        96.0f
                    )
                }
            })
        }
    });
}

} // namespace gallery::pages
