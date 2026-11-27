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

const Color kAmber  {0.82f, 0.52f, 0.10f, 0.88f};

const Color kCoral  {0.84f, 0.30f, 0.36f, 0.88f};

const Color kGreen  {0.16f, 0.62f, 0.34f, 0.88f};

} // namespace

// ==================== 8. Visibility 显隐控制独立展示页 ====================
WidgetPtr build_layout_visibility_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Visibility 响应式显隐控制", 22.0f),
            app.make_muted("无需重建控件树，通过绑定 Property<bool> 即可动态切换 Collapse（折叠不占空间）或 Hidden（隐藏绘制与命中但保留原布局占位）。", 12.5f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "Collapse 折叠 vs Hidden 保留占位对比", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Wrap {
                        .spacing = 16.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            ToggleSwitch { .label = "显示 Collapse 项 (隐藏时折叠宽度为 0)" }.bind_two_way(app.layout_vis_collapse),
                            ToggleSwitch { .label = "显示 Hidden 项 (隐藏时保留占位)" }.bind_two_way(app.layout_vis_hidden)
                        }
                    },
                    make_stage_box(HStack {
                        .spacing = 8.0f,
                        .content = {
                            make_demo_tile("固定项 1", kBlue, 96.0f, 40.0f),
                            Visibility {
                                .visible = app.layout_vis_collapse.get(),
                                .maintain_size = false,
                                .child = make_demo_tile("Collapse 折叠", kCoral, 124.0f, 40.0f)
                            }.bind(&RenderVisibility::set_visible, app.layout_vis_collapse),
                            Visibility {
                                .visible = app.layout_vis_hidden.get(),
                                .maintain_size = true,
                                .child = make_demo_tile("Hidden 占位", kAmber, 124.0f, 40.0f)
                            }.bind(&RenderVisibility::set_visible, app.layout_vis_hidden),
                            make_demo_tile("固定项 2", kGreen, 96.0f, 40.0f)
                        }
                    })
                }
            })
        }
    });
}

} // namespace gallery::pages
