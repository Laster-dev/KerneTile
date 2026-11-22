#include "dwrite_text_engine.hpp"
#include "builtin_text_engine.hpp"
#include "kernetile/text/stem_darkening.hpp"
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#pragma comment(lib, "dwrite.lib")

namespace kernetile::text {

static std::wstring utf8_to_wide(std::string_view utf8) {
    if (utf8.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &wide[0], size_needed);
    return wide;
}

DWriteTextEngine::DWriteTextEngine() {
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_factory.GetAddressOf())
    );
    if (FAILED(hr)) {
        throw std::runtime_error("DWriteCreateFactory failed");
    }
}

static bool is_cjk_or_non_ascii(uint32_t ch) noexcept {
    return ch > 0x7Fu;
}

static HFONT create_system_font(float font_size, bool use_cjk_fallback, std::string_view font_family = "") {
    std::wstring family;
    if (!font_family.empty()) {
        family = utf8_to_wide(font_family);
    } else {
        family = use_cjk_fallback ? L"Microsoft YaHei UI" : L"Segoe UI";
    }
    int font_height = -static_cast<int>(font_size + 0.5f);
    if (font_height == 0) font_height = -12;
    return CreateFontW(
        font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family.c_str()
    );
}

FontMetrics DWriteTextEngine::get_metrics(float font_size, std::string_view font_family) {
    HDC hdc = CreateCompatibleDC(nullptr);
    if (hdc) {
        HFONT hfont = create_system_font(font_size, false, font_family);
        HGDIOBJ old_font = SelectObject(hdc, hfont);
        TEXTMETRICW tm{};
        bool ok = GetTextMetricsW(hdc, &tm) != FALSE;
        SelectObject(hdc, old_font);
        DeleteObject(hfont);
        DeleteDC(hdc);
        if (ok) {
            return FontMetrics{
                .font_size = font_size,
                .ascender = static_cast<float>(tm.tmAscent),
                .descender = -static_cast<float>(tm.tmDescent),
                .line_height = static_cast<float>(tm.tmHeight + tm.tmExternalLeading)
            };
        }
    }

    return FontMetrics{
        .font_size = font_size,
        .ascender = font_size * 0.8f,
        .descender = -font_size * 0.2f,
        .line_height = font_size * 1.25f
    };
}

TextLayoutResult DWriteTextEngine::shape_text(
    std::string_view utf8_text, 
    float font_size, 
    std::string_view font_family)
{
    std::wstring wide_text = utf8_to_wide(utf8_text);
    TextLayoutResult result;
    result.metrics = get_metrics(font_size, font_family);
    result.glyphs.reserve(wide_text.size());

    HDC hdc = CreateCompatibleDC(nullptr);
    HFONT hfont_latin = hdc ? create_system_font(font_size, false, font_family) : nullptr;
    HFONT hfont_cjk   = hdc ? create_system_font(font_size, true, font_family)  : nullptr;
    HGDIOBJ old_font  = (hdc && hfont_latin) ? SelectObject(hdc, hfont_latin) : nullptr;
    bool current_is_cjk = false;

    MAT2 mat{};
    mat.eM11.value = 1;
    mat.eM22.value = 1;

    float current_x = 0.0f;
    for (wchar_t ch : wide_text) {
        float adv = is_cjk_or_non_ascii(ch) ? font_size : (font_size * 0.5f);
        float gw = adv;
        float gh = result.metrics.line_height;
        float bx = 0.0f;
        float by = result.metrics.ascender;

        if (hdc && hfont_latin && hfont_cjk) {
            bool want_cjk = is_cjk_or_non_ascii(ch);
            if (want_cjk != current_is_cjk) {
                SelectObject(hdc, want_cjk ? hfont_cjk : hfont_latin);
                current_is_cjk = want_cjk;
            }
            GLYPHMETRICS gm{};
            if (GetGlyphOutlineW(hdc, static_cast<UINT>(ch), GGO_METRICS, &gm, 0, nullptr, &mat) != GDI_ERROR) {
                adv = static_cast<float>(gm.gmCellIncX);
                gw = static_cast<float>(gm.gmBlackBoxX);
                gh = static_cast<float>(gm.gmBlackBoxY);
                bx = static_cast<float>(gm.gmptGlyphOrigin.x);
                by = static_cast<float>(gm.gmptGlyphOrigin.y);
            }
        }

        GlyphPlacement gp{
            .glyph_id = static_cast<uint32_t>(ch),
            .x = current_x,
            .y = result.metrics.ascender,
            .advance_x = adv,
            .advance_y = 0.0f,
            .width = gw,
            .height = gh,
            .bearing_x = bx,
            .bearing_y = by
        };
        result.glyphs.push_back(gp);
        current_x += adv;
    }

    if (hdc) {
        if (old_font) SelectObject(hdc, old_font);
        if (hfont_latin) DeleteObject(hfont_latin);
        if (hfont_cjk) DeleteObject(hfont_cjk);
        DeleteDC(hdc);
    }

    result.total_width = current_x;
    result.total_height = result.metrics.line_height;
    return result;
}

FontRasterResult DWriteTextEngine::rasterize_glyph(
    uint32_t glyph_id, 
    float font_size,
    bool enable_stem_darkening)
{
    (void)enable_stem_darkening;
    // 使用与 shape_text 完全一致的 Windows TrueType 字形光栅化 (支持中英自动回退)
    HDC hdc = CreateCompatibleDC(nullptr);
    if (hdc) {
        bool use_cjk = is_cjk_or_non_ascii(glyph_id);
        HFONT hfont = create_system_font(font_size, use_cjk);
        HGDIOBJ old_font = SelectObject(hdc, hfont);

        GLYPHMETRICS gm{};
        MAT2 mat{};
        mat.eM11.value = 1;
        mat.eM22.value = 1;

        DWORD size = GetGlyphOutlineW(hdc, glyph_id, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &mat);
        if (size != GDI_ERROR && size > 0) {
            std::vector<uint8_t> buffer(size);
            if (GetGlyphOutlineW(hdc, glyph_id, GGO_GRAY8_BITMAP, &gm, size, buffer.data(), &mat) != GDI_ERROR) {
                uint32_t w = gm.gmBlackBoxX;
                uint32_t h = gm.gmBlackBoxY;
                uint32_t pitch = (w + 3) & ~3;

                std::vector<uint8_t> r8(w * h);
                for (uint32_t y = 0; y < h; ++y) {
                    for (uint32_t x = 0; x < w; ++x) {
                        uint8_t val = buffer[y * pitch + x];
                        uint32_t scaled = static_cast<uint32_t>(val * 255 / 64);
                        r8[y * w + x] = static_cast<uint8_t>(scaled > 255 ? 255 : scaled);
                    }
                }

                SelectObject(hdc, old_font);
                DeleteObject(hfont);
                DeleteDC(hdc);

                return FontRasterResult{
                    .width = w,
                    .height = h,
                    .bearing_x = gm.gmptGlyphOrigin.x,
                    .bearing_y = gm.gmptGlyphOrigin.y,
                    .r8_bitmap = std::move(r8)
                };
            }
        }

        SelectObject(hdc, old_font);
        DeleteObject(hfont);
        DeleteDC(hdc);
    }

    // 后备高质量点阵/矢量笔划光栅化
    BuiltinTextEngine fallback;
    return fallback.rasterize_glyph(glyph_id, font_size, enable_stem_darkening);
}

std::unique_ptr<ITextEngine> create_directwrite_engine() {
    return std::make_unique<DWriteTextEngine>();
}



} // namespace kernetile::text

#else

#if defined(__EMSCRIPTEN__)
#include "web_text_engine.hpp"

namespace kernetile::text {
std::unique_ptr<ITextEngine> create_directwrite_engine() { return nullptr; }

}

#else
#include "vector_text_engine.hpp"

namespace kernetile::text {
std::unique_ptr<ITextEngine> create_directwrite_engine() { return nullptr; }

}
#endif

#endif
