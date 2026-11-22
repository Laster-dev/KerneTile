#pragma once

#include <cstdint>
#include <cstddef>

namespace kernetile::hal {

// 显存缓冲用途枚举
enum class BufferUsage : uint32_t {
    Upload,          // CPU 频繁写入，GPU 读取 (D3D12 Upload Heap / Vulkan Host Visible)
    Storage,         // GPU 高速读写结构化缓冲 (UAV / SSBO / StructuredBuffer)
    Readback,        // GPU 写入，CPU 回读 (D3D12 Readback Heap)
    IndirectArgs     // 间接派发参数缓冲 (D3D12_DISPATCH_ARGUMENTS)
};

// 纹理像素格式
enum class TextureFormat : uint32_t {
    Unknown = 0,
    R8_UNORM,        // 单通道 8-bit (字形图集)
    RGBA8_UNORM,     // 四通道 8-bit 无符号归一化 (离屏存储纹理 / 图标图集)
    RGBA16_FLOAT     // 四通道 16-bit 浮点 (HDR 渲染目标)
};

// GPU 资源状态与屏障枚举
enum class ResourceState : uint32_t {
    Common,              // 通用初始状态
    UnorderedAccess,     // 计算着色器无序随机读写 (UAV / Storage Image)
    PixelShaderResource, // 全屏 Blit 片段着色器采样读取 (SRV)
    CopySrc,             // 拷贝源
    CopyDst,             // 拷贝目标
    Present,             // 交换链呈现展示
    ComputeShaderResource
};

// 纹理描述符
struct TextureDesc {
    uint32_t width{0};
    uint32_t height{0};
    TextureFormat format{TextureFormat::RGBA8_UNORM};
    bool allow_storage{true}; // 是否允许作为 UAV / Storage Image 写入
};

// 缓冲描述符
struct BufferDesc {
    size_t size_bytes{0};
    BufferUsage usage{BufferUsage::Upload};
    size_t structure_byte_stride{0}; // 结构化缓冲单元素步长 (如 UiPrimitive 为 80B)
};

} // namespace kernetile::hal
