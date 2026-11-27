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

const Color kBlue   {0.00f, 0.47f, 0.84f, 0.88f};

const Color kCoral  {0.84f, 0.30f, 0.36f, 0.88f};

} // namespace

// ==================== 4. ZStack & Positioned 独立展示页 ====================
WidgetPtr build_layout_zstack_page(GalleryApp& app) {
    static constexpr const char* kAlignNames[9] = {
        "TopLeft (-1, -1)", "TopCenter (0, -1)", "TopRight (1, -1)",
        "CenterLeft (-1, 0)", "Center (0, 0)", "CenterRight (1, 0)",
        "BottomLeft (-1, 1)", "BottomCenter (0, 1)", "BottomRight (1, 1)"
    };
    static constexpr Alignment kAlignValues[9] = {
        Alignment::top_left(), Alignment::top_center(), Alignment::top_right(),
        Alignment::center_left(), Alignment::center(), Alignment::center_right(),
        Alignment::bottom_left(), Alignment::bottom_center(), Alignment::bottom_right()
    };

    auto align_btn = [&](int idx, const std::string& label) -> WidgetPtr {
        return Button {
            .text = label,
            .on_click = [&app, idx]() {
                app.layout_zstack_align_idx.set(idx);
                app.layout_zstack_align.set(kAlignValues[idx]);
                app.layout_zstack_align_label.set(std::string("当前对齐锚点: ") + kAlignNames[idx]);
            }
        };
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("ZStack & Positioned 层叠与边角定位", 22.0f),
            app.make_muted("ZStack 按声明顺序从底到顶叠加多个图层；Positioned 可把任意子图层独立钉在父容器的四边或四角（例如商品/视频卡片上的左上角标签、右上角未读角标、底部半透明标题条）。", 12.5f),

            // 1. ZStack + Positioned 典型实战卡片
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "Positioned 实战：媒体/信息卡片多图层独立锚定", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Box {
                        .height = 176.0f,
                        .background_color = Color{0.08f, 0.14f, 0.24f, 0.90f},
                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.12f},
                        .border_width = 1.0f,
                        .corner_radius = 8.0f,
                        .child = ZStack {
                            .alignment = Alignment::center(),
                            .content = {
                                VStack {
                                    .align_items = AlignItems::Center,
                                    .spacing = 4.0f,
                                    .content = {
                                        Text { .text = "底层主视觉内容 (居中对齐)", .font_size = 15.0f, .color = Color::white(), .font_weight = FontWeight::SemiBold },
                                        app.make_muted("其余三个浮层由 Positioned 分别锚定在左上、右上和底部全宽", 12.0f)
                                    }
                                },
                                Positioned {
                                    .left = 12.0f,
                                    .top = 12.0f,
                                    .child = make_demo_tile("LIVE · 4K 120Hz", kBlue, 124.0f, 28.0f)
                                },
                                Positioned {
                                    .top = 12.0f,
                                    .right = 12.0f,
                                    .child = make_demo_tile("未读 99+", kCoral, 88.0f, 28.0f)
                                },
                                Positioned {
                                    .left = 12.0f,
                                    .right = 12.0f,
                                    .bottom = 12.0f,
                                    .height = 34.0f,
                                    .child = Box {
                                        .background_color = Color{0.0f, 0.0f, 0.0f, 0.55f},
                                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.14f},
                                        .border_width = 1.0f,
                                        .corner_radius = 6.0f,
                                        .padding = EdgeInsets(0.0f, 12.0f),
                                        .child = Text {
                                            .text = "底部通栏标题条: Positioned { .left = 12, .right = 12, .bottom = 12, .height = 34 }",
                                            .font_size = 12.0f,
                                            .color = Color::white()
                                        },
                                        .alignment = Alignment::center_left()
                                    }
                                }
                            }
                        }
                    }
                }
            }),

            // 2. ZStack 九宫格全局对齐交互
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "ZStack 九宫格全局对齐 (Alignment)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    app.make_accent(app.layout_zstack_align_label, 12.0f),
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 16.0f,
                        .content = {
                            Box {
                                .width = 230.0f,
                                .child = Grid {
                                    .columns = 3,
                                    .column_spacing = 6.0f,
                                    .row_spacing = 6.0f,
                                    .content = {
                                        align_btn(0, "左上"), align_btn(1, "中上"), align_btn(2, "右上"),
                                        align_btn(3, "左中"), align_btn(4, "居中"), align_btn(5, "右中"),
                                        align_btn(6, "左下"), align_btn(7, "中下"), align_btn(8, "右下")
                                    }
                                }
                            },
                            Expanded {
                                .child = Box {
                                    .height = 140.0f,
                                    .background_color = Color{0.0f, 0.0f, 0.0f, 0.16f},
                                    .border_color = Color{1.0f, 1.0f, 1.0f, 0.12f},
                                    .border_width = 1.0f,
                                    .corner_radius = 8.0f,
                                    .child = ZStack {
                                        .alignment = app.layout_zstack_align.get(),
                                        .padding = EdgeInsets(10.0f, 10.0f),
                                        .content = {
                                            Box {
                                                .width = 110.0f,
                                                .height = 56.0f,
                                                .background_color = Color{0.00f, 0.47f, 0.84f, 0.35f},
                                                .border_color = kBlue,
                                                .border_width = 1.5f,
                                                .corner_radius = 6.0f
                                            },
                                            make_demo_tile("锚点子层", kCoral, 82.0f, 30.0f)
                                        }
                                    }.bind(&RenderZStack::set_alignment, app.layout_zstack_align)
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
