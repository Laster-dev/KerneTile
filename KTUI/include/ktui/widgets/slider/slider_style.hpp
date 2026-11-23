#pragma once

#include "slider_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <algorithm>

namespace ktui {

// 滑块视觉样式解析 (Slider Style Layer - 严格对齐 WinUI 3 / Windows 11 Fluent 2 规范)
struct ResolvedSliderStyle {
    Color track_inactive;    // 未激活导轨底色 (SliderTrackFill / ControlStrongFillColorDefault)
    Color track_active;      // 激活高亮导轨色 (SliderTrackValueFill / AccentFillColorDefault)
    Color thumb_fill;        // 手柄外圈表面色 (SliderThumbBackground / ControlSolidFillColorDefault)
    Color thumb_border;      // 手柄外圈立体微描边 (ControlElevationBorder)
    Color thumb_core;        // 手柄中心强调色圆点 (SliderThumb Inner Dot)
    Color shadow_color;      // 柔和圆形投影色
    Color tick_color;        // 步进刻度色 (SliderTickBarFill)
    Color focus_ring;        // 键盘焦点环色

    float track_height{4.0f};
    float thumb_radius{10.0f};
    float thumb_scale{1.0f}; // WinUI 3 外圈恒定 20px，仅内圆做悬停扩张 / 按下收缩
    float border_width{1.0f};
    float core_radius{5.5f}; // 静止 11px (5.5r) -> 悬停 14px (7.0r) -> 按下 9px (4.5r)
};

class SliderStyleResolver {
public:
    static ResolvedSliderStyle resolve(const SliderData& data,
                                       const SliderInteractiveState& state,
                                       const Theme& theme,
                                       float hover_progress = 0.0f,
                                       float press_progress = 0.0f) {
        ResolvedSliderStyle style;
        style.track_height = data.track_height > 0.0f ? data.track_height : 4.0f;
        style.thumb_radius = data.thumb_radius > 0.0f ? data.thumb_radius : 10.0f;
        style.thumb_scale = 1.0f;
        style.border_width = 1.0f;

        // WinUI 3 核心手感: 外环恒定 20px，内圆悬停舒张 (0.55r -> 0.70r)、按下收紧 (0.45r)
        const float rest_core = style.thumb_radius * 0.55f;
        const float hover_core = style.thumb_radius * 0.70f;
        const float press_core = style.thumb_radius * 0.45f;
        const float hovered_core = rest_core + (hover_core - rest_core) * std::clamp(hover_progress, 0.0f, 1.0f);
        style.core_radius = hovered_core + (press_core - hovered_core) * std::clamp(press_progress, 0.0f, 1.0f);

        if (theme.is_dark) {
            // WinUI 3 Dark Theme Tokens
            const Color accent_rest    = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};  // #60CDFF
            const Color accent_hover   = Color{108.0f / 255.0f, 210.0f / 255.0f, 255.0f / 255.0f, 1.0f}; // #6CD2FF
            const Color accent_pressed = Color{76.0f / 255.0f, 180.0f / 255.0f, 229.0f / 255.0f, 1.0f};  // #4CB4E5

            style.track_inactive = (state.is_pressed || state.is_dragging)
                ? Color{1.0f, 1.0f, 1.0f, 0.45f}
                : (state.is_hovered ? Color{1.0f, 1.0f, 1.0f, 0.62f} : Color{1.0f, 1.0f, 1.0f, 0.544f});
            style.track_active = (state.is_pressed || state.is_dragging)
                ? accent_pressed
                : (state.is_hovered ? accent_hover : accent_rest);

            // WinUI 3 Dark ControlSolidFillColorDefault = #454545 (深灰立体外环，绝非刺眼纯白)
            style.thumb_fill   = Color{69.0f / 255.0f, 69.0f / 255.0f, 69.0f / 255.0f, 1.0f};
            style.thumb_border = Color{1.0f, 1.0f, 1.0f, 0.10f};
            style.thumb_core   = style.track_active;
            style.shadow_color = Color{0.0f, 0.0f, 0.0f, 0.28f};
            style.tick_color   = Color{1.0f, 1.0f, 1.0f, 0.38f};
            style.focus_ring   = Color{1.0f, 1.0f, 1.0f, 0.85f};
        } else {
            // WinUI 3 Light Theme Tokens
            const Color accent_rest    = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};   // #005FB8
            const Color accent_hover   = Color{25.0f / 255.0f, 110.0f / 255.0f, 192.0f / 255.0f, 1.0f};
            const Color accent_pressed = Color{49.0f / 255.0f, 126.0f / 255.0f, 200.0f / 255.0f, 1.0f};

            style.track_inactive = (state.is_pressed || state.is_dragging)
                ? Color{0.0f, 0.0f, 0.0f, 0.36f}
                : (state.is_hovered ? Color{0.0f, 0.0f, 0.0f, 0.52f} : Color{0.0f, 0.0f, 0.0f, 0.446f});
            style.track_active = (state.is_pressed || state.is_dragging)
                ? accent_pressed
                : (state.is_hovered ? accent_hover : accent_rest);

            // WinUI 3 Light ControlSolidFillColorDefault = #FFFFFF + 1px ControlStrokeColorDefault
            style.thumb_fill   = Color::white();
            style.thumb_border = Color{0.0f, 0.0f, 0.0f, 0.14f};
            style.thumb_core   = style.track_active;
            style.shadow_color = Color{0.0f, 0.0f, 0.0f, 0.12f};
            style.tick_color   = Color{0.0f, 0.0f, 0.0f, 0.32f};
            style.focus_ring   = Color{0.0f, 0.0f, 0.0f, 0.80f};
        }

        if (state.is_disabled) {
            style.core_radius = rest_core;
            if (theme.is_dark) {
                style.track_inactive = Color{1.0f, 1.0f, 1.0f, 0.158f};
                style.track_active   = Color{1.0f, 1.0f, 1.0f, 0.158f};
                style.thumb_fill     = Color{62.0f / 255.0f, 62.0f / 255.0f, 62.0f / 255.0f, 1.0f};
                style.thumb_border   = Color{1.0f, 1.0f, 1.0f, 0.06f};
                style.thumb_core     = Color{1.0f, 1.0f, 1.0f, 0.25f};
                style.tick_color     = Color{1.0f, 1.0f, 1.0f, 0.15f};
            } else {
                style.track_inactive = Color{0.0f, 0.0f, 0.0f, 0.217f};
                style.track_active   = Color{0.0f, 0.0f, 0.0f, 0.217f};
                style.thumb_fill     = Color::white();
                style.thumb_border   = Color{0.0f, 0.0f, 0.0f, 0.08f};
                style.thumb_core     = Color{0.0f, 0.0f, 0.0f, 0.217f};
                style.tick_color     = Color{0.0f, 0.0f, 0.0f, 0.15f};
            }
            style.shadow_color.a = 0.0f;
        }

        return style;
    }
};

} // namespace ktui
