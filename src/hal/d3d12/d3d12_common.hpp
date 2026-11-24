#pragma once

#include "kernetile/hal/hal_types.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>
#include <cstdio>

namespace kernetile::hal::d3d12 {

using Microsoft::WRL::ComPtr;

inline bool throw_if_failed(HRESULT hr, const char* msg = "") noexcept {
    if (FAILED(hr)) {
        std::fprintf(stderr, "D3D12 Error: %s (HRESULT 0x%08lX)\n", msg ? msg : "", static_cast<unsigned long>(hr));
        return false;
    }
    return true;
}

inline DXGI_FORMAT to_dxgi_format(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::R8_UNORM:        return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::RGBA8_UNORM:     return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA16_FLOAT:    return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default:                             return DXGI_FORMAT_UNKNOWN;
    }
}

inline D3D12_RESOURCE_STATES to_d3d12_resource_states(ResourceState state) noexcept {
    switch (state) {
        case ResourceState::Common:              return D3D12_RESOURCE_STATE_COMMON;
        case ResourceState::UnorderedAccess:     return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case ResourceState::PixelShaderResource: return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        case ResourceState::CopySrc:             return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case ResourceState::CopyDst:             return D3D12_RESOURCE_STATE_COPY_DEST;
        case ResourceState::ComputeShaderResource: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case ResourceState::Present:             return D3D12_RESOURCE_STATE_PRESENT;
        default:                                 return D3D12_RESOURCE_STATE_COMMON;
    }
}

} // namespace kernetile::hal::d3d12
