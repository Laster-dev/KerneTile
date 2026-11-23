#pragma once

// From Uiverse.io by mahiatlinux - https://uiverse.io/mahiatlinux/neat-donkey-46
// MIT License, Copyright (c) Yaya12085 (Yaya Mohamed), mahiatlinux (Maheswar Kumar)

#include <ktui/pal/platform_types.hpp>
#include <functional>
#include <string>
#include <vector>

namespace ktui {

class PaintContext;

// 单选卡片按钮纯数据契约 (RadioButton Data Layer)
struct RadioButtonData {
    std::string label{""};
    std::string icon_text{""}; // 可选图标字符/符号
    std::function<void(PaintContext&, Rect, Color)> icon_painter{nullptr}; // 可选自定义矢量图标绘制器
    int value{0};
    bool checked{false};
    bool disabled{false};
    float tile_width{80.0f};
    float tile_height{72.0f};
    float corner_radius{8.0f};
    float font_size{13.0f};

    std::function<void(int)> on_select{nullptr};
};

// 单选按钮交互状态
struct RadioButtonInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};

// RadioGroup 单选项描述
struct RadioOption {
    std::string label;
    int value{0};
    std::string icon_text{""};
    std::function<void(PaintContext&, Rect, Color)> icon_painter{nullptr};
    bool disabled{false};
};

} // namespace ktui
