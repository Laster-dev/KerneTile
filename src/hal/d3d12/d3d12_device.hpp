#pragma once

#include "kernetile/hal/hal.hpp"
#include "d3d12_common.hpp"
#include <vector>

namespace kernetile::hal::d3d12 {

class D3D12Device final : public GraphicsDevice {
public:
    explicit D3D12Device(bool prefer_warp, bool allow_warp = true);
    ~D3D12Device() override;

    [[nodiscard]] std::unique_ptr<GpuBuffer> create_buffer(const BufferDesc& desc) override;
    [[nodiscard]] std::unique_ptr<GpuTexture> create_texture(const TextureDesc& desc) override;
    [[nodiscard]] std::unique_ptr<ComputePipeline> create_compute_pipeline(
        std::span<const uint8_t> bytecode) override;

    [[nodiscard]] std::unique_ptr<CommandRecorder> create_command_recorder() override;
    [[nodiscard]] std::unique_ptr<Swapchain> create_swapchain(
        void* native_window_handle, 
        uint32_t width, 
        uint32_t height) override;

    void submit(std::unique_ptr<CommandRecorder> recorder) override;
    void wait_idle() override;
    [[nodiscard]] std::string_view get_device_name() const noexcept override { return m_device_name; }

    [[nodiscard]] ID3D12Device* get_native_device() const noexcept { return m_device.Get(); }
    [[nodiscard]] ID3D12CommandQueue* get_native_queue() const noexcept { return m_queue.Get(); }

private:
    void init_root_signatures();
    void init_blit_pso();
    void init_command_signature();

    std::string m_device_name;
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGIAdapter1> m_adapter;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_queue;
    ComPtr<ID3D12CommandAllocator> m_cmd_allocator;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fence_event{nullptr};
    uint64_t m_fence_value{0};

    // 根签名与固定管线
    ComPtr<ID3D12RootSignature> m_compute_root_signature;
    ComPtr<ID3D12RootSignature> m_graphics_root_signature;
    ComPtr<ID3D12PipelineState> m_blit_pso;
    ComPtr<ID3D12CommandSignature> m_dispatch_cmd_signature;

    // 共享描述符堆 (CBV / SRV / UAV)
    ComPtr<ID3D12DescriptorHeap> m_srv_uav_heap;
    uint32_t m_srv_uav_descriptor_size{0};
};

} // namespace kernetile::hal::d3d12
