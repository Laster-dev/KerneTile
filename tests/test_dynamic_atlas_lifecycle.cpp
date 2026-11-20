#include "kernetile/atlas/dynamic_atlas.hpp"
#include "test_common.hpp"
#include <iostream>
#include <cassert>
#include <vector>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_dynamic_atlas_lifecycle..." << std::endl;

    using namespace kernetile::atlas;

    // 1. 验证 2048x2048 R8 单通道字形图集生命周期 (4.0 MiB)
    DynamicAtlas glyph_atlas(2048, 2048, AtlasFormat::R8_UNORM);
    assert(glyph_atlas.get_width() == 2048);
    assert(glyph_atlas.get_height() == 2048);
    assert(glyph_atlas.get_bytes_per_pixel() == 1);
    assert(glyph_atlas.get_format() == AtlasFormat::R8_UNORM);

    // 模拟字形光栅化数据 (16x20 灰度字形)
    std::vector<uint8_t> glyph_data(16 * 20, 200); // 灰度值 200
    auto reg_glyph_opt = glyph_atlas.allocate_and_upload(16, 20, glyph_data.data());
    assert(reg_glyph_opt.has_value());

    const auto& reg_g = *reg_glyph_opt;
    // 总尺寸包含 1px 保护边距: 18x22
    assert(reg_g.total_rect.w == 18 && reg_g.total_rect.h == 22);
    assert(reg_g.content_rect.w == 16 && reg_g.content_rect.h == 20);
    assert(reg_g.content_rect.x == reg_g.total_rect.x + 1);
    assert(reg_g.content_rect.y == reg_g.total_rect.y + 1);

    // 验证 UV 定点化映射
    float norm_u0 = AtlasRegion::from_fixed_uv(reg_g.u0);
    float expected_u0 = static_cast<float>(reg_g.content_rect.x) / 2048.0f;
    assert(std::abs(norm_u0 - expected_u0) < 0.0001f);

    // 验证脏矩形收集
    assert(glyph_atlas.get_dirty_rects().size() == 1);
    assert(glyph_atlas.get_dirty_rects()[0].w == 18 && glyph_atlas.get_dirty_rects()[0].h == 22);
    glyph_atlas.clear_dirty_rects();
    assert(glyph_atlas.get_dirty_rects().empty());
    std::cout << "  -> R8 Glyph Atlas Lifecycle & UV Fixed-point Mapping: PASS" << std::endl;

    // 2. 验证 2048x2048 RGBA8 彩色图标图集生命周期 (16.0 MiB)
    DynamicAtlas icon_atlas(2048, 2048, AtlasFormat::RGBA8_UNORM);
    assert(icon_atlas.get_bytes_per_pixel() == 4);

    // 模拟彩色图标 (32x32 RGBA)
    std::vector<uint8_t> icon_data(32 * 32 * 4, 128);
    auto reg_icon_opt = icon_atlas.allocate_and_upload(32, 32, icon_data.data());
    assert(reg_icon_opt.has_value());
    const auto& reg_i = *reg_icon_opt;
    assert(reg_i.total_rect.w == 34 && reg_i.total_rect.h == 34);
    assert(reg_i.content_rect.w == 32 && reg_i.content_rect.h == 32);
    std::cout << "  -> RGBA8 Icon Atlas Lifecycle: PASS" << std::endl;

    // 3. 验证图集重置 (Reset)
    glyph_atlas.reset();
    assert(glyph_atlas.get_utilization() == 0.0);
    assert(glyph_atlas.get_dirty_rects().empty());

    // 重置后重新分配，应当从 (0, 0) 起始位置再次分配
    auto reg_after_reset = glyph_atlas.allocate_and_upload(16, 16, glyph_data.data());
    assert(reg_after_reset.has_value());
    assert(reg_after_reset->total_rect.x == 0 && reg_after_reset->total_rect.y == 0);
    std::cout << "  -> Atlas Hot Reset & Re-allocation: PASS" << std::endl;

    std::cout << "[TEST] test_dynamic_atlas_lifecycle PASSED successfully!" << std::endl;
    return 0;
}
