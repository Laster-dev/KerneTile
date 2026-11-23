#pragma once

// From Uiverse.io by Galahhad - https://uiverse.io/Galahhad/modern-quail-75
// MIT License, Copyright (c) Galahhad (Galahad)

#include "toggle_switch_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <algorithm>

namespace ktui {

// 开关解析后的视觉样式快照 (ToggleSwitch Style Layer)
// 颜色与尺寸全面对接 KTUI 主题体系，动画曲线与交互效果对齐 Galahhad/modern-quail-75
struct ResolvedToggleSwitchStyle {
    Color track_off{Color{1.0f, 1.0f, 1.0f, 0.22f}};
    Color track_on{Color{0.0f, 0.525f, 0.941f, 1.0f}};
    Color thumb{Color::white()};
    Color cross_color{Color{0.28f, 0.30f, 0.34f, 0.90f}};
    Color check_color{Color{0.0f, 0.525f, 0.941f, 1.0f}};
    Color label_color{Color{0.95f, 0.95f, 0.98f, 1.0f}};

    float track_width{40.0f};
    float track_height{20.0f};
    float thumb_diameter{14.0f};
    float thumb_offset{3.0f};
    float font_size{13.0f};
    float gap{8.0f};
};

class ToggleSwitchStyleResolver {
public:
    static ResolvedToggleSwitchStyle resolve(const ToggleSwitchData& data,
                                             const ToggleSwitchInteractiveState& state,
                                             const Theme& theme) {
        ResolvedToggleSwitchStyle style;

        style.track_width = data.track_width > 0.0f ? data.track_width : 40.0f;
        style.track_height = data.track_height > 0.0f ? data.track_height : 20.0f;
        style.thumb_diameter = std::max(8.0f, style.track_height - 6.0f);
        style.thumb_offset = (style.track_height - style.thumb_diameter) * 0.5f;
        style.font_size = data.font_size > 0.0f ? data.font_size : 13.0f;
        style.gap = data.gap;

        if (theme.is_dark) {
            style.track_off = state.is_hovered
                ? Color{1.0f, 1.0f, 1.0f, 0.28f}
                : Color{1.0f, 1.0f, 1.0f, 0.20f};
            style.track_on = state.is_hovered
                ? Color{0.08f, 0.58f, 0.98f, 1.0f}
                : Color{0.0f, 0.525f, 0.941f, 1.0f}; // KTUI Fluent Dark Accent (#0086F0)
            style.thumb = Color::white();
            style.cross_color = Color{0.26f, 0.28f, 0.32f, 0.92f};
            style.check_color = style.track_on;
            style.label_color = Color{0.95f, 0.95f, 0.98f, 1.0f};
        } else {
            style.track_off = state.is_hovered
                ? Color{0.0f, 0.0f, 0.0f, 0.28f}
                : Color{0.0f, 0.0f, 0.0f, 0.20f};
            style.track_on = state.is_hovered
                ? Color{0.06f, 0.53f, 0.90f, 1.0f}
                : Color{0.0f, 0.47f, 0.84f, 1.0f};   // KTUI Fluent Light Accent (#0078D4)
            style.thumb = Color::white();
            style.cross_color = Color{0.40f, 0.42f, 0.46f, 0.92f};
            style.check_color = style.track_on;
            style.label_color = Color{0.12f, 0.12f, 0.15f, 1.0f};
        }

        if (state.is_disabled) {
            style.track_off.a *= 0.45f;
            style.track_on.a *= 0.45f;
            style.thumb.a = 0.72f;
            style.cross_color.a *= 0.45f;
            style.check_color.a *= 0.45f;
            style.label_color.a *= 0.50f;
        }

        return style;
    }
};

} // namespace ktui
