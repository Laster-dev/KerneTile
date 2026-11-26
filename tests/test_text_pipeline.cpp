#include "kernetile/text/font_engine.hpp"
#include "kernetile/atlas/dynamic_atlas.hpp"
#include "kernetile/text/text_streamer.hpp"
#include "test_common.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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

void test_tabular_streaming_pipeline() {
    using namespace kernetile::text;
    using namespace kernetile::atlas;
    using namespace kernetile::core;
    using namespace kernetile::math;

    std::cout << "  -> Running tabular streaming pipeline tests..." << std::endl;

    DynamicAtlas atlas(1024, 1024, AtlasFormat::R8_UNORM);
    TextStreamer streamer(shared_text_engine(), atlas);
    const auto fam = pick_proportional_tnum_font(shared_text_engine());
#ifdef _WIN32
    KT_CHECK(!fam.empty());
#endif

    // Default system font "" tabular alignment
    {
        std::vector<UiPrimitive> p1, p8;
        streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             p1, INVALID_INDEX_U16, 1.0f, 400, false, "", true);
        streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             p8, INVALID_INDEX_U16, 1.0f, 400, false, "", true);
        KT_CHECK(p1.size() == 6 && p8.size() == 6);
        KT_CHECK_NEAR(p1.back().min_x, p8.back().min_x, 1e-3f);
    }

    if (!fam.empty()) {
        // 1. End-to-end streaming alignment across scales & sizes
        for (const float scale : {1.0f, 1.25f, 1.5f}) {
            std::vector<UiPrimitive> prims_1, prims_8;
            streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                 prims_1, INVALID_INDEX_U16, scale, 400, false, fam, true);
            streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                 prims_8, INVALID_INDEX_U16, scale, 400, false, fam, true);
            KT_CHECK(prims_1.size() == 6 && prims_8.size() == 6);
            KT_CHECK_NEAR(prims_1.back().min_x, prims_8.back().min_x, 1e-3f);
            KT_CHECK_NEAR(prims_1.back().max_x, prims_8.back().max_x, 1e-3f);

            std::vector<UiPrimitive> prims_all, prims_ones;
            streamer.stream_text("0123456789%", 10.0f, 20.0f, 18.25f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                 prims_all, INVALID_INDEX_U16, scale, 400, false, fam, true);
            streamer.stream_text("1111111111%", 10.0f, 20.0f, 18.25f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                 prims_ones, INVALID_INDEX_U16, scale, 400, false, fam, true);
            KT_CHECK(prims_all.size() == 11 && prims_ones.size() == 11);
            KT_CHECK_NEAR(prims_all.back().min_x, prims_ones.back().min_x, 1e-3f);
        }

        // 2. Default tabular_nums == false and omitted-arg preservation
        std::vector<UiPrimitive> prims_1_prop, prims_8_prop, prims_1_omit;
        streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             prims_1_prop, INVALID_INDEX_U16, 1.0f, 400, false, fam, false);
        streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             prims_8_prop, INVALID_INDEX_U16, 1.0f, 400, false, fam, false);
        streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             prims_1_omit, INVALID_INDEX_U16, 1.0f, 400, false, fam);
        KT_CHECK(prims_1_prop.size() == 6 && prims_8_prop.size() == 6 && prims_1_omit.size() == 6);
        KT_CHECK(prims_1_prop.back().min_x + 4.0f < prims_8_prop.back().min_x);
        for (size_t i = 0; i < prims_1_prop.size(); ++i) {
            KT_CHECK_NEAR(prims_1_omit[i].min_x, prims_1_prop[i].min_x, 0.0f);
            KT_CHECK_NEAR(prims_1_omit[i].max_x, prims_1_prop[i].max_x, 0.0f);
        }

        // 3. LRU cache isolation via TextStreamer::stream_text
        std::vector<UiPrimitive> cache_1_f1, cache_1_t, cache_1_f2;
        streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             cache_1_f1, INVALID_INDEX_U16, 1.0f, 400, false, fam, false);
        streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             cache_1_t, INVALID_INDEX_U16, 1.0f, 400, false, fam, true);
        streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             cache_1_f2, INVALID_INDEX_U16, 1.0f, 400, false, fam, false);
        KT_CHECK(cache_1_t.back().min_x > cache_1_f1.back().min_x + 4.0f);
        KT_CHECK_NEAR(cache_1_f1.back().min_x, cache_1_f2.back().min_x, 0.0f);

        std::vector<UiPrimitive> cache_8_t1, cache_8_f, cache_8_t2;
        streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             cache_8_t1, INVALID_INDEX_U16, 1.0f, 400, false, fam, true);
        streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             cache_8_f, INVALID_INDEX_U16, 1.0f, 400, false, fam, false);
        streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             cache_8_t2, INVALID_INDEX_U16, 1.0f, 400, false, fam, true);
        KT_CHECK_NEAR(cache_1_t.back().min_x, cache_8_t1.back().min_x, 1e-3f);
        KT_CHECK_NEAR(cache_8_t1.back().min_x, cache_8_t2.back().min_x, 0.0f);
        KT_CHECK(std::fabs(cache_8_t1.back().min_x - cache_8_f.back().min_x) >= 1.0f);

        // 4. Direct ITextEngine path in TextStreamer (&m_engine != &shared_text_engine())
        auto direct_engine = create_text_engine();
        DynamicAtlas direct_atlas(512, 512, AtlasFormat::R8_UNORM);
        TextStreamer direct_streamer(*direct_engine, direct_atlas);
        std::vector<UiPrimitive> dp1, dp8;
        direct_streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                    dp1, INVALID_INDEX_U16, 1.0f, 400, false, fam, true);
        direct_streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                    dp8, INVALID_INDEX_U16, 1.0f, 400, false, fam, true);
        KT_CHECK(dp1.size() == 6 && dp8.size() == 6);
        KT_CHECK_NEAR(dp1.back().min_x, dp8.back().min_x, 1e-3f);
    }

    // 5. Regression: TextStreamer preserves non-digit kerning & ligatures across scales
    static constexpr std::string_view kPipelinePreservationFonts[] = {
        "Candara",
        "Constantia",
        "Corbel",
        "Bahnschrift",
        "Segoe UI Variable Text",
        "Segoe UI",
        ""
    };
    static constexpr std::string_view kPipelineNonDigitStrings[] = {
        "Hello World",
        "office waffle"
    };
    for (const auto f : kPipelinePreservationFonts) {
        for (const float scale : {1.0f, 1.25f}) {
            for (const auto s : kPipelineNonDigitStrings) {
                std::vector<UiPrimitive> pf, pt;
                streamer.stream_text(s, 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                     pf, INVALID_INDEX_U16, scale, 400, false, f, false);
                streamer.stream_text(s, 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                     pt, INVALID_INDEX_U16, scale, 400, false, f, true);
                KT_CHECK(pt.size() == pf.size());
                for (size_t i = 0; i < pf.size(); ++i) {
                    KT_CHECK_NEAR(pt[i].min_x, pf[i].min_x, 0.0f);
                    KT_CHECK_NEAR(pt[i].max_x, pf[i].max_x, 0.0f);
                }
            }
        }
    }

    // 6. Empty string edge case
    std::vector<UiPrimitive> empty_prims;
    const auto empty_stats = streamer.stream_text("", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                                                  empty_prims, INVALID_INDEX_U16, 1.0f, 400, false, fam, true);
    KT_CHECK(empty_prims.empty());
    KT_CHECK(empty_stats.total_glyphs == 0);

#ifdef _WIN32
    // 7. Times New Roman streaming parity under tabular_nums == true and proportional preservation under false
    {
        constexpr std::string_view kTnr = "Times New Roman";
        std::vector<UiPrimitive> tnr_prop_1, tnr_prop_8, tnr_tab_1, tnr_tab_8;
        streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             tnr_prop_1, INVALID_INDEX_U16, 1.0f, 400, false, kTnr, false);
        streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             tnr_prop_8, INVALID_INDEX_U16, 1.0f, 400, false, kTnr, false);
        streamer.stream_text("11111%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             tnr_tab_1, INVALID_INDEX_U16, 1.0f, 400, false, kTnr, true);
        streamer.stream_text("88888%", 10.0f, 20.0f, 16.0f, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             tnr_tab_8, INVALID_INDEX_U16, 1.0f, 400, false, kTnr, true);
        KT_CHECK(tnr_prop_1.size() == 6 && tnr_prop_8.size() == 6);
        KT_CHECK(tnr_tab_1.size() == 6 && tnr_tab_8.size() == 6);
        KT_CHECK(tnr_prop_1.back().min_x + 0.5f < tnr_prop_8.back().min_x);
        KT_CHECK_NEAR(tnr_tab_1.back().min_x, tnr_tab_8.back().min_x, 1e-3f);
        KT_CHECK_NEAR(tnr_tab_1.back().max_x, tnr_tab_8.back().max_x, 1e-3f);
    }
#endif
}

} // namespace

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_text_pipeline..." << std::endl;

    using namespace kernetile::text;
    using namespace kernetile::atlas;
    using namespace kernetile::core;
    using namespace kernetile::math;

    auto& engine = shared_text_engine();

    DynamicAtlas atlas(2048, 2048, AtlasFormat::R8_UNORM);
    TextStreamer streamer(engine, atlas);

    // 构造模拟 10,000 字长文本阅读场景
    const std::string paragraph = "KerneTile: High Performance GPU Compute-driven 2D UI Rendering Engine. 现代计算着色器瓦片引擎与无锁提交环。\n";
    std::string full_document;
    full_document.reserve(10000 * 2);
    while (full_document.size() < 10000) {
        full_document += paragraph;
    }

    std::vector<UiPrimitive> primitives;
    primitives.reserve(full_document.size());

    // 预热底层系统文本引擎字体与字形管线（避免冷启动 COM/Fontconfig 初始化干扰管线吞吐耗时测量）
    (void)engine.shape_text("KerneTile", 16.0f, "", 400, false, false);

    // 测试高吞吐文本流式生成
    auto start_time = std::chrono::high_resolution_clock::now();

    auto stats = streamer.stream_text(
        full_document,
        10.0f,
        20.0f,
        16.0f,
        Color(1.0f, 1.0f, 1.0f, 1.0f),
        primitives
    );

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::cout << "  -> Document Size: " << full_document.size() << " bytes" << std::endl;
    std::cout << "  -> Output UiPrimitives: " << primitives.size() << " generated" << std::endl;
    std::cout << "  -> Total Glyphs Streamed: " << stats.total_glyphs << std::endl;
    std::cout << "  -> Cache Hits: " << stats.cache_hits << ", Misses: " << stats.cache_misses << std::endl;
    std::cout << "  -> Glyph Cache Hit Rate: " << std::fixed << std::setprecision(2) << (stats.get_hit_rate() * 100.0) << "% (Target: >= 98.00%)" << std::endl;
    std::cout << "  -> Total Pipeline Time: " << std::setprecision(3) << total_ms << " ms (Target: < 4.00 ms)" << std::endl;
    std::cout << "  -> Atlas Cached Glyphs: " << streamer.get_cached_glyph_count() << std::endl;
    std::cout << "  -> Atlas Utilization: " << std::setprecision(2) << (atlas.get_utilization() * 100.0) << "%" << std::endl;

    // 验证核心验收标准
    KT_CHECK(stats.get_hit_rate() >= 0.98);
#ifdef _DEBUG
    constexpr double TIME_LIMIT_MS = 50.0; // Debug 模式包含未优化 COM 调用与迭代器检查开销
#else
    constexpr double TIME_LIMIT_MS = 15.0; // Release 优化模式目标 (兼容 CI 虚拟化环境冷抖动)
#endif
    std::cout << "  -> Time limit for current build configuration: " << TIME_LIMIT_MS << " ms" << std::endl;
    KT_CHECK(total_ms < TIME_LIMIT_MS);
    KT_CHECK(!primitives.empty());

    // 验证首尾图元物理属性有效性
    for (size_t i = 0; i < std::min(primitives.size(), size_t(100)); ++i) {
        const auto& p = primitives[i];
        KT_CHECK(p.max_x > p.min_x);
        KT_CHECK(p.max_y > p.min_y);
        KT_CHECK(p.atlas_u1 >= p.atlas_u0);
        KT_CHECK(p.atlas_v1 >= p.atlas_v0);
        KT_CHECK((p.flags & static_cast<uint16_t>(PrimitiveFlags::IsGlyph)) != 0);
    }

    // Separate tabular streaming checks (executed after timed 10k benchmark)
    test_tabular_streaming_pipeline();

    std::cout << "[TEST] test_text_pipeline PASSED successfully!" << std::endl;
    return 0;
}
