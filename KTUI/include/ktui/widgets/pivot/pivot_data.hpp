#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// WinUI 3 Pivot 页签条目
struct PivotItem {
    std::string header;
    std::string svg_path;
    std::string id;
    bool disabled{false};
};

} // namespace ktui
