#include <ktui/ktui.hpp>
#include <iostream>
#include <string>
#include <clocale>

using namespace ktui;

// 极简演示 ViewModel
struct SimpleDemoViewModel {
    int count{0};
    Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    Property<bool> step_ten{false};
    Property<bool> lock_buttons{false};
    Property<std::string> input_text{"KerneTile KTUI"};
    Property<std::string> input_preview{"输入预览: KerneTile KTUI"};

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

// 构建极简桌面工作台 UI (只保留：按钮计数、Checkbox、文本框)
static WidgetPtr build_simple_ui(SimpleDemoViewModel& vm, std::shared_ptr<Window> win, float win_w, float win_h) {
    return Box {
        .width = win_w,
        .height = win_h,
        .background_color = Color{0.11f, 0.11f, 0.13f, 1.0f},
        .padding = EdgeInsets(16.0f, 18.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::Stretch,
            .spacing = 14.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                // -----------------------------------------------------
                // 1. 按钮计数板块
                // -----------------------------------------------------
                Box {
                    .background_color = Color{0.15f, 0.15f, 0.18f, 1.0f},
                    .border_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                    .border_width = 1.0f,
                    .corner_radius = 5.0f,
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
                                .font_size = 14.0f,
                                .color = Color{0.95f, 0.95f, 0.98f, 1.0f}
                            },
                            HStack {
                                .justify = JustifyContent::FlexStart,
                                .align_items = AlignItems::Center,
                                .spacing = 8.0f,
                                .padding = EdgeInsets::zero(),
                                .content = {
                                    Button {
                                        .text = "+ 增加",
                                        .height = 26.0f,
                                        .corner_radius = 3.0f,
                                        .padding = EdgeInsets(4.0f, 14.0f),
                                        .background_color = Color{0.15f, 0.45f, 0.85f, 1.0f},
                                        .hover_color = Color{0.20f, 0.52f, 0.92f, 1.0f},
                                        .active_color = Color{0.10f, 0.35f, 0.70f, 1.0f},
                                        .text_color = Color::white(),
                                        .border_width = 0.0f,
                                        .font_size = 13.0f,
                                        .on_click = [&vm]() {
                                            if (vm.lock_buttons.get()) return;
                                            vm.count += vm.step_ten.get() ? 10 : 1;
                                            vm.update_count_label();
                                        }
                                    },
                                    Button {
                                        .text = "- 减少",
                                        .height = 26.0f,
                                        .corner_radius = 3.0f,
                                        .padding = EdgeInsets(4.0f, 14.0f),
                                        .background_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                                        .hover_color = Color{0.28f, 0.28f, 0.34f, 1.0f},
                                        .text_color = Color{0.90f, 0.90f, 0.94f, 1.0f},
                                        .border_width = 0.0f,
                                        .font_size = 13.0f,
                                        .on_click = [&vm]() {
                                            if (vm.lock_buttons.get()) return;
                                            vm.count -= vm.step_ten.get() ? 10 : 1;
                                            vm.update_count_label();
                                        }
                                    },
                                    Button {
                                        .text = "重置",
                                        .height = 26.0f,
                                        .corner_radius = 3.0f,
                                        .padding = EdgeInsets(4.0f, 12.0f),
                                        .background_color = Color{0.26f, 0.18f, 0.20f, 1.0f},
                                        .hover_color = Color{0.36f, 0.22f, 0.24f, 1.0f},
                                        .text_color = Color{0.95f, 0.80f, 0.80f, 1.0f},
                                        .border_width = 0.0f,
                                        .font_size = 13.0f,
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

                // -----------------------------------------------------
                // 2. 复选框板块
                // -----------------------------------------------------
                Box {
                    .background_color = Color{0.15f, 0.15f, 0.18f, 1.0f},
                    .border_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                    .border_width = 1.0f,
                    .corner_radius = 5.0f,
                    .padding = EdgeInsets(10.0f, 14.0f),
                    .child = HStack {
                        .justify = JustifyContent::FlexStart,
                        .align_items = AlignItems::Center,
                        .spacing = 20.0f,
                        .padding = EdgeInsets::zero(),
                        .content = {
                            Checkbox {
                                .checked = false,
                                .bind_checked = bind(vm.step_ten),
                                .label = "步长 10 (+10)",
                                .size = 15.0f,
                                .font_size = 13.0f,
                                .on_change = [&vm](bool) { vm.update_count_label(); }
                            },
                            Checkbox {
                                .checked = false,
                                .bind_checked = bind(vm.lock_buttons),
                                .label = "锁定按钮",
                                .size = 15.0f,
                                .font_size = 13.0f,
                                .on_change = [&vm, win](bool locked) {
                                    set_all_buttons_disabled(win->root_render_box(), locked);
                                    vm.update_count_label();
                                }
                            }
                        }
                    }
                },

                // -----------------------------------------------------
                // 3. 文本框板块
                // -----------------------------------------------------
                Box {
                    .background_color = Color{0.15f, 0.15f, 0.18f, 1.0f},
                    .border_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
                    .border_width = 1.0f,
                    .corner_radius = 5.0f,
                    .padding = EdgeInsets(12.0f, 14.0f),
                    .child = VStack {
                        .justify = JustifyContent::FlexStart,
                        .align_items = AlignItems::Stretch,
                        .spacing = 8.0f,
                        .padding = EdgeInsets::zero(),
                        .content = {
                            Text {
                                .text = "单行文本输入 (支持撤销/重做、双向绑定):",
                                .font_size = 12.5f,
                                .color = Color{0.78f, 0.80f, 0.84f, 1.0f}
                            },
                            TextInput {
                                .text = "KerneTile KTUI",
                                .placeholder = "键入中文或英文...",
                                .bind_text = bind(vm.input_text),
                                .height = 28.0f,
                                .corner_radius = 3.0f,
                                .padding = EdgeInsets(4.0f, 8.0f),
                                .font_size = 13.0f,
                                .on_change = [&vm](const std::string& val) {
                                    vm.input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                                }
                            },
                            Text {
                                .text = "输入预览: KerneTile KTUI",
                                .bind_text = bind(vm.input_preview),
                                .font_size = 12.5f,
                                .color = Color{0.38f, 0.78f, 0.98f, 1.0f}
                            }
                        }
                    }
                }
            }
        }
    };
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

static LONG WINAPI ktui_crash_handler(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode == 0xe06d7363) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    FILE* f = std::fopen("D:\\KerneTile\\crash.log", "w");
    auto log_msg = [&](const char* fmt, auto... args) {
        if (f) {
            std::fprintf(f, fmt, args...);
            std::fflush(f);
        }
        std::fprintf(stderr, fmt, args...);
        std::fflush(stderr);
    };

    log_msg("=== CRASH CAUGHT BY HANDLER ===\n");
    log_msg("Exception Code: 0x%08lX\n", ep->ExceptionRecord->ExceptionCode);
    log_msg("Exception Address: %p\n", ep->ExceptionRecord->ExceptionAddress);

    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        ep->ExceptionRecord->NumberParameters >= 2) {
        ULONG_PTR access_type = ep->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR target_addr = ep->ExceptionRecord->ExceptionInformation[1];
        log_msg("Access Violation: %s address %p\n",
                access_type == 0 ? "Read" : (access_type == 1 ? "Write" : "Execute"),
                reinterpret_cast<void*>(target_addr));
    }

    HANDLE proc = GetCurrentProcess();
    SymInitialize(proc, nullptr, TRUE);

    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ep->ContextRecord->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ep->ContextRecord->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ep->ContextRecord->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;

    CONTEXT ctx = *ep->ContextRecord;
    log_msg("\n--- Call Stack ---\n");

    for (int i = 0; i < 32; ++i) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(),
                         &frame, &ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr)) {
            break;
        }

        DWORD64 addr = frame.AddrPC.Offset;
        if (addr == 0) break;

        char symbol_buf[sizeof(SYMBOL_INFO) + 256]{};
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buf);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD line_disp = 0;

        if (SymFromAddr(proc, addr, &displacement, symbol)) {
            if (SymGetLineFromAddr64(proc, addr, &line_disp, &line)) {
                log_msg("[%02d] %s - %s:%u (0x%llX)\n", i, symbol->Name, line.FileName, line.LineNumber, addr);
            } else {
                log_msg("[%02d] %s + 0x%llX (0x%llX)\n", i, symbol->Name, displacement, addr);
            }
        } else {
            log_msg("[%02d] 0x%llX\n", i, addr);
        }
    }

    HANDLE dmp = CreateFileW(L"D:\\KerneTile\\crash.dmp", GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dmp != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(proc, GetCurrentProcessId(), dmp, MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(dmp);
        log_msg("\nMinidump written to D:\\KerneTile\\crash.dmp\n");
    }

    if (f) std::fclose(f);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    AddVectoredExceptionHandler(1, ktui_crash_handler);
    SetUnhandledExceptionFilter(ktui_crash_handler);
#endif
    std::setlocale(LC_ALL, "");
    bool capture_only = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "--capture-only") {
            capture_only = true;
        }
    }

    // 1. 启动 KTUI 框架宿主 (使用静态生命周期，保障 WebAssembly 异步帧循环中上下文安全)
    static Application app(create_platform_backend(capture_only));

    constexpr float kWidth = 480.0f;
    constexpr float kHeight = 310.0f;

    WindowDesc desc;
    desc.title = "KerneTile Studio (KTUI Simple Demo)";
    desc.size = Vec2{kWidth, kHeight};
    desc.resizable = false;

    auto window = app.create_window(desc);
    if (!window) {
        std::cerr << "Failed to create KTUI window!" << std::endl;
        return 1;
    }

    // 2. 声明式挂载精简工作台 UI (按钮计数、Checkbox、文本框)
    static SimpleDemoViewModel vm;
    window->set_content(build_simple_ui(vm, window, kWidth, kHeight));

    // 3. 导出首帧预览图 (Web 平台下直接呈现)
    window->render_frame();
#if !defined(__EMSCRIPTEN__)
    window->export_bmp("KTUI_Studio_Preview.bmp");
#endif

    if (capture_only) {
        std::cout << "[Done] Preview exported: KTUI_Studio_Preview.bmp" << std::endl;
        return 0;
    }

    // 4. 显示窗口并进入主事件循环
    window->show();
    return app.run();
}
