#include "kernetile/hal/hal.hpp"
#include "test_common.hpp"
#include <iostream>
#include <cassert>
#include <vector>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_hal_lifecycle..." << std::endl;

    using namespace kernetile::hal;

    // 1. 初始化 D3D12 设备
    std::unique_ptr<GraphicsDevice> device = create_d3d12_device(false); // 优先硬件 GPU
    if (!device) {
        std::cerr << "Hardware device failed, trying WARP..." << std::endl;
        device = create_d3d12_device(true); // 回退至 WARP
    }

    assert(device != nullptr && "Failed to create D3D12 device");
    std::cout << "  -> Active Adapter: " << device->get_device_name() << std::endl;

    // 2. 连续分配与销毁 1,000 个不同用途的显存缓冲
    std::cout << "  -> Testing 1,000 buffer allocation cycles..." << std::endl;
    for (int i = 0; i < 1000; ++i) {
        BufferDesc upload_desc{
            .size_bytes = 1024 * 64, // 64 KiB
            .usage = BufferUsage::Upload
        };
        auto buf = device->create_buffer(upload_desc);
        assert(buf != nullptr);
        void* ptr = buf->map();
        assert(ptr != nullptr);
        *reinterpret_cast<uint32_t*>(ptr) = 0xDEADBEEF;
        buf->unmap();
    }

    // 3. 连续分配与销毁 50 个 4K 存储纹理
    std::cout << "  -> Testing 50 4K storage texture allocation cycles..." << std::endl;
    for (int i = 0; i < 50; ++i) {
        TextureDesc tex_desc{
            .width = 3840,
            .height = 2160,
            .format = TextureFormat::RGBA8_UNORM,
            .allow_storage = true
        };
        auto tex = device->create_texture(tex_desc);
        assert(tex != nullptr);
        assert(tex->get_width() == 3840);
        assert(tex->get_height() == 2160);
    }

    device->wait_idle();
    std::cout << "[TEST] test_hal_lifecycle PASSED successfully!" << std::endl;
    return 0;
}
