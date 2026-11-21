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
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
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

// 自动搜寻系统可用中文字体作为跨平台矢量字库 Fallback 链
static std::vector<std::string> query_cjk_fallback_fonts() {
    std::vector<std::string> results;
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    const char* cmds[] = {
        "fc-list :lang=zh file 2>/dev/null",
        "fc-match -f '%{file}\n' 'sans-serif:lang=zh' 2>/dev/null",
        "fc-match -f '%{file}\n' 'WenQuanYi Micro Hei' 2>/dev/null",
        "fc-match -f '%{file}\n' 'Noto Sans CJK SC' 2>/dev/null"
    };
    for (const char* cmd : cmds) {
        FILE* fp = popen(cmd, "r");
        if (fp) {
            char buf[1024];
            while (fgets(buf, sizeof(buf), fp)) {
                std::string path(buf);
                while (!path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' ' || path.back() == ':')) {
                    path.pop_back();
                }
                if (!path.empty() && std::filesystem::exists(path)) {
                    if (std::find(results.begin(), results.end(), path) == results.end()) {
                        results.push_back(path);
                    }
                }
            }
            pclose(fp);
        }
    }

    const char* standard_cjk_paths[] = {
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
        "/usr/share/fonts/cjkuni-uming/uming.ttc",
        "/mnt/c/Windows/Fonts/msyh.ttc",
        "/mnt/c/Windows/Fonts/simsun.ttc",
        "/mnt/c/Windows/Fonts/msyhl.ttc"
    };
    for (const char* p : standard_cjk_paths) {
        if (std::filesystem::exists(p)) {
            if (std::find(results.begin(), results.end(), p) == results.end()) {
                results.push_back(p);
            }
        }
    }
#else
    const char* win_cjk_paths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simsun.ttc"
    };
    for (const char* p : win_cjk_paths) {
        if (std::filesystem::exists(p)) {
            results.push_back(p);
        }
    }
#endif
    return results;
}

struct FontFace {
    std::string path;
    std::vector<uint8_t> buffer;
    stbtt_fontinfo info{};
    bool is_valid{false};
};

static bool load_font_face(const std::string& path, FontFace& out_face) {
    if (!std::filesystem::exists(path)) return false;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    auto size = file.tellg();
    if (size <= 0) return false;

    out_face.buffer.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(out_face.buffer.data()), size);

    int font_offset = stbtt_GetFontOffsetForIndex(out_face.buffer.data(), 0);
    if (font_offset >= 0 && stbtt_InitFont(&out_face.info, out_face.buffer.data(), font_offset)) {
        out_face.path = path;
        out_face.is_valid = true;
        return true;
    }
    return false;
}

struct VectorTextEngine::Impl {
    FontFace primary_font;
    std::vector<FontFace> fallback_fonts;
    bool valid{false};
    BuiltinTextEngine fallback_engine;

    // 跨主字库与 Fallback 链高效检索字形
    std::pair<const stbtt_fontinfo*, int> find_glyph(uint32_t codepoint) const {
        if (primary_font.is_valid) {
            int g = stbtt_FindGlyphIndex(&primary_font.info, static_cast<int>(codepoint));
            if (g > 0) return {&primary_font.info, g};
        }
        for (const auto& fb : fallback_fonts) {
            if (fb.is_valid) {
                int g = stbtt_FindGlyphIndex(&fb.info, static_cast<int>(codepoint));
                if (g > 0) return {&fb.info, g};
            }
        }
        return {nullptr, 0};
    }
};

VectorTextEngine::VectorTextEngine(const std::string& font_path)
    : m_impl(std::make_unique<Impl>())
{
    std::string candidate_path = font_path;
    if (candidate_path.empty() || !std::filesystem::exists(candidate_path)) {
        candidate_path = query_system_default_font();
    }

    if (!candidate_path.empty() && load_font_face(candidate_path, m_impl->primary_font)) {
        m_font_loaded = true;
        m_impl->valid = true;
        m_loaded_path = candidate_path;
        std::cout << "[VectorTextEngine] Successfully loaded primary font: " << candidate_path << std::endl;
    }

    // 自动挂载 CJK 字体后备链
    auto cjk_paths = query_cjk_fallback_fonts();
    for (const auto& p : cjk_paths) {
        if (p == m_loaded_path) continue;
        FontFace fb;
        if (load_font_face(p, fb)) {
            std::cout << "[VectorTextEngine] Successfully registered CJK fallback font: " << p << std::endl;
            m_impl->fallback_fonts.push_back(std::move(fb));
            m_font_loaded = true;
            m_impl->valid = true;
            break; // 载入首个高质量 CJK 字库即可
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

    const stbtt_fontinfo* base_info = m_impl->primary_font.is_valid 
        ? &m_impl->primary_font.info 
        : (!m_impl->fallback_fonts.empty() ? &m_impl->fallback_fonts[0].info : nullptr);

    if (!base_info) {
        return m_impl->fallback_engine.get_metrics(font_size, font_family);
    }

    float scale = stbtt_ScaleForPixelHeight(base_info, font_size);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(base_info, &ascent, &descent, &line_gap);

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

    float current_x = 0.0f;
    const char* ptr = utf8_text.data();
    const char* end = ptr + utf8_text.size();

    while (ptr < end) {
        uint32_t codepoint = decode_utf8(ptr, end);
        if (codepoint == 0) break;

        auto [font, glyph_idx] = m_impl->find_glyph(codepoint);
        int adv_int = 0, lsb_int = 0;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        float adv_x = 0.0f;

        if (font && glyph_idx > 0) {
            float scale = stbtt_ScaleForPixelHeight(font, font_size);
            stbtt_GetGlyphHMetrics(font, glyph_idx, &adv_int, &lsb_int);
            stbtt_GetGlyphBitmapBox(font, glyph_idx, scale, scale, &x0, &y0, &x1, &y1);
            adv_x = static_cast<float>(adv_int) * scale;
        } else {
            // 字库均不支持该字符：全角中文字符采用标准字号宽度，半角为半宽
            bool is_wide = (codepoint >= 0x2E80);
            adv_x = is_wide ? font_size : font_size * 0.5f;
            x0 = 0;
            y0 = -static_cast<int>(result.metrics.ascender);
            x1 = static_cast<int>(adv_x);
            y1 = -static_cast<int>(result.metrics.descender);
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

    auto [font, glyph_idx] = m_impl->find_glyph(glyph_id);
    if (font && glyph_idx > 0) {
        float scale = stbtt_ScaleForPixelHeight(font, font_size);
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBox(font, glyph_idx, scale, scale, &x0, &y0, &x1, &y1);

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

        stbtt_MakeGlyphBitmap(font, result.r8_bitmap.data(), w, h, w, scale, scale, glyph_idx);

        if (enable_stem_darkening) {
            StemDarkener::apply(result.r8_bitmap, font_size);
        }

        return result;
    }

    // 尝试后备内置点阵引擎 (支持基础 ASCII 及内置常用 CJK)
    auto fb_res = m_impl->fallback_engine.rasterize_glyph(glyph_id, font_size, enable_stem_darkening);
    bool has_ink = false;
    for (uint8_t b : fb_res.r8_bitmap) {
        if (b > 0) { has_ink = true; break; }
    }
    if (has_ink) {
        return fb_res;
    }

    // 终极缺字后备：标准缺字方框 (Tofu 方框)
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

std::unique_ptr<ITextEngine> create_vector_text_engine(const std::string& font_path) {
    return std::make_unique<VectorTextEngine>(font_path);
}

} // namespace kernetile::text
