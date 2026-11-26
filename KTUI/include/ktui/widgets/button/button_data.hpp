#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <string>
#include <ktui/core/fn.hpp>

namespace ktui {

// 按钮设计变体 (完全参照 CUI / Fluent Design 规范)
enum class ButtonVariant {
    Standard,   // 默认标准按钮 (深灰卡片底色 + 微光细边框，用于普通次要操作)
    Accent,     // 强调主按钮 (主题高亮蓝底色 + 白色文字，用于全局关键主操作)
    Subtle,     // 扁平幽灵按钮 (静止全透明 + 悬停显现微底色，用于工具栏或轻量触发)
    Danger      // 破坏性危险按钮 (警示红底色 + 白色文字，用于删除、终止等不可逆操作)
};

// 按钮尺寸规范 (桌面高信息密度标准)
enum class ButtonSize {
    Compact,    // 极度紧凑型 (高度 24px，字号 12px，内边距 3x8px，适用于密集工具条)
    Normal,     // 桌面标准型 (高度 28px，字号 13px，内边距 4x12px，IDE/工程通用基准)
    Large       // 强调型 (高度 32px，字号 14px，内边距 5x16px，首要唤起/对话框确认)
};

// 按钮交互状态
struct ButtonInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
    bool is_loading{false};
};

// 按钮数据模型 (Data Layer)
// 纯粹持有按钮的数据、绑定属性、变体分类、尺寸及行为回调，与具体绘制和布局逻辑彻底解耦
struct ButtonData {
    std::string text{""};
    std::string icon{""};               // 可选图标 (单字符或微图标标识)
    ButtonVariant variant{ButtonVariant::Standard};
    ButtonSize size{ButtonSize::Normal};
    bool disabled{false};
    bool loading{false};
    bool focusable{true};
    bool enable_ripple{true};           // 是否启用 Fluent 水波纹点击微动效

    Fn<void()> on_click{nullptr};
    bool enable_pointer_reveal{false};  // Opt-in Fluent 指针跟随边缘/表面微光 (R3)
};

} // namespace ktui
