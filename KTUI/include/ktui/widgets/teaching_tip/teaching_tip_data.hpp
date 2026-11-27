#pragma once

#include "../widget.hpp"
#include <ktui/core/strings.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


enum class TeachingTipPlacement : uint8_t {
    Auto = 0,
    Bottom = 1, // 气泡位于目标下方，尾标朝上指向目标
    Top = 2     // 气泡位于目标上方，尾标朝下指向目标
};

struct TeachingTipDesc {
    std::string title;
    std::string subtitle;
    std::string svg_path;
    std::string action_text;
    std::string close_text{FrameworkStrings::current().got_it};
    bool is_light_dismiss_enabled{true};
    TeachingTipPlacement preferred_placement{TeachingTipPlacement::Bottom};
    float width{320.0f};
    bool directional_rim{false};
    bool acrylic_grain{false};
    Fn<void()> on_action{nullptr};
    Fn<void()> on_close{nullptr};
};

} // namespace ktui
