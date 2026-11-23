#pragma once

#include <cstdint>
#include <string>

namespace ktui {

// WinUI 3 进度条运行状态 (Normal / Paused / Error)
enum class ProgressBarState : uint8_t {
    Normal = 0,
    Paused = 1,
    Error  = 2
};

// 进度条数据层契约 (ProgressBar Data Layer - WinUI 3 / Fluent 2 规范)
struct ProgressBarData {
    float value{0.0f};             // 进度比例 (0.0f ~ 1.0f)
    bool indeterminate{false};     // 是否为不确定循环加载模式
    ProgressBarState state{ProgressBarState::Normal};
    float track_height{3.0f};      // 指示条厚度 (WinUI 3 ProgressBarTrackHeight = 3px)
    float width{180.0f};           // 默认组件宽度
    float corner_radius{-1.0f};    // 导轨圆角 (-1 代表默认自动胶囊圆角 height * 0.5)
    bool disabled{false};
};

struct ProgressBarInteractiveState {
    bool is_disabled{false};
};

} // namespace ktui
