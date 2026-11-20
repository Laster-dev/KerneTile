#pragma once

#include "kernetile/text/font_engine.hpp"

namespace kernetile::text {

class BuiltinTextEngine final : public ITextEngine {
public:
    BuiltinTextEngine() = default;
    ~BuiltinTextEngine() override = default;

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
};

} // namespace kernetile::text
