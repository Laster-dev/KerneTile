#pragma once

#include "../widget.hpp"
#include <ktui/core/strings.hpp>
#include "../menu_flyout/menu_flyout_logic.hpp"
#include "../split_view/split_view_logic.hpp"
#include <ktui/binding/binding.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 五区停靠方位
enum class DockZone : uint8_t {
    Left = 0,
    Right = 1,
    Top = 2,
    Bottom = 3,
    Center = 4
};

// 单个可拖拽停靠面板描述
struct DockPane {
    std::string id;
    std::string title;
    DockZone zone{DockZone::Center};
    WidgetPtr content{nullptr};
    bool closable{false};
};

} // namespace ktui
