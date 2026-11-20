#pragma once

#include "kernetile/fx/blur_types.hpp"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace kernetile::fx {

// ============================================================================
// Dual Kawase 多级降采样模糊管线管理器与软件参考模型
// 依据 Marius Bjørge (SIGGRAPH 2015) 几何对称降采样与升采样滤波理论
// ============================================================================
class DualKawaseBlur {
public:
    // 计算根据质量档位与目标分辨率构建的 Pass 调度规划表
    static std::vector<BlurPassDesc> build_pass_plan(
        uint32_t width,
        uint32_t height,
        const BlurParams& params = BlurParams{});

    // 计算金字塔多级降采样离屏纹理占用的显存总字节数 (RGBA8 格式)
    static size_t calculate_pyramid_vram_bytes(
        uint32_t width,
        uint32_t height,
        const BlurParams& params = BlurParams{});

    // CPU 端参考级滤波执行 (用于离线/单元测试环境与数学精度闭环验证)
    static void blur_cpu(
        const uint8_t* src_rgba,
        uint32_t width,
        uint32_t height,
        uint8_t* dst_rgba,
        const BlurParams& params = BlurParams{});

    // CPU 端单次降采样通道 (输入 -> 输出, 4 采样点半对角双线性 + 中心权重)
    static void downsample_cpu(
        const uint8_t* src_rgba,
        uint32_t in_w,
        uint32_t in_h,
        uint8_t* dst_rgba,
        uint32_t out_w,
        uint32_t out_h,
        float offset);

    // CPU 端单次升采样通道 (输入 -> 输出, 8 采样点 Tent 滤波)
    static void upsample_cpu(
        const uint8_t* src_rgba,
        uint32_t in_w,
        uint32_t in_h,
        uint8_t* dst_rgba,
        uint32_t out_w,
        uint32_t out_h,
        float offset);

    // 双线性采样辅助函数 (带 ClampToEdge 边界寻址保护)
    static void sample_bilinear_rgba(
        const uint8_t* src,
        uint32_t w,
        uint32_t h,
        float u,
        float v,
        float out_color[4]);
};

} // namespace kernetile::fx
