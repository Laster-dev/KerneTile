#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ktui {

// SettingsCard 交互状态模型 (Data Layer)
struct SettingsCardInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
