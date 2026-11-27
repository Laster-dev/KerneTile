#pragma once

#include "../menu_flyout/menu_flyout_logic.hpp"
#include "../widget.hpp"
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// WinUI 3 MenuBarItem 顶层菜单栏项 (支持静态 items 或动态 items_builder)
struct MenuBarItem {
    std::string title;
    std::vector<MenuFlyoutItem> items{};
    Fn<std::vector<MenuFlyoutItem>()> items_builder{nullptr};
    bool disabled{false};
};

} // namespace ktui
