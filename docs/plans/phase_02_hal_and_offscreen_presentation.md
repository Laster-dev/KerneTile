# 阶段 2: 显式图形抽象层与离屏存储呈现管线

## 1. 阶段目标与定位

本阶段的核心是构建 KerneTile 的 **现代跨平台显式图形硬件抽象层（HAL）**，并确立可靠、高兼容性的 **GPU 呈现流水线**。
在 KerneTile 中，图形 API 的角色发生了根本性倒置：
- **99% 的核心工作**：底层 API（Direct3D 12、Vulkan 等）纯粹作为 **GPGPU 并行计算引擎** 运行，负责调度计算队列（Compute Queue）、分配结构化显存缓冲（Structured Buffer）以及绑定存储纹理（Storage Texture / UAV）。
- **1% 的呈现工作**：为了规避桌面显卡与驱动对“计算着色器直接写入 sRGB 交换链”的严苛硬件限制，建立“**离屏非 sRGB 存储纹理计算写入 + 全屏 Quad Blit**”的标准化呈现通路。

交付物包括：
1. 轻量级显式硬件抽象层（HAL）：封装 `Device`、`Queue`、`Buffer`、`Texture`、`ComputePipeline`、`CommandBuffer` 与 `Swapchain`。
2. Direct3D 12 与 Vulkan 核心后端驱动落地。
3. 离屏 `R8G8B8A8_UNORM` 存储纹理与全屏呈现 Blit 管线。
4. 静态单图元（SDF 圆角矩形与解析阴影）端到端上屏冒烟验证程序。

---

## 2. 深入开发思路：底层硬件 API 规范陷阱与规避策略

试图让计算着色器直接向操作系统交换链（Swapchain Backbuffer）执行无序写入（UAV / Storage Image）是 2D 计算引擎最容易踩中的工业级“兼容性天坑”。深入核查显卡底层规范可明确以下限制：

```
                    [操作系统窗口与显示交换链 (sRGB / BGRA)]
                                   ▲
                                   │  (全屏 Quad Blit: 100% 硬件兼容)
                                   │
              [离屏存储纹理: R8G8B8A8_UNORM (UAV / Storage Image)]
                                   ▲
                                   │  (计算着色器无序访问写入)
                                   │
         ┌─────────────────────────┴─────────────────────────┐
         │         GPU 16×16 分块计算着色器 (Compute Shader)  │
         │ - 纯数学求值 (SDF / erf 阴影)                      │
         │ - 线性空间混合与 IEC 61966-2-1 传递函数编码        │
         └───────────────────────────────────────────────────┘
```

### 2.1 Direct3D 11/12 的 UAV 格式严苛约束
- **sRGB 格式禁令**：Microsoft Feature Level 11.0 硬件格式支持明确规定，`DXGI_FORMAT_B8G8R8A8_UNORM_SRGB` 严禁 UAV Typed Load 与 Typed Store（在 Direct3D 12 中亦属可选非强制能力）。
- **通道重排（Swizzle）障碍**：桌面交换链通常为 BGRA 格式，而 D3D12 标准 UAV 兼容集合不仅缺失 sRGB，还缺失 BGRA 布局。若试图通过同址别名（Resource Aliasing）将 BGRA 重解释为 RGBA UAV，则强制要求硬件支持 `StandardSwizzle64KBSupported`，否则现代 D3D12 验证层会在运行时直接拦截报错崩溃。

### 2.2 Vulkan 交换链的 Storage 位缺失陷阱
- 依据 Vulkan 规范，交换链图像仅强制保证支持 `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`，并不保证支持 `VK_IMAGE_USAGE_STORAGE_BIT`。应用程序必须在运行时显式查询 `VkSurfaceCapabilitiesKHR::supportedUsageFlags`。
- 广泛存在的实际情况是：许多集成显卡驱动、移动端合成器以及第三方帧率统计覆盖层（如 RivaTuner、OBS Game Capture）在挂钩交换链时，会强行移除 `STORAGE_BIT`，导致应用启动时崩溃。

### 2.3 结论：确立全平台统一离屏计算架构
为了实现 100% 的硬件兼容性与色彩严谨性，KerneTile 确立以下标准呈现路径：
1. **统一计算目标**：计算内核无条件输出至离屏的 `R8G8B8A8_UNORM`（或 HDR 场景下的 `R16G16B16A16_FLOAT`）无类型存储纹理。
2. **片上色彩转换**：所有 Porter-Duff 预乘 Alpha 混合与 IEC 61966-2-1 传递函数转换在着色器内以 32-bit 浮点精度完成，写入存储纹理时已是编码好的标准颜色。
3. **呈现搬运**：在帧末尾由图形管线执行一个无分支、无顶点的全屏 Quad Blit 片段着色器，单次采样离屏纹理直接复制至交换链后备缓冲。这一步在现代 GPU 上仅耗时数十微秒，彻底规避了所有的驱动崩溃与格式限制。

---

## 3. 显式图形 HAL 接口架构设计

HAL 采用轻量化现代 C++20 设计，避免传统引擎（如 Qt RHI 或 Skia Ganesh）厚重的类层次，只暴露与计算分块强相关的最小抽象集合。

### 3.1 核心 HAL 接口定义 (`hal.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <memory>

namespace kernetile::hal {

enum class BufferUsage : uint32_t {
    Upload,          // CPU 可写，GPU 线性读取 (Upload Heap)
    Storage,         // GPU 可读写结构化缓冲 (UAV / SSBO)
    IndirectArgs     // GPU 间接派发参数缓冲
};

enum class TextureFormat {
    R8_UNORM,        // 单通道灰度 (字形图集)
    RGBA8_UNORM,     // 四通道颜色 (离屏存储纹理 / 图标图集)
    RGBA16_FLOAT     // HDR 离屏存储纹理
};

enum class ResourceState {
    Common,
    UnorderedAccess, // 计算着色器写入
    PixelShaderResource, // 全屏 Blit 采样读取
    Present          // 交换链呈现
};

// 抽象资源句柄
struct GpuBuffer {
    virtual ~GpuBuffer() = default;
    [[nodiscard]] virtual void* map() = 0;
    virtual void unmap() = 0;
    [[nodiscard]] virtual uint64_t get_gpu_virtual_address() const noexcept = 0;
};

struct GpuTexture {
    virtual ~GpuTexture() = default;
    [[nodiscard]] virtual uint32_t get_width() const noexcept = 0;
    [[nodiscard]] virtual uint32_t get_height() const noexcept = 0;
    [[nodiscard]] virtual TextureFormat get_format() const noexcept = 0;
};

struct ComputePipeline {
    virtual ~ComputePipeline() = default;
};

// 命令列表抽象
struct CommandRecorder {
    virtual ~CommandRecorder() = default;

    virtual void resource_barrier(
        GpuTexture& texture, 
        ResourceState state_before, 
        ResourceState state_after) = 0;

    virtual void set_compute_pipeline(const ComputePipeline& pipeline) = 0;
    virtual void set_storage_buffer(uint32_t slot, const GpuBuffer& buffer) = 0;
    virtual void set_storage_texture(uint32_t slot, const GpuTexture& texture) = 0;

    // 直接分发计算工作组
    virtual void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) = 0;

    // 间接分发 (由 GPU 端生成的 Indirect Args 触发)
    virtual void dispatch_indirect(const GpuBuffer& args_buffer, uint64_t offset) = 0;

    // 全屏 Quad Blit (将离屏存储纹理拷贝并呈现至交换链)
    virtual void blit_to_swapchain(const GpuTexture& src_offscreen) = 0;
};

// 抽象图形设备
struct GraphicsDevice {
    virtual ~GraphicsDevice() = default;

    [[nodiscard]] virtual std::unique_ptr<GpuBuffer> create_buffer(
        size_t size_bytes, 
        BufferUsage usage) = 0;

    [[nodiscard]] virtual std::unique_ptr<GpuTexture> create_storage_texture(
        uint32_t width, 
        uint32_t height, 
        TextureFormat format) = 0;

    [[nodiscard]] virtual std::unique_ptr<ComputePipeline> create_compute_pipeline(
        std::span<const uint8_t> bytecode) = 0;

    [[nodiscard]] virtual std::unique_ptr<CommandRecorder> create_command_recorder() = 0;

    virtual void submit_and_present(std::unique_ptr<CommandRecorder> recorder) = 0;
    virtual void wait_idle() = 0;
};

} // namespace kernetile::hal
```

---

## 4. 全屏 Quad Blit 呈现着色器实现

全屏 Quad 不需要 CPU 传输任何顶点缓冲，片段着色器直接利用 SV_VertexID（或 gl_VertexIndex）在着色器内部无内存生成一个覆盖整屏的大三角形：

```hlsl
// kerne_blit.slang
Texture2D<float4> g_OffscreenTexture : register(t0);
SamplerState g_PointSampler : register(s0);

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// 0-3 顶点直接生成全屏大三角形 (免顶点缓冲)
[shader("vertex")]
VertexOutput vs_blit(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 ps_blit(VertexOutput input) : SV_Target
{
    // 计算着色器已完成 sRGB 编码，此处直接点采样输出
    return g_OffscreenTexture.Sample(g_PointSampler, input.uv);
}
```

---

## 5. 验证与集成测试计划

### 5.1 自动化单元与冒烟测试
1. **HAL 资源分配与生命周期测试 (`test_hal_lifecycle.cpp`)**：
   - 连续创建并释放 1,000 个 4K 尺寸 `GpuTexture` 与 10 MB `GpuBuffer`，通过 D3D12/Vulkan 内存转储工具检测是否存在显存泄漏或句柄泄露。
2. **计算着色器写入存储纹理一致性测试 (`test_compute_uav_readback.cpp`)**：
   - 编写测试计算内核向 `R8G8B8A8_UNORM` 写入固定颜色棋盘格。
   - 回读（Readback）显存数据并与 CPU 期望值逐像素对比，验证 UAV 读写完全正常。
3. **单图元渲染上屏冒烟展示 (`smoke_primitive_window.cpp`)**：
   - 创建 Win32 / X11 窗口，初始化 D3D12 / Vulkan 设备与交换链。
   - 分配 80 字节 `UiPrimitive`（中心为蓝色圆角矩形，附带外发光软阴影）。
   - 启动计算着色器求值写入离屏纹理，随后调用全屏 Quad Blit 呈现上屏，肉眼与截图工具验证渲染无异常卡顿。

### 5.2 阶段验收标准
- Direct3D 12 验证层（D3D12 Debug Layer）与 Vulkan 验证层（Validation Layer）在运行时录制与提交过程中保持 **0 错误、0 警告**。
- 窗口在拖拽缩放（Resize）时，离屏存储纹理能够快速安全销毁重建，帧画面平滑过渡无撕裂闪烁。
