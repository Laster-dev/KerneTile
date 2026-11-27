#pragma once

#include "../button/button_logic.hpp"
#include "../menu_flyout/menu_flyout_logic.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/core/strings.hpp>
#include <ktui/widgets/widget.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// DropdownButton 交互状态模型 (Data Layer)
struct DropdownButtonInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
