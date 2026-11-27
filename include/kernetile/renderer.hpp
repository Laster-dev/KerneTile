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
    uint32_t color_atlas_size{256};      // 彩色 RGBA8 位图图集初始边长 (按需惰性分配，初始 256x256 仅 256KB，超容自动扩容)
    bool prefer_warp{false};             // 是否优先使用 D3D12 WARP 软件适配器
    bool force_cpu_fallback{false};      // 是否强制使用纯 CPU 瓦片光栅器
    bool enable_occlusion_culling{true}; // 是否启用前端实心矩形遮挡剔除
    bool enable_dithering{true};         // R1: 是否启用 8-bit IGN 抖动降噪 (默认开启，可手动关闭)
    bool enable_pixel_snapping{true};    // R2: 是否启用分数 DPI 物理像素网格对齐 (默认开启，可手动关闭)
    bool enable_font_gamma{true};        // 暗色/亮色文字光渗补偿 (Irradiation Illusion Compensation，默认开启，可手动关闭)
    bool enable_acrylic_grain{false};    // 零纹理亚克力微颗粒噪点 (Zero-Texture Acrylic Micro-Grain，按需开启)
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
    virtual uint32_t push_feathered_clip(
        math::Rect rect,
        float corner_radius = 0.0f,
        float top_feather_px = 0.0f,
        float bottom_feather_px = 0.0f) {
        (void)top_feather_px; (void)bottom_feather_px;
        return push_clip(rect, corner_radius);
    }
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
                                  math::Color color, float scale, uint16_t font_weight = 400,
                                  bool italic = false, std::string_view font_family = "",
                                  bool tabular_nums = false, bool font_gamma = true) {
        (void)font_weight; (void)italic; (void)font_family; (void)tabular_nums; (void)font_gamma;
        draw_text(text, x * scale, y * scale, font_size * scale, color);
    }

    virtual void set_dithering_enabled(bool enabled) { (void)enabled; }
    [[nodiscard]] virtual bool is_dithering_enabled() const noexcept { return true; }

    virtual void set_pixel_snapping_enabled(bool enabled) { (void)enabled; }
    [[nodiscard]] virtual bool is_pixel_snapping_enabled() const noexcept { return true; }

    virtual void set_font_gamma_enabled(bool enabled) { (void)enabled; }
    [[nodiscard]] virtual bool is_font_gamma_enabled() const noexcept { return true; }

    virtual void set_acrylic_grain_enabled(bool enabled) { (void)enabled; }
    [[nodiscard]] virtual bool is_acrylic_grain_enabled() const noexcept { return false; }

    virtual void draw_linear_gradient(
        math::Rect rect,
        math::Color start_color,
        math::Color end_color,
        math::Vec2 start_pt,
        math::Vec2 end_pt,
        float corner_radius = 0.0f,
        float border_width = 0.0f,
        bool oklab = true) {
        (void)rect; (void)start_color; (void)end_color; (void)start_pt; (void)end_pt;
        (void)corner_radius; (void)border_width; (void)oklab;
    }

    // 方向性玻璃高光内描边 (Directional Fresnel Rim Highlight)
    virtual void draw_directional_rim(
        math::Rect rect,
        math::Color top_highlight,
        math::Color bottom_shadow,
        float corner_radius = 8.0f,
        float border_width = 1.0f,
        math::Vec2 light_dir = {0.0f, -1.0f},
        float inner_glow = 0.35f) {
        (void)rect; (void)top_highlight; (void)bottom_shadow;
        (void)corner_radius; (void)border_width; (void)light_dir; (void)inner_glow;
    }

    // 零纹理亚克力微颗粒噪点表面 (Zero-Texture Acrylic Micro-Grain)
    virtual void draw_acrylic_panel(
        math::Rect rect,
        math::Color tint,
        float corner_radius = 8.0f,
        float grain_amount = 0.028f,
        float grain_scale = 1.0f) {
        (void)rect; (void)tint; (void)corner_radius; (void)grain_amount; (void)grain_scale;
    }

    // 解析高斯内阴影 (Analytic SDF Inset Shadow)
    virtual void draw_inset_shadow(
        math::Rect rect,
        math::Color shadow_color,
        float corner_radius = 6.0f,
        float blur_sigma = 6.0f,
        float spread = 0.0f,
        math::Vec2 offset = {0.0f, 2.0f}) {
        (void)rect; (void)shadow_color; (void)corner_radius; (void)blur_sigma; (void)spread; (void)offset;
    }

    // 灵动岛式液态融合 (SDF Smooth-Minimum Metaballs)
    virtual void draw_metaball(
        math::Rect rect_a,
        float radius_a,
        math::Rect rect_b,
        float radius_b,
        float smooth_k,
        math::Color color_a,
        math::Color color_b = {},
        bool two_tone = false,
        float border_width = 0.0f) {
        (void)rect_a; (void)radius_a; (void)rect_b; (void)radius_b; (void)smooth_k;
        (void)color_a; (void)color_b; (void)two_tone; (void)border_width;
    }

    virtual void draw_pointer_reveal(
        math::Rect widget_bounds,
        math::Vec2 cursor_pos,
        float radius,
        math::Color color,
        float corner_radius = 0.0f,
        float border_width = 0.0f,
        float edge_intensity = 1.0f) {
        (void)widget_bounds; (void)cursor_pos; (void)radius; (void)color;
        (void)corner_radius; (void)border_width; (void)edge_intensity;
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

    // 彩色 RGBA8 位图缓存绘制 (支持圆角裁剪、着色调制与子区域 UV 裁剪，覆盖 CPU / D3D12 / Vulkan / WebGPU 全后端)
    [[nodiscard]] virtual bool has_cached_image(uint64_t cache_key) const noexcept {
        (void)cache_key;
        return false;
    }

    virtual void invalidate_cached_image(uint64_t cache_key) {
        (void)cache_key;
    }

    virtual void draw_image(
        uint64_t cache_key,
        math::Rect dst_rect_px,
        uint32_t img_width,
        uint32_t img_height,
        const uint8_t* rgba8_pixels,
        float corner_radius = 0.0f,
        math::Color tint = {1.0f, 1.0f, 1.0f, 1.0f},
        math::Rect uv_rect = {0.0f, 0.0f, 1.0f, 1.0f},
        uint32_t frame_revision = 0) {
        (void)cache_key; (void)dst_rect_px; (void)img_width; (void)img_height;
        (void)rgba8_pixels; (void)corner_radius; (void)tint; (void)uv_rect; (void)frame_revision;
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

#if !defined(NDEBUG)
    // 仅 Debug 模式编译：标记业务场景图元录制结束（后续推入的调试浮层不计入场景脏瓦片统计）
    virtual void mark_scene_end() {}

    // 仅 Debug 模式编译：业务场景（不含调试浮层）帧统计与脏瓦片列表，供 F12 DevTools 零自激观测使用
    virtual const FrameStats& get_last_scene_stats() const {
        return get_last_frame_stats();
    }

    virtual const std::vector<tile::DirtyTileInfo>& get_last_scene_dirty_tiles() const {
        return get_last_dirty_tiles();
    }
#endif

    // 性能指标查询
    virtual const FrameStats& get_last_frame_stats() const = 0;

    // 获取上一帧脏区域外接矩形 (像素坐标)，若无脏区则返回 false
    virtual bool get_last_dirty_bounds(uint32_t& out_x, uint32_t& out_y, uint32_t& out_w, uint32_t& out_h) const {
        (void)out_x; (void)out_y; (void)out_w; (void)out_h;
        return false;
    }

    // 获取上一帧所有脏瓦片列表
    virtual const std::vector<tile::DirtyTileInfo>& get_last_dirty_tiles() const {
        static const std::vector<tile::DirtyTileInfo> kEmpty{};
        return kEmpty;
    }

    // 获取底层设备
    virtual hal::GraphicsDevice* get_device() const = 0;
    virtual bool is_hardware_accelerated() const = 0;
};

} // namespace kernetile
