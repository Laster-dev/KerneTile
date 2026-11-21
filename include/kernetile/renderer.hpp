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
#include <memory>
#include <string_view>
#include <vector>

namespace kernetile {

struct RendererConfig {
    uint32_t width{1920};
    uint32_t height{1080};
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

    // 离屏渲染呈现输出至内存缓冲 (RGBA8 格式)
    virtual void render_to_rgba8(uint8_t* dst_buffer) = 0;

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
