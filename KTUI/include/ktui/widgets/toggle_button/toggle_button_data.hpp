#pragma once

#include "../button/button_logic.hpp"
#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// ToggleButton 交互状态模型 (Data Layer)
struct ToggleButtonInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
