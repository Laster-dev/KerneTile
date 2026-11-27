#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// VirtualList 交互状态模型 (Data Layer)
struct VirtualListInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
