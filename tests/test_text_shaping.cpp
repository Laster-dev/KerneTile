#include "kernetile/text/font_engine.hpp"
#include "test_common.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string_view>

#define KT_CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "CHECK FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        std::exit(1); \
    } \
} while (0)

#define KT_CHECK_NEAR(a, b, eps) do { \
    const float _a = (a), _b = (b); \
    if (std::fabs(_a - _b) > (eps)) { \
        std::fprintf(stderr, "CHECK_NEAR FAILED at %s:%d: %s (%.6f) vs %s (%.6f), eps=%.6f\n", \
                     __FILE__, __LINE__, #a, _a, #b, _b, (eps)); \
        std::exit(1); \
    } \
} while (0)

namespace {

std::string_view pick_proportional_tnum_font(kernetile::text::ITextEngine& engine) {
    static constexpr std::string_view kCandidates[] = {
        "Candara",
        "Constantia",
        "Corbel",
        "Bahnschrift",
        "Segoe UI Variable Text",
        "DejaVu Sans",
        "Liberation Sans",
        "Noto Sans"
    };
    for (const auto fam : kCandidates) {
        const auto prop_1 = engine.shape_text("11111", 16.0f, fam, 400, false, false);
        const auto prop_8 = engine.shape_text("88888", 16.0f, fam, 400, false, false);
        const auto tab_1 = engine.shape_text("11111", 16.0f, fam, 400, false, true);
        const auto tab_8 = engine.shape_text("88888", 16.0f, fam, 400, false, true);
        if (prop_1.glyphs.size() == 5 && prop_8.glyphs.size() == 5 &&
            prop_1.total_width + 0.5f < prop_8.total_width &&
            std::fabs(tab_1.total_width - tab_8.total_width) <= 1e-3f) {
            return fam;
        }
    }
    return {};
}

void test_tabular_figures(kernetile::text::ITextEngine& engine) {
    using namespace kernetile::text;
    std::cout << "  -> Running tabular figures (tnum) shaping tests..." << std::endl;

    const auto fam = pick_proportional_tnum_font(engine);
#ifdef _WIN32
    KT_CHECK(!fam.empty());
#endif
    std::cout << "     Proportional+tnum probe font: '" << (fam.empty() ? "<none>" : fam) << "'" << std::endl;

    // 1. Tabular alignment across sizes (integer & fractional) on default "" font
    for (const float sz : {16.0f, 24.0f, 13.5f, 18.25f}) {
        const auto def_tab_1 = engine.shape_text("11111", sz, "", 400, false, true);
        const auto def_tab_8 = engine.shape_text("88888", sz, "", 400, false, true);
        KT_CHECK_NEAR(def_tab_1.total_width, def_tab_8.total_width, 1e-3f);
        const auto def_all = engine.shape_text("0123456789", sz, "", 400, false, true);
        const auto def_ones = engine.shape_text("1111111111", sz, "", 400, false, true);
        KT_CHECK_NEAR(def_all.total_width, def_ones.total_width, 1e-3f);
    }

    if (!fam.empty()) {
        // 2. Tabular equality on proportional font across integer & fractional sizes
        for (const float sz : {16.0f, 24.0f, 13.5f, 18.25f}) {
            const auto tab_1 = engine.shape_text("11111", sz, fam, 400, false, true);
            const auto tab_8 = engine.shape_text("88888", sz, fam, 400, false, true);
            KT_CHECK(tab_1.glyphs.size() == 5);
            KT_CHECK(tab_8.glyphs.size() == 5);
            KT_CHECK_NEAR(tab_1.total_width, tab_8.total_width, 1e-3f);
            for (size_t i = 0; i < 5; ++i) {
                KT_CHECK_NEAR(tab_1.glyphs[i].advance_x, tab_8.glyphs[i].advance_x, 1e-3f);
                KT_CHECK_NEAR(tab_1.glyphs[i].x, tab_8.glyphs[i].x, 1e-3f);
            }

            const auto tab_all = engine.shape_text("0123456789", sz, fam, 400, false, true);
            const auto tab_ones = engine.shape_text("1111111111", sz, fam, 400, false, true);
            KT_CHECK(tab_all.glyphs.size() == 10);
            KT_CHECK(tab_ones.glyphs.size() == 10);
            KT_CHECK_NEAR(tab_all.total_width, tab_ones.total_width, 1e-3f);
            for (size_t i = 0; i < 10; ++i) {
                KT_CHECK_NEAR(tab_all.glyphs[i].advance_x, tab_ones.glyphs[i].advance_x, 1e-3f);
                KT_CHECK_NEAR(tab_all.glyphs[i].x, tab_ones.glyphs[i].x, 1e-3f);
            }
        }

        // Mixed counter / numeric strings + caret stop alignment
        const auto fps_1 = engine.shape_text("FPS: 111.1%", 16.0f, fam, 400, false, true);
        const auto fps_8 = engine.shape_text("FPS: 888.8%", 16.0f, fam, 400, false, true);
        KT_CHECK_NEAR(fps_1.total_width, fps_8.total_width, 1e-3f);
        KT_CHECK(!fps_1.carets.empty() && !fps_8.carets.empty());
        KT_CHECK_NEAR(fps_1.carets.back().x, fps_8.carets.back().x, 1e-3f);
        KT_CHECK_NEAR(fps_1.glyphs.back().x, fps_8.glyphs.back().x, 1e-3f);

        const auto comma_1 = engine.shape_text("11,111", 16.0f, fam, 400, false, true);
        const auto comma_8 = engine.shape_text("88,888", 16.0f, fam, 400, false, true);
        KT_CHECK_NEAR(comma_1.total_width, comma_8.total_width, 1e-3f);

        // 3. Default preservation (tabular_nums == false and omitted argument)
        const auto prop_1 = engine.shape_text("11111", 16.0f, fam, 400, false, false);
        const auto prop_8 = engine.shape_text("88888", 16.0f, fam, 400, false, false);
        KT_CHECK(prop_1.total_width + 0.5f < prop_8.total_width);

        const auto omit_1 = engine.shape_text("11111", 16.0f, fam);
        KT_CHECK_NEAR(omit_1.total_width, prop_1.total_width, 0.0f);
        KT_CHECK(omit_1.glyphs.size() == prop_1.glyphs.size());
        for (size_t i = 0; i < prop_1.glyphs.size(); ++i) {
            KT_CHECK(omit_1.glyphs[i].glyph_id == prop_1.glyphs[i].glyph_id);
            KT_CHECK_NEAR(omit_1.glyphs[i].x, prop_1.glyphs[i].x, 0.0f);
            KT_CHECK_NEAR(omit_1.glyphs[i].advance_x, prop_1.glyphs[i].advance_x, 0.0f);
        }

        const auto l_omit = layout_text("11111", 16.0f, 400, false, fam);
        const auto l_false = layout_text("11111", 16.0f, 400, false, fam, false);
        KT_CHECK(l_omit.get() == l_false.get());
        KT_CHECK_NEAR(l_omit->total_width, l_false->total_width, 0.0f);

        // 4. LRU cache isolation in layout_text ("11111": false -> true -> false, "88888": true -> false -> true)
        const auto c_1_prop_1 = layout_text("11111", 16.0f, 400, false, fam, false);
        const auto c_1_tab = layout_text("11111", 16.0f, 400, false, fam, true);
        const auto c_1_prop_2 = layout_text("11111", 16.0f, 400, false, fam, false);
        KT_CHECK(c_1_prop_1.get() != c_1_tab.get());
        KT_CHECK(c_1_prop_1.get() == c_1_prop_2.get());
        KT_CHECK(c_1_tab->total_width > c_1_prop_1->total_width + 0.5f);

        const auto c_8_tab_1 = layout_text("88888", 16.0f, 400, false, fam, true);
        const auto c_8_prop = layout_text("88888", 16.0f, 400, false, fam, false);
        const auto c_8_tab_2 = layout_text("88888", 16.0f, 400, false, fam, true);
        KT_CHECK(c_8_tab_1.get() != c_8_prop.get());
        KT_CHECK(c_8_tab_1.get() == c_8_tab_2.get());
        KT_CHECK_NEAR(c_1_tab->total_width, c_8_tab_1->total_width, 1e-3f);
        KT_CHECK(std::fabs(c_8_tab_1->total_width - c_8_prop->total_width) > 0.5f ||
                 c_1_prop_1->total_width + 0.5f < c_8_prop->total_width);

        // 5. Multiline numeric layout alignment
        const auto ml_tab = layout_text("111\n888", 16.0f, 400, false, fam, true);
        KT_CHECK(ml_tab->glyphs.size() == 6);
        KT_CHECK(ml_tab->glyphs[3].y > ml_tab->glyphs[0].y);
        for (size_t i = 0; i < 3; ++i) {
            KT_CHECK_NEAR(ml_tab->glyphs[i].x, ml_tab->glyphs[i + 3].x, 1e-3f);
            KT_CHECK_NEAR(ml_tab->glyphs[i].advance_x, ml_tab->glyphs[i + 3].advance_x, 1e-3f);
        }
    }

    // 6. Edge cases: empty string and non-digit ASCII/CJK text
    const auto empty_shape = engine.shape_text("", 16.0f, fam, 400, false, true);
    KT_CHECK_NEAR(empty_shape.total_width, 0.0f, 1e-6f);
    KT_CHECK(empty_shape.glyphs.empty());
    KT_CHECK(empty_shape.carets.size() == 1);
    KT_CHECK(empty_shape.carets[0].byte == 0);
    KT_CHECK_NEAR(empty_shape.carets[0].x, 0.0f, 1e-6f);

    const auto empty_layout = layout_text("", 16.0f, 400, false, fam, true);
    KT_CHECK_NEAR(empty_layout->total_width, 0.0f, 1e-6f);
    KT_CHECK(empty_layout->glyphs.empty());
    KT_CHECK(empty_layout->carets.size() == 1);

    const std::string_view non_digits = "Hello KerneTile! 现代";
    const auto nd_prop = engine.shape_text(non_digits, 16.0f, fam, 400, false, false);
    const auto nd_tab = engine.shape_text(non_digits, 16.0f, fam, 400, false, true);
    KT_CHECK(nd_prop.glyphs.size() == nd_tab.glyphs.size());
    KT_CHECK_NEAR(nd_prop.total_width, nd_tab.total_width, 0.0f);

    // 7. Regression: OpenType kerning & ligature preservation on non-digit and mixed text
    static constexpr std::string_view kPreservationFonts[] = {
        "Candara",
        "Constantia",
        "Corbel",
        "Bahnschrift",
        "Segoe UI Variable Text",
        "Segoe UI",
        ""
    };
    static constexpr std::string_view kNonDigitStrings[] = {
        "Hello World",
        "office waffle",
        "AVATAR To"
    };
    for (const auto f : kPreservationFonts) {
        for (const auto s : kNonDigitStrings) {
            const auto prop = engine.shape_text(s, 16.0f, f, 400, false, false);
            const auto tab = engine.shape_text(s, 16.0f, f, 400, false, true);
            KT_CHECK(tab.glyphs.size() == prop.glyphs.size());
            KT_CHECK_NEAR(tab.total_width, prop.total_width, 0.0f);
            for (size_t i = 0; i < prop.glyphs.size(); ++i) {
                KT_CHECK(tab.glyphs[i].glyph_id == prop.glyphs[i].glyph_id);
                KT_CHECK_NEAR(tab.glyphs[i].x, prop.glyphs[i].x, 0.0f);
                KT_CHECK_NEAR(tab.glyphs[i].advance_x, prop.glyphs[i].advance_x, 0.0f);
            }
        }

        // Mixed kerning + digits: digits align tabularly AND non-digit kerning prefix is preserved
        const auto avatar_prop = engine.shape_text("AVATAR To 11111", 16.0f, f, 400, false, false);
        const auto avatar_tab1 = engine.shape_text("AVATAR To 11111", 16.0f, f, 400, false, true);
        const auto avatar_tab8 = engine.shape_text("AVATAR To 88888", 16.0f, f, 400, false, true);
        KT_CHECK_NEAR(avatar_tab1.total_width, avatar_tab8.total_width, 1e-3f);
        KT_CHECK(avatar_prop.glyphs.size() == 15);
        KT_CHECK(avatar_tab1.glyphs.size() == 15);
        for (size_t i = 0; i < 9; ++i) { // "AVATAR To"
            KT_CHECK(avatar_tab1.glyphs[i].glyph_id == avatar_prop.glyphs[i].glyph_id);
            KT_CHECK_NEAR(avatar_tab1.glyphs[i].x, avatar_prop.glyphs[i].x, 0.0f);
            KT_CHECK_NEAR(avatar_tab1.glyphs[i].advance_x, avatar_prop.glyphs[i].advance_x, 0.0f);
        }

        const auto mixed_fps1 = engine.shape_text("FPS: 111.1%", 16.0f, f, 400, false, true);
        const auto mixed_fps8 = engine.shape_text("FPS: 888.8%", 16.0f, f, 400, false, true);
        KT_CHECK_NEAR(mixed_fps1.total_width, mixed_fps8.total_width, 1e-3f);

        const auto mixed_comma1 = engine.shape_text("11,111.11%", 16.0f, f, 400, false, true);
        const auto mixed_comma8 = engine.shape_text("88,888.88%", 16.0f, f, 400, false, true);
        KT_CHECK_NEAR(mixed_comma1.total_width, mixed_comma8.total_width, 1e-3f);

        const auto emoji1 = engine.shape_text("111\xF0\x9F\x9A\x80" "111", 16.0f, f, 400, false, true);
        const auto emoji8 = engine.shape_text("888\xF0\x9F\x9A\x80" "888", 16.0f, f, 400, false, true);
        KT_CHECK_NEAR(emoji1.total_width, emoji8.total_width, 1e-3f);

        const auto lig_prop = engine.shape_text("office waffle 111", 16.0f, f, 400, false, false);
        const auto lig_tab1 = engine.shape_text("office waffle 111", 16.0f, f, 400, false, true);
        const auto lig_tab8 = engine.shape_text("office waffle 888", 16.0f, f, 400, false, true);
        KT_CHECK_NEAR(lig_tab1.total_width, lig_tab8.total_width, 1e-3f);
        KT_CHECK(lig_tab1.glyphs.size() == lig_prop.glyphs.size());
        KT_CHECK(lig_tab1.glyphs.size() > 3);
        for (size_t i = 0; i < lig_prop.glyphs.size() - 3; ++i) {
            KT_CHECK(lig_tab1.glyphs[i].glyph_id == lig_prop.glyphs[i].glyph_id);
            KT_CHECK_NEAR(lig_tab1.glyphs[i].x, lig_prop.glyphs[i].x, 0.0f);
            KT_CHECK_NEAR(lig_tab1.glyphs[i].advance_x, lig_prop.glyphs[i].advance_x, 0.0f);
        }
    }

#ifdef _WIN32
    // 8. Regression: Times New Roman digit kerning suppression under tabular_nums=true
    //    while preserving default proportional '1'+'1' kerning and non-digit prefix kerning.
    {
        constexpr std::string_view kTnr = "Times New Roman";
        const auto tnr_prop_1 = engine.shape_text("11111", 16.0f, kTnr, 400, false, false);
        const auto tnr_prop_8 = engine.shape_text("88888", 16.0f, kTnr, 400, false, false);
        KT_CHECK(tnr_prop_1.glyphs.size() == 5 && tnr_prop_8.glyphs.size() == 5);
        KT_CHECK(tnr_prop_1.total_width + 0.5f < tnr_prop_8.total_width);

        const auto tnr_tab_1 = engine.shape_text("11111", 16.0f, kTnr, 400, false, true);
        const auto tnr_tab_8 = engine.shape_text("88888", 16.0f, kTnr, 400, false, true);
        KT_CHECK(tnr_tab_1.glyphs.size() == 5 && tnr_tab_8.glyphs.size() == 5);
        KT_CHECK_NEAR(tnr_tab_1.total_width, tnr_tab_8.total_width, 1e-3f);
        for (size_t i = 0; i < 5; ++i) {
            KT_CHECK_NEAR(tnr_tab_1.glyphs[i].x, tnr_tab_8.glyphs[i].x, 1e-3f);
            KT_CHECK_NEAR(tnr_tab_1.glyphs[i].advance_x, tnr_tab_8.glyphs[i].advance_x, 1e-3f);
        }

        const auto tnr_avatar_prop = engine.shape_text("AVATAR To 11111", 16.0f, kTnr, 400, false, false);
        const auto tnr_avatar_tab1 = engine.shape_text("AVATAR To 11111", 16.0f, kTnr, 400, false, true);
        const auto tnr_avatar_tab8 = engine.shape_text("AVATAR To 88888", 16.0f, kTnr, 400, false, true);
        KT_CHECK_NEAR(tnr_avatar_tab1.total_width, tnr_avatar_tab8.total_width, 1e-3f);
        KT_CHECK(tnr_avatar_prop.glyphs.size() == 15);
        KT_CHECK(tnr_avatar_tab1.glyphs.size() == 15);
        KT_CHECK(tnr_avatar_tab8.glyphs.size() == 15);
        for (size_t i = 0; i < 9; ++i) { // "AVATAR To"
            KT_CHECK(tnr_avatar_tab1.glyphs[i].glyph_id == tnr_avatar_prop.glyphs[i].glyph_id);
            KT_CHECK_NEAR(tnr_avatar_tab1.glyphs[i].x, tnr_avatar_prop.glyphs[i].x, 0.0f);
            KT_CHECK_NEAR(tnr_avatar_tab1.glyphs[i].advance_x, tnr_avatar_prop.glyphs[i].advance_x, 0.0f);
        }

        const auto tnr_pct_1 = engine.shape_text("-111.11%", 16.0f, kTnr, 400, false, true);
        const auto tnr_pct_8 = engine.shape_text("-888.88%", 16.0f, kTnr, 400, false, true);
        KT_CHECK_NEAR(tnr_pct_1.total_width, tnr_pct_8.total_width, 1e-3f);
    }
#endif
}

} // namespace

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_text_shaping..." << std::endl;

    using namespace kernetile::text;

    auto engine = create_text_engine();
    KT_CHECK(engine != nullptr);

    // 1. 验证英文字符串排版
    std::string_view latin_text = "Hello KerneTile!";
    auto layout_latin = engine->shape_text(latin_text, 16.0f);

    std::cout << "  -> Latin Text: '" << latin_text << "' -> Glyphs: " << layout_latin.glyphs.size() 
              << ", Width: " << layout_latin.total_width << "px" << std::endl;

    KT_CHECK(layout_latin.glyphs.size() == latin_text.size());
    KT_CHECK(layout_latin.total_width > 50.0f);
    KT_CHECK(layout_latin.metrics.line_height >= 16.0f);

    // 验证单调递增的排版 X 坐标
    for (size_t i = 1; i < layout_latin.glyphs.size(); ++i) {
        KT_CHECK(layout_latin.glyphs[i].x >= layout_latin.glyphs[i - 1].x);
    }

    // 2. 验证中文字符串全角排版
    std::string_view cjk_text = "现代GPU计算";
    auto layout_cjk = engine->shape_text(cjk_text, 20.0f);

    std::cout << "  -> CJK Text: '" << cjk_text << "' -> Glyphs: " << layout_cjk.glyphs.size() 
              << ", Width: " << layout_cjk.total_width << "px" << std::endl;

    KT_CHECK(layout_cjk.glyphs.size() == 7); // 7 个字符
    KT_CHECK(layout_cjk.total_width > 100.0f);

    const auto combining = layout_text("e\xcc\x81", 16.0f);
    KT_CHECK(combining->carets.size() == 2);
    KT_CHECK(combining->carets.back().byte == 3);
    KT_CHECK(!combining->glyphs.empty());
    KT_CHECK(combining->carets.front().byte == 0);
    const auto multiline = layout_text("中文\nGPU", 16.0f);
    KT_CHECK(multiline->glyphs.size() == 5);
    KT_CHECK(multiline->glyphs[2].y > multiline->glyphs[0].y);

    // 3. 验证字形光栅化
    auto raster = engine->rasterize_glyph('A', 16.0f, true);
    KT_CHECK(raster.width > 0);
    KT_CHECK(raster.height > 0);
    KT_CHECK(!raster.r8_bitmap.empty());
    KT_CHECK(raster.r8_bitmap.size() == raster.width * raster.height);

    std::cout << "  -> Glyph 'A' Raster: " << raster.width << "x" << raster.height << " (" 
              << raster.r8_bitmap.size() << " bytes R8)" << std::endl;

    // 4. Tabular figures (tnum) tests
    test_tabular_figures(*engine);

    std::cout << "[TEST] test_text_shaping PASSED successfully!" << std::endl;
    return 0;
}
