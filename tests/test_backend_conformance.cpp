#include "test_common.hpp"
#include "kernetile/cpu/cpu_rasterizer.hpp"
#include "kernetile/cpu/image_compare.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32) && defined(KT_CONFORMANCE_ENABLE_D3D12) && !defined(KERNETILE_PURE_GDI)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "kernetile/hal/hal.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_recorder.hpp"
#include "d3d12_texture.hpp"
#include "compute/ui_compute.hpp"
#if defined(KT_PRECOMPILED_DXBC)
#include "ui_compute_dxbc.hpp"
#else
#include "ui_compute_shader.hpp"
#include <d3dcompiler.h>
#endif
#endif

#define KT_CHECK(cond)                                                                             \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::fprintf(stderr, "CHECK FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)

using namespace kernetile::cpu;
using namespace kernetile::core;
using namespace kernetile::tile;

namespace {

uint16_t pack_unorm16(float v) {
    return static_cast<uint16_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 65535.0f));
}

uint32_t max_alpha_diff(const uint8_t* a, const uint8_t* b, uint32_t w, uint32_t h) {
    uint32_t max_diff = 0;
    const size_t n = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < n; ++i) {
        const uint32_t d = static_cast<uint32_t>(
            std::abs(static_cast<int>(a[i * 4 + 3]) - static_cast<int>(b[i * 4 + 3])));
        if (d > max_diff) max_diff = d;
    }
    return max_diff;
}

uint32_t max_flat_run(const std::vector<uint8_t>& fb, uint32_t width, uint32_t y) {
    uint32_t best = 1;
    uint32_t cur = 1;
    for (uint32_t x = 1; x < width; ++x) {
        const uint8_t prev = fb[(static_cast<size_t>(y) * width + x - 1) * 4];
        const uint8_t curr = fb[(static_cast<size_t>(y) * width + x) * 4];
        if (curr == prev) {
            ++cur;
            best = std::max(best, cur);
        } else {
            cur = 1;
        }
    }
    return best;
}

std::string read_text_file(const char* rel_path) {
    std::ifstream ifs(rel_path, std::ios::binary);
    KT_CHECK(ifs.is_open());
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

} // namespace

void test_cpu_conformance() {
    std::cout << "[Test 1] Testing CPU vs Reference Image Conformance (SSIM & Max LSB)..." << std::endl;

    const uint32_t width = 128;
    const uint32_t height = 128;

    std::vector<uint8_t> img_single(width * height * 4, 0);
    std::vector<uint8_t> img_multi(width * height * 4, 0);

    // 构造全要素图元集合
    std::vector<UiPrimitive> primitives;

    // 图元 1: 背景卡片 (带大半径高斯软阴影)
    UiPrimitive p1{};
    p1.min_x = 20.0f;
    p1.min_y = 20.0f;
    p1.max_x = 108.0f;
    p1.max_y = 108.0f;
    p1.radius_top_left = 12.0f;
    p1.radius_top_right = 12.0f;
    p1.radius_bottom_right = 12.0f;
    p1.radius_bottom_left = 12.0f;
    p1.color_r = 0.2f;
    p1.color_g = 0.25f;
    p1.color_b = 0.35f;
    p1.color_a = 0.9f;
    p1.shadow_blur = 6.0f;
    p1.shadow_spread = 2.0f;
    p1.shadow_offset_y = 4.0f;
    primitives.push_back(p1);

    // 图元 2: 内部高亮按钮 (带 2px 等宽描边)
    UiPrimitive p2{};
    p2.min_x = 35.0f;
    p2.min_y = 45.0f;
    p2.max_x = 93.0f;
    p2.max_y = 85.0f;
    p2.radius_top_left = 6.0f;
    p2.radius_top_right = 6.0f;
    p2.radius_bottom_right = 6.0f;
    p2.radius_bottom_left = 6.0f;
    p2.color_r = 0.0f;
    p2.color_g = 0.7f;
    p2.color_b = 0.9f;
    p2.color_a = 1.0f;
    p2.border_width = 2.0f;
    primitives.push_back(p2);

    TileGrid grid(width, height);
    TileBinner binner;
    BinningResult bin_res = binner.bin_primitives(grid, primitives);

    std::vector<uint32_t> dirty_tiles;
    for (uint32_t i = 0; i < grid.get_total_tiles(); ++i) {
        dirty_tiles.push_back(i);
    }

    // 运行 1: 单线程执行
    CpuTileRasterizer::RenderParams params_single{};
    params_single.width = width;
    params_single.height = height;
    params_single.primitives = primitives;
    params_single.tile_indices = bin_res.indices;
    params_single.tile_headers = bin_res.headers;
    params_single.dirty_tiles = dirty_tiles;
    params_single.thread_count = 1;
    CpuTileRasterizer::render(params_single, img_single.data());

    // 运行 2: 多线程并发执行 (4 线程分片)
    CpuTileRasterizer::RenderParams params_multi = params_single;
    params_multi.thread_count = 4;
    CpuTileRasterizer::render(params_multi, img_multi.data());

    // 比较金标像素一致性
    ImageCompareResult comp = ImageCompare::compare_rgba8(img_single.data(), img_multi.data(), width, height);

    std::cout << "  Conformance Comparison:" << std::endl;
    std::cout << "    Max LSB Diff:     " << comp.max_lsb_diff << " (Target <= 1)" << std::endl;
    std::cout << "    Mean Abs Error:   " << comp.mean_abs_error << std::endl;
    std::cout << "    SSIM Metric:      " << comp.ssim << " (Target >= 0.9995)" << std::endl;
    std::cout << "    Differing Pixels: " << comp.differing_pixels << " / " << (width * height) << std::endl;

    KT_CHECK(comp.max_lsb_diff == 0); // 单线程与多线程在相同瓦片划分下必须 100% 逐位精确相同
    KT_CHECK(comp.ssim >= 0.9999);

    std::cout << "  -> Backend conformance PASSED." << std::endl;
}

void test_static_cross_backend_shader_parity() {
    std::cout << "[Test 2] Testing Static Cross-Backend Shader & ABI Parity (HLSL/WGSL/D3D12/Vulkan/WebGPU)..." << std::endl;

    const std::string hlsl = read_text_file("shaders/ui_compute_d3d12.hlsl");
    const std::string wgsl = read_text_file("shaders/ui_compute.wgsl");
    const std::string d3d12_cpp = read_text_file("src/compute/ui_compute_d3d12.cpp");
    const std::string vulkan_cpp = read_text_file("src/compute/ui_compute_vulkan.cpp");
    const std::string web_cpp = read_text_file("src/compute/ui_compute_web.cpp");
    const std::string compute_hpp = read_text_file("src/compute/ui_compute.hpp");
    const std::string cpu_cpp = read_text_file("src/cpu/cpu_rasterizer.cpp");

    // Frame ABI: 24-dword layout with render_flags at slot 6
    KT_CHECK(compute_hpp.find("kRenderFlagIgnDithering = 0x1u") != std::string::npos);
    KT_CHECK(d3d12_cpp.find("parameters[6] = frame.render_flags;") != std::string::npos);
    KT_CHECK(vulkan_cpp.find("params[6] = frame.render_flags;") != std::string::npos);
    KT_CHECK(web_cpp.find("uniformData[6] = render_flags;") != std::string::npos);
    KT_CHECK(hlsl.find("tile_count, groups_x, clip_count, mica_enabled, render_flags, pad1;") != std::string::npos);
    KT_CHECK(wgsl.find("render_flags: u32,") != std::string::npos);

    // Binding slots 0..8 parity
    for (int i = 0; i <= 8; ++i) {
        const std::string vk_b = "KT_BINDING(" + std::to_string(i) + ")";
        const std::string wg_b = "@binding(" + std::to_string(i) + ")";
        KT_CHECK(hlsl.find(vk_b) != std::string::npos);
        KT_CHECK(wgsl.find(wg_b) != std::string::npos);
    }
    KT_CHECK(vulkan_cpp.find("std::array<VkDescriptorType, 9>") != std::string::npos);

    // IGN 3-instruction arithmetic constants across CPU, HLSL, WGSL
    for (const char* k : {"52.9829189", "0.06711056", "0.00583715"}) {
        KT_CHECK(hlsl.find(k) != std::string::npos);
        KT_CHECK(wgsl.find(k) != std::string::npos);
        KT_CHECK(cpu_cpp.find(k) != std::string::npos);
    }
    KT_CHECK(hlsl.find("(render_flags & 1u) != 0u") != std::string::npos);
    KT_CHECK(wgsl.find("(frame.render_flags & 1u) != 0u") != std::string::npos);

    // Feature flags: HasLinearGradient (2u), IsPointerReveal (512u), kClipFlagRevealBounds (1u)
    KT_CHECK(hlsl.find("prim.flags_clip & 512u") != std::string::npos);
    KT_CHECK(wgsl.find("(prim.flags_clip & 512u) != 0u") != std::string::npos);
    KT_CHECK(hlsl.find("(prim.flags_clip & 2u) != 0u") != std::string::npos);
    KT_CHECK(wgsl.find("(prim.flags_clip & 2u) != 0u") != std::string::npos);
    KT_CHECK(hlsl.find("(c.flags & 1u) != 0u") != std::string::npos);
    KT_CHECK(wgsl.find("(c.flags & 1u) != 0u") != std::string::npos);

    // KT_VULKAN explicit 8-bit quantization parity before SFLOAT writeback
    KT_CHECK(hlsl.find("#if defined(KT_VULKAN)") != std::string::npos);
    KT_CHECK(hlsl.find("round(saturate(color) * 255.0) / 255.0") != std::string::npos);

    std::cout << "  -> Static cross-backend shader & ABI parity PASSED." << std::endl;
}

#if defined(_WIN32) && defined(KT_CONFORMANCE_ENABLE_D3D12) && !defined(KERNETILE_PURE_GDI)

class WarpComputeRunner {
public:
    WarpComputeRunner() {
        using namespace kernetile::hal;
        m_device = create_d3d12_device(true);
        KT_CHECK(m_device != nullptr);

        const std::string name(m_device->get_device_name());
        const bool is_warp =
            name.find("Microsoft") != std::string::npos ||
            name.find("Basic Render") != std::string::npos ||
            name.find("WARP") != std::string::npos ||
            name.find("Software") != std::string::npos;
        KT_CHECK(is_warp);
        std::cout << "  -> WARP Compute Adapter: " << name << std::endl;

#if defined(KT_PRECOMPILED_DXBC)
        m_pipeline = m_device->create_compute_pipeline(
            {::ui_compute_dxbc, sizeof(::ui_compute_dxbc)});
#else
        kernetile::hal::d3d12::ComPtr<ID3DBlob> binary, errors;
        const auto status = D3DCompile(
            kernetile::compute::shader_source,
            sizeof(kernetile::compute::shader_source) - 1,
            "ui_compute_d3d12.hlsl",
            nullptr, nullptr, "compose", "cs_5_1",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &binary, &errors);
        if (FAILED(status) && errors) {
            std::fprintf(stderr, "%s\n", static_cast<const char*>(errors->GetBufferPointer()));
        }
        KT_CHECK(SUCCEEDED(status) && binary != nullptr);
        m_pipeline = m_device->create_compute_pipeline(
            {static_cast<const uint8_t*>(binary->GetBufferPointer()), binary->GetBufferSize()});
#endif
        KT_CHECK(m_pipeline != nullptr);

        m_dummy_atlas = m_device->create_texture(
            TextureDesc{16, 16, TextureFormat::R8_UNORM, false});
        KT_CHECK(m_dummy_atlas != nullptr);
    }

    ~WarpComputeRunner() {
        if (m_device) {
            m_device->wait_idle();
        }
    }

    std::vector<uint8_t> render(
        uint32_t width,
        uint32_t height,
        std::span<const UiPrimitive> primitives,
        std::span<const uint32_t> indices,
        std::span<const TileHeader> headers,
        std::span<const ClipRect> clips,
        bool mica_enabled,
        const std::array<uint32_t, 16>& mica_grid,
        bool enable_dithering)
    {
        using namespace kernetile::hal;
        using namespace kernetile::hal::d3d12;

        const TileGrid grid(width, height);
        const uint32_t tile_count = grid.get_total_tiles();
        KT_CHECK(headers.size() == tile_count);

        std::vector<kernetile::compute::Tile> tiles;
        tiles.reserve(tile_count);
        for (uint32_t id = 0; id < tile_count; ++id) {
            const auto coord = grid.get_tile_coord(id);
            tiles.push_back(kernetile::compute::Tile{
                coord.x, coord.y, headers[id].primitive_offset, headers[id].primitive_count});
        }

        auto make_upload = [&](std::span<const std::byte> bytes) {
            const size_t sz = std::max<size_t>(sizeof(UiPrimitive), bytes.size());
            auto buf = m_device->create_buffer(BufferDesc{sz, BufferUsage::Upload});
            KT_CHECK(buf != nullptr);
            void* mapped = buf->map();
            KT_CHECK(mapped != nullptr);
            std::memset(mapped, 0, sz);
            if (!bytes.empty()) {
                std::memcpy(mapped, bytes.data(), bytes.size());
            }
            return buf;
        };

        auto buf_prims = make_upload(std::as_bytes(primitives));
        auto buf_idx   = make_upload(std::as_bytes(indices));
        auto buf_tiles = make_upload(std::as_bytes(std::span<const kernetile::compute::Tile>(tiles)));
        auto buf_clips = make_upload(std::as_bytes(clips));

        auto offscreen = m_device->create_texture(
            TextureDesc{width, height, TextureFormat::RGBA8_UNORM, true});
        KT_CHECK(offscreen != nullptr);

        auto* d3d_dev = static_cast<D3D12Device*>(m_device.get());
        auto* d3d_offscreen = static_cast<D3D12Texture*>(offscreen.get());
        auto* d3d_atlas = static_cast<D3D12Texture*>(m_dummy_atlas.get());

        D3D12_RESOURCE_DESC tex_desc = d3d_offscreen->get_native_resource()->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 total_bytes = 0;
        d3d_dev->get_native_device()->GetCopyableFootprints(
            &tex_desc, 0, 1, 0, &footprint, nullptr, nullptr, &total_bytes);
        KT_CHECK(total_bytes > 0);

        auto readback = m_device->create_buffer(
            BufferDesc{static_cast<size_t>(total_bytes), BufferUsage::Readback});
        KT_CHECK(readback != nullptr);

        auto rec = m_device->create_command_recorder();
        KT_CHECK(rec != nullptr);
        auto* d3d_rec = static_cast<D3D12CommandRecorder*>(rec.get());
        KT_CHECK(d3d_rec->is_valid());

        rec->resource_barrier(*offscreen, d3d_offscreen->get_current_state(), ResourceState::UnorderedAccess);
        if (d3d_atlas->get_current_state() != ResourceState::ComputeShaderResource) {
            rec->resource_barrier(*m_dummy_atlas, d3d_atlas->get_current_state(), ResourceState::ComputeShaderResource);
        }

        rec->set_compute_pipeline(*m_pipeline);
        rec->set_storage_buffer(0, *buf_prims);
        rec->set_storage_buffer(1, *buf_idx);
        rec->set_storage_buffer(2, *buf_tiles);
        rec->set_storage_buffer(3, *buf_clips);
        rec->set_storage_texture(0, *offscreen);
        d3d_rec->set_compute_atlases(*d3d_atlas, *d3d_atlas);

        const uint32_t groups_x = std::min(tile_count, 1024u);
        std::array<uint32_t, 24> params{};
        params[0] = width;
        params[1] = height;
        params[2] = tile_count;
        params[3] = groups_x;
        params[4] = static_cast<uint32_t>(clips.size());
        params[5] = mica_enabled ? 1u : 0u;
        params[6] = enable_dithering ? kernetile::compute::kRenderFlagIgnDithering : 0u;
        params[7] = 0u;
        for (size_t i = 0; i < 16; ++i) {
            params[8 + i] = mica_grid[i];
        }
        d3d_rec->get_native_list()->SetComputeRoot32BitConstants(
            6, static_cast<UINT>(params.size()), params.data(), 0);
        rec->dispatch(groups_x, (tile_count + groups_x - 1) / groups_x, 1);
        rec->copy_texture_to_buffer(*offscreen, *readback);
        rec->close();

        m_device->submit(std::move(rec));
        m_device->wait_idle();

        const auto* mapped_rb = static_cast<const uint8_t*>(readback->map());
        KT_CHECK(mapped_rb != nullptr);
        const auto* src = mapped_rb + footprint.Offset;
        std::vector<uint8_t> out(static_cast<size_t>(width) * height * 4, 0);
        for (uint32_t row = 0; row < height; ++row) {
            std::memcpy(
                out.data() + static_cast<size_t>(row) * width * 4,
                src + static_cast<size_t>(row) * footprint.Footprint.RowPitch,
                static_cast<size_t>(width) * 4);
        }
        readback->unmap();
        return out;
    }

private:
    std::unique_ptr<kernetile::hal::GraphicsDevice> m_device;
    std::unique_ptr<kernetile::hal::ComputePipeline> m_pipeline;
    std::unique_ptr<kernetile::hal::GpuTexture> m_dummy_atlas;
};

static std::vector<uint8_t> render_cpu(
    uint32_t width,
    uint32_t height,
    std::span<const UiPrimitive> primitives,
    const BinningResult& bin_res,
    std::span<const ClipRect> clips,
    bool mica_enabled,
    const std::array<uint32_t, 16>& mica_grid,
    bool enable_dithering)
{
    const TileGrid grid(width, height);
    std::vector<uint32_t> all_tiles(grid.get_total_tiles());
    for (uint32_t i = 0; i < grid.get_total_tiles(); ++i) {
        all_tiles[i] = i;
    }

    std::vector<uint8_t> fb(static_cast<size_t>(width) * height * 4, 0);
    CpuTileRasterizer::RenderParams params{};
    params.width = width;
    params.height = height;
    params.primitives = primitives;
    params.tile_indices = bin_res.indices;
    params.tile_headers = bin_res.headers;
    params.dirty_tiles = all_tiles;
    params.clip_rects = clips;
    params.atlas_pixels = nullptr; // Zero-LUT proof
    params.atlas_width = 0;
    params.atlas_height = 0;
    params.mica_enabled = mica_enabled;
    params.mica_grid = mica_grid;
    params.enable_dithering = enable_dithering;
    params.thread_count = 1;
    CpuTileRasterizer::render(params, fb.data());
    return fb;
}

void test_warp_parity_r1_ign_dithering(WarpComputeRunner& warp) {
    std::cout << "[Test 3] Testing CPU vs D3D12 WARP Parity: R1 IGN Dithering (Gradients, Mica, Solids, Shadows)..." << std::endl;
    TileBinner binner;

    // 1. Shallow linear gradient 0 -> 16/255 in [0, 0, 256, 32] over transparent [32..63]
    {
        const uint32_t w = 256, h = 64;
        const TileGrid grid(w, h);
        UiPrimitive grad{};
        grad.min_x = 0.0f; grad.min_y = 0.0f; grad.max_x = 256.0f; grad.max_y = 32.0f;
        grad.color_r = 0.0f; grad.color_g = 0.0f; grad.color_b = 0.0f; grad.color_a = 1.0f;
        const float end_val = 16.0f / 255.0f;
        grad.atlas_u0 = pack_unorm16(end_val);
        grad.atlas_v0 = pack_unorm16(end_val);
        grad.atlas_u1 = pack_unorm16(end_val);
        grad.atlas_v1 = pack_unorm16(1.0f);
        grad.shadow_offset_x = 0.0f; grad.shadow_offset_y = 0.0f;
        grad.shadow_blur = 256.0f; grad.shadow_spread = 0.0f;
        grad.flags = static_cast<uint16_t>(PrimitiveFlags::HasLinearGradient);

        std::vector<UiPrimitive> prims = {grad};
        auto bin_res = binner.bin_primitives(grid, prims);

        auto cpu_off = render_cpu(w, h, prims, bin_res, {}, false, {}, false);
        auto gpu_off = warp.render(w, h, prims, bin_res.indices, bin_res.headers, {}, false, {}, false);
        auto cpu_on = render_cpu(w, h, prims, bin_res, {}, false, {}, true);
        auto gpu_on = warp.render(w, h, prims, bin_res.indices, bin_res.headers, {}, false, {}, true);

        const uint32_t cpu_run_off = max_flat_run(cpu_off, w, 16);
        const uint32_t gpu_run_off = max_flat_run(gpu_off, w, 16);
        const uint32_t cpu_run_on = max_flat_run(cpu_on, w, 16);
        const uint32_t gpu_run_on = max_flat_run(gpu_on, w, 16);
        KT_CHECK(cpu_run_off >= 12);
        KT_CHECK(gpu_run_off >= 12);
        KT_CHECK(cpu_run_on < cpu_run_off);
        KT_CHECK(gpu_run_on < gpu_run_off);

        for (uint32_t x = 0; x < w; ++x) {
            const float ideal = ((static_cast<float>(x) + 0.5f) / 256.0f) * 16.0f;
            const size_t rect_idx = (16u * w + x) * 4;
            KT_CHECK(cpu_on[rect_idx + 3] == 255);
            KT_CHECK(gpu_on[rect_idx + 3] == 255);
            KT_CHECK(std::abs(static_cast<float>(cpu_on[rect_idx]) - ideal) <= 1.0f);
            KT_CHECK(std::abs(static_cast<float>(gpu_on[rect_idx]) - ideal) <= 1.0f);

            const size_t bg_idx = (48u * w + x) * 4;
            for (int c = 0; c < 4; ++c) {
                KT_CHECK(cpu_on[bg_idx + c] == 0);
                KT_CHECK(gpu_on[bg_idx + c] == 0);
            }
        }

        const auto comp = ImageCompare::compare_rgba8(cpu_on.data(), gpu_on.data(), w, h);
        KT_CHECK(comp.max_lsb_diff <= 1);
        KT_CHECK(comp.ssim >= 0.9999);
        KT_CHECK(max_alpha_diff(cpu_on.data(), gpu_on.data(), w, h) == 0);
    }

    // 2. Dark-mode smooth Mica backdrop (empty primitives)
    {
        const uint32_t w = 256, h = 64;
        const TileGrid grid(w, h);
        std::vector<UiPrimitive> empty_prims;
        auto bin_empty = binner.bin_primitives(grid, empty_prims);
        std::array<uint32_t, 16> mica_grid{};
        for (uint32_t gy = 0; gy < 4; ++gy) {
            for (uint32_t gx = 0; gx < 4; ++gx) {
                const uint32_t c = 10u + gx * 3u;
                mica_grid[gy * 4 + gx] = c | (c << 8) | (c << 16) | (255u << 24);
            }
        }

        auto cpu_off = render_cpu(w, h, empty_prims, bin_empty, {}, true, mica_grid, false);
        auto gpu_off = warp.render(w, h, empty_prims, bin_empty.indices, bin_empty.headers, {}, true, mica_grid, false);
        auto cpu_on = render_cpu(w, h, empty_prims, bin_empty, {}, true, mica_grid, true);
        auto gpu_on = warp.render(w, h, empty_prims, bin_empty.indices, bin_empty.headers, {}, true, mica_grid, true);

        KT_CHECK(max_flat_run(cpu_off, w, 10) >= 16);
        KT_CHECK(max_flat_run(gpu_off, w, 10) >= 16);
        KT_CHECK(max_flat_run(cpu_on, w, 10) < max_flat_run(cpu_off, w, 10));
        KT_CHECK(max_flat_run(gpu_on, w, 10) < max_flat_run(gpu_off, w, 10));
        for (uint32_t x = 0; x < w; ++x) {
            KT_CHECK(cpu_on[(10u * w + x) * 4 + 3] == 255);
            KT_CHECK(gpu_on[(10u * w + x) * 4 + 3] == 255);
        }

        const auto comp = ImageCompare::compare_rgba8(cpu_on.data(), gpu_on.data(), w, h);
        KT_CHECK(comp.max_lsb_diff <= 1);
        KT_CHECK(max_alpha_diff(cpu_on.data(), gpu_on.data(), w, h) == 0);
    }

    // 3. Solid fractional color (color_r = 0.150f = 38.25 LSB) testing last_opaque_cover bypass
    {
        const uint32_t w = 128, h = 128;
        const TileGrid grid(w, h);
        UiPrimitive solid{};
        solid.min_x = 0.0f; solid.min_y = 0.0f; solid.max_x = 128.0f; solid.max_y = 128.0f;
        solid.color_r = 0.150f; solid.color_g = 0.0f; solid.color_b = 1.0f; solid.color_a = 1.0f;
        std::vector<UiPrimitive> prims = {solid};
        auto bin_res = binner.bin_primitives(grid, prims);

        auto cpu_off = render_cpu(w, h, prims, bin_res, {}, false, {}, false);
        auto gpu_off = warp.render(w, h, prims, bin_res.indices, bin_res.headers, {}, false, {}, false);
        auto cpu_on = render_cpu(w, h, prims, bin_res, {}, false, {}, true);
        auto gpu_on = warp.render(w, h, prims, bin_res.indices, bin_res.headers, {}, false, {}, true);

        uint32_t cpu_38 = 0, cpu_39 = 0, gpu_38 = 0, gpu_39 = 0;
        for (uint32_t i = 0; i < w * h; ++i) {
            KT_CHECK(cpu_off[i * 4 + 0] == 38);
            KT_CHECK(gpu_off[i * 4 + 0] == 38);
            KT_CHECK(cpu_on[i * 4 + 1] == 0);
            KT_CHECK(gpu_on[i * 4 + 1] == 0);
            KT_CHECK(cpu_on[i * 4 + 3] == 255);
            KT_CHECK(gpu_on[i * 4 + 3] == 255);
            KT_CHECK(cpu_on[i * 4 + 0] == 38 || cpu_on[i * 4 + 0] == 39);
            KT_CHECK(gpu_on[i * 4 + 0] == 38 || gpu_on[i * 4 + 0] == 39);
            if (cpu_on[i * 4 + 0] == 38) ++cpu_38; else ++cpu_39;
            if (gpu_on[i * 4 + 0] == 38) ++gpu_38; else ++gpu_39;
        }
        KT_CHECK(cpu_38 > cpu_39 && cpu_39 > 1000);
        KT_CHECK(gpu_38 > gpu_39 && gpu_39 > 1000);

        const auto comp = ImageCompare::compare_rgba8(cpu_on.data(), gpu_on.data(), w, h);
        KT_CHECK(comp.max_lsb_diff <= 1);
        KT_CHECK(comp.ssim >= 0.9999);
        KT_CHECK(max_alpha_diff(cpu_on.data(), gpu_on.data(), w, h) == 0);
    }

    // 4. Large-radius Gaussian shadow (shadow_blur = 24.0f)
    {
        const uint32_t w = 256, h = 64;
        const TileGrid grid(w, h);
        UiPrimitive bg{};
        bg.min_x = 0.0f; bg.min_y = 0.0f; bg.max_x = 256.0f; bg.max_y = 64.0f;
        bg.color_r = 0.15f; bg.color_g = 0.15f; bg.color_b = 0.15f; bg.color_a = 1.0f;

        UiPrimitive shadow_box{};
        shadow_box.min_x = 16.0f; shadow_box.min_y = 16.0f;
        shadow_box.max_x = 80.0f; shadow_box.max_y = 48.0f;
        shadow_box.color_r = 0.0f; shadow_box.color_g = 0.0f; shadow_box.color_b = 0.0f; shadow_box.color_a = 0.4f;
        shadow_box.shadow_blur = 24.0f;

        std::vector<UiPrimitive> prims = {bg, shadow_box};
        auto bin_res = binner.bin_primitives(grid, prims);

        auto cpu_off = render_cpu(w, h, prims, bin_res, {}, false, {}, false);
        auto gpu_off = warp.render(w, h, prims, bin_res.indices, bin_res.headers, {}, false, {}, false);
        auto cpu_on = render_cpu(w, h, prims, bin_res, {}, false, {}, true);
        auto gpu_on = warp.render(w, h, prims, bin_res.indices, bin_res.headers, {}, false, {}, true);

        uint32_t cpu_diff = 0, gpu_diff = 0;
        for (uint32_t x = 80; x < 150; ++x) {
            const size_t idx = (32u * w + x) * 4;
            if (cpu_on[idx] != cpu_off[idx]) ++cpu_diff;
            if (gpu_on[idx] != gpu_off[idx]) ++gpu_diff;
            KT_CHECK(std::abs(static_cast<int>(cpu_on[idx]) - static_cast<int>(cpu_off[idx])) <= 1);
            KT_CHECK(std::abs(static_cast<int>(gpu_on[idx]) - static_cast<int>(gpu_off[idx])) <= 1);
        }
        KT_CHECK(cpu_diff > 0 && gpu_diff > 0);

        const auto comp = ImageCompare::compare_rgba8(cpu_on.data(), gpu_on.data(), w, h);
        KT_CHECK(comp.max_lsb_diff <= 1);
        KT_CHECK(comp.ssim >= 0.9999);
        KT_CHECK(max_alpha_diff(cpu_on.data(), gpu_on.data(), w, h) == 0);
    }

    // 5. HasLinearGradient edge cases (diagonal rounded+border, degenerate fallback, 0->1, 1->0.5 non-occlusion)
    {
        const uint32_t w = 256, h = 256;
        const TileGrid grid(w, h);

        auto make_grad = [](float x0, float y0, float x1, float y1,
                            float r0, float g0, float b0, float a0,
                            float r1, float g1, float b1, float a1,
                            float sx, float sy, float ex, float ey,
                            float radius, float border) {
            UiPrimitive p{};
            p.min_x = x0; p.min_y = y0; p.max_x = x1; p.max_y = y1;
            p.radius_top_left = p.radius_top_right = p.radius_bottom_right = p.radius_bottom_left = radius;
            p.color_r = r0 * a0; p.color_g = g0 * a0; p.color_b = b0 * a0; p.color_a = a0;
            p.atlas_u0 = pack_unorm16(r1 * a1);
            p.atlas_v0 = pack_unorm16(g1 * a1);
            p.atlas_u1 = pack_unorm16(b1 * a1);
            p.atlas_v1 = pack_unorm16(a1);
            p.border_width = border;
            p.shadow_offset_x = sx; p.shadow_offset_y = sy;
            p.shadow_blur = ex; p.shadow_spread = ey;
            p.flags = static_cast<uint16_t>(PrimitiveFlags::HasLinearGradient);
            if (border > 0.0f) {
                p.flags |= static_cast<uint16_t>(PrimitiveFlags::HasBorder);
            }
            return p;
        };

        // Diagonal rounded + border
        auto g_diag = make_grad(16.0f, 16.0f, 120.0f, 120.0f,
                                0.0f, 0.2f, 0.8f, 1.0f,
                                0.9f, 0.4f, 0.1f, 1.0f,
                                16.0f, 16.0f, 120.0f, 120.0f,
                                12.0f, 2.0f);
        // Degenerate zero-length vertical fallback
        auto g_degen = make_grad(140.0f, 16.0f, 240.0f, 120.0f,
                                 0.1f, 0.1f, 0.6f, 1.0f,
                                 0.8f, 0.8f, 0.2f, 1.0f,
                                 180.0f, 50.0f, 180.0f, 50.0f,
                                 0.0f, 0.0f);
        // start_a = 0 -> end_a = 1
        auto g_fade_in = make_grad(16.0f, 140.0f, 120.0f, 240.0f,
                                   1.0f, 0.0f, 0.0f, 0.0f,
                                   1.0f, 0.0f, 0.0f, 1.0f,
                                   16.0f, 140.0f, 120.0f, 140.0f,
                                   0.0f, 0.0f);
        // Green background under start_a=1 -> end_a=0.5 non-occlusion gradient
        UiPrimitive green_bg{};
        green_bg.min_x = 140.0f; green_bg.min_y = 140.0f; green_bg.max_x = 240.0f; green_bg.max_y = 240.0f;
        green_bg.color_r = 0.0f; green_bg.color_g = 1.0f; green_bg.color_b = 0.0f; green_bg.color_a = 1.0f;
        auto g_non_occ = make_grad(140.0f, 140.0f, 240.0f, 240.0f,
                                   1.0f, 0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f, 0.5f,
                                   140.0f, 140.0f, 240.0f, 140.0f,
                                   0.0f, 0.0f);

        std::vector<UiPrimitive> prims = {g_diag, g_degen, g_fade_in, green_bg, g_non_occ};
        auto bin_res = binner.bin_primitives(grid, prims, true);

        auto cpu_on = render_cpu(w, h, prims, bin_res, {}, false, {}, true);
        auto gpu_on = warp.render(w, h, prims, bin_res.indices, bin_res.headers, {}, false, {}, true);

        // Non-occlusion check: green background must show through at right side of g_non_occ (x=235, y=190)
        const size_t non_occ_idx = (190u * w + 235u) * 4;
        KT_CHECK(cpu_on[non_occ_idx + 1] > 100);
        KT_CHECK(gpu_on[non_occ_idx + 1] > 100);

        const auto comp = ImageCompare::compare_rgba8(cpu_on.data(), gpu_on.data(), w, h);
        KT_CHECK(comp.max_lsb_diff <= 1);
        KT_CHECK(max_alpha_diff(cpu_on.data(), gpu_on.data(), w, h) == 0);
    }

    std::cout << "  -> R1 IGN Dithering CPU vs WARP parity PASSED." << std::endl;
}

void test_warp_parity_r2_pixel_snapping(WarpComputeRunner& warp) {
    std::cout << "[Test 4] Testing CPU vs D3D12 WARP Parity: R2 Snapped Borders & Hairlines (125% & 150%)..." << std::endl;

    const uint32_t w = 256, h = 256;
    const TileGrid grid(w, h);
    TileBinner binner;

    std::vector<UiPrimitive> prims;
    // 1.25x snapped 1px horizontal hairline at y = [21, 22]
    {
        UiPrimitive p{};
        p.min_x = 20.0f; p.min_y = 21.0f; p.max_x = 100.0f; p.max_y = 22.0f;
        p.color_r = 1.0f; p.color_g = 1.0f; p.color_b = 1.0f; p.color_a = 1.0f;
        prims.push_back(p);
    }
    // 1.25x snapped 1px rect border [20, 50, 90, 90]
    {
        UiPrimitive p{};
        p.min_x = 20.0f; p.min_y = 50.0f; p.max_x = 90.0f; p.max_y = 90.0f;
        p.color_r = 1.0f; p.color_g = 1.0f; p.color_b = 1.0f; p.color_a = 1.0f;
        p.border_width = 1.0f;
        prims.push_back(p);
    }
    // 1.25x snapped 1px rounded border (r=6) at [110, 50, 180, 90]
    {
        UiPrimitive p{};
        p.min_x = 110.0f; p.min_y = 50.0f; p.max_x = 180.0f; p.max_y = 90.0f;
        p.radius_top_left = p.radius_top_right = p.radius_bottom_right = p.radius_bottom_left = 6.0f;
        p.color_r = 0.9f; p.color_g = 0.95f; p.color_b = 1.0f; p.color_a = 1.0f;
        p.border_width = 1.0f;
        prims.push_back(p);
    }
    // 1.5x snapped 2px vertical hairline at x = [140, 142]
    {
        UiPrimitive p{};
        p.min_x = 140.0f; p.min_y = 120.0f; p.max_x = 142.0f; p.max_y = 180.0f;
        p.color_r = 1.0f; p.color_g = 1.0f; p.color_b = 1.0f; p.color_a = 1.0f;
        prims.push_back(p);
    }
    // 1.5x snapped 2px rect border [20, 120, 90, 170]
    {
        UiPrimitive p{};
        p.min_x = 20.0f; p.min_y = 120.0f; p.max_x = 90.0f; p.max_y = 170.0f;
        p.color_r = 1.0f; p.color_g = 1.0f; p.color_b = 1.0f; p.color_a = 1.0f;
        p.border_width = 2.0f;
        prims.push_back(p);
    }

    auto bin_res = binner.bin_primitives(grid, prims);
    auto cpu_fb = render_cpu(w, h, prims, bin_res, {}, false, {}, false);
    auto gpu_fb = warp.render(w, h, prims, bin_res.indices, bin_res.headers, {}, false, {}, false);

    auto px_a = [&](const std::vector<uint8_t>& fb, uint32_t x, uint32_t y) {
        return fb[(static_cast<size_t>(y) * w + x) * 4 + 3];
    };

    for (const auto* fb_ptr : {&cpu_fb, &gpu_fb}) {
        const auto& fb = *fb_ptr;
        // 1.25x 1px horizontal hairline: y=21 opaque, y=20 and y=22 zero bleed
        KT_CHECK(px_a(fb, 50, 21) == 255);
        KT_CHECK(px_a(fb, 50, 20) == 0);
        KT_CHECK(px_a(fb, 50, 22) == 0);
        // 1.25x 1px border: y=50 opaque, y=49 and y=51 zero bleed
        KT_CHECK(px_a(fb, 50, 50) == 255);
        KT_CHECK(px_a(fb, 50, 49) == 0);
        KT_CHECK(px_a(fb, 50, 51) == 0);
        // 1.25x 1px rounded border: top straight edge opaque, corner (110, 50) rounded out (0)
        KT_CHECK(px_a(fb, 145, 50) == 255);
        KT_CHECK(px_a(fb, 110, 50) == 0);
        // 1.5x 2px vertical hairline: x=140,141 opaque, x=139 and x=142 zero bleed
        KT_CHECK(px_a(fb, 140, 150) == 255);
        KT_CHECK(px_a(fb, 141, 150) == 255);
        KT_CHECK(px_a(fb, 139, 150) == 0);
        KT_CHECK(px_a(fb, 142, 150) == 0);
        // 1.5x 2px rect border: y=120,121 opaque, y=119 and y=122 zero bleed
        KT_CHECK(px_a(fb, 50, 120) == 255);
        KT_CHECK(px_a(fb, 50, 121) == 255);
        KT_CHECK(px_a(fb, 50, 119) == 0);
        KT_CHECK(px_a(fb, 50, 122) == 0);
    }

    const auto comp = ImageCompare::compare_rgba8(cpu_fb.data(), gpu_fb.data(), w, h);
    KT_CHECK(comp.max_lsb_diff == 0);
    KT_CHECK(comp.differing_pixels == 0);
    KT_CHECK(comp.ssim == 1.0);

    std::cout << "  -> R2 Pixel Snapping CPU vs WARP parity PASSED (differing_pixels=0)." << std::endl;
}

void test_warp_parity_r3_pointer_reveal(WarpComputeRunner& warp) {
    std::cout << "[Test 5] Testing CPU vs D3D12 WARP Parity: R3 Pointer Reveal (Surface, Edge, Corner, Nested Clip)..." << std::endl;

    const uint32_t w = 512, h = 128;
    const TileGrid grid(w, h);
    TileBinner binner;

    ClipRect card_clip{};
    card_clip.min_x = 16.0f; card_clip.min_y = 16.0f;
    card_clip.max_x = 496.0f; card_clip.max_y = 96.0f;
    card_clip.corner_radius = 8.0f;
    card_clip.parent_index = INVALID_INDEX_U32;
    card_clip.flags = kClipFlagRevealBounds;

    auto make_reveal = [&](float cx, float cy, float r, float bw, uint16_t clip_idx) {
        UiPrimitive p{};
        p.min_x = std::max(card_clip.min_x, cx - r);
        p.min_y = std::max(card_clip.min_y, cy - r);
        p.max_x = std::min(card_clip.max_x, cx + r);
        p.max_y = std::min(card_clip.max_y, cy + r);
        p.radius_top_left = p.radius_top_right = p.radius_bottom_right = p.radius_bottom_left = 8.0f;
        p.color_r = 1.0f; p.color_g = 1.0f; p.color_b = 1.0f; p.color_a = 1.0f;
        p.border_width = bw;
        p.shadow_offset_x = cx;
        p.shadow_offset_y = cy;
        p.shadow_blur = r;
        p.shadow_spread = 1.0f;
        p.flags = static_cast<uint16_t>(PrimitiveFlags::IsPointerReveal);
        if (bw > 0.0f) {
            p.flags |= static_cast<uint16_t>(PrimitiveFlags::HasBorder);
        }
        p.clip_index = clip_idx;
        return p;
    };

    // 1. Surface reveal (256, 56), R = 48
    {
        std::vector<ClipRect> clips = {card_clip};
        std::vector<UiPrimitive> prims = {make_reveal(256.0f, 56.0f, 48.0f, 0.0f, 0)};
        auto bin_res = binner.bin_primitives(grid, prims, true, clips);
        auto cpu_fb = render_cpu(w, h, prims, bin_res, clips, false, {}, false);
        auto gpu_fb = warp.render(w, h, prims, bin_res.indices, bin_res.headers, clips, false, {}, false);

        const uint8_t a_peak = cpu_fb[(56u * w + 256u) * 4 + 3];
        const uint8_t a_mid  = cpu_fb[(56u * w + 280u) * 4 + 3];
        const uint8_t a_out  = cpu_fb[(56u * w + 310u) * 4 + 3];
        KT_CHECK(a_peak > 240);
        KT_CHECK(a_mid > 20 && a_mid < a_peak);
        KT_CHECK(a_out == 0);

        const auto comp = ImageCompare::compare_rgba8(cpu_fb.data(), gpu_fb.data(), w, h);
        KT_CHECK(comp.max_lsb_diff == 0);
        KT_CHECK(comp.differing_pixels == 0);
    }

    // 2. Edge reveal (256, 16), R = 48, border_width = 1 (no false interior borders)
    {
        std::vector<ClipRect> clips = {card_clip};
        std::vector<UiPrimitive> prims = {make_reveal(256.0f, 16.0f, 48.0f, 1.0f, 0)};
        auto bin_res = binner.bin_primitives(grid, prims, true, clips);
        auto cpu_fb = render_cpu(w, h, prims, bin_res, clips, false, {}, false);
        auto gpu_fb = warp.render(w, h, prims, bin_res.indices, bin_res.headers, clips, false, {}, false);

        KT_CHECK(cpu_fb[(16u * w + 256u) * 4 + 3] > 200);
        KT_CHECK(cpu_fb[(56u * w + 256u) * 4 + 3] == 0);
        KT_CHECK(cpu_fb[(56u * w + 208u) * 4 + 3] == 0);
        KT_CHECK(cpu_fb[(56u * w + 303u) * 4 + 3] == 0);

        const auto comp = ImageCompare::compare_rgba8(cpu_fb.data(), gpu_fb.data(), w, h);
        KT_CHECK(comp.max_lsb_diff == 0);
        KT_CHECK(comp.differing_pixels == 0);
    }

    // 3. Corner reveal (18, 18), R = 48: outer corner pixel (16, 16) excluded
    {
        std::vector<ClipRect> clips = {card_clip};
        std::vector<UiPrimitive> prims = {make_reveal(18.0f, 18.0f, 48.0f, 1.0f, 0)};
        auto bin_res = binner.bin_primitives(grid, prims, true, clips);
        auto cpu_fb = render_cpu(w, h, prims, bin_res, clips, false, {}, false);
        auto gpu_fb = warp.render(w, h, prims, bin_res.indices, bin_res.headers, clips, false, {}, false);

        KT_CHECK(cpu_fb[(16u * w + 16u) * 4 + 3] == 0);
        const auto comp = ImageCompare::compare_rgba8(cpu_fb.data(), gpu_fb.data(), w, h);
        KT_CHECK(comp.max_lsb_diff == 0);
        KT_CHECK(comp.differing_pixels == 0);
    }

    // 4. Nested ScrollView parent clip [230, 0, 290, 128] + child kClipFlagRevealBounds
    {
        ClipRect scroll_clip{};
        scroll_clip.min_x = 230.0f; scroll_clip.min_y = 0.0f;
        scroll_clip.max_x = 290.0f; scroll_clip.max_y = 128.0f;
        scroll_clip.corner_radius = 0.0f;
        scroll_clip.parent_index = INVALID_INDEX_U32;
        scroll_clip.flags = 0u;

        ClipRect child_reveal_clip = card_clip;
        child_reveal_clip.parent_index = 0;

        std::vector<ClipRect> clips = {scroll_clip, child_reveal_clip};
        std::vector<UiPrimitive> prims = {make_reveal(256.0f, 16.0f, 48.0f, 1.0f, 1)};
        auto bin_res = binner.bin_primitives(grid, prims, true, clips);
        auto cpu_fb = render_cpu(w, h, prims, bin_res, clips, false, {}, false);
        auto gpu_fb = warp.render(w, h, prims, bin_res.indices, bin_res.headers, clips, false, {}, false);

        KT_CHECK(cpu_fb[(16u * w + 225u) * 4 + 3] == 0);
        KT_CHECK(cpu_fb[(16u * w + 256u) * 4 + 3] > 200);
        const auto comp = ImageCompare::compare_rgba8(cpu_fb.data(), gpu_fb.data(), w, h);
        KT_CHECK(comp.max_lsb_diff == 0);
        KT_CHECK(comp.differing_pixels == 0);
    }

    std::cout << "  -> R3 Pointer Reveal CPU vs WARP parity PASSED (differing_pixels=0)." << std::endl;
}

#endif // _WIN32 && KT_CONFORMANCE_ENABLE_D3D12 && !KERNETILE_PURE_GDI

int main() {
    configure_test_environment();
    std::cout << "=== Phase 9 Backend Conformance & Parity Tests ===" << std::endl;

    test_cpu_conformance();
    test_static_cross_backend_shader_parity();

#if defined(_WIN32) && defined(KT_CONFORMANCE_ENABLE_D3D12) && !defined(KERNETILE_PURE_GDI)
    WarpComputeRunner warp;
    test_warp_parity_r1_ign_dithering(warp);
    test_warp_parity_r2_pixel_snapping(warp);
    test_warp_parity_r3_pointer_reveal(warp);
#else
    std::cout << "[INFO] D3D12 WARP parity tests skipped (non-Windows or Pure-GDI/no-compute build)." << std::endl;
#endif

    std::cout << "=== All Backend Conformance Tests PASSED (100%) ===" << std::endl;
    return 0;
}
