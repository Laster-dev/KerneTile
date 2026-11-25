#include <ktui/pal/platform_backend.hpp>
#include "linux_backend.hpp"

namespace ktui {

std::unique_ptr<IPlatformBackend> create_linux_platform_backend() {
    return std::make_unique<LinuxBackend>();
}

} // namespace ktui
