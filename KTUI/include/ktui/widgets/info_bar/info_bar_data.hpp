#pragma once

#include <ktui/binding/binding.hpp>
#include <ktui/widgets/widget.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// WinUI 3 InfoBar 严重级别
enum class InfoBarSeverity : uint8_t {
    Informational = 0,
    Success       = 1,
    Warning       = 2,
    Error         = 3
};

struct InfoBarData {
    std::string title;
    std::string message;
    InfoBarSeverity severity{InfoBarSeverity::Informational};
    bool is_open{true};
    bool is_closable{true};
    std::string action_text;
    Fn<void()> on_action{nullptr};
    Fn<void()> on_close{nullptr};
};

} // namespace ktui
