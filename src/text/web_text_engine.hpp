#pragma once

#include "kernetile/text/font_engine.hpp"
#include <memory>

namespace kernetile::text {

// WebAssembly / HTML5 Canvas 2D 硬件级矢量中英文文本排版与光栅化引擎
class WebTextEngine : public ITextEngine {
public:
    WebTextEngine();
    ~WebTextEngine() override;

    FontMetrics get_metrics(
        float font_size,
        std::string_view font_family = "") override;

    TextLayoutResult shape_text(
        std::string_view utf8_text,
        float font_size,
        std::string_view font_family = "") override;

    FontRasterResult rasterize_glyph(
        uint32_t glyph_id,
        float font_size,
        bool enable_stem_darkening = true) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

std::unique_ptr<ITextEngine> create_web_text_engine();

} // namespace kernetile::text
