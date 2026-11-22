#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ui_compute.hpp"
#include "ui_compute_shader.hpp"
#include "d3d12_buffer.hpp"
#include "d3d12_command_recorder.hpp"
#include "d3d12_device.hpp"

#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cstring>

namespace kernetile::compute {
namespace {
using namespace hal;
using namespace hal::d3d12;

class D3D12Backend final : public Backend {
public:
    explicit D3D12Backend(void* window) : m_window(window), m_device(false, false) {
        ComPtr<ID3DBlob> binary, errors;
        const auto status = D3DCompile(shader_source, sizeof(shader_source) - 1, "ui_compute_d3d12.hlsl",
            nullptr, nullptr, "compose", "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &binary, &errors);
        if (FAILED(status) && errors) {
            std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << '\n';
        }
        throw_if_failed(status, "Compile UI tile shader");
        m_pipeline = m_device.create_compute_pipeline(
            {static_cast<const uint8_t*>(binary->GetBufferPointer()), binary->GetBufferSize()});
    }

    ~D3D12Backend() override {
        try { m_device.wait_idle(); } catch (const std::exception&) {}
    }

    bool ready() const noexcept override { return !m_failed; }

    bool submit(const Frame& frame) override {
        try {
            m_device.wait_idle();
            resize(frame);
            auto recorder = m_device.create_command_recorder();
            auto& commands = static_cast<D3D12CommandRecorder&>(*recorder);
            if (!frame.tiles.empty()) compose(commands, frame);
            auto& back = *static_cast<D3D12Swapchain&>(*m_swapchain).get_current_back_buffer();
            commands.resource_barrier(*m_output, m_output->get_current_state(), ResourceState::CopySrc);
            commands.resource_barrier(back, ResourceState::Present, ResourceState::CopyDst);
            commands.get_native_list()->CopyResource(back.get_native_resource(), m_output->get_native_resource());
            commands.resource_barrier(back, ResourceState::CopyDst, ResourceState::Present);
            commands.close();
            m_device.submit(std::move(recorder));
            m_swapchain->present(false);
            return true;
        } catch (const std::exception&) {
            m_failed = true;
            return false;
        }
    }

private:
    void resize(const Frame& frame) {
        if (!m_swapchain) {
            m_swapchain = m_device.create_swapchain(m_window, frame.width, frame.height);
        } else if (m_swapchain->get_width() != frame.width || m_swapchain->get_height() != frame.height) {
            m_swapchain->resize(frame.width, frame.height);
            m_output.reset();
        }
        if (!m_output) m_output = std::make_unique<D3D12Texture>(m_device.get_native_device(),
            TextureDesc{frame.width, frame.height, TextureFormat::RGBA8_UNORM, true});
    }

    D3D12Buffer& upload_buffer(size_t slot, size_t size) {
        auto& buffer = m_uploads[slot];
        if (!buffer || buffer->get_size_bytes() < size) {
            buffer = std::make_unique<D3D12Buffer>(m_device.get_native_device(),
                BufferDesc{size, BufferUsage::Upload});
        }
        return *buffer;
    }

    void compose(D3D12CommandRecorder& commands, const Frame& frame) {
        const bool first_atlas = !m_atlas;
        if (first_atlas) m_atlas = std::make_unique<D3D12Texture>(m_device.get_native_device(),
            TextureDesc{frame.atlas_width, frame.atlas_height, TextureFormat::R8_UNORM, false});

        if (first_atlas || !frame.atlas_updates.empty()) {
            const atlas::RectU32 full{0, 0, frame.atlas_width, frame.atlas_height};
            const auto rects = first_atlas ? std::span(&full, 1) : frame.atlas_updates;
            const auto aligned = [](size_t value, size_t alignment) {
                return (value + alignment - 1) & ~(alignment - 1);
            };
            size_t total = 0;
            for (const auto& rect : rects) {
                total += aligned(aligned(rect.w, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * rect.h,
                                 D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
            }
            auto& upload = upload_buffer(4, total);
            auto* bytes = static_cast<uint8_t*>(upload.map());
            commands.resource_barrier(*m_atlas, m_atlas->get_current_state(), ResourceState::CopyDst);
            size_t offset = 0;
            for (const auto& rect : rects) {
                const auto pitch = static_cast<UINT>(aligned(rect.w, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
                for (uint32_t row = 0; row < rect.h; ++row) {
                    const size_t source = size_t(rect.y + row) * frame.atlas_width + rect.x;
                    std::memcpy(bytes + offset + size_t(row) * pitch, frame.atlas.data() + source, rect.w);
                }
                D3D12_TEXTURE_COPY_LOCATION src{upload.get_native_resource(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
                src.PlacedFootprint.Offset = offset;
                src.PlacedFootprint.Footprint = {DXGI_FORMAT_R8_UNORM, rect.w, rect.h, 1, pitch};
                D3D12_TEXTURE_COPY_LOCATION dst{m_atlas->get_native_resource(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
                commands.get_native_list()->CopyTextureRegion(&dst, rect.x, rect.y, 0, &src, nullptr);
                offset += aligned(size_t(pitch) * rect.h, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
            }
            commands.resource_barrier(*m_atlas, ResourceState::CopyDst, ResourceState::ComputeShaderResource);
        }

        commands.set_compute_pipeline(*m_pipeline);
        const std::array input{std::as_bytes(frame.primitives), std::as_bytes(frame.indices),
                               std::as_bytes(frame.tiles), std::as_bytes(frame.clips)};
        for (uint32_t i = 0; i < input.size(); ++i) {
            auto& buffer = upload_buffer(i, std::max<size_t>(sizeof(core::UiPrimitive), input[i].size()));
            if (!input[i].empty()) std::memcpy(buffer.map(), input[i].data(), input[i].size());
            commands.set_storage_buffer(i, buffer);
        }
        commands.resource_barrier(*m_output, m_output->get_current_state(), ResourceState::UnorderedAccess);
        commands.set_storage_texture(0, *m_output);
        commands.set_compute_atlas(*m_atlas);
        const auto count = static_cast<uint32_t>(frame.tiles.size());
        const auto groups_x = std::min(count, 1024u);
        const std::array parameters{frame.width, frame.height, count, groups_x, static_cast<uint32_t>(frame.clips.size())};
        commands.get_native_list()->SetComputeRoot32BitConstants(6, static_cast<UINT>(parameters.size()), parameters.data(), 0);
        commands.dispatch(groups_x, (count + groups_x - 1) / groups_x, 1);
    }

    void* m_window;
    D3D12Device m_device;
    std::unique_ptr<ComputePipeline> m_pipeline;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<D3D12Texture> m_output, m_atlas;
    std::array<std::unique_ptr<D3D12Buffer>, 5> m_uploads;
    bool m_failed = false;
};
} // namespace

std::unique_ptr<Backend> create(void* window, void*) {
    if (!window) return nullptr;
    try { return std::make_unique<D3D12Backend>(window); }
    catch (const std::exception&) { return nullptr; }
}

} // namespace kernetile::compute
