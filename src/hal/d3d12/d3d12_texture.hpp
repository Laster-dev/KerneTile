#pragma once

#include "kernetile/hal/hal.hpp"
#include "d3d12_common.hpp"

namespace kernetile::hal::d3d12 {

class D3D12Texture final : public GpuTexture {
public:
    D3D12Texture(ID3D12Device* device, const TextureDesc& desc)
        : m_desc(desc), m_current_state(ResourceState::Common)
    {
        D3D12_HEAP_PROPERTIES heap_props{};
        heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC res_desc{};
        res_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        res_desc.Alignment = 0;
        res_desc.Width = desc.width;
        res_desc.Height = desc.height;
        res_desc.DepthOrArraySize = 1;
        res_desc.MipLevels = 1;
        res_desc.Format = to_dxgi_format(desc.format);
        res_desc.SampleDesc.Count = 1;
        res_desc.SampleDesc.Quality = 0;
        res_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        res_desc.Flags = desc.allow_storage ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

        throw_if_failed(
            device->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &res_desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&m_resource)),
            "CreateCommittedResource for Texture failed"
        );
    }

    // 从已有 ID3D12Resource (例如交换链后备缓冲) 构造
    D3D12Texture(ComPtr<ID3D12Resource> resource, const TextureDesc& desc, ResourceState init_state)
        : m_resource(resource), m_desc(desc), m_current_state(init_state) {}

    [[nodiscard]] uint32_t get_width() const noexcept override { return m_desc.width; }
    [[nodiscard]] uint32_t get_height() const noexcept override { return m_desc.height; }
    [[nodiscard]] TextureFormat get_format() const noexcept override { return m_desc.format; }
    [[nodiscard]] ResourceState get_current_state() const noexcept override { return m_current_state; }

    void set_current_state(ResourceState state) noexcept { m_current_state = state; }
    [[nodiscard]] ID3D12Resource* get_native_resource() const noexcept { return m_resource.Get(); }
    [[nodiscard]] const TextureDesc& get_desc() const noexcept { return m_desc; }

private:
    TextureDesc m_desc;
    ResourceState m_current_state;
    ComPtr<ID3D12Resource> m_resource;
};

} // namespace kernetile::hal::d3d12
