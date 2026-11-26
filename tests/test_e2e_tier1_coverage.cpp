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
#include <kernetile/atlas/dynamic_atlas.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace kernetile_e2e;

namespace {

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
void set_dithering_cfg(Cfg& cfg, bool enabled) {
    if constexpr (HasRendererDitheringConfig<Cfg>) {
        cfg.enable_dithering = enabled;
    }
}

template <typename Cfg>
void set_snapping_cfg(Cfg& cfg, bool enabled) {
    if constexpr (HasRendererPixelSnappingConfig<Cfg>) {
        cfg.enable_pixel_snapping = enabled;
    }
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

// ============================================================================
// F1: Opt-in 8-bit IGN Dithering (Engine & Backends) — 5 Tests
// ============================================================================

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_t1_default_disabled() {
    if constexpr (!HasRendererDitheringConfig<Cfg> || !HasRendererDitheringApi<Ren>) {
        return pending_case("R1 RendererConfig::enable_dithering / KerneTileRenderer dithering API not landed yet");
    } else {
        Cfg cfg{};
        KT_E2E_CHECK(cfg.enable_dithering == false);
        cfg.force_cpu_fallback = true;
        auto ren = Ren::create(cfg);
        KT_E2E_CHECK(ren != nullptr);
        KT_E2E_CHECK(ren->is_dithering_enabled() == false);
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_t2_toggle_api_and_pixel_effect() {
    if constexpr (!HasRendererDitheringConfig<Cfg> || !HasRendererDitheringApi<Ren>) {
        return pending_case("R1 dithering API not landed yet");
    } else {
        Cfg cfg{};
        cfg.width = 128;
        cfg.height = 64;
        cfg.force_cpu_fallback = true;
        auto ren = Ren::create(cfg);
        KT_E2E_CHECK(ren != nullptr);

        auto grid = make_horizontal_ramp_mica(16, 24, 32, 40);
        ren->set_mica_backdrop(true, grid);

        KT_E2E_CHECK(ren->is_dithering_enabled() == false);
        ren->begin_frame();
        ren->end_frame();
        auto off_pixels = render_cpu_frame(*ren, 128, 64);

        ren->set_dithering_enabled(true);
        KT_E2E_CHECK(ren->is_dithering_enabled() == true);
        ren->resize(128, 64);
        ren->begin_frame();
        ren->end_frame();
        auto on_pixels = render_cpu_frame(*ren, 128, 64);

        KT_E2E_CHECK(fnv1a_rgba8(off_pixels) != fnv1a_rgba8(on_pixels));
        KT_E2E_CHECK(count_diff_pixels(off_pixels, on_pixels) > 0);

        ren->set_dithering_enabled(false);
        KT_E2E_CHECK(ren->is_dithering_enabled() == false);
        ren->resize(128, 64);
        ren->begin_frame();
        ren->end_frame();
        auto off_again = render_cpu_frame(*ren, 128, 64);
        KT_E2E_CHECK(count_diff_pixels(off_pixels, off_again) == 0);
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_t3_smooth_ramp_dither_quality() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 RendererConfig::enable_dithering not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 32;
        const auto grid = make_horizontal_ramp_mica(16, 20, 24, 28);

        Cfg cfg_off{};
        cfg_off.width = W;
        cfg_off.height = H;
        cfg_off.force_cpu_fallback = true;
        set_dithering_cfg(cfg_off, false);
        auto ren_off = Ren::create(cfg_off);
        ren_off->set_mica_backdrop(true, grid);
        ren_off->begin_frame();
        ren_off->end_frame();
        auto img_off = render_cpu_frame(*ren_off, W, H);

        Cfg cfg_on = cfg_off;
        set_dithering_cfg(cfg_on, true);
        auto ren_on = Ren::create(cfg_on);
        ren_on->set_mica_backdrop(true, grid);
        ren_on->begin_frame();
        ren_on->end_frame();
        auto img_on = render_cpu_frame(*ren_on, W, H);

        auto row_off = extract_row_channel(img_off, W, 16, 0, W, 0);
        auto row_on = extract_row_channel(img_on, W, 16, 0, W, 0);

        const double ideal_start = 16.0 + 12.0 * (0.5 / W);
        const double ideal_end = 16.0 + 12.0 * ((W - 0.5) / W);

        auto stats_off = analyze_gradient_banding(row_off, ideal_start, ideal_end);
        auto stats_on = analyze_gradient_banding(row_on, ideal_start, ideal_end);

        KT_E2E_CHECK(stats_off.max_flat_run >= 15);
        KT_E2E_CHECK(stats_on.monotonic_blocks);
        KT_E2E_CHECK_MSG(stats_on.max_lsb_error <= 1.001, "Dithered ramp must stay within 1 LSB of ideal");
        KT_E2E_CHECK_MSG(stats_on.max_flat_run < stats_off.max_flat_run, "IGN dithering must break wide flat bands");
        KT_E2E_CHECK_MSG(stats_on.spatial_dither_transitions >= 20, "IGN dithering must introduce spatial transitions");
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_t4_cpu_vs_d3d12_warp_ign_parity() {
#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 64;
        const auto grid = make_horizontal_ramp_mica(16, 24, 32, 40, 2, 4);

        Cfg cfg{};
        cfg.width = W;
        cfg.height = H;
        cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, grid);
        ren->begin_frame();
        ren->end_frame();
        auto cpu_rgba = render_cpu_frame(*ren, W, H);

        D3D12OffscreenRunner warp;
        KT_E2E_CHECK_MSG(warp.is_ready(), "D3D12 WARP device/pipeline failed to initialize");
        D3D12SceneInput scene{};
        scene.width = W;
        scene.height = H;
        scene.mica_enabled = true;
        scene.mica_grid = grid;
        scene.render_flags = 1u;
        auto gpu_rgba = warp.render(scene);

        auto cmp = compare_rgba8(cpu_rgba, gpu_rgba, W, H);
        KT_E2E_CHECK_MSG(cmp.max_lsb_diff <= 1, "CPU vs D3D12 WARP IGN max LSB diff > 1");
        KT_E2E_CHECK_MSG(cmp.ssim >= 0.999, "CPU vs D3D12 WARP IGN SSIM < 0.999");
        return passed_case();
    }
#else
    return passed_case();
#endif
}

template <typename Cfg = kernetile::RendererConfig>
CaseResult f1_t5_vulkan_and_webgpu_shader_contract() {
    auto audit = audit_shader_files();
    KT_E2E_CHECK_MSG(audit.hlsl_found && audit.wgsl_found, "Shader source files missing on disk");
    KT_E2E_CHECK_MSG(audit.hlsl_has_vk_path, "HLSL missing KT_VULKAN vk::binding path");

    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        if (!audit.hlsl_has_ign_constants || !audit.wgsl_has_ign_constants) {
            return pending_case("R1 HLSL/WGSL IGN shader updates not landed yet");
        }
    }
    KT_E2E_CHECK_MSG(audit.hlsl_has_ign_constants, "HLSL missing IGN constants (52.9829189 / 0.06711056 / 0.00583715)");
    KT_E2E_CHECK_MSG(audit.hlsl_has_render_flags, "HLSL missing render_flags in Frame cbuffer");
    KT_E2E_CHECK_MSG(audit.wgsl_has_ign_constants, "WGSL missing IGN constants (52.9829189 / 0.06711056 / 0.00583715)");
    KT_E2E_CHECK_MSG(audit.wgsl_has_render_flags, "WGSL missing render_flags in Frame uniform");
    return passed_case();
}

// ============================================================================
// F2: Banding Elimination Targets (Mica, Gradients, Shadows) — 5 Tests
// ============================================================================

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_t1_dark_mode_mica_4x4_ramp() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 64;
        const auto dark_mica = make_horizontal_ramp_mica(18, 22, 26, 30, 0, 2);

        Cfg cfg{};
        cfg.width = W;
        cfg.height = H;
        cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, dark_mica);
        ren->begin_frame();
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        auto row_r = extract_row_channel(img, W, 32, 0, W, 0);
        auto st = analyze_gradient_banding(row_r, 18.0 + 12.0 * (0.5 / W), 18.0 + 12.0 * ((W - 0.5) / W));
        KT_E2E_CHECK(st.monotonic_blocks);
        KT_E2E_CHECK(st.max_lsb_error <= 1.001);
        KT_E2E_CHECK(st.max_flat_run <= 12);
        KT_E2E_CHECK(st.spatial_dither_transitions >= 16);
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_t2_slow_linear_gradient() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 32;
        const auto slow_grid = make_horizontal_ramp_mica(20, 22, 24, 26);

        Cfg cfg_off{};
        cfg_off.width = W;
        cfg_off.height = H;
        cfg_off.force_cpu_fallback = true;
        set_dithering_cfg(cfg_off, false);
        auto r_off = Ren::create(cfg_off);
        r_off->set_mica_backdrop(true, slow_grid);
        r_off->begin_frame();
        r_off->end_frame();
        auto img_off = render_cpu_frame(*r_off, W, H);

        Cfg cfg_on = cfg_off;
        set_dithering_cfg(cfg_on, true);
        auto r_on = Ren::create(cfg_on);
        r_on->set_mica_backdrop(true, slow_grid);
        r_on->begin_frame();
        r_on->end_frame();
        auto img_on = render_cpu_frame(*r_on, W, H);

        auto st_off = analyze_gradient_banding(extract_row_channel(img_off, W, 16, 0, W, 0), 20.0, 26.0);
        auto st_on = analyze_gradient_banding(extract_row_channel(img_on, W, 16, 0, W, 0), 20.0, 26.0);

        KT_E2E_CHECK(st_off.max_flat_run >= 32);
        KT_E2E_CHECK(st_on.max_flat_run < st_off.max_flat_run / 2);
        KT_E2E_CHECK(st_on.max_lsb_error <= 1.001);
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_t3_large_radius_gaussian_shadow() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 192;
        constexpr uint32_t H = 192;
        const kernetile::math::Color bg{40.0f / 255.0f, 40.0f / 255.0f, 44.0f / 255.0f, 1.0f};
        const kernetile::math::Color card{48.0f / 255.0f, 48.0f / 255.0f, 52.0f / 255.0f, 1.0f};

        auto render_shadow = [&](bool dither) {
            Cfg cfg{};
            cfg.width = W;
            cfg.height = H;
            cfg.force_cpu_fallback = true;
            set_dithering_cfg(cfg, dither);
            auto ren = Ren::create(cfg);
            ren->begin_frame();
            ren->draw_rect({0.0f, 0.0f, static_cast<float>(W), static_cast<float>(H)}, bg);
            ren->draw_shadowed_box({56.0f, 56.0f, 136.0f, 136.0f}, card, 8.0f, 24.0f, 0.0f, {0.0f, 4.0f});
            ren->end_frame();
            return render_cpu_frame(*ren, W, H);
        };

        auto img_off = render_shadow(false);
        auto img_on = render_shadow(true);
        KT_E2E_CHECK(count_diff_pixels(img_off, img_on) > 100);

        auto tail = extract_row_channel(img_on, W, 96, 12, 52, 0);
        auto st = analyze_gradient_banding(tail, static_cast<double>(tail.front()), static_cast<double>(tail.back()));
        KT_E2E_CHECK(st.monotonic_blocks);
        KT_E2E_CHECK(st.spatial_dither_transitions > 0);
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_t4_cpu_vs_d3d12_warp_dark_ramp_and_shadow() {
#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        const auto dark_mica = make_horizontal_ramp_mica(18, 22, 26, 30);

        Cfg cfg{};
        cfg.width = W;
        cfg.height = H;
        cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, dark_mica);
        ren->begin_frame();
        ren->draw_shadowed_box({32.0f, 32.0f, 96.0f, 96.0f},
                               {0.18f, 0.19f, 0.22f, 1.0f}, 6.0f, 24.0f, 0.0f, {0.0f, 4.0f});
        ren->end_frame();
        auto cpu_rgba = render_cpu_frame(*ren, W, H);

        kernetile::core::UiPrimitive box{};
        box.min_x = 32.0f; box.min_y = 32.0f; box.max_x = 96.0f; box.max_y = 96.0f;
        box.radius_top_left = box.radius_top_right = box.radius_bottom_right = box.radius_bottom_left = 6.0f;
        box.color_r = 0.18f; box.color_g = 0.19f; box.color_b = 0.22f; box.color_a = 1.0f;
        box.shadow_blur = 24.0f;
        box.shadow_offset_y = 4.0f;

        D3D12OffscreenRunner warp;
        KT_E2E_CHECK(warp.is_ready());
        D3D12SceneInput scene{};
        scene.width = W;
        scene.height = H;
        scene.primitives = {box};
        scene.mica_enabled = true;
        scene.mica_grid = dark_mica;
        scene.render_flags = 1u;
        auto gpu_rgba = warp.render(scene);

        auto cmp = compare_rgba8(cpu_rgba, gpu_rgba, W, H);
        KT_E2E_CHECK_MSG(cmp.max_lsb_diff <= 1, "CPU vs D3D12 WARP dark ramp+shadow LSB diff > 1");
        KT_E2E_CHECK_MSG(cmp.ssim >= 0.999, "CPU vs D3D12 WARP dark ramp+shadow SSIM < 0.999");
        return passed_case();
    }
#else
    return passed_case();
#endif
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_t5_multi_channel_rgb_smooth_gradient() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 32;
        const auto grid = make_horizontal_ramp_mica(16, 22, 28, 34, 4, 10);

        Cfg cfg{};
        cfg.width = W;
        cfg.height = H;
        cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, grid);
        ren->begin_frame();
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        for (uint32_t ch = 0; ch < 3; ++ch) {
            const double off = (ch == 0) ? 0.0 : (ch == 1 ? 4.0 : 10.0);
            auto samples = extract_row_channel(img, W, 16, 0, W, ch);
            auto st = analyze_gradient_banding(samples, 16.0 + off, 34.0 + off);
            KT_E2E_CHECK(st.monotonic_blocks);
            KT_E2E_CHECK(st.max_lsb_error <= 1.001);
            KT_E2E_CHECK(st.spatial_dither_transitions >= 12);
        }
        return passed_case();
    }
}

// ============================================================================
// F3: Opt-in Fractional DPI Physical Pixel Snapping — 5 Tests
// ============================================================================

template <typename Cfg = kernetile::RendererConfig,
          typename Ren = kernetile::KerneTileRenderer,
          typename Win = ktui::Window,
          typename Ctx = ktui::PaintContext>
CaseResult f3_t1_default_disabled() {
    if constexpr (!HasRendererPixelSnappingConfig<Cfg> ||
                  !HasRendererPixelSnappingApi<Ren> ||
                  !HasWindowPixelSnappingApi<Win> ||
                  !HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 pixel snapping APIs not landed yet");
    } else {
        Cfg cfg{};
        KT_E2E_CHECK(cfg.enable_pixel_snapping == false);

        cfg.force_cpu_fallback = true;
        auto ren = Ren::create(cfg);
        KT_E2E_CHECK(ren != nullptr);
        KT_E2E_CHECK(ren->is_pixel_snapping_enabled() == false);

        Ctx ctx(ren.get(), 1.25f);
        KT_E2E_CHECK(ctx.pixel_snapping() == false);

        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {200.0f, 150.0f};
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        Win* wptr = win.get();
        KT_E2E_CHECK(wptr->pixel_snapping_enabled() == false);
        return passed_case();
    }
}

template <typename Ren = kernetile::KerneTileRenderer,
          typename Win = ktui::Window,
          typename Ctx = ktui::PaintContext>
CaseResult f3_t2_renderer_and_window_toggle_api() {
    if constexpr (!HasRendererPixelSnappingApi<Ren> ||
                  !HasWindowPixelSnappingApi<Win> ||
                  !HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 pixel snapping toggle APIs not landed yet");
    } else {
        kernetile::RendererConfig cfg{};
        cfg.force_cpu_fallback = true;
        auto ren = Ren::create(cfg);
        ren->set_pixel_snapping_enabled(true);
        KT_E2E_CHECK(ren->is_pixel_snapping_enabled() == true);
        ren->set_pixel_snapping_enabled(false);
        KT_E2E_CHECK(ren->is_pixel_snapping_enabled() == false);

        Ctx ctx(ren.get(), 1.5f);
        ctx.set_pixel_snapping(true);
        KT_E2E_CHECK(ctx.pixel_snapping() == true);
        ctx.set_pixel_snapping(false);
        KT_E2E_CHECK(ctx.pixel_snapping() == false);

        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {200.0f, 150.0f};
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        Win* wptr = win.get();
        wptr->set_pixel_snapping_enabled(true);
        KT_E2E_CHECK(wptr->pixel_snapping_enabled() == true);
        wptr->set_pixel_snapping_enabled(false);
        KT_E2E_CHECK(wptr->pixel_snapping_enabled() == false);
        return passed_case();
    }
}

template <typename Ctx = ktui::PaintContext>
CaseResult f3_t3_dpi_125_1px_border_crisp() {
    if constexpr (!HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 PaintContext::set_pixel_snapping not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        const ktui::Color bg_col{20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f};
        const ktui::Color bd_col{220.0f / 255.0f, 220.0f / 255.0f, 220.0f / 255.0f, 1.0f};
        const std::array<uint8_t, 4> bg_u8{20, 20, 20, 255};
        const std::array<uint8_t, 4> bd_u8{220, 220, 220, 255};

        kernetile::RendererConfig cfg{};
        cfg.width = W;
        cfg.height = H;
        cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);

        ren->begin_frame();
        Ctx ctx(ren.get(), 1.25f);
        ctx.set_pixel_snapping(true);
        ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 102.4f, 102.4f), bg_col);
        ctx.draw_rect(ktui::Rect::from_xywh(10.3f, 10.3f, 40.0f, 30.0f), bd_col, 0.0f, 1.0f);
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        KT_E2E_CHECK_MSG(!has_fractional_color_bleed_col(img, W, H, 35, 8, 22, bg_u8, bd_u8),
                         "1.25x snapped 1px horizontal border must have zero fractional alpha bleed");
        const uint32_t h_px = count_matching_pixels_col(img, W, H, 35, 8, 22, bd_u8);
        KT_E2E_CHECK_MSG(h_px == 1u, "1.25x snapped 1.0px border must round to 1 physical pixel");

        KT_E2E_CHECK_MSG(!has_fractional_color_bleed_row(img, W, H, 30, 8, 22, bg_u8, bd_u8),
                         "1.25x snapped 1px vertical border must have zero fractional alpha bleed");
        const uint32_t v_px = count_matching_pixels_row(img, W, H, 30, 8, 22, bd_u8);
        KT_E2E_CHECK_MSG(v_px == 1u, "1.25x snapped 1.0px vertical border must round to 1 physical pixel");
        return passed_case();
    }
}

template <typename Ctx = ktui::PaintContext>
CaseResult f3_t4_dpi_150_1px_border_2px_crisp() {
    if constexpr (!HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 PaintContext::set_pixel_snapping not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        const ktui::Color bg_col{20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f};
        const ktui::Color bd_col{220.0f / 255.0f, 220.0f / 255.0f, 220.0f / 255.0f, 1.0f};
        const std::array<uint8_t, 4> bg_u8{20, 20, 20, 255};
        const std::array<uint8_t, 4> bd_u8{220, 220, 220, 255};

        kernetile::RendererConfig cfg{};
        cfg.width = W;
        cfg.height = H;
        cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);

        ren->begin_frame();
        Ctx ctx(ren.get(), 1.5f);
        ctx.set_pixel_snapping(true);
        ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 90.0f, 90.0f), bg_col);
        ctx.draw_rect(ktui::Rect::from_xywh(10.3f, 10.3f, 40.0f, 30.0f), bd_col, 0.0f, 1.0f);
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        KT_E2E_CHECK_MSG(!has_fractional_color_bleed_col(img, W, H, 40, 10, 25, bg_u8, bd_u8),
                         "1.5x snapped 1px border must have zero fractional alpha bleed");
        const uint32_t h_px = count_matching_pixels_col(img, W, H, 40, 10, 25, bd_u8);
        KT_E2E_CHECK_MSG(h_px == 2u, "1.5x snapped 1.0px border must round to 2 physical pixels");
        return passed_case();
    }
}

template <typename Ctx = ktui::PaintContext>
CaseResult f3_t5_paint_context_draw_line_hv_and_warp_parity() {
    if constexpr (!HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 PaintContext::set_pixel_snapping not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        const ktui::Color bg_col{20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f};
        const ktui::Color line_col{240.0f / 255.0f, 240.0f / 255.0f, 240.0f / 255.0f, 1.0f};
        const std::array<uint8_t, 4> bg_u8{20, 20, 20, 255};
        const std::array<uint8_t, 4> ln_u8{240, 240, 240, 255};

        for (float scale : {1.25f, 1.5f}) {
            kernetile::RendererConfig cfg{};
            cfg.width = W;
            cfg.height = H;
            cfg.force_cpu_fallback = true;
            auto ren = kernetile::KerneTileRenderer::create(cfg);
            ren->begin_frame();
            Ctx ctx(ren.get(), scale);
            ctx.set_pixel_snapping(true);
            ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 100.0f, 100.0f), bg_col);
            ctx.draw_line(ktui::PointF{10.0f, 20.3f}, ktui::PointF{70.0f, 20.3f}, line_col, 1.0f);
            ctx.draw_line(ktui::PointF{30.3f, 30.0f}, ktui::PointF{30.3f, 70.0f}, line_col, 1.0f);
            ren->end_frame();
            auto cpu_img = render_cpu_frame(*ren, W, H);

            KT_E2E_CHECK_MSG(!has_fractional_color_bleed_col(cpu_img, W, H, 40, 18, 35, bg_u8, ln_u8),
                             "Snapped horizontal draw_line must not bleed");
            KT_E2E_CHECK_MSG(!has_fractional_color_bleed_row(cpu_img, W, H, 50, 28, 52, bg_u8, ln_u8),
                             "Snapped vertical draw_line must not bleed");
        }

#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
        kernetile::core::UiPrimitive bg_prim{};
        bg_prim.min_x = 0.0f; bg_prim.min_y = 0.0f; bg_prim.max_x = 128.0f; bg_prim.max_y = 128.0f;
        bg_prim.color_r = 20.0f / 255.0f; bg_prim.color_g = 20.0f / 255.0f;
        bg_prim.color_b = 20.0f / 255.0f; bg_prim.color_a = 1.0f;

        kernetile::core::UiPrimitive border_prim{};
        border_prim.min_x = 16.0f; border_prim.min_y = 16.0f; border_prim.max_x = 80.0f; border_prim.max_y = 64.0f;
        border_prim.color_r = 220.0f / 255.0f; border_prim.color_g = 220.0f / 255.0f;
        border_prim.color_b = 220.0f / 255.0f; border_prim.color_a = 1.0f;
        border_prim.border_width = 2.0f;

        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        ren->push_primitive(bg_prim);
        ren->push_primitive(border_prim);
        ren->end_frame();
        auto cpu_rgba = render_cpu_frame(*ren, W, H);

        D3D12OffscreenRunner warp;
        KT_E2E_CHECK(warp.is_ready());
        D3D12SceneInput scene{};
        scene.width = W; scene.height = H;
        scene.primitives = {bg_prim, border_prim};
        auto gpu_rgba = warp.render(scene);

        auto cmp = compare_rgba8(cpu_rgba, gpu_rgba, W, H);
        KT_E2E_CHECK_MSG(cmp.max_lsb_diff == 0 && cmp.differing_pixels == 0,
                         "Snapped border CPU vs D3D12 WARP must be pixel-identical");
#endif
        return passed_case();
    }
}

// ============================================================================
// F4: Pointer Reveal Engine & 16x16 Dirty-Tile Culling — 5 Tests
// ============================================================================

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_t1_abi_and_flag_contract() {
    static_assert(sizeof(kernetile::core::UiPrimitive) == 80, "UiPrimitive ABI must remain 80 bytes");
    static_assert(alignof(kernetile::core::UiPrimitive) == 16, "UiPrimitive ABI must remain 16-byte aligned");
    static_assert(sizeof(kernetile::core::ClipRect) == 32, "ClipRect ABI must remain 32 bytes");
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint16_t val = static_cast<uint16_t>(FlagsT::IsPointerReveal);
        KT_E2E_CHECK(val == 0x0200u);
        return passed_case();
    }
}

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_t2_bounded_rect_intersection() {
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 256;
        const kernetile::math::Color bg{30.0f / 255.0f, 30.0f / 255.0f, 34.0f / 255.0f, 1.0f};

        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto base_ren = kernetile::KerneTileRenderer::create(cfg);
        base_ren->begin_frame();
        base_ren->draw_rect({0.0f, 0.0f, 256.0f, 256.0f}, bg);
        base_ren->end_frame();
        auto base_img = render_cpu_frame(*base_ren, W, H);

        auto rev_ren = kernetile::KerneTileRenderer::create(cfg);
        rev_ren->begin_frame();
        rev_ren->draw_rect({0.0f, 0.0f, 256.0f, 256.0f}, bg);
        auto p = make_reveal_prim<FlagsT>(
            32.0f, 32.0f, 224.0f, 224.0f, 128.0f, 128.0f, 40.0f, 1.0f, 1.0f, 1.0f, 0.5f);
        rev_ren->push_primitive(p);
        rev_ren->end_frame();
        auto rev_img = render_cpu_frame(*rev_ren, W, H);

        bool any_inside_diff = false;
        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < W; ++x) {
                const size_t idx = (static_cast<size_t>(y) * W + x) * 4u;
                const bool diff = (base_img[idx] != rev_img[idx]) ||
                                  (base_img[idx + 1] != rev_img[idx + 1]) ||
                                  (base_img[idx + 2] != rev_img[idx + 2]);
                if (x < 86 || x > 170 || y < 86 || y > 170) {
                    KT_E2E_CHECK_MSG(!diff, "Pointer reveal must not affect pixels outside bounded rect");
                } else if (diff) {
                    any_inside_diff = true;
                }
            }
        }
        KT_E2E_CHECK(any_inside_diff);
        return passed_case();
    }
}

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_t3_radial_peak_and_decay() {
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 256;
        const kernetile::math::Color bg{24.0f / 255.0f, 24.0f / 255.0f, 28.0f / 255.0f, 1.0f};

        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 256.0f, 256.0f}, bg);
        ren->push_primitive(make_reveal_prim<FlagsT>(
            16.0f, 16.0f, 240.0f, 240.0f, 128.0f, 128.0f, 48.0f, 1.0f, 1.0f, 1.0f, 0.6f));
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        auto px_r = [&](uint32_t x, uint32_t y) { return img[(static_cast<size_t>(y) * W + x) * 4u]; };
        const uint8_t center = px_r(128, 128);
        const uint8_t mid = px_r(148, 128);
        const uint8_t edge = px_r(168, 128);
        const uint8_t outside = px_r(190, 128);

        KT_E2E_CHECK(center > mid);
        KT_E2E_CHECK(mid > edge);
        KT_E2E_CHECK(edge >= outside);
        KT_E2E_CHECK(outside == 24u);
        return passed_case();
    }
}

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_t4_dirty_tracker_localized_culling() {
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 640;
        constexpr uint32_t H = 480;
        const kernetile::math::Color bg{24.0f / 255.0f, 24.0f / 255.0f, 28.0f / 255.0f, 1.0f};

        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);

        auto render_step = [&](float cx, float cy) {
            ren->begin_frame();
            ren->draw_rect({0.0f, 0.0f, 640.0f, 480.0f}, bg);
            ren->push_primitive(make_reveal_prim<FlagsT>(
                40.0f, 40.0f, 600.0f, 440.0f, cx, cy, 32.0f, 1.0f, 1.0f, 1.0f, 0.4f));
            ren->end_frame();
            return ren->get_last_frame_stats();
        };

        auto s1 = render_step(120.0f, 120.0f);
        KT_E2E_CHECK(s1.total_tiles == 1200u);
        auto s2 = render_step(128.0f, 124.0f);
        KT_E2E_CHECK_MSG(s2.dirty_tiles > 0u, "Cursor movement must dirty overlapping tiles");
        KT_E2E_CHECK_MSG(s2.dirty_tiles <= 49u, "Pointer reveal must cull non-overlapping 16x16 tiles");
        KT_E2E_CHECK_MSG(s2.culling_ratio > 95.0, "Culling ratio must exceed 95% on 640x480 canvas");
        return passed_case();
    }
}

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_t5_cpu_vs_d3d12_warp_reveal_parity() {
#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        kernetile::core::UiPrimitive bg_prim{};
        bg_prim.min_x = 0.0f; bg_prim.min_y = 0.0f; bg_prim.max_x = 128.0f; bg_prim.max_y = 128.0f;
        bg_prim.color_r = 28.0f / 255.0f; bg_prim.color_g = 28.0f / 255.0f;
        bg_prim.color_b = 32.0f / 255.0f; bg_prim.color_a = 1.0f;

        auto rev_prim = make_reveal_prim<FlagsT>(
            16.0f, 16.0f, 112.0f, 112.0f, 64.0f, 64.0f, 40.0f, 1.0f, 1.0f, 1.0f, 0.5f);

        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        ren->push_primitive(bg_prim);
        ren->push_primitive(rev_prim);
        ren->end_frame();
        auto cpu_rgba = render_cpu_frame(*ren, W, H);

        D3D12OffscreenRunner warp;
        KT_E2E_CHECK(warp.is_ready());
        D3D12SceneInput scene{};
        scene.width = W; scene.height = H;
        scene.primitives = {bg_prim, rev_prim};
        auto gpu_rgba = warp.render(scene);

        auto cmp = compare_rgba8(cpu_rgba, gpu_rgba, W, H);
        KT_E2E_CHECK_MSG(cmp.max_lsb_diff <= 2, "CPU vs D3D12 WARP pointer reveal max LSB diff > 2");
        KT_E2E_CHECK_MSG(cmp.ssim >= 0.999, "CPU vs D3D12 WARP pointer reveal SSIM < 0.999");
        return passed_case();
    }
#else
    return passed_case();
#endif
}

// ============================================================================
// F5: Pointer Reveal Widgets (Button, NavigationView, SettingsCard) — 5 Tests
// ============================================================================

CaseResult f5_t1_default_disabled() {
    if constexpr (!has_pointer_reveal_api<ktui::Button>() ||
                  !has_pointer_reveal_api<ktui::ButtonData>() ||
                  !has_pointer_reveal_api<ktui::NavigationView>() ||
                  !has_pointer_reveal_api<ktui::NavigationViewData>() ||
                  !has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R3 enable_pointer_reveal widget fields not landed yet");
    } else {
        ktui::Button btn{};
        ktui::ButtonData btn_data{};
        ktui::NavigationView nav{};
        ktui::NavigationViewData nav_data{};
        ktui::SettingsCard card{};

        KT_E2E_CHECK(get_pointer_reveal_default(btn) == false);
        KT_E2E_CHECK(get_pointer_reveal_default(btn_data) == false);
        KT_E2E_CHECK(get_pointer_reveal_default(nav) == false);
        KT_E2E_CHECK(get_pointer_reveal_default(nav_data) == false);
        KT_E2E_CHECK(get_pointer_reveal_default(card) == false);
        return passed_case();
    }
}

CaseResult f5_t2_button_reveal_on_pointer_move() {
    if constexpr (!has_pointer_reveal_api<ktui::Button>()) {
        return pending_case("R3 Button::enable_pointer_reveal not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {320.0f, 160.0f};
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::Button b{.text = "Reveal Button"};
        set_pointer_reveal_opt(b, true);
        win->set_content(ktui::Box{
            .width = 320.0f,
            .height = 160.0f,
            .padding = ktui::EdgeInsets(20.0f),
            .child = std::move(b)
        });
        win->render_frame(true);

        auto send_move = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };

        auto fb_a = send_move(36.0f, 32.0f);
        auto fb_b = send_move(110.0f, 32.0f);
        KT_E2E_CHECK_MSG(count_diff_pixels(fb_a, fb_b) > 10,
                         "Pointer reveal Button must update pixels as pointer moves across surface");
        return passed_case();
    }
}

CaseResult f5_t3_navigation_view_item_reveal() {
    if constexpr (!has_pointer_reveal_api<ktui::NavigationView>()) {
        return pending_case("R3 NavigationView::enable_pointer_reveal not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {640.0f, 400.0f};
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::NavigationView nav{};
        nav.menu_items = {
            ktui::NavigationViewItem{.id = "home", .label = "Home", .icon = ktui::NavigationIcon::Home},
            ktui::NavigationViewItem{.id = "settings", .label = "Settings", .icon = ktui::NavigationIcon::Settings}
        };
        nav.selected_id = "home";
        nav.pane_display_mode = ktui::NavigationPaneDisplayMode::Left;
        nav.is_pane_open = true;
        set_pointer_reveal_opt(nav, true);

        win->set_content(std::move(nav));
        win->render_frame(true);

        auto send_move = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };

        auto fb_a = send_move(60.0f, 90.0f);
        auto fb_b = send_move(160.0f, 90.0f);
        KT_E2E_CHECK_MSG(count_diff_pixels(fb_a, fb_b) > 10,
                         "Pointer reveal NavigationView must update item sheen on PointerMove");
        return passed_case();
    }
}

CaseResult f5_t4_settings_card_reveal() {
    if constexpr (!has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R3 SettingsCard::enable_pointer_reveal not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {520.0f, 200.0f};
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::SettingsCard card{
            .header = "Reveal Card",
            .description = "Pointer reveal test",
            .is_click_enabled = true
        };
        set_pointer_reveal_opt(card, true);
        win->set_content(ktui::Box{
            .width = 520.0f,
            .height = 200.0f,
            .padding = ktui::EdgeInsets(16.0f),
            .child = std::move(card)
        });
        win->render_frame(true);

        auto send_move = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };

        auto fb_a = send_move(80.0f, 48.0f);
        auto fb_b = send_move(340.0f, 48.0f);
        KT_E2E_CHECK_MSG(count_diff_pixels(fb_a, fb_b) > 10,
                         "Pointer reveal SettingsCard must update highlight on PointerMove");
        return passed_case();
    }
}

CaseResult f5_t5_headless_window_localized_dirty_tiles() {
    if constexpr (!has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R3 SettingsCard::enable_pointer_reveal not landed yet");
    } else {
        constexpr uint32_t W = 640;
        constexpr uint32_t H = 480;
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {static_cast<float>(W), static_cast<float>(H)};
        auto win = app.create_window(wd);
        KT_E2E_CHECK(win != nullptr);
        win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::SettingsCard card{
            .header = "Localized Reveal",
            .description = "Checks 16x16 dirty isolation",
            .is_click_enabled = true
        };
        set_pointer_reveal_opt(card, true);
        win->set_content(ktui::Box{
            .width = static_cast<float>(W),
            .height = static_cast<float>(H),
            .padding = ktui::EdgeInsets(20.0f),
            .child = std::move(card)
        });
        win->render_frame(true);

        auto send_move = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };

        auto fb1 = send_move(100.0f, 50.0f);
        auto fb2 = send_move(116.0f, 52.0f);

        uint32_t min_x = W, min_y = H, max_x = 0, max_y = 0;
        size_t diff_cnt = 0;
        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < W; ++x) {
                const size_t i = (static_cast<size_t>(y) * W + x) * 4u;
                if (fb1[i] != fb2[i] || fb1[i + 1] != fb2[i + 1] || fb1[i + 2] != fb2[i + 2]) {
                    min_x = std::min(min_x, x);
                    min_y = std::min(min_y, y);
                    max_x = std::max(max_x, x);
                    max_y = std::max(max_y, y);
                    ++diff_cnt;
                }
            }
        }
        KT_E2E_CHECK(diff_cnt > 0);
        const uint32_t area = (max_x - min_x + 1u) * (max_y - min_y + 1u);
        KT_E2E_CHECK_MSG(area < (W * H) / 4u, "Pointer reveal invalidation must stay localized");
        return passed_case();
    }
}

// ============================================================================
// F6: Opt-in Rubber-Band Overscroll — 5 Tests
// ============================================================================

CaseResult f6_t1_default_disabled() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>() ||
                  !has_rubber_band_api<ktui::ScrollView>() ||
                  !has_rubber_band_api<ktui::VirtualList>()) {
        return pending_case("R4 rubber_band_overscroll API not landed yet");
    } else {
        ktui::SmoothScrollPhysics phys{};
        ktui::ScrollView sv{};
        ktui::VirtualList vl{};
        KT_E2E_CHECK(get_rubber_band_opt(phys) == false);
        KT_E2E_CHECK(get_rubber_band_opt(sv) == false);
        KT_E2E_CHECK(get_rubber_band_opt(vl) == false);
        return passed_case();
    }
}

CaseResult f6_t2_strict_clamp_when_disabled() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        ktui::SmoothScrollPhysics phys{};
        KT_E2E_CHECK(get_rubber_band_opt(phys) == false);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(500.0f);

        phys.scroll_to(-100.0f);
        KT_E2E_CHECK(phys.offset() >= 0.0f && phys.offset() <= 500.0f);
        KT_E2E_CHECK(phys.overscroll_amount() == 0.0f);

        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(120.0f, 0.016);
        KT_E2E_CHECK_MSG(phys.offset() == 0.0f && phys.overscroll_amount() == 0.0f,
                         "Disabled rubber_band_overscroll must clamp strictly at top 0");
        phys.end_touch_drag_for_test(0.0);

        phys.jump_to(500.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(-120.0f, 0.016);
        KT_E2E_CHECK_MSG(phys.offset() == 500.0f && phys.overscroll_amount() == 0.0f,
                         "Disabled rubber_band_overscroll must clamp strictly at max_scroll");
        phys.end_touch_drag_for_test(0.0);
        return passed_case();
    }
}

CaseResult f6_t3_hyperbolic_formula_past_top() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        constexpr float d = 400.0f;
        constexpr float c = 0.55f;
        for (float x : {50.0f, 100.0f, 150.0f}) {
            ktui::SmoothScrollPhysics phys{};
            set_rubber_band_opt(phys, true);
            phys.set_viewport_extent(d);
            phys.set_max_scroll(500.0f);
            phys.jump_to(0.0f);
            phys.begin_touch_drag();
            phys.apply_touch_drag_with_dt(x, 0.016);
            const float expected = (x * d * c) / (d + c * x);
            const float actual = -phys.offset();
            KT_E2E_CHECK_MSG(std::abs(actual - expected) < 1e-4f,
                             "Top overscroll must match (x*d*c)/(d+c*x) within 1e-4");
        }
        return passed_case();
    }
}

CaseResult f6_t4_hyperbolic_formula_past_bottom() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        constexpr float d = 400.0f;
        constexpr float c = 0.55f;
        constexpr float max_s = 500.0f;
        constexpr float x = 80.0f;

        ktui::SmoothScrollPhysics phys{};
        set_rubber_band_opt(phys, true);
        phys.set_viewport_extent(d);
        phys.set_max_scroll(max_s);
        phys.jump_to(max_s);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(-x, 0.016);

        const float expected = (x * d * c) / (d + c * x);
        const float actual = phys.offset() - max_s;
        KT_E2E_CHECK_MSG(std::abs(actual - expected) < 1e-4f,
                         "Bottom overscroll must match (x*d*c)/(d+c*x) within 1e-4");
        return passed_case();
    }
}

CaseResult f6_t5_release_handoff_to_spring_solver() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>() ||
                  !has_rubber_band_api<ktui::ScrollView>() ||
                  !has_rubber_band_api<ktui::VirtualList>()) {
        return pending_case("R4 rubber_band_overscroll APIs not landed yet");
    } else {
        ktui::SmoothScrollPhysics phys{};
        set_rubber_band_opt(phys, true);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(500.0f);
        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(100.0f, 0.016);
        KT_E2E_CHECK(phys.offset() < -20.0f);
        phys.end_touch_drag_for_test(0.0);
        KT_E2E_CHECK(phys.is_bouncing());

        bool monotonic_rebound = true;
        for (int i = 0; i < 180 && phys.step_touch_for_test(1.0 / 60.0); ++i) {
            if (phys.offset() > 1.0f) {
                monotonic_rebound = false;
            }
        }
        KT_E2E_CHECK(monotonic_rebound);
        KT_E2E_CHECK(std::abs(phys.offset()) < 0.5f);
        KT_E2E_CHECK(!phys.is_bouncing());

        ktui::ScrollView sv{.width = 200.0f, .height = 200.0f, .child = ktui::Box{.height = 600.0f}};
        set_rubber_band_opt(sv, true);
        ktui::WidgetPtr sv_w = sv;
        KT_E2E_CHECK(sv_w != nullptr);

        ktui::VirtualList vl{.item_count = 50, .item_height = 32.0f, .width = 200.0f, .height = 200.0f};
        set_rubber_band_opt(vl, true);
        ktui::WidgetPtr vl_w = vl;
        KT_E2E_CHECK(vl_w != nullptr);
        return passed_case();
    }
}

// ============================================================================
// F7: Tabular Figures (tabular_nums) Text Engine & LRU Cache — 5 Tests
// ============================================================================

template <typename TextEng = kernetile::text::ITextEngine, typename SV = std::string_view>
CaseResult f7_t1_default_false_compatibility() {
    auto engine = kernetile::text::create_text_engine();
    KT_E2E_CHECK(engine != nullptr);
    auto def_res = engine->shape_text("12345", 16.0f, "Segoe UI", 400, false);
    auto def_layout = kernetile::text::layout_text("12345", 16.0f, 400, false, "Segoe UI");
    KT_E2E_CHECK(def_res.glyphs.size() == 5u);
    KT_E2E_CHECK(def_layout != nullptr && def_layout->glyphs.size() == 5u);

    if constexpr (!HasTextEngineTabular<TextEng> || !HasLayoutTextTabular<SV>) {
        return pending_case("R5 ITextEngine::shape_text / layout_text tabular_nums not landed yet");
    } else {
        TextEng& eng = *engine;
        SV txt("12345");
        SV fam("Segoe UI");
        auto exp_res = eng.shape_text(txt, 16.0f, fam, 400, false, false);
        auto exp_layout = kernetile::text::layout_text(txt, 16.0f, 400, false, fam, false);
        KT_E2E_CHECK(std::abs(exp_res.total_width - def_res.total_width) < 1e-4f);
        KT_E2E_CHECK(std::abs(exp_layout->total_width - def_layout->total_width) < 1e-4f);
        return passed_case();
    }
}

template <typename TextEng = kernetile::text::ITextEngine>
CaseResult f7_t2_shape_text_11111_vs_88888_parity() {
    auto engine = kernetile::text::create_text_engine();
    KT_E2E_CHECK(engine != nullptr);
    if constexpr (!HasTextEngineTabular<TextEng>) {
        return pending_case("R5 ITextEngine::shape_text(..., tabular_nums) not landed yet");
    } else {
        TextEng& eng = *engine;
        auto r1 = eng.shape_text("11111", 16.0f, "Segoe UI", 400, false, true);
        auto r8 = eng.shape_text("88888", 16.0f, "Segoe UI", 400, false, true);
        KT_E2E_CHECK(r1.total_width > 10.0f && r8.total_width > 10.0f);
        KT_E2E_CHECK_MSG(std::abs(r1.total_width - r8.total_width) < 1e-3f,
                         "shape_text tabular_nums=true must align '11111' and '88888' within 1e-3 px");
        return passed_case();
    }
}

template <typename SV = std::string_view>
CaseResult f7_t3_layout_text_01234_vs_88888_parity() {
    if constexpr (!HasLayoutTextTabular<SV>) {
        return pending_case("R5 layout_text(..., tabular_nums) not landed yet");
    } else {
        SV s0("01234");
        SV s8("88888");
        SV fam("Segoe UI");
        auto l0 = kernetile::text::layout_text(s0, 18.0f, 400, false, fam, true);
        auto l8 = kernetile::text::layout_text(s8, 18.0f, 400, false, fam, true);
        KT_E2E_CHECK(l0 != nullptr && l8 != nullptr);
        KT_E2E_CHECK_MSG(std::abs(l0->total_width - l8->total_width) < 1e-3f,
                         "layout_text tabular_nums=true must align '01234' and '88888' within 1e-3 px");
        return passed_case();
    }
}

template <typename SV = std::string_view>
CaseResult f7_t4_lru_cache_key_isolation() {
    if constexpr (!HasLayoutTextTabular<SV>) {
        return pending_case("R5 layout_text(..., tabular_nums) not landed yet");
    } else {
        SV s1("11111");
        SV s8("88888");
        SV fam("Segoe UI");
        auto prop_1 = kernetile::text::layout_text(s1, 16.0f, 400, false, fam, false);
        auto prop_8 = kernetile::text::layout_text(s8, 16.0f, 400, false, fam, false);
        auto tab_1 = kernetile::text::layout_text(s1, 16.0f, 400, false, fam, true);
        auto tab_8 = kernetile::text::layout_text(s8, 16.0f, 400, false, fam, true);

        KT_E2E_CHECK(prop_1 != nullptr && tab_1 != nullptr);
        KT_E2E_CHECK_MSG(prop_1.get() != tab_1.get(),
                         "LRU cache must isolate tabular_nums=false vs tabular_nums=true entries");
        KT_E2E_CHECK(std::abs(tab_1->total_width - tab_8->total_width) < 1e-3f);
        if (prop_8->total_width - prop_1->total_width > 1.0f) {
            KT_E2E_CHECK(tab_1->total_width > prop_1->total_width + 1.0f);
        }
        return passed_case();
    }
}

template <typename Streamer = kernetile::text::TextStreamer, typename Ren = kernetile::KerneTileRenderer>
CaseResult f7_t5_text_streamer_and_draw_text_scaled() {
    if constexpr (!HasTextStreamerTabular<Streamer> || !HasRendererDrawTextTabular<Ren>) {
        return pending_case("R5 TextStreamer::stream_text / KerneTileRenderer::draw_text_scaled tabular_nums not landed yet");
    } else {
        kernetile::atlas::DynamicAtlas atlas(512, 512, kernetile::atlas::AtlasFormat::R8_UNORM);
        Streamer streamer(kernetile::text::shared_text_engine(), atlas);
        kernetile::RendererConfig cfg{};
        cfg.width = 160; cfg.height = 96; cfg.force_cpu_fallback = true;
        auto ren = Ren::create(cfg);

        std::vector<kernetile::core::UiPrimitive> prims_1, prims_8;
        auto st1 = streamer.stream_text("11111", 10.0f, 30.0f, 16.0f, {1, 1, 1, 1},
                                        prims_1, kernetile::core::INVALID_INDEX_U16, 1.0f,
                                        400, false, "Segoe UI", true);
        auto st8 = streamer.stream_text("88888", 10.0f, 30.0f, 16.0f, {1, 1, 1, 1},
                                        prims_8, kernetile::core::INVALID_INDEX_U16, 1.0f,
                                        400, false, "Segoe UI", true);
        KT_E2E_CHECK(st1.total_glyphs == 5u && st8.total_glyphs == 5u);
        KT_E2E_CHECK(prims_1.size() == 5u && prims_8.size() == 5u);

        ren->begin_frame();
        ren->draw_text_scaled("11111", 10.0f, 20.0f, 16.0f, {1, 1, 1, 1}, 1.0f, 400, false, "Segoe UI", true);
        ren->draw_text_scaled("88888", 10.0f, 50.0f, 16.0f, {1, 1, 1, 1}, 1.0f, 400, false, "Segoe UI", true);
        ren->end_frame();
        auto rgba = render_cpu_frame(*ren, 160, 96);
        KT_E2E_CHECK(fnv1a_rgba8(rgba) != 0);
        return passed_case();
    }
}

// ============================================================================
// F8: Tabular Figures (tabular_nums) in KTUI Text & TextMeasure — 5 Tests
// ============================================================================

template <typename Txt = ktui::Text>
CaseResult f8_t1_text_default_false() {
    if constexpr (!HasTabularNumsField<Txt>) {
        return pending_case("R5 ktui::Text::tabular_nums not landed yet");
    } else {
        Txt t{};
        KT_E2E_CHECK(t.tabular_nums == false);
        return passed_case();
    }
}

template <typename TM = ktui::TextMeasure>
CaseResult f8_t2_text_measure_width_parity() {
    if constexpr (!HasTextMeasureTabular<TM, ktui::FontWeight>) {
        return pending_case("R5 TextMeasure tabular_nums overloads not landed yet");
    } else {
        float w1 = TM::measure_width("11111", 16.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        float w8 = TM::measure_width("88888", 16.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        KT_E2E_CHECK(w1 > 10.0f && w8 > 10.0f);
        KT_E2E_CHECK_MSG(std::abs(w1 - w8) < 1e-3f,
                         "TextMeasure::measure_width with tabular_nums=true must align '11111' and '88888'");
        return passed_case();
    }
}

template <typename TM = ktui::TextMeasure>
CaseResult f8_t3_cursor_x_and_hit_test_alignment() {
    if constexpr (!HasTextMeasureCursorShort<TM> &&
                  !HasTextMeasureCursorFull<TM, ktui::FontWeight>) {
        return pending_case("R5 TextMeasure::cursor_x / hit_test_cursor tabular_nums not landed yet");
    } else {
        for (size_t idx = 1; idx <= 5; ++idx) {
            float x1 = measure_cursor_x_opt<TM>(
                "11111", 16.0f, idx, ktui::FontWeight::Regular, false, "Segoe UI", true);
            float x8 = measure_cursor_x_opt<TM>(
                "88888", 16.0f, idx, ktui::FontWeight::Regular, false, "Segoe UI", true);
            KT_E2E_CHECK_MSG(std::abs(x1 - x8) < 1e-3f,
                             "Tabular cursor_x positions must align identically across digits");
        }
        float mid3 = measure_cursor_x_opt<TM>(
            "11111", 16.0f, 3, ktui::FontWeight::Regular, false, "Segoe UI", true);
        size_t ht1 = hit_test_cursor_opt<TM>(
            "11111", 16.0f, mid3, ktui::FontWeight::Regular, false, "Segoe UI", true);
        size_t ht8 = hit_test_cursor_opt<TM>(
            "88888", 16.0f, mid3, ktui::FontWeight::Regular, false, "Segoe UI", true);
        KT_E2E_CHECK(ht1 == 3u && ht8 == 3u);
        return passed_case();
    }
}

template <typename Txt = ktui::Text>
CaseResult f8_t4_render_text_layout_parity() {
    if constexpr (!HasTabularNumsField<Txt>) {
        return pending_case("R5 ktui::Text::tabular_nums not landed yet");
    } else {
        auto make_node = [](std::string str) {
            Txt t{.text = std::move(str), .font_size = 16.0f};
            t.tabular_nums = true;
            ktui::WidgetPtr w = t;
            auto node = w->create_render_node();
            node->layout(ktui::BoxConstraints::loose({400.0f, 100.0f}));
            return node;
        };
        auto n1 = make_node("11111");
        auto n8 = make_node("88888");
        KT_E2E_CHECK(n1->size().x > 10.0f);
        KT_E2E_CHECK_MSG(std::abs(n1->size().x - n8->size().x) < 1e-3f,
                         "RenderText layout width with tabular_nums=true must match for '11111' and '88888'");
        return passed_case();
    }
}

template <typename Ctx = ktui::PaintContext>
CaseResult f8_t5_paint_context_draw_text_parity() {
    if constexpr (!HasPaintContextDrawTextTabular<Ctx, ktui::PointF, ktui::Color, ktui::FontWeight>) {
        return pending_case("R5 PaintContext::draw_text(..., tabular_nums) not landed yet");
    } else {
        auto render_str = [&](std::string_view str) {
            kernetile::RendererConfig cfg{};
            cfg.width = 160; cfg.height = 64; cfg.force_cpu_fallback = true;
            auto ren = kernetile::KerneTileRenderer::create(cfg);
            ren->begin_frame();
            Ctx ctx(ren.get(), 1.0f);
            ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 160.0f, 64.0f), {0, 0, 0, 1});
            ctx.draw_text(str, ktui::PointF{10.0f, 16.0f}, 16.0f, ktui::Color{1, 1, 1, 1},
                          ktui::FontWeight::Regular, false, "Segoe UI", true);
            ren->end_frame();
            return render_cpu_frame(*ren, 160, 64);
        };

        auto img1 = render_str("11111");
        auto img8 = render_str("88888");
        auto max_lit_x = [](const std::vector<uint8_t>& img) {
            uint32_t mx = 0;
            for (uint32_t y = 0; y < 64; ++y) {
                for (uint32_t x = 0; x < 160; ++x) {
                    if (img[(y * 160 + x) * 4u] > 30u) mx = std::max(mx, x);
                }
            }
            return mx;
        };
        const uint32_t x1 = max_lit_x(img1);
        const uint32_t x8 = max_lit_x(img8);
        KT_E2E_CHECK(x1 > 20u && x8 > 20u);
        KT_E2E_CHECK_MSG(std::abs(static_cast<int>(x1) - static_cast<int>(x8)) <= 2,
                         "Painted tabular '11111' and '88888' must have matching horizontal ink extent");
        return passed_case();
    }
}

// ============================================================================
// F9: ktui_gallery Integration & Showcase Pre-Checks — 5 Tests
// ============================================================================

std::string read_gallery_corpus() {
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

CaseResult f9_t1_gallery_routes_and_automation_present() {
    std::string main_src = read_workspace_file("examples/ktui_gallery/main.cpp");
    std::string app_src = read_workspace_file("examples/ktui_gallery/gallery_app.cpp");
    KT_E2E_CHECK_MSG(!main_src.empty() && !app_src.empty(), "ktui_gallery sources must be readable");
    KT_E2E_CHECK(main_src.find("--automation") != std::string::npos);
    KT_E2E_CHECK(main_src.find("OK PONG") != std::string::npos);
    KT_E2E_CHECK(app_src.find("\"button\"") != std::string::npos);
    KT_E2E_CHECK(app_src.find("\"settings_card\"") != std::string::npos);
    KT_E2E_CHECK(app_src.find("\"navigation_view\"") != std::string::npos);
    KT_E2E_CHECK(app_src.find("\"virtual_list\"") != std::string::npos);
    KT_E2E_CHECK(app_src.find("\"__settings__\"") != std::string::npos);
    return passed_case();
}

CaseResult f9_t2_gallery_enables_r1_dithering() {
    const std::string corpus = read_gallery_corpus();
    const bool has_dither = (corpus.find("set_dithering_enabled") != std::string::npos) ||
                            (corpus.find("enable_dithering") != std::string::npos);
    if (!has_dither) {
        return pending_case("M3 ktui_gallery R1 dithering integration not landed yet");
    }
    return passed_case();
}

CaseResult f9_t3_gallery_enables_r2_pixel_snapping() {
    const std::string corpus = read_gallery_corpus();
    const bool has_snap = (corpus.find("set_pixel_snapping_enabled") != std::string::npos) ||
                          (corpus.find("enable_pixel_snapping") != std::string::npos) ||
                          (corpus.find("set_pixel_snapping") != std::string::npos);
    if (!has_snap) {
        return pending_case("M3 ktui_gallery R2 pixel snapping integration not landed yet");
    }
    return passed_case();
}

CaseResult f9_t4_gallery_enables_r3_r4_reveal_and_rubber_band() {
    const std::string corpus = read_gallery_corpus();
    const bool has_reveal = (corpus.find("enable_pointer_reveal") != std::string::npos) ||
                            (corpus.find("pointer_reveal(") != std::string::npos);
    const bool has_rb = (corpus.find("rubber_band_overscroll") != std::string::npos) ||
                        (corpus.find("set_rubber_band_enabled") != std::string::npos);
    if (!has_reveal || !has_rb) {
        return pending_case("M3 ktui_gallery R3/R4 integration not landed yet");
    }
    return passed_case();
}

CaseResult f9_t5_gallery_enables_r5_tabular_nums() {
    const std::string corpus = read_gallery_corpus();
    const bool has_tnum = (corpus.find("tabular_nums") != std::string::npos);
    if (!has_tnum) {
        return pending_case("M3 ktui_gallery R5 tabular_nums integration not landed yet");
    }
    return passed_case();
}

// ============================================================================
// F10: Build, Footprint & Default Preservation — 5 Tests
// ============================================================================

CaseResult f10_t1_release_inspector_zero_guard() {
#if defined(NDEBUG)
    KT_E2E_CHECK_MSG(KTUI_INSPECTOR_ACTIVE == 0, "Release build MUST have KTUI_INSPECTOR_ACTIVE == 0");
#else
    KT_E2E_CHECK(KTUI_INSPECTOR_ACTIVE == 0 || KTUI_INSPECTOR_ACTIVE == 1);
#endif
    return passed_case();
}

CaseResult f10_t2_default_disabled_bit_exact_render() {
    constexpr uint32_t W = 256;
    constexpr uint32_t H = 128;
    const auto mica = make_horizontal_ramp_mica(20, 24, 28, 32);

    auto render_with_defaults = [&](bool explicit_false) {
        kernetile::RendererConfig cfg{};
        cfg.width = W;
        cfg.height = H;
        cfg.force_cpu_fallback = true;
        if (explicit_false) {
            set_dithering_cfg(cfg, false);
            set_snapping_cfg(cfg, false);
        }
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->set_mica_backdrop(true, mica);
        ren->begin_frame();
        ren->draw_rect({20.3f, 20.3f, 120.3f, 80.3f}, {0.9f, 0.9f, 0.9f, 1.0f}, 0.0f, 1.0f);
        ren->draw_shadowed_box({140.0f, 24.0f, 220.0f, 96.0f}, {0.2f, 0.25f, 0.3f, 1.0f},
                               6.0f, 12.0f, 0.0f, {0.0f, 3.0f});
        ren->end_frame();
        return render_cpu_frame(*ren, W, H);
    };

    auto def_img = render_with_defaults(false);
    auto exp_img = render_with_defaults(true);
    KT_E2E_CHECK_MSG(fnv1a_rgba8(def_img) == fnv1a_rgba8(exp_img),
                     "Default config render must be bit-exact with explicit false flags");
    KT_E2E_CHECK(count_diff_pixels(def_img, exp_img) == 0);
    return passed_case();
}

CaseResult f10_t3_default_disabled_scroll_clamp() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        ktui::SmoothScrollPhysics phys{};
        phys.set_viewport_extent(360.0f);
        phys.set_max_scroll(800.0f);
        phys.scroll_to(-50.0f);
        KT_E2E_CHECK(phys.offset() == 0.0f);
        phys.scroll_to(900.0f);
        KT_E2E_CHECK(phys.offset() <= 800.0f);
        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(100.0f, 0.016);
        KT_E2E_CHECK(phys.offset() == 0.0f && phys.overscroll_amount() == 0.0f);
        return passed_case();
    }
}

CaseResult f10_t4_default_disabled_text_proportional() {
    auto engine = kernetile::text::create_text_engine();
    KT_E2E_CHECK(engine != nullptr);
    std::string_view prop_fam = "Segoe UI";
    bool found_prop = false;
    for (std::string_view cand : {"Georgia", "Candara", "Constantia", "Corbel", "Bahnschrift", "Times New Roman", "Liberation Sans", "DejaVu Sans", "Segoe UI"}) {
        float w1_c = ktui::TextMeasure::measure_width("11111", 16.0f, ktui::FontWeight::Regular, false, cand, false);
        float w8_c = ktui::TextMeasure::measure_width("88888", 16.0f, ktui::FontWeight::Regular, false, cand, false);
        if (w8_c > w1_c + 1.0f) {
            prop_fam = cand;
            found_prop = true;
            break;
        }
    }
    float w1_def = ktui::TextMeasure::measure_width("11111", 16.0f, ktui::FontWeight::Regular, false, prop_fam);
    float w1_off = ktui::TextMeasure::measure_width("11111", 16.0f, ktui::FontWeight::Regular, false, prop_fam, false);
    float w8_def = ktui::TextMeasure::measure_width("88888", 16.0f, ktui::FontWeight::Regular, false, prop_fam);
    KT_E2E_CHECK(w1_def > 5.0f && w8_def > 5.0f);
    KT_E2E_CHECK(std::abs(w1_def - w1_off) < 1e-4f);
#if defined(_WIN32)
    KT_E2E_CHECK(found_prop);
#endif
    if (found_prop) {
        KT_E2E_CHECK_MSG(w8_def > w1_def + 1.0f, "Default proportional digits must remain unchanged");
    }
    return passed_case();
}

CaseResult f10_t5_no_exceptions_no_rtti_compliance() {
#if defined(_HAS_EXCEPTIONS)
    static_assert(_HAS_EXCEPTIONS == 0, "E2E suite must compile with _HAS_EXCEPTIONS=0");
#endif
#if defined(_HAS_STATIC_RTTI)
    static_assert(_HAS_STATIC_RTTI == 0, "E2E suite must compile with _HAS_STATIC_RTTI=0");
#endif
#if defined(_CPPUNWIND) || defined(__cpp_exceptions)
    static_assert(false, "E2E suite must compile without C++ exceptions (/EHs-c-)");
#endif
#if defined(_CPPRTTI)
    static_assert(false, "E2E suite must compile without RTTI (/GR-)");
#endif
#if defined(__has_feature)
    static_assert(!__has_feature(cxx_exceptions), "E2E suite must disable cxx_exceptions");
    static_assert(!__has_feature(cxx_rtti), "E2E suite must disable cxx_rtti");
#endif
    static_assert(std::is_trivially_copyable_v<kernetile::core::UiPrimitive>);
    static_assert(std::is_trivially_copyable_v<kernetile::core::ClipRect>);
    KT_E2E_CHECK(sizeof(kernetile::core::UiPrimitive) == 80);
    return passed_case();
}

} // namespace

int main() {
    TestRunner runner("KerneTile & KTUI E2E Tier 1 Coverage (F1-F10)");

    // F1: IGN Dithering Engine & Backends (5)
    runner.run_case("F1_T1_default_disabled", f1_t1_default_disabled<>);
    runner.run_case("F1_T2_toggle_api_and_pixel_effect", f1_t2_toggle_api_and_pixel_effect<>);
    runner.run_case("F1_T3_smooth_ramp_dither_quality", f1_t3_smooth_ramp_dither_quality<>);
    runner.run_case("F1_T4_cpu_vs_d3d12_warp_ign_parity", f1_t4_cpu_vs_d3d12_warp_ign_parity<>);
    runner.run_case("F1_T5_vulkan_and_webgpu_shader_contract", f1_t5_vulkan_and_webgpu_shader_contract<>);

    // F2: Banding Elimination Targets (5)
    runner.run_case("F2_T1_dark_mode_mica_4x4_ramp", f2_t1_dark_mode_mica_4x4_ramp<>);
    runner.run_case("F2_T2_slow_linear_gradient", f2_t2_slow_linear_gradient<>);
    runner.run_case("F2_T3_large_radius_gaussian_shadow", f2_t3_large_radius_gaussian_shadow<>);
    runner.run_case("F2_T4_cpu_vs_d3d12_warp_dark_ramp_and_shadow", f2_t4_cpu_vs_d3d12_warp_dark_ramp_and_shadow<>);
    runner.run_case("F2_T5_multi_channel_rgb_smooth_gradient", f2_t5_multi_channel_rgb_smooth_gradient<>);

    // F3: Fractional DPI Physical Pixel Snapping (5)
    runner.run_case("F3_T1_default_disabled", f3_t1_default_disabled<>);
    runner.run_case("F3_T2_renderer_and_window_toggle_api", f3_t2_renderer_and_window_toggle_api<>);
    runner.run_case("F3_T3_dpi_125_1px_border_crisp", f3_t3_dpi_125_1px_border_crisp<>);
    runner.run_case("F3_T4_dpi_150_1px_border_2px_crisp", f3_t4_dpi_150_1px_border_2px_crisp<>);
    runner.run_case("F3_T5_paint_context_draw_line_hv_and_warp_parity", f3_t5_paint_context_draw_line_hv_and_warp_parity<>);

    // F4: Pointer Reveal Engine & 16x16 Dirty-Tile Culling (5)
    runner.run_case("F4_T1_abi_and_flag_contract", f4_t1_abi_and_flag_contract<>);
    runner.run_case("F4_T2_bounded_rect_intersection", f4_t2_bounded_rect_intersection<>);
    runner.run_case("F4_T3_radial_peak_and_decay", f4_t3_radial_peak_and_decay<>);
    runner.run_case("F4_T4_dirty_tracker_localized_culling", f4_t4_dirty_tracker_localized_culling<>);
    runner.run_case("F4_T5_cpu_vs_d3d12_warp_reveal_parity", f4_t5_cpu_vs_d3d12_warp_reveal_parity<>);

    // F5: Pointer Reveal Widgets (5)
    runner.run_case("F5_T1_default_disabled", f5_t1_default_disabled);
    runner.run_case("F5_T2_button_reveal_on_pointer_move", f5_t2_button_reveal_on_pointer_move);
    runner.run_case("F5_T3_navigation_view_item_reveal", f5_t3_navigation_view_item_reveal);
    runner.run_case("F5_T4_settings_card_reveal", f5_t4_settings_card_reveal);
    runner.run_case("F5_T5_headless_window_localized_dirty_tiles", f5_t5_headless_window_localized_dirty_tiles);

    // F6: Rubber-Band Overscroll (5)
    runner.run_case("F6_T1_default_disabled", f6_t1_default_disabled);
    runner.run_case("F6_T2_strict_clamp_when_disabled", f6_t2_strict_clamp_when_disabled);
    runner.run_case("F6_T3_hyperbolic_formula_past_top", f6_t3_hyperbolic_formula_past_top);
    runner.run_case("F6_T4_hyperbolic_formula_past_bottom", f6_t4_hyperbolic_formula_past_bottom);
    runner.run_case("F6_T5_release_handoff_to_spring_solver", f6_t5_release_handoff_to_spring_solver);

    // F7: Tabular Figures Text Engine & LRU Cache (5)
    runner.run_case("F7_T1_default_false_compatibility", f7_t1_default_false_compatibility<>);
    runner.run_case("F7_T2_shape_text_11111_vs_88888_parity", f7_t2_shape_text_11111_vs_88888_parity<>);
    runner.run_case("F7_T3_layout_text_01234_vs_88888_parity", f7_t3_layout_text_01234_vs_88888_parity<>);
    runner.run_case("F7_T4_lru_cache_key_isolation", f7_t4_lru_cache_key_isolation<>);
    runner.run_case("F7_T5_text_streamer_and_draw_text_scaled", f7_t5_text_streamer_and_draw_text_scaled<>);

    // F8: Tabular Figures KTUI Text & TextMeasure (5)
    runner.run_case("F8_T1_text_default_false", f8_t1_text_default_false<>);
    runner.run_case("F8_T2_text_measure_width_parity", f8_t2_text_measure_width_parity<>);
    runner.run_case("F8_T3_cursor_x_and_hit_test_alignment", f8_t3_cursor_x_and_hit_test_alignment<>);
    runner.run_case("F8_T4_render_text_layout_parity", f8_t4_render_text_layout_parity<>);
    runner.run_case("F8_T5_paint_context_draw_text_parity", f8_t5_paint_context_draw_text_parity<>);

    // F9: ktui_gallery Integration Pre-Checks (5)
    runner.run_case("F9_T1_gallery_routes_and_automation_present", f9_t1_gallery_routes_and_automation_present);
    runner.run_case("F9_T2_gallery_enables_r1_dithering", f9_t2_gallery_enables_r1_dithering);
    runner.run_case("F9_T3_gallery_enables_r2_pixel_snapping", f9_t3_gallery_enables_r2_pixel_snapping);
    runner.run_case("F9_T4_gallery_enables_r3_r4_reveal_and_rubber_band", f9_t4_gallery_enables_r3_r4_reveal_and_rubber_band);
    runner.run_case("F9_T5_gallery_enables_r5_tabular_nums", f9_t5_gallery_enables_r5_tabular_nums);

    // F10: Build, Footprint & Default Preservation (5)
    runner.run_case("F10_T1_release_inspector_zero_guard", f10_t1_release_inspector_zero_guard);
    runner.run_case("F10_T2_default_disabled_bit_exact_render", f10_t2_default_disabled_bit_exact_render);
    runner.run_case("F10_T3_default_disabled_scroll_clamp", f10_t3_default_disabled_scroll_clamp);
    runner.run_case("F10_T4_default_disabled_text_proportional", f10_t4_default_disabled_text_proportional);
    runner.run_case("F10_T5_no_exceptions_no_rtti_compliance", f10_t5_no_exceptions_no_rtti_compliance);

    KT_E2E_CHECK(runner.total() == 50u);
    return runner.finalize();
}
