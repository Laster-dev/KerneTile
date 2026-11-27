#pragma once

#include "../widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <optional>
#include <string>

namespace ktui {


// 文本溢出处理策略
enum class TextOverflow : uint8_t {
    Clip,       // 硬裁剪
    Ellipsis,   // 超出宽度自动截断并追加 "..." (UTF-8 安全)
    Wrap        // 超出宽度自动多行折行
};

} // namespace ktui
