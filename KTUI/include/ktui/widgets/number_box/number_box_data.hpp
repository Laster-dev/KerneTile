#pragma once

#include "../textbox/textbox_logic.hpp"
#include "../widget.hpp"
#include <ktui/core/strings.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace ktui {


// WinUI 3 NumberBoxSpinButtonPlacementMode
enum class NumberBoxSpinButtonPlacementMode : uint8_t {
    Inline = 0,   // 默认：右侧内嵌上下步进微调按钮
    Compact = 1,  // 悬停或聚焦时显示步进按钮
    Hidden = 2    // 隐藏步进按钮，仅纯数字输入
};

} // namespace ktui
