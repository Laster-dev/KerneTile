#pragma once

#include "../widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/layout/box_constraints.hpp>
#include <ktui/layout/flex_types.hpp>
#include <memory>
#include <optional>

namespace ktui {

// Box 交互状态模型 (Data Layer)
struct BoxInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
