#pragma once

// From Uiverse.io by Galahhad - https://uiverse.io/Galahhad/modern-quail-75
// MIT License, Copyright (c) Galahhad (Galahad)

#include <string>
#include <ktui/core/fn.hpp>

namespace ktui {

// 开关纯数据契约 (ToggleSwitch Data Layer)
struct ToggleSwitchData {
    std::string label{""};
    bool checked{false};
    float track_width{40.0f};      // KTUI 桌面紧凑尺寸 40x20px
    float track_height{20.0f};
    float font_size{13.0f};        // KTUI 基准字号 13px
    float gap{8.0f};               // 开关与标签间距 8px
    bool disabled{false};
    bool inset_shadow{false};      // 是否在开关槽道绘制解析高斯内阴影
    bool liquid_thumb{false};      // 是否使用 SDF Smooth-Min 液态粘连球囊

    Fn<void(bool)> on_change{nullptr};
};

// 开关交互状态
struct ToggleSwitchInteractiveState {
    bool is_hovered{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_disabled{false};
};

} // namespace ktui
