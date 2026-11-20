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

FontMetrics DWriteTextEngine::get_metrics(float font_size, std::string_view font_family) {
    std::wstring family = font_family.empty() ? L"Segoe UI" : utf8_to_wide(font_family);

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_factory->CreateTextFormat(
        family.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font_size,
        L"",
        &format
    );

    if (FAILED(hr)) {
        return FontMetrics{
            .font_size = font_size,
            .ascender = font_size * 0.8f,
            .descender = -font_size * 0.2f,
            .line_height = font_size * 1.25f
        };
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
    std::wstring family = font_family.empty() ? L"Segoe UI" : utf8_to_wide(font_family);

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    m_factory->CreateTextFormat(
        family.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font_size,
        L"",
        &format
    );

    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    m_factory->CreateTextLayout(
        wide_text.c_str(),
        static_cast<UINT32>(wide_text.size()),
        format.Get(),
        100000.0f,
        100000.0f,
        &layout
    );

    DWRITE_TEXT_METRICS text_metrics{};
    layout->GetMetrics(&text_metrics);

    TextLayoutResult result;
    result.metrics = get_metrics(font_size, font_family);
    result.total_width = text_metrics.widthIncludingTrailingWhitespace;
    result.total_height = text_metrics.height;

    UINT32 clusterCount = 0;
    layout->GetClusterMetrics(nullptr, 0, &clusterCount);
    std::vector<DWRITE_CLUSTER_METRICS> clusterMetrics(clusterCount);
    layout->GetClusterMetrics(clusterMetrics.data(), clusterCount, &clusterCount);

    result.glyphs.reserve(wide_text.size());
    FLOAT current_x = 0.0f;
    UINT32 text_idx = 0;

    for (UINT32 c = 0; c < clusterCount; ++c) {
        const auto& cm = clusterMetrics[c];
        FLOAT adv = cm.width;
        FLOAT char_adv = cm.length > 0 ? (adv / cm.length) : adv;

        for (UINT16 k = 0; k < cm.length && text_idx < wide_text.size(); ++k, ++text_idx) {
            GlyphPlacement gp{
                .glyph_id = static_cast<uint32_t>(wide_text[text_idx]),
                .x = current_x,
                .y = result.metrics.ascender,
                .advance_x = char_adv,
                .advance_y = 0.0f,
                .width = char_adv,
                .height = result.metrics.line_height,
                .bearing_x = 0.0f,
                .bearing_y = result.metrics.ascender
            };
            result.glyphs.push_back(gp);
            current_x += char_adv;
        }
    }

    return result;
}

FontRasterResult DWriteTextEngine::rasterize_glyph(
    uint32_t glyph_id, 
    float font_size,
    bool enable_stem_darkening)
{
    // 优先使用 Windows GDI 原生高性能矢量字形光栅化 (GetGlyphOutlineW GGO_GRAY8_BITMAP)
    HDC hdc = CreateCompatibleDC(nullptr);
    if (hdc) {
        int font_height = -static_cast<int>(font_size + 0.5f);
        HFONT hfont = CreateFontW(
            font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
        );
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

                if (enable_stem_darkening) {
                    StemDarkener::apply(r8, font_size);
                }

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

std::unique_ptr<ITextEngine> create_text_engine() {
    try {
        return create_directwrite_engine();
    } catch (...) {
        return create_builtin_engine();
    }
}

} // namespace kernetile::text

#else

namespace kernetile::text {
std::unique_ptr<ITextEngine> create_directwrite_engine() { return nullptr; }
std::unique_ptr<ITextEngine> create_text_engine() { return create_builtin_engine(); }
}

#endif
