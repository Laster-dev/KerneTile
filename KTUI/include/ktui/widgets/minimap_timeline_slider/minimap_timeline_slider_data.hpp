#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// MinimapTimelineSlider 交互状态模型 (Data Layer)
struct MinimapTimelineSliderInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
