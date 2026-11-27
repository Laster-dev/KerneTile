#include "../gallery_pages.hpp"
#include <cmath>

using namespace ktui;

namespace gallery::pages {

namespace {

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

WidgetPtr make_chip(const std::string& label, Color tint) {
    return Box {
        .background_color = Color{tint.r, tint.g, tint.b, 0.22f},
        .border_color = Color{tint.r, tint.g, tint.b, 0.55f},
        .border_width = 1.0f,
        .corner_radius = 12.0f,
        .padding = EdgeInsets(5.0f, 12.0f),
        .child = Text {
            .text = label,
            .font_size = 12.0f,
            .font_weight = FontWeight::Medium
        }
    };
}

const Color kBlue   {0.00f, 0.47f, 0.84f, 0.88f};

const Color kTeal   {0.06f, 0.60f, 0.56f, 0.88f};

const Color kPurple {0.48f, 0.36f, 0.86f, 0.88f};

const Color kAmber  {0.82f, 0.52f, 0.10f, 0.88f};

const Color kCoral  {0.84f, 0.30f, 0.36f, 0.88f};

const Color kGreen  {0.16f, 0.62f, 0.34f, 0.88f};

} // namespace

// ==================== 2. Wrap 流式自动换行布局展示页 ====================
WidgetPtr build_layout_wrap_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Wrap 流式自动换行布局", 22.0f),
            app.make_muted("当主轴可用宽度不足时自动折行，适用于标签云、筛选 Chip、响应式工具栏；拖拽窗口边缘即可观察实时重排。", 12.5f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "行内对齐与行列间距控制", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .spacing = 12.0f,
                        .content = {
                            SegmentedControl {
                                .items = {
                                    SegmentedItem{ .label = "左对齐" },
                                    SegmentedItem{ .label = "居中" },
                                    SegmentedItem{ .label = "右对齐" },
                                    SegmentedItem{ .label = "两端对齐" }
                                },
                                .on_selected_index_changed = [&app](int idx) {
                                    static constexpr JustifyContent kMap[] = {
                                        JustifyContent::FlexStart,
                                        JustifyContent::Center,
                                        JustifyContent::FlexEnd,
                                        JustifyContent::SpaceBetween
                                    };
                                    if (idx >= 0 && idx < 4) app.layout_wrap_align.set(kMap[idx]);
                                }
                            }.bind_two_way(app.layout_wrap_align_idx),
                            Spacer {},
                            Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.layout_wrap_spacing_label).bind(&RenderText::set_color, app.muted_text)
                        }
                    },
                    Slider {
                        .min_value = 2.0f,
                        .max_value = 20.0f,
                        .step = 2.0f,
                        .on_change = [&app](float v) {
                            app.layout_wrap_spacing_label.set("流式行列间距: " + std::to_string(static_cast<int>(std::lround(v))) + " px");
                        }
                    }.bind_two_way(app.layout_wrap_spacing),
                    make_stage_box(
                        Wrap {
                            .spacing = app.layout_wrap_spacing.get(),
                            .run_spacing = app.layout_wrap_spacing.get(),
                            .alignment = app.layout_wrap_align.get(),
                            .content = {
                                make_chip("C++20 Modules", kBlue),
                                make_chip("Direct3D 12 Compute", kTeal),
                                make_chip("Vulkan SPIR-V", kPurple),
                                make_chip("WebGPU WGSL", kAmber),
                                make_chip("16x16 Tile Binning", kGreen),
                                make_chip("Analytical SDF", kCoral),
                                make_chip("Dual Kawase Blur", kBlue),
                                make_chip("Premultiplied Linear Alpha", kTeal),
                                make_chip("Spring Physics", kPurple),
                                make_chip("Reactive Binding", kAmber),
                                make_chip("Homemade Mica", kGreen),
                                make_chip("Zero-RTTI / Zero-Exceptions", kCoral)
                            }
                        }.bind(&RenderWrap::set_alignment, app.layout_wrap_align)
                         .bind(&RenderWrap::set_spacing, app.layout_wrap_spacing)
                         .bind(&RenderWrap::set_run_spacing, app.layout_wrap_spacing)
                    )
                }
            })
        }
    });
}

} // namespace gallery::pages
