#pragma once

#include "slider_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <algorithm>

namespace ktui {

// 滑块视觉样式解析 (Slider Style Layer)
struct ResolvedSliderStyle {
    Color track_inactive;    // 未激活导轨底色
    Color track_active;      // 激活高亮导轨色 (Fluent Accent)
    Color thumb_fill;        // 手柄背景色
    Color thumb_border;      // 手柄外圈边框色
    Color thumb_core;        // 手柄中心圆点色 (Fluent Accent)
    Color shadow_color;      // 悬停/按下柔和发光投影

    float track_height{4.0f};
    float thumb_radius{8.5f};
    float thumb_scale{1.0f}; // 悬停 1.15x，按下 0.88x (Fluent 弹簧微质感)
    float border_width{1.0f};
    float core_radius{4.0f};
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
        style.thumb_radius = data.thumb_radius > 0.0f ? data.thumb_radius : 8.5f;
        style.core_radius = std::max(2.5f, style.thumb_radius * 0.45f);

        // 弹性手感: 悬停轻度舒张 (+12%)，按下微收缩 (-10%)
        style.thumb_scale = 1.0f + hover_progress * 0.12f - press_progress * 0.10f;

        if (theme.is_dark) {
            style.track_inactive = state.is_hovered
                ? Color{1.0f, 1.0f, 1.0f, 0.28f}
                : Color{1.0f, 1.0f, 1.0f, 0.18f};
            style.track_active = (state.is_pressed || state.is_dragging)
                ? Color{0.0f, 0.45f, 0.85f, 1.0f}
                : (state.is_hovered ? Color{0.10f, 0.60f, 1.0f, 1.0f} : Color{0.0f, 0.525f, 0.941f, 1.0f});
            // Windows 11 Fluent 规范: 手柄采用立体纯白圆盘，悬停/按下微缩放
            style.thumb_fill = Color::white();
            style.thumb_border = Color{0.0f, 0.0f, 0.0f, 0.18f};
            style.thumb_core = style.track_active;
            style.shadow_color = Color{0.0f, 0.0f, 0.0f, 0.35f};
        } else {
            style.track_inactive = state.is_hovered
                ? Color{0.0f, 0.0f, 0.0f, 0.28f}
                : Color{0.0f, 0.0f, 0.0f, 0.18f};
            style.track_active = (state.is_pressed || state.is_dragging)
                ? Color{0.0f, 0.40f, 0.76f, 1.0f}
                : (state.is_hovered ? Color{0.06f, 0.53f, 0.90f, 1.0f} : Color{0.0f, 0.47f, 0.84f, 1.0f});
            style.thumb_fill = Color::white();
            style.thumb_border = Color{0.0f, 0.0f, 0.0f, 0.12f};
            style.thumb_core = style.track_active;
            style.shadow_color = Color{0.0f, 0.0f, 0.0f, 0.20f};
        }

        if (state.is_disabled) {
            style.track_inactive.a *= 0.40f;
            style.track_active.a *= 0.40f;
            style.thumb_fill = Color{0.65f, 0.65f, 0.70f, 0.60f};
            style.thumb_border.a *= 0.30f;
            style.thumb_core.a *= 0.40f;
            style.shadow_color.a = 0.0f;
        }

        return style;
    }
};

} // namespace ktui
