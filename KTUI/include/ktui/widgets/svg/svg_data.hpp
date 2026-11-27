#pragma once

#include <ktui/binding/binding.hpp>
#include <ktui/widgets/widget.hpp>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// Svg 交互状态模型 (Data Layer)
struct SvgInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
