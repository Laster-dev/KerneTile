#pragma once

#include <ktui/binding/binding.hpp>
#include <ktui/widgets/widget.hpp>
#include <limits>
#include <memory>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// ScrollView 交互状态模型 (Data Layer)
struct ScrollViewInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
