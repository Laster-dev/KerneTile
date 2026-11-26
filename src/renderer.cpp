#include "kernetile/renderer.hpp"
#include <chrono>
#include <cstring>
#include <unordered_map>
#include "compute/ui_compute.hpp"

namespace kernetile {

class KerneTileRendererImpl : public KerneTileRenderer {
public:
    explicit KerneTileRendererImpl(const RendererConfig& config)
        : m_config(config),
          m_grid(config.width, config.height),
          m_dirty_tracker(m_grid.get_total_tiles()),
          m_atlas(std::clamp(config.atlas_size, 256u, 4096u),
                  std::clamp(config.atlas_size, 256u, 4096u),
                  atlas::AtlasFormat::R8_UNORM)
    {


        // 2. 初始化文本与字形图集流式通道
        m_text_streamer = std::make_unique<text::TextStreamer>(text::shared_text_engine(), m_atlas);
    }

    void resize(uint32_t new_width, uint32_t new_height) override {
        m_config.width = new_width;
        m_config.height = new_height;
        m_grid = tile::TileGrid(new_width, new_height);
        m_dirty_tracker = tile::DirtyTracker(m_grid.get_total_tiles());
        m_dirty_tracker.reset(); // 强制全量重绘 (包含空/Mica 背景瓦片)，彻底杜绝宽变步长错位花屏
        m_gpu_full_frame = true;
    }

    void begin_frame() override {
        // 图集满载或发生溢出时在帧首执行分代安全驱逐，避免长时运行丢字或破坏当帧图元 UV
        if (m_atlas.has_overflowed() || m_atlas.get_utilization() >= 0.92) {
            m_atlas.reset();
            m_mask_cache.clear();
            if (m_text_streamer) {
                m_text_streamer->clear_cache();
            }
            m_dirty_tracker.reset();
            m_gpu_full_frame = true;
        }
        if (m_color_atlas && (m_color_atlas->has_overflowed() || m_color_atlas->get_utilization() >= 0.92)) {
            m_color_atlas->reset();
            m_image_cache.clear();
            m_dirty_tracker.reset();
            m_gpu_full_frame = true;
        }
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
        draw_text_scaled(text, x, y, font_size, color, 1.0f);
    }

    void draw_text_scaled(std::string_view text, float x, float y, float font_size,
                          math::Color color, float scale, uint16_t font_weight = 400,
                          bool italic = false, std::string_view font_family = "") override {
        if (!m_text_streamer) return;
        uint16_t clip_idx = (m_clip_stack.depth() > 0) 
            ? static_cast<uint16_t>(m_clip_stack.current_clip_index()) 
            : core::INVALID_INDEX_U16;

        m_text_streamer->stream_text(
            text, x, y, font_size, color,
            m_primitives, clip_idx, scale, font_weight, italic, font_family);
    }

    bool has_cached_mask(uint64_t cache_key) const noexcept override {
        return m_mask_cache.find(cache_key) != m_mask_cache.end();
    }

    void draw_alpha_mask(
        uint64_t cache_key,
        math::Rect dst_rect_px,
        uint32_t mask_width,
        uint32_t mask_height,
        const uint8_t* alpha_pixels,
        math::Color tint) override
    {
        if (tint.a <= 0.001f || mask_width == 0 || mask_height == 0) return;

        auto it = m_mask_cache.find(cache_key);
        if (it == m_mask_cache.end()) {
            if (!alpha_pixels) return;
            auto reg = m_atlas.allocate_and_upload(mask_width, mask_height, alpha_pixels, mask_width);
            if (!reg.has_value()) return;
            it = m_mask_cache.emplace(cache_key, *reg).first;
        }

        const auto& region = it->second;
        core::UiPrimitive prim{};
        prim.min_x = dst_rect_px.min_x;
        prim.min_y = dst_rect_px.min_y;
        prim.max_x = dst_rect_px.max_x;
        prim.max_y = dst_rect_px.max_y;

        prim.color_r = tint.r * tint.a;
        prim.color_g = tint.g * tint.a;
        prim.color_b = tint.b * tint.a;
        prim.color_a = tint.a;

        prim.atlas_u0 = region.u0;
        prim.atlas_v0 = region.v0;
        prim.atlas_u1 = region.u1;
        prim.atlas_v1 = region.v1;
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph);

        push_primitive(prim);
    }

    bool has_cached_image(uint64_t cache_key) const noexcept override {
        return m_image_cache.find(cache_key) != m_image_cache.end();
    }

    void invalidate_cached_image(uint64_t cache_key) override {
        if (m_image_cache.erase(cache_key) > 0) {
            m_dirty_tracker.reset();
            m_gpu_full_frame = true;
        }
    }

    void draw_image(
        uint64_t cache_key,
        math::Rect dst_rect_px,
        uint32_t img_width,
        uint32_t img_height,
        const uint8_t* rgba8_pixels,
        float corner_radius,
        math::Color tint,
        math::Rect uv_rect,
        uint32_t frame_revision) override
    {
        if (tint.a <= 0.001f || img_width == 0 || img_height == 0) return;

        if (!m_color_atlas) {
            uint32_t sz = std::clamp(m_config.color_atlas_size, 256u, 4096u);
            if (img_width + 2 > sz || img_height + 2 > sz) {
                sz = std::min(4096u, std::max(sz, std::max(img_width + 2, img_height + 2)));
            }
            m_color_atlas = std::make_unique<atlas::DynamicAtlas>(sz, sz, atlas::AtlasFormat::RGBA8_UNORM);
        }

        auto it = m_image_cache.find(cache_key);
        if (it != m_image_cache.end()) {
            // 同 cache_key 尺寸变化时淘汰旧槽位；尺寸一致且 frame_revision 变化时原地覆写像素（零分配、零 reset 频闪）
            auto& entry = it->second;
            if (entry.region.content_rect.w != img_width || entry.region.content_rect.h != img_height) {
                m_image_cache.erase(it);
                it = m_image_cache.end();
            } else if (entry.revision != frame_revision && rgba8_pixels) {
                m_color_atlas->update_region(entry.region, rgba8_pixels,
                                             static_cast<size_t>(img_width) * 4u, true);
                entry.revision = frame_revision;
            }
        }
        if (it == m_image_cache.end()) {
            if (!rgba8_pixels) return;
            auto reg = m_color_atlas->allocate_and_upload(img_width, img_height, rgba8_pixels,
                                                          static_cast<size_t>(img_width) * 4u, true);
            if (!reg.has_value()) {
                // 若当帧彩色图集空间不足，重建/扩容后重试一次
                uint32_t cur_sz = m_color_atlas->get_width();
                uint32_t req_sz = std::max(img_width + 2, img_height + 2);
                uint32_t new_sz = std::min(4096u, std::max(cur_sz, req_sz));
                if (new_sz > cur_sz) {
                    m_color_atlas = std::make_unique<atlas::DynamicAtlas>(new_sz, new_sz, atlas::AtlasFormat::RGBA8_UNORM);
                } else {
                    m_color_atlas->reset();
                }
                m_image_cache.clear();
                m_dirty_tracker.reset();
                m_gpu_full_frame = true;
                reg = m_color_atlas->allocate_and_upload(img_width, img_height, rgba8_pixels,
                                                         static_cast<size_t>(img_width) * 4u, true);
                if (!reg.has_value()) return;
            }
            it = m_image_cache.emplace(cache_key, CachedImageEntry{*reg, frame_revision}).first;
        }

        const auto& entry = it->second;
        const auto& region = entry.region;
        const float u0_base = static_cast<float>(region.u0) * (1.0f / 65535.0f);
        const float v0_base = static_cast<float>(region.v0) * (1.0f / 65535.0f);
        const float u1_base = static_cast<float>(region.u1) * (1.0f / 65535.0f);
        const float v1_base = static_cast<float>(region.v1) * (1.0f / 65535.0f);

        const float sub_u0 = std::clamp(uv_rect.min_x, 0.0f, 1.0f);
        const float sub_v0 = std::clamp(uv_rect.min_y, 0.0f, 1.0f);
        const float sub_u1 = std::clamp(uv_rect.max_x, 0.0f, 1.0f);
        const float sub_v1 = std::clamp(uv_rect.max_y, 0.0f, 1.0f);

        const float fin_u0 = u0_base + (u1_base - u0_base) * sub_u0;
        const float fin_v0 = v0_base + (v1_base - v0_base) * sub_v0;
        const float fin_u1 = u0_base + (u1_base - u0_base) * sub_u1;
        const float fin_v1 = v0_base + (v1_base - v0_base) * sub_v1;

        core::UiPrimitive prim{};
        prim.min_x = dst_rect_px.min_x;
        prim.min_y = dst_rect_px.min_y;
        prim.max_x = dst_rect_px.max_x;
        prim.max_y = dst_rect_px.max_y;

        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;

        prim.color_r = tint.r * tint.a;
        prim.color_g = tint.g * tint.a;
        prim.color_b = tint.b * tint.a;
        prim.color_a = tint.a;

        prim.atlas_u0 = atlas::AtlasRegion::to_fixed_uv(fin_u0);
        prim.atlas_v0 = atlas::AtlasRegion::to_fixed_uv(fin_v0);
        prim.atlas_u1 = atlas::AtlasRegion::to_fixed_uv(fin_u1);
        prim.atlas_v1 = atlas::AtlasRegion::to_fixed_uv(fin_v1);
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::IsIcon);
        // 动图切帧时把 frame_revision 写入未用的 shadow_offset_y，使 DirtyTracker 仅标脏该图像覆盖的瓦片
        prim.shadow_offset_y = static_cast<float>(entry.revision & 0xFFFFu);

        push_primitive(prim);
    }

    void end_frame() override {
        auto t0 = std::chrono::high_resolution_clock::now();

        // 1. 瓦片网格图元分箱、裁剪链求交与实心遮挡剔除
        const auto& clips = m_clip_stack.get_clips();
        m_last_binning = m_binner.bin_primitives(
            m_grid,
            m_primitives,
            m_config.enable_occlusion_culling,
            clips);

        auto t1 = std::chrono::high_resolution_clock::now();

        // 2. FNV-1a 脏瓦片哈希追踪 (按图元内容与裁剪几何链精确局部标脏，避免 Clip 变动全屏失效)
        m_last_dirty_result = m_dirty_tracker.update(
            m_grid,
            m_last_binning.headers,
            m_last_binning.indices,
            m_primitives,
            clips);

        // 记录性能与帧指标
        m_last_stats.frame_index = m_frame_index;
        m_last_stats.total_primitives = static_cast<uint32_t>(m_primitives.size());
        m_last_stats.total_tiles = m_grid.get_total_tiles();
        m_last_stats.dirty_tiles = m_last_dirty_result.dirty_count;
        m_last_stats.culling_ratio = m_last_dirty_result.culling_ratio * 100.0;
        m_last_stats.cpu_binning_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }

    void set_compute_target(void* window, void* display) override {
        m_compute_window = window;
        m_compute_display = display;
        m_backend = m_config.force_cpu_fallback ? nullptr : compute::create(window, display, m_config.prefer_warp);
        m_gpu_full_frame = true;
        m_gpu_active = false;
    }

    bool compute_ready() const noexcept override { return m_backend && m_backend->ready(); }

    void set_mica_backdrop(bool enabled, const std::array<uint32_t, 16>& grid) override {
        if (m_mica_enabled != enabled || (enabled && m_mica_grid != grid)) {
            m_mica_enabled = enabled;
            m_mica_grid = grid;
            m_dirty_tracker.reset();
            m_gpu_full_frame = true;
        }
    }

    bool present_computed_frame(bool force_present) override {
        if (compute_ready()) {
            const auto& dirty = m_last_dirty_result.dirty_tiles;
            if (!m_gpu_full_frame && dirty.empty() && !force_present) return m_gpu_active;
            m_compute_tiles.clear();
            auto append = [this](uint32_t x, uint32_t y, uint32_t id) {
                const auto& tile = m_last_binning.headers[id];
                m_compute_tiles.push_back({x, y, tile.primitive_offset, tile.primitive_count});
            };
            if (m_gpu_full_frame) {
                for (uint32_t id = 0; id < m_grid.get_total_tiles(); ++id) {
                    const auto coord = m_grid.get_tile_coord(id);
                    append(coord.x, coord.y, id);
                }
            } else {
                for (const auto& tile : dirty) append(tile.tx, tile.ty, tile.tile_id);
            }
            const uint32_t atlas_width = m_atlas.get_width();
            const uint32_t atlas_height = m_atlas.get_height();
            const uint32_t color_atlas_width = m_color_atlas ? m_color_atlas->get_width() : 0u;
            const uint32_t color_atlas_height = m_color_atlas ? m_color_atlas->get_height() : 0u;
            const std::span<const uint8_t> color_atlas_span = m_color_atlas
                ? std::span<const uint8_t>{m_color_atlas->get_pixels(), size_t(color_atlas_width) * color_atlas_height * 4u}
                : std::span<const uint8_t>{};
            const std::span<const atlas::RectU32> color_atlas_updates = m_color_atlas
                ? m_color_atlas->get_dirty_rects()
                : std::span<const atlas::RectU32>{};
            const compute::Frame frame{m_config.width, m_config.height, m_primitives,
                m_last_binning.indices, m_compute_tiles, m_clip_stack.get_clips(),
                {m_atlas.get_pixels(), size_t(atlas_width) * atlas_height}, atlas_width,
                atlas_height, m_atlas.get_dirty_rects(), m_mica_enabled, m_mica_grid,
                color_atlas_span, color_atlas_width, color_atlas_height, color_atlas_updates};
            const auto start = std::chrono::steady_clock::now();
            if (m_backend->submit(frame)) {
                m_atlas.clear_dirty_rects();
                if (m_color_atlas) m_color_atlas->clear_dirty_rects();
                m_gpu_full_frame = false;
                m_gpu_active = true;
                m_last_stats.render_time_us =
                    std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
                return true;
            }
            // 若 GPU 设备发生异常重置/TDR，立即原地重建 D3D12 后端并重试，绝不让旧交换链冻结卡死在窗口上
            if (m_compute_window && !m_config.force_cpu_fallback) {
                m_backend.reset();
                m_backend = compute::create(m_compute_window, m_compute_display, m_config.prefer_warp);
                m_gpu_full_frame = true;
                if (m_backend && m_backend->ready()) {
                    m_compute_tiles.clear();
                    for (uint32_t id = 0; id < m_grid.get_total_tiles(); ++id) {
                        const auto coord = m_grid.get_tile_coord(id);
                        append(coord.x, coord.y, id);
                    }
                    const compute::Frame retry_frame{m_config.width, m_config.height, m_primitives,
                        m_last_binning.indices, m_compute_tiles, m_clip_stack.get_clips(),
                        {m_atlas.get_pixels(), size_t(atlas_width) * atlas_height}, atlas_width,
                        atlas_height, m_atlas.get_dirty_rects(), m_mica_enabled, m_mica_grid,
                        color_atlas_span, color_atlas_width, color_atlas_height, color_atlas_updates};
                    if (m_backend->submit(retry_frame)) {
                        m_atlas.clear_dirty_rects();
                        if (m_color_atlas) m_color_atlas->clear_dirty_rects();
                        m_gpu_full_frame = false;
                        m_gpu_active = true;
                        return true;
                    }
                }
            }
        }
        if (m_gpu_active) {
            m_dirty_tracker.reset();
            m_last_dirty_result = m_dirty_tracker.update(m_grid, m_last_binning.headers,
                m_last_binning.indices, m_primitives, m_clip_stack.get_clips());
            m_last_stats.dirty_tiles = m_last_dirty_result.dirty_count;
        }
        m_gpu_full_frame = true;
        m_gpu_active = false;
        return false;
    }

    void render_to_rgba8(uint8_t* dst_buffer) override {
        if (!dst_buffer) return;
        if (m_last_dirty_result.dirty_count == 0) return; // 零脏瓦片直接跳过，零开销！

        auto t0 = std::chrono::high_resolution_clock::now();

        // 仅收集真正发生几何、色彩或层叠变化的脏瓦片 (复用持久内存，零堆分配)
        m_dirty_tile_ids.clear();
        m_dirty_tile_ids.reserve(m_last_dirty_result.dirty_count);
        for (const auto& dt : m_last_dirty_result.dirty_tiles) {
            m_dirty_tile_ids.push_back(dt.tile_id);
        }

        // 调用 CPU 软件光栅引擎 (即使在无 GPU 硬件环境下依然能以极致速度精准渲染)
        cpu::CpuTileRasterizer::RenderParams params{};
        params.width = m_config.width;
        params.height = m_config.height;
        params.primitives = m_primitives;
        params.tile_indices = m_last_binning.indices;
        params.tile_headers = m_last_binning.headers;
        params.dirty_tiles = m_dirty_tile_ids;
        params.clip_rects = m_clip_stack.get_clips();
        params.atlas_pixels = m_atlas.get_pixels();
        params.atlas_width = m_atlas.get_width();
        params.atlas_height = m_atlas.get_height();
        params.color_atlas_pixels = m_color_atlas ? m_color_atlas->get_pixels() : nullptr;
        params.color_atlas_width = m_color_atlas ? m_color_atlas->get_width() : 0u;
        params.color_atlas_height = m_color_atlas ? m_color_atlas->get_height() : 0u;
        params.mica_enabled = m_mica_enabled;
        params.mica_grid = m_mica_grid;

        cpu::CpuTileRasterizer::render(params, dst_buffer);

        // CPU 渲染路径下同样消费掉图集脏矩形记录，防止动图播放时 m_dirty_rects 无限累积
        m_atlas.clear_dirty_rects();
        if (m_color_atlas) m_color_atlas->clear_dirty_rects();

        auto t1 = std::chrono::high_resolution_clock::now();
        m_last_stats.render_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }

    bool get_last_dirty_bounds(uint32_t& out_x, uint32_t& out_y, uint32_t& out_w, uint32_t& out_h) const override {
        if (m_last_dirty_result.dirty_count == 0) return false;
        uint32_t min_tx = UINT32_MAX, min_ty = UINT32_MAX;
        uint32_t max_tx = 0, max_ty = 0;
        for (const auto& dt : m_last_dirty_result.dirty_tiles) {
            min_tx = std::min(min_tx, dt.tx);
            min_ty = std::min(min_ty, dt.ty);
            max_tx = std::max(max_tx, dt.tx);
            max_ty = std::max(max_ty, dt.ty);
        }
        out_x = min_tx * 16;
        out_y = min_ty * 16;
        out_w = std::min((max_tx + 1) * 16, m_config.width) - out_x;
        out_h = std::min((max_ty + 1) * 16, m_config.height) - out_y;
        return true;
    }

    const FrameStats& get_last_frame_stats() const override {
        return m_last_stats;
    }

    hal::GraphicsDevice* get_device() const override {
        if (!m_config.force_cpu_fallback && !m_device) {
            m_device = hal::create_default_device(m_config.prefer_warp);
        }
        return m_device.get();
    }

    bool is_hardware_accelerated() const override {
        return m_gpu_active && compute_ready();
    }

private:
    RendererConfig m_config{};
    tile::TileGrid m_grid;
    tile::TileBinner m_binner{};
    tile::DirtyTracker m_dirty_tracker;
    fx::ClipStack m_clip_stack{};
    atlas::DynamicAtlas m_atlas;
    std::unique_ptr<atlas::DynamicAtlas> m_color_atlas{};
    struct CachedImageEntry {
        atlas::AtlasRegion region;
        uint32_t revision{0};
    };
    std::unordered_map<uint64_t, atlas::AtlasRegion> m_mask_cache{};
    std::unordered_map<uint64_t, CachedImageEntry> m_image_cache{};

    std::unique_ptr<text::TextStreamer> m_text_streamer{};
    mutable std::unique_ptr<hal::GraphicsDevice> m_device;
    std::unique_ptr<compute::Backend> m_backend;
    void* m_compute_window{nullptr};
    void* m_compute_display{nullptr};
    std::vector<compute::Tile> m_compute_tiles;
    bool m_gpu_full_frame = true;
    bool m_gpu_active = false;
    bool m_mica_enabled = false;
    std::array<uint32_t, 16> m_mica_grid{};

    std::vector<core::UiPrimitive> m_primitives{};
    std::vector<uint32_t> m_dirty_tile_ids{};
    tile::BinningResult m_last_binning{};
    tile::DirtyTrackingResult m_last_dirty_result{};

    uint32_t m_frame_index{0};

    FrameStats m_last_stats{};
};

std::unique_ptr<KerneTileRenderer> KerneTileRenderer::create(const RendererConfig& config) {
    return std::make_unique<KerneTileRendererImpl>(config);
}

} // namespace kernetile
