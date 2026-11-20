#include <ktui/ktui.hpp>
#include <kernetile/renderer.hpp>
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>

using namespace ktui;

// 保存标准 32-bit BMP 位图文件
static void save_bmp_image(const char* filepath, int width, int height, const uint8_t* rgba) {
#pragma pack(push, 1)
    struct BmpHeader {
        uint16_t bfType{0x4D42};
        uint32_t bfSize{0};
        uint16_t bfReserved1{0};
        uint16_t bfReserved2{0};
        uint32_t bfOffBits{54};
        uint32_t biSize{40};
        int32_t  biWidth{0};
        int32_t  biHeight{0};
        uint16_t biPlanes{1};
        uint16_t biBitCount{32};
        uint32_t biCompression{0};
        uint32_t biSizeImage{0};
        int32_t  biXPelsPerMeter{2835};
        int32_t  biYPelsPerMeter{2835};
        uint32_t biClrUsed{0};
        uint32_t biClrImportant{0};
    };
#pragma pack(pop)

    BmpHeader header;
    header.biWidth = width;
    header.biHeight = -height;
    header.biSizeImage = static_cast<uint32_t>(width * height * 4);
    header.bfSize = 54 + header.biSizeImage;

    std::vector<uint8_t> bgra(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        bgra[i * 4 + 0] = rgba[i * 4 + 2];
        bgra[i * 4 + 1] = rgba[i * 4 + 1];
        bgra[i * 4 + 2] = rgba[i * 4 + 0];
        bgra[i * 4 + 3] = rgba[i * 4 + 3];
    }

    FILE* fp = fopen(filepath, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fwrite(bgra.data(), 1, bgra.size(), fp);
        fclose(fp);
    }
}

// 简洁的计数器 + Checkbox + 输入框 ViewModel
struct DemoViewModel {
    int count{0};
    Property<std::string> count_text{"Count: 0   (Step: 1)"};
    Property<bool> step_ten{false};
    Property<bool> lock_buttons{false};
    Property<std::string> input_text{""};
    Property<std::string> input_preview{"Input Preview: (empty)"};

    void update_count_label() {
        int step = step_ten.get() ? 10 : 1;
        std::string lock_str = lock_buttons.get() ? "  [LOCKED]" : "";
        count_text.set("Count: " + std::to_string(count) + "   (Step: " + std::to_string(step) + ")" + lock_str);
    }
};

static DemoViewModel g_vm;
static std::unique_ptr<kernetile::KerneTileRenderer> g_renderer;
static WidgetPtr g_ui_root;
static std::shared_ptr<RenderBox> g_root_render_box;
static EventDispatcher g_dispatcher;
static FocusManager g_focus_manager;
static std::vector<uint8_t> g_framebuffer;

static int g_win_w = 520;
static int g_win_h = 370;

// 递归收集所有 RenderButton，用于 Checkbox 实时联动禁用/启用
static void set_all_buttons_disabled(RenderNode* node, bool disabled) {
    if (!node) return;
    if (auto* btn = dynamic_cast<RenderButton*>(node)) {
        btn->set_disabled(disabled);
    }
    for (RenderNode* c = node->first_child(); c != nullptr; c = c->next_sibling()) {
        set_all_buttons_disabled(c, disabled);
    }
}

// 构建简洁、紧凑的桌面演示 UI（按钮计数 + Checkbox + 输入框）
static WidgetPtr build_demo_ui() {
    // 1. 计数器卡片
    WidgetPtr counter_buttons = HStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::Center,
        .spacing = 8.0f,
        .padding = EdgeInsets::zero(),
        .content = {
            Button {
                .text = "+ Increment",
                .height = 28.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 14.0f),
                .background_color = Color{0.15f, 0.44f, 0.80f, 1.0f},
                .hover_color = Color{0.20f, 0.52f, 0.90f, 1.0f},
                .active_color = Color{0.10f, 0.34f, 0.65f, 1.0f},
                .text_color = Color::white(),
                .border_color = Color{0.28f, 0.58f, 0.95f, 1.0f},
                .border_width = 1.0f,
                .font_size = 14.0f,
                .on_click = []() {
                    if (g_vm.lock_buttons.get()) return;
                    g_vm.count += g_vm.step_ten.get() ? 10 : 1;
                    g_vm.update_count_label();
                }
            },
            Button {
                .text = "- Decrement",
                .height = 28.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 14.0f),
                .background_color = Color{0.22f, 0.22f, 0.25f, 1.0f},
                .hover_color = Color{0.29f, 0.29f, 0.33f, 1.0f},
                .active_color = Color{0.16f, 0.16f, 0.18f, 1.0f},
                .text_color = Color{0.92f, 0.92f, 0.95f, 1.0f},
                .border_color = Color{0.34f, 0.34f, 0.38f, 1.0f},
                .border_width = 1.0f,
                .font_size = 14.0f,
                .on_click = []() {
                    if (g_vm.lock_buttons.get()) return;
                    g_vm.count -= g_vm.step_ten.get() ? 10 : 1;
                    g_vm.update_count_label();
                }
            },
            Button {
                .text = "Reset",
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
                .on_click = []() {
                    if (g_vm.lock_buttons.get()) return;
                    g_vm.count = 0;
                    g_vm.update_count_label();
                }
            }
        }
    };

    WidgetPtr counter_card = Box {
        .width = 488.0f,
        .height = 92.0f,
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
                    .text = "Count: 0   (Step: 1)",
                    .bind_text = bind(g_vm.count_text),
                    .font_size = 16.0f,
                    .color = Color{0.95f, 0.96f, 0.98f, 1.0f}
                },
                counter_buttons
            }
        }
    };

    // 2. Checkbox 选项卡片
    WidgetPtr checkbox_card = Box {
        .width = 488.0f,
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
                    .bind_checked = bind(g_vm.step_ten),
                    .label = "Step by 10 (Fast Increment / Decrement)",
                    .size = 15.0f,
                    .corner_radius = 2.0f,
                    .font_size = 14.0f,
                    .on_change = [](bool) {
                        g_vm.update_count_label();
                    }
                },
                Checkbox {
                    .checked = false,
                    .bind_checked = bind(g_vm.lock_buttons),
                    .label = "Lock Counter (Disable Counter Buttons)",
                    .size = 15.0f,
                    .corner_radius = 2.0f,
                    .font_size = 14.0f,
                    .on_change = [](bool locked) {
                        set_all_buttons_disabled(g_root_render_box.get(), locked);
                        g_vm.update_count_label();
                    }
                }
            }
        }
    };

    // 3. 输入控件卡片
    WidgetPtr input_card = Box {
        .width = 488.0f,
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
                    .text = "Text Input (Click to focus and type):",
                    .font_size = 13.0f,
                    .color = Color{0.75f, 0.77f, 0.80f, 1.0f}
                },
                Box {
                    .width = 460.0f,
                    .height = 28.0f,
                    .child = TextInput {
                        .text = "",
                        .placeholder = "Type something here...",
                        .bind_text = bind(g_vm.input_text),
                        .height = 28.0f,
                        .corner_radius = 3.0f,
                        .padding = EdgeInsets(4.0f, 8.0f),
                        .font_size = 14.0f,
                        .on_change = [](const std::string& val) {
                            if (val.empty()) {
                                g_vm.input_preview.set("Input Preview: (empty)");
                            } else {
                                g_vm.input_preview.set("Input Preview: " + val);
                            }
                        }
                    }
                },
                Text {
                    .text = "Input Preview: (empty)",
                    .bind_text = bind(g_vm.input_preview),
                    .font_size = 14.0f,
                    .color = Color{0.40f, 0.82f, 0.98f, 1.0f}
                }
            }
        }
    };

    // 根背景容器
    return Box {
        .width = static_cast<float>(g_win_w),
        .height = static_cast<float>(g_win_h),
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
                    .text = "KTUI Desktop Demo - Counter, Checkbox & TextInput",
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

// 渲染并呈现到窗口
static void render_and_present(HWND hwnd) {
    if (!g_renderer || !g_root_render_box || g_win_w <= 0 || g_win_h <= 0) return;

    // 1. 布局
    BoxConstraints window_constraints = BoxConstraints::tight(Vec2{static_cast<float>(g_win_w), static_cast<float>(g_win_h)});
    g_root_render_box->layout(window_constraints);

    // 2. 绘制发射图元
    g_renderer->begin_frame();
    PaintContext ctx(g_renderer.get());
    g_root_render_box->paint(ctx);
    g_renderer->end_frame();

    // 3. 光栅化输出 RGBA8
    g_framebuffer.resize(static_cast<size_t>(g_win_w) * g_win_h * 4);
    g_renderer->render_to_rgba8(g_framebuffer.data());

    // 4. 提交到 Win32 窗口
    if (hwnd) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = g_win_w;
        bmi.bmiHeader.biHeight = -g_win_h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        std::vector<uint8_t> bgra(g_framebuffer.size());
        const size_t pixel_count = static_cast<size_t>(g_win_w) * g_win_h;
        for (size_t i = 0; i < pixel_count; ++i) {
            bgra[i * 4 + 0] = g_framebuffer[i * 4 + 2];
            bgra[i * 4 + 1] = g_framebuffer[i * 4 + 1];
            bgra[i * 4 + 2] = g_framebuffer[i * 4 + 0];
            bgra[i * 4 + 3] = 255;
        }

        SetDIBitsToDevice(hdc, 0, 0, g_win_w, g_win_h, 0, 0, 0, g_win_h,
                          bgra.data(), &bmi, DIB_RGB_COLORS);

        EndPaint(hwnd, &ps);
    }
}

// Win32 消息处理（完整接入 EventDispatcher + FocusManager）
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1; // 禁止 GDI 擦除背景防止闪烁

        case WM_PAINT:
            render_and_present(hwnd);
            return 0;

        case WM_MOUSEMOVE: {
            float x = static_cast<float>(static_cast<short>(LOWORD(lparam)));
            float y = static_cast<float>(static_cast<short>(HIWORD(lparam)));
            PointerEvent ev(PlatformEventType::PointerMove, PointF{x, y}, PointerButton::None);
            g_dispatcher.dispatch_pointer_event(g_root_render_box.get(), ev);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            SetCapture(hwnd);
            float x = static_cast<float>(static_cast<short>(LOWORD(lparam)));
            float y = static_cast<float>(static_cast<short>(HIWORD(lparam)));

            // 焦点路由：点击可聚焦控件则聚焦，点击其他地方则失焦
            HitTestResult hit_res;
            RenderBox* new_focus = nullptr;
            if (g_root_render_box && g_root_render_box->hit_test(hit_res, PointF{x, y})) {
                for (const auto& entry : hit_res.path()) {
                    if (entry.target && entry.target->is_focusable) {
                        new_focus = entry.target;
                        break;
                    }
                }
            }
            if (new_focus) {
                g_focus_manager.request_focus(new_focus);
            } else {
                g_focus_manager.clear_focus();
            }

            // 派发 PointerDown
            PointerEvent ev(PlatformEventType::PointerDown, PointF{x, y}, PointerButton::Primary);
            g_dispatcher.dispatch_pointer_event(g_root_render_box.get(), ev);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONUP: {
            ReleaseCapture();
            float x = static_cast<float>(static_cast<short>(LOWORD(lparam)));
            float y = static_cast<float>(static_cast<short>(HIWORD(lparam)));
            PointerEvent ev(PlatformEventType::PointerUp, PointF{x, y}, PointerButton::Primary);
            g_dispatcher.dispatch_pointer_event(g_root_render_box.get(), ev);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_KEYDOWN: {
            if (wparam == VK_ESCAPE) {
                g_focus_manager.clear_focus();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (wparam == VK_TAB) {
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                g_focus_manager.handle_tab_navigation(g_root_render_box.get(), shift);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            KeyEvent ev(PlatformEventType::KeyDown, static_cast<uint32_t>(wparam));
            g_dispatcher.dispatch_key_event(g_focus_manager.focused_node(), g_root_render_box.get(), ev);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_CHAR: {
            uint32_t ch = static_cast<uint32_t>(wparam);
            if (ch >= 32 && ch <= 126) {
                KeyEvent ev(PlatformEventType::ImeCommit, ch);
                ev.text = std::string(1, static_cast<char>(ch));
                g_dispatcher.dispatch_key_event(g_focus_manager.focused_node(), g_root_render_box.get(), ev);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_SIZE: {
            int new_w = LOWORD(lparam);
            int new_h = HIWORD(lparam);
            if (new_w > 0 && new_h > 0) {
                g_win_w = new_w;
                g_win_h = new_h;
                if (g_renderer) {
                    g_renderer->resize(static_cast<uint32_t>(g_win_w), static_cast<uint32_t>(g_win_h));
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int main(int argc, char* argv[]) {
    // 启用 Per-Monitor DPI 感知 V2，彻底杜绝 Windows 系统拉伸导致的文字发糊！
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    bool headless_capture_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--headless" || std::string(argv[i]) == "--capture-only") {
            headless_capture_only = true;
        }
    }

    // 1. 初始化 KerneTile 渲染器
    kernetile::RendererConfig cfg;
    cfg.width = static_cast<uint32_t>(g_win_w);
    cfg.height = static_cast<uint32_t>(g_win_h);
    cfg.prefer_warp = true;
    g_renderer = kernetile::KerneTileRenderer::create(cfg);
    if (!g_renderer) {
        std::cerr << "Failed to initialize KerneTile renderer!" << std::endl;
        return 1;
    }

    // 2. 构建声明式 UI
    g_ui_root = build_demo_ui();
    g_root_render_box = g_ui_root->create_render_node();

    // 3. 离屏渲染首帧并导出预览图
    render_and_present(nullptr);
    save_bmp_image("KTUI_Studio_Preview.bmp", g_win_w, g_win_h, g_framebuffer.data());

    if (headless_capture_only) {
        std::cout << "[Done] Preview exported: KTUI_Studio_Preview.bmp" << std::endl;
        return 0;
    }

    // 4. 创建精确客户区尺寸的原生桌面窗口
    HINSTANCE hinstance = GetModuleHandleW(nullptr);
    const wchar_t* kClass = L"KTUI_Demo_Window_Class";

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinstance;
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc{0, 0, g_win_w, g_win_h};
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_APPWINDOW);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClass,
        L"KTUI Demo - Counter, Checkbox & TextInput",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hinstance, nullptr
    );

    if (!hwnd) {
        std::cerr << "Failed to create window!" << std::endl;
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
