#pragma once

#include <ktui/widgets/widget.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// WinUI 3 MenuFlyout / ContextMenu 菜单项
// 支持普通项、快捷键、图标、分隔线、Check 勾选、Radio 单选、多级级联子菜单 (sub_items)，
// 以及任意真实控件嵌套：右侧伴随控件 (trailing_widget) 与整行自定义控件树 (custom_widget)
struct MenuFlyoutItem {
    std::string label;
    std::string shortcut;      // 右对齐快捷键提示 (如 "Ctrl+C")
    std::string svg_path;      // 可选 24x24 Fluent SVG Path 图标
    bool is_separator{false};
    bool is_checkable{false};
    bool checked{false};
    bool disabled{false};
    Fn<void()> on_click{nullptr};

    // 扩展能力 (保持原有指定初始化器 100% 向后兼容)
    bool is_radio{false};                    // 单选圆点指示器 (配合 checked 使用)
    bool keep_open_on_click{false};          // 点击后保持菜单打开
    Fn<void(bool)> on_toggle{nullptr};       // 勾选项状态变更回调
    std::vector<MenuFlyoutItem> sub_items{}; // 多级级联子菜单
    WidgetPtr trailing_widget{nullptr};      // 右侧伴随真实控件 (如 ToggleSwitch / NumberBox / InfoBadge 等)
    WidgetPtr custom_widget{nullptr};        // 整行任意嵌套真实控件树 (如 Slider / SegmentedControl / HStack 等)
    float custom_height{0.0f};               // 自定义控件行高 (0 = 自动测量)

    static MenuFlyoutItem separator() {
        MenuFlyoutItem it;
        it.is_separator = true;
        return it;
    }

    static MenuFlyoutItem widget(WidgetPtr w, float height = 0.0f) {
        MenuFlyoutItem it;
        it.custom_widget = std::move(w);
        it.custom_height = height;
        return it;
    }
};

} // namespace ktui
