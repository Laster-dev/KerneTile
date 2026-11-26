#pragma once

#include "kernetile/hal/hal.hpp"
#include "d3d12_common.hpp"
#include "d3d12_buffer.hpp"
#include "d3d12_texture.hpp"
#include "d3d12_pipeline.hpp"
#include "d3d12_swapchain.hpp"

namespace kernetile::hal::d3d12 {

class D3D12CommandRecorder final : public CommandRecorder {
public:
    D3D12CommandRecorder(
        ID3D12Device* device,
        ID3D12CommandAllocator* allocator,
        ID3D12RootSignature* compute_root_sig,
        ID3D12RootSignature* graphics_root_sig,
        ID3D12PipelineState* blit_pso,
        ID3D12CommandSignature* dispatch_cmd_sig,
        ID3D12DescriptorHeap* srv_uav_heap,
        uint32_t srv_uav_descriptor_size)
        : m_device(device),
          m_allocator(allocator),
          m_compute_root_sig(compute_root_sig),
          m_graphics_root_sig(graphics_root_sig),
          m_blit_pso(blit_pso),
          m_dispatch_cmd_sig(dispatch_cmd_sig),
          m_srv_uav_heap(srv_uav_heap),
          m_srv_uav_descriptor_size(srv_uav_descriptor_size)
    {
        if (device && allocator && throw_if_failed(
                device->CreateCommandList(
                    0,
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    allocator,
                    nullptr,
                    IID_PPV_ARGS(&m_cmd_list)),
                "CreateCommandList failed")) {
            // 默认绑定 CBV/SRV/UAV 堆
            if (m_srv_uav_heap) {
                ID3D12DescriptorHeap* heaps[] = { m_srv_uav_heap };
                m_cmd_list->SetDescriptorHeaps(1, heaps);
            }
        }
    }

    [[nodiscard]] bool is_valid() const noexcept { return m_cmd_list != nullptr; }

    void set_descriptor_base(uint32_t base) noexcept { m_descriptor_base = base; }

    bool reset(ID3D12CommandAllocator* allocator) {
        if (!m_cmd_list || !allocator) return false;
        if (!throw_if_failed(m_cmd_list->Reset(allocator, nullptr), "CommandList Reset failed")) {
            return false;
        }
        if (m_srv_uav_heap) {
            ID3D12DescriptorHeap* heaps[] = { m_srv_uav_heap };
            m_cmd_list->SetDescriptorHeaps(1, heaps);
        }
        return true;
    }

    void resource_barrier(
        GpuTexture& texture, 
        ResourceState state_before, 
        ResourceState state_after) override
    {
        if (state_before == state_after) return;

        auto* d3d_tex = static_cast<D3D12Texture*>(&texture);
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = d3d_tex->get_native_resource();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = to_d3d12_resource_states(state_before);
        barrier.Transition.StateAfter = to_d3d12_resource_states(state_after);

        m_cmd_list->ResourceBarrier(1, &barrier);
        d3d_tex->set_current_state(state_after);
    }

    void set_compute_pipeline(const ComputePipeline& pipeline) override {
        const auto* d3d_pipeline = static_cast<const D3D12ComputePipeline*>(&pipeline);
        m_cmd_list->SetComputeRootSignature(d3d_pipeline->get_root_signature());
        m_cmd_list->SetPipelineState(d3d_pipeline->get_native_pso());
    }

    void set_storage_buffer(uint32_t slot, const GpuBuffer& buffer) override {
        // slot 0: Primitives (t0), slot 1: TileIndices (t1), slot 2: TileHeaders (t2)
        m_cmd_list->SetComputeRootShaderResourceView(slot, buffer.get_gpu_virtual_address());
    }

    void set_storage_texture(uint32_t slot, const GpuTexture& texture) override {
        auto* d3d_tex = const_cast<D3D12Texture*>(static_cast<const D3D12Texture*>(&texture));
        
        // 创建临时 UAV 描述符并写入槽位 (slot 0 对应根参数 4: u0)
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
        uav_desc.Format = to_dxgi_format(d3d_tex->get_format());
        uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        cpu_handle.ptr += (m_descriptor_base + slot + 10) * m_srv_uav_descriptor_size;
        m_device->CreateUnorderedAccessView(d3d_tex->get_native_resource(), nullptr, &uav_desc, cpu_handle);

        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        gpu_handle.ptr += (m_descriptor_base + slot + 10) * m_srv_uav_descriptor_size;
        m_cmd_list->SetComputeRootDescriptorTable(4, gpu_handle);
    }

    void set_compute_atlas(const D3D12Texture& atlas) {
        set_compute_atlases(atlas, atlas);
    }

    void set_compute_atlases(const D3D12Texture& atlas, const D3D12Texture& color_atlas) {
        auto cpu_base = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        auto gpu_base = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        cpu_base.ptr += (m_descriptor_base + 1) * m_srv_uav_descriptor_size;
        gpu_base.ptr += (m_descriptor_base + 1) * m_srv_uav_descriptor_size;

        D3D12_SHADER_RESOURCE_VIEW_DESC desc0{};
        desc0.Format = to_dxgi_format(atlas.get_format());
        desc0.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc0.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc0.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(atlas.get_native_resource(), &desc0, cpu_base);

        D3D12_CPU_DESCRIPTOR_HANDLE cpu1 = cpu_base;
        cpu1.ptr += m_srv_uav_descriptor_size;
        D3D12_SHADER_RESOURCE_VIEW_DESC desc1{};
        desc1.Format = to_dxgi_format(color_atlas.get_format());
        desc1.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc1.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc1.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(color_atlas.get_native_resource(), &desc1, cpu1);

        m_cmd_list->SetComputeRootDescriptorTable(5, gpu_base);
    }

    void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) override {
        m_cmd_list->Dispatch(group_count_x, group_count_y, group_count_z);
    }

    void dispatch_indirect(const GpuBuffer& args_buffer, uint64_t offset_bytes) override {
        auto* d3d_buf = static_cast<const D3D12Buffer*>(&args_buffer);
        m_cmd_list->ExecuteIndirect(
            m_dispatch_cmd_sig,
            1,
            d3d_buf->get_native_resource(),
            offset_bytes,
            nullptr,
            0
        );
    }

    void copy_texture_to_buffer(const GpuTexture& src_texture, const GpuBuffer& dst_buffer) override {
        auto* d3d_tex = const_cast<D3D12Texture*>(static_cast<const D3D12Texture*>(&src_texture));
        auto* d3d_buf = const_cast<D3D12Buffer*>(static_cast<const D3D12Buffer*>(&dst_buffer));

        resource_barrier(*d3d_tex, d3d_tex->get_current_state(), ResourceState::CopySrc);

        D3D12_TEXTURE_COPY_LOCATION dst_loc{};
        dst_loc.pResource = d3d_buf->get_native_resource();
        dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        
        D3D12_RESOURCE_DESC src_tex_desc = d3d_tex->get_native_resource()->GetDesc();
        m_device->GetCopyableFootprints(
            &src_tex_desc,
            0, 1, 0,
            &dst_loc.PlacedFootprint,
            nullptr, nullptr, nullptr
        );

        D3D12_TEXTURE_COPY_LOCATION src_loc{};
        src_loc.pResource = d3d_tex->get_native_resource();
        src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src_loc.SubresourceIndex = 0;

        m_cmd_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
    }

    void blit_to_swapchain(Swapchain& swapchain, const GpuTexture& src_offscreen) override {
        auto* d3d_swapchain = static_cast<D3D12Swapchain*>(&swapchain);
        auto* d3d_offscreen = const_cast<D3D12Texture*>(static_cast<const D3D12Texture*>(&src_offscreen));
        auto* back_buffer = d3d_swapchain->get_current_back_buffer();

        // 1. 屏障转换: 离屏纹理 -> PixelShaderResource; 交换链后备缓冲 -> RenderTarget
        resource_barrier(*d3d_offscreen, d3d_offscreen->get_current_state(), ResourceState::PixelShaderResource);
        resource_barrier(*back_buffer, back_buffer->get_current_state(), ResourceState::UnorderedAccess); // 临时借用过渡

        D3D12_RESOURCE_BARRIER rtv_barrier{};
        rtv_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        rtv_barrier.Transition.pResource = back_buffer->get_native_resource();
        rtv_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        rtv_barrier.Transition.StateBefore = to_d3d12_resource_states(back_buffer->get_current_state());
        rtv_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_cmd_list->ResourceBarrier(1, &rtv_barrier);

        // 2. 设置图形管线与视口
        m_cmd_list->SetGraphicsRootSignature(m_graphics_root_sig);
        m_cmd_list->SetPipelineState(m_blit_pso);

        ID3D12DescriptorHeap* heaps[] = { m_srv_uav_heap };
        m_cmd_list->SetDescriptorHeaps(1, heaps);

        // 创建临时 SRV (放入 heap 槽位 0)
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = to_dxgi_format(d3d_offscreen->get_format());
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Texture2D.MipLevels = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        m_device->CreateShaderResourceView(d3d_offscreen->get_native_resource(), &srv_desc, cpu_handle);

        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        m_cmd_list->SetGraphicsRootDescriptorTable(0, gpu_handle);

        D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(swapchain.get_width()), static_cast<float>(swapchain.get_height()), 0.0f, 1.0f};
        D3D12_RECT scissor{0, 0, static_cast<LONG>(swapchain.get_width()), static_cast<LONG>(swapchain.get_height())};
        m_cmd_list->RSSetViewports(1, &viewport);
        m_cmd_list->RSSetScissorRects(1, &scissor);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = d3d_swapchain->get_current_rtv_handle();
        m_cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

        // 3. 绘制全屏大三角形 (3 个顶点由着色器自动生成)
        m_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_cmd_list->DrawInstanced(3, 1, 0, 0);

        // 4. 屏障转换回 Present
        rtv_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        rtv_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_cmd_list->ResourceBarrier(1, &rtv_barrier);
        back_buffer->set_current_state(ResourceState::Present);
    }

    void close() override {
        throw_if_failed(m_cmd_list->Close(), "CommandList Close failed");
    }

    [[nodiscard]] ID3D12GraphicsCommandList* get_native_list() const noexcept {
        return m_cmd_list.Get();
    }

private:
    ID3D12Device* m_device;
    ID3D12CommandAllocator* m_allocator;
    ID3D12RootSignature* m_compute_root_sig;
    ID3D12RootSignature* m_graphics_root_sig;
    ID3D12PipelineState* m_blit_pso;
    ID3D12CommandSignature* m_dispatch_cmd_sig;
    ID3D12DescriptorHeap* m_srv_uav_heap;
    uint32_t m_srv_uav_descriptor_size;
    uint32_t m_descriptor_base{0};
    ComPtr<ID3D12GraphicsCommandList> m_cmd_list;
};

} // namespace kernetile::hal::d3d12
