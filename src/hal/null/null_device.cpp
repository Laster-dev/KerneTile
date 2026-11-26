#include "null_device.hpp"

namespace kernetile::hal {

namespace null {
std::unique_ptr<GraphicsDevice> create_null_device() {
    return std::make_unique<NullDevice>();
}
} // namespace null

std::unique_ptr<GraphicsDevice> create_default_device(bool prefer_warp) {
#if defined(_WIN32) && !defined(KERNETILE_PURE_GDI)
    if (auto dev = create_d3d12_device(prefer_warp)) {
        return dev;
    }
    return null::create_null_device();
#else
    (void)prefer_warp;
    return null::create_null_device();
#endif
}

} // namespace kernetile::hal
