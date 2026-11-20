#pragma once

#include <string>
#include <functional>
#include <cstdint>

namespace ktui {

// 复选框三种状态 (完全对齐 CUI CheckState 规范)
enum class CheckState : uint8_t {
    Unchecked,      // 未选中
    Checked,        // 已选中
    Indeterminate   // 不确定 / 半选态 (用于树形结构及批量子项部分选中)
};

// 复选框纯数据契约 (Data Layer)
struct CheckboxData {
    std::string label{""};
    CheckState state{CheckState::Unchecked};
    bool is_three_state{false};
    float box_size{18.0f};           // 桌面基准 18px，紧凑模式 16px/14px
    float corner_radius{3.0f};       // 桌面 3px 微圆角 (完全对齐 CUI ApplyCornerRadius(3.0f))
    float font_size{13.0f};          // 桌面 13px 字体
    float gap{8.0f};                 // 方框与标签间距 8px (对齐 CUI)
    bool disabled{false};

    std::function<void(bool)> on_change{nullptr};                 // 经典两态回调
    std::function<void(CheckState)> on_state_change{nullptr};     // 三态回调
};

// 复选框当前交互状态
struct CheckboxInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};

} // namespace ktui
