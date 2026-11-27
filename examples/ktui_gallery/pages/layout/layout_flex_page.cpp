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

const Color kAmber  {0.82f, 0.52f, 0.10f, 0.88f};

const Color kCoral  {0.84f, 0.30f, 0.36f, 0.88f};

const Color kGreen  {0.16f, 0.62f, 0.34f, 0.88f};

} // namespace

// ==================== 1. Flex 弹性布局展示页 ====================
WidgetPtr build_layout_flex_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Flex 弹性布局 (HStack / VStack / Expanded / Flexible / Spacer)", 22.0f),
            app.make_muted("双通道 Flexbox 求解器：支持主轴对齐、交叉轴对齐、反向排布、flex_grow 比例分配、flex_shrink 溢出压缩与 32 项内零堆分配。", 12.5f),

            // 交互式 Flex Playground
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "交互式主轴 / 交叉轴对齐演练场", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Wrap {
                        .spacing = 12.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            HStack {
                                .spacing = 6.0f,
                                .content = {
                                    app.make_muted("Justify:", 12.0f),
                                    SegmentedControl {
                                        .items = {
                                            SegmentedItem{ .label = "Start" },
                                            SegmentedItem{ .label = "Center" },
                                            SegmentedItem{ .label = "End" },
                                            SegmentedItem{ .label = "Between" },
                                            SegmentedItem{ .label = "Evenly" }
                                        },
                                        .on_selected_index_changed = [&app](int idx) {
                                            static constexpr JustifyContent kMap[] = {
                                                JustifyContent::FlexStart,
                                                JustifyContent::Center,
                                                JustifyContent::FlexEnd,
                                                JustifyContent::SpaceBetween,
                                                JustifyContent::SpaceEvenly
                                            };
                                            if (idx >= 0 && idx < 5) app.layout_flex_justify.set(kMap[idx]);
                                        }
                                    }.bind_two_way(app.layout_flex_justify_idx)
                                }
                            },
                            HStack {
                                .spacing = 6.0f,
                                .content = {
                                    app.make_muted("Align:", 12.0f),
                                    SegmentedControl {
                                        .items = {
                                            SegmentedItem{ .label = "Start" },
                                            SegmentedItem{ .label = "Center" },
                                            SegmentedItem{ .label = "End" },
                                            SegmentedItem{ .label = "Stretch" }
                                        },
                                        .on_selected_index_changed = [&app](int idx) {
                                            static constexpr AlignItems kMap[] = {
                                                AlignItems::FlexStart,
                                                AlignItems::Center,
                                                AlignItems::FlexEnd,
                                                AlignItems::Stretch
                                            };
                                            if (idx >= 0 && idx < 4) app.layout_flex_align.set(kMap[idx]);
                                        }
                                    }.bind_two_way(app.layout_flex_align_idx)
                                }
                            },
                            ToggleSwitch {
                                .label = "反向排布 (RowReverse)",
                                .on_change = [&app](bool rev) {
                                    app.layout_flex_dir.set(rev ? FlexDirection::RowReverse : FlexDirection::Row);
                                }
                            }.bind_two_way(app.layout_flex_reverse)
                        }
                    },
                    HStack {
                        .spacing = 12.0f,
                        .content = {
                            Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.layout_flex_gap_label).bind(&RenderText::set_color, app.muted_text),
                            Expanded {
                                .child = Slider {
                                    .min_value = 0.0f,
                                    .max_value = 28.0f,
                                    .step = 2.0f,
                                    .on_change = [&app](float v) {
                                        app.layout_flex_gap_label.set("主轴间距 (spacing): " + std::to_string(static_cast<int>(std::lround(v))) + " px");
                                    }
                                }.bind_two_way(app.layout_flex_gap)
                            }
                        }
                    },
                    make_stage_box(
                        HStack {
                            .justify = app.layout_flex_justify.get(),
                            .align_items = app.layout_flex_align.get(),
                            .spacing = app.layout_flex_gap.get(),
                            .content = {
                                make_demo_tile("A (72x34)", kBlue, 72.0f, 34.0f),
                                make_demo_tile("B (96x56)", kTeal, 96.0f, 56.0f),
                                make_demo_tile("C (80x42)", kPurple, 80.0f, 42.0f),
                                make_demo_tile("D (68x28)", kAmber, 68.0f, 28.0f)
                            }
                        }.bind(&RenderFlexBox::set_justify, app.layout_flex_justify)
                         .bind(&RenderFlexBox::set_align_items, app.layout_flex_align)
                         .bind(&RenderFlexBox::set_gap, app.layout_flex_gap)
                         .bind(&RenderFlexBox::set_direction, app.layout_flex_dir),
                        116.0f
                    )
                }
            }),

            // Expanded 比例分配 & Spacer
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "Expanded 权重分配 (1 : 2 : 1) 与 Spacer 弹性推开", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    make_stage_box(HStack {
                        .spacing = 8.0f,
                        .content = {
                            Expanded { .flex = 1.0f, .child = make_demo_tile("Expanded(1)", kBlue) },
                            Expanded { .flex = 2.0f, .child = make_demo_tile("Expanded(2)", kTeal) },
                            Expanded { .flex = 1.0f, .child = make_demo_tile("Expanded(1)", kPurple) }
                        }
                    }),
                    make_stage_box(HStack {
                        .spacing = 8.0f,
                        .content = {
                            make_demo_tile("左侧工具", kGreen, 92.0f, 36.0f),
                            Spacer {},
                            make_demo_tile("右侧状态", kAmber, 92.0f, 36.0f),
                            make_demo_tile("操作", kCoral, 68.0f, 36.0f)
                        }
                    })
                }
            }),

            // flex_shrink 溢出压缩演示
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = {
                    Text { .text = "Flexible flex_shrink 溢出比例压缩", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    app.make_muted("在 360px 固定宽容器内放入总宽 500px 的三个子项：第一项 shrink=0 保持 140px，后两项按 shrink=1 与 shrink=2 比例自动压缩，绝不越界。", 12.0f),
                    Box {
                        .width = 360.0f,
                        .background_color = Color{0.0f, 0.0f, 0.0f, 0.14f},
                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.10f},
                        .border_width = 1.0f,
                        .corner_radius = 6.0f,
                        .padding = EdgeInsets(8.0f, 8.0f),
                        .child = HStack {
                            .spacing = 6.0f,
                            .content = {
                                Flexible { .flex_shrink = 0.0f, .child = make_demo_tile("固定 140px", kCoral, 140.0f, 38.0f) },
                                Flexible { .flex_shrink = 1.0f, .child = make_demo_tile("shrink=1", kBlue, 180.0f, 38.0f) },
                                Flexible { .flex_shrink = 2.0f, .child = make_demo_tile("shrink=2", kTeal, 180.0f, 38.0f) }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages
