#include "kernetile/atlas/shelf_packer.hpp"
#include "test_common.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <random>
#include <cassert>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_shelf_packing_ratio..." << std::endl;

    using namespace kernetile::atlas;

    constexpr uint32_t ATLAS_W = 2048;
    constexpr uint32_t ATLAS_H = 2048;
    ShelfAtlasPacker packer(ATLAS_W, ATLAS_H);

    // 构造典型 UI 排版字形尺寸分布 (14px ~ 48px, 包含 1px 保护边距后的尺寸)
    // 真实场景：16px (18x18), 24px (26x26), 32px (34x34) 占比最高
    std::mt19937 rng(42);
    std::discrete_distribution<uint32_t> size_dist({
        10, // 14px -> 16x16
        50, // 16px -> 18x18 (高频正文字体)
        20, // 20px -> 22x22
        15, // 24px -> 26x26 (标题字)
        4,  // 32px -> 34x34 (大标题)
        1   // 48px -> 50x50 (横幅大字)
    });

    const uint32_t standard_sizes[] = { 16, 18, 22, 26, 34, 50 };

    constexpr size_t TOTAL_ITEMS = 12000;
    std::vector<std::pair<uint32_t, uint32_t>> items;
    items.reserve(TOTAL_ITEMS);

    for (size_t i = 0; i < TOTAL_ITEMS; ++i) {
        uint32_t s = standard_sizes[size_dist(rng)];
        // 允许宽度存在微小字偶间距差异 (宽约为高度的 0.6 ~ 1.0 倍)
        uint32_t w = std::max(8u, static_cast<uint32_t>(s * (0.6f + (rng() % 40) / 100.0f)));
        items.emplace_back(w, s);
    }

    // 执行连续在线插入并测量耗时
    size_t packed_count = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (const auto& [w, h] : items) {
        auto rect = packer.allocate(w, h);
        if (rect.has_value()) {
            packed_count++;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    double avg_alloc_us = total_us / packed_count;

    double utilization = packer.get_utilization();

    std::cout << "  -> Total Packed Glyphs: " << packed_count << std::endl;
    std::cout << "  -> Shelves Created: " << packer.get_shelf_count() << std::endl;
    std::cout << "  -> Guillotine Recycled Blocks: " << packer.get_free_rect_count() << std::endl;
    std::cout << "  -> Allocated Area: " << packer.get_allocated_area() << " / " << packer.get_total_area() << " pixels" << std::endl;
    std::cout << "  -> Packing Utilization: " << std::fixed << std::setprecision(2) << (utilization * 100.0) << "% (Target: >= 82.00%)" << std::endl;
    std::cout << "  -> Average Allocate Latency: " << std::setprecision(3) << avg_alloc_us << " us (Target: < 1.50 us)" << std::endl;

    // 验证核心指标
    assert(utilization >= 0.82 && "Packing efficiency must be at least 82%!");
    assert(avg_alloc_us < 1.5 && "Average allocation latency must be < 1.5 microseconds!");

    std::cout << "[TEST] test_shelf_packing_ratio PASSED successfully!" << std::endl;
    return 0;
}
