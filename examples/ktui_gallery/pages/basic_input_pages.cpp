#include "gallery_pages.hpp"
#include <cmath>

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_buttons_card(GalleryApp& app) {
    return app.make_card(VStack {
        .spacing = 10.0f,
        .content = {
            Text { .text = Bind(app.count_text), .font_size = 14.0f },
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 8.0f,
                .content = {
                    Button {
                        .text = "+ 增加",
                        .variant = ButtonVariant::Accent,
                        .on_click = [&app]() { app.count += app.step_ten.get() ? 10 : 1; app.refresh_count(); },
                        .disabled = Bind(app.locked)
                    },
                    Button {
                        .text = "- 减少",
                        .on_click = [&app]() { app.count -= app.step_ten.get() ? 10 : 1; app.refresh_count(); },
                        .disabled = Bind(app.locked)
                    },
                    Button {
                        .text = "重置",
                        .variant = ButtonVariant::Danger,
                        .on_click = [&app]() { app.count = 0; app.refresh_count(); },
                        .disabled = Bind(app.locked)
                    }
#if !defined(__ANDROID__)
                    ,
                    Button {
                        .text = "+ 新建窗口",
                        .on_click = [&app]() { app.spawn_child_window(); }
                    }
#endif
                }
            }
        }
    });
}

WidgetPtr build_toggles_card(GalleryApp& app) {
    return app.make_card(HStack {
        .align_items = AlignItems::Center,
        .spacing = 20.0f,
        .content = {
            Checkbox { .checked = Bind(app.step_ten), .label = "步长 10 (+10)", .on_change = [&app](bool) { app.refresh_count(); } },
            ToggleSwitch { .checked = Bind(app.locked), .label = "锁定按钮", .on_change = [&app](bool) { app.refresh_count(); } }
        }
    });
}

WidgetPtr build_combobox_card(GalleryApp& app) {
    return app.make_card(HStack {
        .align_items = AlignItems::Center,
        .spacing = 12.0f,
        .content = {
            ComboBox {
                .items = {
                    "Direct3D 12 Compute",
                    "Vulkan Compute",
                    "WebGPU (WASM)",
                    "CPU SIMD Rasterizer",
                    ComboBoxItem{"Software WARP (已禁用)", true},
                    "Metal (macOS / iOS)",
                    "OpenGL ES 3.1 Compute",
                    "D3D12 + Mica Backdrop",
                    "Vulkan + Subpass Tile",
                    "Headless Offscreen",
                    "4K HiDPI 200% Scale",
                    "8K Ultra Tile Grid",
                    "Low-Power Integrated",
                    "High-Performance Discrete",
                    "Custom Render Graph"
                },
                .selected_index = Bind(app.combo_idx),
                .placeholder = "选择渲染后端...",
                .width = 210.0f,
                .max_visible_items = 6,
                .max_dropdown_height = 200.0f,
                .on_change = [&app](int, const std::string& label) {
                    app.combo_preview.set("已选后端: " + label);
                }
            },
            Text { .text = Bind(app.combo_preview), .font_size = 12.0f, .color = Bind(app.muted_text) }
        }
    });
}

WidgetPtr build_radio_button_card(GalleryApp& app) {
    return app.make_card(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 10.0f,
        .content = {
            Text { .text = "单选按钮 (WinUI 3 RadioButton)", .font_size = 14.0f },
            RadioGroup {
                .selected_value = Bind(app.radio_btn_val),
                .options = {
                    RadioOption { .label = "选项 A — 推荐默认配置", .value = 0 },
                    RadioOption { .label = "选项 B — 高刷新率优先", .value = 1 },
                    RadioOption { .label = "选项 C — 极简省电模式", .value = 2 },
                    RadioOption { .label = "选项 D — 已禁用项", .value = 3, .disabled = true }
                },
                .spacing = 4.0f
            }
        }
    });
}

WidgetPtr build_radio_card_group_card(GalleryApp& app) {
    return app.make_card(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 10.0f,
        .content = {
            Text { .text = "单选卡片 (RadioCardGroup)", .font_size = 14.0f },
            RadioCardGroup {
                .selected_value = Bind(app.radio_card_val),
                .options = {
                    RadioCardOption {
                        .label = "节能",
                        .value = 0,
                        .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                            float cx = std::round(r.min_x + r.width() * 0.5f);
                            float cy = std::round(r.min_y + r.height() * 0.5f);
                            ctx.draw_rect(Rect::from_xywh(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f), col, 7.0f, 1.6f);
                            ctx.draw_line(PointF{cx, cy - 3.5f}, PointF{cx, cy + 3.5f}, col, 1.6f);
                        }
                    },
                    RadioCardOption {
                        .label = "均衡",
                        .value = 1,
                        .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                            float cx = std::round(r.min_x + r.width() * 0.5f);
                            float cy = std::round(r.min_y + r.height() * 0.5f);
                            ctx.draw_rect(Rect::from_xywh(cx - 8.0f, cy - 5.0f, 16.0f, 10.0f), col, 3.0f, 1.6f);
                            ctx.draw_rect(Rect::from_xywh(cx - 3.0f, cy - 2.0f, 6.0f, 4.0f), col, 1.0f, 0.0f);
                        }
                    },
                    RadioCardOption {
                        .label = "性能",
                        .value = 2,
                        .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                            float cx = std::round(r.min_x + r.width() * 0.5f);
                            float cy = std::round(r.min_y + r.height() * 0.5f);
                            ctx.draw_line(PointF{cx + 1.5f, cy - 8.0f}, PointF{cx - 4.5f, cy + 0.5f}, col, 1.8f);
                            ctx.draw_line(PointF{cx - 4.5f, cy + 0.5f}, PointF{cx + 1.0f, cy + 0.5f}, col, 1.8f);
                            ctx.draw_line(PointF{cx + 1.0f, cy + 0.5f}, PointF{cx - 1.5f, cy + 8.0f}, col, 1.8f);
                        }
                    }
                },
                .tile_width = 86.0f,
                .tile_height = 68.0f
            }
        }
    });
}

WidgetPtr build_text_input_card(GalleryApp& app) {
    return app.make_card(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 8.0f,
        .content = {
            Text { .text = "单行文本输入 (支持撤销/重做、双向绑定):", .color = Bind(app.muted_text) },
            TextInput {
                .text = Bind(app.input_text),
                .placeholder = "键入中文或英文...",
                .on_change = [&app](const std::string& val) {
                    app.input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                }
            },
            Text { .text = Bind(app.input_preview), .color = Bind(app.accent_text) }
        }
    });
}

WidgetPtr build_buttons_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "按钮与开关", .font_size = 20.0f },
            build_buttons_card(app),
            build_toggles_card(app)
        }
    });
}

WidgetPtr build_inputs_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "文本输入", .font_size = 20.0f },
            build_text_input_card(app)
        }
    });
}

WidgetPtr build_selection_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Text { .text = "下拉选择框、单选按钮与单选卡片", .font_size = 20.0f },
            build_combobox_card(app),
            build_radio_button_card(app),
            build_radio_card_group_card(app)
        }
    });
}

} // namespace gallery::pages
