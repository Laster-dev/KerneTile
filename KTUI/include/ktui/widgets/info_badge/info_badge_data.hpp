#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


enum class InfoBadgeSeverity : uint8_t {
    Attention = 0,
    Informational = 1,
    Success = 2,
    Caution = 3,
    Critical = 4
};

} // namespace ktui
