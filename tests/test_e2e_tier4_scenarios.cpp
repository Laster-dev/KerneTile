#include "e2e_test_common.hpp"
#include "e2e_d3d12_harness.hpp"

#include <ktui/ktui.hpp>
#if __has_include("pal/headless_backend.hpp")
#include "pal/headless_backend.hpp"
#else
#include "../KTUI/src/pal/headless_backend.hpp"
#endif

#include <kernetile/renderer.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace kernetile_e2e;

namespace tier4_detail {

constexpr uint32_t pack_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255u) noexcept {
    return static_cast<uint32_t>(r) |
           (static_cast<uint32_t>(g) << 8u) |
           (static_cast<uint32_t>(b) << 16u) |
           (static_cast<uint32_t>(a) << 24u);
}

std::array<uint32_t, 16> make_dark_mica_grid() {
    std::array<uint32_t, 16> grid{};
    const uint8_t cols[4] = {18, 22, 26, 30};
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            uint8_t v = cols[x];
            grid[y * 4 + x] = pack_rgba8(v, static_cast<uint8_t>(v + 1), static_cast<uint8_t>(v + 3), 255u);
        }
    }
    return grid;
}

template <typename Cfg>
void set_dithering_cfg_opt(Cfg& cfg, bool enabled) {
    if constexpr (HasRendererDitheringConfig<Cfg>) {
        cfg.enable_dithering = enabled;
    } else {
        (void)cfg; (void)enabled;
    }
}

template <typename Win>
void set_window_dithering_opt(Win& win, bool enabled) {
    if constexpr (HasWindowDitheringApi<Win>) {
        win.set_dithering_enabled(enabled);
    } else {
        (void)win; (void)enabled;
    }
}

template <typename Win>
void set_window_snapping_opt(Win& win, bool enabled) {
    if constexpr (HasWindowPixelSnappingApi<Win>) {
        win.set_pixel_snapping_enabled(enabled);
    } else {
        (void)win; (void)enabled;
    }
}

template <typename Win>
bool get_window_dithering_opt(const Win& win) {
    if constexpr (HasWindowDitheringApi<Win>) {
        return static_cast<bool>(win.dithering_enabled());
    } else {
        (void)win;
        return false;
    }
}

template <typename Win>
bool get_window_snapping_opt(const Win& win) {
    if constexpr (HasWindowPixelSnappingApi<Win>) {
        return static_cast<bool>(win.pixel_snapping_enabled());
    } else {
        (void)win;
        return false;
    }
}

template <typename Txt>
void set_tabular_nums_opt(Txt& txt, bool enabled) {
    if constexpr (HasTabularNumsField<Txt>) {
        txt.tabular_nums = enabled;
    } else if constexpr (HasSetTabularNums<Txt>) {
        txt.set_tabular_nums(enabled);
    } else {
        (void)txt; (void)enabled;
    }
}

template <typename Txt>
bool get_tabular_nums_opt(const Txt& txt) {
    if constexpr (HasTabularNumsField<Txt>) {
        return static_cast<bool>(txt.tabular_nums);
    } else {
        (void)txt;
        return false;
    }
}

template <typename FlagsT>
kernetile::core::UiPrimitive make_reveal_prim(float bx0, float by0, float bx1, float by1,
                                              float cx, float cy, float radius,
                                              float r, float g, float b, float a) {
    kernetile::core::UiPrimitive p{};
    p.min_x = std::max(bx0, cx - radius);
    p.min_y = std::max(by0, cy - radius);
    p.max_x = std::min(bx1, cx + radius);
    p.max_y = std::min(by1, cy + radius);
    p.color_r = r * a;
    p.color_g = g * a;
    p.color_b = b * a;
    p.color_a = a;
    p.shadow_offset_x = cx;
    p.shadow_offset_y = cy;
    p.shadow_blur = radius;
    if constexpr (HasPointerRevealFlag<FlagsT>) {
        p.flags = static_cast<uint16_t>(FlagsT::IsPointerReveal);
    }
    return p;
}

std::vector<uint8_t> render_cpu_frame(kernetile::KerneTileRenderer& ren, uint32_t w, uint32_t h) {
    std::vector<uint8_t> out(static_cast<size_t>(w) * h * 4u, 0u);
    ren.render_to_rgba8(out.data());
    return out;
}

std::vector<uint8_t> read_headless_rgba8(const ktui::HeadlessWindow& hw) {
    const auto& fb = hw.virtual_framebuffer();
    std::vector<uint8_t> out(fb.size() * 4u);
    std::memcpy(out.data(), fb.data(), out.size());
    return out;
}

struct DiffRegion {
    size_t count{0};
    uint32_t min_x{0}, min_y{0}, max_x{0}, max_y{0};
    uint32_t area() const noexcept {
        return count == 0 ? 0u : (max_x - min_x + 1u) * (max_y - min_y + 1u);
    }
};

DiffRegion measure_diff_region(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                               uint32_t w, uint32_t h) {
    KT_E2E_CHECK(a.size() == static_cast<size_t>(w) * h * 4u);
    KT_E2E_CHECK(b.size() == static_cast<size_t>(w) * h * 4u);
    DiffRegion dr{};
    dr.min_x = w;
    dr.min_y = h;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4u;
            if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2] || a[i + 3] != b[i + 3]) {
                dr.min_x = std::min(dr.min_x, x);
                dr.min_y = std::min(dr.min_y, y);
                dr.max_x = std::max(dr.max_x, x);
                dr.max_y = std::max(dr.max_y, y);
                ++dr.count;
            }
        }
    }
    return dr;
}

std::string read_gallery_source_corpus() {
    std::string corpus = read_workspace_file("examples/ktui_gallery/main.cpp");
    corpus += "\n";
    corpus += read_workspace_file("examples/ktui_gallery/gallery_app.cpp");
    const std::array<const char*, 11> pages = {
        "examples/ktui_gallery/pages/home_page.cpp",
        "examples/ktui_gallery/pages/settings_about_pages.cpp",
        "examples/ktui_gallery/pages/polish_showcase_page.cpp",
        "examples/ktui_gallery/pages/squircle_page.cpp",
        "examples/ktui_gallery/pages/basic_input_pages.cpp",
        "examples/ktui_gallery/pages/layout_pages.cpp",
        "examples/ktui_gallery/pages/pro_controls_pages.cpp",
        "examples/ktui_gallery/pages/geek_controls_pages.cpp",
        "examples/ktui_gallery/pages/progress_page.cpp",
        "examples/ktui_gallery/pages/dialogs_menus_page.cpp",
        "examples/ktui_gallery/pages/media_desktop_pages.cpp"
    };
    for (const char* p : pages) {
        corpus += "\n";
        corpus += read_workspace_file(p);
    }
    return corpus;
}

// Custom RenderBox emitting a horizontal divider line and a 1px bordered panel
// for testing R2 fractional DPI line/border snapping inside a real KTUI Window.
class RenderDashboardDivider : public ktui::RenderBox {
public:
    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 400.0f;
        set_size(m_constraints.constrain(ktui::Vec2{w, 16.0f}));
    }

    void paint(ktui::PaintContext& ctx) override {
        const ktui::Color line_col{220.0f / 255.0f, 220.0f / 255.0f, 225.0f / 255.0f, 1.0f};
        ctx.draw_line(ktui::PointF{0.0f, 8.3f}, ktui::PointF{size().x, 8.3f}, line_col, 1.0f);
    }
};

class DashboardDividerWidget : public ktui::Widget {
public:
    std::shared_ptr<ktui::RenderBox> create_render_node() override {
        return ktui::make_render_node<RenderDashboardDivider>();
    }
    void update_render_node(std::shared_ptr<ktui::RenderBox>) override {}
};

#if defined(_WIN32)
std::string locate_gallery_executable() {
    std::vector<std::string> candidates;
#ifdef KT_GALLERY_EXE
    candidates.emplace_back(KT_GALLERY_EXE);
#endif
#if defined(NDEBUG)
    candidates.emplace_back("build-clang/ktui_gallery.exe");
    candidates.emplace_back("d:/KerneTile/build-clang/ktui_gallery.exe");
    candidates.emplace_back("build-clang/examples/ktui_gallery/ktui_gallery.exe");
    candidates.emplace_back("d:/KerneTile/build-clang/examples/ktui_gallery/ktui_gallery.exe");
    candidates.emplace_back("../build-clang/ktui_gallery.exe");
#else
    candidates.emplace_back("build-clang-debug/ktui_gallery.exe");
    candidates.emplace_back("d:/KerneTile/build-clang-debug/ktui_gallery.exe");
    candidates.emplace_back("build-clang-debug/examples/ktui_gallery/ktui_gallery.exe");
    candidates.emplace_back("d:/KerneTile/build-clang-debug/examples/ktui_gallery/ktui_gallery.exe");
    candidates.emplace_back("../build-clang-debug/ktui_gallery.exe");
    candidates.emplace_back("build-clang/ktui_gallery.exe");
    candidates.emplace_back("d:/KerneTile/build-clang/ktui_gallery.exe");
#endif
    candidates.emplace_back("ktui_gallery.exe");
    candidates.emplace_back("examples/ktui_gallery/ktui_gallery.exe");

    for (const auto& p : candidates) {
        if (p.empty()) continue;
        const DWORD attrs = GetFileAttributesA(p.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return p;
        }
    }
    return {};
}

struct GalleryAutomationSession {
    HANDLE stdin_write{nullptr};
    HANDLE stdout_read{nullptr};
    PROCESS_INFORMATION pi{};
    bool started{false};
    std::string rx_buffer;

    ~GalleryAutomationSession() {
        if (stdin_write) {
            CloseHandle(stdin_write);
            stdin_write = nullptr;
        }
        if (stdout_read) {
            CloseHandle(stdout_read);
            stdout_read = nullptr;
        }
        if (started) {
            if (pi.hProcess) {
                if (WaitForSingleObject(pi.hProcess, 500) == WAIT_TIMEOUT) {
                    TerminateProcess(pi.hProcess, 1);
                }
                CloseHandle(pi.hProcess);
                pi.hProcess = nullptr;
            }
            if (pi.hThread) {
                CloseHandle(pi.hThread);
                pi.hThread = nullptr;
            }
        }
    }

    bool launch(const std::string& exe_path) {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE stdout_write = nullptr;
        HANDLE stdin_read = nullptr;
        if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) return false;
        if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(stdout_write);
            return false;
        }
        if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
            CloseHandle(stdout_write);
            return false;
        }
        if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(stdout_write);
            CloseHandle(stdin_read);
            return false;
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = stdin_read;
        si.hStdOutput = stdout_write;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        std::string cmdline = "\"" + exe_path + "\" --automation";
        BOOL ok = CreateProcessA(
            nullptr,
            cmdline.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi);

        CloseHandle(stdout_write);
        CloseHandle(stdin_read);
        if (!ok) {
            return false;
        }
        started = true;
        return true;
    }

    bool send_command(const std::string& cmd, std::string& out_reply, DWORD timeout_ms = 8000) {
        out_reply.clear();
        std::string wire = cmd + "\n";
        DWORD written = 0;
        if (!WriteFile(stdin_write, wire.data(), static_cast<DWORD>(wire.size()), &written, nullptr) ||
            written != wire.size()) {
            return false;
        }

        const ULONGLONG deadline = GetTickCount64() + timeout_ms;
        for (;;) {
            const size_t nl = rx_buffer.find('\n');
            if (nl != std::string::npos) {
                out_reply = rx_buffer.substr(0, nl);
                rx_buffer.erase(0, nl + 1);
                while (!out_reply.empty() && out_reply.back() == '\r') {
                    out_reply.pop_back();
                }
                return true;
            }

            DWORD avail = 0;
            if (!PeekNamedPipe(stdout_read, nullptr, 0, nullptr, &avail, nullptr)) {
                return false;
            }
            if (avail > 0) {
                char tmp[1024];
                DWORD to_read = std::min<DWORD>(avail, sizeof(tmp));
                DWORD got = 0;
                if (!ReadFile(stdout_read, tmp, to_read, &got, nullptr) || got == 0) {
                    return false;
                }
                rx_buffer.append(tmp, tmp + got);
                continue;
            }

            if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
                return false;
            }
            if (GetTickCount64() >= deadline) {
                return false;
            }
            Sleep(10);
        }
    }

    DWORD wait_exit(DWORD timeout_ms = 5000) {
        if (stdin_write) {
            CloseHandle(stdin_write);
            stdin_write = nullptr;
        }
        DWORD wr = WaitForSingleObject(pi.hProcess, timeout_ms);
        KT_E2E_CHECK_MSG(wr == WAIT_OBJECT_0, "ktui_gallery --automation did not exit cleanly after QUIT");
        DWORD code = 1;
        KT_E2E_CHECK(GetExitCodeProcess(pi.hProcess, &code) != FALSE);
        return code;
    }
};

struct ControlCenter {
    float x{0.0f};
    float y{0.0f};
};

std::vector<ControlCenter> parse_list_centers(const std::string& list_reply, size_t max_count) {
    std::vector<ControlCenter> out;
    size_t pos = 0;
    while (out.size() < max_count) {
        size_t cx_pos = list_reply.find("\"cx\":", pos);
        if (cx_pos == std::string::npos) break;
        size_t cy_pos = list_reply.find("\"cy\":", cx_pos);
        if (cy_pos == std::string::npos) break;
        float cx = std::strtof(list_reply.c_str() + cx_pos + 5, nullptr);
        float cy = std::strtof(list_reply.c_str() + cy_pos + 5, nullptr);
        if (cx > 1.0f && cy > 1.0f) {
            out.push_back(ControlCenter{cx, cy});
        }
        pos = cy_pos + 5;
    }
    return out;
}
#endif

// ============================================================================
// T4_APP_01: ktui_gallery --automation Interactive Dark-Mode Showcase Session
// ============================================================================
CaseResult t4_app_01_gallery_automation_showcase_session() {
#if !defined(_WIN32)
    return passed_case();
#else
    const std::string corpus = read_gallery_source_corpus();
    const bool has_r1 = (corpus.find("set_dithering_enabled") != std::string::npos) ||
                        (corpus.find("enable_dithering") != std::string::npos);
    const bool has_r2 = (corpus.find("set_pixel_snapping_enabled") != std::string::npos) ||
                        (corpus.find("enable_pixel_snapping") != std::string::npos) ||
                        (corpus.find("set_pixel_snapping") != std::string::npos);
    const bool has_r3 = (corpus.find("enable_pointer_reveal") != std::string::npos) ||
                        (corpus.find("pointer_reveal(") != std::string::npos);
    const bool has_r4 = (corpus.find("rubber_band_overscroll") != std::string::npos) ||
                        (corpus.find("set_rubber_band_enabled") != std::string::npos);
    const bool has_r5 = (corpus.find("tabular_nums") != std::string::npos);

    const std::string exe_path = locate_gallery_executable();
    if (exe_path.empty()) {
#if KT_E2E_REQUIRE_ALL == 0
        return pending_case("ktui_gallery.exe not built/found on disk yet");
#else
        KT_E2E_CHECK_MSG(false, "ktui_gallery.exe missing when KT_E2E_REQUIRE_ALL!=0");
#endif
    }

    GalleryAutomationSession session;
    KT_E2E_CHECK_MSG(session.launch(exe_path), "Failed to launch ktui_gallery.exe --automation");

    std::string reply;
    KT_E2E_CHECK_MSG(session.send_command("PING", reply), "PING timed out");
    KT_E2E_CHECK_MSG(reply == "OK PONG", reply);

    KT_E2E_CHECK_MSG(session.send_command("INFO", reply), "INFO timed out");
    KT_E2E_CHECK_MSG(reply.rfind("OK {", 0) == 0, reply);
    KT_E2E_CHECK(reply.find("\"w\":") != std::string::npos);
    KT_E2E_CHECK(reply.find("\"h\":") != std::string::npos);
    KT_E2E_CHECK(reply.find("\"dpi\":") != std::string::npos);
    KT_E2E_CHECK(reply.find("\"page\":") != std::string::npos);
    KT_E2E_CHECK(reply.find("\"dark\":") != std::string::npos);

    KT_E2E_CHECK(session.send_command("THEME dark", reply) && reply.rfind("OK", 0) == 0);
    KT_E2E_CHECK(session.send_command("RESIZE 1280 800", reply) && reply.rfind("OK", 0) == 0);

    const std::array<const char*, 8> routes = {
        "home", "button", "settings_card", "navigation_view",
        "virtual_list", "data_grid", "progress_bar", "__settings__"
    };
    for (const char* route : routes) {
        KT_E2E_CHECK_MSG(session.send_command(std::string("NAV ") + route, reply), route);
        KT_E2E_CHECK_MSG(reply.rfind("OK", 0) == 0, reply);
    }

    // Exercise interactive PointerMove across showcase controls on button & settings_card
    for (const char* page : {"button", "settings_card"}) {
        KT_E2E_CHECK(session.send_command(std::string("NAV ") + page, reply) && reply.rfind("OK", 0) == 0);
        KT_E2E_CHECK(session.send_command("LIST", reply) && reply.rfind("OK [", 0) == 0);
        auto centers = parse_list_centers(reply, 4);
        KT_E2E_CHECK_MSG(!centers.empty(), "LIST returned zero interactive controls");
        for (const auto& pt : centers) {
            char cmd[64];
            std::snprintf(cmd, sizeof(cmd), "MOVE %.1f %.1f", pt.x, pt.y);
            KT_E2E_CHECK(session.send_command(cmd, reply) && reply.rfind("OK", 0) == 0);
        }
    }

    // Exercise SCROLL on virtual_list page
    KT_E2E_CHECK(session.send_command("NAV virtual_list", reply) && reply.rfind("OK", 0) == 0);
    KT_E2E_CHECK(session.send_command("LIST", reply) && reply.rfind("OK [", 0) == 0);
    KT_E2E_CHECK(session.send_command("SCROLL 640 420 -240", reply) && reply.rfind("OK", 0) == 0);
    KT_E2E_CHECK(session.send_command("SCROLL 640 420 240", reply) && reply.rfind("OK", 0) == 0);

    KT_E2E_CHECK(session.send_command("THEME light", reply) && reply.rfind("OK", 0) == 0);
    KT_E2E_CHECK(session.send_command("QUIT", reply) && reply.rfind("OK", 0) == 0);

    const DWORD exit_code = session.wait_exit();
    KT_E2E_CHECK_MSG(exit_code == 0u, "ktui_gallery --automation exited with non-zero status");

    if (!has_r1 || !has_r2 || !has_r3 || !has_r4 || !has_r5) {
#if KT_E2E_REQUIRE_ALL == 0
        return pending_case("ktui_gallery M3 showcase opt-in flags not all integrated yet");
#else
        KT_E2E_CHECK_MSG(has_r1, "ktui_gallery missing R1 dithering integration");
        KT_E2E_CHECK_MSG(has_r2, "ktui_gallery missing R2 pixel snapping integration");
        KT_E2E_CHECK_MSG(has_r3, "ktui_gallery missing R3 pointer reveal integration");
        KT_E2E_CHECK_MSG(has_r4, "ktui_gallery missing R4 rubber-band overscroll integration");
        KT_E2E_CHECK_MSG(has_r5, "ktui_gallery missing R5 tabular_nums integration");
#endif
    }
    return passed_case();
#endif
}

// ============================================================================
// T4_APP_02: High-DPI (125% & 150%) Settings & Dashboard Session
// ============================================================================
template <typename Win = ktui::Window,
          typename Ctx = ktui::PaintContext,
          typename Txt = ktui::Text,
          typename TM = ktui::TextMeasure>
CaseResult t4_app_02_high_dpi_settings_dashboard_session() {
    if constexpr (!HasWindowDitheringApi<Win> ||
                  !HasWindowPixelSnappingApi<Win> ||
                  !HasPaintContextSnappingApi<Ctx> ||
                  !HasTabularNumsField<Txt> ||
                  !HasTextMeasureTabular<TM, ktui::FontWeight>) {
        return pending_case("R1/R2/R5 APIs for High-DPI dashboard not landed yet");
    } else {
        for (float scale : {1.25f, 1.5f}) {
            auto run_dashboard = [&](bool enable_features) {
                ktui::Application app(ktui::create_platform_backend(true));
                app.set_theme(ktui::Theme::dark());
                ktui::WindowDesc wd{};
                wd.size = {520.0f, 360.0f};
                wd.custom_titlebar = false;
                auto win = app.create_window(wd);
                KT_E2E_CHECK(win != nullptr);
                win->set_backdrop(ktui::WindowBackdrop::Mica);
                auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());
                hw->set_dpi_scale(scale);
                set_window_dithering_opt(*win, enable_features);
                set_window_snapping_opt(*win, enable_features);

                Txt fps_a{.text = "111.1 FPS", .font_size = 14.0f};
                Txt fps_b{.text = "888.8 FPS", .font_size = 14.0f};
                set_tabular_nums_opt(fps_a, enable_features);
                set_tabular_nums_opt(fps_b, enable_features);

                ktui::SettingsCard card1{
                    .header = "Rendering Quality",
                    .description = "IGN Dithering + Snapping",
                    .content = ktui::HStack{
                        .spacing = 12.0f,
                        .content = {std::move(fps_a), std::move(fps_b)}
                    }
                };
                ktui::SettingsCard card2{
                    .header = "Display Scaling",
                    .description = "Crisp 1px borders and dividers"
                };

                win->set_content(ktui::VStack{
                    .spacing = 10.0f,
                    .padding = ktui::EdgeInsets(16.0f),
                    .content = {
                        std::move(card1),
                        ktui::make_widget<DashboardDividerWidget>(),
                        std::move(card2)
                    }
                });
                win->render_frame(true);
                return read_headless_rgba8(*hw);
            };

            auto fb_off = run_dashboard(false);
            auto fb_on = run_dashboard(true);
            const uint32_t pw = static_cast<uint32_t>(520.0f * scale);
            const uint32_t ph = static_cast<uint32_t>(360.0f * scale);
            KT_E2E_CHECK(fb_on.size() == static_cast<size_t>(pw) * ph * 4u);
            KT_E2E_CHECK_MSG(count_diff_pixels(fb_off, fb_on) > 100u,
                             "High-DPI dashboard with dithering + snapping must differ from baseline");

            float w1 = TM::measure_width("111.1 FPS", 14.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
            float w8 = TM::measure_width("888.8 FPS", 14.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
            KT_E2E_CHECK_MSG(std::abs(w1 - w8) < 1e-3f,
                             "Dashboard tabular FPS counter width must not jitter");
        }

        // Verify PaintContext divider line crispness at 1.25x & 1.5x over dark background
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 64;
        const std::array<uint8_t, 4> bg_u8{20, 20, 24, 255};
        const std::array<uint8_t, 4> ln_u8{220, 220, 225, 255};
        for (float scale : {1.25f, 1.5f}) {
            kernetile::RendererConfig cfg{};
            cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
            auto ren = kernetile::KerneTileRenderer::create(cfg);
            ren->begin_frame();
            Ctx ctx(ren.get(), scale);
            ctx.set_pixel_snapping(true);
            ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 100.0f, 50.0f),
                          ktui::Color{20.0f / 255.0f, 20.0f / 255.0f, 24.0f / 255.0f, 1.0f});
            ctx.draw_line(ktui::PointF{8.0f, 16.3f}, ktui::PointF{80.0f, 16.3f},
                          ktui::Color{220.0f / 255.0f, 220.0f / 255.0f, 225.0f / 255.0f, 1.0f}, 1.0f);
            ren->end_frame();
            auto img = render_cpu_frame(*ren, W, H);
            KT_E2E_CHECK_MSG(!has_fractional_color_bleed_col(img, W, H, 40, 12, 36, bg_u8, ln_u8),
                             "Dashboard horizontal divider must have zero fractional alpha bleed");
        }
        return passed_case();
    }
}

// ============================================================================
// T4_APP_03: Interactive Navigation & Control Reveal Workload
// ============================================================================
CaseResult t4_app_03_interactive_navigation_and_reveal_workload() {
    if constexpr (!has_pointer_reveal_api<ktui::NavigationView>() ||
                  !has_pointer_reveal_api<ktui::Button>() ||
                  !has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R3 pointer reveal widget APIs not landed yet");
    } else {
        constexpr uint32_t W = 720;
        constexpr uint32_t H = 480;
        ktui::Application app(ktui::create_platform_backend(true));
        app.set_theme(ktui::Theme::dark());
        ktui::WindowDesc wd{};
        wd.size = {static_cast<float>(W), static_cast<float>(H)};
        wd.custom_titlebar = false;
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::NavigationView nav{};
        nav.menu_items = {
            ktui::NavigationViewItem{.id = "home", .label = "Home", .icon = ktui::NavigationIcon::Home},
            ktui::NavigationViewItem{.id = "perf", .label = "Performance", .icon = ktui::NavigationIcon::Motion},
            ktui::NavigationViewItem{.id = "settings", .label = "Settings", .icon = ktui::NavigationIcon::Settings}
        };
        nav.selected_id = "home";
        nav.pane_display_mode = ktui::NavigationPaneDisplayMode::Left;
        nav.is_pane_open = true;
        nav.open_pane_length = 220.0f;
        set_pointer_reveal_opt(nav, true);

        nav.page_builder = [](const std::string& page_id) -> ktui::WidgetPtr {
            ktui::Button primary{.text = "Apply " + page_id, .variant = ktui::ButtonVariant::Accent};
            set_pointer_reveal_opt(primary, true);
            ktui::Button secondary{.text = "Reset"};
            set_pointer_reveal_opt(secondary, true);
            ktui::SettingsCard card{
                .header = "Reveal Card (" + page_id + ")",
                .description = "Tracks pointer highlight with 16x16 tile culling",
                .is_click_enabled = true
            };
            set_pointer_reveal_opt(card, true);

            return ktui::VStack{
                .spacing = 10.0f,
                .padding = ktui::EdgeInsets(20.0f),
                .content = {
                    ktui::HStack{.spacing = 10.0f, .content = {std::move(primary), std::move(secondary)}},
                    std::move(card)
                }
            };
        };

        win->set_content(std::move(nav));
        win->render_frame(true);

        auto sweep = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };

        // 1. Sweep across NavigationView sidebar items
        auto fb_nav1 = sweep(90.0f, 82.0f);
        auto fb_nav2 = sweep(130.0f, 84.0f);
        auto nav_diff = measure_diff_region(fb_nav1, fb_nav2, W, H);
        KT_E2E_CHECK_MSG(nav_diff.count > 0u, "NavigationView sidebar reveal must react to cursor sweep");
        KT_E2E_CHECK_MSG(nav_diff.area() < (W * H) / 4u, "NavigationView reveal invalidation must stay localized");

        // 2. Sweep across content Button and SettingsCard
        auto fb_btn1 = sweep(265.0f, 34.0f);
        auto fb_btn2 = sweep(295.0f, 34.0f);
        auto btn_diff = measure_diff_region(fb_btn1, fb_btn2, W, H);
        KT_E2E_CHECK_MSG(btn_diff.count > 0u, "Button reveal inside NavigationView must react to cursor sweep");
        KT_E2E_CHECK_MSG(btn_diff.area() < (W * H) / 4u, "Button reveal invalidation must stay localized");

        auto fb_card1 = sweep(320.0f, 92.0f);
        auto fb_card2 = sweep(440.0f, 92.0f);
        auto card_diff = measure_diff_region(fb_card1, fb_card2, W, H);
        KT_E2E_CHECK_MSG(card_diff.count > 0u, "SettingsCard reveal must react to cursor sweep");
        KT_E2E_CHECK_MSG(card_diff.area() < (W * H) / 4u, "SettingsCard reveal invalidation must stay localized");

        // 3. Move pointer outside window -> exit clears highlight cleanly
        auto fb_exit = sweep(-20.0f, -20.0f);
        KT_E2E_CHECK(count_diff_pixels(fb_card2, fb_exit) > 0u);
        return passed_case();
    }
}

// ============================================================================
// T4_APP_04: Long-List Touch Overscroll & Spring Rebound Workload
// ============================================================================
CaseResult t4_app_04_long_list_touch_overscroll_and_rebound() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>() ||
                  !has_rubber_band_api<ktui::ScrollView>() ||
                  !has_rubber_band_api<ktui::VirtualList>() ||
                  !has_pointer_reveal_api<ktui::Button>()) {
        return pending_case("R4 rubber-band or R3 Button reveal APIs not landed yet");
    } else {
        // 1. Exact formula check for multi-step touch drag on SmoothScrollPhysics
        constexpr float d = 420.0f;
        constexpr float c = 0.55f;
        ktui::SmoothScrollPhysics phys{};
        set_rubber_band_opt(phys, true);
        phys.set_viewport_extent(d);
        phys.set_max_scroll(1200.0f);
        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(40.0f, 0.016);
        phys.apply_touch_drag_with_dt(50.0f, 0.016);
        const float x_total = 90.0f;
        const float expected_top = (x_total * d * c) / (d + c * x_total);
        KT_E2E_CHECK_MSG(std::abs((-phys.offset()) - expected_top) < 1e-4f,
                         "Multi-step top touch drag must satisfy f(x)=(x*d*c)/(d+c*x) within 1e-4");
        phys.end_touch_drag_for_test(0.0);
        KT_E2E_CHECK(phys.is_bouncing());
        for (int i = 0; i < 180 && phys.step_touch_for_test(1.0 / 60.0); ++i) {}
        KT_E2E_CHECK(std::abs(phys.offset()) < 0.5f && !phys.is_bouncing());

        // 2. Full VirtualList (100 items) touch overscroll at top AND bottom + interleaved pointer reveal
        ktui::Application app(ktui::create_platform_backend(true));
        app.set_theme(ktui::Theme::dark());
        ktui::WindowDesc wd{};
        wd.size = {420.0f, 360.0f};
        wd.custom_titlebar = false;
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::VirtualList vl{
            .item_count = 100,
            .item_height = 40.0f,
            .width = 420.0f,
            .height = 360.0f,
            .item_builder = [](size_t idx) -> ktui::WidgetPtr {
                ktui::Button b{.text = "List Row #" + std::to_string(idx)};
                set_pointer_reveal_opt(b, true);
                return b;
            }
        };
        set_rubber_band_opt(vl, true);

        win->set_content(std::move(vl));
        win->render_frame(true);

        auto* rvl = static_cast<ktui::RenderVirtualList*>(win->root_render_box());
        KT_E2E_CHECK(rvl != nullptr);

        auto touch_ev = [&](ktui::PlatformEventType t, float y) {
            ktui::PlatformEvent pe{};
            pe.type = t;
            pe.device_kind = ktui::PointerDeviceKind::Touch;
            pe.button = ktui::PointerButton::Primary;
            pe.position = {160.0f, y};
            win->handle_platform_event(pe);
            win->render_frame();
        };

        // Top overscroll
        touch_ev(ktui::PlatformEventType::PointerDown, 120.0f);
        touch_ev(ktui::PlatformEventType::PointerMove, 140.0f);
        touch_ev(ktui::PlatformEventType::PointerMove, 220.0f);
        KT_E2E_CHECK(rvl->scroll_offset() < -10.0f);
        touch_ev(ktui::PlatformEventType::PointerUp, 220.0f);
        for (int i = 0; i < 70; ++i) {
            app.step(1.0f / 60.0f);
            win->render_frame();
            if (std::abs(rvl->scroll_offset()) < 0.5f) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        KT_E2E_CHECK(std::abs(rvl->scroll_offset()) < 2.0f);

        // Jump to bottom (max_scroll = 100*40 - 360 = 3640) and touch overscroll past bottom
        rvl->scroll_to(3640.0f);
        for (int i = 0; i < 80; ++i) {
            app.step(1.0f / 60.0f);
            win->render_frame();
            if (std::abs(rvl->scroll_offset() - 3640.0f) < 0.5f) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        touch_ev(ktui::PlatformEventType::PointerDown, 260.0f);
        touch_ev(ktui::PlatformEventType::PointerMove, 240.0f);
        touch_ev(ktui::PlatformEventType::PointerMove, 140.0f);
        KT_E2E_CHECK_MSG(rvl->scroll_offset() > 3645.0f, "VirtualList must overscroll past bottom boundary");
        touch_ev(ktui::PlatformEventType::PointerUp, 140.0f);
        for (int i = 0; i < 70; ++i) {
            app.step(1.0f / 60.0f);
            win->render_frame();
            if (std::abs(rvl->scroll_offset() - 3640.0f) < 0.5f) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        KT_E2E_CHECK(std::abs(rvl->scroll_offset() - 3640.0f) < 2.0f);

        // Interleaved pointer reveal on visible bottom item
        auto move_mouse = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };
        auto fb1 = move_mouse(60.0f, 300.0f);
        auto fb2 = move_mouse(180.0f, 300.0f);
        KT_E2E_CHECK_MSG(count_diff_pixels(fb1, fb2) > 5u,
                         "Pointer reveal must still function after bottom VirtualList rebound");
        return passed_case();
    }
}

// ============================================================================
// T4_APP_05: Multi-Backend Rendering Parity End-to-End Scene
// ============================================================================
template <typename Cfg = kernetile::RendererConfig,
          typename Ren = kernetile::KerneTileRenderer,
          typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult t4_app_05_multi_backend_rendering_parity_scene() {
    auto audit = audit_shader_files();
    KT_E2E_CHECK_MSG(audit.hlsl_found && audit.wgsl_found, "Shader files must exist");
    KT_E2E_CHECK_MSG(audit.hlsl_has_vk_path, "HLSL must include KT_VULKAN vk::binding path");

    if constexpr (!HasRendererDitheringConfig<Cfg> || !HasPointerRevealFlag<FlagsT>) {
        return pending_case("R1 dithering or R3 IsPointerReveal not landed yet");
    } else {
        KT_E2E_CHECK_MSG(audit.hlsl_has_ign_constants && audit.hlsl_has_render_flags && audit.hlsl_has_pointer_reveal,
                         "HLSL shader contract incomplete");
        KT_E2E_CHECK_MSG(audit.wgsl_has_ign_constants && audit.wgsl_has_render_flags && audit.wgsl_has_pointer_reveal,
                         "WGSL shader contract incomplete");

#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 192;
        const auto dark_mica = make_dark_mica_grid();

        // Card with Gaussian shadow + 1.5x snapped 1px border (2 physical px) + IsPointerReveal sheen
        kernetile::core::UiPrimitive shadow_card{};
        shadow_card.min_x = 36.0f; shadow_card.min_y = 30.0f;
        shadow_card.max_x = 220.0f; shadow_card.max_y = 156.0f;
        shadow_card.radius_top_left = shadow_card.radius_top_right =
            shadow_card.radius_bottom_right = shadow_card.radius_bottom_left = 6.0f;
        shadow_card.color_r = 0.16f; shadow_card.color_g = 0.17f;
        shadow_card.color_b = 0.20f; shadow_card.color_a = 1.0f;
        shadow_card.shadow_blur = 20.0f;
        shadow_card.shadow_offset_y = 4.0f;

        kernetile::core::UiPrimitive snapped_border{};
        snapped_border.min_x = 36.0f; snapped_border.min_y = 30.0f;
        snapped_border.max_x = 220.0f; snapped_border.max_y = 156.0f;
        snapped_border.radius_top_left = snapped_border.radius_top_right =
            snapped_border.radius_bottom_right = snapped_border.radius_bottom_left = 6.0f;
        snapped_border.color_r = 0.82f; snapped_border.color_g = 0.84f;
        snapped_border.color_b = 0.88f; snapped_border.color_a = 1.0f;
        snapped_border.border_width = 2.0f; // 1.0 logical px @ 1.5x DPI -> 2 physical px

        auto reveal = make_reveal_prim<FlagsT>(
            36.0f, 30.0f, 220.0f, 156.0f, 128.0f, 92.0f, 44.0f, 1.0f, 1.0f, 1.0f, 0.35f);

        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg_opt(cfg, true);
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, dark_mica);
        ren->begin_frame();
        ren->push_primitive(shadow_card);
        ren->push_primitive(snapped_border);
        ren->push_primitive(reveal);
        ren->end_frame();
        auto cpu_rgba = render_cpu_frame(*ren, W, H);

        D3D12OffscreenRunner warp;
        KT_E2E_CHECK_MSG(warp.is_ready(), "D3D12 WARP device/pipeline failed to initialize");
        D3D12SceneInput scene{};
        scene.width = W;
        scene.height = H;
        scene.primitives = {shadow_card, snapped_border, reveal};
        scene.mica_enabled = true;
        scene.mica_grid = dark_mica;
        scene.render_flags = 1u;
        auto gpu_rgba = warp.render(scene);

        auto cmp = compare_rgba8(cpu_rgba, gpu_rgba, W, H);
        KT_E2E_CHECK_MSG(cmp.max_lsb_diff <= 2, "T4_APP_05 CPU vs D3D12 WARP max LSB diff > 2");
        KT_E2E_CHECK_MSG(cmp.ssim >= 0.999, "T4_APP_05 CPU vs D3D12 WARP SSIM < 0.999");
#endif
        return passed_case();
    }
}

// ============================================================================
// T4_APP_06: Clean Default-Off Baseline & Release Binary Footprint Audit
// ============================================================================
template <typename Win = ktui::Window,
          typename Ctx = ktui::PaintContext,
          typename Txt = ktui::Text>
CaseResult t4_app_06_default_off_baseline_and_release_footprint() {
    // 1. Compile-time R7 checks: zero exceptions, zero RTTI, Release inspector off
#if defined(NDEBUG)
    KT_E2E_CHECK_MSG(KTUI_INSPECTOR_ACTIVE == 0, "Release build MUST have KTUI_INSPECTOR_ACTIVE == 0");
#else
    KT_E2E_CHECK(KTUI_INSPECTOR_ACTIVE == 0 || KTUI_INSPECTOR_ACTIVE == 1);
#endif
#if defined(_HAS_EXCEPTIONS)
    static_assert(_HAS_EXCEPTIONS == 0, "Must compile with _HAS_EXCEPTIONS=0");
#endif
#if defined(_HAS_STATIC_RTTI)
    static_assert(_HAS_STATIC_RTTI == 0, "Must compile with _HAS_STATIC_RTTI=0");
#endif
#if defined(_CPPUNWIND) || defined(__cpp_exceptions)
    static_assert(false, "Must compile with /EHs-c-");
#endif
#if defined(_CPPRTTI)
    static_assert(false, "Must compile with /GR-");
#endif

    // 2. Verify KTUI/CMakeLists.txt and ui_inspector.hpp strictly gate DevTools to Debug
    const std::string ktui_cmake = read_workspace_file("KTUI/CMakeLists.txt");
    const std::string inspector_hpp = read_workspace_file("KTUI/include/ktui/app/ui_inspector.hpp");
    KT_E2E_CHECK(!ktui_cmake.empty() && !inspector_hpp.empty());
    KT_E2E_CHECK(ktui_cmake.find("$<$<CONFIG:Debug>:KTUI_ENABLE_INSPECTOR=1>") != std::string::npos);
    KT_E2E_CHECK(inspector_hpp.find("!defined(NDEBUG)") != std::string::npos);

    // 3. Audit Release ktui_gallery.exe binary if built on disk for zero DevTools bloat
#if defined(_WIN32)
    for (const char* rel_exe : {"build-clang/examples/ktui_gallery/ktui_gallery.exe",
                                "d:/KerneTile/build-clang/examples/ktui_gallery/ktui_gallery.exe"}) {
        std::ifstream ifs(rel_exe, std::ios::binary);
        if (ifs.good()) {
            std::string bin((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            KT_E2E_CHECK(!bin.empty());
            KT_E2E_CHECK_MSG(bin.find("KTUI DevTools") == std::string::npos,
                             "Release ktui_gallery.exe must not contain Debug DevTools strings");
            KT_E2E_CHECK_MSG(bin.find("DevToolsWindowHost") == std::string::npos,
                             "Release ktui_gallery.exe must not contain DevToolsWindowHost");
            break;
        }
    }
#endif

    // 4. Full multi-widget default-off Window session: defaults must all be false & bit-exact
    ktui::Application app(ktui::create_platform_backend(true));
    app.set_theme(ktui::Theme::dark());
    ktui::WindowDesc wd{};
    wd.size = {480.0f, 320.0f};
    wd.custom_titlebar = false;
    auto win = app.create_window(wd);
    KT_E2E_CHECK(win != nullptr);
    auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());
    hw->set_dpi_scale(1.25f);

    KT_E2E_CHECK(get_window_dithering_opt(*win) == false);
    KT_E2E_CHECK(get_window_snapping_opt(*win) == false);

    ktui::Button def_btn{.text = "Baseline"};
    ktui::NavigationView def_nav{};
    ktui::SettingsCard def_card{.header = "Baseline Card"};
    ktui::ScrollView def_sv{};
    ktui::VirtualList def_vl{};
    ktui::SmoothScrollPhysics def_phys{};
    Txt def_txt1{.text = "11111", .font_size = 16.0f};
    Txt def_txt8{.text = "88888", .font_size = 16.0f};

    KT_E2E_CHECK(get_pointer_reveal_default(def_btn) == false);
    KT_E2E_CHECK(get_pointer_reveal_default(def_nav) == false);
    KT_E2E_CHECK(get_pointer_reveal_default(def_card) == false);
    KT_E2E_CHECK(get_rubber_band_opt(def_sv) == false);
    KT_E2E_CHECK(get_rubber_band_opt(def_vl) == false);
    KT_E2E_CHECK(get_rubber_band_opt(def_phys) == false);
    KT_E2E_CHECK(get_tabular_nums_opt(def_txt1) == false);

    // Proportional digits remain unchanged by default
    std::string_view prop_fam = "Georgia";
    bool found_prop = false;
    for (std::string_view cand : {"Georgia", "Candara", "Constantia", "Corbel", "Bahnschrift", "Times New Roman", "Liberation Sans", "DejaVu Sans", "Segoe UI"}) {
        float c1 = ktui::TextMeasure::measure_width("11111", 16.0f, ktui::FontWeight::Regular, false, cand, false);
        float c8 = ktui::TextMeasure::measure_width("88888", 16.0f, ktui::FontWeight::Regular, false, cand, false);
        if (c8 > c1 + 1.0f) {
            prop_fam = cand;
            found_prop = true;
            break;
        }
    }
    float w1 = ktui::TextMeasure::measure_width("11111", 16.0f, ktui::FontWeight::Regular, false, prop_fam);
    float w8 = ktui::TextMeasure::measure_width("88888", 16.0f, ktui::FontWeight::Regular, false, prop_fam);
#if defined(_WIN32)
    KT_E2E_CHECK(found_prop);
#endif
    if (found_prop) {
        KT_E2E_CHECK_MSG(w8 > w1 + 1.0f, "Default proportional digit widths must remain distinct");
    } else {
        KT_E2E_CHECK(w1 > 5.0f && w8 > 5.0f);
    }

    // Scroll clamping remains strict by default
    def_phys.set_viewport_extent(300.0f);
    def_phys.set_max_scroll(500.0f);
    def_phys.jump_to(0.0f);
    def_phys.begin_touch_drag();
    def_phys.apply_touch_drag_with_dt(120.0f, 0.016);
    KT_E2E_CHECK(def_phys.offset() == 0.0f && def_phys.overscroll_amount() == 0.0f);

    // Render default Window and verify bit-exact repeatability
    win->set_content(ktui::VStack{
        .spacing = 8.0f,
        .padding = ktui::EdgeInsets(12.0f),
        .content = {
            std::move(def_btn),
            std::move(def_card),
            std::move(def_txt1),
            std::move(def_txt8)
        }
    });
    win->render_frame(true);
    const auto fb_a = read_headless_rgba8(*hw);
    win->render_frame(true);
    const auto fb_b = read_headless_rgba8(*hw);
    KT_E2E_CHECK(fnv1a_rgba8(fb_a) == fnv1a_rgba8(fb_b));
    return passed_case();
}

} // namespace tier4_detail

using namespace tier4_detail;

int main() {
    TestRunner runner("KerneTile & KTUI E2E Tier 4 Real-World Scenarios (T4_APP_01..06)");

    runner.run_case("T4_APP_01_gallery_automation_showcase_session", t4_app_01_gallery_automation_showcase_session);
    runner.run_case("T4_APP_02_high_dpi_settings_dashboard_session", t4_app_02_high_dpi_settings_dashboard_session<>);
    runner.run_case("T4_APP_03_interactive_navigation_and_reveal_workload", t4_app_03_interactive_navigation_and_reveal_workload);
    runner.run_case("T4_APP_04_long_list_touch_overscroll_and_rebound", t4_app_04_long_list_touch_overscroll_and_rebound);
    runner.run_case("T4_APP_05_multi_backend_rendering_parity_scene", t4_app_05_multi_backend_rendering_parity_scene<>);
    runner.run_case("T4_APP_06_default_off_baseline_and_release_footprint", t4_app_06_default_off_baseline_and_release_footprint<>);

    KT_E2E_CHECK(runner.total() == 6u);
    return runner.finalize();
}
