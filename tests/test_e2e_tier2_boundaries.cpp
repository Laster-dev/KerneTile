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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
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
// F1: IGN Dithering Boundaries — 5 Tests
// ============================================================================

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_b1_saturation_clamp_0_and_255() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 64;
        constexpr uint32_t H = 64;
        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);

        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 32.0f, 64.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
        ren->draw_rect({32.0f, 0.0f, 64.0f, 64.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < 31; ++x) {
                const size_t i = (static_cast<size_t>(y) * W + x) * 4u;
                KT_E2E_CHECK_MSG(img[i] == 0u && img[i + 1] == 0u && img[i + 2] == 0u && img[i + 3] == 255u,
                                 "Black 0.0 must never underflow-wrap to 255 under IGN dithering");
            }
            for (uint32_t x = 33; x < W; ++x) {
                const size_t i = (static_cast<size_t>(y) * W + x) * 4u;
                KT_E2E_CHECK_MSG(img[i] == 255u && img[i + 1] == 255u && img[i + 2] == 255u && img[i + 3] == 255u,
                                 "White 1.0 must never overflow-wrap to 0 under IGN dithering");
            }
        }
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_b2_exact_integer_midtone_bounds() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 64;
        constexpr uint32_t H = 64;
        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);

        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 64.0f, 64.0f}, {128.0f / 255.0f, 64.0f / 255.0f, 192.0f / 255.0f, 1.0f});
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        for (size_t i = 0; i < img.size(); i += 4) {
            KT_E2E_CHECK(std::abs(static_cast<int>(img[i + 0]) - 128) <= 1);
            KT_E2E_CHECK(std::abs(static_cast<int>(img[i + 1]) - 64) <= 1);
            KT_E2E_CHECK(std::abs(static_cast<int>(img[i + 2]) - 192) <= 1);
            KT_E2E_CHECK(img[i + 3] == 255u);
        }
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_b3_tiny_1x1_and_odd_non64_sizes() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        {
            Cfg cfg{};
            cfg.width = 1; cfg.height = 1; cfg.force_cpu_fallback = true;
            set_dithering_cfg(cfg, true);
            auto ren = Ren::create(cfg);
            ren->begin_frame();
            ren->draw_rect({0.0f, 0.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f});
            ren->end_frame();
            auto img = render_cpu_frame(*ren, 1, 1);
            KT_E2E_CHECK(img.size() == 4u && img[3] == 255u);
        }
        {
            constexpr uint32_t W = 67;
            constexpr uint32_t H = 35;
            const auto mica = make_horizontal_ramp_mica(16, 24, 32, 40);
            Cfg cfg{};
            cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
            set_dithering_cfg(cfg, true);
            auto ren = Ren::create(cfg);
            ren->set_mica_backdrop(true, mica);
            ren->begin_frame();
            ren->end_frame();
            auto cpu_rgba = render_cpu_frame(*ren, W, H);
            KT_E2E_CHECK(cpu_rgba.size() == static_cast<size_t>(W) * H * 4u);

#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
            D3D12OffscreenRunner warp;
            KT_E2E_CHECK(warp.is_ready());
            D3D12SceneInput scene{};
            scene.width = W; scene.height = H;
            scene.mica_enabled = true; scene.mica_grid = mica;
            scene.render_flags = 1u;
            auto gpu_rgba = warp.render(scene);
            auto cmp = compare_rgba8(cpu_rgba, gpu_rgba, W, H);
            KT_E2E_CHECK(cmp.max_lsb_diff <= 1);
#endif
        }
        return passed_case();
    }
}

template <typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_b4_rapid_toggle_on_off_on() {
    if constexpr (!HasRendererDitheringApi<Ren>) {
        return pending_case("R1 KerneTileRenderer dithering API not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 64;
        const auto mica = make_horizontal_ramp_mica(16, 24, 32, 40);
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, mica);

        ren->set_dithering_enabled(true);
        ren->resize(W, H); ren->begin_frame(); ren->end_frame();
        auto f1 = render_cpu_frame(*ren, W, H);

        ren->set_dithering_enabled(false);
        ren->resize(W, H); ren->begin_frame(); ren->end_frame();
        auto f2 = render_cpu_frame(*ren, W, H);

        ren->set_dithering_enabled(true);
        ren->resize(W, H); ren->begin_frame(); ren->end_frame();
        auto f3 = render_cpu_frame(*ren, W, H);

        KT_E2E_CHECK(count_diff_pixels(f1, f2) > 0);
        KT_E2E_CHECK(count_diff_pixels(f1, f3) == 0);
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f1_b5_zero_alpha_degenerate_primitive() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 64;
        constexpr uint32_t H = 64;
        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 64.0f, 64.0f}, {0.5f, 0.5f, 0.5f, 0.0f});
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);
        for (size_t i = 0; i < img.size(); i += 4) {
            KT_E2E_CHECK_MSG(img[i + 3] == 0u, "Zero-alpha primitive must keep alpha 0");
            KT_E2E_CHECK_MSG(img[i] == 0u && img[i + 1] == 0u && img[i + 2] == 0u,
                             "Zero-alpha transparent canvas must not emit RGB dither noise");
        }
        return passed_case();
    }
}

// ============================================================================
// F2: Banding Elimination Boundaries — 5 Tests
// ============================================================================

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_b1_near_black_mica() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 32;
        const auto near_black = make_horizontal_ramp_mica(12, 13, 15, 16, 0, 2);
        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->set_mica_backdrop(true, near_black);
        ren->begin_frame();
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        for (size_t i = 0; i < img.size(); i += 4) {
            KT_E2E_CHECK(img[i] >= 11u && img[i] <= 17u);
            KT_E2E_CHECK(img[i + 2] >= 13u && img[i + 2] <= 19u);
        }
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_b2_zero_blur_shadow() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 128.0f, 128.0f}, {0.2f, 0.2f, 0.2f, 1.0f});
        ren->draw_shadowed_box({32.0f, 32.0f, 96.0f, 96.0f}, {0.8f, 0.8f, 0.8f, 1.0f},
                               0.0f, 0.0f, 0.0f, {4.0f, 4.0f});
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);
        KT_E2E_CHECK(std::abs(static_cast<int>(img[(4 * W + 4) * 4]) - 51) <= 1);
        KT_E2E_CHECK(std::abs(static_cast<int>(img[(64 * W + 64) * 4]) - 204) <= 1);
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_b3_extreme_blur_64_shadow() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 256;
        constexpr uint32_t H = 256;
        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 256.0f, 256.0f}, {44.0f / 255.0f, 44.0f / 255.0f, 48.0f / 255.0f, 1.0f});
        ren->draw_shadowed_box({96.0f, 96.0f, 160.0f, 160.0f},
                               {56.0f / 255.0f, 56.0f / 255.0f, 60.0f / 255.0f, 1.0f},
                               8.0f, 64.0f, 0.0f, {0.0f, 4.0f});
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        auto tail = extract_row_channel(img, W, 128, 10, 90, 0);
        auto st = analyze_gradient_banding(tail, static_cast<double>(tail.front()), static_cast<double>(tail.back()));
        KT_E2E_CHECK(st.monotonic_blocks);
        KT_E2E_CHECK(tail.front() >= tail.back());
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_b4_flat_uniform_color_variance() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 64;
        constexpr uint32_t H = 64;
        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 64.0f, 64.0f}, {32.0f / 255.0f, 32.0f / 255.0f, 36.0f / 255.0f, 1.0f});
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        for (size_t i = 0; i < img.size(); i += 4) {
            KT_E2E_CHECK(std::abs(static_cast<int>(img[i + 0]) - 32) <= 1);
            KT_E2E_CHECK(std::abs(static_cast<int>(img[i + 1]) - 32) <= 1);
            KT_E2E_CHECK(std::abs(static_cast<int>(img[i + 2]) - 36) <= 1);
        }
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename Ren = kernetile::KerneTileRenderer>
CaseResult f2_b5_degenerate_zero_area_primitive() {
    if constexpr (!HasRendererDitheringConfig<Cfg>) {
        return pending_case("R1 dithering not landed yet");
    } else {
        constexpr uint32_t W = 64;
        constexpr uint32_t H = 64;
        Cfg cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        set_dithering_cfg(cfg, true);
        auto ren = Ren::create(cfg);
        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 64.0f, 64.0f}, {20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f});
        ren->draw_rect({40.0f, 40.0f, 20.0f, 20.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
        ren->draw_rect({10.0f, 10.0f, 10.0f, 10.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);
        for (size_t i = 0; i < img.size(); i += 4) {
            KT_E2E_CHECK(std::abs(static_cast<int>(img[i]) - 20) <= 1);
            KT_E2E_CHECK(std::abs(static_cast<int>(img[i + 1]) - 20) <= 1);
        }
        return passed_case();
    }
}

// ============================================================================
// F3: Fractional DPI Pixel Snapping Boundaries — 5 Tests
// ============================================================================

template <typename Ctx = ktui::PaintContext>
CaseResult f3_b1_scale_matrix_100_125_150_200() {
    if constexpr (!HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 PaintContext::set_pixel_snapping not landed yet");
    } else {
        constexpr uint32_t W = 160;
        constexpr uint32_t H = 160;
        const ktui::Color bg_col{18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f};
        const ktui::Color bd_col{230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 1.0f};
        const std::array<uint8_t, 4> bg_u8{18, 18, 18, 255};
        const std::array<uint8_t, 4> bd_u8{230, 230, 230, 255};

        const std::array<std::pair<float, uint32_t>, 4> cases = {{
            {1.0f, 1u}, {1.25f, 1u}, {1.5f, 2u}, {2.0f, 2u}
        }};
        for (auto [scale, expected_px] : cases) {
            kernetile::RendererConfig cfg{};
            cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
            auto ren = kernetile::KerneTileRenderer::create(cfg);
            ren->begin_frame();
            Ctx ctx(ren.get(), scale);
            ctx.set_pixel_snapping(true);
            ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, static_cast<float>(W) / scale, static_cast<float>(H) / scale), bg_col);
            ctx.draw_rect(ktui::Rect::from_xywh(10.37f, 10.37f, 36.0f, 28.0f), bd_col, 0.0f, 1.0f);
            ren->end_frame();
            auto img = render_cpu_frame(*ren, W, H);

            const uint32_t probe_x = static_cast<uint32_t>(std::round(28.0f * scale));
            KT_E2E_CHECK(!has_fractional_color_bleed_col(img, W, H, probe_x, 8, 30, bg_u8, bd_u8));
            const uint32_t actual_px = count_matching_pixels_col(img, W, H, probe_x, 8, 30, bd_u8);
            KT_E2E_CHECK(actual_px == expected_px);
        }
        return passed_case();
    }
}

template <typename Ctx = ktui::PaintContext>
CaseResult f3_b2_zero_border_width_stays_zero() {
    if constexpr (!HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 PaintContext::set_pixel_snapping not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        Ctx ctx(ren.get(), 1.25f);
        ctx.set_pixel_snapping(true);
        ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 100.0f, 100.0f), {0.1f, 0.1f, 0.1f, 1.0f});
        ctx.draw_rect(ktui::Rect::from_xywh(16.0f, 16.0f, 40.0f, 40.0f), {0.8f, 0.2f, 0.2f, 1.0f}, 0.0f, 0.0f);
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        const size_t idx = (static_cast<size_t>(45) * W + 45) * 4u;
        KT_E2E_CHECK_MSG(img[idx] >= 200u && img[idx + 1] <= 55u,
                         "border_width=0.0 must remain solid fill, not clamped up to >=1px border");
        return passed_case();
    }
}

template <typename Ctx = ktui::PaintContext>
CaseResult f3_b3_subpixel_025_border_clamps_to_1px() {
    if constexpr (!HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 PaintContext::set_pixel_snapping not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        const std::array<uint8_t, 4> bg_u8{20, 20, 20, 255};
        const std::array<uint8_t, 4> bd_u8{220, 220, 220, 255};

        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        Ctx ctx(ren.get(), 1.25f);
        ctx.set_pixel_snapping(true);
        ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 102.4f, 102.4f), {20 / 255.0f, 20 / 255.0f, 20 / 255.0f, 1.0f});
        ctx.draw_rect(ktui::Rect::from_xywh(16.0f, 16.0f, 40.0f, 40.0f), {220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f},
                      0.0f, 0.25f);
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        KT_E2E_CHECK(!has_fractional_color_bleed_col(img, W, H, 45, 16, 26, bg_u8, bd_u8));
        KT_E2E_CHECK_MSG(count_matching_pixels_col(img, W, H, 45, 16, 26, bd_u8) >= 1u,
                         "Positive sub-pixel border_width=0.25 must clamp to >= 1 physical pixel");
        return passed_case();
    }
}

template <typename Ctx = ktui::PaintContext>
CaseResult f3_b4_diagonal_line_unaffected() {
    if constexpr (!HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 PaintContext::set_pixel_snapping not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        Ctx ctx(ren.get(), 1.5f);
        ctx.set_pixel_snapping(true);
        ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 85.0f, 85.0f), {0.0f, 0.0f, 0.0f, 1.0f});
        ctx.draw_line(ktui::PointF{10.0f, 10.0f}, ktui::PointF{50.0f, 50.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 1.5f);
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        KT_E2E_CHECK(img[(45 * W + 45) * 4u] > 200u);
        KT_E2E_CHECK(img[(15 * W + 45) * 4u] < 10u);
        return passed_case();
    }
}

template <typename Ctx = ktui::PaintContext>
CaseResult f3_b5_zero_area_and_1x1_box() {
    if constexpr (!HasPaintContextSnappingApi<Ctx>) {
        return pending_case("R2 PaintContext::set_pixel_snapping not landed yet");
    } else {
        constexpr uint32_t W = 64;
        constexpr uint32_t H = 64;
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        Ctx ctx(ren.get(), 1.25f);
        ctx.set_pixel_snapping(true);
        ctx.draw_rect(ktui::Rect::from_xywh(0.0f, 0.0f, 50.0f, 50.0f), {0.0f, 0.0f, 0.0f, 1.0f});
        ctx.draw_rect(ktui::Rect::from_xywh(10.0f, 10.0f, 0.0f, 0.0f), {1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 1.0f);
        ctx.draw_rect(ktui::Rect::from_xywh(20.0f, 20.0f, 1.0f, 1.0f), {1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 0.0f);
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);
        KT_E2E_CHECK(img[(25 * W + 25) * 4u] > 200u);
        return passed_case();
    }
}

// ============================================================================
// F4: Pointer Reveal Engine Boundaries — 5 Tests
// ============================================================================

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_b1_cursor_on_tile_boundary() {
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 64;
        constexpr uint32_t H = 64;
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 64.0f, 64.0f}, {24.0f / 255.0f, 24.0f / 255.0f, 24.0f / 255.0f, 1.0f});
        ren->push_primitive(make_reveal_prim<FlagsT>(
            0.0f, 0.0f, 64.0f, 64.0f, 16.0f, 16.0f, 24.0f, 1.0f, 1.0f, 1.0f, 0.6f));
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        const uint8_t p_tl = img[(15 * W + 15) * 4u];
        const uint8_t p_br = img[(16 * W + 16) * 4u];
        const uint8_t p_tr = img[(15 * W + 16) * 4u];
        const uint8_t p_bl = img[(16 * W + 15) * 4u];
        KT_E2E_CHECK(p_tl > 40u);
        KT_E2E_CHECK(std::abs(static_cast<int>(p_tl) - static_cast<int>(p_br)) <= 2);
        KT_E2E_CHECK(std::abs(static_cast<int>(p_tr) - static_cast<int>(p_bl)) <= 2);
        return passed_case();
    }
}

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_b2_cursor_outside_widget_bounds() {
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 512;
        constexpr uint32_t H = 512;
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;

        auto base_ren = kernetile::KerneTileRenderer::create(cfg);
        base_ren->begin_frame();
        base_ren->draw_rect({0.0f, 0.0f, 512.0f, 512.0f}, {0.1f, 0.1f, 0.1f, 1.0f});
        base_ren->end_frame();
        auto base_img = render_cpu_frame(*base_ren, W, H);

        auto rev_ren = kernetile::KerneTileRenderer::create(cfg);
        rev_ren->begin_frame();
        rev_ren->draw_rect({0.0f, 0.0f, 512.0f, 512.0f}, {0.1f, 0.1f, 0.1f, 1.0f});
        rev_ren->push_primitive(make_reveal_prim<FlagsT>(
            10.0f, 10.0f, 100.0f, 60.0f, 400.0f, 400.0f, 32.0f, 1.0f, 1.0f, 1.0f, 0.5f));
        rev_ren->end_frame();
        auto rev_img = render_cpu_frame(*rev_ren, W, H);

        KT_E2E_CHECK_MSG(count_diff_pixels(base_img, rev_img) == 0,
                         "Empty (cursor+-radius) intersect bounds must produce 0 reveal pixels");
        return passed_case();
    }
}

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_b3_zero_radius_reveal() {
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 64;
        constexpr uint32_t H = 64;
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 64.0f, 64.0f}, {30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f});
        ren->push_primitive(make_reveal_prim<FlagsT>(
            0.0f, 0.0f, 64.0f, 64.0f, 32.0f, 32.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f));
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);
        KT_E2E_CHECK(img[(10 * W + 10) * 4u] == 30u);
        KT_E2E_CHECK(img[(35 * W + 35) * 4u] == 30u);
        return passed_case();
    }
}

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_b4_rapid_sweep_4_corners() {
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 320;
        constexpr uint32_t H = 240;
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);

        const std::array<std::pair<float, float>, 4> corners = {{
            {40.0f, 40.0f}, {280.0f, 40.0f}, {280.0f, 200.0f}, {40.0f, 200.0f}
        }};
        std::vector<uint8_t> fb(static_cast<size_t>(W) * H * 4u, 0u);

        for (size_t idx = 0; idx < corners.size(); ++idx) {
            auto [cx, cy] = corners[idx];
            ren->begin_frame();
            ren->draw_rect({0.0f, 0.0f, 320.0f, 240.0f}, {24.0f / 255.0f, 24.0f / 255.0f, 28.0f / 255.0f, 1.0f});
            ren->push_primitive(make_reveal_prim<FlagsT>(
                16.0f, 16.0f, 304.0f, 224.0f, cx, cy, 28.0f, 1.0f, 1.0f, 1.0f, 0.5f));
            ren->end_frame();
            ren->render_to_rgba8(fb.data());

            const auto& stats = ren->get_last_frame_stats();
            if (idx > 0) {
                KT_E2E_CHECK(stats.dirty_tiles > 0u && stats.dirty_tiles < 64u);
            }
            const uint32_t px = static_cast<uint32_t>(cx);
            const uint32_t py = static_cast<uint32_t>(cy);
            KT_E2E_CHECK(fb[(py * W + px) * 4u] > 40u);
        }
        return passed_case();
    }
}

template <typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f4_b5_clipped_by_rounded_clip_rect() {
    if constexpr (!HasPointerRevealFlag<FlagsT>) {
        return pending_case("R3 PrimitiveFlags::IsPointerReveal not landed yet");
    } else {
        constexpr uint32_t W = 128;
        constexpr uint32_t H = 128;
        kernetile::RendererConfig cfg{};
        cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);

        ren->begin_frame();
        ren->draw_rect({0.0f, 0.0f, 128.0f, 128.0f}, {20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f});
        ren->push_clip({16.0f, 16.0f, 112.0f, 112.0f}, 24.0f);
        ren->push_primitive(make_reveal_prim<FlagsT>(
            16.0f, 16.0f, 112.0f, 112.0f, 24.0f, 24.0f, 36.0f, 1.0f, 1.0f, 1.0f, 0.6f));
        ren->pop_clip();
        ren->end_frame();
        auto img = render_cpu_frame(*ren, W, H);

        KT_E2E_CHECK_MSG(img[(17 * W + 17) * 4u] == 20u,
                         "Pointer reveal must respect rounded ClipRect corner");
        KT_E2E_CHECK(img[(28 * W + 28) * 4u] > 40u);
        return passed_case();
    }
}

// ============================================================================
// F5: Pointer Reveal Widgets Boundaries — 5 Tests
// ============================================================================

CaseResult f5_b1_enter_move_exit_clears_reveal() {
    if constexpr (!has_pointer_reveal_api<ktui::Button>()) {
        return pending_case("R3 Button::enable_pointer_reveal not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {320.0f, 160.0f};
        auto win = app.create_window(wd);
        if (win) win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::Button b{.text = "Hover Exit"};
        set_pointer_reveal_opt(b, true);
        win->set_content(ktui::Box{
            .width = 320.0f, .height = 160.0f,
            .padding = ktui::EdgeInsets(24.0f),
            .child = std::move(b)
        });
        win->render_frame(true);
        auto baseline = read_headless_rgba8(*hw);

        ktui::PlatformEvent move_in{};
        move_in.type = ktui::PlatformEventType::PointerMove;
        move_in.device_kind = ktui::PointerDeviceKind::Mouse;
        move_in.position = {44.0f, 36.0f};
        win->handle_platform_event(move_in);
        win->render_frame();
        auto hovered = read_headless_rgba8(*hw);
        KT_E2E_CHECK(count_diff_pixels(baseline, hovered) > 10);

        ktui::PlatformEvent move_out{};
        move_out.type = ktui::PlatformEventType::PointerMove;
        move_out.device_kind = ktui::PointerDeviceKind::Mouse;
        move_out.position = {300.0f, 140.0f};
        win->handle_platform_event(move_out);
        win->render_frame();
        auto exited = read_headless_rgba8(*hw);
        KT_E2E_CHECK_MSG(count_diff_pixels(baseline, exited) == 0,
                         "Moving pointer out of reveal Button must clear reveal cleanly");
        return passed_case();
    }
}

CaseResult f5_b2_disabled_widget_no_reveal() {
    if constexpr (!has_pointer_reveal_api<ktui::Button>() ||
                  !has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R3 widget enable_pointer_reveal not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {400.0f, 240.0f};
        auto win = app.create_window(wd);
        if (win) win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::Button b{.text = "Disabled", .disabled = true};
        set_pointer_reveal_opt(b, true);
        ktui::SettingsCard c{.header = "Disabled Card", .is_click_enabled = true, .disabled = true};
        set_pointer_reveal_opt(c, true);

        win->set_content(ktui::VStack{
            .spacing = 12.0f,
            .padding = ktui::EdgeInsets(16.0f),
            .content = {std::move(b), std::move(c)}
        });
        win->render_frame(true);
        auto base = read_headless_rgba8(*hw);

        ktui::PlatformEvent pe{};
        pe.type = ktui::PlatformEventType::PointerMove;
        pe.device_kind = ktui::PointerDeviceKind::Mouse;
        pe.position = {60.0f, 30.0f};
        win->handle_platform_event(pe);
        win->render_frame();
        auto after_btn = read_headless_rgba8(*hw);

        pe.position = {60.0f, 90.0f};
        win->handle_platform_event(pe);
        win->render_frame();
        auto after_card = read_headless_rgba8(*hw);

        KT_E2E_CHECK(count_diff_pixels(base, after_btn) == 0);
        KT_E2E_CHECK(count_diff_pixels(base, after_card) == 0);
        return passed_case();
    }
}

CaseResult f5_b3_pointer_outside_widget_bounds() {
    if constexpr (!has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R3 SettingsCard::enable_pointer_reveal not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {400.0f, 300.0f};
        auto win = app.create_window(wd);
        if (win) win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::SettingsCard card{.header = "Bounded Card", .is_click_enabled = true};
        set_pointer_reveal_opt(card, true);
        win->set_content(ktui::Box{
            .width = 400.0f, .height = 300.0f,
            .padding = ktui::EdgeInsets(20.0f),
            .child = std::move(card),
            .alignment = ktui::Alignment::top_left()
        });
        win->render_frame(true);
        auto base = read_headless_rgba8(*hw);

        ktui::PlatformEvent pe{};
        pe.type = ktui::PlatformEventType::PointerMove;
        pe.device_kind = ktui::PointerDeviceKind::Mouse;
        pe.position = {380.0f, 280.0f};
        win->handle_platform_event(pe);
        win->render_frame();
        auto after = read_headless_rgba8(*hw);
        KT_E2E_CHECK(count_diff_pixels(base, after) == 0);
        return passed_case();
    }
}

CaseResult f5_b4_consecutive_moves_same_tile() {
    if constexpr (!has_pointer_reveal_api<ktui::SettingsCard>()) {
        return pending_case("R3 SettingsCard::enable_pointer_reveal not landed yet");
    } else {
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {640.0f, 480.0f};
        auto win = app.create_window(wd);
        if (win) win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::SettingsCard card{.header = "Same Tile Card", .is_click_enabled = true};
        set_pointer_reveal_opt(card, true);
        win->set_content(ktui::Box{
            .width = 640.0f, .height = 480.0f,
            .padding = ktui::EdgeInsets(16.0f),
            .child = std::move(card)
        });
        win->render_frame(true);

        auto move_at = [&](float x, float y) {
            ktui::PlatformEvent pe{};
            pe.type = ktui::PlatformEventType::PointerMove;
            pe.device_kind = ktui::PointerDeviceKind::Mouse;
            pe.position = {x, y};
            win->handle_platform_event(pe);
            win->render_frame();
            return read_headless_rgba8(*hw);
        };

        auto fb1 = move_at(66.0f, 34.0f);
        auto fb2 = move_at(70.0f, 36.0f);
        KT_E2E_CHECK(count_diff_pixels(fb1, fb2) > 0);
        return passed_case();
    }
}

CaseResult f5_b5_tiny_button_smaller_than_radius() {
    if constexpr (!has_pointer_reveal_api<ktui::Button>()) {
        return pending_case("R3 Button::enable_pointer_reveal not landed yet");
    } else {
        constexpr uint32_t W = 200;
        constexpr uint32_t H = 200;
        ktui::Application app(ktui::create_platform_backend(true));
        ktui::WindowDesc wd{};
        wd.size = {static_cast<float>(W), static_cast<float>(H)};
        auto win = app.create_window(wd);
        if (win) win->set_show_titlebar(false);
        auto* hw = static_cast<ktui::HeadlessWindow*>(win->platform_window());

        ktui::Button b{.text = "X", .height = 20.0f, .padding = ktui::EdgeInsets(2.0f, 6.0f)};
        set_pointer_reveal_opt(b, true);
        win->set_content(ktui::Box{
            .width = static_cast<float>(W), .height = static_cast<float>(H),
            .padding = ktui::EdgeInsets(80.0f),
            .child = std::move(b),
            .alignment = ktui::Alignment::top_left()
        });
        win->render_frame(true);
        auto base = read_headless_rgba8(*hw);

        ktui::PlatformEvent pe{};
        pe.type = ktui::PlatformEventType::PointerMove;
        pe.device_kind = ktui::PointerDeviceKind::Mouse;
        pe.position = {90.0f, 90.0f};
        win->handle_platform_event(pe);
        win->render_frame();
        auto after = read_headless_rgba8(*hw);

        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < W; ++x) {
                if (x < 75 || x > 145 || y < 75 || y > 110) {
                    const size_t i = (static_cast<size_t>(y) * W + x) * 4u;
                    KT_E2E_CHECK(base[i] == after[i] && base[i + 1] == after[i + 1] && base[i + 2] == after[i + 2]);
                }
            }
        }
        return passed_case();
    }
}

// ============================================================================
// F6: Rubber-Band Overscroll Boundaries — 5 Tests
// ============================================================================

CaseResult f6_b1_zero_displacement_exact_zero() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        ktui::SmoothScrollPhysics phys{};
        set_rubber_band_opt(phys, true);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(500.0f);
        phys.jump_to(0.0f);
        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(0.0f, 0.016);
        KT_E2E_CHECK(phys.offset() == 0.0f);
        KT_E2E_CHECK(phys.overscroll_amount() == 0.0f);
        return passed_case();
    }
}

CaseResult f6_b2_tiny_and_huge_displacement_asymptote() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        constexpr float d = 400.0f;
        constexpr float c = 0.55f;
        {
            ktui::SmoothScrollPhysics phys{};
            set_rubber_band_opt(phys, true);
            phys.set_viewport_extent(d);
            phys.set_max_scroll(500.0f);
            phys.jump_to(0.0f);
            phys.begin_touch_drag();
            phys.apply_touch_drag_with_dt(0.5f, 0.016);
            const float expected = (0.5f * d * c) / (d + c * 0.5f);
            KT_E2E_CHECK(std::abs((-phys.offset()) - expected) < 1e-4f);
        }
        {
            ktui::SmoothScrollPhysics phys{};
            set_rubber_band_opt(phys, true);
            phys.set_viewport_extent(d);
            phys.set_max_scroll(500.0f);
            phys.jump_to(0.0f);
            phys.begin_touch_drag();
            phys.apply_touch_drag_with_dt(5000.0f, 0.016);
            const float mag = -phys.offset();
            KT_E2E_CHECK(std::isfinite(mag));
            KT_E2E_CHECK(mag > 50.0f && mag < d);
        }
        return passed_case();
    }
}

CaseResult f6_b3_wheel_scroll_vs_touch_drag() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        ktui::SmoothScrollPhysics phys{};
        set_rubber_band_opt(phys, true);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(500.0f);
        phys.jump_to(0.0f);
        phys.add_wheel_delta(-360.0f);
        for (int i = 0; i < 30; ++i) phys.step_for_test(0.016f * i);
        KT_E2E_CHECK(std::isfinite(phys.offset()));
        KT_E2E_CHECK(phys.offset() >= -400.0f && phys.offset() <= 500.0f);
        return passed_case();
    }
}

CaseResult f6_b4_max_scroll_zero_non_scrollable() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        {
            ktui::SmoothScrollPhysics phys{};
            phys.set_viewport_extent(400.0f);
            phys.set_max_scroll(0.0f);
            phys.begin_touch_drag();
            phys.apply_touch_drag_with_dt(80.0f, 0.016);
            KT_E2E_CHECK(phys.offset() == 0.0f);
        }
        {
            ktui::SmoothScrollPhysics phys{};
            set_rubber_band_opt(phys, true);
            phys.set_viewport_extent(400.0f);
            phys.set_max_scroll(0.0f);
            phys.begin_touch_drag();
            phys.apply_touch_drag_with_dt(80.0f, 0.016);
            KT_E2E_CHECK(phys.offset() < 0.0f);
            phys.end_touch_drag_for_test(0.0);
            for (int i = 0; i < 180 && phys.step_touch_for_test(1.0 / 60.0); ++i) {}
            KT_E2E_CHECK(std::isfinite(phys.offset()) && std::abs(phys.offset()) < 0.5f);
        }
        return passed_case();
    }
}

CaseResult f6_b5_velocity_reversal_mid_rebound() {
    if constexpr (!has_rubber_band_api<ktui::SmoothScrollPhysics>()) {
        return pending_case("R4 SmoothScrollPhysics rubber_band_overscroll not landed yet");
    } else {
        ktui::SmoothScrollPhysics phys{};
        set_rubber_band_opt(phys, true);
        phys.set_viewport_extent(400.0f);
        phys.set_max_scroll(600.0f);
        phys.jump_to(0.0f);

        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(120.0f, 0.016);
        phys.end_touch_drag_for_test(0.0);
        for (int i = 0; i < 5; ++i) phys.step_touch_for_test(1.0 / 60.0);

        phys.begin_touch_drag();
        phys.apply_touch_drag_with_dt(-90.0f, 0.016);
        phys.apply_touch_drag_with_dt(-90.0f, 0.016);
        phys.end_touch_drag_for_test(0.0);

        for (int i = 0; i < 240 && phys.step_touch_for_test(1.0 / 60.0); ++i) {
            KT_E2E_CHECK(std::isfinite(phys.offset()) && std::isfinite(phys.velocity()));
        }
        KT_E2E_CHECK(phys.offset() >= 0.0f && phys.offset() <= 600.0f);
        return passed_case();
    }
}

// ============================================================================
// F7: Tabular Figures Text Engine Boundaries — 5 Tests
// ============================================================================

template <typename TextEng = kernetile::text::ITextEngine, typename SV = std::string_view>
CaseResult f7_b1_empty_string() {
    if constexpr (!HasTextEngineTabular<TextEng> || !HasLayoutTextTabular<SV>) {
        return pending_case("R5 tabular_nums text engine API not landed yet");
    } else {
        auto eng_ptr = kernetile::text::create_text_engine();
        TextEng& eng = *eng_ptr;
        SV empty_sv("");
        SV fam("Segoe UI");
        auto s = eng.shape_text(empty_sv, 16.0f, fam, 400, false, true);
        auto l = kernetile::text::layout_text(empty_sv, 16.0f, 400, false, fam, true);
        KT_E2E_CHECK(s.total_width == 0.0f && s.glyphs.empty());
        KT_E2E_CHECK(l != nullptr && l->total_width == 0.0f && l->glyphs.empty());
        return passed_case();
    }
}

template <typename TextEng = kernetile::text::ITextEngine>
CaseResult f7_b2_single_char_1_vs_8() {
    if constexpr (!HasTextEngineTabular<TextEng>) {
        return pending_case("R5 ITextEngine::shape_text tabular_nums not landed yet");
    } else {
        auto eng_ptr = kernetile::text::create_text_engine();
        TextEng& eng = *eng_ptr;
        auto r1 = eng.shape_text("1", 16.0f, "Segoe UI", 400, false, true);
        auto r8 = eng.shape_text("8", 16.0f, "Segoe UI", 400, false, true);
        KT_E2E_CHECK(r1.total_width > 2.0f);
        KT_E2E_CHECK(std::abs(r1.total_width - r8.total_width) < 1e-3f);
        return passed_case();
    }
}

template <typename SV = std::string_view>
CaseResult f7_b3_mixed_alphanumeric_fps() {
    if constexpr (!HasLayoutTextTabular<SV>) {
        return pending_case("R5 layout_text tabular_nums not landed yet");
    } else {
        SV s1("FPS: 111.1%");
        SV s8("FPS: 888.8%");
        SV fam("Segoe UI");
        auto a = kernetile::text::layout_text(s1, 16.0f, 400, false, fam, true);
        auto b = kernetile::text::layout_text(s8, 16.0f, 400, false, fam, true);
        KT_E2E_CHECK(a && b);
        KT_E2E_CHECK(std::abs(a->total_width - b->total_width) < 1e-3f);
        return passed_case();
    }
}

template <typename SV = std::string_view>
CaseResult f7_b4_punctuation_and_decimals() {
    if constexpr (!HasLayoutTextTabular<SV>) {
        return pending_case("R5 layout_text tabular_nums not landed yet");
    } else {
        SV s1("12,345.67");
        SV s8("89,012.34");
        SV fam("Segoe UI");
        auto a = kernetile::text::layout_text(s1, 18.0f, 600, false, fam, true);
        auto b = kernetile::text::layout_text(s8, 18.0f, 600, false, fam, true);
        KT_E2E_CHECK(a && b);
        KT_E2E_CHECK(std::abs(a->total_width - b->total_width) < 1e-3f);
        return passed_case();
    }
}

template <typename SV = std::string_view>
CaseResult f7_b5_cache_key_matrix_weight_size_tabular() {
    if constexpr (!HasLayoutTextTabular<SV>) {
        return pending_case("R5 layout_text tabular_nums not landed yet");
    } else {
        SV s1("11111");
        SV fam("Segoe UI");
        auto p14_reg_f = kernetile::text::layout_text(s1, 14.0f, 400, false, fam, false);
        auto p14_reg_t = kernetile::text::layout_text(s1, 14.0f, 400, false, fam, true);
        auto p14_bld_t = kernetile::text::layout_text(s1, 14.0f, 700, false, fam, true);
        auto p18_reg_t = kernetile::text::layout_text(s1, 18.0f, 400, false, fam, true);
        auto p18_reg_t2 = kernetile::text::layout_text(s1, 18.0f, 400, false, fam, true);

        KT_E2E_CHECK(p14_reg_f.get() != p14_reg_t.get());
        KT_E2E_CHECK(p14_reg_t.get() != p14_bld_t.get());
        KT_E2E_CHECK(p14_reg_t.get() != p18_reg_t.get());
        KT_E2E_CHECK(p18_reg_t.get() == p18_reg_t2.get());
        KT_E2E_CHECK(p18_reg_t->total_width > p14_reg_t->total_width);
        return passed_case();
    }
}

// ============================================================================
// F8: Tabular Figures KTUI Text Boundaries — 5 Tests
// ============================================================================

template <typename Txt = ktui::Text>
CaseResult f8_b1_empty_text_widget() {
    if constexpr (!HasTabularNumsField<Txt>) {
        return pending_case("R5 ktui::Text::tabular_nums not landed yet");
    } else {
        Txt t{.text = "", .font_size = 16.0f};
        t.tabular_nums = true;
        ktui::WidgetPtr w = t;
        auto node = w->create_render_node();
        node->layout(ktui::BoxConstraints::loose({200.0f, 100.0f}));
        KT_E2E_CHECK(node->size().x == 0.0f);

        kernetile::RendererConfig cfg{};
        cfg.width = 64; cfg.height = 64; cfg.force_cpu_fallback = true;
        auto ren = kernetile::KerneTileRenderer::create(cfg);
        ren->begin_frame();
        ktui::PaintContext ctx(ren.get(), 1.0f);
        node->paint(ctx);
        ren->end_frame();
        return passed_case();
    }
}

template <typename Txt = ktui::Text, typename TM = ktui::TextMeasure>
CaseResult f8_b2_multiline_wrapped_numeric() {
    if constexpr (!HasTabularNumsField<Txt> ||
                  !HasTextMeasureTabular<TM, ktui::FontWeight>) {
        return pending_case("R5 ktui::Text / TextMeasure tabular_nums not landed yet");
    } else {
        Txt t{.text = "11111\n88888", .font_size = 16.0f, .overflow = ktui::TextOverflow::Wrap};
        t.tabular_nums = true;
        ktui::WidgetPtr w = t;
        auto node = w->create_render_node();
        node->layout(ktui::BoxConstraints::loose({60.0f, 200.0f}));
        KT_E2E_CHECK(node->size().y >= 32.0f);

        float w1 = TM::measure_width("11111", 16.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        float w8 = TM::measure_width("88888", 16.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        KT_E2E_CHECK(std::abs(w1 - w8) < 1e-3f);
        return passed_case();
    }
}

template <typename TM = ktui::TextMeasure>
CaseResult f8_b3_cursor_x_zero_and_end() {
    if constexpr (!HasTextMeasureCursorShort<TM> &&
                  !HasTextMeasureCursorFull<TM, ktui::FontWeight>) {
        return pending_case("R5 TextMeasure::cursor_x tabular_nums not landed yet");
    } else {
        float x0 = measure_cursor_x_opt<TM>(
            "12345", 16.0f, 0, ktui::FontWeight::Regular, false, "Segoe UI", true);
        float x5 = measure_cursor_x_opt<TM>(
            "12345", 16.0f, 5, ktui::FontWeight::Regular, false, "Segoe UI", true);
        float total_w = 0.0f;
        if constexpr (HasTextMeasureTabular<TM, ktui::FontWeight>) {
            total_w = TM::measure_width("12345", 16.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        } else {
            total_w = TM::measure_width("12345", 16.0f);
        }
        KT_E2E_CHECK(x0 == 0.0f);
        KT_E2E_CHECK(std::abs(x5 - total_w) < 1e-3f);
        return passed_case();
    }
}

template <typename TM = ktui::TextMeasure>
CaseResult f8_b4_hit_test_cursor_clamping() {
    if constexpr (!HasTextMeasureCursorShort<TM> &&
                  !HasTextMeasureCursorFull<TM, ktui::FontWeight>) {
        return pending_case("R5 TextMeasure::hit_test_cursor tabular_nums not landed yet");
    } else {
        size_t lo = hit_test_cursor_opt<TM>(
            "12345", 16.0f, -100.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        size_t hi = hit_test_cursor_opt<TM>(
            "12345", 16.0f, 9999.0f, ktui::FontWeight::Regular, false, "Segoe UI", true);
        KT_E2E_CHECK(lo == 0u);
        KT_E2E_CHECK(hi == 5u);
        return passed_case();
    }
}

template <typename TM = ktui::TextMeasure>
CaseResult f8_b5_text_measure_at_125_and_150_scale() {
    if constexpr (!HasTextMeasureTabular<TM, ktui::FontWeight>) {
        return pending_case("R5 TextMeasure tabular_nums not landed yet");
    } else {
        for (float scale : {1.25f, 1.5f}) {
            float w1 = TM::measure_width("11111", 16.0f * scale, ktui::FontWeight::Regular, false, "Segoe UI", true);
            float w8 = TM::measure_width("88888", 16.0f * scale, ktui::FontWeight::Regular, false, "Segoe UI", true);
            KT_E2E_CHECK(std::abs(w1 - w8) < 1e-3f);
        }
        return passed_case();
    }
}

// ============================================================================
// F9: ktui_gallery Boundary & Config Pre-Checks — 5 Tests
// ============================================================================

CaseResult f9_b1_gallery_showcase_routes_declared() {
    const std::string app_src = read_workspace_file("examples/ktui_gallery/gallery_app.cpp");
    KT_E2E_CHECK(!app_src.empty());
    const std::array<const char*, 10> routes = {
        "\"button\"", "\"settings_card\"", "\"navigation_view\"", "\"virtual_list\"",
        "\"data_grid\"", "\"progress_bar\"", "\"slider\"", "\"label\"",
        "\"__settings__\"", "\"about\""
    };
    for (const char* r : routes) {
        KT_E2E_CHECK_MSG(app_src.find(r) != std::string::npos, r);
    }
    return passed_case();
}

CaseResult f9_b2_gallery_theme_toggle_constants() {
    const std::string main_src = read_workspace_file("examples/ktui_gallery/main.cpp");
    KT_E2E_CHECK(main_src.find("\"THEME\"") != std::string::npos);
    KT_E2E_CHECK(main_src.find("Theme::dark()") != std::string::npos);
    KT_E2E_CHECK(main_src.find("Theme::light()") != std::string::npos);
    return passed_case();
}

CaseResult f9_b3_gallery_dpi_and_resize_support() {
    const std::string main_src = read_workspace_file("examples/ktui_gallery/main.cpp");
    KT_E2E_CHECK(main_src.find("\"RESIZE\"") != std::string::npos);
    KT_E2E_CHECK(main_src.find("\\\"dpi\\\":") != std::string::npos);
    return passed_case();
}

CaseResult f9_b4_gallery_automation_timeout_and_quit() {
    const std::string main_src = read_workspace_file("examples/ktui_gallery/main.cpp");
    KT_E2E_CHECK(main_src.find("\"QUIT\"") != std::string::npos);
    KT_E2E_CHECK(main_src.find("fflush(stdout)") != std::string::npos);
    return passed_case();
}

CaseResult f9_b5_gallery_no_debug_only_leakage() {
    const std::string main_src = read_workspace_file("examples/ktui_gallery/main.cpp");
    const std::string app_src = read_workspace_file("examples/ktui_gallery/gallery_app.cpp");
    if (main_src.find("ui_inspector") != std::string::npos ||
        app_src.find("ui_inspector") != std::string::npos ||
        main_src.find("devtools") != std::string::npos ||
        app_src.find("devtools") != std::string::npos) {
        KT_E2E_CHECK(main_src.find("KTUI_INSPECTOR_ACTIVE") != std::string::npos ||
                     app_src.find("KTUI_INSPECTOR_ACTIVE") != std::string::npos);
    }
#if defined(NDEBUG)
    KT_E2E_CHECK(KTUI_INSPECTOR_ACTIVE == 0);
#endif
    return passed_case();
}

// ============================================================================
// F10: Footprint, Parity & Default Preservation Boundaries — 5 Tests
// ============================================================================

CaseResult f10_b1_cpu_vs_d3d12_warp_odd_size_130x66() {
#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
    constexpr uint32_t W = 130;
    constexpr uint32_t H = 66;
    const auto mica = make_horizontal_ramp_mica(18, 22, 26, 30);

    kernetile::core::UiPrimitive card{};
    card.min_x = 12.0f; card.min_y = 10.0f; card.max_x = 118.0f; card.max_y = 56.0f;
    card.radius_top_left = card.radius_top_right = card.radius_bottom_right = card.radius_bottom_left = 6.0f;
    card.color_r = 0.2f; card.color_g = 0.25f; card.color_b = 0.35f; card.color_a = 1.0f;
    card.border_width = 1.5f;

    kernetile::RendererConfig cfg{};
    cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
    auto ren = kernetile::KerneTileRenderer::create(cfg);
    ren->set_mica_backdrop(true, mica);
    ren->begin_frame();
    ren->push_primitive(card);
    ren->end_frame();
    auto cpu_rgba = render_cpu_frame(*ren, W, H);

    D3D12OffscreenRunner warp;
    KT_E2E_CHECK(warp.is_ready());
    D3D12SceneInput scene{};
    scene.width = W; scene.height = H;
    scene.primitives = {card};
    scene.mica_enabled = true;
    scene.mica_grid = mica;
    auto gpu_rgba = warp.render(scene);

    auto cmp = compare_rgba8(cpu_rgba, gpu_rgba, W, H);
    KT_E2E_CHECK_MSG(cmp.max_lsb_diff <= 1 && cmp.ssim >= 0.999,
                     "Odd-width 130x66 CPU vs D3D12 WARP readback parity failed");
    return passed_case();
#else
    return passed_case();
#endif
}

CaseResult f10_b2_zero_primitive_empty_frame_parity() {
#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
    constexpr uint32_t W = 128;
    constexpr uint32_t H = 64;
    kernetile::RendererConfig cfg{};
    cfg.width = W; cfg.height = H; cfg.force_cpu_fallback = true;
    auto ren = kernetile::KerneTileRenderer::create(cfg);
    ren->begin_frame();
    ren->end_frame();
    auto cpu_rgba = render_cpu_frame(*ren, W, H);

    D3D12OffscreenRunner warp;
    KT_E2E_CHECK(warp.is_ready());
    D3D12SceneInput scene{};
    scene.width = W; scene.height = H;
    auto gpu_rgba = warp.render(scene);

    KT_E2E_CHECK(count_diff_pixels(cpu_rgba, gpu_rgba) == 0);
    for (uint8_t byte : gpu_rgba) {
        KT_E2E_CHECK(byte == 0u);
    }
    return passed_case();
#else
    return passed_case();
#endif
}

CaseResult f10_b3_dirty_tracker_viewport_resize_reset() {
    kernetile::RendererConfig cfg{};
    cfg.width = 256; cfg.height = 256; cfg.force_cpu_fallback = true;
    auto ren = kernetile::KerneTileRenderer::create(cfg);
    ren->begin_frame();
    ren->draw_rect({0, 0, 256, 256}, {0.1f, 0.1f, 0.1f, 1.0f});
    ren->end_frame();

    ren->resize(320, 192);
    ren->begin_frame();
    ren->draw_rect({0, 0, 320, 192}, {0.1f, 0.1f, 0.1f, 1.0f});
    ren->end_frame();
    const auto& st = ren->get_last_frame_stats();
    KT_E2E_CHECK(st.total_tiles == 240u);
    KT_E2E_CHECK_MSG(st.dirty_tiles == st.total_tiles,
                     "Viewport resize must force full-frame tile invalidation");
    return passed_case();
}

template <typename Ren = kernetile::KerneTileRenderer>
CaseResult f10_b4_all_off_vs_single_feature_on_isolation() {
    if constexpr (!HasRendererDitheringApi<Ren> || !HasRendererPixelSnappingApi<Ren>) {
        return pending_case("R1/R2 renderer toggle APIs not landed yet");
    } else {
        kernetile::RendererConfig cfg{};
        cfg.force_cpu_fallback = true;
        auto ren = Ren::create(cfg);
        ren->set_dithering_enabled(true);
        KT_E2E_CHECK(ren->is_dithering_enabled() == true);
        KT_E2E_CHECK(ren->is_pixel_snapping_enabled() == false);
        ren->set_dithering_enabled(false);
        ren->set_pixel_snapping_enabled(true);
        KT_E2E_CHECK(ren->is_dithering_enabled() == false);
        KT_E2E_CHECK(ren->is_pixel_snapping_enabled() == true);
        return passed_case();
    }
}

template <typename Cfg = kernetile::RendererConfig, typename FlagsT = kernetile::core::PrimitiveFlags>
CaseResult f10_b5_shader_kt_vulkan_and_wgsl_guard_parity() {
    auto audit = audit_shader_files();
    KT_E2E_CHECK(audit.hlsl_found && audit.wgsl_found);
    KT_E2E_CHECK(audit.hlsl_has_vk_path);

    const std::string hlsl = read_workspace_file("shaders/ui_compute_d3d12.hlsl");
    const std::string wgsl = read_workspace_file("shaders/ui_compute.wgsl");
    KT_E2E_CHECK(hlsl.find("mica_grid") != std::string::npos);
    KT_E2E_CHECK(wgsl.find("mica_grid") != std::string::npos);
    KT_E2E_CHECK(wgsl.find("@binding(6)") != std::string::npos);
    KT_E2E_CHECK(wgsl.find("@binding(7)") != std::string::npos);

    if constexpr (!HasRendererDitheringConfig<Cfg> || !HasPointerRevealFlag<FlagsT>) {
        if (!audit.hlsl_has_ign_constants || !audit.hlsl_has_pointer_reveal ||
            !audit.wgsl_has_ign_constants || !audit.wgsl_has_pointer_reveal) {
            return pending_case("M1 shader R1/R3 parity updates not landed yet");
        }
    }
    KT_E2E_CHECK(audit.hlsl_has_ign_constants && audit.hlsl_has_pointer_reveal && audit.hlsl_has_render_flags);
    KT_E2E_CHECK(audit.wgsl_has_ign_constants && audit.wgsl_has_pointer_reveal && audit.wgsl_has_render_flags);
    return passed_case();
}

} // namespace

int main() {
    TestRunner runner("KerneTile & KTUI E2E Tier 2 Boundaries (F1-F10)");

    // F1: IGN Dithering Boundaries (5)
    runner.run_case("F1_B1_saturation_clamp_0_and_255", f1_b1_saturation_clamp_0_and_255<>);
    runner.run_case("F1_B2_exact_integer_midtone_bounds", f1_b2_exact_integer_midtone_bounds<>);
    runner.run_case("F1_B3_tiny_1x1_and_odd_non64_sizes", f1_b3_tiny_1x1_and_odd_non64_sizes<>);
    runner.run_case("F1_B4_rapid_toggle_on_off_on", f1_b4_rapid_toggle_on_off_on<>);
    runner.run_case("F1_B5_zero_alpha_degenerate_primitive", f1_b5_zero_alpha_degenerate_primitive<>);

    // F2: Banding Elimination Boundaries (5)
    runner.run_case("F2_B1_near_black_mica", f2_b1_near_black_mica<>);
    runner.run_case("F2_B2_zero_blur_shadow", f2_b2_zero_blur_shadow<>);
    runner.run_case("F2_B3_extreme_blur_64_shadow", f2_b3_extreme_blur_64_shadow<>);
    runner.run_case("F2_B4_flat_uniform_color_variance", f2_b4_flat_uniform_color_variance<>);
    runner.run_case("F2_B5_degenerate_zero_area_primitive", f2_b5_degenerate_zero_area_primitive<>);

    // F3: Fractional DPI Pixel Snapping Boundaries (5)
    runner.run_case("F3_B1_scale_matrix_100_125_150_200", f3_b1_scale_matrix_100_125_150_200<>);
    runner.run_case("F3_B2_zero_border_width_stays_zero", f3_b2_zero_border_width_stays_zero<>);
    runner.run_case("F3_B3_subpixel_025_border_clamps_to_1px", f3_b3_subpixel_025_border_clamps_to_1px<>);
    runner.run_case("F3_B4_diagonal_line_unaffected", f3_b4_diagonal_line_unaffected<>);
    runner.run_case("F3_B5_zero_area_and_1x1_box", f3_b5_zero_area_and_1x1_box<>);

    // F4: Pointer Reveal Engine Boundaries (5)
    runner.run_case("F4_B1_cursor_on_tile_boundary", f4_b1_cursor_on_tile_boundary<>);
    runner.run_case("F4_B2_cursor_outside_widget_bounds", f4_b2_cursor_outside_widget_bounds<>);
    runner.run_case("F4_B3_zero_radius_reveal", f4_b3_zero_radius_reveal<>);
    runner.run_case("F4_B4_rapid_sweep_4_corners", f4_b4_rapid_sweep_4_corners<>);
    runner.run_case("F4_B5_clipped_by_rounded_clip_rect", f4_b5_clipped_by_rounded_clip_rect<>);

    // F5: Pointer Reveal Widgets Boundaries (5)
    runner.run_case("F5_B1_enter_move_exit_clears_reveal", f5_b1_enter_move_exit_clears_reveal);
    runner.run_case("F5_B2_disabled_widget_no_reveal", f5_b2_disabled_widget_no_reveal);
    runner.run_case("F5_B3_pointer_outside_widget_bounds", f5_b3_pointer_outside_widget_bounds);
    runner.run_case("F5_B4_consecutive_moves_same_tile", f5_b4_consecutive_moves_same_tile);
    runner.run_case("F5_B5_tiny_button_smaller_than_radius", f5_b5_tiny_button_smaller_than_radius);

    // F6: Rubber-Band Overscroll Boundaries (5)
    runner.run_case("F6_B1_zero_displacement_exact_zero", f6_b1_zero_displacement_exact_zero);
    runner.run_case("F6_B2_tiny_and_huge_displacement_asymptote", f6_b2_tiny_and_huge_displacement_asymptote);
    runner.run_case("F6_B3_wheel_scroll_vs_touch_drag", f6_b3_wheel_scroll_vs_touch_drag);
    runner.run_case("F6_B4_max_scroll_zero_non_scrollable", f6_b4_max_scroll_zero_non_scrollable);
    runner.run_case("F6_B5_velocity_reversal_mid_rebound", f6_b5_velocity_reversal_mid_rebound);

    // F7: Tabular Figures Text Engine Boundaries (5)
    runner.run_case("F7_B1_empty_string", f7_b1_empty_string<>);
    runner.run_case("F7_B2_single_char_1_vs_8", f7_b2_single_char_1_vs_8<>);
    runner.run_case("F7_B3_mixed_alphanumeric_fps", f7_b3_mixed_alphanumeric_fps<>);
    runner.run_case("F7_B4_punctuation_and_decimals", f7_b4_punctuation_and_decimals<>);
    runner.run_case("F7_B5_cache_key_matrix_weight_size_tabular", f7_b5_cache_key_matrix_weight_size_tabular<>);

    // F8: Tabular Figures KTUI Text Boundaries (5)
    runner.run_case("F8_B1_empty_text_widget", f8_b1_empty_text_widget<>);
    runner.run_case("F8_B2_multiline_wrapped_numeric", f8_b2_multiline_wrapped_numeric<>);
    runner.run_case("F8_B3_cursor_x_zero_and_end", f8_b3_cursor_x_zero_and_end<>);
    runner.run_case("F8_B4_hit_test_cursor_clamping", f8_b4_hit_test_cursor_clamping<>);
    runner.run_case("F8_B5_text_measure_at_125_and_150_scale", f8_b5_text_measure_at_125_and_150_scale<>);

    // F9: ktui_gallery Boundary & Config Pre-Checks (5)
    runner.run_case("F9_B1_gallery_showcase_routes_declared", f9_b1_gallery_showcase_routes_declared);
    runner.run_case("F9_B2_gallery_theme_toggle_constants", f9_b2_gallery_theme_toggle_constants);
    runner.run_case("F9_B3_gallery_dpi_and_resize_support", f9_b3_gallery_dpi_and_resize_support);
    runner.run_case("F9_B4_gallery_automation_timeout_and_quit", f9_b4_gallery_automation_timeout_and_quit);
    runner.run_case("F9_B5_gallery_no_debug_only_leakage", f9_b5_gallery_no_debug_only_leakage);

    // F10: Footprint, Parity & Default Preservation Boundaries (5)
    runner.run_case("F10_B1_cpu_vs_d3d12_warp_odd_size_130x66", f10_b1_cpu_vs_d3d12_warp_odd_size_130x66);
    runner.run_case("F10_B2_zero_primitive_empty_frame_parity", f10_b2_zero_primitive_empty_frame_parity);
    runner.run_case("F10_B3_dirty_tracker_viewport_resize_reset", f10_b3_dirty_tracker_viewport_resize_reset);
    runner.run_case("F10_B4_all_off_vs_single_feature_on_isolation", f10_b4_all_off_vs_single_feature_on_isolation<>);
    runner.run_case("F10_B5_shader_kt_vulkan_and_wgsl_guard_parity", f10_b5_shader_kt_vulkan_and_wgsl_guard_parity<>);

    KT_E2E_CHECK(runner.total() == 50u);
    return runner.finalize();
}
