#include "kernetile/renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
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
#if !defined(NDEBUG)
          m_scene_dirty_tracker(m_grid.get_total_tiles()),
#endif
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
#if !defined(NDEBUG)
        m_scene_dirty_tracker = tile::DirtyTracker(m_grid.get_total_tiles());
        m_scene_dirty_tracker.reset();
#endif
        m_gpu_full_frame = true;
    }

    [[nodiscard]] bool has_atlas_overflowed() const noexcept override {
        return m_atlas.has_overflowed();
    }

    void begin_frame() override {
        // 图集满载或发生溢出时在帧首执行安全驱逐/倍增扩容，避免高 DPI + CJK 场景单帧循环溢出丢字
        if (m_atlas.has_overflowed() || m_atlas.get_utilization() >= 0.92) {
            const bool overflowed = m_atlas.has_overflowed();
            const uint32_t cur_sz = m_atlas.get_width();
            if (overflowed && cur_sz < 4096u) {
                const uint32_t next_sz = std::min(4096u, cur_sz * 2u);
                m_atlas = atlas::DynamicAtlas(next_sz, next_sz, atlas::AtlasFormat::R8_UNORM);
                m_text_streamer = std::make_unique<text::TextStreamer>(text::shared_text_engine(), m_atlas);
            } else {
                m_atlas.reset();
                if (m_text_streamer) {
                    m_text_streamer->clear_cache();
                }
            }
            m_mask_cache.clear();
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
#if !defined(NDEBUG)
        m_scene_prim_count = SIZE_MAX;
        m_has_scene_split = false;
#endif
        m_frame_index++;
    }

#if !defined(NDEBUG)
    void mark_scene_end() override {
        m_scene_prim_count = m_primitives.size();
        m_has_scene_split = true;
    }
#endif

    void push_primitive(const core::UiPrimitive& prim) override {
        core::UiPrimitive p = prim;
        if (p.clip_index == core::INVALID_INDEX_U16 && m_clip_stack.depth() > 0) {
            p.clip_index = static_cast<uint16_t>(m_clip_stack.current_clip_index());
        }
        if (m_config.enable_pixel_snapping) {
            snap_primitive_in_place(p);
        }
        m_primitives.push_back(p);
    }

    uint32_t push_clip(math::Rect rect, float corner_radius) override {
        return m_clip_stack.push_clip(rect, corner_radius);
    }

    uint32_t push_feathered_clip(
        math::Rect rect,
        float corner_radius,
        float top_feather_px,
        float bottom_feather_px) override
    {
        uint32_t flags = 0u;
        uint32_t top_q = 0u;
        uint32_t bot_q = 0u;
        if (top_feather_px > 0.1f) {
            flags |= core::kClipFlagFeatherTop;
            top_q = static_cast<uint32_t>(std::lround(std::clamp(top_feather_px, 0.0f, 4095.0f) * 16.0f)) & 0xFFFFu;
        }
        if (bottom_feather_px > 0.1f) {
            flags |= core::kClipFlagFeatherBottom;
            bot_q = static_cast<uint32_t>(std::lround(std::clamp(bottom_feather_px, 0.0f, 4095.0f) * 16.0f)) & 0xFFFFu;
        }
        const uint32_t reserved = top_q | (bot_q << 16u);
        return m_clip_stack.push_clip(rect, corner_radius, flags, reserved);
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
                          bool italic = false, std::string_view font_family = "",
                          bool tabular_nums = false, bool font_gamma = true) override {
        if (!m_text_streamer) return;
        uint16_t clip_idx = (m_clip_stack.depth() > 0) 
            ? static_cast<uint16_t>(m_clip_stack.current_clip_index()) 
            : core::INVALID_INDEX_U16;

        m_text_streamer->stream_text(
            text, x, y, font_size, color,
            m_primitives, clip_idx, scale, font_weight, italic, font_family, tabular_nums, font_gamma);
    }

    void set_dithering_enabled(bool enabled) override {
        if (m_config.enable_dithering != enabled) {
            m_config.enable_dithering = enabled;
            m_dirty_tracker.reset();
#if !defined(NDEBUG)
            m_scene_dirty_tracker.reset();
#endif
            m_gpu_full_frame = true;
        }
    }

    bool is_dithering_enabled() const noexcept override {
        return m_config.enable_dithering;
    }

    void set_pixel_snapping_enabled(bool enabled) override {
        if (m_config.enable_pixel_snapping != enabled) {
            m_config.enable_pixel_snapping = enabled;
            m_dirty_tracker.reset();
#if !defined(NDEBUG)
            m_scene_dirty_tracker.reset();
#endif
            m_gpu_full_frame = true;
        }
    }

    bool is_pixel_snapping_enabled() const noexcept override {
        return m_config.enable_pixel_snapping;
    }

    void set_font_gamma_enabled(bool enabled) override {
        if (m_config.enable_font_gamma != enabled) {
            m_config.enable_font_gamma = enabled;
            m_dirty_tracker.reset();
#if !defined(NDEBUG)
            m_scene_dirty_tracker.reset();
#endif
            m_gpu_full_frame = true;
        }
    }

    bool is_font_gamma_enabled() const noexcept override {
        return m_config.enable_font_gamma;
    }

    void set_acrylic_grain_enabled(bool enabled) override {
        if (m_config.enable_acrylic_grain != enabled) {
            m_config.enable_acrylic_grain = enabled;
            m_dirty_tracker.reset();
#if !defined(NDEBUG)
            m_scene_dirty_tracker.reset();
#endif
            m_gpu_full_frame = true;
        }
    }

    bool is_acrylic_grain_enabled() const noexcept override {
        return m_config.enable_acrylic_grain;
    }

    void draw_linear_gradient(
        math::Rect rect,
        math::Color start_color,
        math::Color end_color,
        math::Vec2 start_pt,
        math::Vec2 end_pt,
        float corner_radius = 0.0f,
        float border_width = 0.0f,
        bool oklab = true) override
    {
        auto pack_unorm16 = [](float v) noexcept -> uint16_t {
            return static_cast<uint16_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 65535.0f));
        };

        core::UiPrimitive prim{};
        prim.min_x = rect.min_x;
        prim.min_y = rect.min_y;
        prim.max_x = rect.max_x;
        prim.max_y = rect.max_y;

        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;

        prim.color_r = start_color.r * start_color.a;
        prim.color_g = start_color.g * start_color.a;
        prim.color_b = start_color.b * start_color.a;
        prim.color_a = start_color.a;

        const float er = end_color.r * end_color.a;
        const float eg = end_color.g * end_color.a;
        const float eb = end_color.b * end_color.a;
        const float ea = end_color.a;
        prim.atlas_u0 = pack_unorm16(er);
        prim.atlas_v0 = pack_unorm16(eg);
        prim.atlas_u1 = pack_unorm16(eb);
        prim.atlas_v1 = pack_unorm16(ea);

        prim.border_width = border_width;
        prim.shadow_offset_x = start_pt.x;
        prim.shadow_offset_y = start_pt.y;
        prim.shadow_blur = end_pt.x;
        prim.shadow_spread = end_pt.y;
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::HasLinearGradient);
        if (oklab) {
            prim.flags |= static_cast<uint16_t>(core::PrimitiveFlags::OkLabGradient);
        }
        if (border_width > 0.0f) {
            prim.flags |= static_cast<uint16_t>(core::PrimitiveFlags::HasBorder);
        }

        push_primitive(prim);
    }

    void draw_directional_rim(
        math::Rect rect,
        math::Color top_highlight,
        math::Color bottom_shadow,
        float corner_radius = 8.0f,
        float border_width = 1.0f,
        math::Vec2 light_dir = {0.0f, -1.0f},
        float inner_glow = 0.35f) override
    {
        if (rect.is_empty() || (top_highlight.a <= 0.0005f && bottom_shadow.a <= 0.0005f)) {
            return;
        }
        auto pack_unorm16 = [](float v) noexcept -> uint16_t {
            return static_cast<uint16_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 65535.0f));
        };

        float lx = light_dir.x;
        float ly = light_dir.y;
        const float len = std::hypot(lx, ly);
        if (len > 1e-4f) {
            lx /= len;
            ly /= len;
        } else {
            lx = 0.0f;
            ly = -1.0f;
        }

        core::UiPrimitive prim{};
        prim.min_x = rect.min_x;
        prim.min_y = rect.min_y;
        prim.max_x = rect.max_x;
        prim.max_y = rect.max_y;

        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;

        prim.color_r = top_highlight.r * top_highlight.a;
        prim.color_g = top_highlight.g * top_highlight.a;
        prim.color_b = top_highlight.b * top_highlight.a;
        prim.color_a = top_highlight.a;

        const float br = bottom_shadow.r * bottom_shadow.a;
        const float bg = bottom_shadow.g * bottom_shadow.a;
        const float bb = bottom_shadow.b * bottom_shadow.a;
        const float ba = bottom_shadow.a;
        prim.atlas_u0 = pack_unorm16(br);
        prim.atlas_v0 = pack_unorm16(bg);
        prim.atlas_u1 = pack_unorm16(bb);
        prim.atlas_v1 = pack_unorm16(ba);

        prim.border_width = std::max(0.5f, border_width);
        prim.shadow_blur = 0.0f;
        prim.shadow_spread = std::max(0.0f, inner_glow);
        prim.shadow_offset_x = lx;
        prim.shadow_offset_y = ly;
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::HasDirectionalRim | core::PrimitiveFlags::HasBorder);

        push_primitive(prim);
    }

    void draw_acrylic_panel(
        math::Rect rect,
        math::Color tint,
        float corner_radius = 8.0f,
        float grain_amount = 0.028f,
        float grain_scale = 1.0f) override
    {
        if (rect.is_empty() || tint.a <= 0.0005f) return;
        core::UiPrimitive prim{};
        prim.min_x = rect.min_x;
        prim.min_y = rect.min_y;
        prim.max_x = rect.max_x;
        prim.max_y = rect.max_y;
        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;
        prim.color_r = tint.r * tint.a;
        prim.color_g = tint.g * tint.a;
        prim.color_b = tint.b * tint.a;
        prim.color_a = tint.a;
        prim.border_width = 0.0f;
        prim.shadow_blur = std::max(grain_scale, 1.0f);
        prim.shadow_spread = std::clamp(grain_amount, 0.0f, 0.25f);
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::HasAcrylicGrain);
        push_primitive(prim);
    }

    void draw_backdrop_blur(
        math::Rect rect,
        math::Color tint,
        float corner_radius = 8.0f,
        float blur_radius = 36.0f) override
    {
        if (rect.is_empty() || tint.a <= 0.0005f) return;
        core::UiPrimitive prim{};
        prim.min_x = rect.min_x;
        prim.min_y = rect.min_y;
        prim.max_x = rect.max_x;
        prim.max_y = rect.max_y;
        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;
        prim.color_r = tint.r * tint.a;
        prim.color_g = tint.g * tint.a;
        prim.color_b = tint.b * tint.a;
        prim.color_a = tint.a;
        prim.border_width = 0.0f;
        prim.shadow_blur = std::clamp(blur_radius, 4.0f, 96.0f);
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter);
        push_primitive(prim);
    }

    void draw_inset_shadow(
        math::Rect rect,
        math::Color shadow_color,
        float corner_radius = 6.0f,
        float blur_sigma = 6.0f,
        float spread = 0.0f,
        math::Vec2 offset = {0.0f, 2.0f}) override
    {
        if (rect.is_empty() || shadow_color.a <= 0.0005f) return;
        core::UiPrimitive prim{};
        prim.min_x = rect.min_x;
        prim.min_y = rect.min_y;
        prim.max_x = rect.max_x;
        prim.max_y = rect.max_y;
        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;
        prim.color_r = shadow_color.r * shadow_color.a;
        prim.color_g = shadow_color.g * shadow_color.a;
        prim.color_b = shadow_color.b * shadow_color.a;
        prim.color_a = shadow_color.a;
        prim.border_width = 0.0f;
        prim.shadow_blur = std::max(blur_sigma, 0.25f);
        prim.shadow_spread = spread;
        prim.shadow_offset_x = offset.x;
        prim.shadow_offset_y = offset.y;
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::HasInsetShadow);
        push_primitive(prim);
    }

    void draw_metaball(
        math::Rect rect_a,
        float radius_a,
        math::Rect rect_b,
        float radius_b,
        float smooth_k,
        math::Color color_a,
        math::Color color_b = {},
        bool two_tone = false,
        float border_width = 0.0f) override
    {
        const float k = std::max(smooth_k, 0.0f);
        const float pad = k * 0.5f + 2.0f + std::max(0.0f, border_width) * 0.5f;
        const float bx0 = std::min(rect_a.min_x, rect_b.min_x) - pad;
        const float by0 = std::min(rect_a.min_y, rect_b.min_y) - pad;
        const float bx1 = std::max(rect_a.max_x, rect_b.max_x) + pad;
        const float by1 = std::max(rect_a.max_y, rect_b.max_y) + pad;
        const float bw = bx1 - bx0;
        const float bh = by1 - by0;
        if (bw <= 0.001f || bh <= 0.001f) return;
        const float max_a = two_tone ? std::max(color_a.a, color_b.a) : color_a.a;
        if (max_a <= 0.0005f) return;

        auto pack_unorm16 = [](float v) noexcept -> uint16_t {
            return static_cast<uint16_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 65535.0f));
        };
        auto q6 = [](float v) noexcept -> uint32_t {
            return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 63.0f)) & 63u;
        };

        core::UiPrimitive prim{};
        prim.min_x = bx0;
        prim.min_y = by0;
        prim.max_x = bx1;
        prim.max_y = by1;

        prim.radius_top_left = rect_a.min_x;
        prim.radius_top_right = rect_a.min_y;
        prim.radius_bottom_right = rect_a.max_x;
        prim.radius_bottom_left = rect_a.max_y;

        prim.atlas_u0 = pack_unorm16((rect_b.min_x - bx0) / bw);
        prim.atlas_v0 = pack_unorm16((rect_b.min_y - by0) / bh);
        prim.atlas_u1 = pack_unorm16((rect_b.max_x - bx0) / bw);
        prim.atlas_v1 = pack_unorm16((rect_b.max_y - by0) / bh);

        prim.color_r = color_a.r * color_a.a;
        prim.color_g = color_a.g * color_a.a;
        prim.color_b = color_a.b * color_a.a;
        prim.color_a = color_a.a;

        prim.border_width = std::max(0.0f, border_width);
        prim.shadow_blur = k;
        prim.shadow_spread = std::max(0.0f, radius_a);
        prim.shadow_offset_x = std::max(0.0f, radius_b);
        if (two_tone) {
            const uint32_t pk = q6(color_b.r) | (q6(color_b.g) << 6u) | (q6(color_b.b) << 12u) | (q6(color_b.a) << 18u);
            prim.shadow_offset_y = static_cast<float>(pk + 1u);
        } else {
            prim.shadow_offset_y = -1.0f;
        }
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::IsMetaball);
        if (border_width > 0.0f) {
            prim.flags |= static_cast<uint16_t>(core::PrimitiveFlags::HasBorder);
        }
        push_primitive(prim);
    }

    void draw_pointer_reveal(
        math::Rect widget_bounds,
        math::Vec2 cursor_pos,
        float radius,
        math::Color color,
        float corner_radius = 0.0f,
        float border_width = 0.0f,
        float edge_intensity = 1.0f) override
    {
        if (radius <= 0.001f || color.a <= 0.0005f || widget_bounds.is_empty()) return;

        const math::Rect radius_box{
            cursor_pos.x - radius,
            cursor_pos.y - radius,
            cursor_pos.x + radius,
            cursor_pos.y + radius
        };
        const math::Rect clamped = radius_box.intersection(widget_bounds);
        if (clamped.is_empty()) return;

        m_clip_stack.push_clip(widget_bounds, corner_radius, core::kClipFlagRevealBounds);

        core::UiPrimitive prim{};
        prim.min_x = clamped.min_x;
        prim.min_y = clamped.min_y;
        prim.max_x = clamped.max_x;
        prim.max_y = clamped.max_y;

        prim.radius_top_left = corner_radius;
        prim.radius_top_right = corner_radius;
        prim.radius_bottom_right = corner_radius;
        prim.radius_bottom_left = corner_radius;

        prim.color_r = color.r * color.a;
        prim.color_g = color.g * color.a;
        prim.color_b = color.b * color.a;
        prim.color_a = color.a;

        prim.border_width = border_width;
        prim.shadow_offset_x = cursor_pos.x;
        prim.shadow_offset_y = cursor_pos.y;
        prim.shadow_blur = radius;
        prim.shadow_spread = edge_intensity;
        prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::IsPointerReveal);
        if (border_width > 0.0f) {
            prim.flags |= static_cast<uint16_t>(core::PrimitiveFlags::HasBorder);
        }

        push_primitive(prim);
        m_clip_stack.pop_clip();
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
#if !defined(NDEBUG)
            m_scene_dirty_tracker.reset();
#endif
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

#if !defined(NDEBUG)
        // 仅 Debug 模式：对纯业务场景图元独立计算脏瓦片，彻底消除调试浮层自反馈循环
        if (m_has_scene_split && m_scene_prim_count <= m_primitives.size()) {
            std::span<const core::UiPrimitive> scene_prims{m_primitives.data(), m_scene_prim_count};
            auto scene_bin = m_scene_binner.bin_primitives(
                m_grid,
                scene_prims,
                m_config.enable_occlusion_culling,
                clips);
            m_last_scene_dirty_result = m_scene_dirty_tracker.update(
                m_grid,
                scene_bin.headers,
                scene_bin.indices,
                scene_prims,
                clips);
            m_last_scene_stats = m_last_stats;
            m_last_scene_stats.total_primitives = static_cast<uint32_t>(m_scene_prim_count);
            m_last_scene_stats.dirty_tiles = m_last_scene_dirty_result.dirty_count;
            m_last_scene_stats.culling_ratio = m_last_scene_dirty_result.culling_ratio * 100.0;
        } else {
            m_last_scene_stats = m_last_stats;
        }
#endif
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
        bool changed = (m_mica_enabled != enabled);
        if (!changed && enabled) {
            for (size_t i = 0; i < 16; ++i) {
                const uint32_t a = m_mica_grid[i];
                const uint32_t b = grid[i];
                if (a == b) continue;
                auto ch_diff = [](uint32_t x, uint32_t y, unsigned shift) noexcept {
                    int dx = static_cast<int>((x >> shift) & 0xFFu) - static_cast<int>((y >> shift) & 0xFFu);
                    return dx < 0 ? -dx : dx;
                };
                if (ch_diff(a, b, 0) > 2 || ch_diff(a, b, 8) > 2 || ch_diff(a, b, 16) > 2 || ch_diff(a, b, 24) > 2) {
                    changed = true;
                    break;
                }
            }
        }
        if (changed) {
            m_mica_enabled = enabled;
            m_mica_grid = grid;
            m_dirty_tracker.reset();
#if !defined(NDEBUG)
            m_scene_dirty_tracker.reset();
#endif
            m_gpu_full_frame = true;
        }
    }

    bool present_computed_frame(bool force_present) override {
        if (compute_ready()) {
            const auto& dirty = m_last_dirty_result.dirty_tiles;
            if (!m_gpu_full_frame && dirty.empty() && m_gpu_active) return true;
            if (!m_gpu_full_frame && dirty.empty() && !force_present) return m_gpu_active;
            m_compute_tiles.clear();
            auto append = [this](uint32_t x, uint32_t y, uint32_t id) {
                const auto& tile = m_last_binning.headers[id];
                m_compute_tiles.push_back({x, y, tile.primitive_offset, tile.primitive_count});
            };
            bool has_backdrop_filter = false;
            for (const auto& p : m_primitives) {
                if ((p.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter)) != 0) {
                    has_backdrop_filter = true;
                    break;
                }
            }
            if (m_gpu_full_frame || has_backdrop_filter) {
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
            const uint32_t render_flags =
                (m_config.enable_dithering ? compute::kRenderFlagIgnDithering : 0u) |
                (m_config.enable_font_gamma ? compute::kRenderFlagFontGamma : 0u) |
                (m_config.enable_acrylic_grain ? compute::kRenderFlagAcrylicGrain : 0u);
            const compute::Frame frame{m_config.width, m_config.height, m_primitives,
                m_last_binning.indices, m_compute_tiles, m_clip_stack.get_clips(),
                {m_atlas.get_pixels(), size_t(atlas_width) * atlas_height}, atlas_width,
                atlas_height, m_atlas.get_dirty_rects(), m_mica_enabled, m_mica_grid,
                color_atlas_span, color_atlas_width, color_atlas_height, color_atlas_updates,
                render_flags};
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
                        color_atlas_span, color_atlas_width, color_atlas_height, color_atlas_updates,
                        render_flags};
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
        params.enable_dithering = m_config.enable_dithering;
        params.enable_font_gamma = m_config.enable_font_gamma;
        params.enable_acrylic_grain = m_config.enable_acrylic_grain;

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

    const std::vector<tile::DirtyTileInfo>& get_last_dirty_tiles() const override {
        return m_last_dirty_result.dirty_tiles;
    }

    const FrameStats& get_last_frame_stats() const override {
        return m_last_stats;
    }

#if !defined(NDEBUG)
    const std::vector<tile::DirtyTileInfo>& get_last_scene_dirty_tiles() const override {
        return m_has_scene_split ? m_last_scene_dirty_result.dirty_tiles : m_last_dirty_result.dirty_tiles;
    }

    const FrameStats& get_last_scene_stats() const override {
        return m_has_scene_split ? m_last_scene_stats : m_last_stats;
    }
#endif

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
    static void snap_primitive_in_place(core::UiPrimitive& p) noexcept {
        constexpr uint16_t kSkipMask =
            static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph) |
            static_cast<uint16_t>(core::PrimitiveFlags::IsIcon) |
            static_cast<uint16_t>(core::PrimitiveFlags::IsPointerReveal) |
            static_cast<uint16_t>(core::PrimitiveFlags::IsMetaball);
        if ((p.flags & kSkipMask) != 0) return;

        const bool is_gradient = (p.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasLinearGradient)) != 0;
        const bool is_rim = (p.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasDirectionalRim)) != 0;
        const bool is_acrylic = (p.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasAcrylicGrain)) != 0;
        const bool is_backdrop = (p.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter)) != 0;
        const bool is_inset = (p.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasInsetShadow)) != 0;
        if (!is_gradient && !is_rim && !is_acrylic && !is_backdrop && !is_inset && p.shadow_blur > 0.0f) return;

        const float w_orig = p.max_x - p.min_x;
        const float h_orig = p.max_y - p.min_y;
        if (w_orig <= 0.0f || h_orig <= 0.0f) return;

        if (p.border_width <= 0.0f &&
            p.radius_top_left >= w_orig * 0.499f &&
            std::abs(w_orig - h_orig) < 0.01f) {
            return;
        }

        if (p.border_width > 0.0f) {
            p.border_width = std::max(1.0f, std::round(p.border_width));
        }

        auto snap_axis = [&](float& a, float& b) {
            const float ext = b - a;
            if (p.border_width <= 0.0f && ext < 2.0f) {
                const float w = std::max(1.0f, std::round(ext));
                const float mid = (a + b) * 0.5f;
                const int w_int = static_cast<int>(w);
                const float center = (w_int & 1) ? (std::floor(mid) + 0.5f) : std::round(mid);
                a = center - w * 0.5f;
                b = center + w * 0.5f;
            } else {
                a = std::round(a);
                b = std::round(b);
                const float min_ext = (p.border_width > 0.0f) ? std::max(1.0f, p.border_width) : 1.0f;
                if (b - a < min_ext) {
                    b = a + min_ext;
                }
            }
        };

        snap_axis(p.min_x, p.max_x);
        snap_axis(p.min_y, p.max_y);
    }

    RendererConfig m_config{};
    tile::TileGrid m_grid;
    tile::TileBinner m_binner{};
    tile::DirtyTracker m_dirty_tracker;
#if !defined(NDEBUG)
    tile::TileBinner m_scene_binner{};
    tile::DirtyTracker m_scene_dirty_tracker;
#endif
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
#if !defined(NDEBUG)
    tile::DirtyTrackingResult m_last_scene_dirty_result{};
    size_t m_scene_prim_count{SIZE_MAX};
    bool m_has_scene_split{false};
#endif

    uint32_t m_frame_index{0};

    FrameStats m_last_stats{};
#if !defined(NDEBUG)
    FrameStats m_last_scene_stats{};
#endif
};

std::unique_ptr<KerneTileRenderer> KerneTileRenderer::create(const RendererConfig& config) {
    return std::make_unique<KerneTileRendererImpl>(config);
}

} // namespace kernetile
