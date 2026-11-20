#include "kernetile/text/font_engine.hpp"
#include "kernetile/atlas/dynamic_atlas.hpp"
#include "kernetile/text/text_streamer.hpp"
#include "test_common.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <string>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_text_pipeline..." << std::endl;

    using namespace kernetile::text;
    using namespace kernetile::atlas;
    using namespace kernetile::core;
    using namespace kernetile::math;

    auto engine = create_text_engine();
    assert(engine != nullptr);

    DynamicAtlas atlas(2048, 2048, AtlasFormat::R8_UNORM);
    TextStreamer streamer(*engine, atlas);

    // 构造模拟 10,000 字长文本阅读场景
    const std::string paragraph = "KerneTile: High Performance GPU Compute-driven 2D UI Rendering Engine. 现代计算着色器瓦片引擎与无锁提交环。\n";
    std::string full_document;
    full_document.reserve(10000 * 2);
    while (full_document.size() < 10000) {
        full_document += paragraph;
    }

    std::vector<UiPrimitive> primitives;
    primitives.reserve(full_document.size());

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
    assert(stats.get_hit_rate() >= 0.98 && "Glyph cache hit rate must exceed 98%!");
#ifdef _DEBUG
    constexpr double TIME_LIMIT_MS = 25.0; // Debug 模式包含未优化 COM 调用与迭代器检查开销
#else
    constexpr double TIME_LIMIT_MS = 4.0;  // Release 优化模式目标
#endif
    std::cout << "  -> Time limit for current build configuration: " << TIME_LIMIT_MS << " ms" << std::endl;
    assert(total_ms < TIME_LIMIT_MS && "10,000 character pipeline streaming must complete within time limit!");
    assert(!primitives.empty());

    // 验证首尾图元物理属性有效性
    for (size_t i = 0; i < std::min(primitives.size(), size_t(100)); ++i) {
        const auto& p = primitives[i];
        assert(p.max_x > p.min_x && "Valid width required");
        assert(p.max_y > p.min_y && "Valid height required");
        assert(p.atlas_u1 >= p.atlas_u0 && "Valid UV mapping required");
        assert(p.atlas_v1 >= p.atlas_v0);
        assert((p.flags & static_cast<uint16_t>(PrimitiveFlags::IsGlyph)) != 0);
    }

    std::cout << "[TEST] test_text_pipeline PASSED successfully!" << std::endl;
    return 0;
}
