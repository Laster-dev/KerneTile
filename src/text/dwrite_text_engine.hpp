#pragma once

#include "kernetile/text/font_engine.hpp"

#if defined(_WIN32)
#include <dwrite.h>
#include <wrl/client.h>

namespace kernetile::text {

class DWriteTextEngine final : public ITextEngine {
public:
    DWriteTextEngine();
    ~DWriteTextEngine() override = default;

    [[nodiscard]] TextLayoutResult shape_text(
        std::string_view utf8_text, 
        float font_size, 
        std::string_view font_family) override;

    [[nodiscard]] FontRasterResult rasterize_glyph(
        uint32_t glyph_id, 
        float font_size,
        bool enable_stem_darkening) override;

    [[nodiscard]] FontMetrics get_metrics(
        float font_size, 
        std::string_view font_family) override;

private:
    Microsoft::WRL::ComPtr<IDWriteFactory> m_factory;
    Microsoft::WRL::ComPtr<IDWriteFontFace> m_default_font_face;
};

} // namespace kernetile::text
#endif
