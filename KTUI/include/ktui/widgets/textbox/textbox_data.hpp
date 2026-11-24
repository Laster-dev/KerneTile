#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <string>
#include <ktui/core/fn.hpp>
#include <vector>

namespace ktui {

// 文本框撤销/重做快照 (Undo/Redo Snapshot)
struct TextBoxUndoState {
    std::string text;
    size_t cursor_pos{0};
    size_t selection_anchor{0};
};

// 文本框交互状态
struct TextBoxInteractiveState {
    bool is_hovered{false};
    bool is_focused{false};
    bool is_disabled{false};
    bool is_read_only{false};
    bool is_dragging_selection{false};
};

// 文本框数据模型 (TextBox Data Layer)
// 纯粹持有文本框的数据、属性、响应式绑定与行为回调，与绘制和事件逻辑彻底解耦
struct TextBoxData {
    std::string text{""};
    std::string placeholder{""};
    bool is_password{false};
    bool is_password_revealed{false};
    bool is_read_only{false};
    bool disabled{false};
    bool accepts_return{false};
    bool text_wrapping{false};
    bool show_reveal_button{false};   // 密码模式下是否显示右侧显示/隐藏明文眼睛按钮
    float font_size{13.0f};

    Fn<void(const std::string&)> on_change{nullptr};
    Fn<void(const std::string&)> on_submit{nullptr};
};

} // namespace ktui
