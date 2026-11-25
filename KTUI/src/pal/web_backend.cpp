#include <ktui/pal/platform_backend.hpp>
#include "web_backend.hpp"

namespace ktui {

std::unique_ptr<IPlatformBackend> create_web_platform_backend() {
    return std::make_unique<WebBackend>();
}

} // namespace ktui
