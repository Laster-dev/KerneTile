#pragma once

#include "../textbox/textbox_logic.hpp"
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// GhostTextBox 交互状态模型 (Data Layer)
struct GhostTextBoxInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
