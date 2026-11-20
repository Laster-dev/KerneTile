#include "kernetile/atlas/dynamic_atlas.hpp"
#include "test_common.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_atlas_bleeding..." << std::endl;

    using namespace kernetile::atlas;

    // 创建 256x256 RGBA8 动态图集
    DynamicAtlas atlas(256, 256, AtlasFormat::RGBA8_UNORM);

    // 构造纯红色块 A (32x32, RGBA = 255, 0, 0, 255)
    std::vector<uint8_t> red_data(32 * 32 * 4);
    for (size_t i = 0; i < 32 * 32; ++i) {
        red_data[i * 4 + 0] = 255;
        red_data[i * 4 + 1] = 0;
        red_data[i * 4 + 2] = 0;
        red_data[i * 4 + 3] = 255;
    }

    // 构造纯蓝色块 B (32x32, RGBA = 0, 0, 255, 255)
    std::vector<uint8_t> blue_data(32 * 32 * 4);
    for (size_t i = 0; i < 32 * 32; ++i) {
        blue_data[i * 4 + 0] = 0;
        blue_data[i * 4 + 1] = 0;
        blue_data[i * 4 + 2] = 255;
        blue_data[i * 4 + 3] = 255;
    }

    // 依次分配并上传色块 A 与 色块 B (二者在货架内水平紧密相邻)
    auto region_a_opt = atlas.allocate_and_upload(32, 32, red_data.data());
    auto region_b_opt = atlas.allocate_and_upload(32, 32, blue_data.data());

    assert(region_a_opt.has_value() && "Allocation A must succeed");
    assert(region_b_opt.has_value() && "Allocation B must succeed");

    const auto& reg_a = *region_a_opt;
    const auto& reg_b = *region_b_opt;

    // 确认二者相邻：A 的右外包围盒紧贴 B 的左外包围盒
    std::cout << "  -> Region A (Red): content=(" << reg_a.content_rect.x << ", " << reg_a.content_rect.y << ", 32, 32)" << std::endl;
    std::cout << "  -> Region B (Blue): content=(" << reg_b.content_rect.x << ", " << reg_b.content_rect.y << ", 32, 32)" << std::endl;
    assert(reg_a.total_rect.right() == reg_b.total_rect.x && "Blocks must be placed adjacently in shelf");

    float u0_a = AtlasRegion::from_fixed_uv(reg_a.u0);
    float v0_a = AtlasRegion::from_fixed_uv(reg_a.v0);
    float u1_a = AtlasRegion::from_fixed_uv(reg_a.u1);
    float v1_a = AtlasRegion::from_fixed_uv(reg_a.v1);

    float u0_b = AtlasRegion::from_fixed_uv(reg_b.u0);
    float v0_b = AtlasRegion::from_fixed_uv(reg_b.v0);

    // 1. 采样色块 A 中心点：应为纯红
    auto p_center_a = atlas.sample_bilinear((u0_a + u1_a) * 0.5f, (v0_a + v1_a) * 0.5f);
    std::cout << "  -> Block A Center: R=" << p_center_a[0] << ", G=" << p_center_a[1] << ", B=" << p_center_a[2] << ", A=" << p_center_a[3] << std::endl;
    assert(p_center_a[0] == 1.0f && p_center_a[2] == 0.0f);

    // 2. 关键校验：在色块 A 最右侧极限边界采样 (u1_a - 微量偏移)
    // 双线性核向右扩散半个像素，理应平滑过渡至 1px 透明边距，而绝对不能接触到相邻的蓝色块 B！
    constexpr int SUBPIXEL_STEPS = 100;
    double max_blue_in_red_block = 0.0;

    for (int step = 0; step <= SUBPIXEL_STEPS; ++step) {
        float t = static_cast<float>(step) / static_cast<float>(SUBPIXEL_STEPS);
        float sample_u = u0_a + (u1_a - u0_a) * t;
        float sample_v = (v0_a + v1_a) * 0.5f;

        auto sample = atlas.sample_bilinear(sample_u, sample_v);
        // 任何处于 A 区域内的采样点，其 B 通道值必须为严格为 0
        if (sample[2] > max_blue_in_red_block) {
            max_blue_in_red_block = sample[2];
        }
    }

    std::cout << "  -> Maximum Blue Bleed in Red Block: " << max_blue_in_red_block << std::endl;
    assert(max_blue_in_red_block == 0.0 && "Bleeding detected: Blue channel leaked into Red block!");

    // 3. 同理校验：在色块 B 区域内的任何采样点，R 通道值必须严格为 0
    double max_red_in_blue_block = 0.0;
    for (int step = 0; step <= SUBPIXEL_STEPS; ++step) {
        float t = static_cast<float>(step) / static_cast<float>(SUBPIXEL_STEPS);
        float sample_u = u0_b + (u1_a - u0_a) * t;
        float sample_v = (v0_b + v1_a) * 0.5f;

        auto sample = atlas.sample_bilinear(sample_u, sample_v);
        if (sample[0] > max_red_in_blue_block) {
            max_red_in_blue_block = sample[0];
        }
    }

    std::cout << "  -> Maximum Red Bleed in Blue Block: " << max_red_in_blue_block << std::endl;
    assert(max_red_in_blue_block == 0.0 && "Bleeding detected: Red channel leaked into Blue block!");

    std::cout << "[TEST] test_atlas_bleeding PASSED successfully (Zero bleeding confirmed)!" << std::endl;
    return 0;
}
