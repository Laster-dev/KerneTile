#pragma once

#include "../checkbox/checkbox_logic.hpp"
#include "../number_box/number_box_logic.hpp"
#include "../textbox/textbox_logic.hpp"
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 紧凑型属性检查器条目 (工程参数表行数据)
struct InspectorRow {
    std::string key;
    std::string value;
    std::string default_value;
    std::string type_tag{"str"}; // "int" / "bool" / "hex" / "str" / "float"
    bool read_only{false};

    [[nodiscard]] bool is_modified() const noexcept {
        return !default_value.empty() && value != default_value;
    }
};

} // namespace ktui
