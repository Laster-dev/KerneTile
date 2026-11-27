#pragma once

#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {


enum class ToastSeverity : uint8_t {
    Informational = 0,
    Success = 1,
    Warning = 2,
    Error = 3
};

enum class ToastPlacement : uint8_t {
    BottomRight = 0,  // Windows 11 默认右下角通知
    TopRight = 1,
    BottomCenter = 2
};

struct ToastDesc {
    std::string title;
    std::string message;
    ToastSeverity severity{ToastSeverity::Informational};
    ToastPlacement placement{ToastPlacement::BottomRight};
    float duration_sec{3.5f};          // <= 0 表示不自动消失
    float width{340.0f};
    bool directional_rim{false};
    bool acrylic_grain{false};
    std::string action_text{""};
    Fn<void()> on_action{nullptr};
    Fn<void()> on_close{nullptr};
};

} // namespace ktui
