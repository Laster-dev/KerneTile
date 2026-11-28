#pragma once

#include "combobox_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <algorithm>

namespace ktui {

// 下拉框解析后的视觉样式快照 (ComboBox Style Layer - WinUI 3 / Windows 11 Fluent 2 规范)
struct ResolvedComboBoxStyle {
    // 触发框 (Closed / Open Trigger Button)
    Color background;
    Color border;
    Color bottom_border;       // WinUI 3 ControlElevationBorder 底部微高光/阴影线
    Color text;
    Color placeholder_text;
    Color chevron;

    // 弹出面板 (Flyout Popup Surface)
    Color popup_bg;
    Color popup_border;
    Color popup_shadow;

    // 选项条目 (ComboBoxItem)
    Color item_hover_bg;
    Color item_selected_bg;
    Color item_pressed_bg;
    Color item_text;
    Color item_disabled_text;
    Color accent_pill;         // WinUI 3 标志性左侧 3px x 16px 强调色选中竖条
    Color scrollbar_thumb;

    float height{32.0f};
    float item_height{32.0f};
    float font_size{13.0f};
    float corner_radius{4.0f};       // WinUI 3 ControlCornerRadius = 4px
    float popup_corner_radius{8.0f}; // WinUI 3 OverlayCornerRadius = 8px
    float item_corner_radius{4.0f};
    float border_width{1.0f};
    float padding_x{11.0f};
    float popup_padding{4.0f};
};

class ComboBoxStyleResolver {
public:
    static ResolvedComboBoxStyle resolve(const ComboBoxData& data,
                                         const ComboBoxInteractiveState& state,
                                         const Theme& theme) {
        ResolvedComboBoxStyle style;
        style.height = data.height > 0.0f ? data.height : 32.0f;
        style.item_height = data.item_height > 0.0f ? data.item_height : 32.0f;
        style.font_size = data.font_size > 0.0f ? data.font_size : 13.0f;

        if (theme.is_dark) {
            // WinUI 3 Dark Theme Tokens
            if (state.is_pressed || state.is_open) {
                style.background = Color{1.0f, 1.0f, 1.0f, 0.033f};      // ControlFillColorTertiary
            } else if (state.is_hovered) {
                style.background = Color{1.0f, 1.0f, 1.0f, 0.084f};      // ControlFillColorSecondary
            } else {
                style.background = Color{1.0f, 1.0f, 1.0f, 0.060f};      // ControlFillColorDefault
            }

            style.border           = Color{1.0f, 1.0f, 1.0f, 0.085f};    // ControlStrokeColorDefault
            style.bottom_border    = Color{1.0f, 1.0f, 1.0f, 0.14f};     // ControlElevationBorder
            style.text             = Color{1.0f, 1.0f, 1.0f, 0.96f};
            style.placeholder_text = Color{1.0f, 1.0f, 1.0f, 0.56f};
            style.chevron          = Color{1.0f, 1.0f, 1.0f, 0.78f};

            // Flyout Surface (WinUI 3 AcrylicBackgroundFillColorDefaultBrush Dark #2C2C2C)
            style.popup_bg         = Color{44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 0.74f};
            style.popup_border     = Color{1.0f, 1.0f, 1.0f, 0.09f};
            style.popup_shadow     = Color{0.0f, 0.0f, 0.0f, 0.34f};

            style.item_hover_bg    = Color{1.0f, 1.0f, 1.0f, 0.0605f};   // SubtleFillColorSecondary
            style.item_selected_bg = Color{1.0f, 1.0f, 1.0f, 0.0605f};
            style.item_pressed_bg  = Color{1.0f, 1.0f, 1.0f, 0.0419f};   // SubtleFillColorTertiary
            style.item_text        = Color{1.0f, 1.0f, 1.0f, 0.96f};
            style.item_disabled_text = Color{1.0f, 1.0f, 1.0f, 0.36f};
            style.accent_pill      = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}; // #60CDFF
            style.scrollbar_thumb  = Color{1.0f, 1.0f, 1.0f, 0.38f};
        } else {
            // WinUI 3 Light Theme Tokens
            if (state.is_pressed || state.is_open) {
                style.background = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.35f};
            } else if (state.is_hovered) {
                style.background = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.58f};
            } else {
                style.background = Color{1.0f, 1.0f, 1.0f, 0.75f};
            }

            style.border           = Color{0.0f, 0.0f, 0.0f, 0.08f};
            style.bottom_border    = Color{0.0f, 0.0f, 0.0f, 0.16f};
            style.text             = Color{0.0f, 0.0f, 0.0f, 0.895f};
            style.placeholder_text = Color{0.0f, 0.0f, 0.0f, 0.56f};
            style.chevron          = Color{0.0f, 0.0f, 0.0f, 0.65f};

            // Flyout Surface (WinUI 3 AcrylicBackgroundFillColorDefaultBrush Light #FCFCFC)
            style.popup_bg         = Color{252.0f / 255.0f, 252.0f / 255.0f, 252.0f / 255.0f, 0.80f};
            style.popup_border     = Color{0.0f, 0.0f, 0.0f, 0.08f};
            style.popup_shadow     = Color{0.0f, 0.0f, 0.0f, 0.14f};

            style.item_hover_bg    = Color{0.0f, 0.0f, 0.0f, 0.038f};
            style.item_selected_bg = Color{0.0f, 0.0f, 0.0f, 0.045f};
            style.item_pressed_bg  = Color{0.0f, 0.0f, 0.0f, 0.025f};
            style.item_text        = Color{0.0f, 0.0f, 0.0f, 0.895f};
            style.item_disabled_text = Color{0.0f, 0.0f, 0.0f, 0.36f};
            style.accent_pill      = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};            // #005FB8
            style.scrollbar_thumb  = Color{0.0f, 0.0f, 0.0f, 0.32f};
        }

        if (state.is_disabled) {
            if (theme.is_dark) {
                style.background       = Color{1.0f, 1.0f, 1.0f, 0.04f};
                style.border           = Color{1.0f, 1.0f, 1.0f, 0.05f};
                style.bottom_border    = Color::transparent();
                style.text             = Color{1.0f, 1.0f, 1.0f, 0.36f};
                style.placeholder_text = Color{1.0f, 1.0f, 1.0f, 0.30f};
                style.chevron          = Color{1.0f, 1.0f, 1.0f, 0.30f};
            } else {
                style.background       = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.30f};
                style.border           = Color{0.0f, 0.0f, 0.0f, 0.06f};
                style.bottom_border    = Color::transparent();
                style.text             = Color{0.0f, 0.0f, 0.0f, 0.36f};
                style.placeholder_text = Color{0.0f, 0.0f, 0.0f, 0.30f};
                style.chevron          = Color{0.0f, 0.0f, 0.0f, 0.30f};
            }
        }

        return style;
    }
};

} // namespace ktui
