#pragma once

// RadioCard inspired by Uiverse.io (mahiatlinux/neat-donkey-46, MIT License)
// RadioButton follows WinUI 3 (Windows 11 Fluent 2) official specification

#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <string>
#include <vector>

namespace ktui {

class PaintContext;

// 单选按钮 / 单选卡片数据层契约
struct RadioButtonData {
    std::string label{""};
    std::string icon_text{""}; // 单选卡片可选图标字符/符号
    std::function<void(PaintContext&, Rect, Color)> icon_painter{nullptr}; // 单选卡片可选矢量图标绘制器
    int value{0};
    bool checked{false};
    bool disabled{false};
    float tile_width{84.0f};   // 单选卡片宽度
    float tile_height{68.0f};  // 单选卡片高度
    float corner_radius{8.0f}; // 单选卡片圆角
    float font_size{13.0f};
    float gap{8.0f};           // 经典单选按钮圆圈与文字间距

    std::function<void(int)> on_select{nullptr};
};

using RadioCardData = RadioButtonData;

// 单选交互状态
struct RadioButtonInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};

// 单选项描述 (通用用于 RadioGroup 与 RadioCardGroup)
struct RadioOption {
    std::string label;
    int value{0};
    std::string icon_text{""};
    std::function<void(PaintContext&, Rect, Color)> icon_painter{nullptr};
    bool disabled{false};
};

using RadioCardOption = RadioOption;

} // namespace ktui
