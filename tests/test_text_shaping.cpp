#include "kernetile/text/font_engine.hpp"
#include "test_common.hpp"
#include <iostream>
#include <cassert>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_text_shaping..." << std::endl;

    using namespace kernetile::text;

    auto engine = create_text_engine();
    assert(engine != nullptr);

    // 1. 验证英文字符串排版
    std::string_view latin_text = "Hello KerneTile!";
    auto layout_latin = engine->shape_text(latin_text, 16.0f);

    std::cout << "  -> Latin Text: '" << latin_text << "' -> Glyphs: " << layout_latin.glyphs.size() 
              << ", Width: " << layout_latin.total_width << "px" << std::endl;

    assert(layout_latin.glyphs.size() == latin_text.size());
    assert(layout_latin.total_width > 50.0f);
    assert(layout_latin.metrics.line_height >= 16.0f);

    // 验证单调递增的排版 X 坐标
    for (size_t i = 1; i < layout_latin.glyphs.size(); ++i) {
        assert(layout_latin.glyphs[i].x >= layout_latin.glyphs[i - 1].x);
    }

    // 2. 验证中文字符串全角排版
    std::string_view cjk_text = "现代GPU计算";
    auto layout_cjk = engine->shape_text(cjk_text, 20.0f);

    std::cout << "  -> CJK Text: '" << cjk_text << "' -> Glyphs: " << layout_cjk.glyphs.size() 
              << ", Width: " << layout_cjk.total_width << "px" << std::endl;

    assert(layout_cjk.glyphs.size() == 7); // 7 个字符
    assert(layout_cjk.total_width > 100.0f);

    const auto combining = layout_text("e\xcc\x81", 16.0f);
    assert(combining->carets.size() == 2);
    assert(combining->carets.back().byte == 3);
    assert(combining->glyphs.size() == 1);
    const auto multiline = layout_text("中文\nGPU", 16.0f);
    assert(multiline->glyphs.size() == 5);
    assert(multiline->glyphs[2].y > multiline->glyphs[0].y);

    // 3. 验证字形光栅化
    auto raster = engine->rasterize_glyph('A', 16.0f, true);
    assert(raster.width > 0);
    assert(raster.height > 0);
    assert(!raster.r8_bitmap.empty());
    assert(raster.r8_bitmap.size() == raster.width * raster.height);

    std::cout << "  -> Glyph 'A' Raster: " << raster.width << "x" << raster.height << " (" 
              << raster.r8_bitmap.size() << " bytes R8)" << std::endl;

    std::cout << "[TEST] test_text_shaping PASSED successfully!" << std::endl;
    return 0;
}
