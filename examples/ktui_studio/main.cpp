#include <ktui/ktui.hpp>
#include <kernetile/renderer.hpp>
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

using namespace ktui;

// 保存标准 32-bit BMP 位图文件
static void save_bmp_image(const char* filepath, int width, int height, const uint8_t* rgba) {
#pragma pack(push, 1)
    struct BmpHeader {
        uint16_t bfType{0x4D42}; // "BM"
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
    header.biHeight = -height; // top-down
    header.biSizeImage = static_cast<uint32_t>(width * height * 4);
    header.bfSize = 54 + header.biSizeImage;

    std::vector<uint8_t> bgra(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        bgra[i * 4 + 0] = rgba[i * 4 + 2]; // B
        bgra[i * 4 + 1] = rgba[i * 4 + 1]; // G
        bgra[i * 4 + 2] = rgba[i * 4 + 0]; // R
        bgra[i * 4 + 3] = rgba[i * 4 + 3]; // A
    }

    FILE* fp = fopen(filepath, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fwrite(bgra.data(), 1, bgra.size(), fp);
        fclose(fp);
    }
}

// 桌面专业 IDE ViewModel
struct StudioViewModel {
    Property<std::string> search_filter{""};
    Property<std::string> status_text{"Ready | KerneTile Engine Initialized"};
    Property<bool> opt_hlsl_compute{true};
    Property<bool> opt_culling{true};
    Property<bool> opt_kawase_blur{true};
    Property<float> blur_radius{4.0f};
    Property<uint32_t> build_counter{0};
    std::vector<std::string> console_lines{
        "[Init] KerneTile GPGPU Direct3D 12 Engine Loaded.",
        "[Tile] 16x16 Tile Binning mesh allocated: 240 x 135.",
        "[VirtIO] Submission channel throughput ready: 240 M/s.",
        "[KTUI] Desktop High Information Density System active."
    };
};

static StudioViewModel g_vm;
static std::unique_ptr<kernetile::KerneTileRenderer> g_renderer;
static WidgetPtr g_ui_root;
static std::shared_ptr<RenderBox> g_root_render_box;
static std::vector<uint8_t> g_framebuffer;
static int g_win_w = 1024;
static int g_win_h = 680;

// 1. 顶部紧凑菜单栏 (高度 24px, 直角贴边)
static WidgetPtr create_menu_bar() {
    WidgetPtr menu_row = HStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::Center,
        .spacing = 16.0f,
        .padding = EdgeInsets::zero(),
        .content = {
            Text { .text = "File", .font_size = 12.0f, .color = Color{0.8f, 0.8f, 0.82f, 1.0f} },
            Text { .text = "Edit", .font_size = 12.0f, .color = Color{0.8f, 0.8f, 0.82f, 1.0f} },
            Text { .text = "Selection", .font_size = 12.0f, .color = Color{0.8f, 0.8f, 0.82f, 1.0f} },
            Text { .text = "View", .font_size = 12.0f, .color = Color{0.8f, 0.8f, 0.82f, 1.0f} },
            Text { .text = "Build", .font_size = 12.0f, .color = Color{0.8f, 0.8f, 0.82f, 1.0f} },
            Text { .text = "Terminal", .font_size = 12.0f, .color = Color{0.8f, 0.8f, 0.82f, 1.0f} },
            Text { .text = "Help", .font_size = 12.0f, .color = Color{0.8f, 0.8f, 0.82f, 1.0f} }
        }
    };

    return Box {
        .width = static_cast<float>(g_win_w),
        .height = 24.0f,
        .background_color = Color{0.14f, 0.14f, 0.15f, 1.0f},
        .border_color = Color{0.22f, 0.22f, 0.25f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 0.0f,
        .padding = EdgeInsets(2.0f, 8.0f),
        .child = menu_row
    };
}

// 2. 顶部工具栏 (高度 36px, 紧凑 26px 控件)
static WidgetPtr create_toolbar() {
    WidgetPtr tool_row = HStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::Center,
        .spacing = 8.0f,
        .padding = EdgeInsets::zero(),
        .content = {
            Button {
                .text = "Compile Kernel",
                .height = 26.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 10.0f),
                .background_color = Color{0.10f, 0.45f, 0.25f, 1.0f}, // 墨绿强调
                .hover_color = Color{0.12f, 0.55f, 0.30f, 1.0f},
                .active_color = Color{0.08f, 0.35f, 0.18f, 1.0f},
                .on_click = []() {
                    g_vm.build_counter.set(g_vm.build_counter.get() + 1);
                    std::string msg = "Compiled successfully! Build #" + std::to_string(g_vm.build_counter.get());
                    g_vm.status_text.set(msg);
                    g_vm.console_lines.push_back("[Build] " + msg);
                }
            },
            Button {
                .text = "Run 4K Bench",
                .height = 26.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 10.0f),
                .background_color = Color{0.16f, 0.40f, 0.75f, 1.0f}, // 蔚蓝强调
                .hover_color = Color{0.20f, 0.48f, 0.88f, 1.0f},
                .active_color = Color{0.12f, 0.32f, 0.62f, 1.0f},
                .on_click = []() {
                    g_vm.status_text.set("4K Bench: 99.98% Culling Ratio, 120 FPS");
                    g_vm.console_lines.push_back("[Bench] 32400 tiles tested, 0 LSB difference.");
                }
            },
            Button {
                .text = "Clear Log",
                .height = 26.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 10.0f),
                .on_click = []() {
                    g_vm.console_lines.clear();
                    g_vm.console_lines.push_back("[Console] Log cleared.");
                    g_vm.status_text.set("Console cleared.");
                }
            },
            TextInput {
                .placeholder = "Filter symbols (Ctrl+P)...",
                .bind_text = bind(g_vm.search_filter),
                .height = 26.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 8.0f),
                .font_size = 12.0f
            }
        }
    };

    return Box {
        .width = static_cast<float>(g_win_w),
        .height = 36.0f,
        .background_color = Color{0.18f, 0.18f, 0.20f, 1.0f},
        .border_color = Color{0.24f, 0.24f, 0.26f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 0.0f,
        .padding = EdgeInsets(5.0f, 8.0f),
        .child = tool_row
    };
}

// 3. 左侧资源浏览器面板 (宽度 240px)
static WidgetPtr create_explorer_panel() {
    WidgetPtr tree_view = VStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::FlexStart,
        .spacing = 8.0f,
        .padding = EdgeInsets::zero(),
        .content = {
            Text { .text = "PROJECT EXPLORER", .font_size = 11.0f, .color = Color{0.6f, 0.6f, 0.65f, 1.0f} },
            Checkbox {
                .checked = true,
                .bind_checked = bind(g_vm.opt_hlsl_compute),
                .label = "kerne_tile_compute.hlsl",
                .size = 14.0f,
                .corner_radius = 2.0f
            },
            Checkbox {
                .checked = true,
                .bind_checked = bind(g_vm.opt_culling),
                .label = "tile_binner.cpp",
                .size = 14.0f,
                .corner_radius = 2.0f
            },
            Checkbox {
                .checked = true,
                .bind_checked = bind(g_vm.opt_kawase_blur),
                .label = "dual_kawase_blur.cpp",
                .size = 14.0f,
                .corner_radius = 2.0f
            },
            Text { .text = "SHADERS & PIPELINES", .font_size = 11.0f, .color = Color{0.6f, 0.6f, 0.65f, 1.0f} },
            Text { .text = "  -> kerne_sdf.slang", .font_size = 12.0f, .color = Color{0.75f, 0.75f, 0.78f, 1.0f} },
            Text { .text = "  -> kerne_shadow.slang", .font_size = 12.0f, .color = Color{0.75f, 0.75f, 0.78f, 1.0f} },
            Text { .text = "  -> kerne_blit.hlsl", .font_size = 12.0f, .color = Color{0.75f, 0.75f, 0.78f, 1.0f} }
        }
    };

    return Box {
        .width = 240.0f,
        .height = static_cast<float>(g_win_h - 24 - 36 - 22),
        .background_color = Color{0.13f, 0.13f, 0.14f, 1.0f},
        .border_color = Color{0.20f, 0.20f, 0.22f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 0.0f,
        .padding = EdgeInsets(8.0f),
        .child = tree_view
    };
}

// 4. 右侧属性检视卡片 (Inspector)
static WidgetPtr create_inspector_card() {
    WidgetPtr content = VStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::FlexStart,
        .spacing = 8.0f,
        .padding = EdgeInsets::zero(),
        .content = {
            Text { .text = "Kernel Configuration (Inspector)", .font_size = 13.0f, .color = Color{0.95f, 0.95f, 0.95f, 1.0f} },
            HStack {
                .justify = JustifyContent::FlexStart,
                .align_items = AlignItems::Center,
                .spacing = 10.0f,
                .padding = EdgeInsets::zero(),
                .content = {
                    Text { .text = "Kawase Blur Radius:", .font_size = 12.0f },
                    Slider {
                        .value = 4.0f,
                        .min_value = 1.0f,
                        .max_value = 16.0f,
                        .bind_value = bind(g_vm.blur_radius),
                        .track_height = 4.0f,
                        .thumb_size = 12.0f
                    }
                }
            },
            HStack {
                .justify = JustifyContent::FlexStart,
                .align_items = AlignItems::Center,
                .spacing = 8.0f,
                .padding = EdgeInsets::zero(),
                .content = {
                    Text { .text = "Tile Mesh: 16 x 16 (256 threads)", .font_size = 12.0f, .color = Color{0.20f, 0.75f, 0.95f, 1.0f} },
                    Text { .text = "|  LDS Cache: 256 B", .font_size = 12.0f, .color = Color{0.7f, 0.7f, 0.7f, 1.0f} },
                    Text { .text = "|  Pipeline: Compute SM 6.0", .font_size = 12.0f, .color = Color{0.7f, 0.7f, 0.7f, 1.0f} }
                }
            }
        }
    };

    return Box {
        .width = static_cast<float>(g_win_w - 240 - 16),
        .height = 130.0f,
        .background_color = Color{0.16f, 0.16f, 0.18f, 1.0f},
        .border_color = Color{0.25f, 0.25f, 0.28f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 4.0f, // 4px 微圆角
        .padding = EdgeInsets(10.0f),
        .child = content
    };
}

// 5. 控制台输出卡片
static WidgetPtr create_console_card() {
    WidgetPtr lines_view = VStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::FlexStart,
        .spacing = 4.0f,
        .padding = EdgeInsets::zero(),
        .content = {
            Text { .text = "[Console & Execution Output]", .font_size = 11.0f, .color = Color{0.5f, 0.8f, 0.5f, 1.0f} },
            Text { .text = "Line 1: [Init] KerneTile GPGPU Direct3D 12 Engine Loaded.", .font_size = 12.0f },
            Text { .text = "Line 2: [Tile] 16x16 Tile Binning mesh allocated: 240 x 135.", .font_size = 12.0f },
            Text { .text = "Line 3: [VirtIO] Submission channel throughput ready: 240 M/s.", .font_size = 12.0f },
            Text { .text = "Line 4: [KTUI] Desktop High Information Density System active.", .font_size = 12.0f },
            Text { .text = "Line 5: [Engine] Zero memory leak validated. Double-click to execute.", .font_size = 12.0f }
        }
    };

    return Box {
        .width = static_cast<float>(g_win_w - 240 - 16),
        .height = static_cast<float>(g_win_h - 24 - 36 - 22 - 130 - 24),
        .background_color = Color{0.11f, 0.11f, 0.12f, 1.0f},
        .border_color = Color{0.22f, 0.22f, 0.25f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 4.0f,
        .padding = EdgeInsets(8.0f),
        .child = lines_view
    };
}

// 6. 底部状态栏 (高度 22px, 直角贴边)
static WidgetPtr create_status_bar() {
    WidgetPtr status_row = HStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::Center,
        .spacing = 16.0f,
        .padding = EdgeInsets::zero(),
        .content = {
            Text { .text = "Ready", .bind_text = bind(g_vm.status_text), .font_size = 11.0f, .color = Color::white() },
            Text { .text = "Ln 1, Col 1", .font_size = 11.0f, .color = Color::white() },
            Text { .text = "UTF-8", .font_size = 11.0f, .color = Color::white() },
            Text { .text = "D3D12 SM 6.0", .font_size = 11.0f, .color = Color::white() },
            Text { .text = "120 FPS", .font_size = 11.0f, .color = Color::white() }
        }
    };

    return Box {
        .width = static_cast<float>(g_win_w),
        .height = 22.0f,
        .background_color = Color{0.08f, 0.45f, 0.75f, 1.0f}, // VS Code 经典亮蓝
        .border_width = 0.0f,
        .corner_radius = 0.0f,
        .padding = EdgeInsets(2.0f, 8.0f),
        .child = status_row
    };
}

// 组合整屏 UI
static WidgetPtr build_desktop_ui() {
    WidgetPtr menu = create_menu_bar();
    WidgetPtr tool = create_toolbar();
    WidgetPtr explorer = create_explorer_panel();
    WidgetPtr inspector = create_inspector_card();
    WidgetPtr console = create_console_card();
    WidgetPtr status = create_status_bar();

    WidgetPtr right_column = VStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::FlexStart,
        .spacing = 6.0f,
        .padding = EdgeInsets(8.0f),
        .content = { inspector, console }
    };

    WidgetPtr main_split = HStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::FlexStart,
        .spacing = 0.0f,
        .padding = EdgeInsets::zero(),
        .content = { explorer, right_column }
    };

    return VStack {
        .justify = JustifyContent::FlexStart,
        .align_items = AlignItems::FlexStart,
        .spacing = 0.0f,
        .padding = EdgeInsets::zero(),
        .content = { menu, tool, main_split, status }
    };
}

// 渲染整树到内存帧缓冲并呈现
static void render_and_present(HWND hwnd) {
    if (!g_renderer || !g_root_render_box) return;

    // 1. 约束与布局
    BoxConstraints window_constraints = BoxConstraints::tight(Vec2{static_cast<float>(g_win_w), static_cast<float>(g_win_h)});
    g_root_render_box->layout(window_constraints);

    // 2. 发射图元
    g_renderer->begin_frame();
    PaintContext ctx(g_renderer.get());
    g_root_render_box->paint(ctx);
    g_renderer->end_frame();

    // 3. 渲染到 RGBA8 帧缓冲
    g_framebuffer.resize(g_win_w * g_win_h * 4);
    g_renderer->render_to_rgba8(g_framebuffer.data());

    // 4. 提交呈现到窗口
    if (hwnd) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = g_win_w;
        bmi.bmiHeader.biHeight = -g_win_h; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        // RGBA -> BGRA 转给 Windows GDI
        std::vector<uint8_t> bgra(g_framebuffer.size());
        for (size_t i = 0; i < g_framebuffer.size() / 4; ++i) {
            bgra[i * 4 + 0] = g_framebuffer[i * 4 + 2];
            bgra[i * 4 + 1] = g_framebuffer[i * 4 + 1];
            bgra[i * 4 + 2] = g_framebuffer[i * 4 + 0];
            bgra[i * 4 + 3] = g_framebuffer[i * 4 + 3];
        }

        SetDIBitsToDevice(hdc, 0, 0, g_win_w, g_win_h, 0, 0, 0, g_win_h,
                          bgra.data(), &bmi, DIB_RGB_COLORS);

        EndPaint(hwnd, &ps);
    }
}

// Win32 消息循环
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_PAINT:
            render_and_present(hwnd);
            return 0;

        case WM_MOUSEMOVE: {
            float x = static_cast<float>(static_cast<short>(LOWORD(lparam)));
            float y = static_cast<float>(static_cast<short>(HIWORD(lparam)));
            HitTestResult hit_res;
            if (g_root_render_box && g_root_render_box->hit_test(hit_res, PointF{x, y})) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            float x = static_cast<float>(static_cast<short>(LOWORD(lparam)));
            float y = static_cast<float>(static_cast<short>(HIWORD(lparam)));
            HitTestResult hit_res;
            if (g_root_render_box && g_root_render_box->hit_test(hit_res, PointF{x, y})) {
                PointerEvent ev(PlatformEventType::PointerDown, PointF{x, y}, PointerButton::Primary);
                for (auto& entry : hit_res.path()) {
                    if (entry.target->on_pointer_down) {
                        entry.target->on_pointer_down(ev);
                        break;
                    }
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            float x = static_cast<float>(static_cast<short>(LOWORD(lparam)));
            float y = static_cast<float>(static_cast<short>(HIWORD(lparam)));
            HitTestResult hit_res;
            if (g_root_render_box && g_root_render_box->hit_test(hit_res, PointF{x, y})) {
                PointerEvent ev(PlatformEventType::PointerUp, PointF{x, y}, PointerButton::Primary);
                for (auto& entry : hit_res.path()) {
                    if (entry.target->on_pointer_up) {
                        entry.target->on_pointer_up(ev);
                        break;
                    }
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_CHAR: {
            char c = static_cast<char>(wparam);
            if (c >= 32 && c <= 126) {
                std::string current = g_vm.search_filter.get();
                current.push_back(c);
                g_vm.search_filter.set(current);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wparam == VK_BACK) {
                std::string current = g_vm.search_filter.get();
                if (!current.empty()) {
                    current.pop_back();
                    g_vm.search_filter.set(current);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            } else if (wparam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            return 0;
        }

        case WM_SIZE: {
            g_win_w = LOWORD(lparam);
            g_win_h = HIWORD(lparam);
            if (g_renderer) {
                g_renderer->resize(g_win_w, g_win_h);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int main(int argc, char* argv[]) {
    bool headless_capture_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--headless" || std::string(argv[i]) == "--capture-only") {
            headless_capture_only = true;
        }
    }

    std::cout << "==========================================================" << std::endl;
    std::cout << "   KTUI Studio Demo - 桌面高信息密度现代声明式 IDE 演示   " << std::endl;
    std::cout << "==========================================================" << std::endl;

    // 1. 初始化 KerneTile 引擎门面 (自动选择 D3D12 GPGPU / WARP / CPU 回退)
    kernetile::RendererConfig cfg;
    cfg.width = g_win_w;
    cfg.height = g_win_h;
    cfg.prefer_warp = true;
    g_renderer = kernetile::KerneTileRenderer::create(cfg);

    if (!g_renderer) {
        std::cerr << "Failed to initialize KerneTile renderer!" << std::endl;
        return 1;
    }

    // 2. 构建声明式 UI
    g_ui_root = build_desktop_ui();
    g_root_render_box = g_ui_root->create_render_node();

    // 3. 初始首帧布局与离屏绘制，并输出高清位图成品
    std::cout << "[Render] 正在渲染桌面级高信息密度首帧 (" << g_win_w << "x" << g_win_h << ")..." << std::endl;
    render_and_present(nullptr);

    const char* bmp_path = "KTUI_Studio_Preview.bmp";
    save_bmp_image(bmp_path, g_win_w, g_win_h, g_framebuffer.data());
    std::cout << "[Export] 成功导出首帧高清位图成品至: " << bmp_path << std::endl;

    if (headless_capture_only) {
        std::cout << "[Done] 截图生成完毕，无头模式退出。" << std::endl;
        return 0;
    }

    // 4. 创建 Windows 原生窗口进行实时桌面互动
    HINSTANCE hinstance = GetModuleHandleW(nullptr);
    const wchar_t* kClass = L"KTUI_Studio_Window_Class";

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinstance;
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClass,
        L"KerneTile Studio (KTUI Desktop - High Information Density)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, g_win_w, g_win_h,
        nullptr, nullptr, hinstance, nullptr
    );

    if (!hwnd) {
        std::cerr << "Failed to create native desktop window!" << std::endl;
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    std::cout << "[Window] 真实桌面交互窗口已启动！支持鼠标 Hover、点击编译、输入文本与滑块互动。" << std::endl;

    // 消息泵
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
