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
            app.make_page_title(title, 22.0f),
            Text { .text = subtitle, .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
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
                    Text { .font_size = 15.0f }.bind(&RenderText::set_text, app.count_text),
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Tooltip {
                                .text = "按步长递增计数 (支持 Tab 焦点与 Space/Enter 激活)",
                                .child = Button {
                                    .text = "递增计数 (+)",
                                    .variant = ButtonVariant::Accent,
                                    .on_click = [&app]() {
                                        app.count += app.step_ten.get() ? 10 : 1;
                                        app.refresh_count();
                                    }
                                }.bind(&RenderButton::set_disabled, app.locked)
                            },
                            Tooltip {
                                .text = "将当前计数清零重置",
                                .child = Button {
                                    .text = "归零重置",
                                    .variant = ButtonVariant::Danger,
                                    .on_click = [&app]() {
                                        app.count = 0;
                                        app.refresh_count();
                                    }
                                }.bind(&RenderButton::set_disabled, app.locked)
                            },
                            Button {
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    app.app->set_theme(app.app->current_theme().is_dark ? Theme::light() : Theme::dark());
                                }
                            }.bind(&RenderButton::set_text, app.theme_btn_text),
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
                    app.make_muted("主窗口与子窗口共享响应式状态与主题切换。", 12.0f),
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
                        .label = "每次递增步长设为 +10 (联动 Button 页面计数器)",
                        .on_change = [&app](bool v) {
                            app.step_ten.set(v);
                            app.refresh_count();
                        }
                    }.bind_two_way(app.step_ten),
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
                        .label = "锁定计数器按钮 (联动 Button 页面)",
                        .on_change = [&app](bool v) {
                            app.locked.set(v);
                            app.refresh_count();
                        }
                    }.bind_two_way(app.locked),
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
                        .options = {
                            RadioOption { .label = "选项 A — 推荐默认配置", .value = 0 },
                            RadioOption { .label = "选项 B — 高刷新率优先 (120 FPS)", .value = 1 },
                            RadioOption { .label = "选项 C — 极简省电模式", .value = 2 },
                            RadioOption { .label = "选项 D — 已禁用项", .value = 3, .disabled = true }
                        },
                        .spacing = 6.0f
                    }.bind_two_way(app.radio_btn_val)
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
                    }.bind_two_way(app.radio_card_val)
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
                        .placeholder = "选择渲染后端...",
                        .width = 240.0f,
                        .max_visible_items = 6,
                        .max_dropdown_height = 200.0f,
                        .on_change = [&app](int, const std::string& label) {
                            app.combo_preview.set("已选后端: " + label);
                        }
                    }.bind_two_way(app.combo_idx),
                    app.make_accent(app.combo_preview, 12.5f)
                }
            })
        }
    });
}

// ==================== Label ====================
WidgetPtr build_label_page(GalleryApp& app) {
    static constexpr const char* kTagSvg =
        "M12 2.25c.3 0 .59.07.86.2l6.5 3.1c.54.26.89.8.89 1.4v4.73c0 4.65-3.07 8.79-7.53 10.12a2.5 2.5 0 0 1-1.44 0C6.82 20.47 3.75 16.33 3.75 11.68V6.95c0-.6.35-1.14.89-1.4l6.5-3.1c.27-.13.56-.2.86-.2Z";

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Label", "WinUI 3 / Fluent 2 语义标签与富文本排版组件，支持多级字重、斜体、下划线、删除线、必填星号 (*)、前置 SVG 图标与语义胶囊。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "字体字重、斜体与文本装饰 (Bold / Italic / Underline / Strikethrough)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 16.0f,
                        .content = {
                            Label { .text = "细体 Light (300)", .font_weight = FontWeight::Light },
                            Label { .text = "常规 Regular (400)", .font_weight = FontWeight::Regular },
                            Label { .text = "半粗 SemiBold (600)", .font_weight = FontWeight::SemiBold },
                            Label { .text = "粗体 Bold (700)", .bold = true }
                        }
                    },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 16.0f,
                        .content = {
                            Label { .text = "斜体 Italic 文本", .italic = true },
                            Label { .text = "粗斜体 Bold + Italic", .bold = true, .italic = true },
                            Label { .text = "下划线 Underline", .variant = LabelVariant::Accent, .underline = true },
                            Label { .text = "删除线 Strikethrough", .variant = LabelVariant::Caption, .strikethrough = true }
                        }
                    },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 16.0f,
                        .content = {
                            Label { .text = "原价 ¥299.00", .variant = LabelVariant::Caption, .strikethrough = true },
                            Label { .text = "限时特惠 ¥149.00", .variant = LabelVariant::Error, .bold = true },
                            Label { .text = "组合装饰 (粗体+斜体+下划线)", .variant = LabelVariant::Success, .bold = true, .italic = true, .underline = true },
                            Label { .text = "下划线+删除线", .underline = true, .strikethrough = true }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "WinUI 3 语义字阶与表单必填标签", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Label { .text = "Display 展示大标题 (26px Bold)", .variant = LabelVariant::Title, .font_size = 26.0f, .bold = true },
                    Label { .text = "Title 标题标签 (20px SemiBold)", .variant = LabelVariant::Title },
                    Label { .text = "Subtitle 副标题标签 (16px SemiBold)", .variant = LabelVariant::Subtitle },
                    Label { .text = "Body 正文标签 (13.5px Regular)", .variant = LabelVariant::Body },
                    Label { .text = "Caption 辅助说明标签 (12px)", .variant = LabelVariant::Caption },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            Label { .text = "用户名", .variant = LabelVariant::Body, .required = true },
                            Label { .text = "安全校验已通过", .variant = LabelVariant::Success, .svg_path = kTagSvg },
                            Label { .text = "只读禁用标签", .variant = LabelVariant::Body, .disabled = true }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "Fluent 2 语义胶囊标签 (pill = true)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Label { .text = "默认标签", .variant = LabelVariant::Body, .pill = true },
                            Label { .text = "强调 Accent", .variant = LabelVariant::Accent, .pill = true, .svg_path = kTagSvg },
                            Label { .text = "已就绪 Success", .variant = LabelVariant::Success, .pill = true },
                            Label { .text = "高刷新 Warning", .variant = LabelVariant::Warning, .pill = true },
                            Label { .text = "异常 Error", .variant = LabelVariant::Error, .pill = true }
                        }
                    }
                }
            })
        }
    });
}

// ==================== RichTextBlock ====================
WidgetPtr build_rich_text_block_page(GalleryApp& app) {
    auto link_status = std::make_shared<Property<std::string>>("点击下方富文本中的蓝色超链接，或直接拖动鼠标跨段划选并按 Ctrl+C 复制。");

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "RichTextBlock", "WinUI 3 富文本排版控件：支持多段落混排、多字号基线对齐、粗体/斜体/下划线/删除线/荧光高亮、内联代码胶囊、可点击超链接与拖拽选区复制。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "混排文档、内联代码与交互式超链接", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    RichTextBlock {
                        .paragraphs = {
                            RichTextParagraph{
                                .runs = {
                                    RichTextRun{ .text = "KerneTile ", .font_size = 18.0f, .bold = true },
                                    RichTextRun{ .text = "是一套基于 " },
                                    RichTextRun{ .text = "16×16 Tile Binning", .is_code = true },
                                    RichTextRun{ .text = " 与 " },
                                    RichTextRun{ .text = "GPU Compute Shader", .bold = true, .color = Color{0.38f, 0.80f, 1.0f, 1.0f} },
                                    RichTextRun{ .text = " 的极简跨平台 2D UI 引擎，支持 " },
                                    RichTextRun{ .text = "Direct3D 12", .link_url = "d3d12", .on_click = [link_status]() {
                                        link_status->set("已点击超链接: Direct3D 12 Compute 后端 (Windows 11 默认)");
                                    }},
                                    RichTextRun{ .text = "、" },
                                    RichTextRun{ .text = "Vulkan 1.3", .link_url = "vulkan", .on_click = [link_status]() {
                                        link_status->set("已点击超链接: Vulkan 1.3 Compute 后端 (Linux / Android)");
                                    }},
                                    RichTextRun{ .text = " 与 " },
                                    RichTextRun{ .text = "WebGPU / WASM", .link_url = "wgpu", .on_click = [link_status]() {
                                        link_status->set("已点击超链接: WebGPU / WASM 单文件浏览器后端");
                                    }},
                                    RichTextRun{ .text = "。" }
                                },
                                .spacing_after = 10.0f
                            },
                            RichTextParagraph{
                                .runs = {
                                    RichTextRun{ .text = "支持丰富的行内修饰：" },
                                    RichTextRun{ .text = "粗体强调", .bold = true },
                                    RichTextRun{ .text = "、" },
                                    RichTextRun{ .text = "斜体引述", .italic = true },
                                    RichTextRun{ .text = "、" },
                                    RichTextRun{ .text = "下划线标注", .underline = true },
                                    RichTextRun{ .text = "、" },
                                    RichTextRun{ .text = "已废弃接口", .strikethrough = true, .color = Color{0.62f, 0.62f, 0.66f, 1.0f} },
                                    RichTextRun{ .text = "、以及 " },
                                    RichTextRun{ .text = "荧光重点高亮", .bold = true, .color = Color{0.10f, 0.10f, 0.12f, 1.0f}, .highlight = Color{1.0f, 0.86f, 0.30f, 0.92f} },
                                    RichTextRun{ .text = " 和内联代码 " },
                                    RichTextRun{ .text = "RichTextBlock { .paragraphs = ... }", .is_code = true },
                                    RichTextRun{ .text = "。" }
                                },
                                .spacing_after = 6.0f
                            }
                        },
                        .font_size = 14.0f,
                        .line_height = 1.55f,
                        .is_text_selection_enabled = true
                    },
                    Text { .font_size = 12.5f }.bind(&RenderText::set_text, *link_status).bind(&RenderText::set_color, app.accent_text)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "单段快捷写法 (.runs = {...}) 与只读不可选模式", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    RichTextBlock {
                        .runs = {
                            RichTextRun{ .text = "版本状态：", .font_weight = FontWeight::SemiBold },
                            RichTextRun{ .text = "v0.0.2 已发布 ", .bold = true, .color = Color{0.25f, 0.82f, 0.45f, 1.0f} },
                            RichTextRun{ .text = "— 完整 40+ WinUI 3 控件库编译为单文件 " },
                            RichTextRun{ .text = "ktui_gallery.exe", .is_code = true },
                            RichTextRun{ .text = " 体积仅约 " },
                            RichTextRun{ .text = "1.0 MB", .bold = true, .underline = true },
                            RichTextRun{ .text = "（零外部运行库依赖）。" }
                        },
                        .font_size = 13.5f,
                        .is_text_selection_enabled = false
                    }
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
                        .placeholder = "键入中文或英文...",
                        .on_change = [&app](const std::string& val) {
                            app.input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                        }
                    }.bind_two_way(app.input_text),
                    Text {}.bind(&RenderText::set_text, app.input_preview).bind(&RenderText::set_color, app.accent_text)
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
                    }.bind_two_way(app.password_val),
                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.password_status).bind(&RenderText::set_color, app.muted_text)
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
                            }.bind_two_way(app.number_int_val),
                            NumberBox {
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
                            }.bind_two_way(app.number_float_val)
                        }
                    },
                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.number_status).bind(&RenderText::set_color, app.muted_text)
                }
            })
        }
    });
}

} // namespace gallery::pages
