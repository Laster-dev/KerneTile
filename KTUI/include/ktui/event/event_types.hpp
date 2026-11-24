#pragma once

#include <ktui/pal/platform_types.hpp>
#include <string>
#include <cstdint>

namespace ktui {

class RenderBox;

// UI 统一事件基类 (支持冒泡与捕获阻断)
class UIEvent {
public:
    virtual ~UIEvent() = default;

    // 终止事件继续沿树向上冒泡或向下捕获
    void stop_propagation() noexcept {
        m_propagation_stopped = true;
    }

    [[nodiscard]] bool is_propagation_stopped() const noexcept {
        return m_propagation_stopped;
    }

    // 阻止浏览器或平台默认行为
    void prevent_default() noexcept {
        m_default_prevented = true;
    }

    [[nodiscard]] bool is_default_prevented() const noexcept {
        return m_default_prevented;
    }

private:
    bool m_propagation_stopped{false};
    bool m_default_prevented{false};
};

// 指针事件 (鼠标、触控、手写笔)
class PointerEvent : public UIEvent {
public:
    PlatformEventType type = PlatformEventType::PointerMove;
    PointF position{0.0f, 0.0f};          // 根视口绝对物理坐标
    PointF local_position{0.0f, 0.0f};    // 针对当前目标控件自身的局部坐标
    Vec2 delta{0.0f, 0.0f};               // 滚轮或位移增量
    PointerButton button = PointerButton::None;
    PointerDeviceKind device_kind = PointerDeviceKind::Mouse;
    uint32_t pointer_id{0};
    RenderBox* gesture_capture_target{nullptr}; // 祖先手势识别器 (ScrollView / NavigationView) 声明独占捕获
    KeyModifiers modifiers;
    uint64_t timestamp_ms{0};

    PointerEvent() = default;
    PointerEvent(PlatformEventType t, PointF pos, PointerButton btn = PointerButton::None,
                 KeyModifiers mods = KeyModifiers{}, Vec2 d = Vec2{})
        : type(t), position(pos), local_position(pos), delta(d), button(btn), modifiers(mods) {}
};

// 键盘事件
class KeyEvent : public UIEvent {
public:
    PlatformEventType type = PlatformEventType::KeyDown;
    uint32_t key_code{0};                 // 物理扫描码
    std::string text;                     // 提交字符或拼音
    KeyModifiers modifiers;
    uint64_t timestamp_ms{0};

    KeyEvent() = default;
    KeyEvent(PlatformEventType t, uint32_t code, std::string txt = "", KeyModifiers mods = KeyModifiers{})
        : type(t), key_code(code), text(std::move(txt)), modifiers(mods) {}
};

// 焦点变化事件
class FocusEvent : public UIEvent {
public:
    bool gained_focus{false};             // true: 获得焦点; false: 失去焦点

    explicit FocusEvent(bool gained) noexcept : gained_focus(gained) {}
};

} // namespace ktui
