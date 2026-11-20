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
#include <filesystem>

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

// 获取系统默认字体 (默认 sans-serif，无任何额外偏向或路径猜测)
static std::string query_system_default_font() {
#if !defined(_WIN32)
    // 坚决使用系统默认字体 (sans-serif)
    const char* cmd = "fc-match -f '%{file}' sans-serif 2>/dev/null";
    FILE* fp = popen(cmd, "r");
    if (!fp) {
        fp = popen("fc-match -f '%{file}' 2>/dev/null", "r");
    }
    if (fp) {
        char buf[1024];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string path(buf);
            while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) {
                path.pop_back();
            }
            pclose(fp);
            if (!path.empty() && std::filesystem::exists(path)) {
                return path;
            }
        } else {
            pclose(fp);
        }
    }
#else
    const char* win_font = "C:\\Windows\\Fonts\\msyh.ttc";
    if (std::filesystem::exists(win_font)) return win_font;
#endif
    return "";
}

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
    if (candidate_path.empty() || !std::filesystem::exists(candidate_path)) {
        candidate_path = query_system_default_font();
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
                    std::cout << "[VectorTextEngine] Successfully loaded system default font: " << candidate_path << std::endl;
                }
            }
        }
    }

    if (!m_font_loaded) {
        std::cout << "[VectorTextEngine] Warning: Could not open vector font, fallback to BuiltinTextEngine." << std::endl;
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
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;

        if (glyph_idx > 0) {
            stbtt_GetGlyphHMetrics(&m_impl->font_info, glyph_idx, &adv_int, &lsb_int);
            stbtt_GetGlyphBitmapBox(&m_impl->font_info, glyph_idx, scale, scale, &x0, &y0, &x1, &y1);
        } else {
            // 字体不支持该字符：全角字符用标准字号宽度，半角用半宽
            bool is_wide = (codepoint >= 0x2E80);
            adv_int = static_cast<int>((is_wide ? font_size : font_size * 0.5f) / scale);
            x0 = 0;
            y0 = -static_cast<int>(result.metrics.ascender);
            x1 = static_cast<int>(adv_int * scale);
            y1 = -static_cast<int>(result.metrics.descender);
        }

        float adv_x = static_cast<float>(adv_int) * scale;
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
        // 字体不支持该字符：坚决按不支持的来，渲染优雅标准缺字矩形框 (Tofu 方框)
        bool is_wide = (glyph_id >= 0x2E80);
        uint32_t w = std::max(4u, static_cast<uint32_t>(is_wide ? font_size : font_size * 0.5f));
        uint32_t h = std::max(4u, static_cast<uint32_t>(font_size));
        FontRasterResult tofu{
            .width = w,
            .height = h,
            .bearing_x = 0,
            .bearing_y = static_cast<int32_t>(font_size * 0.8f),
            .r8_bitmap = std::vector<uint8_t>(static_cast<size_t>(w) * h, 0)
        };
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                    tofu.r8_bitmap[y * w + x] = 200;
                }
            }
        }
        return tofu;
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
