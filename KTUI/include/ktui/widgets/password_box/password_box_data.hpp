#pragma once

#include "../textbox/textbox_logic.hpp"
#include "../widget.hpp"
#include <ktui/core/strings.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <memory>
#include <string>

namespace ktui {


// WinUI 3 PasswordRevealMode
enum class PasswordRevealMode : uint8_t {
    Peek = 0,    // 默认：输入非空时右侧显示眼睛按钮，按住可临时预览明文，松开恢复掩码
    Hidden = 1,  // 始终隐藏，不显示眼睛按钮
    Visible = 2  // 始终以明文显示
};

} // namespace ktui
