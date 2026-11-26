#include "test_common.hpp"
#include "kernetile/kernetile.hpp"
#include "kernetile/text/font_engine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cassert>
#include <string_view>
#include <vector>

#define KT_CHECK(cond)                                                                         \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::cerr << "FAIL at " << __FILE__ << ":" << __LINE__ << ": " #cond << std::endl; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (false)

using namespace kernetile;
using namespace kernetile::math;

void test_renderer_api_lifecycle() {
    std::cout << "[Test 1] Testing KerneTileRenderer Lifecycle & Draw APIs..." << std::endl;

    RendererConfig config{};
    config.width = 800;
    config.height = 600;
    config.force_cpu_fallback = true; // 确保在 CI 和无显卡环境下均可 100% 运行

    auto renderer = KerneTileRenderer::create(config);
    assert(renderer != nullptr);

    // 1. 开始第一帧
    renderer->begin_frame();

    // 绘制背景矩形
    renderer->draw_rect(Rect{0.0f, 0.0f, 800.0f, 600.0f}, Color(0.1f, 0.15f, 0.2f, 1.0f));

    // 绘制带软阴影的圆角卡片
    renderer->draw_shadowed_box(
        Rect{100.0f, 100.0f, 400.0f, 300.0f},
        Color(0.2f, 0.6f, 0.9f, 1.0f),
        16.0f, // 圆角
        8.0f,  // 阴影模糊
        2.0f,  // 阴影外扩
        Vec2{0.0f, 4.0f} // 阴影偏移
    );

    // 绘制文本
    renderer->draw_text("KerneTile 2D Engine", 120.0f, 140.0f, 24.0f, Color::white());

    // 结束第一帧
    renderer->end_frame();

    const auto& stats1 = renderer->get_last_frame_stats();
    std::cout << "  Frame 1: Total Prims=" << stats1.total_primitives 
              << ", Dirty Tiles=" << stats1.dirty_tiles 
              << ", Binning Time=" << stats1.cpu_binning_time_us << " us" << std::endl;

    assert(stats1.total_primitives >= 3);
    assert(stats1.dirty_tiles > 0);

    // 2. 离屏渲染输出至内存
    std::vector<uint8_t> buffer(800 * 600 * 4, 0);
    renderer->render_to_rgba8(buffer.data());

    // 校验中心区域像素非空
    size_t center_idx = (200 * 800 + 250) * 4;
    assert(buffer[center_idx + 3] == 255); // Alpha 完全不透明

    // 3. 测试窗口动态尺寸调整 (Resize)
    renderer->resize(1920, 1080);
    renderer->begin_frame();
    renderer->draw_rect(Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, Color::black());
    renderer->end_frame();

    const auto& stats2 = renderer->get_last_frame_stats();
    assert(stats2.total_tiles == (1920 / 16) * (1080 / 16 + 1)); // 120 * 68 = 8,160

    std::cout << "  -> KerneTileRenderer Lifecycle PASSED." << std::endl;
}

void test_m1_2_opt_in_features() {
    std::cout << "[Test 2] Testing M1.2 Opt-In Defaults, Toggle Invalidation, R2 Snapping, R1/R3/R5 APIs..." << std::endl;

    RendererConfig default_cfg{};
    KT_CHECK(default_cfg.enable_dithering == false);
    KT_CHECK(default_cfg.enable_pixel_snapping == false);
    default_cfg.width = 256;
    default_cfg.height = 256;
    default_cfg.force_cpu_fallback = true;

    auto renderer = KerneTileRenderer::create(default_cfg);
    KT_CHECK(renderer != nullptr);
    KT_CHECK(renderer->is_dithering_enabled() == false);
    KT_CHECK(renderer->is_pixel_snapping_enabled() == false);

    // Static frame then settle to 0 dirty tiles
    auto draw_static = [&]() {
        renderer->begin_frame();
        renderer->draw_rect(Rect{0.0f, 0.0f, 256.0f, 256.0f}, Color(0.1f, 0.1f, 0.12f, 1.0f));
        renderer->end_frame();
    };
    draw_static();
    draw_static();
    KT_CHECK(renderer->get_last_frame_stats().dirty_tiles == 0);

    // Toggling dithering must invalidate dirty tracker
    renderer->set_dithering_enabled(true);
    KT_CHECK(renderer->is_dithering_enabled() == true);
    draw_static();
    KT_CHECK(renderer->get_last_frame_stats().dirty_tiles == renderer->get_last_frame_stats().total_tiles);

    // Toggling pixel snapping must invalidate dirty tracker
    draw_static();
    KT_CHECK(renderer->get_last_frame_stats().dirty_tiles == 0);
    renderer->set_pixel_snapping_enabled(true);
    KT_CHECK(renderer->is_pixel_snapping_enabled() == true);

    std::vector<uint8_t> fb(256 * 256 * 4, 0);
    renderer->begin_frame();
    // Hairline divider 1.25px tall at y = [50.4, 51.65] -> snaps to [51.0, 52.0] (pixel row y=51)
    renderer->draw_rect(Rect{10.0f, 50.4f, 110.0f, 51.65f}, Color::white(), 0.0f, 0.0f);
    // Bordered box with border_width = 1.25 -> snaps to 1px crisp border on integer bounds
    renderer->draw_rect(Rect{20.3f, 80.3f, 80.3f, 120.3f}, Color::white(), 0.0f, 1.25f);
    // Gradient + PointerReveal + tabular_nums draw_text_scaled
    renderer->draw_linear_gradient(Rect{120.0f, 10.0f, 240.0f, 60.0f},
                                   Color(0.0f, 0.2f, 0.4f, 1.0f),
                                   Color(0.2f, 0.6f, 0.8f, 1.0f),
                                   Vec2{120.0f, 10.0f}, Vec2{240.0f, 10.0f});
    renderer->draw_pointer_reveal(Rect{120.0f, 80.0f, 240.0f, 140.0f},
                                  Vec2{180.0f, 82.0f}, 32.0f,
                                  Color(1.0f, 1.0f, 1.0f, 0.5f), 6.0f, 1.0f, 1.0f);
    renderer->draw_text_scaled("11111", 20.0f, 160.0f, 16.0f, Color::white(), 1.25f, 400, false, "", true);
    renderer->end_frame();
    KT_CHECK(renderer->get_last_frame_stats().dirty_tiles == renderer->get_last_frame_stats().total_tiles);
    renderer->render_to_rgba8(fb.data());

    // Verify snapped hairline at x=50: y=51 is full 255, y=50 and y=52 are 0 (zero fractional alpha bleed)
    KT_CHECK(fb[(51 * 256 + 50) * 4 + 3] == 255);
    KT_CHECK(fb[(50 * 256 + 50) * 4 + 3] == 0);
    KT_CHECK(fb[(52 * 256 + 50) * 4 + 3] == 0);

    // Verify snapped 1px bordered box top edge at x=50, y=80 is crisp 255 with adjacent rows 0
    KT_CHECK(fb[(80 * 256 + 50) * 4 + 3] == 255);
    KT_CHECK(fb[(79 * 256 + 50) * 4 + 3] == 0);
    KT_CHECK(fb[(81 * 256 + 50) * 4 + 3] == 0);

    // Verify gradient and pointer reveal produced non-zero pixels
    KT_CHECK(fb[(30 * 256 + 180) * 4 + 3] == 255);
    KT_CHECK(fb[(80 * 256 + 180) * 4 + 3] > 100);

    std::cout << "  -> M1.2 Opt-In APIs & R2 Snapping PASSED." << std::endl;
}

void test_pointer_reveal_4k_dirty_tile_isolation() {
    std::cout << "[Test 3] Testing R3 4K Full-Renderer Dirty-Tile Isolation (Enter -> Move -> Exit -> Settle)..." << std::endl;

    RendererConfig cfg{};
    cfg.width = 3840;
    cfg.height = 2160;
    cfg.force_cpu_fallback = true;

    auto renderer = KerneTileRenderer::create(cfg);
    KT_CHECK(renderer != nullptr);

    std::vector<uint8_t> fb(static_cast<size_t>(cfg.width) * cfg.height * 4, 0);
    const Rect card{160.0f, 160.0f, 640.0f, 240.0f}; // 480x80 card
    const Color card_color(0.2f, 0.25f, 0.3f, 1.0f);
    const Color reveal_color(1.0f, 1.0f, 1.0f, 0.5f);

    // Frame 0: Card only
    renderer->begin_frame();
    renderer->draw_rect(card, card_color, 8.0f, 0.0f);
    renderer->end_frame();
    renderer->render_to_rgba8(fb.data());

    // Frame 1: Pointer enter at (240, 200), R = 48 -> X=[192,288] (tx 12..17), Y=[160,240] (ty 10..14) -> 30 tiles
    renderer->begin_frame();
    renderer->draw_rect(card, card_color, 8.0f, 0.0f);
    renderer->draw_pointer_reveal(card, Vec2{240.0f, 200.0f}, 48.0f, reveal_color, 8.0f, 0.0f, 1.0f);
    renderer->end_frame();
    renderer->render_to_rgba8(fb.data());

    const auto& stats1 = renderer->get_last_frame_stats();
    KT_CHECK(stats1.total_tiles == 32400u);
    KT_CHECK(stats1.dirty_tiles == 30u);
    for (const auto& dt : renderer->get_last_dirty_tiles()) {
        KT_CHECK(dt.tx >= 12u && dt.tx <= 17u);
        KT_CHECK(dt.ty >= 10u && dt.ty <= 14u);
    }

    const size_t distant_idx = (static_cast<size_t>(200) * cfg.width + 600u) * 4;
    const std::array<uint8_t, 4> distant_before = {
        fb[distant_idx + 0], fb[distant_idx + 1], fb[distant_idx + 2], fb[distant_idx + 3]
    };

    // Frame 2: Pointer move to (280, 200), R = 48 -> union tx [12..20], ty [10..14] -> <= 45 tiles
    renderer->begin_frame();
    renderer->draw_rect(card, card_color, 8.0f, 0.0f);
    renderer->draw_pointer_reveal(card, Vec2{280.0f, 200.0f}, 48.0f, reveal_color, 8.0f, 0.0f, 1.0f);
    renderer->end_frame();
    renderer->render_to_rgba8(fb.data());

    const FrameStats stats2 = renderer->get_last_frame_stats();
    KT_CHECK(stats2.dirty_tiles > 0u && stats2.dirty_tiles <= 45u);
    KT_CHECK(stats2.culling_ratio >= 99.8);
    for (const auto& dt : renderer->get_last_dirty_tiles()) {
        KT_CHECK(dt.tx >= 12u && dt.tx <= 20u);
        KT_CHECK(dt.ty >= 10u && dt.ty <= 14u);
    }
    uint32_t bx = 0, by = 0, bw = 0, bh = 0;
    KT_CHECK(renderer->get_last_dirty_bounds(bx, by, bw, bh));
    KT_CHECK(bx == 192u && by == 160u && bw == 144u && bh == 80u);

    // Distant card pixel (600, 200) must be untouched
    KT_CHECK(fb[distant_idx + 0] == distant_before[0]);
    KT_CHECK(fb[distant_idx + 1] == distant_before[1]);
    KT_CHECK(fb[distant_idx + 2] == distant_before[2]);
    KT_CHECK(fb[distant_idx + 3] == distant_before[3]);

    // Frame 3: Cursor exit (card only) -> tx [14..20], ty [10..14] -> <= 35 tiles
    renderer->begin_frame();
    renderer->draw_rect(card, card_color, 8.0f, 0.0f);
    renderer->end_frame();
    renderer->render_to_rgba8(fb.data());

    const auto& stats3 = renderer->get_last_frame_stats();
    KT_CHECK(stats3.dirty_tiles > 0u && stats3.dirty_tiles <= 35u);
    for (const auto& dt : renderer->get_last_dirty_tiles()) {
        KT_CHECK(dt.tx >= 14u && dt.tx <= 20u);
        KT_CHECK(dt.ty >= 10u && dt.ty <= 14u);
    }

    // Frame 4: Static settle -> 0 dirty tiles
    renderer->begin_frame();
    renderer->draw_rect(card, card_color, 8.0f, 0.0f);
    renderer->end_frame();
    KT_CHECK(renderer->get_last_frame_stats().dirty_tiles == 0u);

    std::cout << "  -> R3 4K Dirty-Tile Isolation PASSED (move dirty_tiles=" << stats2.dirty_tiles
              << ", culling=" << stats2.culling_ratio << "%)." << std::endl;
}

void test_default_disabled_preservation_and_r2_r5() {
    std::cout << "[Test 4] Testing Default-Disabled Preservation (R1, R2, R3, R5) + 1.5x DPI & Skip Rules..." << std::endl;

    // 1. R1 default false + true -> false bit-exact restoration
    {
        const uint32_t w = 256, h = 64;
        RendererConfig cfg{};
        cfg.width = w;
        cfg.height = h;
        cfg.force_cpu_fallback = true;
        auto renderer = KerneTileRenderer::create(cfg);
        KT_CHECK(renderer != nullptr);
        KT_CHECK(renderer->is_dithering_enabled() == false);

        std::array<uint32_t, 16> mica_grid{};
        for (uint32_t gy = 0; gy < 4; ++gy) {
            for (uint32_t gx = 0; gx < 4; ++gx) {
                const uint32_t c = 10u + gx * 3u;
                mica_grid[gy * 4 + gx] = c | (c << 8) | (c << 16) | (255u << 24);
            }
        }
        renderer->set_mica_backdrop(true, mica_grid);

        auto render_r1_scene = [&](std::vector<uint8_t>& out) {
            out.assign(static_cast<size_t>(w) * h * 4, 0);
            renderer->begin_frame();
            renderer->draw_linear_gradient(
                Rect{0.0f, 0.0f, 256.0f, 32.0f},
                Color(0.0f, 0.0f, 0.0f, 1.0f),
                Color(16.0f / 255.0f, 16.0f / 255.0f, 16.0f / 255.0f, 1.0f),
                Vec2{0.0f, 0.0f}, Vec2{256.0f, 0.0f});
            renderer->draw_rect(Rect{0.0f, 32.0f, 128.0f, 64.0f}, Color(0.150f, 0.0f, 1.0f, 1.0f));
            renderer->end_frame();
            renderer->render_to_rgba8(out.data());
        };

        std::vector<uint8_t> fb_default, fb_on, fb_restored;
        render_r1_scene(fb_default);

        // Verify flat quantization on gradient row y=16 and solid fractional R==38 at (64, 48)
        uint32_t best_run = 1, cur_run = 1;
        for (uint32_t x = 1; x < w; ++x) {
            if (fb_default[(16u * w + x) * 4] == fb_default[(16u * w + x - 1u) * 4]) {
                best_run = std::max(best_run, ++cur_run);
            } else {
                cur_run = 1;
            }
        }
        KT_CHECK(best_run >= 12u);
        KT_CHECK(fb_default[(48u * w + 64u) * 4 + 0] == 38u);

        renderer->set_dithering_enabled(true);
        render_r1_scene(fb_on);
        KT_CHECK(fb_on != fb_default);

        renderer->set_dithering_enabled(false);
        render_r1_scene(fb_restored);
        KT_CHECK(fb_restored == fb_default);
    }

    // 2. R2 default false fractional bleed + true -> false restoration + 1.5x DPI + skip rules
    {
        const uint32_t w = 256, h = 256;
        RendererConfig cfg{};
        cfg.width = w;
        cfg.height = h;
        cfg.force_cpu_fallback = true;
        auto renderer = KerneTileRenderer::create(cfg);
        KT_CHECK(renderer != nullptr);
        KT_CHECK(renderer->is_pixel_snapping_enabled() == false);

        auto render_r2_bleed_scene = [&](std::vector<uint8_t>& out) {
            out.assign(static_cast<size_t>(w) * h * 4, 0);
            renderer->begin_frame();
            renderer->draw_rect(Rect{10.0f, 50.4f, 110.0f, 51.65f}, Color::white(), 0.0f, 0.0f);
            renderer->draw_rect(Rect{20.3f, 80.3f, 80.3f, 120.3f}, Color::white(), 0.0f, 1.25f);
            renderer->end_frame();
            renderer->render_to_rgba8(out.data());
        };

        std::vector<uint8_t> fb_default, fb_snapped, fb_restored;
        render_r2_bleed_scene(fb_default);

        const uint8_t bleed_line_a = fb_default[(50u * w + 50u) * 4 + 3];
        const uint8_t bleed_box_a = fb_default[(81u * w + 50u) * 4 + 3];
        KT_CHECK(bleed_line_a > 0u && bleed_line_a < 255u);
        KT_CHECK(bleed_box_a > 0u && bleed_box_a < 255u);

        renderer->set_pixel_snapping_enabled(true);
        render_r2_bleed_scene(fb_snapped);
        KT_CHECK(fb_snapped[(51u * w + 50u) * 4 + 3] == 255u);
        KT_CHECK(fb_snapped[(50u * w + 50u) * 4 + 3] == 0u);

        renderer->set_pixel_snapping_enabled(false);
        render_r2_bleed_scene(fb_restored);
        KT_CHECK(fb_restored == fb_default);

        // 1.5x DPI snapping: 1.5px hairline, 1.5px border, and 1.5px rounded border
        renderer->set_pixel_snapping_enabled(true);
        std::vector<uint8_t> fb_150(static_cast<size_t>(w) * h * 4, 0);
        renderer->begin_frame();
        renderer->draw_rect(Rect{10.0f, 50.2f, 110.0f, 51.7f}, Color::white(), 0.0f, 0.0f);
        renderer->draw_rect(Rect{20.2f, 80.2f, 80.2f, 120.2f}, Color::white(), 0.0f, 1.5f);
        renderer->draw_rect(Rect{100.2f, 80.2f, 160.2f, 120.2f}, Color::white(), 6.0f, 1.5f);
        renderer->end_frame();
        renderer->render_to_rgba8(fb_150.data());

        // 1.5px hairline snaps to 2px [50, 52]
        KT_CHECK(fb_150[(50u * w + 50u) * 4 + 3] == 255u);
        KT_CHECK(fb_150[(51u * w + 50u) * 4 + 3] == 255u);
        KT_CHECK(fb_150[(49u * w + 50u) * 4 + 3] == 0u);
        KT_CHECK(fb_150[(52u * w + 50u) * 4 + 3] == 0u);
        // 1.5px box border snaps to 2px [80, 82]
        KT_CHECK(fb_150[(80u * w + 50u) * 4 + 3] == 255u);
        KT_CHECK(fb_150[(81u * w + 50u) * 4 + 3] == 255u);
        KT_CHECK(fb_150[(79u * w + 50u) * 4 + 3] == 0u);
        KT_CHECK(fb_150[(82u * w + 50u) * 4 + 3] == 0u);
        // Rounded 1.5px border preserves rounded corner at (100, 80) while straight edge is 2px crisp
        KT_CHECK(fb_150[(80u * w + 100u) * 4 + 3] == 0u);
        KT_CHECK(fb_150[(80u * w + 130u) * 4 + 3] == 255u);
        KT_CHECK(fb_150[(81u * w + 130u) * 4 + 3] == 255u);
        KT_CHECK(fb_150[(79u * w + 130u) * 4 + 3] == 0u);
        KT_CHECK(fb_150[(82u * w + 130u) * 4 + 3] == 0u);

        // Skip rules: IsGlyph, IsIcon, IsPointerReveal, Gaussian shadow, circular dot remain identical
        std::vector<uint8_t> rgba_icon(16u * 16u * 4u, 255u);
        auto render_skip_scene = [&](bool snap, std::vector<uint8_t>& out) {
            RendererConfig scfg{};
            scfg.width = w;
            scfg.height = h;
            scfg.force_cpu_fallback = true;
            scfg.enable_pixel_snapping = snap;
            auto sr = KerneTileRenderer::create(scfg);
            KT_CHECK(sr != nullptr);
            out.assign(static_cast<size_t>(w) * h * 4, 0);
            sr->begin_frame();
            sr->draw_text_scaled("123", 10.3f, 20.3f, 16.0f, Color::white(), 1.0f);
            sr->draw_image(9001u, Rect{10.3f, 50.3f, 26.3f, 66.3f}, 16u, 16u, rgba_icon.data(), 0.0f, Color::white());
            sr->draw_pointer_reveal(Rect{40.3f, 50.3f, 120.3f, 90.3f}, Vec2{80.3f, 70.3f}, 24.0f,
                                    Color(1.0f, 1.0f, 1.0f, 0.5f), 4.0f, 0.0f, 1.0f);
            sr->draw_shadowed_box(Rect{140.3f, 20.3f, 200.3f, 80.3f}, Color(0.2f, 0.3f, 0.4f, 1.0f),
                                  8.0f, 12.0f, 0.0f, Vec2{0.0f, 2.0f});
            sr->draw_rect(Rect{40.3f, 140.3f, 60.3f, 160.3f}, Color::white(), 10.0f, 0.0f);
            sr->end_frame();
            sr->render_to_rgba8(out.data());
        };
        std::vector<uint8_t> fb_skip_off, fb_skip_on;
        render_skip_scene(false, fb_skip_off);
        render_skip_scene(true, fb_skip_on);
        KT_CHECK(fb_skip_off == fb_skip_on);
    }

    // 3. R3 default no-reveal + R=0 / alpha=0 no-op
    {
        const uint32_t w = 256, h = 256;
        RendererConfig cfg{};
        cfg.width = w;
        cfg.height = h;
        cfg.force_cpu_fallback = true;
        auto renderer = KerneTileRenderer::create(cfg);
        KT_CHECK(renderer != nullptr);

        const Rect card{40.0f, 40.0f, 200.0f, 120.0f};
        std::vector<uint8_t> fb_base(static_cast<size_t>(w) * h * 4, 0);
        renderer->begin_frame();
        renderer->draw_rect(card, Color(0.2f, 0.2f, 0.25f, 1.0f), 8.0f, 0.0f);
        renderer->end_frame();
        renderer->render_to_rgba8(fb_base.data());

        // Settle
        renderer->begin_frame();
        renderer->draw_rect(card, Color(0.2f, 0.2f, 0.25f, 1.0f), 8.0f, 0.0f);
        renderer->end_frame();
        KT_CHECK(renderer->get_last_frame_stats().dirty_tiles == 0u);

        // R = 0 no-op
        renderer->begin_frame();
        renderer->draw_rect(card, Color(0.2f, 0.2f, 0.25f, 1.0f), 8.0f, 0.0f);
        renderer->draw_pointer_reveal(card, Vec2{120.0f, 80.0f}, 0.0f, Color(1.0f, 1.0f, 1.0f, 0.5f), 8.0f, 0.0f, 1.0f);
        renderer->end_frame();
        KT_CHECK(renderer->get_last_frame_stats().dirty_tiles == 0u);

        // alpha = 0 no-op
        renderer->begin_frame();
        renderer->draw_rect(card, Color(0.2f, 0.2f, 0.25f, 1.0f), 8.0f, 0.0f);
        renderer->draw_pointer_reveal(card, Vec2{120.0f, 80.0f}, 48.0f, Color(1.0f, 1.0f, 1.0f, 0.0f), 8.0f, 0.0f, 1.0f);
        renderer->end_frame();
        KT_CHECK(renderer->get_last_frame_stats().dirty_tiles == 0u);
    }

    // 4. R5 draw_text_scaled default tabular_nums = false vs omitted vs true
#ifdef _WIN32
    {
        auto& text_engine = kernetile::text::shared_text_engine();
        static constexpr std::string_view kCandidates[] = {
            "Candara",
            "Times New Roman",
            "Constantia",
            "Corbel",
            "Bahnschrift",
            "Segoe UI Variable Text"
        };
        std::string_view prop_font{};
        for (const auto fam : kCandidates) {
            const auto p1 = text_engine.shape_text("11111", 20.0f, fam, 400, false, false);
            const auto p8 = text_engine.shape_text("88888", 20.0f, fam, 400, false, false);
            const auto t1 = text_engine.shape_text("11111", 20.0f, fam, 400, false, true);
            const auto t8 = text_engine.shape_text("88888", 20.0f, fam, 400, false, true);
            if (p1.glyphs.size() == 5 && p8.glyphs.size() == 5 &&
                p1.total_width + 0.5f < p8.total_width &&
                std::fabs(t1.total_width - t8.total_width) <= 1e-3f) {
                prop_font = fam;
                break;
            }
        }
        KT_CHECK(!prop_font.empty());

        const uint32_t w = 256, h = 64;
        auto render_str = [&](std::string_view str, int mode) {
            RendererConfig cfg{};
            cfg.width = w;
            cfg.height = h;
            cfg.force_cpu_fallback = true;
            auto r = KerneTileRenderer::create(cfg);
            KT_CHECK(r != nullptr);
            std::vector<uint8_t> out(static_cast<size_t>(w) * h * 4, 0);
            r->begin_frame();
            if (mode < 0) {
                r->draw_text_scaled(str, 10.0f, 16.0f, 20.0f, Color::white(), 1.25f, 400, false, prop_font);
            } else {
                r->draw_text_scaled(str, 10.0f, 16.0f, 20.0f, Color::white(), 1.25f, 400, false, prop_font, mode > 0);
            }
            r->end_frame();
            r->render_to_rgba8(out.data());
            return out;
        };

        auto max_x_of = [&](const std::vector<uint8_t>& fb) {
            uint32_t mx = 0;
            for (uint32_t y = 0; y < h; ++y) {
                for (uint32_t x = 0; x < w; ++x) {
                    if (fb[(static_cast<size_t>(y) * w + x) * 4 + 3] > 0u) {
                        mx = std::max(mx, x);
                    }
                }
            }
            KT_CHECK(mx > 0u);
            return mx;
        };

        const auto fb_1_omit  = render_str("11111%", -1);
        const auto fb_1_false = render_str("11111%", 0);
        const auto fb_8_false = render_str("88888%", 0);
        const auto fb_1_true  = render_str("11111%", 1);
        const auto fb_8_true  = render_str("88888%", 1);

        KT_CHECK(fb_1_omit == fb_1_false);
        const uint32_t max_1_false = max_x_of(fb_1_false);
        const uint32_t max_8_false = max_x_of(fb_8_false);
        const uint32_t max_1_true  = max_x_of(fb_1_true);
        const uint32_t max_8_true  = max_x_of(fb_8_true);

        KT_CHECK(max_1_false < max_8_false);
        KT_CHECK(std::abs(static_cast<int>(max_1_true) - static_cast<int>(max_8_true)) <= 1);
    }
#endif

    std::cout << "  -> Default-Disabled Preservation (R1, R2, R3, R5) + 1.5x DPI & Skip Rules PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 10 Renderer Lifecycle Tests ===" << std::endl;

    test_renderer_api_lifecycle();
    test_m1_2_opt_in_features();
    test_pointer_reveal_4k_dirty_tile_isolation();
    test_default_disabled_preservation_and_r2_r5();

    std::cout << "=== All Renderer Lifecycle Tests PASSED (100%) ===" << std::endl;
    return 0;
}
