#include "vector_text_engine.hpp"
#include "builtin_text_engine.hpp"
#include "kernetile/text/stem_darkening.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <cstdio>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace kernetile::text {

static inline uint32_t decode_utf8(const char*& ptr, const char* end) noexcept {
    if (ptr >= end) return 0;
    auto lead = static_cast<uint8_t>(*ptr++);
    if (lead < 0x80) return lead;

    if ((lead & 0xE0) == 0xC0 && ptr < end) {
        auto b1 = static_cast<uint8_t>(*ptr++);
        return ((lead & 0x1F) << 6) | (b1 & 0x3F);
    }
    if ((lead & 0xF0) == 0xE0 && ptr + 1 < end) {
        auto b1 = static_cast<uint8_t>(*ptr++);
        auto b2 = static_cast<uint8_t>(*ptr++);
        return ((lead & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
    }
    if ((lead & 0xF8) == 0xF0 && ptr + 2 < end) {
        auto b1 = static_cast<uint8_t>(*ptr++);
        auto b2 = static_cast<uint8_t>(*ptr++);
        auto b3 = static_cast<uint8_t>(*ptr++);
        return ((lead & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
    }
    return 0xFFFD;
}

// Linux deepin / Debian / Windows 共享默认中文字体查找路径
static const char* const kSystemDefaultFontPaths[] = {
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    "/mnt/c/Windows/Fonts/msyh.ttc",
    "/mnt/c/Windows/Fonts/simhei.ttf",
    "C:\\Windows\\Fonts\\msyh.ttc",
    "C:\\Windows\\Fonts\\simhei.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
};

struct VectorTextEngine::Impl {
    std::vector<uint8_t> font_buffer;
    stbtt_fontinfo font_info{};
    bool valid{false};
    BuiltinTextEngine fallback_engine;
};

VectorTextEngine::VectorTextEngine(const std::string& font_path)
    : m_impl(std::make_unique<Impl>())
{
    std::string candidate_path = font_path;
    if (candidate_path.empty()) {
        for (const char* path : kSystemDefaultFontPaths) {
            std::ifstream test(path, std::ios::binary);
            if (test.good()) {
                candidate_path = path;
                break;
            }
        }
    }

    if (!candidate_path.empty()) {
        std::ifstream file(candidate_path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            auto size = file.tellg();
            if (size > 0) {
                m_impl->font_buffer.resize(static_cast<size_t>(size));
                file.seekg(0, std::ios::beg);
                file.read(reinterpret_cast<char*>(m_impl->font_buffer.data()), size);

                int font_offset = stbtt_GetFontOffsetForIndex(m_impl->font_buffer.data(), 0);
                if (font_offset >= 0 && stbtt_InitFont(&m_impl->font_info, m_impl->font_buffer.data(), font_offset)) {
                    m_font_loaded = true;
                    m_impl->valid = true;
                    m_loaded_path = candidate_path;
                }
            }
        }
    }
}

VectorTextEngine::~VectorTextEngine() = default;

FontMetrics VectorTextEngine::get_metrics(float font_size, std::string_view font_family) {
    if (!m_impl->valid) {
        return m_impl->fallback_engine.get_metrics(font_size, font_family);
    }

    float scale = stbtt_ScaleForPixelHeight(&m_impl->font_info, font_size);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&m_impl->font_info, &ascent, &descent, &line_gap);

    return FontMetrics{
        .font_size = font_size,
        .ascender = static_cast<float>(ascent) * scale,
        .descender = static_cast<float>(descent) * scale,
        .line_height = static_cast<float>(ascent - descent + line_gap) * scale
    };
}

TextLayoutResult VectorTextEngine::shape_text(std::string_view utf8_text, float font_size, std::string_view font_family) {
    if (!m_impl->valid) {
        return m_impl->fallback_engine.shape_text(utf8_text, font_size, font_family);
    }

    TextLayoutResult result;
    result.metrics = get_metrics(font_size, font_family);

    float scale = stbtt_ScaleForPixelHeight(&m_impl->font_info, font_size);
    float current_x = 0.0f;
    const char* ptr = utf8_text.data();
    const char* end = ptr + utf8_text.size();

    while (ptr < end) {
        uint32_t codepoint = decode_utf8(ptr, end);
        if (codepoint == 0) break;

        int glyph_idx = stbtt_FindGlyphIndex(&m_impl->font_info, static_cast<int>(codepoint));
        int adv_int = 0, lsb_int = 0;
        if (glyph_idx > 0) {
            stbtt_GetGlyphHMetrics(&m_impl->font_info, glyph_idx, &adv_int, &lsb_int);
        } else {
            adv_int = (codepoint >= 0x4E00 && codepoint <= 0x9FA5) ? static_cast<int>(font_size / scale) : static_cast<int>(font_size * 0.5f / scale);
        }

        float adv_x = static_cast<float>(adv_int) * scale;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        if (glyph_idx > 0) {
            stbtt_GetGlyphBitmapBox(&m_impl->font_info, glyph_idx, scale, scale, &x0, &y0, &x1, &y1);
        }

        float gw = static_cast<float>(std::max(1, x1 - x0));
        float gh = static_cast<float>(std::max(1, y1 - y0));

        GlyphPlacement gp{
            .glyph_id = codepoint,
            .x = current_x,
            .y = result.metrics.ascender,
            .advance_x = adv_x,
            .advance_y = 0.0f,
            .width = gw,
            .height = gh,
            .bearing_x = static_cast<float>(x0),
            .bearing_y = static_cast<float>(-y0)
        };

        result.glyphs.push_back(gp);
        current_x += adv_x;
    }

    result.total_width = current_x;
    result.total_height = result.metrics.line_height;
    return result;
}

FontRasterResult VectorTextEngine::rasterize_glyph(uint32_t glyph_id, float font_size, bool enable_stem_darkening) {
    if (!m_impl->valid) {
        return m_impl->fallback_engine.rasterize_glyph(glyph_id, font_size, enable_stem_darkening);
    }

    int glyph_idx = stbtt_FindGlyphIndex(&m_impl->font_info, static_cast<int>(glyph_id));
    if (glyph_idx <= 0) {
        return m_impl->fallback_engine.rasterize_glyph(glyph_id, font_size, enable_stem_darkening);
    }

    float scale = stbtt_ScaleForPixelHeight(&m_impl->font_info, font_size);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(&m_impl->font_info, glyph_idx, scale, scale, &x0, &y0, &x1, &y1);

    int w = x1 - x0;
    int h = y1 - y0;

    if (w <= 0 || h <= 0) {
        return FontRasterResult{
            .width = 1,
            .height = 1,
            .bearing_x = 0,
            .bearing_y = 0,
            .r8_bitmap = std::vector<uint8_t>(1, 0)
        };
    }

    FontRasterResult result{
        .width = static_cast<uint32_t>(w),
        .height = static_cast<uint32_t>(h),
        .bearing_x = x0,
        .bearing_y = -y0,
        .r8_bitmap = std::vector<uint8_t>(static_cast<size_t>(w) * h, 0)
    };

    stbtt_MakeGlyphBitmap(&m_impl->font_info, result.r8_bitmap.data(), w, h, w, scale, scale, glyph_idx);

    if (enable_stem_darkening) {
        StemDarkener::apply(result.r8_bitmap, font_size);
    }

    return result;
}

std::unique_ptr<ITextEngine> create_vector_text_engine(const std::string& font_path) {
    return std::make_unique<VectorTextEngine>(font_path);
}

} // namespace kernetile::text
