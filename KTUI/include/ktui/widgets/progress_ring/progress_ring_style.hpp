#pragma once

#include "progress_ring_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <algorithm>

namespace ktui {

// 环形加载圈视觉样式快照 (ProgressRing Style Layer)
// 严格对齐 Windows 11 Fluent 2 / WinUI 3 Design Tokens
struct ResolvedProgressRingStyle {
    Color track_color;       // 底槽环颜色 (ProgressRingTrackBackground)
    Color indicator_color;   // 进度/旋转高亮弧颜色 (ProgressRingForeground / SystemAccentColor)

    float size{32.0f};
    float stroke_width{3.5f};
    bool show_track{false};
};

class ProgressRingStyleResolver {
public:
    static ResolvedProgressRingStyle resolve(const ProgressRingData& data,
                                            const ProgressRingInteractiveState& state,
                                            const Theme& theme) {
        ResolvedProgressRingStyle style;
        style.size = data.size > 0.0f ? data.size : 32.0f;
        if (data.stroke_width > 0.0f) {
            style.stroke_width = data.stroke_width;
        } else {
            // WinUI 3 官方标准比例: 32px 对应 3.5px，16px 对应 2.0px，64px 对应 6.0px
            style.stroke_width = std::clamp(style.size * 0.105f, 2.0f, 8.0f);
        }
        // 确定模式默认显示底槽，不确定模式遵从 data.show_track
        style.show_track = !data.indeterminate ? true : data.show_track;

        if (theme.is_dark) {
            style.track_color = Color{1.0f, 1.0f, 1.0f, 0.08f};
            style.indicator_color = Color{0.0f, 0.525f, 0.941f, 1.0f}; // WinUI 3 Dark Accent (#0086F0)
        } else {
            style.track_color = Color{0.0f, 0.0f, 0.0f, 0.08f};
            style.indicator_color = Color{0.0f, 0.47f, 0.84f, 1.0f};   // WinUI 3 Light Accent (#0078D4)
        }

        if (state.is_disabled) {
            style.track_color.a *= 0.40f;
            style.indicator_color = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, 0.25f}
                : Color{0.0f, 0.0f, 0.0f, 0.22f};
        }

        return style;
    }
};

} // namespace ktui
