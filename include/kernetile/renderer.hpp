#pragma once

#include "kernetile/core/types.hpp"
#include "kernetile/core/primitives.hpp"
#include "kernetile/math/rect.hpp"
#include "kernetile/math/color.hpp"
#include "kernetile/hal/hal.hpp"
#include "kernetile/tile/tile_grid.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include "kernetile/tile/dirty_tracker.hpp"
#include "kernetile/atlas/dynamic_atlas.hpp"
#include "kernetile/text/font_engine.hpp"
#include "kernetile/text/text_streamer.hpp"
#include "kernetile/fx/clip_stack.hpp"
#include "kernetile/cpu/cpu_rasterizer.hpp"
#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace kernetile {

struct RendererConfig {
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t atlas_size{1024};           // 动态字形/蒙版图集边长 (默认 1024x1024 仅 1MB，溢出自动驱逐；辅助/浮动窗可设为 512)
    bool prefer_warp{false};             // 是否优先使用 D3D12 WARP 软件适配器
    bool force_cpu_fallback{false};      // 是否强制使用纯 CPU 瓦片光栅器
    bool enable_occlusion_culling{true}; // 是否启用前端实心矩形遮挡剔除
};

struct FrameStats {
    uint32_t frame_index{0};
    uint32_t total_primitives{0};
    uint32_t total_tiles{0};
    uint32_t dirty_tiles{0};
    double   culling_ratio{0.0};       // 脏块剔除百分比 (0.0% - 100.0%)
    double   cpu_binning_time_us{0.0}; // CPU 瓦片分箱耗时 (微秒)
    double   render_time_us{0.0};      // 渲染通道耗时 (微秒)
};

// ============================================================================
// KerneTile 引擎统一工业级渲染器门面 API
// 串联 HAL、计算着色器、瓦片分箱、动态图集、文本排版、视觉特效与软件回退
// ============================================================================
class KerneTileRenderer {
public:
    static std::unique_ptr<KerneTileRenderer> create(const RendererConfig& config = {});
    virtual ~KerneTileRenderer() = default;

    virtual void resize(uint32_t new_width, uint32_t new_height) = 0;

    // 帧生命周期控制
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;

    // 绘制命令录入
    virtual void push_primitive(const core::UiPrimitive& prim) = 0;
    virtual uint32_t push_clip(math::Rect rect, float corner_radius = 0.0f) = 0;
    virtual void pop_clip() = 0;

    // 高层便捷绘制 API
    virtual void draw_rect(
        math::Rect rect,
        math::Color color,
        float corner_radius = 0.0f,
        float border_width = 0.0f) = 0;

    virtual void draw_shadowed_box(
        math::Rect rect,
        math::Color color,
        float corner_radius,
        float shadow_blur,
        float shadow_spread,
        math::Vec2 shadow_offset) = 0;

    virtual void draw_text(
        std::string_view text,
        float x,
        float y,
        float font_size,
        math::Color color) = 0;

    virtual void draw_text_scaled(std::string_view text, float x, float y, float font_size,
                                  math::Color color, float scale, uint16_t font_weight = 400, bool italic = false) {
        (void)font_weight; (void)italic;
        draw_text(text, x * scale, y * scale, font_size * scale, color);
    }

    // R8 Alpha 蒙版缓存绘制 (供 NanoSVG 矢量图标等光栅化至 DynamicAtlas 后单图元极速着色渲染)
    [[nodiscard]] virtual bool has_cached_mask(uint64_t cache_key) const noexcept {
        (void)cache_key;
        return false;
    }

    virtual void draw_alpha_mask(
        uint64_t cache_key,
        math::Rect dst_rect_px,
        uint32_t mask_width,
        uint32_t mask_height,
        const uint8_t* alpha_pixels,
        math::Color tint) {
        (void)cache_key; (void)dst_rect_px; (void)mask_width; (void)mask_height; (void)alpha_pixels; (void)tint;
    }

    // 离屏渲染呈现输出至内存缓冲 (RGBA8 格式)
    virtual void render_to_rgba8(uint8_t* dst_buffer) = 0;

    virtual void set_compute_target(void* window, void* display = nullptr) { (void)window; (void)display; }
    virtual bool compute_ready() const noexcept { return false; }
    virtual bool present_computed_frame(bool force_present = false) { (void)force_present; return false; }

    // 自算云母背景 (4x4 屏幕空间模糊壁纸色场，D3D12 / Vulkan / WebGPU / CPU 全后端零额外纹理一致渲染)
    virtual void set_mica_backdrop(bool enabled, const std::array<uint32_t, 16>& grid) {
        (void)enabled; (void)grid;
    }

    // 性能指标查询
    virtual const FrameStats& get_last_frame_stats() const = 0;

    // 获取上一帧脏区域外接矩形 (像素坐标)，若无脏区则返回 false
    virtual bool get_last_dirty_bounds(uint32_t& out_x, uint32_t& out_y, uint32_t& out_w, uint32_t& out_h) const {
        (void)out_x; (void)out_y; (void)out_w; (void)out_h;
        return false;
    }

    // 获取底层设备
    virtual hal::GraphicsDevice* get_device() const = 0;
    virtual bool is_hardware_accelerated() const = 0;
};

} // namespace kernetile
