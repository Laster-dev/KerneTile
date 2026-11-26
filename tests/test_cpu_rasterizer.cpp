#include "test_common.hpp"
#include "kernetile/cpu/cpu_rasterizer.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cassert>
#include <vector>

#define KT_CHECK(cond)                                                                         \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::cerr << "FAIL at " << __FILE__ << ":" << __LINE__ << ": " #cond << std::endl; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (false)

using namespace kernetile::cpu;
using namespace kernetile::core;
using namespace kernetile::tile;

void test_adaptive_concurrency_dispatcher() {
    std::cout << "[Test 1] Testing Adaptive Concurrency Dispatcher Thread Counts..." << std::endl;

    const uint32_t host_cores = 16;

    // 1. 极小局部重绘 (<= 8 瓦片，如光标闪烁): 必须返回 1 线程，避免线程上下文切换开销
    assert(CpuTileRasterizer::calculate_worker_threads(1, host_cores) == 1);
    assert(CpuTileRasterizer::calculate_worker_threads(4, host_cores) == 1);
    assert(CpuTileRasterizer::calculate_worker_threads(8, host_cores) == 1);

    // 2. 中小交互 (<= 64 瓦片，如按钮悬停/下拉菜单): 启动 2-4 线程
    uint32_t t_medium = CpuTileRasterizer::calculate_worker_threads(32, host_cores);
    assert(t_medium >= 2 && t_medium <= 4);

    // 3. 大面积/全屏重绘 (> 64 瓦片): 启动最大 8 线程
    uint32_t t_large = CpuTileRasterizer::calculate_worker_threads(500, host_cores);
    assert(t_large >= 4 && t_large <= 8);

    std::cout << "  -> Adaptive concurrency dispatcher PASSED." << std::endl;
}

void test_cpu_tile_rasterization() {
    std::cout << "[Test 2] Testing Pure CPU Tile Rasterization Output..." << std::endl;

    const uint32_t width = 64;
    const uint32_t height = 64;
    std::vector<uint8_t> framebuffer(width * height * 4, 0);

    // 构造一个翡翠绿圆角矩形图元 [16, 16, 48, 48], 圆角 8.0, 纯翡翠绿 RGBA=(0.0, 0.8, 0.4, 1.0)
    UiPrimitive prim{};
    prim.min_x = 16.0f;
    prim.min_y = 16.0f;
    prim.max_x = 48.0f;
    prim.max_y = 48.0f;
    prim.radius_top_left = 8.0f;
    prim.radius_top_right = 8.0f;
    prim.radius_bottom_right = 8.0f;
    prim.radius_bottom_left = 8.0f;
    prim.color_r = 0.0f;
    prim.color_g = 0.8f;
    prim.color_b = 0.4f;
    prim.color_a = 1.0f;

    std::vector<UiPrimitive> primitives = {prim};

    TileGrid grid(width, height);
    TileBinner binner;
    BinningResult bin_res = binner.bin_primitives(grid, primitives);

    // 提取脏瓦片
    std::vector<uint32_t> dirty_tiles;
    for (uint32_t i = 0; i < grid.get_total_tiles(); ++i) {
        if (bin_res.headers[i].primitive_count > 0) {
            dirty_tiles.push_back(i);
        }
    }
    assert(!dirty_tiles.empty());

    CpuTileRasterizer::RenderParams params{};
    params.width = width;
    params.height = height;
    params.primitives = primitives;
    params.tile_indices = bin_res.indices;
    params.tile_headers = bin_res.headers;
    params.dirty_tiles = dirty_tiles;
    params.thread_count = 1;

    CpuTileRasterizer::render(params, framebuffer.data());

    // 校验中心点像素 (32, 32)：必须为纯翡翠绿 (R=0, G=204, B=102, A=255)
    size_t center_idx = (32 * width + 32) * 4;
    std::cout << "  Center pixel RGBA: ("
              << static_cast<int>(framebuffer[center_idx + 0]) << ", "
              << static_cast<int>(framebuffer[center_idx + 1]) << ", "
              << static_cast<int>(framebuffer[center_idx + 2]) << ", "
              << static_cast<int>(framebuffer[center_idx + 3]) << ")" << std::endl;

    assert(framebuffer[center_idx + 0] == 0);
    assert(framebuffer[center_idx + 1] == 204);
    assert(framebuffer[center_idx + 2] == 102);
    assert(framebuffer[center_idx + 3] == 255);

    // 校验外部像素 (4, 4)：必须完全透明 (0, 0, 0, 0)
    size_t out_idx = (4 * width + 4) * 4;
    assert(framebuffer[out_idx + 3] == 0);

    std::cout << "  -> Pure CPU tile rasterization PASSED." << std::endl;
}

static uint16_t pack_unorm16(float v) {
    return static_cast<uint16_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 65535.0f));
}

void test_ign_dithering_and_linear_gradient() {
    std::cout << "[Test 3] Testing R1 IGN Dithering (Gradient, Mica, Gaussian Shadow)..." << std::endl;

    const uint32_t width = 256;
    const uint32_t height = 64;
    TileGrid grid(width, height);
    TileBinner binner;

    std::vector<uint32_t> all_tiles(grid.get_total_tiles());
    for (uint32_t i = 0; i < grid.get_total_tiles(); ++i) all_tiles[i] = i;

    // 1. Shallow linear gradient ramp [0, 0, 256, 32] from 0 to 16/255 (16 px per 1 LSB step)
    UiPrimitive grad{};
    grad.min_x = 0.0f; grad.min_y = 0.0f;
    grad.max_x = 256.0f; grad.max_y = 32.0f;
    grad.color_r = 0.0f; grad.color_g = 0.0f; grad.color_b = 0.0f; grad.color_a = 1.0f;
    const float end_val = 16.0f / 255.0f;
    grad.atlas_u0 = pack_unorm16(end_val);
    grad.atlas_v0 = pack_unorm16(end_val);
    grad.atlas_u1 = pack_unorm16(end_val);
    grad.atlas_v1 = pack_unorm16(1.0f);
    grad.shadow_offset_x = 0.0f; grad.shadow_offset_y = 0.0f;
    grad.shadow_blur = 256.0f;   grad.shadow_spread = 0.0f;
    grad.flags = static_cast<uint16_t>(PrimitiveFlags::HasLinearGradient);

    std::vector<UiPrimitive> prims = {grad};
    auto bin_res = binner.bin_primitives(grid, prims);

    std::vector<uint8_t> fb_off(width * height * 4, 0);
    std::vector<uint8_t> fb_on(width * height * 4, 0);

    CpuTileRasterizer::RenderParams params{};
    params.width = width;
    params.height = height;
    params.primitives = prims;
    params.tile_indices = bin_res.indices;
    params.tile_headers = bin_res.headers;
    params.dirty_tiles = all_tiles;
    params.thread_count = 1;
    params.enable_dithering = false;
    CpuTileRasterizer::render(params, fb_off.data());

    params.enable_dithering = true;
    CpuTileRasterizer::render(params, fb_on.data());

    auto max_flat_run = [&](const std::vector<uint8_t>& fb, uint32_t y) {
        uint32_t best = 1, cur = 1;
        for (uint32_t x = 1; x < width; ++x) {
            uint8_t prev = fb[(y * width + x - 1) * 4];
            uint8_t curr = fb[(y * width + x) * 4];
            if (curr == prev) {
                cur++;
                best = std::max(best, cur);
            } else {
                cur = 1;
            }
        }
        return best;
    };

    const uint32_t run_off = max_flat_run(fb_off, 16);
    const uint32_t run_on = max_flat_run(fb_on, 16);
    KT_CHECK(run_off >= 12);
    KT_CHECK(run_on < run_off);

    int max_err = 0;
    for (uint32_t x = 0; x < width; ++x) {
        const size_t idx = (16 * width + x) * 4;
        KT_CHECK(fb_on[idx + 3] == 255); // Alpha must never be dithered
        const float ideal = ((static_cast<float>(x) + 0.5f) / 256.0f) * 16.0f;
        const int err = static_cast<int>(std::ceil(std::abs(static_cast<float>(fb_on[idx]) - ideal)));
        max_err = std::max(max_err, err);
    }
    KT_CHECK(max_err <= 1);

    // 2. Dark-mode Mica ramp (empty primitives -> fill_mica_to_fb)
    std::vector<UiPrimitive> empty_prims;
    auto bin_empty = binner.bin_primitives(grid, empty_prims);
    std::array<uint32_t, 16> mica_grid{};
    for (uint32_t gy = 0; gy < 4; ++gy) {
        for (uint32_t gx = 0; gx < 4; ++gx) {
            uint32_t c = 10u + gx * 3u;
            mica_grid[gy * 4 + gx] = c | (c << 8) | (c << 16) | (255u << 24);
        }
    }
    params.primitives = empty_prims;
    params.tile_indices = bin_empty.indices;
    params.tile_headers = bin_empty.headers;
    params.mica_enabled = true;
    params.mica_grid = mica_grid;

    params.enable_dithering = false;
    CpuTileRasterizer::render(params, fb_off.data());
    params.enable_dithering = true;
    CpuTileRasterizer::render(params, fb_on.data());

    KT_CHECK(max_flat_run(fb_off, 10) >= 16);
    KT_CHECK(max_flat_run(fb_on, 10) < max_flat_run(fb_off, 10));
    for (uint32_t x = 0; x < width; ++x) {
        KT_CHECK(fb_on[(10 * width + x) * 4 + 3] == 255);
    }

    // 3. Large-radius analytical Gaussian shadow over dark background
    UiPrimitive bg{};
    bg.min_x = 0.0f; bg.min_y = 0.0f; bg.max_x = 256.0f; bg.max_y = 64.0f;
    bg.color_r = 0.15f; bg.color_g = 0.15f; bg.color_b = 0.15f; bg.color_a = 1.0f;

    UiPrimitive shadow_box{};
    shadow_box.min_x = 16.0f; shadow_box.min_y = 16.0f;
    shadow_box.max_x = 80.0f; shadow_box.max_y = 48.0f;
    shadow_box.color_r = 0.0f; shadow_box.color_g = 0.0f; shadow_box.color_b = 0.0f; shadow_box.color_a = 0.4f;
    shadow_box.shadow_blur = 24.0f;

    std::vector<UiPrimitive> shadow_prims = {bg, shadow_box};
    auto bin_shadow = binner.bin_primitives(grid, shadow_prims);
    params.mica_enabled = false;
    params.primitives = shadow_prims;
    params.tile_indices = bin_shadow.indices;
    params.tile_headers = bin_shadow.headers;

    params.enable_dithering = false;
    CpuTileRasterizer::render(params, fb_off.data());
    params.enable_dithering = true;
    CpuTileRasterizer::render(params, fb_on.data());

    uint32_t diff_pixels = 0;
    for (uint32_t x = 80; x < 150; ++x) {
        const size_t idx = (32 * width + x) * 4;
        if (fb_on[idx] != fb_off[idx]) ++diff_pixels;
        KT_CHECK(std::abs(static_cast<int>(fb_on[idx]) - static_cast<int>(fb_off[idx])) <= 1);
    }
    KT_CHECK(diff_pixels > 0);

    std::cout << "  -> IGN dithering (Gradient, Mica, Gaussian Shadow) PASSED." << std::endl;
}

void test_pointer_reveal_cpu() {
    std::cout << "[Test 4] Testing R3 IsPointerReveal (Surface, Edge, Rounded Clip, R=0)..." << std::endl;

    const uint32_t width = 256;
    const uint32_t height = 160;
    TileGrid grid(width, height);
    TileBinner binner;

    std::vector<uint32_t> all_tiles(grid.get_total_tiles());
    for (uint32_t i = 0; i < grid.get_total_tiles(); ++i) all_tiles[i] = i;

    ClipRect widget_clip{};
    widget_clip.min_x = 40.0f; widget_clip.min_y = 40.0f;
    widget_clip.max_x = 240.0f; widget_clip.max_y = 120.0f;
    widget_clip.corner_radius = 8.0f;
    widget_clip.parent_index = INVALID_INDEX_U32;
    widget_clip.flags = kClipFlagRevealBounds;
    std::vector<ClipRect> clips = {widget_clip};

    // 1. Surface reveal centered at (140, 80) with radius 32
    UiPrimitive surf{};
    surf.min_x = 108.0f; surf.min_y = 48.0f;
    surf.max_x = 172.0f; surf.max_y = 112.0f;
    surf.radius_top_left = surf.radius_top_right = surf.radius_bottom_right = surf.radius_bottom_left = 8.0f;
    surf.color_r = 1.0f; surf.color_g = 1.0f; surf.color_b = 1.0f; surf.color_a = 1.0f;
    surf.shadow_offset_x = 140.0f; surf.shadow_offset_y = 80.0f;
    surf.shadow_blur = 32.0f;
    surf.shadow_spread = 1.0f;
    surf.flags = static_cast<uint16_t>(PrimitiveFlags::IsPointerReveal);
    surf.clip_index = 0;

    std::vector<UiPrimitive> surf_prims = {surf};
    auto bin_surf = binner.bin_primitives(grid, surf_prims, true, clips);
    std::vector<uint8_t> fb(width * height * 4, 0);

    CpuTileRasterizer::RenderParams params{};
    params.width = width;
    params.height = height;
    params.primitives = surf_prims;
    params.tile_indices = bin_surf.indices;
    params.tile_headers = bin_surf.headers;
    params.dirty_tiles = all_tiles;
    params.clip_rects = clips;
    params.thread_count = 1;
    CpuTileRasterizer::render(params, fb.data());

    const uint8_t a_center = fb[(80 * width + 140) * 4 + 3];
    const uint8_t a_mid = fb[(80 * width + 156) * 4 + 3];
    const uint8_t a_out = fb[(80 * width + 175) * 4 + 3];
    KT_CHECK(a_center > 240);
    KT_CHECK(a_mid > 20 && a_mid < a_center);
    KT_CHECK(a_out == 0);

    // 2. Edge reveal near top edge (140, 40) with radius 32, border_width = 2.0
    UiPrimitive edge{};
    edge.min_x = 108.0f; edge.min_y = 40.0f;
    edge.max_x = 172.0f; edge.max_y = 72.0f;
    edge.radius_top_left = edge.radius_top_right = edge.radius_bottom_right = edge.radius_bottom_left = 8.0f;
    edge.color_r = 1.0f; edge.color_g = 1.0f; edge.color_b = 1.0f; edge.color_a = 1.0f;
    edge.border_width = 2.0f;
    edge.shadow_offset_x = 140.0f; edge.shadow_offset_y = 40.0f;
    edge.shadow_blur = 32.0f;
    edge.shadow_spread = 1.0f;
    edge.flags = static_cast<uint16_t>(PrimitiveFlags::IsPointerReveal | PrimitiveFlags::HasBorder);
    edge.clip_index = 0;

    std::vector<UiPrimitive> edge_prims = {edge};
    auto bin_edge = binner.bin_primitives(grid, edge_prims, true, clips);
    std::fill(fb.begin(), fb.end(), 0);
    params.primitives = edge_prims;
    params.tile_indices = bin_edge.indices;
    params.tile_headers = bin_edge.headers;
    CpuTileRasterizer::render(params, fb.data());

    // Top edge at y=41 (p.y=41.5) must be lit; interior (140, 55) must be 0;
    // clamped interior vertical edge (108, 55) MUST NOT draw a false border!
    KT_CHECK(fb[(41 * width + 140) * 4 + 3] > 200);
    KT_CHECK(fb[(55 * width + 140) * 4 + 3] == 0);
    KT_CHECK(fb[(55 * width + 108) * 4 + 3] == 0);

    // 3. Rounded clip corner exclusion: cursor near top-left corner (41, 41)
    edge.min_x = 40.0f; edge.min_y = 40.0f; edge.max_x = 73.0f; edge.max_y = 73.0f;
    edge.shadow_offset_x = 41.0f; edge.shadow_offset_y = 41.0f;
    edge_prims[0] = edge;
    bin_edge = binner.bin_primitives(grid, edge_prims, true, clips);
    std::fill(fb.begin(), fb.end(), 0);
    params.primitives = edge_prims;
    params.tile_indices = bin_edge.indices;
    params.tile_headers = bin_edge.headers;
    CpuTileRasterizer::render(params, fb.data());
    // Pixel (40, 40) lies outside corner_radius=8 squircle clip
    KT_CHECK(fb[(40 * width + 40) * 4 + 3] == 0);

    // 4. R == 0 emits zero pixels
    edge.shadow_blur = 0.0f;
    edge_prims[0] = edge;
    bin_edge = binner.bin_primitives(grid, edge_prims, true, clips);
    std::fill(fb.begin(), fb.end(), 0);
    params.primitives = edge_prims;
    params.tile_indices = bin_edge.indices;
    params.tile_headers = bin_edge.headers;
    CpuTileRasterizer::render(params, fb.data());
    for (uint8_t byte : fb) {
        KT_CHECK(byte == 0);
    }

    std::cout << "  -> IsPointerReveal (Surface, Edge, Rounded Clip, R=0) PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 9 CPU Rasterizer Unit Tests ===" << std::endl;

    test_adaptive_concurrency_dispatcher();
    test_cpu_tile_rasterization();
    test_ign_dithering_and_linear_gradient();
    test_pointer_reveal_cpu();

    std::cout << "=== All CPU Rasterizer Tests PASSED (100%) ===" << std::endl;
    return 0;
}
