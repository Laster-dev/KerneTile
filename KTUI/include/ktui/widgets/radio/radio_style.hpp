#pragma once

// From Uiverse.io by mahiatlinux - https://uiverse.io/mahiatlinux/neat-donkey-46
// MIT License, Copyright (c) Yaya12085 (Yaya Mohamed), mahiatlinux (Maheswar Kumar)

#include "radio_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>

namespace ktui {

// 单选卡片样式快照 (RadioButton Style Layer)
// 颜色对接 KTUI 深浅色主题，形态与动效还原 Uiverse.io (mahiatlinux/neat-donkey-46)
struct ResolvedRadioStyle {
    Color background{Color{1.0f, 1.0f, 1.0f, 0.04f}};
    Color border_idle{Color{1.0f, 1.0f, 1.0f, 0.18f}};
    Color accent{Color{0.0f, 0.525f, 0.941f, 1.0f}};
    Color focus_ring{Color{0.0f, 0.525f, 0.941f, 0.35f}};
    Color dot_bg{Color{0.14f, 0.15f, 0.18f, 1.0f}};
    Color icon_idle{Color{0.74f, 0.76f, 0.80f, 1.0f}};
    Color label_idle{Color{0.62f, 0.65f, 0.70f, 1.0f}};

    float tile_width{80.0f};
    float tile_height{72.0f};
    float corner_radius{8.0f};
    float border_width{2.0f};
    float dot_diameter{12.0f};
    float dot_margin{5.0f};
    float font_size{13.0f};
};

class RadioStyleResolver {
public:
    static ResolvedRadioStyle resolve(const RadioButtonData& data,
                                      const RadioButtonInteractiveState& state,
                                      const Theme& theme) {
        ResolvedRadioStyle style;
        style.tile_width = data.tile_width > 0.0f ? data.tile_width : 80.0f;
        style.tile_height = data.tile_height > 0.0f ? data.tile_height : 72.0f;
        style.corner_radius = data.corner_radius > 0.0f ? data.corner_radius : 8.0f;
        style.border_width = 2.0f;
        style.dot_diameter = 12.0f;
        style.dot_margin = 5.0f;
        style.font_size = data.font_size > 0.0f ? data.font_size : 13.0f;

        if (theme.is_dark) {
            style.background = Color{1.0f, 1.0f, 1.0f, 0.045f};
            style.border_idle = Color{1.0f, 1.0f, 1.0f, 0.18f};
            style.accent = Color{0.0f, 0.525f, 0.941f, 1.0f};      // KTUI Dark Accent (#0086F0)
            style.focus_ring = Color{0.0f, 0.525f, 0.941f, 0.35f};
            style.dot_bg = Color{0.14f, 0.15f, 0.18f, 1.0f};
            style.icon_idle = Color{0.74f, 0.76f, 0.80f, 1.0f};
            style.label_idle = Color{0.64f, 0.67f, 0.72f, 1.0f};
        } else {
            style.background = Color{0.0f, 0.0f, 0.0f, 0.03f};
            style.border_idle = Color{0.0f, 0.0f, 0.0f, 0.18f};
            style.accent = Color{0.0f, 0.47f, 0.84f, 1.0f};        // KTUI Light Accent (#0078D4)
            style.focus_ring = Color{0.0f, 0.47f, 0.84f, 0.30f};
            style.dot_bg = Color{0.97f, 0.98f, 0.99f, 1.0f};
            style.icon_idle = Color{0.35f, 0.38f, 0.43f, 1.0f};
            style.label_idle = Color{0.42f, 0.45f, 0.50f, 1.0f};
        }

        if (state.is_disabled) {
            style.background.a *= 0.5f;
            style.border_idle.a *= 0.5f;
            style.accent.a *= 0.45f;
            style.icon_idle.a *= 0.45f;
            style.label_idle.a *= 0.45f;
        }

        return style;
    }
};

} // namespace ktui
