#pragma once

#include "kernetile/text/font_engine.hpp"
#include "kernetile/atlas/dynamic_atlas.hpp"
#include "kernetile/core/primitives.hpp"
#include "kernetile/math/color.hpp"
#include <unordered_map>
#include <string_view>
#include <vector>

namespace kernetile::text {

struct CachedGlyph {
    atlas::AtlasRegion region;
    uint32_t width{0};
    uint32_t height{0};
    int32_t bearing_x{0};
    int32_t bearing_y{0};
};

struct TextStreamStats {
    uint32_t total_glyphs{0};
    uint32_t cache_hits{0};
    uint32_t cache_misses{0};

    [[nodiscard]] double get_hit_rate() const noexcept {
        return total_glyphs == 0 ? 1.0 : static_cast<double>(cache_hits) / static_cast<double>(total_glyphs);
    }
};

class TextStreamer {
public:
    TextStreamer(ITextEngine& engine, atlas::DynamicAtlas& atlas)
        : m_engine(engine), m_atlas(atlas) {}

    ~TextStreamer() = default;

    TextStreamStats stream_text(
        std::string_view utf8_text,
        float x,
        float y,
        float font_size,
        const math::Color& color,
        std::vector<core::UiPrimitive>& out_primitives,
        uint16_t clip_index = core::INVALID_INDEX_U16,
        float render_scale = 1.0f,
        uint16_t font_weight = 400,
        bool italic = false)
    {
        TextStreamStats stats;
        const float raster_size = font_size * render_scale;
        const auto layout = &m_engine == &shared_text_engine()
            ? layout_text(utf8_text, font_size, font_weight, italic)
            : std::make_shared<const TextLayoutResult>(m_engine.shape_text(utf8_text, font_size, "", font_weight, italic));

        for (const auto& gp : layout->glyphs) {
            stats.total_glyphs++;

            // 跳过无形字符 (如空格、不可见控制符)
            if (gp.glyph_id == ' ' || gp.glyph_id == '\t' || gp.glyph_id == '\n' || gp.glyph_id == '\r') {
                stats.cache_hits++;
                continue;
            }

            // 64 位复合缓存键: (高 32 位为缩放字号，低 32 位为字形 ID)
            uint64_t key = (static_cast<uint64_t>(raster_size * 100.0f) << 32) | gp.glyph_id;

            auto it = m_glyph_cache.find(key);
            if (it == m_glyph_cache.end()) {
                // 缓存未命中：光栅化字形并灌入图集
                stats.cache_misses++;
                auto raster = m_engine.rasterize_glyph(gp.glyph_id, raster_size, true);

                if (raster.width > 0 && raster.height > 0 && !raster.r8_bitmap.empty()) {
                    auto region_opt = m_atlas.allocate_and_upload(
                        raster.width, 
                        raster.height, 
                        raster.r8_bitmap.data()
                    );

                    if (region_opt.has_value()) {
                        CachedGlyph cg{
                            .region = *region_opt,
                            .width = raster.width,
                            .height = raster.height,
                            .bearing_x = raster.bearing_x,
                            .bearing_y = raster.bearing_y
                        };
                        it = m_glyph_cache.emplace(key, cg).first;
                    }
                } else {
                    it = m_glyph_cache.emplace(key, CachedGlyph{}).first;
                }
            } else {
                stats.cache_hits++;
            }

            // 若成功获取/缓存字形，发射 80 字节 UiPrimitive
            if (it != m_glyph_cache.end() && it->second.width > 0) {
                const auto& cg = it->second;

                float min_x = std::floor((x + gp.x) * render_scale + static_cast<float>(cg.bearing_x) + 0.5f);
                float min_y = std::floor((y + gp.y) * render_scale - static_cast<float>(cg.bearing_y) + 0.5f);
                float max_x = min_x + static_cast<float>(cg.width);
                float max_y = min_y + static_cast<float>(cg.height);

                core::UiPrimitive prim{};
                prim.min_x = min_x;
                prim.min_y = min_y;
                prim.max_x = max_x;
                prim.max_y = max_y;

                // 注入预乘颜色
                prim.color_r = color.r * color.a;
                prim.color_g = color.g * color.a;
                prim.color_b = color.b * color.a;
                prim.color_a = color.a;

                // 绑定图集定点化 UV
                prim.atlas_u0 = cg.region.u0;
                prim.atlas_v0 = cg.region.v0;
                prim.atlas_u1 = cg.region.u1;
                prim.atlas_v1 = cg.region.v1;

                prim.flags = static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph | core::PrimitiveFlags::StemDarkening);
                prim.clip_index = clip_index;

                out_primitives.push_back(prim);
            }
        }

        return stats;
    }

    void clear_cache() noexcept {
        m_glyph_cache.clear();
    }

    [[nodiscard]] size_t get_cached_glyph_count() const noexcept {
        return m_glyph_cache.size();
    }

private:
    ITextEngine& m_engine;
    atlas::DynamicAtlas& m_atlas;
    std::unordered_map<uint64_t, CachedGlyph> m_glyph_cache;
};

} // namespace kernetile::text
