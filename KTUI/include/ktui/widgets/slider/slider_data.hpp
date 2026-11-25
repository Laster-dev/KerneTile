#pragma once

#include <string>
#include <ktui/core/fn.hpp>

namespace ktui {

enum class SliderOrientation : uint8_t {
    Horizontal = 0,
    Vertical   = 1,
};

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

    Fn<void(float)> on_change{nullptr};
    Fn<void(float)> on_change_end{nullptr};

    // WinUI 3 扩展: 竖向模式 / 独立刻度频率 / 双滑块区间模式
    SliderOrientation orientation{SliderOrientation::Horizontal};
    float tick_frequency{0.0f};    // 独立刻度间隔 (0 表示沿用 step 的默认规则)
    bool show_ticks{false};        // 强制显示刻度线
    bool range_mode{false};        // 双滑块区间模式 (RangeSlider)
    float range_start{20.0f};      // 双滑块左/下端值
    float range_end{80.0f};        // 双滑块右/上端值
    Fn<void(float, float)> on_range_changed{nullptr};
};

struct SliderInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_dragging{false};
    bool is_focused{false};
    bool is_disabled{false};
};

} // namespace ktui
