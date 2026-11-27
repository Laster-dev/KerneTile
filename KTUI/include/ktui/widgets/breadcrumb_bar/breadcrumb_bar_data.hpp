#pragma once

#include "../menu_flyout/menu_flyout_logic.hpp"
#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {


struct BreadcrumbItem {
    std::string id;
    std::string label;
    std::string svg_path;
    std::vector<std::string> siblings{}; // 可选：同级兄弟节点，点击段右侧 › / ▾ 弹出 MenuFlyout 快速横跳

    bool operator==(const BreadcrumbItem&) const = default;
};

} // namespace ktui
