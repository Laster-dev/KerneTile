#pragma once

#include "kernetile/hal/hal.hpp"
#include "d3d12_common.hpp"
#include "d3d12_texture.hpp"
#include <vector>

namespace kernetile::hal::d3d12 {

class D3D12Swapchain final : public Swapchain {
public:
    static constexpr uint32_t BUFFER_COUNT = 2;

    D3D12Swapchain(
        IDXGIFactory4* factory,
        ID3D12Device* device,
        ID3D12CommandQueue* queue,
        HWND hwnd,
        uint32_t width,
        uint32_t height)
        : m_device(device), m_width(width), m_height(height)
    {
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Stereo = FALSE;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = BUFFER_COUNT;
        sd.Scaling = DXGI_SCALING_NONE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        ComPtr<IDXGISwapChain1> swapchain1;
        HRESULT hr = factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &swapchain1);
        if (FAILED(hr)) {
            sd.Scaling = DXGI_SCALING_STRETCH;
            hr = factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &swapchain1);
        }
        throw_if_failed(hr, "CreateSwapChainForHwnd failed");

        throw_if_failed(swapchain1.As(&m_swapchain), "Query IDXGISwapChain3 failed");

        // 创建 RTV 描述符堆
        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
        rtv_heap_desc.NumDescriptors = BUFFER_COUNT;
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        throw_if_failed(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&m_rtv_heap)), "Create RTV heap failed");
        m_rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        create_render_target_views();
    }

    void resize(uint32_t width, uint32_t height) override {
        if (width == 0 || height == 0) return;
        m_width = width;
        m_height = height;

        // 释放旧后备缓冲引用
        for (uint32_t i = 0; i < BUFFER_COUNT; ++i) {
            m_back_buffers[i].reset();
        }

        throw_if_failed(
            m_swapchain->ResizeBuffers(
                BUFFER_COUNT,
                width,
                height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                0),
            "ResizeBuffers failed"
        );

        create_render_target_views();
    }

    void present(bool vsync) override {
        throw_if_failed(m_swapchain->Present(vsync ? 1 : 0, 0), "Swapchain Present failed");
        m_current_buffer_index = m_swapchain->GetCurrentBackBufferIndex();
    }

    [[nodiscard]] uint32_t get_width() const noexcept override { return m_width; }
    [[nodiscard]] uint32_t get_height() const noexcept override { return m_height; }

    [[nodiscard]] D3D12Texture* get_current_back_buffer() const noexcept {
        return m_back_buffers[m_swapchain->GetCurrentBackBufferIndex()].get();
    }

    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_current_rtv_handle() const noexcept {
        uint32_t idx = m_swapchain->GetCurrentBackBufferIndex();
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += idx * m_rtv_descriptor_size;
        return handle;
    }

private:
    void create_render_target_views() {
        m_current_buffer_index = m_swapchain->GetCurrentBackBufferIndex();
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();

        m_back_buffers.resize(BUFFER_COUNT);
        for (uint32_t i = 0; i < BUFFER_COUNT; ++i) {
            ComPtr<ID3D12Resource> back_buffer;
            throw_if_failed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffer)), "GetBuffer for Swapchain failed");

            m_device->CreateRenderTargetView(back_buffer.Get(), nullptr, rtv_handle);

            TextureDesc desc{
                .width = m_width,
                .height = m_height,
                .format = TextureFormat::RGBA8_UNORM,
                .allow_storage = false
            };
            m_back_buffers[i] = std::make_unique<D3D12Texture>(back_buffer, desc, ResourceState::Present);

            rtv_handle.ptr += m_rtv_descriptor_size;
        }
    }

    ID3D12Device* m_device;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_current_buffer_index{0};
    uint32_t m_rtv_descriptor_size{0};
    ComPtr<IDXGISwapChain3> m_swapchain;
    ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
    std::vector<std::unique_ptr<D3D12Texture>> m_back_buffers;
};

} // namespace kernetile::hal::d3d12
