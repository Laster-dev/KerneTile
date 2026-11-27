#pragma once

#include "../menu_flyout/menu_flyout_logic.hpp"
#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {


// WinUI 3 CommandBarDefaultLabelPosition
enum class CommandBarDefaultLabelPosition : uint8_t {
    Right = 0,     // 现代紧凑工具栏 (40px 高，图标在左、文字在右)
    Bottom = 1,    // 经典 WinUI 3 工具栏 (56px 高，图标在上、文字在下)
    Collapsed = 2  // 仅图标模式 (40px 高，隐藏文字标签)
};

// CommandBar 命令项描述 (支持普通按钮、ToggleButton 与垂直分隔线)
struct CommandBarItem {
    std::string label;
    std::string svg_path;               // Fluent 24x24 SVG 路径 (d="...")
    std::string shortcut;               // 溢出/二级菜单快捷键提示
    bool is_separator{false};
    bool is_toggle{false};
    bool checked{false};
    bool disabled{false};
    Fn<void()> on_click{nullptr};
    Fn<void(bool)> on_toggle{nullptr};

    static CommandBarItem separator() {
        CommandBarItem it;
        it.is_separator = true;
        return it;
    }
};

} // namespace ktui
