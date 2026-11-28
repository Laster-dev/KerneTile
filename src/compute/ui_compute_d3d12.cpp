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
        : m_window(window), m_device(prefer_warp, prefer_warp, false, window) {
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
            KT_LOG_ERR("D3D12 Shader: %s", static_cast<const char*>(errors->GetBufferPointer()));
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
        commands.set_descriptor_base(static_cast<uint32_t>(m_frame_slot * 128));
        if (!frame.tiles.empty()) compose(commands, slot, frame);
        auto& back = *back_ptr;
        commands.resource_barrier(*m_output, m_output->get_current_state(), ResourceState::CopySrc);
        commands.resource_barrier(back, ResourceState::Present, ResourceState::CopyDst);
        if (m_output->get_width() == frame.width && m_output->get_height() == frame.height) {
            commands.get_native_list()->CopyResource(back.get_native_resource(), m_output->get_native_resource());
        } else {
            D3D12_TEXTURE_COPY_LOCATION src{m_output->get_native_resource(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
            D3D12_TEXTURE_COPY_LOCATION dst{back.get_native_resource(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
            D3D12_BOX box{0u, 0u, 0u, frame.width, frame.height, 1u};
            commands.get_native_list()->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
        }
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
        std::unique_ptr<D3D12Buffer> color_atlas_upload;
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
        }
        // 按 256px 步长向上对齐容量复用 m_output UAV 纹理，避免拖动缩放窗口时每变化 1px 就销毁重建 GPU 纹理
        const bool need_output = !m_output ||
            m_output->get_width() < frame.width ||
            m_output->get_height() < frame.height;
        if (need_output) {
            if (m_output) wait_all_frames();
            const uint32_t cap_w = ((frame.width + 255u) / 256u) * 256u;
            const uint32_t cap_h = ((frame.height + 255u) / 256u) * 256u;
            m_output = std::make_unique<D3D12Texture>(m_device.get_native_device(),
                TextureDesc{cap_w, cap_h, TextureFormat::RGBA8_UNORM, true});
            m_scene.reset();
        }
        const uint32_t bw = std::max(1u, (frame.width + 3u) / 4u);
        const uint32_t bh = std::max(1u, (frame.height + 3u) / 4u);
        if (m_blur_a && (m_blur_a->get_width() != bw || m_blur_a->get_height() != bh)) {
            wait_all_frames();
            m_blur_a.reset();
            m_blur_b.reset();
        }
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

    D3D12Buffer& color_atlas_upload_buffer(FrameSlot& slot, size_t size) {
        auto& buffer = slot.color_atlas_upload;
        if (!buffer || buffer->get_size_bytes() < size) {
            buffer = std::make_unique<D3D12Buffer>(m_device.get_native_device(),
                BufferDesc{std::max<size_t>(256, size), BufferUsage::Upload});
        }
        return *buffer;
    }

    void compose(D3D12CommandRecorder& commands, FrameSlot& slot, const Frame& frame) {
        const bool recreate_atlas = !m_atlas ||
            m_atlas->get_desc().width != frame.atlas_width ||
            m_atlas->get_desc().height != frame.atlas_height;
        if (recreate_atlas) {
            if (m_atlas) m_device.wait_idle();
            m_atlas = std::make_unique<D3D12Texture>(m_device.get_native_device(),
                TextureDesc{frame.atlas_width, frame.atlas_height, TextureFormat::R8_UNORM, false});
        }

        using DirtyRect = std::remove_cvref_t<decltype(frame.atlas_updates)>::value_type;
        std::vector<DirtyRect> full_rect_storage;
        std::span<const DirtyRect> rects = frame.atlas_updates;
        if (recreate_atlas && frame.atlas_width > 0 && frame.atlas_height > 0 && !frame.atlas.empty()) {
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
        } else if (recreate_atlas) {
            commands.resource_barrier(*m_atlas, m_atlas->get_current_state(), ResourceState::ComputeShaderResource);
        }

        // 彩色 RGBA8 图集按需惰性创建与增量上传 (未调用 draw_image 时零显存分配，t5 直接复用 m_atlas 占位)
        const bool has_color_atlas = (frame.color_atlas_width > 0 && frame.color_atlas_height > 0 && !frame.color_atlas.empty());
        if (has_color_atlas) {
            const uint32_t ca_w = frame.color_atlas_width;
            const uint32_t ca_h = frame.color_atlas_height;
            const bool recreate_color = !m_color_atlas ||
                m_color_atlas->get_width() != ca_w || m_color_atlas->get_height() != ca_h;
            if (recreate_color) {
                if (m_color_atlas) wait_all_frames();
                m_color_atlas = std::make_unique<D3D12Texture>(m_device.get_native_device(),
                    TextureDesc{ca_w, ca_h, TextureFormat::RGBA8_UNORM, false});
            }
            std::vector<DirtyRect> full_color_rect_storage;
            std::span<const DirtyRect> color_rects = frame.color_atlas_updates;
            if (recreate_color) {
                full_color_rect_storage.push_back(DirtyRect{0, 0, ca_w, ca_h});
                color_rects = full_color_rect_storage;
            }
            if (!color_rects.empty()) {
                const auto aligned = [](size_t value, size_t alignment) {
                    return (value + alignment - 1) & ~(alignment - 1);
                };
                size_t total = 0;
                for (const auto& rect : color_rects) {
                    total += aligned(aligned(size_t(rect.w) * 4u, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * rect.h,
                                     D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
                }
                auto& staging = color_atlas_upload_buffer(slot, total);
                auto* bytes = static_cast<uint8_t*>(staging.map());
                commands.resource_barrier(*m_color_atlas, m_color_atlas->get_current_state(), ResourceState::CopyDst);
                size_t offset = 0;
                for (const auto& rect : color_rects) {
                    const auto row_bytes = size_t(rect.w) * 4u;
                    const auto pitch = static_cast<UINT>(aligned(row_bytes, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
                    for (uint32_t row = 0; row < rect.h; ++row) {
                        const size_t source = (size_t(rect.y + row) * ca_w + rect.x) * 4u;
                        std::memcpy(bytes + offset + size_t(row) * pitch, frame.color_atlas.data() + source, row_bytes);
                    }
                    D3D12_TEXTURE_COPY_LOCATION src{staging.get_native_resource(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
                    src.PlacedFootprint.Offset = offset;
                    src.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, rect.w, rect.h, 1, pitch};
                    D3D12_TEXTURE_COPY_LOCATION dst{m_color_atlas->get_native_resource(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
                    commands.get_native_list()->CopyTextureRegion(&dst, rect.x, rect.y, 0, &src, nullptr);
                    offset += aligned(size_t(pitch) * rect.h, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
                }
                commands.resource_barrier(*m_color_atlas, ResourceState::CopyDst, ResourceState::ComputeShaderResource);
            } else if (recreate_color) {
                commands.resource_barrier(*m_color_atlas, m_color_atlas->get_current_state(), ResourceState::ComputeShaderResource);
            }
        }

        commands.set_compute_pipeline(*m_pipeline);
        const std::array input{std::as_bytes(frame.primitives), std::as_bytes(frame.indices),
                               std::as_bytes(frame.tiles), std::as_bytes(frame.clips)};
        for (uint32_t i = 0; i < input.size(); ++i) {
            auto& buffer = upload_buffer(slot, i, std::max<size_t>(sizeof(core::UiPrimitive), input[i].size()));
            if (!input[i].empty()) std::memcpy(buffer.map(), input[i].data(), input[i].size());
            commands.set_storage_buffer(i, buffer);
        }
        const auto count = static_cast<uint32_t>(frame.tiles.size());
        const auto groups_x = std::min(count, 1024u);
        const auto groups_y = (count + groups_x - 1) / groups_x;
        std::array<uint32_t, 24> parameters{};
        parameters[0] = frame.width;
        parameters[1] = frame.height;
        parameters[2] = count;
        parameters[3] = groups_x;
        parameters[4] = static_cast<uint32_t>(frame.clips.size());
        parameters[5] = frame.mica_enabled ? 1u : 0u;
        parameters[6] = frame.render_flags;
        for (size_t i = 0; i < 16; ++i) parameters[8 + i] = frame.mica_grid[i];

        uint32_t backdrop_split = 0xFFFFFFFFu;
        for (uint32_t i = 0; i < static_cast<uint32_t>(frame.primitives.size()); ++i) {
            if ((frame.primitives[i].flags & static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter)) != 0) {
                backdrop_split = i;
                break;
            }
        }

        const uint32_t base_desc = static_cast<uint32_t>(m_frame_slot * 128);
        const D3D12Texture& ca_tex = m_color_atlas ? *m_color_atlas : *m_atlas;

        if (backdrop_split == 0xFFFFFFFFu) {
            commands.set_descriptor_base(base_desc);
            commands.resource_barrier(*m_output, m_output->get_current_state(), ResourceState::UnorderedAccess);
            commands.set_storage_texture(0, *m_output);
            commands.set_compute_atlases(*m_atlas, ca_tex);
            parameters[7] = 0u;
            commands.get_native_list()->SetComputeRoot32BitConstants(6, static_cast<UINT>(parameters.size()), parameters.data(), 0);
            commands.dispatch(groups_x, groups_y, 1);
            return;
        }

        // 惰性分配背景场景与 1/4 分辨率模糊纹理 (无弹出层时零显存开销)
        if (!m_scene) {
            m_scene = std::make_unique<D3D12Texture>(m_device.get_native_device(),
                TextureDesc{m_output->get_width(), m_output->get_height(), TextureFormat::RGBA8_UNORM, true});
        }
        const uint32_t bw = std::max(1u, (frame.width + 3u) / 4u);
        const uint32_t bh = std::max(1u, (frame.height + 3u) / 4u);
        if (!m_blur_a) {
            m_blur_a = std::make_unique<D3D12Texture>(m_device.get_native_device(),
                TextureDesc{bw, bh, TextureFormat::RGBA8_UNORM, true});
            m_blur_b = std::make_unique<D3D12Texture>(m_device.get_native_device(),
                TextureDesc{bw, bh, TextureFormat::RGBA8_UNORM, true});
        }

        // Pass 1: 主场景 (prim_id < backdrop_split) 写入 m_scene
        commands.set_descriptor_base(base_desc + 0u);
        commands.resource_barrier(*m_scene, m_scene->get_current_state(), ResourceState::UnorderedAccess);
        commands.set_storage_texture(0, *m_scene);
        commands.set_compute_atlases(*m_atlas, ca_tex);
        parameters[7] = (1u << 28) | (backdrop_split & 0x0FFFFFFFu);
        commands.get_native_list()->SetComputeRoot32BitConstants(6, static_cast<UINT>(parameters.size()), parameters.data(), 0);
        commands.dispatch(groups_x, groups_y, 1);
        commands.resource_barrier(*m_scene, ResourceState::UnorderedAccess, ResourceState::ComputeShaderResource);

        // Pass 2: 双轮 1/4 分辨率 13-tap 可分离高斯重度背景模糊 (WinUI 3 Acrylic 级模糊)
        const uint32_t bgx = (bw + 15u) / 16u;
        const uint32_t bgy = (bh + 15u) / 16u;
        auto run_blur = [&](uint32_t desc_off, uint32_t mode, uint32_t is_scene_src, D3D12Texture& src, D3D12Texture& dst) {
            commands.set_descriptor_base(base_desc + desc_off);
            commands.resource_barrier(dst, dst.get_current_state(), ResourceState::UnorderedAccess);
            commands.set_storage_texture(0, dst);
            commands.set_compute_atlases(*m_atlas, ca_tex, &src, &src);
            parameters[7] = (mode << 28) | is_scene_src;
            commands.get_native_list()->SetComputeRoot32BitConstants(6, static_cast<UINT>(parameters.size()), parameters.data(), 0);
            commands.dispatch(bgx, bgy, 1);
            commands.resource_barrier(dst, ResourceState::UnorderedAccess, ResourceState::ComputeShaderResource);
        };
        run_blur(16u, 3u, 1u, *m_scene,  *m_blur_a);
        run_blur(32u, 4u, 0u, *m_blur_a, *m_blur_b);
        run_blur(48u, 3u, 0u, *m_blur_b, *m_blur_a);
        run_blur(64u, 4u, 0u, *m_blur_a, *m_blur_b);

        // Pass 3: 从 m_scene 恢复底层并在弹出层区域采样 m_blur_b 执行 WinUI 3 亚克力合成
        commands.set_descriptor_base(base_desc + 80u);
        commands.resource_barrier(*m_output, m_output->get_current_state(), ResourceState::UnorderedAccess);
        commands.set_storage_texture(0, *m_output);
        commands.set_compute_atlases(*m_atlas, ca_tex, m_scene.get(), m_blur_b.get());
        parameters[7] = (2u << 28) | (backdrop_split & 0x0FFFFFFFu);
        commands.get_native_list()->SetComputeRoot32BitConstants(6, static_cast<UINT>(parameters.size()), parameters.data(), 0);
        commands.dispatch(groups_x, groups_y, 1);
    }

    void* m_window;
    D3D12Device m_device;
    std::unique_ptr<ComputePipeline> m_pipeline;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<D3D12Texture> m_output, m_atlas, m_color_atlas;
    std::unique_ptr<D3D12Texture> m_scene, m_blur_a, m_blur_b;
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
