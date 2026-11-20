#include "test_common.hpp"
#include "kernetile/hal/hal.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace kernetile::hal;

void test_warp_adapter_creation() {
    std::cout << "[Test 1] Testing D3D12 WARP Software Adapter Creation..." << std::endl;

    // 显式传入 prefer_warp = true，强制使用微软 WARP 纯 CPU 驱动
    auto warp_device = create_d3d12_device(true);
    assert(warp_device != nullptr);

    std::string device_name = std::string(warp_device->get_device_name());
    std::cout << "  WARP Device Name: " << device_name << std::endl;

    // 验证设备名称中包含 "Microsoft Basic Render Driver" 或 "WARP" 或 "Software"
    assert(!device_name.empty());
    assert(device_name.find("Microsoft") != std::string::npos ||
           device_name.find("Basic Render") != std::string::npos ||
           device_name.find("WARP") != std::string::npos ||
           device_name.find("Software") != std::string::npos);

    // 验证在 WARP 适配器上成功分配资源
    BufferDesc buf_desc{
        .size_bytes = 65536,
        .usage = BufferUsage::Upload
    };

    auto buffer = warp_device->create_buffer(buf_desc);
    assert(buffer != nullptr);
    void* ptr = buffer->map();
    assert(ptr != nullptr);
    *reinterpret_cast<uint32_t*>(ptr) = 0xCAFEBABE;
    buffer->unmap();

    // 验证在 WARP 适配器上分配存储纹理
    TextureDesc tex_desc{
        .width = 1920,
        .height = 1080,
        .format = TextureFormat::RGBA8_UNORM,
        .allow_storage = true
    };

    auto texture = warp_device->create_texture(tex_desc);
    assert(texture != nullptr);
    assert(texture->get_width() == 1920);
    assert(texture->get_height() == 1080);

    std::cout << "  -> D3D12 WARP software adapter PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 9 D3D12 WARP Adapter Unit Tests ===" << std::endl;

    test_warp_adapter_creation();

    std::cout << "=== All WARP Adapter Tests PASSED (100%) ===" << std::endl;
    return 0;
}
