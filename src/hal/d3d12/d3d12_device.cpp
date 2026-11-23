#include "d3d12_device.hpp"
#include "d3d12_buffer.hpp"
#include "d3d12_texture.hpp"
#include "d3d12_pipeline.hpp"
#include "d3d12_swapchain.hpp"
#include "d3d12_command_recorder.hpp"
#include "embedded_vs_blit.hpp"
#include "embedded_ps_blit.hpp"
#include <fstream>
#include <vector>

namespace kernetile::hal::d3d12 {

static std::vector<uint8_t> read_binary_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
}

D3D12Device::D3D12Device(bool prefer_warp, bool allow_warp, bool init_blit) {
    UINT dxgi_flags = 0;
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug_controller;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
        debug_controller->EnableDebugLayer();
        dxgi_flags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    throw_if_failed(CreateDXGIFactory2(dxgi_flags, IID_PPV_ARGS(&m_factory)), "CreateDXGIFactory2 failed");

    // 适配器选择
    if (prefer_warp) {
        throw_if_failed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&m_adapter)), "EnumWarpAdapter failed");
    } else {
        for (UINT i = 0; m_factory->EnumAdapters1(i, &m_adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            m_adapter->GetDesc1(&desc);
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                // 找到第一个物理硬件适配器
                if (SUCCEEDED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
                    break;
                }
            }
            m_adapter.Reset();
        }
        // 如果未找到硬件适配器，自动降级至 WARP
        if (!m_adapter) {
            if (!allow_warp) throw std::runtime_error("No hardware D3D12 adapter");
            std::cout << "[D3D12] Hardware GPU unavailable, falling back to WARP software adapter." << std::endl;
            throw_if_failed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&m_adapter)), "EnumWarpAdapter fallback failed");
        }
    }

    DXGI_ADAPTER_DESC1 adapter_desc{};
    m_adapter->GetDesc1(&adapter_desc);
    char name_buf[128]{};
    WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, -1, name_buf, sizeof(name_buf), nullptr, nullptr);
    m_device_name = name_buf;

    throw_if_failed(
        D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)),
        "D3D12CreateDevice failed"
    );

    // 创建直接命令队列 (Direct Command Queue)
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    throw_if_failed(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_queue)), "CreateCommandQueue failed");

    // 创建通用命令分配器
    throw_if_failed(
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmd_allocator)),
        "CreateCommandAllocator failed"
    );

    // 创建帧同步栅栏与事件
    throw_if_failed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)), "CreateFence failed");
    m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fence_event) {
        throw std::runtime_error("CreateEvent for D3D12 fence failed");
    }

    // 创建描述符堆 (CBV / SRV / UAV)
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 256;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    throw_if_failed(m_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_srv_uav_heap)), "CreateDescriptorHeap failed");
    m_srv_uav_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    init_root_signatures();
    if (init_blit) {
        init_blit_pso();
    }
    init_command_signature();
}

D3D12Device::~D3D12Device() {
    try { wait_idle(); } catch (const std::exception&) {}
    if (m_fence_event) {
        CloseHandle(m_fence_event);
        m_fence_event = nullptr;
    }
}

void D3D12Device::init_root_signatures() {
    // 1. 计算根签名 (Compute Root Signature)
    // 参数 0: Root SRV (t0: UiPrimitives)
    // 参数 1: Root SRV (t1: TileIndices)
    // 参数 2: Root SRV (t2: TileHeaders)
    // 参数 3: Root SRV (t3: DirtyTiles)
    // 参数 4: Descriptor Table (1 UAV: u0 OutputStorage)
    // 参数 5: Descriptor Table (1 SRV: t4 Atlas Texture)
    D3D12_ROOT_PARAMETER root_params[7]{};

    // t0
    root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    root_params[0].Descriptor.ShaderRegister = 0;
    root_params[0].Descriptor.RegisterSpace = 0;
    root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // t1
    root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    root_params[1].Descriptor.ShaderRegister = 1;
    root_params[1].Descriptor.RegisterSpace = 0;
    root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // t2
    root_params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    root_params[2].Descriptor.ShaderRegister = 2;
    root_params[2].Descriptor.RegisterSpace = 0;
    root_params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // t3 (DirtyTiles)
    root_params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    root_params[3].Descriptor.ShaderRegister = 3;
    root_params[3].Descriptor.RegisterSpace = 0;
    root_params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // u0 Table
    D3D12_DESCRIPTOR_RANGE uav_range{};
    uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uav_range.NumDescriptors = 1;
    uav_range.BaseShaderRegister = 0;
    uav_range.RegisterSpace = 0;
    uav_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    root_params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[4].DescriptorTable.NumDescriptorRanges = 1;
    root_params[4].DescriptorTable.pDescriptorRanges = &uav_range;
    root_params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // t4 Table
    D3D12_DESCRIPTOR_RANGE srv_range{};
    srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range.NumDescriptors = 1;
    srv_range.BaseShaderRegister = 4;
    srv_range.RegisterSpace = 0;
    srv_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    root_params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[5].DescriptorTable.NumDescriptorRanges = 1;
    root_params[5].DescriptorTable.pDescriptorRanges = &srv_range;
    root_params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    root_params[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_params[6].Constants.Num32BitValues = 24;
    root_params[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 静态采样器 (Clamp s0)
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC compute_sig_desc{};
    compute_sig_desc.NumParameters = 7;
    sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    compute_sig_desc.pParameters = root_params;
    compute_sig_desc.NumStaticSamplers = 1;
    compute_sig_desc.pStaticSamplers = &sampler;
    compute_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> serialized_sig;
    ComPtr<ID3DBlob> error_blob;
    throw_if_failed(
        D3D12SerializeRootSignature(&compute_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized_sig, &error_blob),
        "Serialize Compute Root Signature failed"
    );
    throw_if_failed(
        m_device->CreateRootSignature(0, serialized_sig->GetBufferPointer(), serialized_sig->GetBufferSize(), IID_PPV_ARGS(&m_compute_root_signature)),
        "Create Compute Root Signature failed"
    );

    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    // 2. 图形根签名 (Graphics Blit Root Signature)
    D3D12_DESCRIPTOR_RANGE blit_srv_range{};
    blit_srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    blit_srv_range.NumDescriptors = 1;
    blit_srv_range.BaseShaderRegister = 0;
    blit_srv_range.RegisterSpace = 0;
    blit_srv_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER gfx_param{};
    gfx_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gfx_param.DescriptorTable.NumDescriptorRanges = 1;
    gfx_param.DescriptorTable.pDescriptorRanges = &blit_srv_range;
    gfx_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC gfx_sig_desc{};
    gfx_sig_desc.NumParameters = 1;
    gfx_sig_desc.pParameters = &gfx_param;
    gfx_sig_desc.NumStaticSamplers = 1;
    gfx_sig_desc.pStaticSamplers = &sampler;
    gfx_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    throw_if_failed(
        D3D12SerializeRootSignature(&gfx_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized_sig, &error_blob),
        "Serialize Graphics Root Signature failed"
    );
    throw_if_failed(
        m_device->CreateRootSignature(0, serialized_sig->GetBufferPointer(), serialized_sig->GetBufferSize(), IID_PPV_ARGS(&m_graphics_root_signature)),
        "Create Graphics Root Signature failed"
    );
}

void D3D12Device::init_blit_pso() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature = m_graphics_root_signature.Get();
    pso_desc.VS = { g_embedded_vs_blit, sizeof(g_embedded_vs_blit) };
    pso_desc.PS = { g_embedded_ps_blit, sizeof(g_embedded_ps_blit) };

    // 不透明渲染，直接覆盖
    pso_desc.BlendState.AlphaToCoverageEnable = FALSE;
    pso_desc.BlendState.IndependentBlendEnable = FALSE;
    pso_desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pso_desc.SampleMask = UINT_MAX;
    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.DepthStencilState.DepthEnable = FALSE;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;

    throw_if_failed(
        m_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_blit_pso)),
        "CreateGraphicsPipelineState for Blit failed"
    );
}

void D3D12Device::init_command_signature() {
    D3D12_INDIRECT_ARGUMENT_DESC arg{};
    arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

    D3D12_COMMAND_SIGNATURE_DESC desc{};
    desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
    desc.NumArgumentDescs = 1;
    desc.pArgumentDescs = &arg;

    throw_if_failed(
        m_device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_dispatch_cmd_signature)),
        "CreateCommandSignature failed"
    );
}

std::unique_ptr<GpuBuffer> D3D12Device::create_buffer(const BufferDesc& desc) {
    return std::make_unique<D3D12Buffer>(m_device.Get(), desc);
}

std::unique_ptr<GpuTexture> D3D12Device::create_texture(const TextureDesc& desc) {
    return std::make_unique<D3D12Texture>(m_device.Get(), desc);
}

std::unique_ptr<ComputePipeline> D3D12Device::create_compute_pipeline(std::span<const uint8_t> bytecode) {
    return std::make_unique<D3D12ComputePipeline>(m_device.Get(), m_compute_root_signature.Get(), bytecode);
}

std::unique_ptr<CommandRecorder> D3D12Device::create_command_recorder() {
    return std::make_unique<D3D12CommandRecorder>(
        m_device.Get(),
        m_cmd_allocator.Get(),
        m_compute_root_signature.Get(),
        m_graphics_root_signature.Get(),
        m_blit_pso.Get(),
        m_dispatch_cmd_signature.Get(),
        m_srv_uav_heap.Get(),
        m_srv_uav_descriptor_size
    );
}

std::unique_ptr<Swapchain> D3D12Device::create_swapchain(void* native_window_handle, uint32_t width, uint32_t height) {
    return std::make_unique<D3D12Swapchain>(
        m_factory.Get(),
        m_device.Get(),
        m_queue.Get(),
        static_cast<HWND>(native_window_handle),
        width,
        height
    );
}

void D3D12Device::submit(std::unique_ptr<CommandRecorder> recorder) {
    auto* d3d_rec = static_cast<D3D12CommandRecorder*>(recorder.get());
    ID3D12CommandList* lists[] = { d3d_rec->get_native_list() };
    m_queue->ExecuteCommandLists(1, lists);
}

void D3D12Device::wait_idle() {
    m_fence_value++;
    throw_if_failed(m_queue->Signal(m_fence.Get(), m_fence_value), "Queue Signal failed");
    if (m_fence->GetCompletedValue() < m_fence_value) {
        throw_if_failed(m_fence->SetEventOnCompletion(m_fence_value, m_fence_event), "SetEventOnCompletion failed");
        WaitForSingleObject(m_fence_event, INFINITE);
    }
    throw_if_failed(m_cmd_allocator->Reset(), "CommandAllocator Reset failed");
}

} // namespace kernetile::hal::d3d12

namespace kernetile::hal {

std::unique_ptr<GraphicsDevice> create_d3d12_device(bool prefer_warp) {
    return std::make_unique<d3d12::D3D12Device>(prefer_warp);
}

} // namespace kernetile::hal
