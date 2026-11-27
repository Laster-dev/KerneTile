#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


enum class DataGridSortDirection : uint8_t {
    None = 0,
    Ascending = 1,
    Descending = 2
};

enum class DataGridCellOverflow : uint8_t {
    Ellipsis = 0, // 列宽不足时自动以省略号 "..." 截断 (默认)
    Clip     = 1  // 列宽不足时直接硬裁剪
};

struct DataGridColumn {
    std::string title;
    float width{140.0f}; // <= 0 表示自动均分剩余宽度
    bool sortable{true};
    bool numeric{false};
};

} // namespace ktui
