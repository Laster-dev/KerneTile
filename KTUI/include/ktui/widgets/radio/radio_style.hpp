#pragma once

// RadioCard inspired by Uiverse.io (mahiatlinux/neat-donkey-46, MIT License)
// RadioButton follows WinUI 3 (Windows 11 Fluent 2) official specification

#include "radio_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <algorithm>

namespace ktui {

// ==================== 1. 经典 WinUI 3 单选按钮样式 (RadioButton) ====================
struct ResolvedWinUIRadioStyle {
    Color outer_fill;         // 外圆填充 (未选中微透 / 选中 WinUI 强调色)
    Color outer_border;       // 未选中 1px 外圆描边 (ControlStrongStrokeColorDefault)
    Color inner_dot;          // 选中时中心圆点 (Dark: #000000 / Light: #FFFFFF)
    Color text;               // 标签文本色 (自动适配深浅色主题)

    float outer_diameter{20.0f}; // WinUI 3 RadioButton 固定 20px
    float inner_radius{5.0f};    // 静止 10px(5r) -> 悬停 12px(6r) -> 按下 8px(4r)
    float border_width{1.0f};
    float font_size{13.0f};
    float gap{8.0f};
    float row_height{28.0f};
};

class WinUIRadioStyleResolver {
public:
    static ResolvedWinUIRadioStyle resolve(const RadioButtonData& data,
                                           const RadioButtonInteractiveState& state,
                                           const Theme& theme,
                                           float hover_progress = 0.0f,
                                           float press_progress = 0.0f) {
        ResolvedWinUIRadioStyle s;
        s.outer_diameter = 20.0f;
        s.border_width = 1.0f;
        s.font_size = data.font_size > 0.0f ? data.font_size : 13.0f;
        s.gap = data.gap > 0.0f ? data.gap : 8.0f;
        s.row_height = std::max(28.0f, s.font_size + 12.0f);

        // WinUI 3 经典单选内圆微交互: 静止 5.0r -> 悬停 6.0r -> 按下 4.0r
        float rest_r = 5.0f;
        float hov_r = 6.0f;
        float prs_r = 4.0f;
        float h = rest_r + (hov_r - rest_r) * std::clamp(hover_progress, 0.0f, 1.0f);
        s.inner_radius = h + (prs_r - h) * std::clamp(press_progress, 0.0f, 1.0f);

        if (theme.is_dark) {
            const Color accent_rest  = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}; // #60CDFF
            const Color accent_hover = Color{108.0f / 255.0f, 210.0f / 255.0f, 255.0f / 255.0f, 1.0f};
            const Color accent_press = Color{76.0f / 255.0f, 180.0f / 255.0f, 229.0f / 255.0f, 1.0f};

            if (data.checked) {
                s.outer_fill   = state.is_pressed ? accent_press : (state.is_hovered ? accent_hover : accent_rest);
                s.outer_border = Color::transparent();
                s.inner_dot    = Color{0.0f, 0.0f, 0.0f, 0.90f}; // WinUI 3 TextOnAccentFillColorPrimary
            } else {
                s.outer_fill   = state.is_pressed ? Color{1.0f, 1.0f, 1.0f, 0.08f}
                               : (state.is_hovered ? Color{1.0f, 1.0f, 1.0f, 0.055f} : Color{0.0f, 0.0f, 0.0f, 0.10f});
                s.outer_border = state.is_hovered ? Color{1.0f, 1.0f, 1.0f, 0.72f} : Color{1.0f, 1.0f, 1.0f, 0.544f};
                s.inner_dot    = Color::transparent();
            }
            s.text = theme.colors.text_primary;
        } else {
            const Color accent_rest  = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f}; // #005FB8
            const Color accent_hover = Color{25.0f / 255.0f, 110.0f / 255.0f, 192.0f / 255.0f, 1.0f};
            const Color accent_press = Color{49.0f / 255.0f, 126.0f / 255.0f, 200.0f / 255.0f, 1.0f};

            if (data.checked) {
                s.outer_fill   = state.is_pressed ? accent_press : (state.is_hovered ? accent_hover : accent_rest);
                s.outer_border = Color::transparent();
                s.inner_dot    = Color::white();
            } else {
                s.outer_fill   = state.is_pressed ? Color{0.0f, 0.0f, 0.0f, 0.06f}
                               : (state.is_hovered ? Color{0.0f, 0.0f, 0.0f, 0.038f} : Color{0.0f, 0.0f, 0.0f, 0.02f});
                s.outer_border = state.is_hovered ? Color{0.0f, 0.0f, 0.0f, 0.62f} : Color{0.0f, 0.0f, 0.0f, 0.446f};
                s.inner_dot    = Color::transparent();
            }
            s.text = theme.colors.text_primary;
        }

        if (state.is_disabled) {
            s.inner_radius = rest_r;
            if (theme.is_dark) {
                s.outer_fill   = data.checked ? Color{1.0f, 1.0f, 1.0f, 0.16f} : Color::transparent();
                s.outer_border = data.checked ? Color::transparent() : Color{1.0f, 1.0f, 1.0f, 0.16f};
                s.inner_dot    = data.checked ? Color{0.0f, 0.0f, 0.0f, 0.50f} : Color::transparent();
                s.text         = Color{1.0f, 1.0f, 1.0f, 0.36f};
            } else {
                s.outer_fill   = data.checked ? Color{0.0f, 0.0f, 0.0f, 0.22f} : Color::transparent();
                s.outer_border = data.checked ? Color::transparent() : Color{0.0f, 0.0f, 0.0f, 0.22f};
                s.inner_dot    = data.checked ? Color::white() : Color::transparent();
                s.text         = Color{0.0f, 0.0f, 0.0f, 0.36f};
            }
        }
        return s;
    }
};

// ==================== 2. 单选卡片样式 (RadioCard) ====================
struct ResolvedRadioStyle {
    Color background{Color{1.0f, 1.0f, 1.0f, 0.045f}};
    Color border_idle{Color{1.0f, 1.0f, 1.0f, 0.14f}};
    Color accent{Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}};
    Color dot_bg{Color{0.14f, 0.15f, 0.18f, 1.0f}};
    Color icon_idle{Color{0.78f, 0.80f, 0.84f, 1.0f}};
    Color label_idle{Color{0.72f, 0.75f, 0.80f, 1.0f}};

    float tile_width{84.0f};
    float tile_height{68.0f};
    float corner_radius{8.0f};
    float border_width{1.5f};
    float dot_diameter{10.0f};
    float dot_margin{6.0f};
    float font_size{13.0f};
};

class RadioStyleResolver {
public:
    static ResolvedRadioStyle resolve(const RadioButtonData& data,
                                      const RadioButtonInteractiveState& state,
                                      const Theme& theme) {
        ResolvedRadioStyle style;
        style.tile_width = data.tile_width > 0.0f ? data.tile_width : 84.0f;
        style.tile_height = data.tile_height > 0.0f ? data.tile_height : 68.0f;
        style.corner_radius = data.corner_radius > 0.0f ? data.corner_radius : 8.0f;
        style.border_width = 1.5f;
        style.dot_diameter = 10.0f;
        style.dot_margin = 6.0f;
        style.font_size = data.font_size > 0.0f ? data.font_size : 13.0f;

        if (theme.is_dark) {
            style.background = state.is_hovered ? Color{1.0f, 1.0f, 1.0f, 0.075f} : Color{1.0f, 1.0f, 1.0f, 0.045f};
            style.border_idle = Color{1.0f, 1.0f, 1.0f, 0.14f};
            style.accent = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}; // #60CDFF
            style.dot_bg = Color{0.18f, 0.19f, 0.22f, 1.0f};
            style.icon_idle = Color{0.82f, 0.84f, 0.88f, 1.0f};
            style.label_idle = Color{0.76f, 0.79f, 0.84f, 1.0f};
        } else {
            style.background = state.is_hovered ? Color{0.0f, 0.0f, 0.0f, 0.045f} : Color{1.0f, 1.0f, 1.0f, 0.75f};
            style.border_idle = Color{0.0f, 0.0f, 0.0f, 0.14f};
            style.accent = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};            // #005FB8
            style.dot_bg = Color::white();
            style.icon_idle = Color{0.25f, 0.28f, 0.33f, 1.0f};
            style.label_idle = Color{0.20f, 0.23f, 0.28f, 1.0f};
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
