#pragma once

#include "../textbox/textbox_logic.hpp"
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 过滤表达式语法状态
enum class SyntaxFilterState : uint8_t {
    Empty = 0,
    Valid = 1,   // 合法：浅绿底色 + 绿灯
    Warning = 2, // 警告（如未知字段名）：浅黄底色 + 黄灯
    Error = 3    // 语法错误（括号不匹配/缺少操作符或右值）：浅红底色 + 红灯
};

} // namespace ktui
