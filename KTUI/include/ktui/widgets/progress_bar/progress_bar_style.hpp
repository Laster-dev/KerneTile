#pragma once

#include "progress_bar_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <algorithm>

namespace ktui {

// 进度条视觉样式解析 (ProgressBar Style Layer)
struct ResolvedProgressBarStyle {
    Color track_bg;          // 未激活导轨底色
    Color indicator_color;   // 进度填充色 (Fluent Accent)
    Color glow_color;        // 流光/高光叠加色

    float track_height{4.0f};
    float corner_radius{2.0f};
};

class ProgressBarStyleResolver {
public:
    static ResolvedProgressBarStyle resolve(const ProgressBarData& data,
                                            const ProgressBarInteractiveState& state,
                                            const Theme& theme) {
        ResolvedProgressBarStyle style;
        style.track_height = data.track_height > 0.0f ? data.track_height : 4.0f;
        style.corner_radius = data.corner_radius >= 0.0f ? data.corner_radius : (style.track_height * 0.5f);

        if (theme.is_dark) {
            style.track_bg = Color{1.0f, 1.0f, 1.0f, 0.16f};
            style.indicator_color = Color{0.0f, 0.525f, 0.941f, 1.0f}; // KTUI Fluent Dark Accent (#0086F0)
            style.glow_color = Color{0.35f, 0.78f, 1.0f, 0.85f};
        } else {
            style.track_bg = Color{0.0f, 0.0f, 0.0f, 0.12f};
            style.indicator_color = Color{0.0f, 0.47f, 0.84f, 1.0f};   // KTUI Fluent Light Accent (#0078D4)
            style.glow_color = Color{0.20f, 0.65f, 1.0f, 0.85f};
        }

        if (state.is_disabled) {
            style.track_bg.a *= 0.45f;
            style.indicator_color.a *= 0.45f;
            style.glow_color.a *= 0.45f;
        }

        return style;
    }
};

} // namespace ktui
