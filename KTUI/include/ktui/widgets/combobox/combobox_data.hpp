#pragma once

#include <functional>
#include <string>
#include <vector>

namespace ktui {

// 下拉框选项条目 (ComboBox Option)
struct ComboBoxItem {
    std::string label;
    bool disabled{false};

    ComboBoxItem() = default;
    ComboBoxItem(std::string text, bool is_disabled = false)
        : label(std::move(text)), disabled(is_disabled) {}
    ComboBoxItem(const char* text, bool is_disabled = false)
        : label(text ? text : ""), disabled(is_disabled) {}
};

// 下拉框数据层契约 (ComboBox Data Layer - WinUI 3 / Fluent 2 规范)
struct ComboBoxData {
    std::vector<ComboBoxItem> items;
    int selected_index{-1};            // 当前选中索引 (-1 代表未选中，显示 placeholder)
    std::string placeholder{"请选择..."};
    float width{180.0f};               // 默认控件宽度
    float height{32.0f};               // WinUI 3 ComboBoxMinHeight = 32px
    float item_height{32.0f};          // WinUI 3 ComboBoxItem 高度 = 32px
    float font_size{13.0f};
    int max_visible_items{8};          // 下拉面板最大可见条数 (超出自动滚轮滚动)
    bool disabled{false};

    std::function<void(int, const std::string&)> on_change{nullptr};
};

// 下拉框交互状态层
struct ComboBoxInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_open{false};
    bool is_focused{false};
    bool is_disabled{false};
};

} // namespace ktui
