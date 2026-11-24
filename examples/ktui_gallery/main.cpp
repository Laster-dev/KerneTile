#include <ktui/ktui.hpp>
#include <clocale>
#include <memory>
#include <string>
#include <vector>

#if defined(__ANDROID__)
#include <pal/android_backend.hpp>
#include <android/native_window_jni.h>
#include <jni.h>

namespace kernetile::text {
void android_init_text_bridge(JNIEnv* env, jclass bridge_class);
}
#endif

using namespace ktui;

namespace {

struct GalleryApp {
    Application* app{nullptr};
    std::shared_ptr<Window> window;
    int count{0};
    int child_window_seq{1};
    std::vector<std::shared_ptr<Property<std::string>>> child_notes;

    Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    Property<bool> step_ten{false};
    Property<bool> locked{false};
    Property<int> radio_btn_val{1};
    Property<int> radio_card_val{1};
    Property<std::string> input_text{"KerneTile KTUI Gallery"};
    Property<std::string> input_preview{"输入预览: KerneTile KTUI Gallery"};
    Property<std::string> theme_btn_text{"切换浅色主题"};
    Property<float> slider_val{45.0f};
    Property<float> progress_norm{0.45f};
    Property<std::string> slider_text{"调节数值 (滑块): 45%"};
    Property<int> combo_idx{0};
    Property<std::string> combo_preview{"已选后端: Direct3D 12 Compute"};
    Property<std::string> nav_selected{"home"};
    Property<NavigationPaneDisplayMode> nav_mode{NavigationPaneDisplayMode::Auto};
    Property<int> nav_mode_idx{0};

    Property<Color> window_bg{Color::transparent()};
    Property<Color> card_bg{Color{1.0f, 1.0f, 1.0f, 0.045f}};
    Property<Color> card_border{Color{1.0f, 1.0f, 1.0f, 0.07f}};
    Property<Color> muted_text{Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f}};
    Property<Color> accent_text{Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}};

    void refresh_count() {
        int step = step_ten.get() ? 10 : 1;
        count_text.set("当前计数: " + std::to_string(count) + "   (步长: " + std::to_string(step) + ")" +
                       (locked.get() ? "  [已锁定]" : ""));
    }

    void setup(Application& application, std::shared_ptr<Window> win) {
        app = &application;
        window = std::move(win);

        app->theme.subscribe([this](const Theme& t) {
            if (t.is_dark) {
#if defined(__ANDROID__)
                window_bg.set(Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f});
                card_bg.set(Color{0x27 / 255.0f, 0x28 / 255.0f, 0x2C / 255.0f, 1.0f});
                card_border.set(Color{1.0f, 1.0f, 1.0f, 0.06f});
#else
                window_bg.set(Color::transparent());
                card_bg.set(Color{1.0f, 1.0f, 1.0f, 0.045f});
                card_border.set(Color{1.0f, 1.0f, 1.0f, 0.07f});
#endif
                muted_text.set(Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f});
                accent_text.set(Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f});
                theme_btn_text.set("切换浅色主题");
            } else {
#if defined(__ANDROID__)
                window_bg.set(Color{0xEA / 255.0f, 0xED / 255.0f, 0xF1 / 255.0f, 1.0f});
                card_bg.set(Color{0xFA / 255.0f, 0xFB / 255.0f, 0xFD / 255.0f, 1.0f});
                card_border.set(Color{0.0f, 0.0f, 0.0f, 0.07f});
#else
                window_bg.set(Color::transparent());
                card_bg.set(Color{1.0f, 1.0f, 1.0f, 0.65f});
                card_border.set(Color{0.0f, 0.0f, 0.0f, 0.07f});
#endif
                muted_text.set(Color{0x59 / 255.0f, 0x60 / 255.0f, 0x6A / 255.0f, 1.0f});
                accent_text.set(Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f});
                theme_btn_text.set("切换深色主题");
            }
        });

        auto card = [this](WidgetPtr child) {
            return Box {
                .background_color = Bind(card_bg),
                .border_color = Bind(card_border),
                .border_width = 1.0f,
                .corner_radius = 5.0f,
                .padding = EdgeInsets(12.0f, 14.0f),
                .child = std::move(child)
            };
        };

        auto spawn_child_window = [this, card]() {
            int idx = child_window_seq++;
            WindowDesc child_desc;
            child_desc.title = "工具窗口 #" + std::to_string(idx);
            child_desc.size = Vec2{360.0f, 240.0f};
            child_desc.min_size = Vec2{260.0f, 180.0f};
            child_desc.resizable = true;

            auto child_win = app->create_window(child_desc);
            if (!child_win) return;

            auto note = std::make_shared<Property<std::string>>("窗口 #" + std::to_string(idx) + " 独立输入");
            child_notes.push_back(note);
            child_win->set_content(Box {
                .background_color = Bind(window_bg),
                .padding = EdgeInsets(14.0f, 16.0f),
                .child = VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 10.0f,
                    .content = {
                        card(VStack {
                            .align_items = AlignItems::Stretch,
                            .spacing = 8.0f,
                            .content = {
                                Text { .text = "独立无边框子窗口 (共享全局计数绑定)", .color = Bind(muted_text) },
                                Text { .text = Bind(count_text), .font_size = 14.0f },
                                HStack {
                                    .align_items = AlignItems::Center,
                                    .spacing = 8.0f,
                                    .content = {
                                        Button {
                                            .text = "+1",
                                            .variant = ButtonVariant::Accent,
                                            .on_click = [this]() { count += 1; refresh_count(); }
                                        },
                                        Button {
                                            .text = "-1",
                                            .on_click = [this]() { count -= 1; refresh_count(); }
                                        }
                                    }
                                }
                            }
                        }),
                        TextInput {
                            .text = Bind(*note),
                            .placeholder = "在此窗口独立输入..."
                        }
                    }
                }
            });
            child_win->show();
        };

#if !defined(__ANDROID__)
        // 在标题栏右侧添加自定义操作按钮 (位于置顶 Pin 按钮左侧，对齐 CUI.Core)
        window->titlebar().add_action("+", "新建工具窗口", spawn_child_window);
#endif

        auto make_page = [this](WidgetPtr body) -> WidgetPtr {
            return ScrollView {
                .child = Box {
                    .background_color = Color::transparent(),
                    .padding = EdgeInsets(16.0f, 22.0f),
                    .child = std::move(body)
                }
            };
        };

        auto buttons_card = [this, card, spawn_child_window]() -> WidgetPtr {
            return card(VStack {
                .spacing = 10.0f,
                .content = {
                    Text { .text = Bind(count_text), .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "+ 增加",
                                .variant = ButtonVariant::Accent,
                                .on_click = [this]() { count += step_ten.get() ? 10 : 1; refresh_count(); },
                                .disabled = Bind(locked)
                            },
                            Button {
                                .text = "- 减少",
                                .on_click = [this]() { count -= step_ten.get() ? 10 : 1; refresh_count(); },
                                .disabled = Bind(locked)
                            },
                            Button {
                                .text = "重置",
                                .variant = ButtonVariant::Danger,
                                .on_click = [this]() { count = 0; refresh_count(); },
                                .disabled = Bind(locked)
                            }
#if !defined(__ANDROID__)
                            ,
                            Button {
                                .text = "+ 新建窗口",
                                .on_click = spawn_child_window
                            }
#endif
                        }
                    }
                }
            });
        };

        auto toggles_card = [this, card]() -> WidgetPtr {
            return card(HStack {
                .align_items = AlignItems::Center,
                .spacing = 20.0f,
                .content = {
                    Checkbox { .checked = Bind(step_ten), .label = "步长 10 (+10)", .on_change = [this](bool) { refresh_count(); } },
                    ToggleSwitch { .checked = Bind(locked), .label = "锁定按钮", .on_change = [this](bool) { refresh_count(); } }
                }
            });
        };

        auto combobox_card = [this, card]() -> WidgetPtr {
            return card(HStack {
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
                        .selected_index = Bind(combo_idx),
                        .placeholder = "选择渲染后端...",
                        .width = 210.0f,
                        .max_visible_items = 6,
                        .max_dropdown_height = 200.0f,
                        .on_change = [this](int, const std::string& label) {
                            combo_preview.set("已选后端: " + label);
                        }
                    },
                    Text { .text = Bind(combo_preview), .font_size = 12.0f, .color = Bind(muted_text) }
                }
            });
        };

        auto radio_button_card = [this, card]() -> WidgetPtr {
            return card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "单选按钮 (WinUI 3 RadioButton)", .font_size = 14.0f },
                    RadioGroup {
                        .selected_value = Bind(radio_btn_val),
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
        };

        auto radio_card_group_card = [this, card]() -> WidgetPtr {
            return card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "单选卡片 (RadioCardGroup)", .font_size = 14.0f },
                    RadioCardGroup {
                        .selected_value = Bind(radio_card_val),
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
        };

        auto slider_progress_card = [this, card]() -> WidgetPtr {
            return card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 16.0f,
                        .content = {
                            Text { .text = Bind(slider_text), .color = Bind(muted_text) },
                            HStack {
                                .align_items = AlignItems::Center,
                                .spacing = 8.0f,
                                .content = {
                                    Text { .text = "旋转加载:", .font_size = 11.5f, .color = Bind(muted_text) },
                                    ProgressRing { .indeterminate = true, .size = 18.0f, .stroke_width = 2.5f }
                                }
                            }
                        }
                    },
                    Slider {
                        .value = Bind(slider_val),
                        .min_value = 0.0f,
                        .max_value = 100.0f,
                        .step = 1.0f,
                        .on_change = [this](float v) {
                            slider_val.set(v);
                            slider_text.set("调节数值 (滑块): " + std::to_string(static_cast<int>(std::round(v))) + "%");
                            progress_norm.set(v / 100.0f);
                        }
                    },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Text { .text = "同步进度:", .font_size = 11.5f, .color = Bind(muted_text) },
                            ProgressBar { .value = Bind(progress_norm), .width = 120.0f },
                            ProgressRing { .value = Bind(progress_norm), .indeterminate = false, .size = 18.0f, .stroke_width = 2.5f },
                            Text { .text = "流光:", .font_size = 11.5f, .color = Bind(muted_text) },
                            ProgressBar { .indeterminate = true, .width = 96.0f }
                        }
                    }
                }
            });
        };

        auto progress_ring_card = [this, card]() -> WidgetPtr {
            return card(VStack {
                .align_items = AlignItems::Center,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "WinUI 3 官方规范 ProgressRing (带 Round Cap 药丸圆头):", .font_size = 12.0f, .color = Bind(muted_text) },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 28.0f,
                        .content = {
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    ProgressRing { .indeterminate = true, .size = 18.0f },
                                    Text { .text = "18px 紧凑", .font_size = 11.0f, .color = Bind(muted_text) }
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    ProgressRing { .indeterminate = true, .size = 32.0f },
                                    Text { .text = "32px 标准", .font_size = 11.0f, .color = Bind(muted_text) }
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    ProgressRing { .indeterminate = true, .size = 54.0f },
                                    Text { .text = "54px 大号", .font_size = 11.0f, .color = Bind(muted_text) }
                                }
                            }
                        }
                    }
                }
            });
        };

        auto text_input_card = [this, card]() -> WidgetPtr {
            return card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = {
                    Text { .text = "单行文本输入 (支持撤销/重做、双向绑定):", .color = Bind(muted_text) },
                    TextInput {
                        .text = Bind(input_text),
                        .placeholder = "键入中文或英文...",
                        .on_change = [this](const std::string& val) {
                            input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                        }
                    },
                    Text { .text = Bind(input_preview), .color = Bind(accent_text) }
                }
            });
        };

        window->set_content(NavigationView {
            .menu_items = {
                NavigationViewItem{ .id = "home", .label = "全部控件概览", .icon = NavigationIcon::Home },
                NavigationViewItem::header("基础与输入"),
                NavigationViewItem{
                    .id = "controls", .label = "基础交互", .icon = NavigationIcon::Controls, .badge = "2", .expanded = true,
                    .children = {
                        NavigationViewItem{ .id = "buttons", .label = "按钮与开关", .icon = NavigationIcon::Sparkles },
                        NavigationViewItem{ .id = "inputs",  .label = "文本输入",   .icon = NavigationIcon::Inputs }
                    }
                },
                NavigationViewItem{ .id = "selection", .label = "下拉与单选", .icon = NavigationIcon::ComboBox, .badge = "New" },
                NavigationViewItem::separator(),
                NavigationViewItem::header("反馈与状态"),
                NavigationViewItem{ .id = "progress", .label = "滑块与进度", .icon = NavigationIcon::Progress }
            },
            .footer_items = {
                NavigationViewItem{ .id = "about", .label = "关于 KerneTile", .icon = NavigationIcon::Info }
            },
            .selected_id = Bind(nav_selected),
            .pane_title = "KTUI Gallery",
            .pane_display_mode = Bind(nav_mode),
            .is_pane_open = true,
            .is_pane_toggle_button_visible = true,
            .is_back_button_visible = true,
            .is_settings_visible = true,
            .open_pane_length = 228.0f,
            .page_builder = [=](const std::string& page_id) -> WidgetPtr {
                if (page_id == "buttons") {
                    return make_page(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 12.0f,
                        .content = {
                            Text { .text = "按钮与开关", .font_size = 20.0f },
                            buttons_card(),
                            toggles_card()
                        }
                    });
                }
                if (page_id == "inputs") {
                    return make_page(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 12.0f,
                        .content = {
                            Text { .text = "文本输入", .font_size = 20.0f },
                            text_input_card()
                        }
                    });
                }
                if (page_id == "selection") {
                    return make_page(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 12.0f,
                        .content = {
                            Text { .text = "下拉选择框、单选按钮与单选卡片", .font_size = 20.0f },
                            combobox_card(),
                            radio_button_card(),
                            radio_card_group_card()
                        }
                    });
                }
                if (page_id == "progress") {
                    return make_page(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 12.0f,
                        .content = {
                            Text { .text = "WinUI 3 滑块与进度指示", .font_size = 20.0f },
                            slider_progress_card(),
                            progress_ring_card()
                        }
                    });
                }
                if (page_id == "__settings__") {
                    return make_page(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 12.0f,
                        .content = {
                            Text { .text = "设置", .font_size = 20.0f },
                            card(VStack {
                                .align_items = AlignItems::Stretch,
                                .spacing = 10.0f,
                                .content = {
                                    Text { .text = "外观主题", .font_size = 14.0f },
                                    HStack {
                                        .align_items = AlignItems::Center,
                                        .spacing = 10.0f,
                                        .content = {
                                            Button {
                                                .text = Bind(theme_btn_text),
                                                .variant = ButtonVariant::Accent,
                                                .on_click = [this]() {
                                                    app->set_theme(app->current_theme().is_dark ? Theme::light() : Theme::dark());
                                                }
                                            }
                                        }
                                    }
                                }
                            }),
                            card(VStack {
                                .align_items = AlignItems::Stretch,
                                .spacing = 10.0f,
                                .content = {
                                    Text { .text = "NavigationView 导航栏布局模式 (PaneDisplayMode)", .font_size = 14.0f },
                                    ComboBox {
                                        .items = {"Auto (响应式自动)", "Left (左侧固定)", "LeftCompact (左侧紧凑 48px)", "LeftMinimal (极简浮层)", "Top (顶部水平导航)"},
                                        .selected_index = Bind(nav_mode_idx),
                                        .width = 240.0f,
                                        .on_change = [this](int idx, const std::string&) {
                                            nav_mode.set(static_cast<NavigationPaneDisplayMode>(idx));
                                        }
                                    }
                                }
                            })
                        }
                    });
                }
                if (page_id == "about") {
                    return make_page(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 12.0f,
                        .content = {
                            Text { .text = "关于 KerneTile KTUI", .font_size = 20.0f },
                            card(VStack {
                                .spacing = 6.0f,
                                .content = {
                                    Text { .text = "KerneTile 2D 瓦片计算渲染引擎 + C++20 声明式桌面 UI 框架", .font_size = 13.5f },
                                    Text { .text = "控件视觉全面对标 Windows 11 / WinUI 3 Fluent 2，平滑滚动对齐 Chromium 原版 Impulse 算法。", .font_size = 12.5f, .color = Bind(muted_text) }
                                }
                            })
                        }
                    });
                }
                // 默认 home: 全部控件概览
                return make_page(VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 12.0f,
                    .content = {
                        Text { .text = "KTUI Gallery", .font_size = 22.0f },
                        Text { .text = "Windows 11 / WinUI 3 Fluent 2 控件标准陈列室", .font_size = 12.5f, .color = Bind(muted_text) },
                        buttons_card(),
                        toggles_card(),
                        combobox_card(),
                        radio_button_card(),
                        radio_card_group_card(),
                        slider_progress_card(),
                        progress_ring_card(),
                        text_input_card()
                    }
                });
            }
        });
    }
};

} // namespace

#if defined(__ANDROID__)

namespace {

struct AndroidGalleryState {
    std::unique_ptr<Application> app;
    std::shared_ptr<Window> window;
    GalleryApp gallery;

    void init_ui() {
        app = std::make_unique<Application>(create_platform_backend(false));

        WindowDesc desc;
        desc.title = "KTUI Gallery";
        desc.size = Vec2{412.0f, 915.0f};
        desc.resizable = false;
        desc.custom_titlebar = false;

        window = app->create_window(desc);
        if (!window) return;

        gallery.setup(*app, window);
        window->show();
        app->run();
    }
};

std::unique_ptr<AndroidGalleryState> g_gallery;

std::string jstring_to_utf8(JNIEnv* env, jstring jstr) {
    if (!env || !jstr) return "";
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    if (!chars) return "";
    std::string out(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return out;
}

} // namespace

extern "C" {

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeInit(JNIEnv* env, jclass clazz) {
    kernetile::text::android_init_text_bridge(env, clazz);
    ktui::AndroidBackend::set_jni_bridge(env, clazz);
    if (!g_gallery) {
        g_gallery = std::make_unique<AndroidGalleryState>();
        g_gallery->init_ui();
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeSurfaceChanged(
    JNIEnv* env, jclass, jobject surface, jint width, jint height, jfloat density) {
    ANativeWindow* native_win = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_surface_changed(native_win, width, height, density);
    }
    if (native_win) {
        ANativeWindow_release(native_win);
    }
    if (g_studio && g_studio->window) {
        g_studio->window->request_redraw();
        g_studio->window->render_frame(true);
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeSurfaceDestroyed(JNIEnv*, jclass) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_surface_destroyed();
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeStepFrame(JNIEnv*, jclass) {
    if (g_studio && g_studio->app) {
        g_studio->app->step_frame();
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnTouch(
    JNIEnv*, jclass, jint action, jfloat x, jfloat y) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_touch(action, x, y);
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnKey(
    JNIEnv*, jclass, jint action, jint key_code, jboolean shift, jboolean ctrl, jboolean alt) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_key(action, key_code, shift == JNI_TRUE, ctrl == JNI_TRUE, alt == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnImeComposition(
    JNIEnv* env, jclass, jstring text, jint cursor_byte) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_ime_composition(jstring_to_utf8(env, text), static_cast<size_t>(std::max(0, cursor_byte)));
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnImeCommit(JNIEnv* env, jclass, jstring text) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_ime_commit(jstring_to_utf8(env, text));
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeShutdown(JNIEnv*, jclass) {
    g_gallery.reset();
}

} // extern "C"

#else // !defined(__ANDROID__)

#if defined(__EMSCRIPTEN__)
static std::unique_ptr<Application> g_web_app;
static std::unique_ptr<GalleryApp> g_web_gallery;
#endif

int main() {
    std::setlocale(LC_ALL, "");

#if defined(__EMSCRIPTEN__)
    g_web_app = std::make_unique<Application>(create_platform_backend(false));
    auto& app = *g_web_app;
#else
    Application app(create_platform_backend(false));
#endif

    WindowDesc desc;
    desc.title = "KTUI Gallery";
    desc.size = Vec2{960.0f, 640.0f};
    desc.min_size = Vec2{460.0f, 400.0f};
    desc.resizable = true;

    auto window = app.create_window(desc);
    if (!window) return 1;

#if defined(__EMSCRIPTEN__)
    g_web_gallery = std::make_unique<GalleryApp>();
    auto& gallery = *g_web_gallery;
#else
    GalleryApp gallery;
#endif
    gallery.setup(app, window);

    window->show();
    return app.run();
}

#endif
