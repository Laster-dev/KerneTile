#pragma once

#include "kernetile/core/types.hpp"
#include "kernetile/core/primitives.hpp"
#include "kernetile/core/tile_header.hpp"
#include "kernetile/tile/tile_grid.hpp"
#include "kernetile/fx/clip_stack.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <span>

namespace kernetile::cpu {

// ============================================================================
// 纯 CPU 16×16 瓦片光栅化引擎与自适应并发调度器
// 无论在无显卡服务器、CI 构建还是安全模式下，提供 100% 像素级一致的回退渲染
// ============================================================================
class CpuTileRasterizer {
public:
    CpuTileRasterizer() = default;

    // 根据待处理脏瓦片数量与宿主 CPU 核心数，自适应计算最优工作线程数 (1-8T)
    // 规避微小局部重绘下的线程同步与上下文切换开销反噬
    static uint32_t calculate_worker_threads(uint32_t dirty_tile_count, uint32_t cpu_cores) noexcept;

    // 渲染参数描述
    struct RenderParams {
        uint32_t width{0};
        uint32_t height{0};
        std::span<const core::UiPrimitive> primitives{};
        std::span<const uint32_t>          tile_indices{};
        std::span<const core::TileHeader>  tile_headers{};
        std::span<const uint32_t>          dirty_tiles{}; // 待渲染的 tile_id 列表
        std::span<const core::ClipRect>    clip_rects{};
        const uint8_t*                     atlas_pixels{nullptr};
        uint32_t                           atlas_width{0};
        uint32_t                           atlas_height{0};
        const uint8_t*                     color_atlas_pixels{nullptr};
        uint32_t                           color_atlas_width{0};
        uint32_t                           color_atlas_height{0};
        uint32_t                           thread_count{0}; // 0 代表自适应计算
        bool                               mica_enabled{false};
        std::array<uint32_t, 16>           mica_grid{};
        bool                               enable_dithering{false};
    };

    // 执行纯 CPU 瓦片光栅化渲染，写出至 RGBA8 帧缓冲区
    static void render(
        const RenderParams& params,
        uint8_t* dst_framebuffer);

    // 单个 16×16 瓦片的独立光栅化核函数
    static void render_tile(
        uint32_t tile_x,
        uint32_t tile_y,
        uint32_t tile_id,
        uint32_t width,
        uint32_t height,
        std::span<const core::UiPrimitive> primitives,
        std::span<const uint32_t>          tile_indices,
        std::span<const core::TileHeader>  tile_headers,
        std::span<const core::ClipRect>    clip_rects,
        uint8_t* dst_framebuffer,
        const uint8_t* atlas_pixels = nullptr,
        uint32_t atlas_width = 0,
        uint32_t atlas_height = 0,
        bool mica_enabled = false,
        const std::array<uint32_t, 16>* mica_grid = nullptr,
        const uint8_t* color_atlas_pixels = nullptr,
        uint32_t color_atlas_width = 0,
        uint32_t color_atlas_height = 0,
        bool enable_dithering = false);

    // 数学内核 (与 Slang / DXIL 着色器完全相同的 Abramowitz & Stegun 系数)
    static float erf_approx(float x) noexcept;
    static float evaluate_coverage(float d) noexcept;
};

} // namespace kernetile::cpu
