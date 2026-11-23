#pragma once

#include "progress_bar_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <algorithm>

namespace ktui {

// 进度条视觉样式解析 (ProgressBar Style Layer - 严格对齐 WinUI 3 / Windows 11 Fluent 2 规范)
struct ResolvedProgressBarStyle {
    Color track_bg;          // 1px 极细背景底线色 (ControlStrongStrokeColorDefault)
    Color indicator_color;   // 3px 药丸进度指示色 (Accent / Caution / Critical)
    Color glow_color;        // 保留字段兼容外部访问

    float rail_height{1.0f};   // WinUI 3 底槽为 1px 极细水平线，居中于指示条背后
    float track_height{3.0f};  // WinUI 3 前景指示条默认 3px 胶囊
    float corner_radius{1.5f};
    bool show_rail{true};      // WinUI 3 不确定模式隐藏 1px 底线，确定模式显示
};

class ProgressBarStyleResolver {
public:
    static ResolvedProgressBarStyle resolve(const ProgressBarData& data,
                                            const ProgressBarInteractiveState& state,
                                            const Theme& theme) {
        ResolvedProgressBarStyle style;
        style.track_height = data.track_height > 0.0f ? data.track_height : 3.0f;
        style.rail_height = 1.0f;
        style.corner_radius = data.corner_radius >= 0.0f ? data.corner_radius : (style.track_height * 0.5f);
        style.show_rail = !data.indeterminate;

        if (theme.is_dark) {
            // WinUI 3 Dark Theme Tokens
            style.track_bg = Color{1.0f, 1.0f, 1.0f, 0.42f}; // ControlStrongStrokeColorDefault
            switch (data.state) {
                case ProgressBarState::Paused:
                    style.indicator_color = Color{252.0f / 255.0f, 225.0f / 255.0f, 0.0f, 1.0f};           // #FCE100 Caution
                    break;
                case ProgressBarState::Error:
                    style.indicator_color = Color{255.0f / 255.0f, 153.0f / 255.0f, 164.0f / 255.0f, 1.0f}; // #FF99A4 Critical
                    break;
                case ProgressBarState::Normal:
                default:
                    style.indicator_color = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};  // #60CDFF Accent
                    break;
            }
            style.glow_color = style.indicator_color;
        } else {
            // WinUI 3 Light Theme Tokens
            style.track_bg = Color{0.0f, 0.0f, 0.0f, 0.38f}; // ControlStrongStrokeColorDefault
            switch (data.state) {
                case ProgressBarState::Paused:
                    style.indicator_color = Color{157.0f / 255.0f, 93.0f / 255.0f, 0.0f, 1.0f};            // #9D5D00 Caution
                    break;
                case ProgressBarState::Error:
                    style.indicator_color = Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 1.0f};  // #C42B1C Critical
                    break;
                case ProgressBarState::Normal:
                default:
                    style.indicator_color = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};            // #005FB8 Accent
                    break;
            }
            style.glow_color = style.indicator_color;
        }

        if (state.is_disabled) {
            if (theme.is_dark) {
                style.track_bg        = Color{1.0f, 1.0f, 1.0f, 0.158f};
                style.indicator_color = Color{1.0f, 1.0f, 1.0f, 0.25f};
            } else {
                style.track_bg        = Color{0.0f, 0.0f, 0.0f, 0.217f};
                style.indicator_color = Color{0.0f, 0.0f, 0.0f, 0.25f};
            }
            style.glow_color = style.indicator_color;
        }

        return style;
    }
};

} // namespace ktui
