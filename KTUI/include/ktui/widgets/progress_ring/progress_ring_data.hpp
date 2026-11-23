#pragma once

namespace ktui {

// 环形加载圈数据层契约 (ProgressRing Data Layer)
// 严格对齐 Windows 11 WinUI 3 官方 ProgressRing 规范 (带圆润端点 Round Line Cap)
struct ProgressRingData {
    float size{32.0f};             // 环形外径 (WinUI 3 标准: 默认 32px，紧凑 16/20px，大号 48/64px)
    float stroke_width{-1.0f};     // 环线粗细 (-1 表示按 WinUI 3 比例自动推导: clamp(size * 0.105f, 2.0f, 8.0f))
    float value{0.0f};             // 确定进度 (0.0f ~ 1.0f)
    bool indeterminate{true};      // 是否为不确定旋转加载动效 (默认 true)
    bool show_track{false};        // 不确定模式下是否显示底槽 (WinUI 3 默认 false，确定模式默认启用)
    bool disabled{false};
};

struct ProgressRingInteractiveState {
    bool is_disabled{false};
};

} // namespace ktui
