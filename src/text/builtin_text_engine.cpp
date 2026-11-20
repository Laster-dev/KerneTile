#include "builtin_text_engine.hpp"
#include "builtin_font_data.hpp"
#include "kernetile/text/stem_darkening.hpp"
#include <cmath>
#include <algorithm>

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
    return 0xFFFD; // 替换字符
}

FontMetrics BuiltinTextEngine::get_metrics(float font_size, std::string_view) {
    return FontMetrics{
        .font_size = font_size,
        .ascender = font_size * 0.8f,
        .descender = -font_size * 0.2f,
        .line_height = font_size * 1.25f
    };
}

TextLayoutResult BuiltinTextEngine::shape_text(
    std::string_view utf8_text, 
    float font_size, 
    std::string_view font_family)
{
    TextLayoutResult result;
    result.metrics = get_metrics(font_size, font_family);

    float current_x = 0.0f;
    const char* ptr = utf8_text.data();
    const char* end = ptr + utf8_text.size();

    while (ptr < end) {
        uint32_t codepoint = decode_utf8(ptr, end);
        if (codepoint == 0) break;

        float adv = font_size * 0.5f;
        if (codepoint == ' ') {
            adv = font_size * 0.3f;
        } else if (codepoint >= 0x2E80) {
            // 全角字符
            adv = font_size * 1.0f;
        } else if (codepoint >= 'A' && codepoint <= 'Z') {
            adv = font_size * 0.65f;
        } else if (codepoint >= '0' && codepoint <= '9') {
            adv = font_size * 0.55f;
        } else if (codepoint == 'i' || codepoint == 'l' || codepoint == '.' || codepoint == ',') {
            adv = font_size * 0.25f;
        } else if (codepoint == 'm' || codepoint == 'w' || codepoint == 'W') {
            adv = font_size * 0.75f;
        }

        GlyphPlacement gp{
            .glyph_id = codepoint,
            .x = current_x,
            .y = result.metrics.ascender,
            .advance_x = adv,
            .advance_y = 0.0f,
            .width = std::max(4.0f, adv),
            .height = font_size,
            .bearing_x = 0.0f,
            .bearing_y = result.metrics.ascender
        };

        result.glyphs.push_back(gp);
        current_x += adv;
    }

    result.total_width = current_x;
    result.total_height = result.metrics.line_height;
    return result;
}

FontRasterResult BuiltinTextEngine::rasterize_glyph(
    uint32_t glyph_id, 
    float font_size,
    bool enable_stem_darkening)
{
    uint32_t w = std::max(4u, static_cast<uint32_t>(font_size * 0.6f + 0.5f));
    uint32_t h = std::max(4u, static_cast<uint32_t>(font_size + 0.5f));

    if (glyph_id >= 0x2E80) {
        w = std::max(4u, static_cast<uint32_t>(font_size + 0.5f));
    } else if (glyph_id == 'i' || glyph_id == 'l' || glyph_id == '.' || glyph_id == ':') {
        w = std::max(2u, static_cast<uint32_t>(font_size * 0.3f + 0.5f));
    }

    FontRasterResult result{
        .width = w,
        .height = h,
        .bearing_x = 0,
        .bearing_y = static_cast<int32_t>(font_size * 0.8f),
        .r8_bitmap = std::vector<uint8_t>(static_cast<size_t>(w) * h, 0)
    };

    // 空格无墨水点阵
    if (glyph_id == ' ') {
        return result;
    }

    if (glyph_id >= 32 && glyph_id <= 126) {
        // ASCII 8x8 标准字模双线性插值渲染
        const uint8_t* src_rows = g_ascii_8x8[glyph_id - 32];
        for (uint32_t y = 0; y < h; ++y) {
            float sy = (static_cast<float>(y) / static_cast<float>(h)) * 8.0f;
            int iy = std::clamp(static_cast<int>(sy), 0, 7);
            uint8_t row_bits = src_rows[iy];
            for (uint32_t x = 0; x < w; ++x) {
                float sx = (static_cast<float>(x) / static_cast<float>(w)) * 8.0f;
                int ix = std::clamp(static_cast<int>(sx), 0, 7);
                bool on = (row_bits & (0x80 >> ix)) != 0;
                result.r8_bitmap[y * w + x] = on ? 255 : 0;
            }
        }
    } else {
        // 缺字/不支持字符：标准缺字方框 (Tofu 描边矩形)
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                bool border = (x == 0 || x == w - 1 || y == 0 || y == h - 1);
                result.r8_bitmap[y * w + x] = border ? 200 : 0;
            }
        }
    }

    // 线性光空间 Stem Darkening 笔划加粗补偿
    if (enable_stem_darkening) {
        StemDarkener::apply(result.r8_bitmap, font_size);
    }

    return result;
}

std::unique_ptr<ITextEngine> create_builtin_engine() {
    return std::make_unique<BuiltinTextEngine>();
}

} // namespace kernetile::text
