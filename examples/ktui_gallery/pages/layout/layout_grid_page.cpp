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

// ==================== 3. Grid 二维网格布局展示页 ====================
WidgetPtr build_layout_grid_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Grid 二维网格布局 (固定列 / 权重轨道 / 响应式自适应 / 跨列)", 22.0f),
            app.make_muted("支持固定列数、CSS fr 比例权重轨道、min_item_width 响应式 auto-fit 列数以及 GridItem 跨列合并。", 12.5f),

            // 1. 可调列数与间距的等宽网格 + 跨列
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "固定列网格 + GridItem 跨列 (column_span)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .spacing = 12.0f,
                        .content = {
                            SegmentedControl {
                                .items = {
                                    SegmentedItem{ .label = "2 列" },
                                    SegmentedItem{ .label = "3 列" },
                                    SegmentedItem{ .label = "4 列" }
                                },
                                .on_selected_index_changed = [&app](int idx) {
                                    app.layout_grid_cols.set(static_cast<uint32_t>(idx + 2));
                                }
                            }.bind_two_way(app.layout_grid_cols_idx),
                            Spacer {},
                            Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.layout_grid_gap_label).bind(&RenderText::set_color, app.muted_text)
                        }
                    },
                    Slider {
                        .min_value = 4.0f,
                        .max_value = 20.0f,
                        .step = 2.0f,
                        .on_change = [&app](float v) {
                            app.layout_grid_gap_label.set("网格间距: " + std::to_string(static_cast<int>(std::lround(v))) + " px");
                        }
                    }.bind_two_way(app.layout_grid_gap),
                    make_stage_box(
                        Grid {
                            .columns = app.layout_grid_cols.get(),
                            .column_spacing = app.layout_grid_gap.get(),
                            .row_spacing = app.layout_grid_gap.get(),
                            .cell_height = 48.0f,
                            .content = {
                                GridItem { .column_span = 2, .child = make_demo_tile("横跨 2 列 (column_span = 2)", kBlue) },
                                make_demo_tile("单元格 2", kTeal),
                                make_demo_tile("单元格 3", kPurple),
                                make_demo_tile("单元格 4", kAmber),
                                make_demo_tile("单元格 5", kGreen),
                                make_demo_tile("单元格 6", kCoral)
                            }
                        }.bind(&RenderGrid::set_columns, app.layout_grid_cols)
                         .bind(&RenderGrid::set_column_spacing, app.layout_grid_gap)
                         .bind(&RenderGrid::set_row_spacing, app.layout_grid_gap)
                    )
                }
            }),

            // 2. 比例权重轨道 (1fr : 2fr : 1fr)
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = {
                    Text { .text = "比例权重轨道 (column_weights = {1, 2, 1})", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    make_stage_box(Grid {
                        .column_weights = {1.0f, 2.0f, 1.0f},
                        .column_spacing = 8.0f,
                        .row_spacing = 8.0f,
                        .cell_height = 52.0f,
                        .content = {
                            make_demo_tile("侧边栏 (1fr)", kPurple),
                            make_demo_tile("主工作区 (2fr)", kBlue),
                            make_demo_tile("属性检查器 (1fr)", kTeal)
                        }
                    })
                }
            }),

            // 3. 响应式 auto-fit 网格 (min_item_width)
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = {
                    Text { .text = "响应式自适应网格 (min_item_width = 160px)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    app.make_muted("等价 CSS repeat(auto-fit, minmax(160px, 1fr))，随窗口宽度自动增减每行列数。", 12.0f),
                    make_stage_box(Grid {
                        .min_item_width = 160.0f,
                        .column_spacing = 8.0f,
                        .row_spacing = 8.0f,
                        .cell_height = 56.0f,
                        .content = {
                            make_demo_tile("GPU 帧耗时: 0.42ms", kBlue),
                            make_demo_tile("脏瓦片剔除: 99.9%", kGreen),
                            make_demo_tile("活跃图集: 2048px", kTeal),
                            make_demo_tile("并发帧槽: 2 In-Flight", kPurple),
                            make_demo_tile("布局求解: 零堆分配", kAmber),
                            make_demo_tile("刷新率: 原生 VSync", kCoral)
                        }
                    })
                }
            })
        }
    });
}

} // namespace gallery::pages
