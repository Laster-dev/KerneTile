#pragma once

#include "e2e_test_common.hpp"
#include "kernetile/core/primitives.hpp"
#include "kernetile/core/tile_header.hpp"
#include "kernetile/tile/tile_grid.hpp"
#include "kernetile/tile/tile_binner.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "d3d12_device.hpp"
#include "d3d12_command_recorder.hpp"

#if __has_include("ui_compute_dxbc.hpp")
#include "ui_compute_dxbc.hpp"
#define KT_E2E_HAS_PRECOMPILED_DXBC 1
#else
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#define KT_E2E_HAS_PRECOMPILED_DXBC 0
#endif

namespace kernetile_e2e {

struct D3D12SceneInput {
    uint32_t width{128};
    uint32_t height{128};
    std::vector<kernetile::core::UiPrimitive> primitives;
    std::vector<kernetile::core::ClipRect> clips;
    bool mica_enabled{false};
    std::array<uint32_t, 16> mica_grid{};
    uint32_t render_flags{0}; // bit 0 = IGN_DITHERING_ENABLED
    std::vector<uint8_t> atlas_r8;
    uint32_t atlas_width{0};
    uint32_t atlas_height{0};
};

class D3D12OffscreenRunner {
public:
    D3D12OffscreenRunner() {
        m_device = std::make_unique<kernetile::hal::d3d12::D3D12Device>(true, true, false, nullptr);
        if (!m_device || !m_device->is_valid()) {
            m_device.reset();
            return;
        }
        init_pipeline();
    }

    [[nodiscard]] bool is_ready() const noexcept {
        return m_device && m_device->is_valid() && m_pipeline != nullptr;
    }

    [[nodiscard]] std::string_view device_name() const noexcept {
        return m_device ? m_device->get_device_name() : std::string_view{};
    }

    [[nodiscard]] std::vector<uint8_t> render(const D3D12SceneInput& scene) {
        if (!is_ready() || scene.width == 0 || scene.height == 0) {
            return {};
        }

        using namespace kernetile;
        using namespace kernetile::hal;
        using namespace kernetile::hal::d3d12;

        tile::TileGrid grid(scene.width, scene.height);
        tile::TileBinner binner;
        const auto& binning = binner.bin_primitives(grid, scene.primitives, true, scene.clips);

        struct ComputeTile {
            uint32_t x;
            uint32_t y;
            uint32_t offset;
            uint32_t count;
        };
        static_assert(sizeof(ComputeTile) == 16);

        const uint32_t tile_count = grid.get_total_tiles();
        std::vector<ComputeTile> compute_tiles;
        compute_tiles.reserve(tile_count);
        for (uint32_t id = 0; id < tile_count; ++id) {
            const auto coord = grid.get_tile_coord(id);
            const auto& hdr = binning.headers[id];
            compute_tiles.push_back(ComputeTile{coord.x, coord.y, hdr.primitive_offset, hdr.primitive_count});
        }

        auto make_upload_span = [&](std::span<const std::byte> bytes) -> std::unique_ptr<GpuBuffer> {
            const size_t alloc_sz = std::max<size_t>(sizeof(core::UiPrimitive), bytes.size());
            auto buf = m_device->create_buffer(BufferDesc{alloc_sz, BufferUsage::Upload});
            KT_E2E_CHECK(buf != nullptr);
            void* mapped = buf->map();
            KT_E2E_CHECK(mapped != nullptr);
            std::memset(mapped, 0, alloc_sz);
            if (!bytes.empty()) {
                std::memcpy(mapped, bytes.data(), bytes.size());
            }
            return buf;
        };

        auto prim_buf = make_upload_span(std::as_bytes(std::span{scene.primitives}));
        auto idx_buf = make_upload_span(std::as_bytes(std::span{binning.indices}));
        auto tile_buf = make_upload_span(std::as_bytes(std::span{compute_tiles}));
        auto clip_buf = make_upload_span(std::as_bytes(std::span{scene.clips}));

        const uint32_t atlas_w = (scene.atlas_width > 0 && scene.atlas_height > 0 && !scene.atlas_r8.empty())
            ? scene.atlas_width : 16u;
        const uint32_t atlas_h = (scene.atlas_width > 0 && scene.atlas_height > 0 && !scene.atlas_r8.empty())
            ? scene.atlas_height : 16u;
        std::vector<uint8_t> atlas_storage;
        const uint8_t* atlas_ptr = nullptr;
        if (scene.atlas_width > 0 && scene.atlas_height > 0 &&
            scene.atlas_r8.size() >= static_cast<size_t>(atlas_w) * atlas_h) {
            atlas_ptr = scene.atlas_r8.data();
        } else {
            atlas_storage.assign(static_cast<size_t>(atlas_w) * atlas_h, 0u);
            atlas_ptr = atlas_storage.data();
        }

        auto atlas_tex = m_device->create_texture(TextureDesc{atlas_w, atlas_h, TextureFormat::R8_UNORM, false});
        KT_E2E_CHECK(atlas_tex != nullptr);

        auto align_up = [](size_t v, size_t a) -> size_t {
            return (v + a - 1) & ~(a - 1);
        };
        const UINT atlas_pitch = static_cast<UINT>(align_up(atlas_w, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
        const size_t atlas_staging_bytes = align_up(static_cast<size_t>(atlas_pitch) * atlas_h,
                                                    D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        auto atlas_staging = m_device->create_buffer(BufferDesc{atlas_staging_bytes, BufferUsage::Upload});
        KT_E2E_CHECK(atlas_staging != nullptr);
        {
            auto* dst_bytes = static_cast<uint8_t*>(atlas_staging->map());
            KT_E2E_CHECK(dst_bytes != nullptr);
            for (uint32_t row = 0; row < atlas_h; ++row) {
                std::memcpy(dst_bytes + static_cast<size_t>(row) * atlas_pitch,
                            atlas_ptr + static_cast<size_t>(row) * atlas_w,
                            atlas_w);
            }
        }

        auto output_tex = m_device->create_texture(
            TextureDesc{scene.width, scene.height, TextureFormat::RGBA8_UNORM, true});
        KT_E2E_CHECK(output_tex != nullptr);

        auto* d3d_output = static_cast<D3D12Texture*>(output_tex.get());
        auto* d3d_atlas = static_cast<D3D12Texture*>(atlas_tex.get());
        auto* d3d_atlas_staging = static_cast<D3D12Buffer*>(atlas_staging.get());

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT num_rows = 0;
        UINT64 row_size_bytes = 0;
        UINT64 total_readback_bytes = 0;
        const D3D12_RESOURCE_DESC out_desc = d3d_output->get_native_resource()->GetDesc();
        m_device->get_native_device()->GetCopyableFootprints(
            &out_desc, 0, 1, 0, &footprint, &num_rows, &row_size_bytes, &total_readback_bytes);

        auto readback_buf = m_device->create_buffer(
            BufferDesc{static_cast<size_t>(total_readback_bytes), BufferUsage::Readback});
        KT_E2E_CHECK(readback_buf != nullptr);

        auto recorder = m_device->create_command_recorder();
        KT_E2E_CHECK(recorder != nullptr);
        auto* cmd = static_cast<D3D12CommandRecorder*>(recorder.get());
        KT_E2E_CHECK(cmd->is_valid());

        // Upload R8 atlas
        cmd->resource_barrier(*atlas_tex, atlas_tex->get_current_state(), ResourceState::CopyDst);
        {
            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource = d3d_atlas_staging->get_native_resource();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint.Offset = 0;
            src.PlacedFootprint.Footprint = {DXGI_FORMAT_R8_UNORM, atlas_w, atlas_h, 1, atlas_pitch};

            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource = d3d_atlas->get_native_resource();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = 0;

            cmd->get_native_list()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
        cmd->resource_barrier(*atlas_tex, ResourceState::CopyDst, ResourceState::ComputeShaderResource);

        // Bind compute pipeline & 7 root signature parameters
        cmd->set_compute_pipeline(*m_pipeline);
        cmd->set_storage_buffer(0, *prim_buf);
        cmd->set_storage_buffer(1, *idx_buf);
        cmd->set_storage_buffer(2, *tile_buf);
        cmd->set_storage_buffer(3, *clip_buf);
        cmd->resource_barrier(*output_tex, output_tex->get_current_state(), ResourceState::UnorderedAccess);
        cmd->set_storage_texture(0, *output_tex);
        cmd->set_compute_atlases(*d3d_atlas, *d3d_atlas);

        const uint32_t groups_x = std::min(tile_count, 1024u);
        std::array<uint32_t, 24> parameters{};
        parameters[0] = scene.width;
        parameters[1] = scene.height;
        parameters[2] = tile_count;
        parameters[3] = groups_x;
        parameters[4] = static_cast<uint32_t>(scene.clips.size());
        parameters[5] = scene.mica_enabled ? 1u : 0u;
        parameters[6] = scene.render_flags;
        parameters[7] = 0u;
        for (size_t i = 0; i < 16; ++i) {
            parameters[8 + i] = scene.mica_grid[i];
        }
        cmd->get_native_list()->SetComputeRoot32BitConstants(
            6, static_cast<UINT>(parameters.size()), parameters.data(), 0);

        cmd->dispatch(groups_x, (tile_count + groups_x - 1) / groups_x, 1);
        cmd->copy_texture_to_buffer(*output_tex, *readback_buf);
        cmd->close();

        m_device->submit(std::move(recorder));
        m_device->wait_idle();

        const uint8_t* mapped_read = static_cast<const uint8_t*>(readback_buf->map());
        KT_E2E_CHECK(mapped_read != nullptr);

        std::vector<uint8_t> rgba(static_cast<size_t>(scene.width) * scene.height * 4u, 0u);
        const size_t row_bytes = static_cast<size_t>(scene.width) * 4u;
        const size_t pitch = static_cast<size_t>(footprint.Footprint.RowPitch);
        for (uint32_t y = 0; y < scene.height; ++y) {
            std::memcpy(rgba.data() + static_cast<size_t>(y) * row_bytes,
                        mapped_read + footprint.Offset + static_cast<size_t>(y) * pitch,
                        row_bytes);
        }
        readback_buf->unmap();
        return rgba;
    }

private:
    void init_pipeline() {
#if KT_E2E_HAS_PRECOMPILED_DXBC
        m_pipeline = m_device->create_compute_pipeline({ui_compute_dxbc, sizeof(ui_compute_dxbc)});
#else
        const std::array<const wchar_t*, 3> paths = {
            L"shaders/ui_compute_d3d12.hlsl",
            L"../shaders/ui_compute_d3d12.hlsl",
            L"d:/KerneTile/shaders/ui_compute_d3d12.hlsl"
        };
        kernetile::hal::d3d12::ComPtr<ID3DBlob> binary;
        kernetile::hal::d3d12::ComPtr<ID3DBlob> errors;
        HRESULT hr = E_FAIL;
        for (const wchar_t* p : paths) {
            errors.Reset();
            hr = D3DCompileFromFile(
                p, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                "compose", "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                &binary, &errors);
            if (SUCCEEDED(hr)) {
                break;
            }
        }
        if (FAILED(hr) && errors) {
            std::fprintf(stderr, "D3DCompileFromFile error: %s\n",
                         static_cast<const char*>(errors->GetBufferPointer()));
        }
        if (SUCCEEDED(hr) && binary) {
            m_pipeline = m_device->create_compute_pipeline({
                static_cast<const uint8_t*>(binary->GetBufferPointer()),
                binary->GetBufferSize()
            });
        }
#endif
    }

    std::unique_ptr<kernetile::hal::d3d12::D3D12Device> m_device;
    std::unique_ptr<kernetile::hal::ComputePipeline> m_pipeline;
};

} // namespace kernetile_e2e
#endif // defined(_WIN32) && !defined(KERNETILE_PURE_GDI) && !defined(KERNETILE_WINDOWS_PURE_GDI)

namespace kernetile_e2e {

struct ShaderParityAudit {
    bool hlsl_found{false};
    bool wgsl_found{false};
    bool hlsl_has_vk_path{false};
    bool hlsl_has_ign_constants{false};
    bool hlsl_has_render_flags{false};
    bool hlsl_has_pointer_reveal{false};
    bool wgsl_has_ign_constants{false};
    bool wgsl_has_render_flags{false};
    bool wgsl_has_pointer_reveal{false};
};

inline ShaderParityAudit audit_shader_files() {
    ShaderParityAudit res{};
    const std::string hlsl = read_workspace_file("shaders/ui_compute_d3d12.hlsl");
    const std::string wgsl = read_workspace_file("shaders/ui_compute.wgsl");

    res.hlsl_found = !hlsl.empty();
    res.wgsl_found = !wgsl.empty();

    auto has_ign = [](const std::string& src) {
        return src.find("52.9829189") != std::string::npos &&
               src.find("0.06711056") != std::string::npos &&
               src.find("0.00583715") != std::string::npos;
    };

    auto has_reveal_flag = [](const std::string& src) {
        return src.find("512") != std::string::npos ||
               src.find("0x0200") != std::string::npos ||
               src.find("0x200") != std::string::npos;
    };

    auto has_render_flags = [](const std::string& src) {
        return src.find("render_flags") != std::string::npos;
    };

    if (res.hlsl_found) {
        res.hlsl_has_vk_path = (hlsl.find("KT_VULKAN") != std::string::npos) &&
                               (hlsl.find("vk::binding") != std::string::npos);
        res.hlsl_has_ign_constants = has_ign(hlsl);
        res.hlsl_has_render_flags = has_render_flags(hlsl);
        res.hlsl_has_pointer_reveal = has_reveal_flag(hlsl);
    }
    if (res.wgsl_found) {
        res.wgsl_has_ign_constants = has_ign(wgsl);
        res.wgsl_has_render_flags = has_render_flags(wgsl);
        res.wgsl_has_pointer_reveal = has_reveal_flag(wgsl);
    }
    return res;
}

} // namespace kernetile_e2e
