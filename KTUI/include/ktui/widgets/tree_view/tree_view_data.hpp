#pragma once

#include "../checkbox/checkbox_logic.hpp"
#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


enum class TreeViewSelectionMode : uint8_t {
    Single = 0,
    Multiple = 1,
    None = 2
};

struct TreeListColumn {
    std::string title;
    float width{0.0f}; // <= 0 表示自动均分剩余宽度
};

struct TreeViewNode {
    std::string id;
    std::string label;
    std::string svg_path;
    bool is_expanded{false};
    bool is_checked{false};
    std::vector<TreeViewNode> children;
    std::vector<std::string> sub_columns; // TreeListView 模式下的附加列数据 (对应第 2..N 列)

    bool operator==(const TreeViewNode&) const = default;
};

} // namespace ktui
