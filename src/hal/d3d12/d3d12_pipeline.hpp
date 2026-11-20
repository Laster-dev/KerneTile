#pragma once

#include "kernetile/hal/hal.hpp"
#include "d3d12_common.hpp"

namespace kernetile::hal::d3d12 {

class D3D12ComputePipeline final : public ComputePipeline {
public:
    D3D12ComputePipeline(ID3D12Device* device, ID3D12RootSignature* root_sig, std::span<const uint8_t> bytecode)
        : m_root_signature(root_sig)
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = root_sig;
        desc.CS.pShaderBytecode = bytecode.data();
        desc.CS.BytecodeLength = bytecode.size();

        throw_if_failed(
            device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pso)),
            "CreateComputePipelineState failed"
        );
    }

    [[nodiscard]] ID3D12PipelineState* get_native_pso() const noexcept {
        return m_pso.Get();
    }

    [[nodiscard]] ID3D12RootSignature* get_root_signature() const noexcept {
        return m_root_signature.Get();
    }

private:
    ComPtr<ID3D12PipelineState> m_pso;
    ComPtr<ID3D12RootSignature> m_root_signature;
};

} // namespace kernetile::hal::d3d12
