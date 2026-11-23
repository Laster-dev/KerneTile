#pragma once

#include <string>

namespace ktui {

// 进度条数据层契约 (ProgressBar Data Layer)
struct ProgressBarData {
    float value{0.0f};             // 进度比例 (0.0f ~ 1.0f)
    bool indeterminate{false};     // 是否为不确定循环流动加载模式
    float track_height{4.0f};      // 导轨厚度 (基准 4px)
    float width{180.0f};           // 默认组件宽度
    float corner_radius{-1.0f};    // 导轨圆角 (-1 代表默认自动胶囊圆角 height * 0.5)
    bool disabled{false};
};

struct ProgressBarInteractiveState {
    bool is_disabled{false};
};

} // namespace ktui
