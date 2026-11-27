#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


struct SegmentedItem {
    std::string label;
    std::string svg_path;
};

} // namespace ktui
