#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ui_compute.hpp"
#if defined(KT_PRECOMPILED_DXBC)
#include "ui_compute_dxbc.hpp"
#else
#include "ui_compute_shader.hpp"
#include <d3dcompiler.h>
#endif
#include "d3d12_buffer.hpp"
#include "d3d12_command_recorder.hpp"
#include "d3d12_device.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace kernetile::compute {
namespace {
using namespace hal;
using namespace hal::d3d12;

class D3D12Backend final : public Backend {
public:
    static constexpr size_t kFramesInFlight = 2;

    explicit D3D12Backend(void* window, bool prefer_warp)
        : m_window(window), m_device(prefer_warp, prefer_warp, false) {
        if (!m_device.is_valid()) {
            m_failed = true;
            return;
        }
        for (auto& slot : m_slots) {
            if (FAILED(m_device.get_native_device()->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slot.allocator)))) {
                m_failed = true;
                return;
            }
        }
#if defined(KT_PRECOMPILED_DXBC)
        m_pipeline = m_device.create_compute_pipeline({ui_compute_dxbc, sizeof(ui_compute_dxbc)});
#else
        ComPtr<ID3DBlob> binary, errors;
        const auto status = D3DCompile(shader_source, sizeof(shader_source) - 1, "ui_compute_d3d12.hlsl",
            nullptr, nullptr, "compose", "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &binary, &errors);
        if (FAILED(status) && errors) {
            std::fprintf(stderr, "%s\n", static_cast<const char*>(errors->GetBufferPointer()));
        }
        throw_if_failed(status, "Compile UI tile shader");
        m_pipeline = m_device.create_compute_pipeline(
            {static_cast<const uint8_t*>(binary->GetBufferPointer()), binary->GetBufferSize()});
#endif
    }

    ~D3D12Backend() override {
        if (m_device.is_valid()) m_device.wait_idle();
    }

    bool ready() const noexcept override { return !m_failed && m_device.is_valid() && m_pipeline != nullptr; }

    bool submit(const Frame& frame) override {
        if (!ready() || frame.width == 0 || frame.height == 0) return false;
        auto& slot = m_slots[m_frame_slot];
        if (!slot.allocator) return false;
        // 仅等待当前帧槽对应的 GPU Fence 完成，实现 2 帧并发流水线而非每帧全队列 wait_idle
        m_device.wait_fence(slot.fence_value);
        if (FAILED(slot.allocator->Reset())) {
            return false;
        }
        resize(frame);
        if (!m_swapchain || !m_output || !m_output->get_native_resource()) {
            return false;
        }
        auto* back_ptr = static_cast<D3D12Swapchain&>(*m_swapchain).get_current_back_buffer();
        if (!back_ptr || !back_ptr->get_native_resource()) {
            return false;
        }
        // 每帧槽复用同一个 ID3D12GraphicsCommandList，并隔离着色器可见描述符槽位，彻底杜绝并发帧写覆盖与句柄耗尽
        if (!slot.recorder) {
            slot.recorder = m_device.create_command_recorder(slot.allocator.Get());
            if (!slot.recorder || !static_cast<D3D12CommandRecorder&>(*slot.recorder).is_valid()) {
                slot.recorder.reset();
                return false;
            }
        } else {
            if (!static_cast<D3D12CommandRecorder&>(*slot.recorder).reset(slot.allocator.Get())) {
                return false;
            }
        }
        auto& commands = static_cast<D3D12CommandRecorder&>(*slot.recorder);
        commands.set_descriptor_base(static_cast<uint32_t>(m_frame_slot * 16));
        if (!frame.tiles.empty()) compose(commands, slot, frame);
        auto& back = *back_ptr;
        commands.resource_barrier(*m_output, m_output->get_current_state(), ResourceState::CopySrc);
        commands.resource_barrier(back, ResourceState::Present, ResourceState::CopyDst);
        commands.get_native_list()->CopyResource(back.get_native_resource(), m_output->get_native_resource());
        commands.resource_barrier(back, ResourceState::CopyDst, ResourceState::Present);
        commands.close();
        m_device.execute_command_list(commands.get_native_list());
        m_swapchain->present(false);
        if (FAILED(m_device.get_native_device()->GetDeviceRemovedReason())) {
            return false;
        }
        slot.fence_value = m_device.signal_fence();
        m_frame_slot = (m_frame_slot + 1) % kFramesInFlight;
        return true;
    }

private:
    struct FrameSlot {
        ComPtr<ID3D12CommandAllocator> allocator;
        std::unique_ptr<CommandRecorder> recorder;
        std::array<std::unique_ptr<D3D12Buffer>, 4> uploads;
        std::unique_ptr<D3D12Buffer> atlas_upload;
        uint64_t fence_value{0};
    };

    void wait_all_frames() {
        for (const auto& slot : m_slots) {
            m_device.wait_fence(slot.fence_value);
        }
    }

    void resize(const Frame& frame) {
        if (!m_swapchain) {
            m_swapchain = m_device.create_swapchain(m_window, frame.width, frame.height);
        } else if (m_swapchain->get_width() != frame.width || m_swapchain->get_height() != frame.height) {
            wait_all_frames();
            m_swapchain->resize(frame.width, frame.height);
            m_output.reset();
        }
        if (!m_output) m_output = std::make_unique<D3D12Texture>(m_device.get_native_device(),
            TextureDesc{frame.width, frame.height, TextureFormat::RGBA8_UNORM, true});
    }

    D3D12Buffer& upload_buffer(FrameSlot& slot, size_t index, size_t size) {
        auto& buffer = slot.uploads[index];
        if (!buffer || buffer->get_size_bytes() < size) {
            buffer = std::make_unique<D3D12Buffer>(m_device.get_native_device(),
                BufferDesc{size, BufferUsage::Upload});
        }
        return *buffer;
    }

    D3D12Buffer& atlas_upload_buffer(FrameSlot& slot, size_t size) {
        auto& buffer = slot.atlas_upload;
        if (!buffer || buffer->get_size_bytes() < size) {
            buffer = std::make_unique<D3D12Buffer>(m_device.get_native_device(),
                BufferDesc{std::max<size_t>(256, size), BufferUsage::Upload});
        }
        return *buffer;
    }

    void compose(D3D12CommandRecorder& commands, FrameSlot& slot, const Frame& frame) {
        const bool first_atlas = !m_atlas;
        if (first_atlas) m_atlas = std::make_unique<D3D12Texture>(m_device.get_native_device(),
            TextureDesc{frame.atlas_width, frame.atlas_height, TextureFormat::R8_UNORM, false});

        using DirtyRect = std::remove_cvref_t<decltype(frame.atlas_updates)>::value_type;
        std::vector<DirtyRect> full_rect_storage;
        std::span<const DirtyRect> rects = frame.atlas_updates;
        if (first_atlas && frame.atlas_width > 0 && frame.atlas_height > 0 && !frame.atlas.empty()) {
            full_rect_storage.push_back(DirtyRect{0, 0, frame.atlas_width, frame.atlas_height});
            rects = full_rect_storage;
        }

        if (!rects.empty()) {
            const auto aligned = [](size_t value, size_t alignment) {
                return (value + alignment - 1) & ~(alignment - 1);
            };
            size_t total = 0;
            for (const auto& rect : rects) {
                total += aligned(aligned(rect.w, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * rect.h,
                                 D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
            }
            auto& staging = atlas_upload_buffer(slot, total);
            auto* bytes = static_cast<uint8_t*>(staging.map());
            commands.resource_barrier(*m_atlas, m_atlas->get_current_state(), ResourceState::CopyDst);
            size_t offset = 0;
            for (const auto& rect : rects) {
                const auto pitch = static_cast<UINT>(aligned(rect.w, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
                for (uint32_t row = 0; row < rect.h; ++row) {
                    const size_t source = size_t(rect.y + row) * frame.atlas_width + rect.x;
                    std::memcpy(bytes + offset + size_t(row) * pitch, frame.atlas.data() + source, rect.w);
                }
                D3D12_TEXTURE_COPY_LOCATION src{staging.get_native_resource(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
                src.PlacedFootprint.Offset = offset;
                src.PlacedFootprint.Footprint = {DXGI_FORMAT_R8_UNORM, rect.w, rect.h, 1, pitch};
                D3D12_TEXTURE_COPY_LOCATION dst{m_atlas->get_native_resource(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
                commands.get_native_list()->CopyTextureRegion(&dst, rect.x, rect.y, 0, &src, nullptr);
                offset += aligned(size_t(pitch) * rect.h, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
            }
            commands.resource_barrier(*m_atlas, ResourceState::CopyDst, ResourceState::ComputeShaderResource);
        } else if (first_atlas) {
            commands.resource_barrier(*m_atlas, m_atlas->get_current_state(), ResourceState::ComputeShaderResource);
        }

        commands.set_compute_pipeline(*m_pipeline);
        const std::array input{std::as_bytes(frame.primitives), std::as_bytes(frame.indices),
                               std::as_bytes(frame.tiles), std::as_bytes(frame.clips)};
        for (uint32_t i = 0; i < input.size(); ++i) {
            auto& buffer = upload_buffer(slot, i, std::max<size_t>(sizeof(core::UiPrimitive), input[i].size()));
            if (!input[i].empty()) std::memcpy(buffer.map(), input[i].data(), input[i].size());
            commands.set_storage_buffer(i, buffer);
        }
        commands.resource_barrier(*m_output, m_output->get_current_state(), ResourceState::UnorderedAccess);
        commands.set_storage_texture(0, *m_output);
        commands.set_compute_atlas(*m_atlas);
        const auto count = static_cast<uint32_t>(frame.tiles.size());
        const auto groups_x = std::min(count, 1024u);
        std::array<uint32_t, 24> parameters{};
        parameters[0] = frame.width;
        parameters[1] = frame.height;
        parameters[2] = count;
        parameters[3] = groups_x;
        parameters[4] = static_cast<uint32_t>(frame.clips.size());
        parameters[5] = frame.mica_enabled ? 1u : 0u;
        for (size_t i = 0; i < 16; ++i) parameters[8 + i] = frame.mica_grid[i];
        commands.get_native_list()->SetComputeRoot32BitConstants(6, static_cast<UINT>(parameters.size()), parameters.data(), 0);
        commands.dispatch(groups_x, (count + groups_x - 1) / groups_x, 1);
    }

    void* m_window;
    D3D12Device m_device;
    std::unique_ptr<ComputePipeline> m_pipeline;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<D3D12Texture> m_output, m_atlas;
    std::array<FrameSlot, kFramesInFlight> m_slots;
    size_t m_frame_slot{0};
    bool m_failed = false;
};
} // namespace

std::unique_ptr<Backend> create(void* window, void*, bool prefer_warp) {
    if (!window) return nullptr;
    auto backend = std::make_unique<D3D12Backend>(window, prefer_warp);
    return backend->ready() ? std::move(backend) : nullptr;
}

} // namespace kernetile::compute
