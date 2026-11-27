#pragma once

#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// SpotlightTour 交互状态模型 (Data Layer)
struct SpotlightTourInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
