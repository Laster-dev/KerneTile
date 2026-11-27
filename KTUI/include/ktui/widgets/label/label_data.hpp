#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <memory>
#include <string>

namespace ktui {


// WinUI 3 / Fluent 2 语义标签变体
enum class LabelVariant : uint8_t {
    Body = 0,     // 正文主色 (13.5px)
    Caption = 1,  // 次级说明色 (12px)
    Subtitle = 2, // 小标题 (16px)
    Title = 3,    // 大标题 (20px)
    Accent = 4,   // 主题强调色
    Success = 5,  // 成功绿
    Warning = 6,  // 警告琥珀
    Error = 7     // 错误红
};

} // namespace ktui
