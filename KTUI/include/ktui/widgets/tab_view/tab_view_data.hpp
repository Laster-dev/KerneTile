#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {


struct TabViewItem {
    std::string id;
    std::string header;
    std::string svg_path;
    bool is_closable{true};

    bool operator==(const TabViewItem&) const = default;
};

} // namespace ktui
