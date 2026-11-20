#include <ktui/ktui.hpp>
#include <iostream>
#include <string>

using namespace ktui;

// 桌面专业工作台 ViewModel
struct StudioViewModel {
    int count{0};
    Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    Property<bool> step_ten{false};
    Property<bool> lock_buttons{false};
    Property<std::string> input_text{"KerneTile_Studio_v2"};
    Property<std::string> input_preview{"输入预览: KerneTile_Studio_v2"};

    // 控件状态
    Property<std::string> build_profile{"Release (AVX2 + SM6.0)"};
    Property<std::string> selected_theme{"dark"};
    Property<bool> auto_watch{true};
    Property<bool> gpu_binning{true};
    Property<float> compile_progress{0.78f};
    Property<float> blur_radius{12.0f};
    Property<size_t> active_tab{0};
    Property<float> split_width{210.0f};

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

// 构建全套高信息密度桌面 UI (纯声明式，零 windows.h，零平台耦合)
static WidgetPtr build_studio_ui(StudioViewModel& vm, std::shared_ptr<Window> win, float win_w, float win_h) {
    // ---------------------------------------------------------
    // 1. 顶部菜单栏 (Menu Bar)
    // ---------------------------------------------------------
    WidgetPtr top_menu_bar = Box {
        .width = win_w,
        .height = 28.0f,
        .background_color = Color{0.10f, 0.10f, 0.12f, 1.0f},
        .border_color = Color{0.22f, 0.22f, 0.25f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 0.0f,
        .padding = EdgeInsets(2.0f, 8.0f),
        .child = HStack {
            .justify = JustifyContent::SpaceBetween,
            .align_items = AlignItems::Center,
            .spacing = 6.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                HStack {
                    .justify = JustifyContent::FlexStart,
                    .align_items = AlignItems::Center,
                    .spacing = 2.0f,
                    .padding = EdgeInsets::zero(),
                    .content = {
                        Button {
                            .text = "文件 File",
                            .height = 22.0f,
                            .corner_radius = 2.0f,
                            .padding = EdgeInsets(2.0f, 8.0f),
                            .background_color = Color::transparent(),
                            .hover_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                            .text_color = Color{0.85f, 0.85f, 0.90f, 1.0f},
                            .border_width = 0.0f,
                            .font_size = 12.0f,
                            .on_click = [win]() {
                                show_context_menu(win.get(), Vec2{10.0f, 28.0f}, {
                                    MenuItem::item("新建工程 (New Project)", nullptr, "Ctrl+N"),
                                    MenuItem::item("打开文件 (Open File...)", nullptr, "Ctrl+O"),
                                    MenuItem::separator(),
                                    MenuItem::item("保存 (Save)", nullptr, "Ctrl+S"),
                                    MenuItem::separator(),
                                    MenuItem::item("退出 (Exit)", [win]() { if (win) win->close(); }, "Alt+F4")
                                });
                            }
                        },
                        Button {
                            .text = "编辑 Edit",
                            .height = 22.0f,
                            .corner_radius = 2.0f,
                            .padding = EdgeInsets(2.0f, 8.0f),
                            .background_color = Color::transparent(),
                            .hover_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                            .text_color = Color{0.85f, 0.85f, 0.90f, 1.0f},
                            .border_width = 0.0f,
                            .font_size = 12.0f,
                            .on_click = [win]() {
                                show_context_menu(win.get(), Vec2{70.0f, 28.0f}, {
                                    MenuItem::item("撤销 (Undo)", nullptr, "Ctrl+Z"),
                                    MenuItem::item("重做 (Redo)", nullptr, "Ctrl+Y"),
                                    MenuItem::separator(),
                                    MenuItem::item("剪切 (Cut)", nullptr, "Ctrl+X"),
                                    MenuItem::item("复制 (Copy)", nullptr, "Ctrl+C"),
                                    MenuItem::item("粘贴 (Paste)", nullptr, "Ctrl+V"),
                                    MenuItem::separator(),
                                    MenuItem::item("全选 (Select All)", nullptr, "Ctrl+A")
                                });
                            }
                        },
                        Button {
                            .text = "视图 View",
                            .height = 22.0f,
                            .corner_radius = 2.0f,
                            .padding = EdgeInsets(2.0f, 8.0f),
                            .background_color = Color::transparent(),
                            .hover_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                            .text_color = Color{0.85f, 0.85f, 0.90f, 1.0f},
                            .border_width = 0.0f,
                            .font_size = 12.0f
                        },
                        Button {
                            .text = "帮助 Help",
                            .height = 22.0f,
                            .corner_radius = 2.0f,
                            .padding = EdgeInsets(2.0f, 8.0f),
                            .background_color = Color::transparent(),
                            .hover_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                            .text_color = Color{0.85f, 0.85f, 0.90f, 1.0f},
                            .border_width = 0.0f,
                            .font_size = 12.0f
                        }
                    }
                },
                HStack {
                    .justify = JustifyContent::FlexEnd,
                    .align_items = AlignItems::Center,
                    .spacing = 6.0f,
                    .padding = EdgeInsets::zero(),
                    .content = {
                        Button {
                            .text = "模态弹窗 (Modal)",
                            .height = 22.0f,
                            .corner_radius = 2.0f,
                            .padding = EdgeInsets(2.0f, 8.0f),
                            .background_color = Color{0.20f, 0.45f, 0.85f, 1.0f},
                            .hover_color = Color{0.25f, 0.52f, 0.92f, 1.0f},
                            .text_color = Color::white(),
                            .border_width = 0.0f,
                            .font_size = 11.5f,
                            .on_click = [win]() {
                                show_modal(win.get(), {
                                    .title = "系统确认 (System Confirmation)",
                                    .message = "您正在使用 KTUI 高信息密度桌面框架。\n全屏阻断遮罩已激活，所有底层事件均被安全拦截。\n按 Esc 或点确定即可关闭。",
                                    .confirm_text = "确认 (OK)",
                                    .cancel_text = "关闭 (Cancel)"
                                });
                            }
                        },
                        Button {
                            .text = "气泡 (Tooltip)",
                            .height = 22.0f,
                            .corner_radius = 2.0f,
                            .padding = EdgeInsets(2.0f, 8.0f),
                            .background_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                            .hover_color = Color{0.30f, 0.30f, 0.35f, 1.0f},
                            .text_color = Color{0.90f, 0.90f, 0.94f, 1.0f},
                            .border_width = 1.0f,
                            .font_size = 11.5f,
                            .on_click = [win]() {
                                show_tooltip(win.get(), Vec2{win->size().x - 140.0f, 32.0f}, "0% CPU 闲置与硬件视口裁剪已启用！");
                            }
                        }
                    }
                }
            }
        }
    };

    // ---------------------------------------------------------
    // 2. 左侧栏 (Sidebar) 控件集合: TreeView + Select + Switch + RadioGroup
    // ---------------------------------------------------------
    WidgetPtr sidebar_content = Box {
        .background_color = Color{0.12f, 0.12f, 0.14f, 1.0f},
        .padding = EdgeInsets(8.0f, 10.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::Stretch,
            .spacing = 8.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                Text {
                    .text = "工程资源库 (Project)",
                    .font_size = 12.0f,
                    .color = Color{0.60f, 0.60f, 0.65f, 1.0f}
                },
                // 树状图 (TreeView)
                TreeView {
                    .roots = {
                        TreeNode {
                            .label = "KerneTile",
                            .is_expanded = true,
                            .children = {
                                TreeNode { .label = "include" },
                                TreeNode {
                                    .label = "src",
                                    .is_expanded = true,
                                    .children = {
                                        TreeNode { .label = "main.cpp", .is_selected = true },
                                        TreeNode { .label = "renderer.cpp" },
                                        TreeNode { .label = "tile_binning.cpp" }
                                    }
                                },
                                TreeNode { .label = "shaders" }
                            }
                        }
                    },
                    .item_height = 20.0f,
                    .font_size = 12.0f
                },
                Text {
                    .text = "编译配置 (Profile):",
                    .font_size = 12.0f,
                    .color = Color{0.60f, 0.60f, 0.65f, 1.0f}
                },
                // 下拉选择框 (Select / ComboBox)
                Select {
                    .options = {
                        "Release (AVX2 + SM6.0)",
                        "Debug (AddressSanitizer)",
                        "RelWithDebInfo (Profiling)"
                    },
                    .bind_selected = bind(vm.build_profile),
                    .height = 24.0f,
                    .font_size = 12.0f
                },
                Text {
                    .text = "运行参数开关:",
                    .font_size = 12.0f,
                    .color = Color{0.60f, 0.60f, 0.65f, 1.0f}
                },
                // 开关 (Switch)
                HStack {
                    .justify = JustifyContent::SpaceBetween,
                    .align_items = AlignItems::Center,
                    .spacing = 6.0f,
                    .padding = EdgeInsets::zero(),
                    .content = {
                        Text { .text = "自动重绘 (0% CPU 休眠)", .font_size = 11.5f, .color = Color{0.80f, 0.80f, 0.85f, 1.0f} },
                        Switch { .bind_checked = bind(vm.auto_watch), .width = 28.0f, .height = 14.0f }
                    }
                },
                HStack {
                    .justify = JustifyContent::SpaceBetween,
                    .align_items = AlignItems::Center,
                    .spacing = 6.0f,
                    .padding = EdgeInsets::zero(),
                    .content = {
                        Text { .text = "GPU Tile Binning 2.0", .font_size = 11.5f, .color = Color{0.80f, 0.80f, 0.85f, 1.0f} },
                        Switch { .bind_checked = bind(vm.gpu_binning), .width = 28.0f, .height = 14.0f }
                    }
                },
                Text {
                    .text = "界面主题 (Theme):",
                    .font_size = 12.0f,
                    .color = Color{0.60f, 0.60f, 0.65f, 1.0f}
                },
                // 单选按钮组 (RadioGroup)
                RadioGroup {
                    .options = {
                        RadioOption { .label = "深空暗黑 (Dark)", .value = "dark" },
                        RadioOption { .label = "石板灰 (Slate)", .value = "slate" }
                    },
                    .bind_selected = bind(vm.selected_theme),
                    .horizontal = false,
                    .spacing = 4.0f,
                    .circle_size = 12.0f,
                    .font_size = 12.0f
                }
            }
        }
    };

    // ---------------------------------------------------------
    // 3. 右侧工作区 Tab 1: 编辑器与计数器
    // ---------------------------------------------------------
    WidgetPtr tab1_editor = Box {
        .background_color = Color{0.14f, 0.14f, 0.16f, 1.0f},
        .padding = EdgeInsets(10.0f, 12.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::Stretch,
            .spacing = 8.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                // 3.1 计数器行
                Box {
                    .background_color = Color{0.16f, 0.16f, 0.18f, 1.0f},
                    .border_color = Color{0.24f, 0.24f, 0.28f, 1.0f},
                    .border_width = 1.0f,
                    .corner_radius = 3.0f,
                    .padding = EdgeInsets(8.0f, 10.0f),
                    .child = VStack {
                        .justify = JustifyContent::FlexStart,
                        .align_items = AlignItems::FlexStart,
                        .spacing = 6.0f,
                        .padding = EdgeInsets::zero(),
                        .content = {
                            Text {
                                .text = "当前计数: 0   (步长: 1)",
                                .bind_text = bind(vm.count_text),
                                .font_size = 14.0f,
                                .color = Color{0.95f, 0.95f, 0.98f, 1.0f}
                            },
                            HStack {
                                .justify = JustifyContent::FlexStart,
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .padding = EdgeInsets::zero(),
                                .content = {
                                    Button {
                                        .text = "+ 增加",
                                        .height = 24.0f,
                                        .corner_radius = 2.0f,
                                        .padding = EdgeInsets(3.0f, 12.0f),
                                        .background_color = Color{0.15f, 0.44f, 0.80f, 1.0f},
                                        .hover_color = Color{0.20f, 0.52f, 0.90f, 1.0f},
                                        .active_color = Color{0.10f, 0.34f, 0.65f, 1.0f},
                                        .text_color = Color::white(),
                                        .border_width = 0.0f,
                                        .font_size = 12.5f,
                                        .on_click = [&vm]() {
                                            if (vm.lock_buttons.get()) return;
                                            vm.count += vm.step_ten.get() ? 10 : 1;
                                            vm.update_count_label();
                                        }
                                    },
                                    Button {
                                        .text = "- 减少",
                                        .height = 24.0f,
                                        .corner_radius = 2.0f,
                                        .padding = EdgeInsets(3.0f, 12.0f),
                                        .background_color = Color{0.22f, 0.22f, 0.25f, 1.0f},
                                        .hover_color = Color{0.28f, 0.28f, 0.32f, 1.0f},
                                        .text_color = Color{0.90f, 0.90f, 0.94f, 1.0f},
                                        .border_color = Color{0.30f, 0.30f, 0.35f, 1.0f},
                                        .border_width = 1.0f,
                                        .font_size = 12.5f,
                                        .on_click = [&vm]() {
                                            if (vm.lock_buttons.get()) return;
                                            vm.count -= vm.step_ten.get() ? 10 : 1;
                                            vm.update_count_label();
                                        }
                                    },
                                    Button {
                                        .text = "重置",
                                        .height = 24.0f,
                                        .corner_radius = 2.0f,
                                        .padding = EdgeInsets(3.0f, 10.0f),
                                        .background_color = Color{0.24f, 0.18f, 0.19f, 1.0f},
                                        .hover_color = Color{0.34f, 0.22f, 0.23f, 1.0f},
                                        .text_color = Color{0.95f, 0.82f, 0.82f, 1.0f},
                                        .border_color = Color{0.40f, 0.25f, 0.28f, 1.0f},
                                        .border_width = 1.0f,
                                        .font_size = 12.5f,
                                        .on_click = [&vm]() {
                                            if (vm.lock_buttons.get()) return;
                                            vm.count = 0;
                                            vm.update_count_label();
                                        }
                                    }
                                }
                            }
                        }
                    }
                },
                // 3.2 复选框行
                HStack {
                    .justify = JustifyContent::FlexStart,
                    .align_items = AlignItems::Center,
                    .spacing = 16.0f,
                    .padding = EdgeInsets::zero(),
                    .content = {
                        Checkbox {
                            .checked = false,
                            .bind_checked = bind(vm.step_ten),
                            .label = "步长 10 (+10)",
                            .size = 14.0f,
                            .font_size = 12.5f,
                            .on_change = [&vm](bool) { vm.update_count_label(); }
                        },
                        Checkbox {
                            .checked = false,
                            .bind_checked = bind(vm.lock_buttons),
                            .label = "锁定按钮",
                            .size = 14.0f,
                            .font_size = 12.5f,
                            .on_change = [&vm, win](bool locked) {
                                set_all_buttons_disabled(win->root_render_box(), locked);
                                vm.update_count_label();
                            }
                        }
                    }
                },
                // 3.3 文本输入框 (支持撤销重做 Ctrl+Z/Y、双击选词、530ms 光标闪烁、中文输入法)
                Text {
                    .text = "单行文本输入 (支持 Ctrl+Z 撤销/重做、双击选词、IME 中文输入法):",
                    .font_size = 12.5f,
                    .color = Color{0.75f, 0.77f, 0.80f, 1.0f}
                },
                TextInput {
                    .text = "KerneTile_Studio_v2",
                    .placeholder = "键入中文或英文...",
                    .bind_text = bind(vm.input_text),
                    .height = 26.0f,
                    .corner_radius = 3.0f,
                    .padding = EdgeInsets(4.0f, 8.0f),
                    .font_size = 13.0f,
                    .on_change = [&vm](const std::string& val) {
                        vm.input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                    }
                },
                Text {
                    .text = "输入预览: KerneTile_Studio_v2",
                    .bind_text = bind(vm.input_preview),
                    .font_size = 12.5f,
                    .color = Color{0.38f, 0.78f, 0.98f, 1.0f}
                }
            }
        }
    };

    // ---------------------------------------------------------
    // 4. 右侧工作区 Tab 2: 内核编译与状态
    // ---------------------------------------------------------
    WidgetPtr tab2_kernel = Box {
        .background_color = Color{0.14f, 0.14f, 0.16f, 1.0f},
        .padding = EdgeInsets(10.0f, 12.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::Stretch,
            .spacing = 10.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                Text {
                    .text = "KerneTile 2D GPGPU 计算内核编译进度 (78%):",
                    .font_size = 12.5f,
                    .color = Color{0.80f, 0.82f, 0.86f, 1.0f}
                },
                // 进度条 (ProgressBar)
                ProgressBar {
                    .bind_value = bind(vm.compile_progress),
                    .height = 6.0f,
                    .corner_radius = 2.0f,
                    .fill_color = Color{0.20f, 0.55f, 0.95f, 1.0f}
                },
                Text {
                    .text = "Dual Kawase 毛玻璃模糊半径调节 (Slider):",
                    .font_size = 12.5f,
                    .color = Color{0.80f, 0.82f, 0.86f, 1.0f}
                },
                // 滑块 (Slider)
                Slider {
                    .value = 12.0f,
                    .min_value = 0.0f,
                    .max_value = 40.0f,
                    .bind_value = bind(vm.blur_radius)
                },
                Box {
                    .height = 70.0f,
                    .background_color = Color{0.09f, 0.09f, 0.11f, 1.0f},
                    .border_color = Color{0.20f, 0.20f, 0.24f, 1.0f},
                    .border_width = 1.0f,
                    .corner_radius = 3.0f,
                    .padding = EdgeInsets(6.0f, 8.0f),
                    .child = VStack {
                        .justify = JustifyContent::FlexStart,
                        .align_items = AlignItems::FlexStart,
                        .spacing = 3.0f,
                        .padding = EdgeInsets::zero(),
                        .content = {
                            Text { .text = "[Console] KerneTile GPGPU Direct3D 12 Engine Loaded.", .font_size = 11.0f, .color = Color{0.45f, 0.85f, 0.45f, 1.0f} },
                            Text { .text = "[Memory] 16x16 Tile Mesh: 240x135 tiles | LDS Cache: 256 B.", .font_size = 11.0f, .color = Color{0.70f, 0.70f, 0.75f, 1.0f} },
                            Text { .text = "[Scheduler] On-Demand Repainting active (0% CPU at idle).", .font_size = 11.0f, .color = Color{0.38f, 0.75f, 0.95f, 1.0f} }
                        }
                    }
                }
            }
        }
    };

    // ---------------------------------------------------------
    // 5. 右侧多标签页容器 (Tabs)
    // ---------------------------------------------------------
    WidgetPtr right_tabs = Tabs {
        .tabs = {
            TabItem { .title = "编辑器与计数器", .content = tab1_editor },
            TabItem { .title = "内核状态与性能", .content = tab2_kernel }
        },
        .bind_active_index = bind(vm.active_tab),
        .tab_bar_height = 26.0f
    };

    // ---------------------------------------------------------
    // 6. 主工作区分栏器 (SplitPane: 左侧栏 + 右侧多标签页)
    // ---------------------------------------------------------
    float content_h = win_h - 28.0f;
    WidgetPtr main_split_pane = SplitPane {
        .first = sidebar_content,
        .second = right_tabs,
        .direction = SplitDirection::Horizontal,
        .split_position = 210.0f,
        .bind_split_position = bind(vm.split_width),
        .min_first = 140.0f,
        .min_second = 240.0f,
        .splitter_thickness = 4.0f
    };

    // ---------------------------------------------------------
    // 7. 顶级视口 (Top Menu Bar + Main Split Pane)
    // ---------------------------------------------------------
    return Box {
        .width = win_w,
        .height = win_h,
        .background_color = Color{0.10f, 0.10f, 0.12f, 1.0f},
        .border_width = 0.0f,
        .corner_radius = 0.0f,
        .padding = EdgeInsets::zero(),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::Stretch,
            .spacing = 0.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                top_menu_bar,
                Box {
                    .width = win_w,
                    .height = content_h,
                    .child = main_split_pane
                }
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

    // 1. 启动 KTUI 框架宿主（全自动接管平台窗口、DPI、事件循环与 0% CPU 闲置休眠）
    Application app(create_platform_backend(capture_only));

    constexpr float kWidth = 680.0f;
    constexpr float kHeight = 440.0f;

    WindowDesc desc;
    desc.title = "KerneTile Studio (KTUI Desktop - 0% CPU On-Demand)";
    desc.size = Vec2{kWidth, kHeight};
    desc.resizable = false;

    auto window = app.create_window(desc);
    if (!window) {
        std::cerr << "Failed to create KTUI window!" << std::endl;
        return 1;
    }

    // 2. 声明式挂载完整桌面工作台 UI
    StudioViewModel vm;
    window->set_content(build_studio_ui(vm, window, kWidth, kHeight));

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
