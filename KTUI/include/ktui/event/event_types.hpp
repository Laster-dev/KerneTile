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
    Vec2 contact_size{0.0f, 0.0f};
    RenderBox* gesture_capture_target{nullptr}; // 祖先手势识别器 (ScrollView / NavigationView) 声明独占捕获
    KeyModifiers modifiers;
    uint64_t timestamp_ms{0};

    [[nodiscard]] bool is_touch() const noexcept { return device_kind == PointerDeviceKind::Touch; }

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

// 拖拽事件阶段
enum class DragEventType : uint8_t {
    DragStart = 0,
    DragEnter,
    DragOver,
    DragLeave,
    Drop,
    DragEnd
};

// 拖拽载荷 (同时支持窗口内控件拖拽与操作系统文件拖放)
struct DragPayload {
    std::string mime_type;              // 例如 "text/plain", "text/uri-list", "application/x-ktui-item"
    std::string data;                   // 自定义载荷数据 (文本/JSON/ID 等)
    std::vector<std::string> files;     // OS 文件拖放路径列表
    RenderBox* source{nullptr};         // 窗口内拖拽源节点 (OS 外部拖放时为 nullptr)

    [[nodiscard]] bool empty() const noexcept {
        return mime_type.empty() && data.empty() && files.empty();
    }
};

// 统一拖拽事件 (支持窗口内控件拖拽与 OS 文件拖放)
class DragEvent : public UIEvent {
public:
    DragEventType type{DragEventType::DragOver};
    PointF position{0.0f, 0.0f};        // 视口绝对坐标
    PointF local_position{0.0f, 0.0f};  // 目标控件局部坐标
    DragPayload payload;
    KeyModifiers modifiers;
    bool accepted{false};               // DragStart/DragOver/Drop 回调置 true 表示接受拖拽/投放

    DragEvent() = default;
    DragEvent(DragEventType t, PointF pos, DragPayload p = {}, KeyModifiers mods = {})
        : type(t), position(pos), local_position(pos), payload(std::move(p)), modifiers(mods) {}
};

} // namespace ktui
