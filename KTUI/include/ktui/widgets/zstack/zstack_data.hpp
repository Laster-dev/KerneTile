#pragma once

#include "../widget.hpp"
#include <ktui/layout/flex_types.hpp>
#include <ktui/binding/binding.hpp>
#include <vector>
#include <memory>
#include <optional>
#include <ktui/pal/platform_types.hpp>

namespace ktui {

// Zstack 交互状态模型 (Data Layer)
struct ZstackInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};
} // namespace ktui
