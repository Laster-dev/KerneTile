#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 长按确认按钮外观形态：Fluent 2 标准按钮进度条 / 圆形进度环
enum class HoldConfirmShape : uint8_t {
    PillBar = 0,
    CircularRing = 1
};

} // namespace ktui
