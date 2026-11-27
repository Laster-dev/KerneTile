#pragma once

#include "../textbox/textbox_logic.hpp"
#include "../menu_flyout/menu_flyout_logic.hpp"
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 结构化查询字段定义 (如 field="status", options={"open","in_progress","closed"})
struct QueryFilterField {
    std::string field;
    std::string label;
    std::vector<std::string> options;
    Color pill_color{0.0f, 0.0f, 0.0f, 0.0f}; // a==0 时自动使用主题强调色
};

// 已选中的结构化查询胶囊 (Pill)
struct QueryFilterToken {
    std::string field;
    std::string value;
    bool operator==(const QueryFilterToken&) const noexcept = default;
};

} // namespace ktui
