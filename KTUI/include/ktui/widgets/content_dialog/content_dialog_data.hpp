#pragma once

#include <ktui/core/strings.hpp>
#include <ktui/widgets/widget.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// WinUI 3 ContentDialog 返回结果
enum class ContentDialogResult : uint8_t {
    None      = 0,
    Primary   = 1,
    Secondary = 2,
    Close     = 3
};

// WinUI 3 模态对话框配置描述
struct ContentDialogDesc {
    std::string title;
    std::string content;
    std::string primary_button_text;     // 主强调按钮 (可选)
    std::string secondary_button_text;   // 次操作按钮 (可选)
    std::string close_button_text{FrameworkStrings::current().cancel}; // 关闭按钮
    float width{420.0f};
    bool directional_rim{false};
    bool acrylic_grain{false};
    Fn<void(ContentDialogResult)> on_result{nullptr};
};

} // namespace ktui
