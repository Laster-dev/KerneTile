#include <ktui/pal/platform_backend.hpp>
#include "headless_backend.hpp"
#if defined(_WIN32)
#include "win32_backend.hpp"
#elif defined(__ANDROID__)
#include "android_backend.hpp"
#endif

namespace ktui {

#if defined(__EMSCRIPTEN__)
std::unique_ptr<IPlatformBackend> create_web_platform_backend();
#elif !defined(_WIN32) && !defined(__ANDROID__)
std::unique_ptr<IPlatformBackend> create_linux_platform_backend();
#endif

std::unique_ptr<IPlatformBackend> create_platform_backend(bool force_headless) {
    if (force_headless) {
        return std::make_unique<HeadlessBackend>();
    }
#if defined(_WIN32)
    return std::make_unique<Win32Backend>();
#elif defined(__EMSCRIPTEN__)
    return create_web_platform_backend();
#elif defined(__ANDROID__)
    return std::make_unique<AndroidBackend>();
#else
    return create_linux_platform_backend();
#endif
}

} // namespace ktui
