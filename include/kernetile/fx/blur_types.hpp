#pragma once

#include <cstdint>
#include <cstddef>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace kernetile::fx {

// ============================================================================
// Dual Kawase 模糊质量档位
// Low: 1 级降采样 (1/2) + 1 级升采样 = 2 passes
// Medium: 2 级降采样 (1/4) + 2 级升采样 = 4 passes
// High: 3 级降采样 (1/8) + 3 级升采样 = 6 passes
// ============================================================================
enum class BlurQuality : uint8_t {
    Low = 0,    // 1 级金字塔 (1/2 分辨率核心滤波)
    Medium = 1, // 2 级金字塔 (1/4 分辨率核心滤波)
    High = 2    // 3 级金字塔 (1/8 分辨率核心滤波)
};

// ============================================================================
// 与 GPU 着色器 cbuffer BlurUniforms : register(b0) 严格 16 字节对齐
// ============================================================================
#pragma pack(push, 1)
struct alignas(16) BlurUniforms {
    float texel_size_x{0.0f}; // 1.0 / in_w
    float texel_size_y{0.0f}; // 1.0 / in_h
    float offset{1.0f};       // 采样步长比例
    float padding{0.0f};
};
#pragma pack(pop)

static_assert(sizeof(BlurUniforms) == 16, "BlurUniforms must be exactly 16 bytes");
static_assert(alignof(BlurUniforms) == 16, "BlurUniforms must align to 16 bytes");

// 单个 Dual Kawase 计算轮次描述元数据
struct BlurPassDesc {
    uint32_t pass_index{0};
    bool is_downsample{true}; // true: Downsample pass, false: Upsample pass
    uint32_t input_width{0};
    uint32_t input_height{0};
    uint32_t output_width{0};
    uint32_t output_height{0};
    uint32_t pyramid_level{0}; // 目标金字塔层级 (0: 1/2, 1: 1/4, 2: 1/8 ...)
    BlurUniforms uniforms{};
};

// 模糊整体调度控制参数
struct BlurParams {
    BlurQuality quality{BlurQuality::Medium};
    float radius{10.0f};       // 模糊有效半径 (像素)
    float offset_scale{1.0f};  // 采样步进缩放
    uint32_t custom_levels{0}; // 0 代表按 quality 决定，>0 自定义金字塔深度
};

} // namespace kernetile::fx

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
