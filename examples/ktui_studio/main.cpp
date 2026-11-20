#include <ktui/ktui.hpp>
#include <iostream>
#include <string>

using namespace ktui;

// 简洁的计数器 + 复选框 + 输入框 ViewModel
struct DemoViewModel {
    int count{0};
    Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    Property<bool> step_ten{false};
    Property<bool> lock_buttons{false};
    Property<std::string> input_text{""};
    Property<std::string> input_preview{"输入预览: (空)"};

    void update_count_label() {
        int step = step_ten.get() ? 10 : 1;
        std::string lock_str = lock_buttons.get() ? "  [已锁定]" : "";
        count_text.set("当前计数: " + std::to_string(count) + "   (步长: " + std::to_string(step) + ")" + lock_str);
    }
};

// 递归禁用/启用计数按钮
static void set_all_buttons_disabled(RenderNode* node, bool disabled) {
    if (!node) return;
    if (auto* btn = dynamic_cast<RenderButton*>(node)) {
        btn->set_disabled(disabled);
    }
    for (RenderNode* c = node->first_child(); c != nullptr; c = c->next_sibling()) {
        set_all_buttons_disabled(c, disabled);
    }
}

// 纯声明式桌面 UI（零平台 API，零 windows.h）
static WidgetPtr build_demo_ui(DemoViewModel& vm, std::shared_ptr<Window> win, float win_w, float win_h) {
    // 1. 按钮计数卡片
    WidgetPtr counter_buttons = HStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::Center,
        .spacing = 8.0f,
        .padding = EdgeInsets::zero(),
        .content = {
            Button {
                .text = "+ 增加",
                .height = 28.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 16.0f),
                .background_color = Color{0.15f, 0.44f, 0.80f, 1.0f},
                .hover_color = Color{0.20f, 0.52f, 0.90f, 1.0f},
                .active_color = Color{0.10f, 0.34f, 0.65f, 1.0f},
                .text_color = Color::white(),
                .border_color = Color{0.28f, 0.58f, 0.95f, 1.0f},
                .border_width = 1.0f,
                .font_size = 14.0f,
                .on_click = [&vm]() {
                    if (vm.lock_buttons.get()) return;
                    vm.count += vm.step_ten.get() ? 10 : 1;
                    vm.update_count_label();
                }
            },
            Button {
                .text = "- 减少",
                .height = 28.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 16.0f),
                .background_color = Color{0.22f, 0.22f, 0.25f, 1.0f},
                .hover_color = Color{0.29f, 0.29f, 0.33f, 1.0f},
                .active_color = Color{0.16f, 0.16f, 0.18f, 1.0f},
                .text_color = Color{0.92f, 0.92f, 0.95f, 1.0f},
                .border_color = Color{0.34f, 0.34f, 0.38f, 1.0f},
                .border_width = 1.0f,
                .font_size = 14.0f,
                .on_click = [&vm]() {
                    if (vm.lock_buttons.get()) return;
                    vm.count -= vm.step_ten.get() ? 10 : 1;
                    vm.update_count_label();
                }
            },
            Button {
                .text = "重置 Reset",
                .height = 28.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 14.0f),
                .background_color = Color{0.24f, 0.18f, 0.19f, 1.0f},
                .hover_color = Color{0.34f, 0.22f, 0.23f, 1.0f},
                .active_color = Color{0.18f, 0.13f, 0.14f, 1.0f},
                .text_color = Color{0.96f, 0.85f, 0.85f, 1.0f},
                .border_color = Color{0.45f, 0.28f, 0.30f, 1.0f},
                .border_width = 1.0f,
                .font_size = 14.0f,
                .on_click = [&vm]() {
                    if (vm.lock_buttons.get()) return;
                    vm.count = 0;
                    vm.update_count_label();
                }
            }
        }
    };

    WidgetPtr counter_card = Box {
        .width = win_w - 32.0f,
        .height = 94.0f,
        .background_color = Color{0.16f, 0.16f, 0.18f, 1.0f},
        .border_color = Color{0.26f, 0.26f, 0.30f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 3.0f,
        .padding = EdgeInsets(12.0f, 14.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::FlexStart,
            .spacing = 10.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                Text {
                    .text = "当前计数: 0   (步长: 1)",
                    .bind_text = bind(vm.count_text),
                    .font_size = 16.0f,
                    .color = Color{0.95f, 0.96f, 0.98f, 1.0f}
                },
                counter_buttons
            }
        }
    };

    // 2. Checkbox 选项卡片
    WidgetPtr checkbox_card = Box {
        .width = win_w - 32.0f,
        .height = 84.0f,
        .background_color = Color{0.16f, 0.16f, 0.18f, 1.0f},
        .border_color = Color{0.26f, 0.26f, 0.30f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 3.0f,
        .padding = EdgeInsets(12.0f, 14.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::FlexStart,
            .spacing = 10.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                Checkbox {
                    .checked = false,
                    .bind_checked = bind(vm.step_ten),
                    .label = "步长切换为 10 (Fast Step +10)",
                    .size = 15.0f,
                    .corner_radius = 2.0f,
                    .font_size = 14.0f,
                    .on_change = [&vm](bool) {
                        vm.update_count_label();
                    }
                },
                Checkbox {
                    .checked = false,
                    .bind_checked = bind(vm.lock_buttons),
                    .label = "锁定计数器 (禁用上方按钮)",
                    .size = 15.0f,
                    .corner_radius = 2.0f,
                    .font_size = 14.0f,
                    .on_change = [&vm, win](bool locked) {
                        set_all_buttons_disabled(win->root_render_box(), locked);
                        vm.update_count_label();
                    }
                }
            }
        }
    };

    // 3. 输入控件卡片（框架级 IME 自动支持中英文输入与候选框跟随）
    WidgetPtr input_card = Box {
        .width = win_w - 32.0f,
        .height = 114.0f,
        .background_color = Color{0.16f, 0.16f, 0.18f, 1.0f},
        .border_color = Color{0.26f, 0.26f, 0.30f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 3.0f,
        .padding = EdgeInsets(12.0f, 14.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::FlexStart,
            .spacing = 8.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                Text {
                    .text = "文本输入 (点击聚焦，支持中文 IME 输入法与退格编辑):",
                    .font_size = 13.0f,
                    .color = Color{0.75f, 0.77f, 0.80f, 1.0f}
                },
                Box {
                    .width = win_w - 60.0f,
                    .height = 28.0f,
                    .child = TextInput {
                        .text = "",
                        .placeholder = "点击此处输入中文或英文...",
                        .bind_text = bind(vm.input_text),
                        .height = 28.0f,
                        .corner_radius = 3.0f,
                        .padding = EdgeInsets(4.0f, 8.0f),
                        .font_size = 14.0f,
                        .on_change = [&vm](const std::string& val) {
                            if (val.empty()) {
                                vm.input_preview.set("输入预览: (空)");
                            } else {
                                vm.input_preview.set("输入预览: " + val);
                            }
                        }
                    }
                },
                Text {
                    .text = "输入预览: (空)",
                    .bind_text = bind(vm.input_preview),
                    .font_size = 14.0f,
                    .color = Color{0.40f, 0.82f, 0.98f, 1.0f}
                }
            }
        }
    };

    return Box {
        .width = win_w,
        .height = win_h,
        .background_color = Color{0.11f, 0.11f, 0.13f, 1.0f},
        .border_width = 0.0f,
        .corner_radius = 0.0f,
        .padding = EdgeInsets(14.0f, 16.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::FlexStart,
            .spacing = 10.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                Text {
                    .text = "KTUI 桌面演示 - 计数器 / Checkbox / 中文输入法",
                    .font_size = 15.0f,
                    .color = Color{0.92f, 0.93f, 0.96f, 1.0f}
                },
                counter_card,
                checkbox_card,
                input_card
            }
        }
    };
}

int main(int argc, char* argv[]) {
    bool capture_only = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "--capture-only") {
            capture_only = true;
        }
    }

    // 1. 启动 KTUI 框架宿主（自动接管平台窗口、DPI 感知、事件循环、渲染器与 IME 输入法）
    Application app(create_platform_backend(capture_only));

    constexpr float kWidth = 540.0f;
    constexpr float kHeight = 374.0f;

    WindowDesc desc;
    desc.title = "KTUI 桌面演示 - 计数器 / Checkbox / IME";
    desc.size = Vec2{kWidth, kHeight};
    desc.resizable = false;

    auto window = app.create_window(desc);
    if (!window) {
        std::cerr << "Failed to create KTUI window!" << std::endl;
        return 1;
    }

    // 2. 声明式挂载 UI
    DemoViewModel vm;
    window->set_content(build_demo_ui(vm, window, kWidth, kHeight));

    // 3. 导出首帧预览图
    window->render_frame();
    window->export_bmp("KTUI_Studio_Preview.bmp");

    if (capture_only) {
        std::cout << "[Done] Preview exported: KTUI_Studio_Preview.bmp" << std::endl;
        return 0;
    }

    // 4. 显示窗口并交由框架接管主循环
    window->show();
    return app.run();
}
