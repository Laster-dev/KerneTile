#include "kernetile/renderer.hpp"
#include <chrono>
#include <iostream>

namespace kernetile {

class KerneTileRendererImpl : public KerneTileRenderer {
public:
    explicit KerneTileRendererImpl(const RendererConfig& config)
        : m_config(config),
          m_grid(config.width, config.height),
          m_dirty_tracker(m_grid.get_total_tiles()),
          m_atlas(2048, 2048, atlas::AtlasFormat::R8_UNORM)
    {
        // 1. 初始化图形设备 (支持跨平台自动降级至 WARP 或纯 CPU)
        if (!m_config.force_cpu_fallback) {
#if defined(_WIN32)
            try {
                m_device = hal::create_d3d12_device(m_config.prefer_warp);
                m_is_hardware = true;
            } catch (const std::exception& e) {
                std::cerr << "[KerneTile] Direct3D 12 device creation failed (" << e.what() 
                          << "), falling back to pure CPU rasterizer." << std::endl;
                m_device.reset();
                m_is_hardware = false;
            }
#else
            m_device = hal::create_default_device(m_config.prefer_warp);
            m_is_hardware = false; // Linux 默认启用纯 CPU 极速多线程瓦片光栅器
#endif
        } else {
            m_is_hardware = false;
        }

        // 2. 初始化文本与字形图集流式通道
        m_text_engine = text::create_text_engine();
        m_text_streamer = std::make_unique<text::TextStreamer>(*m_text_engine, m_atlas);
    }

    void resize(uint32_t new_width, uint32_t new_height) override {
        m_config.width = new_width;
        m_config.height = new_height;
        m_grid = tile::TileGrid(new_width, new_height);
        m_dirty_tracker = tile::DirtyTracker(m_grid.get_total_tiles());
    }

    void begin_frame() override {
        m_primitives.clear();
        m_clip_stack.clear();
        m_frame_index++;
    }

    void push_primitive(const core::UiPrimitive& prim) override {
        core::UiPrimitive p = prim;
        if (p.clip_index == core::INVALID_INDEX_U16 && m_clip_stack.depth() > 0) {
            p.clip_index = static_cast<uint16_t>(m_clip_stack.current_clip_index());
        }
        m_primitives.push_back(p);
    }

    uint32_t push_clip(math::Rect rect, float corner_radius) override {
        return m_clip_stack.push_clip(rect, corner_radius);
    }

    void pop_clip() override {
        m_clip_stack.pop_clip();
    }

    void draw_rect(
        math::Rect rect,
        math::Color color,
        float corner_radius,
        float border_width) override
    {
        core::UiPrimitive prim{};
        prim.min_x = rect.min_x;
        prim.min_y = rect.min_y;
        prim.max_x = rect.max_x;
        prim.max_y = rect.max_y;

        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;

        // 严格遵循 UiPrimitive 不变量契约: 转换为线性光空间预乘颜色 (0 <= C <= A <= 1)
        // 彻底杜绝半透明图元在光栅化混合时因直通色导致的 RGB 饱和白爆死
        prim.color_r = color.r * color.a;
        prim.color_g = color.g * color.a;
        prim.color_b = color.b * color.a;
        prim.color_a = color.a;

        prim.border_width = border_width;
        push_primitive(prim);
    }

    void draw_shadowed_box(
        math::Rect rect,
        math::Color color,
        float corner_radius,
        float shadow_blur,
        float shadow_spread,
        math::Vec2 shadow_offset) override
    {
        core::UiPrimitive prim{};
        prim.min_x = rect.min_x;
        prim.min_y = rect.min_y;
        prim.max_x = rect.max_x;
        prim.max_y = rect.max_y;

        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;

        // 严格遵循 UiPrimitive 不变量契约: 转换为线性光空间预乘颜色 (0 <= C <= A <= 1)
        // 彻底杜绝半透明图元在光栅化混合时因直通色导致的 RGB 饱和白爆死
        prim.color_r = color.r * color.a;
        prim.color_g = color.g * color.a;
        prim.color_b = color.b * color.a;
        prim.color_a = color.a;

        prim.shadow_blur = shadow_blur;
        prim.shadow_spread = shadow_spread;
        prim.shadow_offset_x = shadow_offset.x;
        prim.shadow_offset_y = shadow_offset.y;

        push_primitive(prim);
    }

    void draw_text(
        std::string_view text,
        float x,
        float y,
        float font_size,
        math::Color color) override
    {
        if (!m_text_streamer) return;
        uint16_t clip_idx = (m_clip_stack.depth() > 0) 
            ? static_cast<uint16_t>(m_clip_stack.current_clip_index()) 
            : core::INVALID_INDEX_U16;

        m_text_streamer->stream_text(
            text, x, y, font_size, color,
            m_primitives, clip_idx);
    }

    void end_frame() override {
        auto t0 = std::chrono::high_resolution_clock::now();

        // 1. 瓦片网格图元分箱与实心遮挡剔除
        m_last_binning = m_binner.bin_primitives(
            m_grid,
            m_primitives,
            m_config.enable_occlusion_culling);

        auto t1 = std::chrono::high_resolution_clock::now();

        // 2. FNV-1a 脏瓦片哈希追踪
        auto dirty_res = m_dirty_tracker.update(
            m_grid,
            m_last_binning.headers,
            m_last_binning.indices,
            m_primitives);

        // 记录性能与帧指标
        m_last_stats.frame_index = m_frame_index;
        m_last_stats.total_primitives = static_cast<uint32_t>(m_primitives.size());
        m_last_stats.total_tiles = m_grid.get_total_tiles();
        m_last_stats.dirty_tiles = dirty_res.dirty_count;
        m_last_stats.culling_ratio = dirty_res.culling_ratio * 100.0;
        m_last_stats.cpu_binning_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }

    void render_to_rgba8(uint8_t* dst_buffer) override {
        if (!dst_buffer) return;

        auto t0 = std::chrono::high_resolution_clock::now();

        // 收集所有脏瓦片索引
        std::vector<uint32_t> dirty_tile_ids;
        dirty_tile_ids.reserve(m_last_stats.dirty_tiles);
        for (uint32_t i = 0; i < m_grid.get_total_tiles(); ++i) {
            if ((m_last_binning.headers[i].primitive_count) > 0) {
                dirty_tile_ids.push_back(i);
            }
        }

        // 调用 CPU 软件光栅引擎 (即使在无 GPU 硬件环境下依然能以极致速度精准渲染)
        cpu::CpuTileRasterizer::RenderParams params{};
        params.width = m_config.width;
        params.height = m_config.height;
        params.primitives = m_primitives;
        params.tile_indices = m_last_binning.indices;
        params.tile_headers = m_last_binning.headers;
        params.dirty_tiles = dirty_tile_ids;
        params.clip_rects = m_clip_stack.get_clips();
        params.atlas_pixels = m_atlas.get_pixels();
        params.atlas_width = m_atlas.get_width();
        params.atlas_height = m_atlas.get_height();

        cpu::CpuTileRasterizer::render(params, dst_buffer);

        auto t1 = std::chrono::high_resolution_clock::now();
        m_last_stats.render_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }

    const FrameStats& get_last_frame_stats() const override {
        return m_last_stats;
    }

    hal::GraphicsDevice* get_device() const override {
        return m_device.get();
    }

    bool is_hardware_accelerated() const override {
        return m_is_hardware;
    }

private:
    RendererConfig m_config{};
    tile::TileGrid m_grid;
    tile::TileBinner m_binner{};
    tile::DirtyTracker m_dirty_tracker;
    fx::ClipStack m_clip_stack{};
    atlas::DynamicAtlas m_atlas;

    std::unique_ptr<text::ITextEngine> m_text_engine{};
    std::unique_ptr<text::TextStreamer> m_text_streamer{};
    std::unique_ptr<hal::GraphicsDevice> m_device{};

    std::vector<core::UiPrimitive> m_primitives{};
    tile::BinningResult m_last_binning{};

    uint32_t m_frame_index{0};
    bool m_is_hardware{false};
    FrameStats m_last_stats{};
};

std::unique_ptr<KerneTileRenderer> KerneTileRenderer::create(const RendererConfig& config) {
    return std::make_unique<KerneTileRendererImpl>(config);
}

} // namespace kernetile
