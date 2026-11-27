#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


enum class PersonPresence : uint8_t {
    None = 0,
    Available = 1,
    Busy = 2,
    Away = 3,
    Offline = 4
};

} // namespace ktui
