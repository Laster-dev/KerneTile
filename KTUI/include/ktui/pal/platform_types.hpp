#pragma once

#include <kernetile/math/vec.hpp>
#include <kernetile/math/rect.hpp>
#include <kernetile/math/color.hpp>
#include <string>
#include <cstdint>

namespace ktui {

using Vec2 = kernetile::math::Vec2;
using Rect = kernetile::math::Rect;
using Color = kernetile::math::Color;
using PointF = kernetile::math::Vec2;
using RectF = kernetile::math::Rect;

// 边缘内边距/外边距 (Top, Right, Bottom, Left)
struct EdgeInsets {
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};
    float left{0.0f};

    constexpr EdgeInsets() noexcept = default;
    constexpr EdgeInsets(float all) noexcept : top(all), right(all), bottom(all), left(all) {}
    constexpr EdgeInsets(float vertical, float horizontal) noexcept
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    constexpr EdgeInsets(float in_top, float in_right, float in_bottom, float in_left) noexcept
        : top(in_top), right(in_right), bottom(in_bottom), left(in_left) {}

    bool operator==(const EdgeInsets&) const noexcept = default;
};

// 窗口类型
enum class WindowType {
    Normal,         // 标准应用窗口
    ToolWindow,     // 工具箱子窗口
    ModalDialog,    // 模态对话框
    Popup           // 无边框置顶浮窗 (右键菜单/下拉列表)
};

// 窗口呈现状态
enum class WindowState {
    Normal,
    Minimized,
    Maximized,
    Fullscreen
};

// 鼠标光标形状
enum class CursorShape {
    Arrow,
    IBeam,
    PointingHand,
    ResizeEW,
    ResizeNS,
    ResizeNWSE,
    ResizeNESW,
    Crosshair,
    Forbidden
};

// 自定义沉浸式标题栏命中测试分类
enum class TitlebarHitTest {
    Client,          // 客户区普通内容
    Caption,         // 可拖拽移动窗口的标题栏
    MinimizeBtn,     // 最小化按钮
    MaximizeBtn,     // 最大化/还原按钮
    CloseBtn,        // 关闭按钮
    BorderLeft,      // 边缘拉伸区域
    BorderRight,
    BorderTop,
    BorderBottom,
    BorderTopLeft,
    BorderTopRight,
    BorderBottomLeft,
    BorderBottomRight
};

// 窗口创建描述符
struct WindowDesc {
    std::string title = "KTUI Window";
    Vec2 size = {800.0f, 600.0f};
    Vec2 min_size = {100.0f, 100.0f};
    Vec2 max_size = {16384.0f, 16384.0f};
    Vec2 position = {-1.0f, -1.0f}; // -1 表示居中或系统默认
    bool resizable = true;
    bool decorated = true;          // 是否显示系统原生标题栏与边框
    bool transparent = false;        // 是否开启透明通道
    bool always_on_top = false;
    WindowType type = WindowType::Normal;
    void* parent_handle = nullptr;
};

// 键盘修饰键状态
struct KeyModifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool meta = false; // Windows 徽标键 / macOS Command 键
};

// 指针按键
enum class PointerButton {
    None,
    Primary,    // 鼠标左键 / 触控接触
    Secondary,  // 鼠标右键
    Auxiliary   // 鼠标滚轮中键
};

// 跨平台统一输入与系统事件类型
enum class PlatformEventType {
    PointerDown,
    PointerMove,
    PointerUp,
    PointerScroll,
    KeyDown,
    KeyUp,
    ImeComposition,
    ImeCommit,
    WindowResize,
    WindowMoved,
    WindowCloseRequested,
    WindowFocusGained,
    WindowFocusLost,
    DpiChanged
};

// 跨平台统一事件结构体
struct PlatformEvent {
    PlatformEventType type = PlatformEventType::PointerMove;
    Vec2 position = {0.0f, 0.0f};      // 局部或全局坐标
    Vec2 delta = {0.0f, 0.0f};         // 移动位移或滚轮偏移
    PointerButton button = PointerButton::None;
    uint32_t key_code = 0;             // USB HID 扫描码
    std::string text;                  // 提交字符或拼音预编辑文本
    KeyModifiers modifiers;
    float dpi_scale = 1.0f;
    uint64_t timestamp_ms = 0;
};

} // namespace ktui
