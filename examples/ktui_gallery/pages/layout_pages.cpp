#include "gallery_pages.hpp"
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

// ==================== 1. Flex 弹性布局展示页 ====================
WidgetPtr build_layout_flex_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Flex 弹性布局 (HStack / VStack / Expanded / Flexible / Spacer)", 22.0f),
            Text { .text = "双通道 Flexbox 求解器：支持主轴对齐、交叉轴对齐、反向排布、flex_grow 比例分配、flex_shrink 溢出压缩与 32 项内零堆分配。", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text),

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
                                    Text { .text = "Justify:", .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text),
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
                                    Text { .text = "Align:", .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text),
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
                    Text { .text = "在 360px 固定宽容器内放入总宽 500px 的三个子项：第一项 shrink=0 保持 140px，后两项按 shrink=1 与 shrink=2 比例自动压缩，绝不越界。", .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text),
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

// ==================== 2. Wrap 流式自动换行布局展示页 ====================
WidgetPtr build_layout_wrap_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Wrap 流式自动换行布局", 22.0f),
            Text { .text = "当主轴可用宽度不足时自动折行，适用于标签云、筛选 Chip、响应式工具栏；拖拽窗口边缘即可观察实时重排。", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text),

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

// ==================== 3. Grid 二维网格布局展示页 ====================
WidgetPtr build_layout_grid_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Grid 二维网格布局 (固定列 / 权重轨道 / 响应式自适应 / 跨列)", 22.0f),
            Text { .text = "支持固定列数、CSS fr 比例权重轨道、min_item_width 响应式 auto-fit 列数以及 GridItem 跨列合并。", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text),

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
                    Text { .text = "等价 CSS repeat(auto-fit, minmax(160px, 1fr))，随窗口宽度自动增减每行列数。", .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text),
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
            Text { .text = "ZStack 按声明顺序从底到顶叠加多个图层；Positioned 可把任意子图层独立钉在父容器的四边或四角（例如商品/视频卡片上的左上角标签、右上角未读角标、底部半透明标题条）。", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text),

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
                                        Text { .text = "其余三个浮层由 Positioned 分别锚定在左上、右上和底部全宽", .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text)
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
                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.layout_zstack_align_label).bind(&RenderText::set_color, app.accent_text),
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

// ==================== 5. Align / Center / AspectRatio / Divider 独立展示页 ====================
WidgetPtr build_layout_align_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Align / Center / AspectRatio / Divider", 22.0f),
            Text { .text = "基础单子项几何约束与装饰原语：固定比例缩放容器、单子项对齐包装以及主题自适应横/竖分隔线。", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text),

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

// ==================== 6. SplitView 可拖拽分隔独立展示页 ====================
WidgetPtr build_layout_split_view_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("SplitView 可拖拽分隔面板", 22.0f),
            Text { .text = "支持水平与垂直双栏自由嵌套、悬停高亮 Fluent 把手、ResizeEW/ResizeNS 光标自动切换、最小尺寸保护与双击重置。", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "水平 + 垂直嵌套可拖拽工作区", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.layout_split_label).bind(&RenderText::set_color, app.accent_text),
                    Box {
                        .height = 300.0f,
                        .background_color = Color{0.0f, 0.0f, 0.0f, 0.14f},
                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.10f},
                        .border_width = 1.0f,
                        .corner_radius = 6.0f,
                        .padding = EdgeInsets(6.0f, 6.0f),
                        .child = SplitView {
                            .vertical = false,
                            .ratio = app.layout_split_ratio.get(),
                            .min_first = 110.0f,
                            .min_second = 180.0f,
                            .first = make_demo_tile("左侧导航 / 文件树", kPurple, -1.0f, -1.0f),
                            .second = SplitView {
                                .vertical = true,
                                .ratio = 0.62f,
                                .min_first = 72.0f,
                                .min_second = 64.0f,
                                .first = make_demo_tile("主视图 / 代码编辑器", kBlue, -1.0f, -1.0f),
                                .second = make_demo_tile("底部控制台 / 调试输出", kTeal, -1.0f, -1.0f)
                            },
                            .on_ratio_changed = [&app](float r) {
                                int p1 = static_cast<int>(std::lround(r * 100.0f));
                                app.layout_split_label.set("当前分割比例: " + std::to_string(p1) + "% / " + std::to_string(100 - p1) + "% (拖拽中间分隔条调整，双击重置)");
                            }
                        }.bind_two_way(app.layout_split_ratio)
                    }
                }
            })
        }
    });
}

// ==================== 7. Visual Studio + Fluent 2 可拖拽 DockPanel 独立展示页 ====================
WidgetPtr build_layout_dock_panel_page(GalleryApp& app) {
    auto zone_name = [](DockZone z) -> const char* {
        switch (z) {
            case DockZone::Left:   return "左侧 (Left)";
            case DockZone::Right:  return "右侧 (Right)";
            case DockZone::Top:    return "顶部 (Top)";
            case DockZone::Bottom: return "底部 (Bottom)";
            case DockZone::Center: return "中央工作区 (Center)";
        }
        return "";
    };

    auto make_pane_body = [&app](const std::string& heading, const std::string& line1, const std::string& line2) -> WidgetPtr {
        return Box {
            .padding = EdgeInsets(10.0f, 12.0f),
            .child = VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 6.0f,
                .content = {
                    Text { .text = heading, .font_size = 13.0f, .font_weight = FontWeight::SemiBold },
                    Text { .text = line1, .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text),
                    Text { .text = line2, .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text)
                }
            }
        };
    };

    WidgetPtr dock_widget = DockPanel {
        .panes = {
            DockPane {
                .id = "explorer",
                .title = "解决方案",
                .zone = DockZone::Left,
                .content = make_pane_body("KerneTile.sln", "├─ renderer.cpp", "└─ dock_panel.hpp")
            },
            DockPane {
                .id = "editor",
                .title = "renderer.cpp",
                .zone = DockZone::Center,
                .content = make_pane_body("主代码编辑区", "按住标题栏拖到中心五向指南针即可停靠。", "拖到另一面板标题栏可合并为多标签页。")
            },
            DockPane {
                .id = "preview",
                .title = "设计视图",
                .zone = DockZone::Center,
                .content = make_pane_body("可视化设计器 (同区标签 2)", "点击顶部标签切换，或拖出拆分停靠。", "拖拽相邻分隔条可实时调节面板宽高。"),
                .closable = true
            },
            DockPane {
                .id = "inspector",
                .title = "属性",
                .zone = DockZone::Right,
                .content = make_pane_body("属性检查器", "Theme: Fluent 2", "FPS: VSync"),
                .closable = true
            },
            DockPane {
                .id = "output",
                .title = "输出终端",
                .zone = DockZone::Bottom,
                .content = make_pane_body("构建输出", "[64/64] Linking ktui_gallery.exe", "100% tests passed (36/36)"),
                .closable = true
            }
        },
        .left_width = 185.0f,
        .right_width = 185.0f,
        .top_height = 115.0f,
        .bottom_height = 115.0f,
        .allow_detach_window = true,
        .on_dock_changed = [&app, zone_name](const std::string& id, DockZone z) {
            app.layout_dock_status.set("已将面板 [" + id + "] 停靠至: " + zone_name(z));
        },
        .on_detach_changed = [&app](const std::string& id, bool detached) {
            app.layout_dock_status.set(detached
                ? ("已将面板 [" + id + "] 撕出为独立窗口 (状态完整保留，关闭窗口或点顶部按钮自动停靠回原位)")
                : ("面板 [" + id + "] 已停靠回主工作区"));
        }
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 12.0f,
                .content = {
                    VStack {
                        .spacing = 4.0f,
                        .content = {
                            app.make_page_title("DockPanel (Visual Studio + Fluent 2 拖拽停靠)", 22.0f),
                            Text { .font_size = 12.5f }.bind(&RenderText::set_text, app.layout_dock_status).bind(&RenderText::set_color, app.accent_text)
                        }
                    },
                    Spacer {},
                    Button {
                        .text = "重置默认停靠",
                        .variant = ButtonVariant::Accent,
                        .on_click = [&app, dock_widget]() {
                            if (auto rn = dock_widget->render_node()) {
                                static_cast<RenderDockPanel*>(rn.get())->reset_layout();
                                app.layout_dock_status.set("已恢复默认五区停靠布局与面板尺寸");
                            }
                        }
                    }
                }
            },
            Box {
                .height = 460.0f,
                .child = dock_widget
            }
        }
    });
}

// ==================== 8. Visibility 显隐控制独立展示页 ====================
WidgetPtr build_layout_visibility_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Visibility 响应式显隐控制", 22.0f),
            Text { .text = "无需重建控件树，通过绑定 Property<bool> 即可动态切换 Collapse（折叠不占空间）或 Hidden（隐藏绘制与命中但保留原布局占位）。", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text),

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
