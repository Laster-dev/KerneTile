#include "kernetile/hal/hal.hpp"
#include "kernetile/tile/tile_grid.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include "kernetile/tile/dirty_tracker.hpp"
#include "test_common.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cmath>

static std::vector<uint8_t> read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_gpu_tile_compute..." << std::endl;

    using namespace kernetile::hal;
    using namespace kernetile::tile;
    using namespace kernetile::core;

    // 1. 初始化图形硬件设备
    auto device = create_d3d12_device(false);
    assert(device != nullptr);
    std::cout << "  -> Activated Device: " << device->get_device_name() << std::endl;

    // 2. 加载 16x16 瓦片片上共享内存 (LDS) 计算着色器字节码
    auto bytecode = read_file_bytes("shaders/kerne_tile_compute.dxil");
    assert(!bytecode.empty() && "Failed to load shaders/kerne_tile_compute.dxil");
    std::cout << "  -> Loaded LDS Compute Shader bytecode: " << bytecode.size() << " bytes" << std::endl;

    auto compute_pipeline = device->create_compute_pipeline(bytecode);
    assert(compute_pipeline != nullptr);

    // 3. 构造测试网格 (256x256 像素 = 16x16 瓦片 = 256 瓦片)
    constexpr uint32_t WIDTH = 256;
    constexpr uint32_t HEIGHT = 256;
    TileGrid grid(WIDTH, HEIGHT);
    assert(grid.get_total_tiles() == 256);

    // 4. 创建 256x256 离屏存储纹理与 CPU 回读缓冲
    TextureDesc tex_desc{
        .width = WIDTH,
        .height = HEIGHT,
        .format = TextureFormat::RGBA8_UNORM,
        .allow_storage = true
    };
    auto offscreen_texture = device->create_texture(tex_desc);
    assert(offscreen_texture != nullptr);

    constexpr size_t READBACK_SIZE = WIDTH * HEIGHT * 4;
    BufferDesc readback_desc{
        .size_bytes = READBACK_SIZE,
        .usage = BufferUsage::Readback
    };
    auto readback_buffer = device->create_buffer(readback_desc);
    assert(readback_buffer != nullptr);

    // --- 帧 0: 全屏背景 + 悬浮圆角按钮 (带软阴影) ---
    std::vector<UiPrimitive> primitives;

    // 图元 0: 背景矩形卡片
    UiPrimitive p_bg{};
    p_bg.min_x = 0.0f; p_bg.min_y = 0.0f;
    p_bg.max_x = 256.0f; p_bg.max_y = 256.0f;
    p_bg.color_r = 0.1f; p_bg.color_g = 0.12f; p_bg.color_b = 0.15f; p_bg.color_a = 1.0f; // 深灰蓝背景
    primitives.push_back(p_bg);

    // 图元 1: 悬浮圆角按钮 (40, 40) 至 (200, 100), 半径 12, 鲜亮翡翠绿, 带软阴影
    UiPrimitive p_btn{};
    p_btn.min_x = 40.0f; p_btn.min_y = 40.0f;
    p_btn.max_x = 200.0f; p_btn.max_y = 100.0f;
    p_btn.radius_top_left = 12.0f;
    p_btn.radius_top_right = 12.0f;
    p_btn.radius_bottom_right = 12.0f;
    p_btn.radius_bottom_left = 12.0f;
    p_btn.color_r = 0.0f; p_btn.color_g = 0.8f; p_btn.color_b = 0.4f; p_btn.color_a = 1.0f; // 翡翠绿
    p_btn.shadow_blur = 16.0f;
    p_btn.shadow_spread = 0.0f;
    p_btn.shadow_offset_x = 0.0f;
    p_btn.shadow_offset_y = 8.0f;
    primitives.push_back(p_btn);

    TileBinner binner;
    DirtyTracker tracker;

    auto bin0 = binner.bin_primitives(grid, primitives, true);
    auto dirty0 = tracker.update(grid, bin0.headers, bin0.indices, primitives);
    std::cout << "  -> Frame 0 Binning & Dirty Tracking: dirty_tiles=" << dirty0.dirty_count << " / 256" << std::endl;
    assert(dirty0.dirty_count > 0);

    // 创建并上传 GPU 结构化缓冲
    auto upload_buffer = [&](const void* data, size_t size_bytes, size_t stride) {
        BufferDesc desc{
            .size_bytes = std::max(size_bytes, size_t(256)),
            .usage = BufferUsage::Upload,
            .structure_byte_stride = stride
        };
        auto buf = device->create_buffer(desc);
        if (size_bytes > 0 && data) {
            std::memcpy(buf->map(), data, size_bytes);
            buf->unmap();
        }
        return buf;
    };

    auto prim_buf0 = upload_buffer(primitives.data(), primitives.size() * sizeof(UiPrimitive), sizeof(UiPrimitive));
    auto idx_buf0 = upload_buffer(bin0.indices.data(), bin0.indices.size() * sizeof(uint32_t), sizeof(uint32_t));
    auto header_buf0 = upload_buffer(bin0.headers.data(), bin0.headers.size() * sizeof(TileHeader), sizeof(TileHeader));
    auto dirty_buf0 = upload_buffer(dirty0.dirty_tiles.data(), dirty0.dirty_tiles.size() * sizeof(DirtyTileInfo), sizeof(DirtyTileInfo));

    // 录制计算着色器命令
    {
        auto recorder = device->create_command_recorder();
        recorder->resource_barrier(*offscreen_texture, ResourceState::Common, ResourceState::UnorderedAccess);
        recorder->set_compute_pipeline(*compute_pipeline);
        recorder->set_storage_buffer(0, *prim_buf0);
        recorder->set_storage_buffer(1, *idx_buf0);
        recorder->set_storage_buffer(2, *header_buf0);
        recorder->set_storage_buffer(3, *dirty_buf0);
        recorder->set_storage_texture(0, *offscreen_texture);

        // 协同派发脏瓦片工作组
        recorder->dispatch(dirty0.dirty_count, 1, 1);

        // 拷贝至回读缓冲以供 CPU 像素校验
        recorder->copy_texture_to_buffer(*offscreen_texture, *readback_buffer);
        recorder->close();

        device->submit(std::move(recorder));
        device->wait_idle();
    }

    // 像素级校验 Frame 0
    auto* pixels0 = reinterpret_cast<const uint8_t*>(readback_buffer->map());
    assert(pixels0 != nullptr);

    auto get_pixel0 = [&](uint32_t x, uint32_t y) {
        size_t offset = (y * WIDTH + x) * 4;
        return std::vector<uint8_t>{
            pixels0[offset + 0],
            pixels0[offset + 1],
            pixels0[offset + 2],
            pixels0[offset + 3]
        };
    };

    // 校验点 1: 按钮中心内部 (120, 70) -> 期望为翡翠绿色 (R~0, G~204, B~102, A=255)
    auto p_center = get_pixel0(120, 70);
    std::cout << "  -> Button Center (120, 70): RGBA(" 
              << (int)p_center[0] << ", " << (int)p_center[1] << ", " 
              << (int)p_center[2] << ", " << (int)p_center[3] << ")" << std::endl;
    assert(p_center[1] > 180 && "Button G channel should be ~204");
    assert(p_center[2] > 80 && "Button B channel should be ~102");
    assert(p_center[3] == 255 && "Button Alpha should be 255");

    // 校验点 2: 远端背景 (240, 240) -> 期望为深灰蓝背景
    auto p_bg_sample = get_pixel0(240, 240);
    std::cout << "  -> Remote Background (240, 240): RGBA("
              << (int)p_bg_sample[0] << ", " << (int)p_bg_sample[1] << ", "
              << (int)p_bg_sample[2] << ", " << (int)p_bg_sample[3] << ")" << std::endl;
    assert(p_bg_sample[0] > 0 && p_bg_sample[1] > 0 && p_bg_sample[2] > 0 && p_bg_sample[3] == 255);

    readback_buffer->unmap();
    std::cout << "  -> Frame 0 Pixel Validation: PASS" << std::endl;

    // --- 帧 1: 局部光标渲染 (仅重绘 1 个脏瓦片，其余 255 个瓦片完全零派发) ---
    // 在 (120, 70) 处插入 2x14 像素白色光标
    std::vector<UiPrimitive> primitives_f1 = primitives;
    UiPrimitive p_cursor{};
    p_cursor.min_x = 120.0f; p_cursor.min_y = 70.0f;
    p_cursor.max_x = 122.0f; p_cursor.max_y = 84.0f;
    p_cursor.color_r = 1.0f; p_cursor.color_g = 1.0f; p_cursor.color_b = 1.0f; p_cursor.color_a = 1.0f;
    primitives_f1.push_back(p_cursor);

    auto bin1 = binner.bin_primitives(grid, primitives_f1, true);
    auto dirty1 = tracker.update(grid, bin1.headers, bin1.indices, primitives_f1);
    std::cout << "  -> Frame 1 (Cursor Insertion): dirty_tiles=" << dirty1.dirty_count 
              << ", culling_ratio=" << (dirty1.culling_ratio * 100.0) << "%" << std::endl;

    // 验证局部重绘脏瓦片剔除率
    assert(dirty1.dirty_count <= 2 && "Cursor must only invalidate 1~2 tiles!");
    assert(dirty1.culling_ratio >= 0.99 && "Dirty tile culling must exceed 99%!");

    auto prim_buf1 = upload_buffer(primitives_f1.data(), primitives_f1.size() * sizeof(UiPrimitive), sizeof(UiPrimitive));
    auto idx_buf1 = upload_buffer(bin1.indices.data(), bin1.indices.size() * sizeof(uint32_t), sizeof(uint32_t));
    auto header_buf1 = upload_buffer(bin1.headers.data(), bin1.headers.size() * sizeof(TileHeader), sizeof(TileHeader));
    auto dirty_buf1 = upload_buffer(dirty1.dirty_tiles.data(), dirty1.dirty_tiles.size() * sizeof(DirtyTileInfo), sizeof(DirtyTileInfo));

    {
        auto recorder = device->create_command_recorder();
        recorder->resource_barrier(*offscreen_texture, ResourceState::Common, ResourceState::UnorderedAccess);
        recorder->set_compute_pipeline(*compute_pipeline);
        recorder->set_storage_buffer(0, *prim_buf1);
        recorder->set_storage_buffer(1, *idx_buf1);
        recorder->set_storage_buffer(2, *header_buf1);
        recorder->set_storage_buffer(3, *dirty_buf1);
        recorder->set_storage_texture(0, *offscreen_texture);

        // 仅对变动的 1 个瓦片派发计算着色器工作组!
        recorder->dispatch(dirty1.dirty_count, 1, 1);

        recorder->copy_texture_to_buffer(*offscreen_texture, *readback_buffer);
        recorder->close();

        device->submit(std::move(recorder));
        device->wait_idle();
    }

    auto* pixels1 = reinterpret_cast<const uint8_t*>(readback_buffer->map());
    assert(pixels1 != nullptr);

    auto get_pixel1 = [&](uint32_t x, uint32_t y) {
        size_t offset = (y * WIDTH + x) * 4;
        return std::vector<uint8_t>{
            pixels1[offset + 0],
            pixels1[offset + 1],
            pixels1[offset + 2],
            pixels1[offset + 3]
        };
    };

    // 校验点 3: 光标像素 (121, 75) 应当更新为白色 (255, 255, 255, 255)
    auto p_cur = get_pixel1(121, 75);
    std::cout << "  -> Cursor Pixel (121, 75): RGBA(" 
              << (int)p_cur[0] << ", " << (int)p_cur[1] << ", " 
              << (int)p_cur[2] << ", " << (int)p_cur[3] << ")" << std::endl;
    assert(p_cur[0] > 240 && p_cur[1] > 240 && p_cur[2] > 240 && p_cur[3] == 255);

    // 校验点 4: 未派发更新的远端背景瓦片 (240, 240) 内容完好保留
    auto p_bg_retained = get_pixel1(240, 240);
    assert(p_bg_retained == p_bg_sample && "Untouched tiles must be perfectly preserved!");
    std::cout << "  -> Untouched Tile Preservation (240, 240): PASS" << std::endl;

    readback_buffer->unmap();

    std::cout << "[TEST] test_gpu_tile_compute PASSED successfully!" << std::endl;
    return 0;
}
