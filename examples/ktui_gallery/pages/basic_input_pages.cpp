#include "gallery_pages.hpp"
#include <cmath>
#include <cstdio>

using namespace ktui;

namespace gallery::pages {

namespace {
WidgetPtr page_header(GalleryApp& app, const std::string& title, const std::string& subtitle) {
    return VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 4.0f,
        .content = {
            Text { .text = title, .font_size = 22.0f },
            Text { .text = subtitle, .font_size = 12.5f, .color = Bind(app.muted_text) }
        }
    };
}
} // namespace

// ==================== Button ====================
WidgetPtr build_button_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Button", "标准、强调色、危险操作及禁用态按钮，支持弹簧按压微动效与焦点键盘激活。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "交互式计数器与按钮变体", .font_size = 14.0f },
                    Text { .text = Bind(app.count_text), .font_size = 15.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "递增计数 (+)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    app.count += app.step_ten.get() ? 10 : 1;
                                    app.refresh_count();
                                },
                                .disabled = Bind(app.locked)
                            },
                            Button {
                                .text = "归零重置",
                                .variant = ButtonVariant::Danger,
                                .on_click = [&app]() {
                                    app.count = 0;
                                    app.refresh_count();
                                },
                                .disabled = Bind(app.locked)
                            },
                            Button {
                                .text = Bind(app.theme_btn_text),
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    app.app->set_theme(app.app->current_theme().is_dark ? Theme::light() : Theme::dark());
                                }
                            },
                            Button {
                                .text = "已禁用按钮",
                                .variant = ButtonVariant::Standard,
                                .disabled = true
                            }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = {
                    Text { .text = "多窗口派生示例", .font_size = 14.0f },
                    Text { .text = "主窗口与子窗口共享响应式状态与主题切换。", .font_size = 12.0f, .color = Bind(app.muted_text) },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "新建独立工具窗口",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() { app.spawn_child_window(); }
                            }
                        }
                    }
                }
            })
        }
    });
}

// ==================== CheckBox ====================
WidgetPtr build_checkbox_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "CheckBox", "WinUI 3 复选框，支持对勾平滑生长动画、悬停态与禁用态。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "基础复选框", .font_size = 14.0f },
                    Checkbox {
                        .checked = Bind(app.step_ten),
                        .label = "每次递增步长设为 +10 (联动 Button 页面计数器)",
                        .on_change = [&app](bool v) {
                            app.step_ten.set(v);
                            app.refresh_count();
                        }
                    },
                    Checkbox {
                        .checked = true,
                        .label = "已选中且禁用",
                        .disabled = true
                    },
                    Checkbox {
                        .checked = false,
                        .label = "未选中且禁用",
                        .disabled = true
                    }
                }
            })
        }
    });
}

// ==================== ToggleSwitch ====================
WidgetPtr build_toggle_switch_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "ToggleSwitch", "WinUI 3 拨动开关，支持物理弹簧滑动胶囊与按压形变。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "状态开关示例", .font_size = 14.0f },
                    ToggleSwitch {
                        .checked = Bind(app.locked),
                        .label = "锁定计数器按钮 (联动 Button 页面)",
                        .on_change = [&app](bool v) {
                            app.locked.set(v);
                            app.refresh_count();
                        }
                    },
                    ToggleSwitch {
                        .checked = true,
                        .label = "开启且禁用",
                        .disabled = true
                    }
                }
            })
        }
    });
}

// ==================== RadioButton ====================
WidgetPtr build_radio_button_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "RadioButton", "经典 WinUI 3 圆形单选按钮组，像素对齐同心圆点与禁用项支持。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "渲染策略单选组", .font_size = 14.0f },
                    RadioGroup {
                        .selected_value = Bind(app.radio_btn_val),
                        .options = {
                            RadioOption { .label = "选项 A — 推荐默认配置", .value = 0 },
                            RadioOption { .label = "选项 B — 高刷新率优先 (120 FPS)", .value = 1 },
                            RadioOption { .label = "选项 C — 极简省电模式", .value = 2 },
                            RadioOption { .label = "选项 D — 已禁用项", .value = 3, .disabled = true }
                        },
                        .spacing = 6.0f
                    }
                }
            })
        }
    });
}

// ==================== RadioCard ====================
WidgetPtr build_radio_card_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "RadioCard", "图标化大卡片单选组件，适合电源模式、主题或布局模板选择。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "性能配置单选卡片组", .font_size = 14.0f },
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
                        .tile_width = 96.0f,
                        .tile_height = 74.0f
                    }
                }
            })
        }
    });
}

// ==================== ComboBox ====================
WidgetPtr build_combobox_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "ComboBox", "WinUI 3 下拉选择框，内置超长列表裁剪、自动向上翻转、高斯软阴影与平滑/触控滚动。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "图形渲染后端选择 (10 项可滚动列表)", .font_size = 14.0f },
                    ComboBox {
                        .items = {
                            ComboBoxItem("Direct3D 12 Compute"),
                            ComboBoxItem("Vulkan 1.3 Compute"),
                            ComboBoxItem("WebGPU (WASM)"),
                            ComboBoxItem("CPU SIMD Tile Rasterizer"),
                            ComboBoxItem("Metal 3 Compute"),
                            ComboBoxItem("OpenGL ES 3.2 (Legacy)", true),
                            ComboBoxItem("Software Fallback (AVX2)"),
                            ComboBoxItem("Headless Test Renderer"),
                            ComboBoxItem("Direct2D Interop"),
                            ComboBoxItem("Remote Tile Stream")
                        },
                        .selected_index = Bind(app.combo_idx),
                        .placeholder = "选择渲染后端...",
                        .width = 240.0f,
                        .max_visible_items = 6,
                        .max_dropdown_height = 200.0f,
                        .on_change = [&app](int, const std::string& label) {
                            app.combo_preview.set("已选后端: " + label);
                        }
                    },
                    Text { .text = Bind(app.combo_preview), .font_size = 12.5f, .color = Bind(app.accent_text) }
                }
            })
        }
    });
}

// ==================== TextBox / TextInput ====================
WidgetPtr build_textbox_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "TextBox", "单行文本输入框，支持完整中文 IME 组合输入、选区高亮、剪贴板与撤销/重做。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "响应式双向绑定文本输入", .font_size = 14.0f },
                    TextInput {
                        .text = Bind(app.input_text),
                        .placeholder = "键入中文或英文...",
                        .on_change = [&app](const std::string& val) {
                            app.input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                        }
                    },
                    Text { .text = Bind(app.input_preview), .color = Bind(app.accent_text) }
                }
            })
        }
    });
}

// ==================== PasswordBox ====================
WidgetPtr build_password_box_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "PasswordBox", "WinUI 3 密码输入框，支持 Peek 按住窥视、Hidden 严格隐藏与掩码态安全禁复制。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "按住窥视模式 (PasswordRevealMode::Peek)", .font_size = 14.0f },
                    PasswordBox {
                        .password = Bind(app.password_val),
                        .placeholder = "输入访问密码...",
                        .header = "访问令牌",
                        .reveal_mode = PasswordRevealMode::Peek,
                        .width = 260.0f,
                        .on_change = [&app](const std::string& v) {
                            app.password_status.set("密码长度: " + std::to_string(v.size()) + " 字符 (按住右侧眼睛可临时预览)");
                        },
                        .on_submit = [&app](const std::string& v) {
                            app.password_status.set("已按 Enter 提交密码 (长度 " + std::to_string(v.size()) + ")");
                        }
                    },
                    Text { .text = Bind(app.password_status), .font_size = 12.0f, .color = Bind(app.muted_text) }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "其他显隐模式对比 (Hidden / Visible)", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            PasswordBox {
                                .password = "Secret123",
                                .header = "严格隐藏 (Hidden)",
                                .reveal_mode = PasswordRevealMode::Hidden,
                                .width = 210.0f
                            },
                            PasswordBox {
                                .password = "VisibleKey",
                                .header = "明文可见 (Visible)",
                                .reveal_mode = PasswordRevealMode::Visible,
                                .width = 210.0f
                            }
                        }
                    }
                }
            })
        }
    });
}

// ==================== NumberBox ====================
WidgetPtr build_number_box_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "NumberBox", "WinUI 3 数值输入框，支持内嵌/紧凑步进按钮、方向键/滚轮微调与算术表达式自动求值。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "Inline 步进与表达式求值", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 16.0f,
                        .content = {
                            NumberBox {
                                .value = Bind(app.number_int_val),
                                .minimum = 0.0,
                                .maximum = 500.0,
                                .small_change = 1.0,
                                .large_change = 10.0,
                                .precision = 0,
                                .accepts_expression = true,
                                .spin_button_placement = NumberBoxSpinButtonPlacementMode::Inline,
                                .header = "整数 [0..500] (可输入 12+8*2)",
                                .width = 220.0f,
                                .on_value_changed = [&app](double v) {
                                    char buf[128];
                                    std::snprintf(buf, sizeof(buf), "当前数值: %.0f / %.2f (支持方向键、PageUp/Down、滚轮与表达式)",
                                                  v, app.number_float_val.get());
                                    app.number_status.set(buf);
                                }
                            },
                            NumberBox {
                                .value = Bind(app.number_float_val),
                                .minimum = -100.0,
                                .maximum = 100.0,
                                .small_change = 0.25,
                                .large_change = 2.0,
                                .precision = 2,
                                .accepts_expression = true,
                                .spin_button_placement = NumberBoxSpinButtonPlacementMode::Compact,
                                .header = "浮点步进 (Compact 悬停显示)",
                                .width = 220.0f,
                                .on_value_changed = [&app](double v) {
                                    char buf[128];
                                    std::snprintf(buf, sizeof(buf), "当前数值: %.0f / %.2f (支持方向键、PageUp/Down、滚轮与表达式)",
                                                  app.number_int_val.get(), v);
                                    app.number_status.set(buf);
                                }
                            }
                        }
                    },
                    Text { .text = Bind(app.number_status), .font_size = 12.0f, .color = Bind(app.muted_text) }
                }
            })
        }
    });
}

} // namespace gallery::pages
