#pragma once

#include <string>
#include <functional>

namespace ktui {

// 滑块数据层契约 (Slider Data Layer - WinUI 3 / Fluent 2 规范)
struct SliderData {
    float value{0.0f};             // 当前值
    float min_value{0.0f};         // 最小值 (默认 0.0)
    float max_value{100.0f};       // 最大值 (默认 100.0)
    float step{0.0f};              // 步进值 (0.0 代表连续调节，>0 为离散步进)
    float track_height{4.0f};      // 导轨厚度 (WinUI 3 SliderTrackHeight = 4px)
    float thumb_radius{10.0f};     // 拖动手柄外圈半径 (WinUI 3 直径 20px)
    float width{180.0f};           // 推荐默认宽度 (180px)
    float height{32.0f};           // 交互热区高度 (WinUI 3 SliderHorizontalHeight = 32px)
    bool disabled{false};

    std::function<void(float)> on_change{nullptr};
    std::function<void(float)> on_change_end{nullptr};
};

struct SliderInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_dragging{false};
    bool is_focused{false};
    bool is_disabled{false};
};

} // namespace ktui
