#include "e2e_test_common.hpp"
#include "e2e_d3d12_harness.hpp"

#include <ktui/ktui.hpp>
#if __has_include("pal/headless_backend.hpp")
#include "pal/headless_backend.hpp"
#else
#include "../KTUI/src/pal/headless_backend.hpp"
#endif

#include <kernetile/renderer.hpp>
#include <kernetile/text/font_engine.hpp>
#include <kernetile/text/text_streamer.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace kernetile_e2e;

namespace tier3_detail {

constexpr uint32_t pack_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255u) noexcept {
    return static_cast<uint32_t>(r) |
           (static_cast<uint32_t>(g) << 8u) |
           (static_cast<uint32_t>(b) << 16u) |
           (static_cast<uint32_t>(a) << 24u);
}

std::array<uint32_t, 16> make_horizontal_ramp_mica(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3,
                                                   uint8_t g_offset = 0, uint8_t b_offset = 0) {
    std::array<uint32_t, 16> grid{};
    const uint8_t cols[4] = {c0, c1, c2, c3};
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            uint8_t r = cols[x];
            uint8_t g = static_cast<uint8_t>(std::min<int>(255, r + g_offset));
            uint8_t b = static_cast<uint8_t>(std::min<int>(255, r + b_offset));
            grid[y * 4 + x] = pack_rgba8(r, g, b, 255u);
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

template <typename Cfg>
void set_snapping_cfg_opt(Cfg& cfg, bool enabled) {
    if constexpr (HasRendererPixelSnappingConfig<Cfg>) {
        cfg.enable_pixel_snapping = enabled;
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

template <typename Ctx>
void set_ctx_snapping_opt(Ctx& ctx, bool enabled) {
    if constexpr (HasPaintContextSnappingApi<Ctx>) {
        ctx.set_pixel_snapping(enabled);
    } else {
        (void)ctx; (void)enabled;
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
                               uint32_t w, uint32_t h, uint32_t tol = 0u) {
    KT_E2E_CHECK(a.size() == static_cast<size_t>(w) * h * 4u);
    KT_E2E_CHECK(b.size() == static_cast<size_t>(w) * h * 4u);
    DiffRegion dr{};
    dr.min_x = w;
    dr.min_y = h;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4u;
            bool d = (static_cast<uint32_t>(std::abs(int(a[i]) - int(b[i]))) > tol) ||
                     (static_cast<uint32_t>(std::abs(int(a[i + 1]) - int(b[i + 1]))) > tol) ||
                     (static_cast<uint32_t>(std::abs(int(a[i + 2]) - int(b[i + 2]))) > tol) ||
                     (static_cast<uint32_t>(std::abs(int(a[i + 3]) - int(b[i + 3]))) > tol);
            if (d) {
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

// ============================================================================
// T3_PAIR_01: R1 IGN Dithering + Dark Mica 4x4 + Analytical Gaussian Shadow
//             + CPU vs D3D12 WARP parity
// ============================================================================
template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult t3_pair_01_ign_mica_gaussian_shadow_parity() {
    if constexpr (!HasRendererDitheringConfig<Cfg> || !HasRendererDitheringApi<Ren>) {
        return pending_case("R1 Renderer dithering API not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 192;
        const auto dark_mica = make_horizontal_ramp_mica(18, 22, 26, 30, 1, 3);

        auto render_cpu_scene = [&](bool dither) {
            Cfg cfg{};
            cfg.width = W;
            cfg.height = H;
            cfg.force_cpu_fallback = true;
            set_dithering_cfg_opt(cfg, dither);
            auto ren = Ren::create(cfg);
            KT_E2E_CHECK(ren != nullptr);
            ren->set_mica_backdrop(true, dark_mica);
            ren->begin_frame();
            ren->draw_shadowed_box({64.0f, 64.0f, 192.0f, 152.0f},
                                   {0.18f, 0.19f, 0.22f, 1.0f}, 8.0f, 14.0f, 0.0f, {0.0f, 4.0f});
            ren->end_frame();
            return render_cpu_frame(*ren, W, H);
        };

        auto cpu_off = render_cpu_scene(false);
        auto cpu_on = render_cpu_scene(true);
        KT_E2E_CHECK_MSG(count_diff_pixels(cpu_off, cpu_on) > 200u,
                         "IGN dithering must alter dark Mica + Gaussian shadow ramp");

        // 采样顶部 y=2（距离 y=64 卡片 > 4*sigma，完全无阴影叠加）验证纯 Mica 4x4 线性插值 + IGN 抖动
        auto mica_row_off = extract_row_channel(cpu_off, W, 2, 0, W, 0);
        auto mica_row_on = extract_row_channel(cpu_on, W, 2, 0, W, 0);
        auto st_off = analyze_gradient_banding(mica_row_off, 18.0 + 12.0 * (0.5 / W), 18.0 + 12.0 * ((W - 0.5) / W));
        auto st_on = analyze_gradient_banding(mica_row_on, 18.0 + 12.0 * (0.5 / W), 18.0 + 12.0 * ((W - 0.5) / W));
        KT_E2E_CHECK(st_on.monotonic_blocks);
        KT_E2E_CHECK(st_on.max_lsb_error <= 1.001);
        KT_E2E_CHECK(st_on.max_flat_run < st_off.max_flat_run);

#if defined(_WIN32) && !defined(KERNETILE_WINDOWS_PURE_GDI)
        kernetile::core::UiPrimitive card{};
        card.min_x = 64.0f; card.min_y = 64.0f; card.max_x = 192.0f; card.max_y = 152.0f;
        card.radius_top_left = card.radius_top_right = card.radius_bottom_right = card.radius_bottom_left = 8.0f;
        card.color_r = 0.18f; card.color_g = 0.19f; card.color_b = 0.22f; card.color_a = 1.0f;
        card.shadow_blur = 14.0f;
        card.shadow_offset_y = 4.0f;

        D3D12OffscreenRunner warp;
        KT_E2E_CHECK_MSG(warp.is_ready(), "D3D12 WARP device/pipeline failed");
        D3D12SceneInput scene{};
        scene.width = W;
        scene.height = H;
        scene.primitives = {card};
        scene.mica_enabled = true;
        scene.mica_grid = dark_mica;
        scene.render_flags = 1u;
        auto gpu_on = warp.render(scene);

        auto cmp = compare_rgba8(cpu_on, gpu_on, W, H);
        KT_E2E_CHECK_MSG(cmp.max_lsb_diff <= 1, "T3_PAIR_01 CPU vs D3D12 WARP max LSB diff > 1");
        KT_E2E_CHECK_MSG(cmp.ssim >= 0.999, "T3_PAIR_01 CPU vs D3D12 WARP SSIM < 0.999");
#endif
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_02: R1 IGN Dithering + R2 Fractional DPI Snapping (1.25x & 1.5x)
//             + CPU vs D3D12 WARP parity
// ============================================================================
template <typename Cfg = kernetile::RendererConfig,
          typename Ren = kernetile::KerneTileRenderer,
          typename Ctx = ktui::PaintContext>
CaseResult t3_pair_02_ign_plus_fractional_dpi_snapping() {
    if constexpr (!HasRendererDitheringConfig<Cfg> ||
                  !HasRendererPixelSnappingConfig<Cfg> ||
                  !HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R1 dithering or R2 pixel snapping APIs not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 128;
        const auto mica = make_horizontal_ramp_mica(18, 22, 26, 30);
        const ktui::Color bd_col{230.0f / 255.0f, 230.0f / 255.0f, 235.0f / 255.0f, 1.0f};
        const std::array<uint8_t, 4> bd_u8{230, 230, 235, 255};

        for (float scale : {1.25f, 1.5f}) {
            auto render_pair = [&](bool dither) {
                Cfg cfg{};
                cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
                set_dithering_cfg_opt(cfg, dither);
                set_snapping_cfg_opt(cfg, true);
                auto ren = Ren::create(cfg);
                ren->set_mica_backdrop(true, mica);
                ren->begin_frame();
                Ctx ctx(ren.get(), scale);
                set_ctx_snapping_opt(ctx, true);
                ctx.draw_rect(ktui::Rect::from_xywh(20.3f, 20.3f, 80.0f, 48.0f), bd_col, 0.0f, 1.0f);
                ren->end_frame();
                return render_cpu_frame(*ren, W, H);
            };

            auto img_no_dither = render_pair(false);
            auto img_dither = render_pair(true);

            // Backdrop outside the box must be dithered
            auto row_off = extract_row_channel(img_no_dither, W, 6, 0, W, 0);
            auto row_on = extract_row_channel(img_dither, W, 6, 0, W, 0);
            auto st_off = analyze_gradient_banding(row_off, 18.0, 30.0);
            auto st_on = analyze_gradient_banding(row_on, 18.0, 30.0);
            KT_E2E_CHECK(st_on.max_flat_run < st_off.max_flat_run);
            KT_E2E_CHECK(st_on.spatial_dither_transitions >= 12u);

            // Snapped 1.0px border must remain exact physical pixel thickness (1px @ 1.25x, 2px @ 1.5x)
            const uint32_t expected_px = (scale > 1.3f) ? 2u : 1u;
            const uint32_t sample_x = static_cast<uint32_t>(50.0f * scale);
            const uint32_t y_lo = static_cast<uint32_t>(18.0f * scale);
            const uint32_t y_hi = static_cast<uint32_t>(30.0f * scale);
            const uint32_t border_px = count_matching_pixels_col(img_dither, W, H, sample_x, y_lo, y_hi, bd_u8, 2u);
            KT_E2E_CHECK_MSG(border_px == expected_px,
                             "Snapped 1px border over dithered backdrop must preserve physical thickness");
        }

#if defined(_WIN32) && !defined(KERNETILE_WINDOWS_PURE_GDI)
        kernetile::core::UiPrimitive snapped_border{};
        snapped_border.min_x = 30.0f; snapped_border.min_y = 30.0f;
        snapped_border.max_x = 150.0f; snapped_border.max_y = 90.0f;
        snapped_border.color_r = 230.0f / 255.0f;
        snapped_border.color_g = 230.0f / 255.0f;
        snapped_border.color_b = 235.0f / 255.0f;
        snapped_border.color_a = 1.0f;
        snapped_border.border_width = 2.0f; // 1.5x snapped 1px border = 2 physical px

        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg_opt(cfg, true);
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, mica);
        ren->begin_frame();
        ren->push_primitive(snapped_border);
        ren->end_frame();
        auto cpu_rgba = render_cpu_frame(*ren, W, H);

        D3D12OffscreenRunner warp;
        KT_E2E_CHECK(warp.is_ready());
        D3D12SceneInput scene{};
        scene.width = W; scene.height = H;
        scene.primitives = {snapped_border};
        scene.mica_enabled = true;
        scene.mica_grid = mica;
        scene.render_flags = 1u;
        auto gpu_rgba = warp.render(scene);

        auto cmp = compare_rgba8(cpu_rgba, gpu_rgba, W, H);
        KT_E2E_CHECK_MSG(cmp.max_lsb_diff <= 1 && cmp.ssim >= 0.999,
                         "T3_PAIR_02 dithered Mica + snapped border CPU vs D3D12 WARP parity failed");
#endif
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_03: R2 Fractional DPI Snapping (1.5x) + R3 Pointer Reveal
// ============================================================================
template <typename Win = ktui::Window,
          typename Ctx = ktui::PaintContext,
          typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult t3_pair_03_dpi_snapping_plus_pointer_reveal() {
    if constexpr (!HasWindowPixelSnappingApi<Win> ||
                  !HasPaintContextSnappingApi<Ctx> ||
                  !HasPointerRevealFlag<FlagsT> ||
                  !has_pointer_reveal_api<ktui::Button>() ||
                  !has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R2 pixel snapping or R3 pointer reveal APIs not landed yet");
    } else {
        // 1. Renderer + PaintContext level: 1.5x snapped 1px border + bounded IsPointerReveal
        constexpr uint32_t W = 240;
        constexpr uint32_t H = 150;
        const ktui::Color bg_col{24.0f / 255.0f, 24.0f / 255.0f, 28.0f / 255.0f, 1.0f};
        const ktui::Color bd_col{210.0f / 255.0f, 210.0f / 255.0f, 210.0f / 255.0f, 1.0f};
        const std::array<uint8_t, 4> bg_u8{24, 24, 28, 255};
        const std::array<uint8_t, 4> bd_u8{210, 210, 210, 255};

        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        Ctx ctx(ren.get(), 1.5f);
        set_ctx_snapping_opt(ctx, true);
        ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 160.0f, 100.0f), bg_col);
        ctx.draw_rect(ktui::Rect::from_xywh(10.3f, 10.3f, 120.0f, 60.0f), bd_col, 0.0f, 1.0f);
        ren->push_primitive(make_reveal_prim<FlagsT>(
            15.0f, 15.0f, 195.0f, 105.0f, 120.0f, 60.0f, 36.0f, 1.0f, 1.0f, 1.0f, 0.45f));
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        // Border region far from reveal cursor (x=40, away from cx=120±36) must stay crisp 2px @ 1.5x
        KT_E2E_CHECK_MSG(!has_fractional_color_bleed_col(img, W, H, 40, 10, 25, bg_u8, bd_u8),
                         "1.5x snapped border must stay crisp when pointer reveal is active");
        KT_E2E_CHECK(count_matching_pixels_col(img, W, H, 40, 10, 25, bd_u8) == 2u);
        // Reveal peak around (120, 60) must brighten over bg
        KT_E2E_CHECK(img[(60u * W + 120u) * 4u] > 40u);

        // 2. Full KTUI Window at 1.5x DPI with snapped Button + SettingsCard reveal
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {400.0f, 240.0f};
        wd.custom_titlebar = false;
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());
        hw->set_dpi_scale(1.5f);
        set_window_snapping_opt(*win, true);

        ktui::Button btn{.text = "Snapped Reveal"};
        set_pointer_reveal_opt(btn, true);
        ktui::SettingsCard card{.header = "Snapped Card", .description = "1.5x DPI", .is_click_enabled = true};
        set_pointer_reveal_opt(card, true);

        win->set_content(ktui::VStack{
            .spacing = 12.0f,
            .padding = ktui::EdgeInsets(16.0f),
            .content = {std::move(btn), std::move(card)}
        });
        win->render_frame(true);

        auto move_at = [&](float lx, float ly) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {lx, ly};
            pe.dpi_scale = 1.5f;
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };

        auto fb_a = move_at(40.0f, 28.0f);
        auto fb_b = move_at(110.0f, 28.0f);
        KT_E2E_CHECK(fb_a.size() == static_cast<size_t>(600) * 360 * 4u);
        KT_E2E_CHECK_MSG(count_diff_pixels(fb_a, fb_b) > 10u,
                         "Pointer reveal must flow across button at 1.5x snapped DPI");
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_04: R3 Pointer Reveal + R4 ScrollView Scrolled Offset + DirtyTracker
// ============================================================================
template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult t3_pair_04_reveal_inside_scrollview_and_dirty_culling() {
    if constexpr (!HasPointerRevealFlag<FlagsT> ||
                  !has_pointer_reveal_api<ktui::Button>() ||
                  !has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R3 pointer reveal APIs not landed yet");
    } else {
        // 1. Renderer-level DirtyTracker 16x16 culling with scrolled reveal bounds
        constexpr uint32_t RW = 640;
        constexpr uint32_t RH = 480;
        kernetile::RendererConfig cfg{};
        cfg.width = RW; cfg.height = RH; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);

        auto step_scrolled = [&](float scroll_y, float cursor_x, float cursor_y) {
            ren->begin_frame();
            ren->draw_rect({0, 0, float(RW), float(RH)}, {0.1f, 0.1f, 0.12f, 1.0f});
            ren->push_clip({20.0f, 20.0f, 600.0f, 440.0f}, 4.0f);
            ren->push_primitive(make_reveal_prim<FlagsT>(
                40.0f, 260.0f - scroll_y, 560.0f, 360.0f - scroll_y,
                cursor_x, cursor_y, 32.0f, 1.0f, 1.0f, 1.0f, 0.45f));
            ren->pop_clip();
            ren->end_frame();
            return ren->get_last_frame_stats();
        };

        auto s0 = step_scrolled(100.0f, 160.0f, 200.0f);
        KT_E2E_CHECK(s0.total_tiles == 1200u);
        auto s1 = step_scrolled(100.0f, 172.0f, 204.0f);
        KT_E2E_CHECK_MSG(s1.dirty_tiles > 0u && s1.dirty_tiles <= 49u,
                         "Scrolled reveal pointer move must keep DirtyTracker localized");
        KT_E2E_CHECK(s1.culling_ratio > 95.0);

        // 2. Full KTUI ScrollView + reveal Button + SettingsCard
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {480.0f, 360.0f};
        wd.custom_titlebar = false;
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        std::vector<ktui::WidgetPtr> rows;
        for (int i = 0; i < 12; ++i) {
            ktui::SettingsCard card{
                .header = "Item " + std::to_string(i),
                .description = "Scrollable reveal item",
                .is_click_enabled = true
            };
            set_pointer_reveal_opt(card, true);
            rows.push_back(std::move(card));
        }

        win->set_content(ktui::ScrollView{
            .width = 480.0f,
            .height = 360.0f,
            .child = ktui::VStack{
                .spacing = 8.0f,
                .padding = ktui::EdgeInsets(12.0f),
                .content = std::move(rows)
            }
        });
        win->render_frame(true);

        auto* sv = static_cast<ktui::RenderScrollView*>(win->root_render_box());
        KT_E2E_CHECK(sv != nullptr);
        sv->scroll_to(140.0f);
        for (int i = 0; i < 60; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            app.step(0.016f);
            win->render_frame();
            if (std::abs(sv->scroll_offset() - 140.0f) < 0.05f && i >= 20) break;
        }
        KT_E2E_CHECK(sv->scroll_offset() > 100.0f);
        // 稳定一帧确保滚动条与滚动偏移完全静止后再测 PointerReveal 局部脏区
        win->render_frame(true);

        auto move_ev = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };

        auto fb1 = move_ev(120.0f, 100.0f);
        auto fb2 = move_ev(136.0f, 102.0f);
        auto diff = measure_diff_region(fb1, fb2, 480u, 360u);
        KT_E2E_CHECK_MSG(diff.count > 0u, "Scrolled reveal card must respond to PointerMove");
        KT_E2E_CHECK_MSG(diff.area() < (480u * 360u) / 4u,
                         "Scrolled reveal invalidation must stay localized");
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_05: R3 Pointer Reveal + R4 VirtualList Rubber-Band & Spring Rebound
// ============================================================================
CaseResult t3_pair_05_reveal_plus_virtuallist_rubber_band() {
    if constexpr (!has_pointer_reveal_api<ktui::Button>() ||
                  !has_rubber_band_api<ktui::VirtualList>() ||
                  !has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R3 Button reveal or R4 VirtualList rubber-band APIs not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {400.0f, 320.0f};
        wd.custom_titlebar = false;
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::VirtualList vl{
            .item_count = 40,
            .item_height = 44.0f,
            .width = 400.0f,
            .height = 320.0f,
            .item_builder = [](size_t idx) -> ktui::WidgetPtr {
                ktui::Button b{.text = "Row " + std::to_string(idx)};
                set_pointer_reveal_opt(b, true);
                return b;
            }
        };
        set_rubber_band_opt(vl, true);

        win->set_content(std::move(vl));
        win->render_frame(true);

        auto* rvl = static_cast<ktui::RenderVirtualList*>(win->root_render_box());
        KT_E2E_CHECK(rvl != nullptr);

        // Touch drag down past top boundary (0.0f)
        auto send_touch = [&](ktui::PlatformEventType type, float y) {
            ktui::PlatformEvent pe{};
            pe.type = type;
            pe.device_kind = ktui::PointerDeviceKind::Touch;
            pe.button = ktui::PointerButton::Primary;
            pe.position = {160.0f, y};
            win->handle_platform_event(pe);
            win->render_frame();
        };

        send_touch(ktui::PlatformEventType::PointerDown, 100.0f);
        send_touch(ktui::PlatformEventType::PointerMove, 140.0f);
        send_touch(ktui::PlatformEventType::PointerMove, 220.0f);
        KT_E2E_CHECK_MSG(rvl->scroll_offset() < -5.0f,
                         "VirtualList with rubber_band_overscroll=true must overscroll past top");

        send_touch(ktui::PlatformEventType::PointerUp, 220.0f);
        for (int i = 0; i < 60; ++i) {
            app.step(1.0f / 60.0f);
            win->render_frame();
            if (std::abs(rvl->scroll_offset()) < 0.5f) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        KT_E2E_CHECK_MSG(std::abs(rvl->scroll_offset()) < 2.0f,
                         "VirtualList SpringSolver rebound must settle back near 0");

        // Now sweep mouse pointer over visible reveal button
        auto move_mouse = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };
        auto fb1 = move_mouse(60.0f, 22.0f);
        auto fb2 = move_mouse(180.0f, 22.0f);
        KT_E2E_CHECK_MSG(count_diff_pixels(fb1, fb2) > 5u,
                         "Pointer reveal on VirtualList item must remain active after rubber-band rebound");
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_06: R2 Fractional DPI Snapping (1.25x / 1.5x) + R5 Text tabular_nums
// ============================================================================
template <typename Win = ktui::Window,
          typename Txt = ktui::Text,
          typename TM = ktui::TextMeasure>
CaseResult t3_pair_06_dpi_snapping_plus_tabular_nums() {
    if constexpr (!HasWindowPixelSnappingApi<Win> ||
                  !HasTabularNumsField<Txt> ||
                  !HasTextMeasureTabular<TM, ktui::FontWeight>) {
        return pending_case("R2 pixel snapping or R5 Text tabular_nums not landed yet");
    } else {
        for (float scale : {1.25f, 1.5f}) {
            ktui::Application app(ktui::create_platform_backend(true));
            ktui::WindowDesc wd{};
            wd.size = {320.0f, 160.0f};
            wd.custom_titlebar = false;
            auto win = app.create_window(wd);
            KT_E2E_CHECK(win != nullptr);
            auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());
            hw->set_dpi_scale(scale);
            set_window_snapping_opt(*win, true);

            Txt t1{.text = "11111", .font_size = 16.0f};
            set_tabular_nums_opt(t1, true);
            Txt t8{.text = "88888", .font_size = 16.0f};
            set_tabular_nums_opt(t8, true);

            ktui::WidgetPtr w1 = t1;
            ktui::WidgetPtr w8 = t8;
            auto n1 = w1->create_render_node();
            auto n8 = w8->create_render_node();
            n1->layout(ktui::BoxConstraints::loose({300.0f, 80.0f}));
            n8->layout(ktui::BoxConstraints::loose({300.0f, 80.0f}));
            KT_E2E_CHECK_MSG(std::abs(n1->size().x - n8->size().x) < 1e-3f,
                             "Tabular RenderText widths must match at fractional DPI");

            win->set_content(ktui::Box{
                .border_color = ktui::Color{0.85f, 0.85f, 0.85f, 1.0f},
                .border_width = 1.0f,
                .padding = ktui::EdgeInsets(14.0f),
                .child = ktui::VStack{
                    .spacing = 6.0f,
                    .content = {std::move(w1), std::move(w8)}
                }
            });
            win->render_frame(true);
            auto fb = read_headless_rgba8(*hw);
            const uint32_t pw = static_cast<uint32_t>(320.0f * scale);
            const uint32_t ph = static_cast<uint32_t>(160.0f * scale);
            KT_E2E_CHECK(fb.size() == static_cast<size_t>(pw) * ph * 4u);
            KT_E2E_CHECK(fnv1a_rgba8(fb) != 0);
        }
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_07: R1 IGN Dithering + R5 Text tabular_nums (draw_text_scaled)
// ============================================================================
template <typename Cfg = kernetile::RendererConfig,
          typename Ren = kernetile::KerneTileRenderer,
          typename SV = std::string_view>
CaseResult t3_pair_07_ign_dithering_plus_tabular_text() {
    if constexpr (!HasRendererDitheringConfig<Cfg> ||
                  !HasRendererDrawTextTabular<Ren> ||
                  !HasLayoutTextTabular<SV>) {
        return pending_case("R1 dithering or R5 draw_text_scaled tabular_nums not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 96;
        const auto dark_mica = make_horizontal_ramp_mica(18, 22, 26, 30);

        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg_opt(cfg, true);
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, dark_mica);
        ren->begin_frame();
        ren->draw_text_scaled("11111", 20.0f, 18.0f, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f},
                              1.0f, 400, false, "Segoe UI", true);
        ren->draw_text_scaled("88888", 20.0f, 52.0f, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f},
                              1.0f, 400, false, "Segoe UI", true);
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        // Background strip at y=6 must still show IGN dithering without banding
        auto bg_row = extract_row_channel(img, W, 6, 0, W, 0);
        auto st = analyze_gradient_banding(bg_row, 18.0, 30.0);
        KT_E2E_CHECK(st.monotonic_blocks);
        KT_E2E_CHECK(st.spatial_dither_transitions >= 12u);

        // Text ink extent on both rows must match closely (tabular digits)
        auto max_lit_x = [&](uint32_t y0, uint32_t y1) {
            uint32_t mx = 0;
            for (uint32_t y = y0; y < y1; ++y) {
                for (uint32_t x = 20; x < W; ++x) {
                    if (img[(y * W + x) * 4u] > 100u) mx = std::max(mx, x);
                }
            }
            return mx;
        };
        const uint32_t x1 = max_lit_x(16, 40);
        const uint32_t x8 = max_lit_x(48, 76);
        KT_E2E_CHECK(x1 > 30u && x8 > 30u);
        KT_E2E_CHECK_MSG(std::abs(static_cast<int>(x1) - static_cast<int>(x8)) <= 2,
                         "Tabular digits drawn over dithered surface must align horizontally");

        auto l1 = kernetile::text::layout_text(SV("11111"), 16.0f, 400, false, SV("Segoe UI"), true);
        auto l8 = kernetile::text::layout_text(SV("88888"), 16.0f, 400, false, SV("Segoe UI"), true);
        KT_E2E_CHECK(std::abs(l1->total_width - l8->total_width) < 1e-3f);
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_08: R4 Rubber-Band Overscroll (ScrollView) + R5 Text tabular_nums
// ============================================================================
template <typename Txt = ktui::Text>
CaseResult t3_pair_08_rubber_band_scroll_plus_tabular_counter() {
    if constexpr (!has_rubber_band_api<ktui::ScrollView>() ||
                  !has_rubber_band_api<ktui::SmoothScrollPhysics>() ||
                  !HasTabularNumsField<Txt>) {
        return pending_case("R4 ScrollView rubber_band or R5 Text::tabular_nums not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {360.0f, 280.0f};
        wd.custom_titlebar = false;
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);

        ktui::Property<std::string> readout{"000.0"};
        Txt counter_txt{.font_size = 15.0f};
        set_tabular_nums_opt(counter_txt, true);
        ktui::WidgetPtr counter_w = counter_txt.bind(&ktui::RenderText::set_text, readout);
        auto counter_node = std::static_pointer_cast<ktui::RenderText>(counter_w->create_render_node());

        ktui::ScrollView sv{
            .width = 360.0f,
            .height = 280.0f,
            .child = ktui::VStack{
                .spacing = 8.0f,
                .padding = ktui::EdgeInsets(12.0f),
                .content = {
                    std::move(counter_w),
                    ktui::Box{.width = 320.0f, .height = 600.0f}
                }
            }
        };
        set_rubber_band_opt(sv, true);
        win->set_content(std::move(sv));
        win->render_frame(true);

        counter_node->layout(ktui::BoxConstraints::loose({300.0f, 60.0f}));
        const float w_000 = counter_node->size().x;

        readout.set("111.1");
        counter_node->layout(ktui::BoxConstraints::loose({300.0f, 60.0f}));
        const float w_111 = counter_node->size().x;

        readout.set("888.8");
        counter_node->layout(ktui::BoxConstraints::loose({300.0f, 60.0f}));
        const float w_888 = counter_node->size().x;

        KT_E2E_CHECK_MSG(std::abs(w_000 - w_111) < 1e-3f && std::abs(w_111 - w_888) < 1e-3f,
                         "Tabular counter inside ScrollView must not jitter width across digits");

        auto* rsv = static_cast<ktui::RenderScrollView*>(win->root_render_box());
        KT_E2E_CHECK(rsv != nullptr);
        ktui::PlatformEvent pe{};
        pe.device_kind = ktui::PointerDeviceKind::Touch;
        pe.button = ktui::PointerButton::Primary;
        pe.type = ktui::PlatformEventType::PointerDown;
        pe.position = {120.0f, 100.0f};
        win->handle_platform_event(pe);
        pe.type = ktui::PlatformEventType::PointerMove;
        pe.position = {120.0f, 190.0f};
        win->handle_platform_event(pe);
        win->render_frame();
        KT_E2E_CHECK(rsv->scroll_offset() < -5.0f);

        pe.type = ktui::PlatformEventType::PointerUp;
        win->handle_platform_event(pe);
        for (int i = 0; i < 60; ++i) {
            app.step(1.0f / 60.0f);
            win->render_frame();
            if (std::abs(rsv->scroll_offset()) < 0.5f) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        KT_E2E_CHECK(std::abs(rsv->scroll_offset()) < 2.0f);
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_09: R1 IGN Dithering + R3 IsPointerReveal Radial Sheen (CPU vs WARP)
// ============================================================================
template <typename Cfg = kernetile::RendererConfig,
          typename Ren = kernetile::KerneTileRenderer,
          typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult t3_pair_09_ign_plus_pointer_reveal_warp_parity() {
    if constexpr (!HasRendererDitheringConfig<Cfg> || !HasPointerRevealFlag<FlagsT>) {
        return pending_case("R1 dithering or R3 IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 192;
        constexpr uint32_t H = 192;
        const auto dark_mica = make_horizontal_ramp_mica(18, 22, 26, 30);
        auto rev = make_reveal_prim<FlagsT>(
            24.0f, 24.0f, 168.0f, 168.0f, 96.0f, 96.0f, 52.0f, 1.0f, 1.0f, 1.0f, 0.45f);

        auto run_cpu = [&](bool dither) {
            Cfg cfg{};
            cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
            set_dithering_cfg_opt(cfg, dither);
            auto ren = Ren::create(cfg);
            ren->set_mica_backdrop(true, dark_mica);
            ren->begin_frame();
            ren->push_primitive(rev);
            ren->end_frame();
            return render_cpu_frame(*ren, W, H);
        };

        auto cpu_off = run_cpu(false);
        auto cpu_on = run_cpu(true);
        KT_E2E_CHECK(count_diff_pixels(cpu_off, cpu_on) > 100u);

        // Radial peak > mid > outside still holds with dithering enabled
        auto px_r = [&](uint32_t x, uint32_t y) { return cpu_on[(y * W + x) * 4u]; };
        KT_E2E_CHECK(px_r(96, 96) > px_r(124, 96));
        KT_E2E_CHECK(px_r(124, 96) > px_r(160, 96));

#if defined(_WIN32) && !defined(KERNETILE_WINDOWS_PURE_GDI)
        D3D12OffscreenRunner warp;
        KT_E2E_CHECK(warp.is_ready());
        D3D12SceneInput scene{};
        scene.width = W; scene.height = H;
        scene.primitives = {rev};
        scene.mica_enabled = true;
        scene.mica_grid = dark_mica;
        scene.render_flags = 1u;
        auto gpu_on = warp.render(scene);

        auto cmp = compare_rgba8(cpu_on, gpu_on, W, H);
        KT_E2E_CHECK_MSG(cmp.max_lsb_diff <= 2 && cmp.ssim >= 0.999,
                         "T3_PAIR_09 dithered Mica + IsPointerReveal CPU vs D3D12 WARP parity failed");
#endif
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_10: R2 DPI Snapping (1.25x) + R4 ScrollView Extent Scaling
// ============================================================================
template <typename Win = ktui::Window>
CaseResult t3_pair_10_dpi_125_plus_scroll_viewport_extent() {
    if constexpr (!HasWindowPixelSnappingApi<Win> ||
                  !has_rubber_band_api<ktui::SmoothScrollPhysics>() ||
                  !has_rubber_band_api<ktui::ScrollView>()) {
        return pending_case("R2 Window pixel snapping or R4 rubber-band APIs not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {400.0f, 320.0f};
        wd.custom_titlebar = false;
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());
        hw->set_dpi_scale(1.25f);
        set_window_snapping_opt(*win, true);

        ktui::ScrollView sv{
            .width = 400.0f,
            .height = 320.0f,
            .child = ktui::Box{
                .width = 380.0f,
                .height = 900.0f,
                .border_color = ktui::Color{0.8f, 0.8f, 0.8f, 1.0f},
                .border_width = 1.0f
            }
        };
        set_rubber_band_opt(sv, true);
        win->set_content(std::move(sv));
        win->render_frame(true);

        // Verify physical framebuffer size is scaled by 1.25x while logical viewport extent d = 320.0f
        KT_E2E_CHECK(hw->virtual_framebuffer().size() == static_cast<size_t>(500) * 400);

        constexpr float d_logical = 320.0f;
        constexpr float c = 0.55f;
        constexpr float x_drag = 80.0f;

        ktui::PlatformEvent pe{};
        pe.device_kind = ktui::PointerDeviceKind::Touch;
        pe.button = ktui::PointerButton::Primary;
        pe.dpi_scale = 1.25f;
        pe.type = ktui::PlatformEventType::PointerDown;
        pe.position = {160.0f, 100.0f};
        win->handle_platform_event(pe);

        pe.type = ktui::PlatformEventType::PointerMove;
        pe.position = {160.0f, 112.0f}; // break 8px touch slop
        win->handle_platform_event(pe);

        pe.position = {160.0f, 112.0f + x_drag};
        win->handle_platform_event(pe);
        win->render_frame();

        auto* rsv = static_cast<ktui::RenderScrollView*>(win->root_render_box());
        KT_E2E_CHECK(rsv != nullptr);
        const float actual_overscroll = -rsv->scroll_offset();
        const float expected_logical = (x_drag * d_logical * c) / (d_logical + c * x_drag);
        const float expected_with_slop = ((x_drag + 12.0f) * d_logical * c) / (d_logical + c * (x_drag + 12.0f));
        KT_E2E_CHECK_MSG(actual_overscroll >= expected_logical - 1.0f &&
                         actual_overscroll <= expected_with_slop + 1.0f,
                         "Rubber-band overscroll must use logical viewport extent d=320 under 1.25x DPI");
        return passed_case();
    }
}

// ============================================================================
// T3_PAIR_11: All 5 Features Default-Off vs Explicit-False (Bit-Exact Match)
// ============================================================================
template <typename Win = ktui::Window, typename Txt = ktui::Text>
CaseResult t3_pair_11_all_disabled_default_vs_explicit_false() {
    auto run_pipeline = [](bool explicit_false) -> std::vector<uint8_t> {
        ktui::Application app(ktui::create_platform_backend(true));
        app.set_theme(ktui::Theme::dark());
        ktui::WindowDesc wd{};
        wd.size = {480.0f, 320.0f};
        wd.custom_titlebar = false;
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());
        hw->set_dpi_scale(1.25f);

        if (explicit_false) {
            set_window_dithering_opt(*win, false);
            set_window_snapping_opt(*win, false);
        }

        ktui::Button btn{.text = "Default Btn"};
        ktui::SettingsCard card{.header = "Default Card", .description = "Baseline", .is_click_enabled = true};
        Txt t1{.text = "11111", .font_size = 14.0f};
        Txt t8{.text = "88888", .font_size = 14.0f};
        ktui::ScrollView sv{
            .width = 450.0f,
            .height = 120.0f,
            .child = ktui::Box{
                .height = 400.0f,
                .border_color = ktui::Color{0.8f, 0.8f, 0.8f, 1.0f},
                .border_width = 1.0f
            }
        };

        if (explicit_false) {
            set_pointer_reveal_opt(btn, false);
            set_pointer_reveal_opt(card, false);
            set_tabular_nums_opt(t1, false);
            set_tabular_nums_opt(t8, false);
            set_rubber_band_opt(sv, false);
        }

        win->set_content(ktui::VStack{
            .spacing = 8.0f,
            .padding = ktui::EdgeInsets(12.0f),
            .content = {
                std::move(btn),
                std::move(card),
                std::move(t1),
                std::move(t8),
                std::move(sv)
            }
        });
        win->render_frame(true);

        ktui::PlatformEvent pe{};
        pe.type = ktui::PlatformEventType::PointerMove;
        pe.device_kind = ktui::PointerDeviceKind::Mouse;
        pe.position = {80.0f, 28.0f};
        pe.dpi_scale = 1.25f;
        win->handle_platform_event(pe);
        win->render_frame(true);
        return read_headless_rgba8(*hw);
    };

    auto def_fb = run_pipeline(false);
    auto exp_fb = run_pipeline(true);
    KT_E2E_CHECK_MSG(fnv1a_rgba8(def_fb) == fnv1a_rgba8(exp_fb),
                     "Default-off Window pipeline must match explicit-false bit-exactly");
    KT_E2E_CHECK(count_diff_pixels(def_fb, exp_fb) == 0u);
    return passed_case();
}

// ============================================================================
// T3_PAIR_12: All 5 Features Enabled Simultaneously in One Window Frame
// ============================================================================
template <typename Win = ktui::Window,
          typename Txt = ktui::Text,
          typename TM = ktui::TextMeasure>
CaseResult t3_pair_12_all_five_features_enabled_simultaneously() {
    if constexpr (!HasWindowDitheringApi<Win> ||
                  !HasWindowPixelSnappingApi<Win> ||
                  !has_pointer_reveal_api<ktui::Button>() ||
                  !has_pointer_reveal_api<ktui::SettingsCard>() ||
                  !has_rubber_band_api<ktui::ScrollView>() ||
                  !HasTabularNumsField<Txt> ||
                  !HasTextMeasureTabular<TM, ktui::FontWeight>) {
        return pending_case("Not all 5 feature APIs landed yet for T3_PAIR_12");
    } else {
        auto run_scene = [&](bool enable_all) {
            ktui::Application app(ktui::create_platform_backend(true));
            app.set_theme(ktui::Theme::dark());
            ktui::WindowDesc wd{};
            wd.size = {520.0f, 360.0f};
            wd.custom_titlebar = false;
            auto win = app.create_window(wd);
            KT_E2E_CHECK(win != nullptr);
            auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());
            hw->set_dpi_scale(1.25f);
            set_window_dithering_opt(*win, enable_all);
            set_window_snapping_opt(*win, enable_all);

            ktui::Button btn{.text = "All-5 Action"};
            set_pointer_reveal_opt(btn, enable_all);
            ktui::SettingsCard card{.header = "All-5 Card", .description = "Combined test", .is_click_enabled = true};
            set_pointer_reveal_opt(card, enable_all);

            Txt t1{.text = "11111", .font_size = 15.0f};
            set_tabular_nums_opt(t1, enable_all);
            Txt t8{.text = "88888", .font_size = 15.0f};
            set_tabular_nums_opt(t8, enable_all);

            ktui::ScrollView sv{
                .width = 490.0f,
                .height = 140.0f,
                .child = ktui::Box{
                    .height = 500.0f,
                    .border_color = ktui::Color{0.85f, 0.85f, 0.85f, 1.0f},
                    .border_width = 1.0f,
                    .padding = ktui::EdgeInsets(10.0f),
                    .child = ktui::VStack{
                        .spacing = 4.0f,
                        .content = {std::move(t1), std::move(t8)}
                    }
                }
            };
            set_rubber_band_opt(sv, enable_all);

            win->set_content(ktui::VStack{
                .spacing = 8.0f,
                .padding = ktui::EdgeInsets(12.0f),
                .content = {std::move(btn), std::move(card), std::move(sv)}
            });
            win->render_frame(true);

            // Sweep pointer on reveal button
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {60.0f, 26.0f};
            pe.dpi_scale = 1.25f;
            win->handle_platform_event(pe);
            win->render_frame();
            auto fb_p1 = read_headless_rgba8(*hw);

            pe.position = {115.0f, 26.0f};
            win->handle_platform_event(pe);
            win->render_frame();
            auto fb_p2 = read_headless_rgba8(*hw);

            if (enable_all) {
                auto diff = measure_diff_region(fb_p1, fb_p2, 650u, 450u);
                KT_E2E_CHECK_MSG(diff.count > 0u, "Combined Window must update localized pointer reveal");
                KT_E2E_CHECK_MSG(diff.area() < (650u * 450u) / 4u,
                                 "Combined Window must not thrash full framebuffer on pointer move");
            }
            return fb_p2;
        };

        auto base_fb = run_scene(false);
        auto all_fb = run_scene(true);
        KT_E2E_CHECK_MSG(fnv1a_rgba8(base_fb) != fnv1a_rgba8(all_fb),
                         "All-5 enabled frame must differ cleanly from all-disabled baseline");

        float tw1 = TM::measure_width("11111", 15.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        float tw8 = TM::measure_width("88888", 15.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        KT_E2E_CHECK(std::abs(tw1 - tw8) < 1e-3f);
        return passed_case();
    }
}

} // namespace tier3_detail

using namespace tier3_detail;

int main() {
    TestRunner runner("KerneTile & KTUI E2E Tier 3 Pairwise Combinations (T3_PAIR_01..12)");

    runner.run_case("T3_PAIR_01_ign_mica_gaussian_shadow_parity", t3_pair_01_ign_mica_gaussian_shadow_parity<>);
    runner.run_case("T3_PAIR_02_ign_plus_fractional_dpi_snapping", t3_pair_02_ign_plus_fractional_dpi_snapping<>);
    runner.run_case("T3_PAIR_03_dpi_snapping_plus_pointer_reveal", t3_pair_03_dpi_snapping_plus_pointer_reveal<>);
    runner.run_case("T3_PAIR_04_reveal_inside_scrollview_and_dirty_culling", t3_pair_04_reveal_inside_scrollview_and_dirty_culling<>);
    runner.run_case("T3_PAIR_05_reveal_plus_virtuallist_rubber_band", t3_pair_05_reveal_plus_virtuallist_rubber_band);
    runner.run_case("T3_PAIR_06_dpi_snapping_plus_tabular_nums", t3_pair_06_dpi_snapping_plus_tabular_nums<>);
    runner.run_case("T3_PAIR_07_ign_dithering_plus_tabular_text", t3_pair_07_ign_dithering_plus_tabular_text<>);
    runner.run_case("T3_PAIR_08_rubber_band_scroll_plus_tabular_counter", t3_pair_08_rubber_band_scroll_plus_tabular_counter<>);
    runner.run_case("T3_PAIR_09_ign_plus_pointer_reveal_warp_parity", t3_pair_09_ign_plus_pointer_reveal_warp_parity<>);
    runner.run_case("T3_PAIR_10_dpi_125_plus_scroll_viewport_extent", t3_pair_10_dpi_125_plus_scroll_viewport_extent<>);
    runner.run_case("T3_PAIR_11_all_disabled_default_vs_explicit_false", t3_pair_11_all_disabled_default_vs_explicit_false<>);
    runner.run_case("T3_PAIR_12_all_five_features_enabled_simultaneously", t3_pair_12_all_five_features_enabled_simultaneously<>);

    KT_E2E_CHECK(runner.total() == 12u);
    return runner.finalize();
}
