#pragma once

#include <ktui/pal/platform_types.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>

namespace ktui::touch {

// Windows 11 Fluent 2 / WCAG 2.2 触控交互标准度量常量
inline constexpr float kMinTargetSize      = 44.0f;  // 最小触控命中热区 (logical px)
inline constexpr float kCompactTargetSize  = 36.0f;  // 紧凑工具栏/关闭按钮最小触控热区
inline constexpr float kSplitterTargetSize = 28.0f;  // SplitView / DockPanel / DataGrid 分隔条触控抓取宽
inline constexpr float kTouchSlopPx        = 10.0f;  // 触控拖动启动阈值 (防手抖误判)
inline constexpr float kAxisLockRatio      = 1.20f;  // 水平/垂直手势主轴锁定比
inline constexpr float kLongPressSec       = 0.50f;  // 触控长按触发时长 (秒)
inline constexpr float kSwipeVelocityPxSec = 320.0f; // 快速轻扫翻页/关闭速度阈值 (px/s)
inline constexpr float kTouchCancelPadPx   = 16.0f;  // 手指滑出按钮边界的额外容差

// 仅在触控输入下将过小的命中矩形向外隐式扩张至 min_size，保持桌面视觉尺寸不变
[[nodiscard]] inline Rect expand_hit_rect(Rect r, PointerDeviceKind kind, float min_size = kMinTargetSize) noexcept {
    if (kind != PointerDeviceKind::Touch) return r;
    const float dx = std::max(0.0f, (min_size - r.width()) * 0.5f);
    const float dy = std::max(0.0f, (min_size - r.height()) * 0.5f);
    return Rect{r.min_x - dx, r.min_y - dy, r.max_x + dx, r.max_y + dy};
}

[[nodiscard]] inline Rect expand_hit_rect(Rect r, float min_size = kMinTargetSize) noexcept {
    const float dx = std::max(0.0f, (min_size - r.width()) * 0.5f);
    const float dy = std::max(0.0f, (min_size - r.height()) * 0.5f);
    return Rect{r.min_x - dx, r.min_y - dy, r.max_x + dx, r.max_y + dy};
}

[[nodiscard]] inline Rect expand_hit_rect_for_touch(Rect r, float min_size = kMinTargetSize) noexcept {
    const float dx = std::max(0.0f, (min_size - r.width()) * 0.5f);
    const float dy = std::max(0.0f, (min_size - r.height()) * 0.5f);
    return Rect{r.min_x - dx, r.min_y - dy, r.max_x + dx, r.max_y + dy};
}

[[nodiscard]] inline bool is_horizontal_drag(float dx, float dy, float slop = kTouchSlopPx) noexcept {
    return std::abs(dx) > slop && std::abs(dx) > std::abs(dy) * kAxisLockRatio;
}

[[nodiscard]] inline bool is_vertical_drag(float dx, float dy, float slop = kTouchSlopPx) noexcept {
    return std::abs(dy) > slop && std::abs(dy) > std::abs(dx) * kAxisLockRatio;
}

// 全局最近输入设备追踪器 (无锁原子，供控件区分桌面鼠标态与触控态无悬停表现)
class InputModeTracker {
public:
    static void notify(PointerDeviceKind kind) noexcept {
        storage().store(static_cast<uint8_t>(kind), std::memory_order_relaxed);
    }

    [[nodiscard]] static PointerDeviceKind current() noexcept {
        return static_cast<PointerDeviceKind>(storage().load(std::memory_order_relaxed));
    }

    [[nodiscard]] static bool is_touch_active() noexcept {
        return current() == PointerDeviceKind::Touch;
    }

private:
    static std::atomic<uint8_t>& storage() noexcept {
        static std::atomic<uint8_t> s_kind{static_cast<uint8_t>(PointerDeviceKind::Mouse)};
        return s_kind;
    }
};

} // namespace ktui::touch
