#include "null_device.hpp"

namespace kernetile::hal {

namespace null {
std::unique_ptr<GraphicsDevice> create_null_device() {
    return std::make_unique<NullDevice>();
}
} // namespace null

std::unique_ptr<GraphicsDevice> create_default_device(bool prefer_warp) {
#if defined(_WIN32)
    try {
        return create_d3d12_device(prefer_warp);
    } catch (...) {
        return null::create_null_device();
    }
#else
    (void)prefer_warp;
    return null::create_null_device();
#endif
}

} // namespace kernetile::hal
