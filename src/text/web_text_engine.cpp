#include "web_text_engine.hpp"
#include "builtin_text_engine.hpp"
#include "kernetile/text/stem_darkening.hpp"
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

// JavaScript Canvas 2D 文本度量与字形光栅化桥接接口
EM_JS(float, ktui_js_measure_char, (uint32_t codepoint, float font_size), {
    if (typeof document === "undefined") {
        if (codepoint === 32) return font_size * 0.3;
        if (codepoint >= 0x2E80) return font_size * 1.0;
        return font_size * 0.55;
    }
    if (!window._ktui_measure_ctx) {
        var c = document.createElement("canvas");
        window._ktui_measure_ctx = c.getContext("2d", { willReadFrequently: true });
    }
    var ctx = window._ktui_measure_ctx;
    var sizeInt = Math.max(8, Math.round(font_size));
    var fontStr = sizeInt + "px -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, \"PingFang SC\", \"Microsoft YaHei\", \"Noto Sans SC\", sans-serif";
    ctx.font = fontStr;
    var ch = String.fromCodePoint(codepoint);
    return ctx.measureText(ch).width;
});

EM_JS(int, ktui_js_rasterize_char, (uint32_t codepoint, float font_size, int* out_w, int* out_h, int* out_bx, int* out_by, uint8_t* dst_buf, int max_buf_len), {
    if (typeof document === "undefined") {
        return 0;
    }
    if (!window._ktui_raster_canvas) {
        window._ktui_raster_canvas = document.createElement("canvas");
        window._ktui_raster_ctx = window._ktui_raster_canvas.getContext("2d", { willReadFrequently: true });
    }
    var canvas = window._ktui_raster_canvas;
    var ctx = window._ktui_raster_ctx;
    var sizeInt = Math.max(8, Math.round(font_size));
    var fontStr = sizeInt + "px -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, \"PingFang SC\", \"Microsoft YaHei\", \"Noto Sans SC\", sans-serif";
    ctx.font = fontStr;

    var ch = String.fromCodePoint(codepoint);
    var metrics = ctx.measureText(ch);

    var actualW = Math.ceil(metrics.width) || Math.ceil(sizeInt * 0.5);
    var actualH = Math.ceil(sizeInt * 1.40);
    if (actualW < 1) actualW = 1;
    if (actualH < 1) actualH = 1;

    // 留出 1px 安全内边距防止边缘抗锯齿切边
    var pad = 1;
    var canvasW = actualW + pad * 2;
    var canvasH = actualH + pad * 2;

    if (canvasW * canvasH > max_buf_len) {
        return 0;
    }

    if (canvas.width !== canvasW || canvas.height !== canvasH) {
        canvas.width = canvasW;
        canvas.height = canvasH;
    }

    ctx.clearRect(0, 0, canvasW, canvasH);
    ctx.font = fontStr;
    ctx.fillStyle = "#ffffff";
    ctx.textBaseline = "alphabetic";

    var baselineX = pad;
    var baselineY = Math.round(sizeInt * 1.08) + pad;
    ctx.fillText(ch, baselineX, baselineY);

    var imgData = ctx.getImageData(0, 0, canvasW, canvasH);
    var pixels = imgData.data;
    var totalPixels = canvasW * canvasH;

    for (var i = 0; i < totalPixels; i++) {
        HEAPU8[dst_buf + i] = pixels[i * 4 + 3]; // 提取 Alpha 覆盖率通道作为 R8 灰度通道
    }

    HEAP32[out_w >> 2] = canvasW;
    HEAP32[out_h >> 2] = canvasH;
    HEAP32[out_bx >> 2] = 0;
    HEAP32[out_by >> 2] = baselineY;

    return 1;
});
#endif

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

struct WebTextEngine::Impl {
    std::unordered_map<uint64_t, float> advance_cache;
    BuiltinTextEngine fallback_engine;

    float get_char_advance(uint32_t codepoint, float font_size) {
        uint32_t fsz_int = static_cast<uint32_t>(font_size * 10.0f + 0.5f);
        uint64_t key = (static_cast<uint64_t>(fsz_int) << 32) | codepoint;
        auto it = advance_cache.find(key);
        if (it != advance_cache.end()) {
            return it->second;
        }

        float adv = 0.0f;
#if defined(__EMSCRIPTEN__)
        adv = ktui_js_measure_char(codepoint, font_size);
#endif
        if (adv <= 0.0f) {
            if (codepoint == ' ') {
                adv = font_size * 0.30f;
            } else if (codepoint >= 0x2E80) {
                adv = font_size * 1.0f;
            } else if (codepoint >= 'A' && codepoint <= 'Z') {
                adv = font_size * 0.65f;
            } else if (codepoint >= '0' && codepoint <= '9') {
                adv = font_size * 0.55f;
            } else {
                adv = font_size * 0.50f;
            }
        }

        advance_cache[key] = adv;
        return adv;
    }
};

WebTextEngine::WebTextEngine()
    : m_impl(std::make_unique<Impl>())
{
    std::cout << "[WebTextEngine] Initialized HTML5 Canvas 2D Native Font Engine." << std::endl;
}

WebTextEngine::~WebTextEngine() = default;

FontMetrics WebTextEngine::get_metrics(float font_size, std::string_view font_family) {
    (void)font_family;
    return FontMetrics{
        .font_size = font_size,
        .ascender = std::round(font_size * 1.08f),
        .descender = -std::round(font_size * 0.28f),
        .line_height = std::round(font_size * 1.36f)
    };
}

TextLayoutResult WebTextEngine::shape_text(
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

        float adv = m_impl->get_char_advance(codepoint, font_size);
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

FontRasterResult WebTextEngine::rasterize_glyph(
    uint32_t glyph_id,
    float font_size,
    bool enable_stem_darkening)
{
    if (glyph_id == ' ' || glyph_id == '\t' || glyph_id == '\r' || glyph_id == '\n') {
        float adv = m_impl->get_char_advance(' ', font_size);
        auto fm = get_metrics(font_size);
        return FontRasterResult{
            .width = static_cast<uint32_t>(adv + 0.5f),
            .height = static_cast<uint32_t>(font_size + 0.5f),
            .bearing_x = 0,
            .bearing_y = static_cast<int32_t>(fm.ascender + 0.5f),
            .r8_bitmap = {}
        };
    }

#if defined(__EMSCRIPTEN__)
    constexpr int kMaxBuf = 256 * 256;
    thread_local std::vector<uint8_t> s_temp_buf(kMaxBuf);

    int out_w = 0, out_h = 0, out_bx = 0, out_by = 0;
    int ok = ktui_js_rasterize_char(glyph_id, font_size, &out_w, &out_h, &out_bx, &out_by, s_temp_buf.data(), kMaxBuf);
    if (ok && out_w > 0 && out_h > 0) {
        FontRasterResult res{
            .width = static_cast<uint32_t>(out_w),
            .height = static_cast<uint32_t>(out_h),
            .bearing_x = out_bx,
            .bearing_y = out_by,
            .r8_bitmap = std::vector<uint8_t>(s_temp_buf.data(), s_temp_buf.data() + (out_w * out_h))
        };
        if (enable_stem_darkening) {
            StemDarkener::apply(res.r8_bitmap, font_size);
        }
        return res;
    }
#endif

    // 若未在浏览器环境 (如 Node.js 自动化测试)，平滑回退至内置点阵引擎
    return m_impl->fallback_engine.rasterize_glyph(glyph_id, font_size, enable_stem_darkening);
}

std::unique_ptr<ITextEngine> create_web_text_engine() {
    return std::make_unique<WebTextEngine>();
}

} // namespace kernetile::text
