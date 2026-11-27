#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// FlipView 交互状态模型 (Data Layer)
struct FlipViewInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
