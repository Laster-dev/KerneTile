#pragma once

#include "../textbox/textbox_logic.hpp"
#include "../widget.hpp"
#include <ktui/core/strings.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// AutoSuggestBox 交互状态模型 (Data Layer)
struct AutoSuggestBoxInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
