#include <ktui/pal/platform_backend.hpp>
#include "headless_backend.hpp"
#if defined(_WIN32)
#include "win32_backend.hpp"
#else
#include "linux_backend.hpp"
#endif

namespace ktui {

std::unique_ptr<IPlatformBackend> create_platform_backend(bool force_headless) {
    if (force_headless) {
        return std::make_unique<HeadlessBackend>();
    }
#if defined(_WIN32)
    return std::make_unique<Win32Backend>();
#else
    return std::make_unique<LinuxBackend>();
#endif
}

} // namespace ktui
