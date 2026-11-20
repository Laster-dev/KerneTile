#include "kernetile/hal/hal.hpp"
#include "kernetile/core/primitives.hpp"
#include "kernetile/core/tile_header.hpp"
#include "test_common.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <fstream>
#include <cstring>

static std::vector<uint8_t> load_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    assert(file.is_open() && "Failed to open shader file");
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_compute_uav_readback..." << std::endl;

    using namespace kernetile::hal;
    using namespace kernetile::core;

    // 1. 初始化 D3D12 设备
    auto device = create_d3d12_device(false);
    assert(device != nullptr);
    std::cout << "  -> Active Device: " << device->get_device_name() << std::endl;

    // 2. 加载在阶段 1 编译出的 DXIL 计算着色器
    auto cs_bytes = load_file("shaders/kerne_test_compute.dxil");
    auto compute_pipeline = device->create_compute_pipeline(cs_bytes);
    assert(compute_pipeline != nullptr);

    // 3. 构造 80 字节 UiPrimitive (在 32.25..96.0 坐标范围内的纯蓝圆角矩形，验证亚像素抗锯齿)
    UiPrimitive test_prim{};
    test_prim.min_x = 32.25f; // 亚像素左边界，使像素 (32, 64) 处于 75% 覆盖率过滤带
    test_prim.min_y = 32.0f;
    test_prim.max_x = 96.0f;
    test_prim.max_y = 96.0f;
    test_prim.radius_top_left = 8.0f;
    test_prim.radius_top_right = 8.0f;
    test_prim.radius_bottom_right = 8.0f;
    test_prim.radius_bottom_left = 8.0f;
    test_prim.color_r = 0.0f;
    test_prim.color_g = 0.0f;
    test_prim.color_b = 1.0f; // 纯蓝
    test_prim.color_a = 1.0f;
    test_prim.border_width = 0.0f;
    test_prim.shadow_blur = 0.0f;

    BufferDesc prim_buf_desc{
        .size_bytes = sizeof(UiPrimitive),
        .usage = BufferUsage::Upload,
        .structure_byte_stride = sizeof(UiPrimitive)
    };
    auto prim_buffer = device->create_buffer(prim_buf_desc);
    std::memcpy(prim_buffer->map(), &test_prim, sizeof(UiPrimitive));
    prim_buffer->unmap();

    // 4. 构造索引缓冲与瓦片头缓冲 (分配 240x135 瓦片空间)
    uint32_t prim_idx = 0;
    BufferDesc idx_buf_desc{
        .size_bytes = sizeof(uint32_t),
        .usage = BufferUsage::Upload,
        .structure_byte_stride = sizeof(uint32_t)
    };
    auto idx_buffer = device->create_buffer(idx_buf_desc);
    std::memcpy(idx_buffer->map(), &prim_idx, sizeof(uint32_t));
    idx_buffer->unmap();

    constexpr uint32_t TOTAL_TILES = 240 * 135;
    std::vector<TileHeader> headers(TOTAL_TILES);
    for (auto& h : headers) {
        h.primitive_offset = 0;
        h.primitive_count = 1; // 全部瓦片指向图元 0 (由着色器内的 SDF 进行边界判断)
        h.flags = 0;
    }
    BufferDesc header_buf_desc{
        .size_bytes = headers.size() * sizeof(TileHeader),
        .usage = BufferUsage::Upload,
        .structure_byte_stride = sizeof(TileHeader)
    };
    auto header_buffer = device->create_buffer(header_buf_desc);
    std::memcpy(header_buffer->map(), headers.data(), headers.size() * sizeof(TileHeader));
    header_buffer->unmap();

    // 5. 创建 256x256 离屏存储纹理 (RGBA8_UNORM UAV)
    TextureDesc tex_desc{
        .width = 256,
        .height = 256,
        .format = TextureFormat::RGBA8_UNORM,
        .allow_storage = true
    };
    auto offscreen_texture = device->create_texture(tex_desc);

    // 6. 创建 Readback 缓冲 (256 * 256 * 4 字节，行步长 1024B 对齐)
    constexpr size_t READBACK_SIZE = 256 * 256 * 4;
    BufferDesc readback_desc{
        .size_bytes = READBACK_SIZE,
        .usage = BufferUsage::Readback
    };
    auto readback_buffer = device->create_buffer(readback_desc);

    // 7. 录制计算着色器派发与回读命令
    auto recorder = device->create_command_recorder();
    recorder->resource_barrier(*offscreen_texture, ResourceState::Common, ResourceState::UnorderedAccess);
    recorder->set_compute_pipeline(*compute_pipeline);
    recorder->set_storage_buffer(0, *prim_buffer);
    recorder->set_storage_buffer(1, *idx_buffer);
    recorder->set_storage_buffer(2, *header_buffer);
    recorder->set_storage_texture(0, *offscreen_texture);

    // 派发 16x16 工作组 (每个工作组 16x16 线程，刚好覆盖 256x256 像素)
    recorder->dispatch(16, 16, 1);

    // 将计算完成的离屏存储纹理拷贝至回读缓冲
    recorder->copy_texture_to_buffer(*offscreen_texture, *readback_buffer);
    recorder->close();

    // 8. 提交并等待执行完毕
    device->submit(std::move(recorder));
    device->wait_idle();

    // 9. CPU 像素级校验
    auto* pixels = reinterpret_cast<const uint8_t*>(readback_buffer->map());
    assert(pixels != nullptr);

    auto get_pixel = [&](uint32_t x, uint32_t y) {
        size_t offset = (y * 256 + x) * 4;
        return std::vector<uint8_t>{
            pixels[offset + 0], // R
            pixels[offset + 1], // G
            pixels[offset + 2], // B
            pixels[offset + 3]  // A
        };
    };

    // 校验 A: (64, 64) 处于圆角矩形中心内部 -> 期望为纯蓝色 (0, 0, 255, 255)
    auto p_center = get_pixel(64, 64);
    std::cout << "  -> Center pixel (64, 64): RGBA(" 
              << (int)p_center[0] << ", " << (int)p_center[1] << ", " 
              << (int)p_center[2] << ", " << (int)p_center[3] << ")" << std::endl;
    assert(p_center[0] == 0);
    assert(p_center[1] == 0);
    assert(p_center[2] >= 250); // 纯蓝
    assert(p_center[3] >= 250); // 不透明

    // 校验 B: (10, 10) 处于圆角矩形外部 -> 期望为完全透明 (0, 0, 0, 0)
    auto p_outside = get_pixel(10, 10);
    std::cout << "  -> Outside pixel (10, 10): RGBA(" 
              << (int)p_outside[0] << ", " << (int)p_outside[1] << ", " 
              << (int)p_outside[2] << ", " << (int)p_outside[3] << ")" << std::endl;
    assert(p_outside[0] == 0 && p_outside[1] == 0 && p_outside[2] == 0 && p_outside[3] == 0);

    // 校验 C: (32, 64) 处于几何边界边缘 -> 期望平滑抗锯齿过渡 (A 介于 0 和 255 之间)
    auto p_edge = get_pixel(32, 64);
    std::cout << "  -> Boundary pixel (32, 64): RGBA(" 
              << (int)p_edge[0] << ", " << (int)p_edge[1] << ", " 
              << (int)p_edge[2] << ", " << (int)p_edge[3] << ")" << std::endl;
    assert(p_edge[3] > 0 && p_edge[3] < 255);

    readback_buffer->unmap();
    std::cout << "[TEST] test_compute_uav_readback PASSED successfully!" << std::endl;
    return 0;
}
