#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <limits>
#include <memory>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// ScrollLinked 交互状态模型 (Data Layer)
struct ScrollLinkedInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
