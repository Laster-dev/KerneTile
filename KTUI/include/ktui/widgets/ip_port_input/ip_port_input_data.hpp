#pragma once

#include "../textbox/textbox_logic.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// IpPortInput 交互状态模型 (Data Layer)
struct IpPortInputInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
