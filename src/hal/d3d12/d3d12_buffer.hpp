#pragma once

#include "kernetile/hal/hal.hpp"
#include "d3d12_common.hpp"

namespace kernetile::hal::d3d12 {

class D3D12Buffer final : public GpuBuffer {
public:
    D3D12Buffer(ID3D12Device* device, const BufferDesc& desc)
        : m_desc(desc)
    {
        D3D12_HEAP_PROPERTIES heap_props{};
        D3D12_RESOURCE_STATES init_state = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        switch (desc.usage) {
            case BufferUsage::Upload:
                heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
                init_state = D3D12_RESOURCE_STATE_GENERIC_READ;
                break;
            case BufferUsage::Readback:
                heap_props.Type = D3D12_HEAP_TYPE_READBACK;
                init_state = D3D12_RESOURCE_STATE_COPY_DEST;
                break;
            case BufferUsage::Storage:
                heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
                flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
                init_state = D3D12_RESOURCE_STATE_COMMON;
                break;
            case BufferUsage::IndirectArgs:
                heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
                init_state = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
                break;
        }

        D3D12_RESOURCE_DESC res_desc{};
        res_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        res_desc.Alignment = 0;
        res_desc.Width = desc.size_bytes;
        res_desc.Height = 1;
        res_desc.DepthOrArraySize = 1;
        res_desc.MipLevels = 1;
        res_desc.Format = DXGI_FORMAT_UNKNOWN;
        res_desc.SampleDesc.Count = 1;
        res_desc.SampleDesc.Quality = 0;
        res_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        res_desc.Flags = flags;

        throw_if_failed(
            device->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &res_desc,
                init_state,
                nullptr,
                IID_PPV_ARGS(&m_resource)),
            "CreateCommittedResource for Buffer failed"
        );

        if (desc.usage == BufferUsage::Upload) {
            D3D12_RANGE read_range{0, 0};
            throw_if_failed(m_resource->Map(0, &read_range, &m_mapped_ptr), "Map Upload buffer failed");
        }
    }

    ~D3D12Buffer() override {
        if (m_mapped_ptr && m_desc.usage == BufferUsage::Upload) {
            D3D12_RANGE written_range{0, m_desc.size_bytes};
            m_resource->Unmap(0, &written_range);
            m_mapped_ptr = nullptr;
        }
    }

    [[nodiscard]] void* map() override {
        if (!m_mapped_ptr) {
            D3D12_RANGE read_range{0, m_desc.size_bytes};
            throw_if_failed(m_resource->Map(0, &read_range, &m_mapped_ptr), "Map buffer failed");
        }
        return m_mapped_ptr;
    }

    void unmap() override {
        if (m_mapped_ptr && m_desc.usage != BufferUsage::Upload) {
            D3D12_RANGE written_range{0, 0};
            m_resource->Unmap(0, &written_range);
            m_mapped_ptr = nullptr;
        }
    }

    [[nodiscard]] uint64_t get_gpu_virtual_address() const noexcept override {
        return m_resource ? m_resource->GetGPUVirtualAddress() : 0;
    }

    [[nodiscard]] size_t get_size_bytes() const noexcept override {
        return m_desc.size_bytes;
    }

    [[nodiscard]] ID3D12Resource* get_native_resource() const noexcept {
        return m_resource.Get();
    }

    [[nodiscard]] const BufferDesc& get_desc() const noexcept {
        return m_desc;
    }

private:
    BufferDesc m_desc;
    ComPtr<ID3D12Resource> m_resource;
    void* m_mapped_ptr{nullptr};
};

} // namespace kernetile::hal::d3d12
