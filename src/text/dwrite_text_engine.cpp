#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite_1.h>
#include <wrl/client.h>

#include "kernetile/text/font_engine.hpp"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace kernetile::text {
namespace {
using Microsoft::WRL::ComPtr;

std::wstring utf16(std::string_view text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(count, L'\0');
    if (count) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count);
    return result;
}

bool is_high_surrogate(wchar_t ch) { return ch >= 0xd800 && ch <= 0xdbff; }
bool is_low_surrogate(wchar_t ch) { return ch >= 0xdc00 && ch <= 0xdfff; }

std::vector<size_t> byte_offsets(std::wstring_view text) {
    std::vector<size_t> result(text.size() + 1);
    size_t byte = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        result[i] = byte;
        const auto ch = text[i];
        if (is_high_surrogate(ch) && i + 1 < text.size() && is_low_surrogate(text[i + 1])) {
            result[++i] = byte;
            byte += 4;
        } else {
            byte += ch < 0x80 ? 1 : ch < 0x800 ? 2 : 3;
        }
    }
    result.back() = byte;
    return result;
}

LOGFONTW system_font() {
    NONCLIENTMETRICSW settings{};
    settings.cbSize = sizeof(settings);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, settings.cbSize, &settings, 0))
        return settings.lfMessageFont;
    LOGFONTW fallback{};
    GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(fallback), &fallback);
    return fallback;
}

struct Glyph {
    ComPtr<IDWriteFontFace> face;
    UINT16 index;
};

class SystemTextEngine final : public ITextEngine {
public:
    SystemTextEngine() : m_font(system_font()) {
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(m_factory.GetAddressOf()))))
            throw std::runtime_error("Cannot initialize DirectWrite");
        m_factory->GetGdiInterop(&m_gdi);
    }

    FontMetrics get_metrics(float size, std::string_view family) override {
        auto layout = make_layout(L"M", size, family);
        DWRITE_LINE_METRICS line{};
        UINT32 count = 0;
        if (layout && SUCCEEDED(layout->GetLineMetrics(&line, 1, &count)) && count)
            return {size, line.baseline, line.baseline - line.height, line.height};
        return {size, size * 0.8f, -size * 0.2f, size};
    }

    TextLayoutResult shape_text(std::string_view text, float size, std::string_view family) override {
        TextLayoutResult result;
        result.metrics = get_metrics(size, family);
        result.total_height = result.metrics.line_height;
        if (text.empty()) { result.carets.push_back({0, 0}); return result; }
        const auto wide = utf16(text);
        const auto offsets = byte_offsets(wide);
        auto layout = make_layout(wide, size, family);
        if (!layout) return result;
        DWRITE_TEXT_METRICS metrics{};
        if (SUCCEEDED(layout->GetMetrics(&metrics))) {
            result.total_width = metrics.widthIncludingTrailingWhitespace;
            result.total_height = metrics.height;
        }
        Collector collector(*this, result, offsets);
        layout->Draw(nullptr, &collector, 0, 0);
        for (const auto byte : grapheme_boundaries(text)) {
            float x = 0, y = 0;
            DWRITE_HIT_TEST_METRICS hit{};
            const auto position = static_cast<UINT32>(std::lower_bound(offsets.begin(), offsets.end(), byte) - offsets.begin());
            layout->HitTestTextPosition(position, FALSE, &x, &y, &hit);
            result.carets.push_back({byte, x});
        }
        return result;
    }

    FontRasterResult rasterize_glyph(uint32_t id, float size, bool) override {
        Glyph glyph{};
        if (id >= glyph_marker) {
            if (id - glyph_marker >= m_glyphs.size()) return {};
            glyph = m_glyphs[id - glyph_marker];
        } else {
            ComPtr<IDWriteFont> font;
            if (!m_gdi || FAILED(m_gdi->CreateFontFromLOGFONT(&m_font, &font)) ||
                FAILED(font->CreateFontFace(&glyph.face))) return {};
            glyph.face->GetGlyphIndices(&id, 1, &glyph.index);
        }

        DWRITE_GLYPH_RUN run{};
        run.fontFace = glyph.face.Get();
        run.fontEmSize = size;
        run.glyphCount = 1;
        run.glyphIndices = &glyph.index;
        ComPtr<IDWriteGlyphRunAnalysis> analysis;
        if (FAILED(m_factory->CreateGlyphRunAnalysis(&run, 1.0f, nullptr,
            DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &analysis))) return {};
        RECT box{};
        if (FAILED(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &box)) ||
            box.right <= box.left || box.bottom <= box.top) return {};
        const auto width = static_cast<uint32_t>(box.right - box.left);
        const auto height = static_cast<uint32_t>(box.bottom - box.top);
        std::vector<uint8_t> coverage(size_t(width) * height * 3);
        if (FAILED(analysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &box,
            coverage.data(), static_cast<UINT32>(coverage.size())))) return {};
        FontRasterResult result{width, height, box.left, -box.top, std::vector<uint8_t>(size_t(width) * height)};
        for (size_t i = 0; i < result.r8_bitmap.size(); ++i) {
            result.r8_bitmap[i] = static_cast<uint8_t>((coverage[i * 3] + coverage[i * 3 + 1] + coverage[i * 3 + 2] + 1) / 3);
        }
        return result;
    }

private:
    static constexpr uint32_t glyph_marker = 1u << 31;

    class Collector final : public IDWriteTextRenderer {
    public:
        Collector(SystemTextEngine& engine, TextLayoutResult& output, const std::vector<size_t>& offsets)
            : m_engine(engine), m_output(output), m_offsets(offsets) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** value) override {
            if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWritePixelSnapping) || iid == __uuidof(IDWriteTextRenderer)) {
                *value = static_cast<IDWriteTextRenderer*>(this);
                AddRef();
                return S_OK;
            }
            *value = nullptr;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
        ULONG STDMETHODCALLTYPE Release() override { return 1; }
        HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override { *disabled = TRUE; return S_OK; }
        HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* matrix) override {
            *matrix = {1, 0, 0, 1, 0, 0};
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* scale) override { *scale = 1; return S_OK; }

        HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE,
            const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION* description, IUnknown*) override {
            const float direction = run->bidiLevel & 1 ? -1.0f : 1.0f;
            std::vector<size_t> clusters(run->glyphCount);
            if (description && description->clusterMap) {
                for (UINT32 i = description->stringLength; i > 0; --i) {
                    const auto glyph = description->clusterMap[i - 1];
                    const auto position = description->textPosition + i - 1;
                    if (glyph < clusters.size() && position < m_offsets.size()) clusters[glyph] = m_offsets[position];
                }
            }
            for (UINT32 i = 0; i < run->glyphCount; ++i) {
                const float advance = run->glyphAdvances[i];
                const auto offset = run->glyphOffsets ? run->glyphOffsets[i] : DWRITE_GLYPH_OFFSET{};
                GlyphPlacement glyph{};
                glyph.glyph_id = m_engine.remember(run->fontFace, run->glyphIndices[i]);
                glyph.x = direction < 0 ? x - advance - offset.advanceOffset : x + offset.advanceOffset;
                glyph.y = y - offset.ascenderOffset;
                glyph.advance_x = advance;
                glyph.cluster = clusters[i];
                m_output.glyphs.push_back(glyph);
                x += advance * direction;
            }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return S_OK; }
    private:
        SystemTextEngine& m_engine;
        TextLayoutResult& m_output;
        const std::vector<size_t>& m_offsets;
    };

    uint32_t remember(IDWriteFontFace* face, UINT16 index) {
        const auto it = std::find_if(m_glyphs.begin(), m_glyphs.end(), [&](const Glyph& glyph) {
            return glyph.index == index && glyph.face.Get() == face;
        });
        if (it != m_glyphs.end()) return glyph_marker + static_cast<uint32_t>(it - m_glyphs.begin());
        m_glyphs.push_back({ComPtr<IDWriteFontFace>(face), index});
        return glyph_marker + static_cast<uint32_t>(m_glyphs.size() - 1);
    }

    ComPtr<IDWriteTextLayout> make_layout(std::wstring_view text, float size, std::string_view family) {
        const auto name = family.empty() ? std::wstring(m_font.lfFaceName) : utf16(family);
        wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
        GetUserDefaultLocaleName(locale, static_cast<int>(std::size(locale)));
        ComPtr<IDWriteTextFormat> format;
        if (FAILED(m_factory->CreateTextFormat(name.c_str(), nullptr,
            static_cast<DWRITE_FONT_WEIGHT>(m_font.lfWeight ? m_font.lfWeight : FW_NORMAL),
            m_font.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, locale, &format))) return {};
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        ComPtr<IDWriteTextLayout> layout;
        m_factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format.Get(), 65536, 65536, &layout);
        return layout;
    }

    LOGFONTW m_font;
    ComPtr<IDWriteFactory> m_factory;
    ComPtr<IDWriteGdiInterop> m_gdi;
    std::vector<Glyph> m_glyphs;
};
} // namespace

std::unique_ptr<ITextEngine> create_directwrite_engine() { return std::make_unique<SystemTextEngine>(); }
std::unique_ptr<ITextEngine> create_text_engine() { return create_directwrite_engine(); }

std::vector<size_t> grapheme_boundaries(std::string_view text) {
    const auto wide = utf16(text);
    const auto offsets = byte_offsets(wide);
    std::vector<WORD> categories(wide.size());
    if (!wide.empty()) GetStringTypeW(CT_CTYPE3, wide.data(), static_cast<int>(wide.size()), categories.data());
    std::vector<size_t> boundaries{0};
    for (size_t i = 1; i < wide.size(); ++i) {
        const auto ch = wide[i];
        const bool joins = is_low_surrogate(ch) || (categories[i] & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK)) ||
            ch == 0x200d || wide[i - 1] == 0x200d || (ch >= 0xfe00 && ch <= 0xfe0f) ||
            (ch == L'\n' && wide[i - 1] == L'\r');
        if (!joins && offsets[i] < text.size()) boundaries.push_back(offsets[i]);
    }
    if (!text.empty()) boundaries.push_back(text.size());
    return boundaries;
}
} // namespace kernetile::text
