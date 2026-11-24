#pragma once

#include "button_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <optional>

namespace ktui {

// 按钮完全解析后的视觉样式快照 (WinUI 3 / Fluent 2 Resolved Visual Tokens)
struct ResolvedButtonStyle {
    Color background{Color::transparent()};
    Color border{Color::transparent()};
    Color elevation_edge{Color::transparent()}; // WinUI 3 ControlElevationBorder 高光/暗部立体边
    bool elevation_on_top{false};               // Dark 模式高光在顶边，Light 模式暗边在底边
    Color text{Color::white()};
    Color focus_ring{Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f}};
    Color ripple{Color{1.0f, 1.0f, 1.0f, 0.14f}};

    float height{32.0f};
    float corner_radius{4.0f};      // WinUI 3 ControlCornerRadius = 4px
    float border_width{1.0f};       // WinUI 3 1px 精致轮廓描边
    float font_size{13.5f};
    float icon_gap{6.0f};
    EdgeInsets padding{EdgeInsets(5.0f, 12.0f)};
    float ripple_duration{0.55f};
};

// WinUI 3 / Fluent 2 按钮样式解析器 (Style Layer)
class ButtonStyleResolver {
public:
    static ResolvedButtonStyle resolve(const ButtonData& data,
                                       const ButtonInteractiveState& state,
                                       const Theme& theme,
                                       std::optional<Color> custom_bg = std::nullopt,
                                       std::optional<Color> custom_hover = std::nullopt,
                                       std::optional<Color> custom_active = std::nullopt,
                                       std::optional<Color> custom_text = std::nullopt,
                                       std::optional<Color> custom_border = std::nullopt,
                                       std::optional<float> custom_height = std::nullopt,
                                       std::optional<float> custom_radius = std::nullopt,
                                       std::optional<EdgeInsets> custom_padding = std::nullopt,
                                       std::optional<float> custom_font_size = std::nullopt) {
        ResolvedButtonStyle style;

        // 1. WinUI 3 尺寸度量 (Standard Button = 32px)
        switch (data.size) {
            case ButtonSize::Compact:
                style.height = 26.0f;
                style.font_size = 12.0f;
                style.padding = EdgeInsets(3.0f, 9.0f);
                break;
            case ButtonSize::Normal:
            default:
                style.height = 32.0f;
                style.font_size = 13.5f;
                style.padding = EdgeInsets(5.0f, 12.0f);
                break;
            case ButtonSize::Large:
                style.height = 36.0f;
                style.font_size = 14.5f;
                style.padding = EdgeInsets(6.0f, 16.0f);
                break;
        }

        style.corner_radius = 4.0f;
        style.border_width = (data.variant == ButtonVariant::Subtle) ? 0.0f : 1.0f;
        style.icon_gap = 6.0f;
        style.ripple_duration = 0.55f;
        style.elevation_on_top = theme.is_dark;

        // 2. WinUI 3 / Fluent 2 官方配色表
        Color idle_bg, hover_bg, pressed_bg, disabled_bg;
        Color idle_border, hover_border, pressed_border, disabled_border;
        Color idle_text, pressed_text, disabled_text;
        Color elev_edge = Color::transparent();

        if (theme.is_dark) {
            switch (data.variant) {
                case ButtonVariant::Accent:
                    idle_bg        = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}; // #60CDFF
                    hover_bg       = Color{88.0f / 255.0f, 188.0f / 255.0f, 235.0f / 255.0f, 1.0f};
                    pressed_bg     = Color{78.0f / 255.0f, 168.0f / 255.0f, 212.0f / 255.0f, 1.0f};
                    idle_border    = Color{1.0f, 1.0f, 1.0f, 0.08f};
                    hover_border   = Color{1.0f, 1.0f, 1.0f, 0.08f};
                    pressed_border = Color{1.0f, 1.0f, 1.0f, 0.04f};
                    elev_edge      = Color{1.0f, 1.0f, 1.0f, 0.14f};
                    idle_text      = Color{0.0f, 0.0f, 0.0f, 0.90f};
                    pressed_text   = Color{0.0f, 0.0f, 0.0f, 0.55f};
                    disabled_bg    = Color{1.0f, 1.0f, 1.0f, 0.158f};
                    disabled_border= Color::transparent();
                    disabled_text  = Color{1.0f, 1.0f, 1.0f, 0.53f};
                    style.ripple   = Color{0.0f, 0.0f, 0.0f, 0.14f};
                    break;
                case ButtonVariant::Subtle:
                    idle_bg        = Color::transparent();
                    hover_bg       = Color{1.0f, 1.0f, 1.0f, 0.0605f};
                    pressed_bg     = Color{1.0f, 1.0f, 1.0f, 0.0419f};
                    idle_border    = Color::transparent();
                    hover_border   = Color::transparent();
                    pressed_border = Color::transparent();
                    idle_text      = Color{1.0f, 1.0f, 1.0f, 0.96f};
                    pressed_text   = Color{1.0f, 1.0f, 1.0f, 0.78f};
                    disabled_bg    = Color::transparent();
                    disabled_border= Color::transparent();
                    disabled_text  = Color{1.0f, 1.0f, 1.0f, 0.36f};
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.10f};
                    break;
                case ButtonVariant::Danger:
                    idle_bg        = Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 1.0f}; // #C42B1C
                    hover_bg       = Color{209.0f / 255.0f, 52.0f / 255.0f, 38.0f / 255.0f, 1.0f};
                    pressed_bg     = Color{168.0f / 255.0f, 36.0f / 255.0f, 23.0f / 255.0f, 1.0f};
                    idle_border    = Color{1.0f, 1.0f, 1.0f, 0.08f};
                    hover_border   = Color{1.0f, 1.0f, 1.0f, 0.12f};
                    pressed_border = Color{1.0f, 1.0f, 1.0f, 0.04f};
                    elev_edge      = Color{1.0f, 1.0f, 1.0f, 0.14f};
                    idle_text      = Color::white();
                    pressed_text   = Color{1.0f, 1.0f, 1.0f, 0.76f};
                    disabled_bg    = Color{1.0f, 1.0f, 1.0f, 0.0419f};
                    disabled_border= Color{1.0f, 1.0f, 1.0f, 0.053f};
                    disabled_text  = Color{1.0f, 1.0f, 1.0f, 0.36f};
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.16f};
                    break;
                case ButtonVariant::Standard:
                default:
                    idle_bg        = Color{1.0f, 1.0f, 1.0f, 0.0605f}; // ControlFillColorDefault
                    hover_bg       = Color{1.0f, 1.0f, 1.0f, 0.0837f}; // ControlFillColorSecondary
                    pressed_bg     = Color{1.0f, 1.0f, 1.0f, 0.0326f}; // ControlFillColorTertiary
                    idle_border    = Color{1.0f, 1.0f, 1.0f, 0.070f};  // ControlStrokeColorDefault
                    hover_border   = Color{1.0f, 1.0f, 1.0f, 0.090f};
                    pressed_border = Color{1.0f, 1.0f, 1.0f, 0.070f};
                    elev_edge      = Color{1.0f, 1.0f, 1.0f, 0.095f};  // Top elevation highlight
                    idle_text      = Color{1.0f, 1.0f, 1.0f, 0.96f};
                    pressed_text   = Color{1.0f, 1.0f, 1.0f, 0.78f};
                    disabled_bg    = Color{1.0f, 1.0f, 1.0f, 0.0419f};
                    disabled_border= Color{1.0f, 1.0f, 1.0f, 0.053f};
                    disabled_text  = Color{1.0f, 1.0f, 1.0f, 0.36f};
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.12f};
                    break;
            }
            style.focus_ring = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};
        } else {
            switch (data.variant) {
                case ButtonVariant::Accent:
                    idle_bg        = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f}; // #005FB8
                    hover_bg       = Color{25.0f / 255.0f, 110.0f / 255.0f, 191.0f / 255.0f, 1.0f};
                    pressed_bg     = Color{49.0f / 255.0f, 124.0f / 255.0f, 198.0f / 255.0f, 1.0f};
                    idle_border    = Color{1.0f, 1.0f, 1.0f, 0.08f};
                    hover_border   = Color{1.0f, 1.0f, 1.0f, 0.08f};
                    pressed_border = Color::transparent();
                    elev_edge      = Color{0.0f, 0.0f, 0.0f, 0.36f};
                    idle_text      = Color::white();
                    pressed_text   = Color{1.0f, 1.0f, 1.0f, 0.72f};
                    disabled_bg    = Color{0.0f, 0.0f, 0.0f, 0.217f};
                    disabled_border= Color::transparent();
                    disabled_text  = Color::white();
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.18f};
                    break;
                case ButtonVariant::Subtle:
                    idle_bg        = Color::transparent();
                    hover_bg       = Color{0.0f, 0.0f, 0.0f, 0.0373f};
                    pressed_bg     = Color{0.0f, 0.0f, 0.0f, 0.0241f};
                    idle_border    = Color::transparent();
                    hover_border   = Color::transparent();
                    pressed_border = Color::transparent();
                    idle_text      = Color{0.0f, 0.0f, 0.0f, 0.90f};
                    pressed_text   = Color{0.0f, 0.0f, 0.0f, 0.61f};
                    disabled_bg    = Color::transparent();
                    disabled_border= Color::transparent();
                    disabled_text  = Color{0.0f, 0.0f, 0.0f, 0.36f};
                    style.ripple   = Color{0.0f, 0.0f, 0.0f, 0.07f};
                    break;
                case ButtonVariant::Danger:
                    idle_bg        = Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 1.0f};
                    hover_bg       = Color{209.0f / 255.0f, 52.0f / 255.0f, 38.0f / 255.0f, 1.0f};
                    pressed_bg     = Color{168.0f / 255.0f, 36.0f / 255.0f, 23.0f / 255.0f, 1.0f};
                    idle_border    = Color{0.0f, 0.0f, 0.0f, 0.08f};
                    hover_border   = Color{0.0f, 0.0f, 0.0f, 0.10f};
                    pressed_border = Color::transparent();
                    elev_edge      = Color{0.0f, 0.0f, 0.0f, 0.30f};
                    idle_text      = Color::white();
                    pressed_text   = Color{1.0f, 1.0f, 1.0f, 0.72f};
                    disabled_bg    = Color{0.0f, 0.0f, 0.0f, 0.217f};
                    disabled_border= Color::transparent();
                    disabled_text  = Color::white();
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.18f};
                    break;
                case ButtonVariant::Standard:
                default:
                    idle_bg        = Color{1.0f, 1.0f, 1.0f, 0.70f};
                    hover_bg       = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.50f};
                    pressed_bg     = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.30f};
                    idle_border    = Color{0.0f, 0.0f, 0.0f, 0.058f};
                    hover_border   = Color{0.0f, 0.0f, 0.0f, 0.058f};
                    pressed_border = Color{0.0f, 0.0f, 0.0f, 0.058f};
                    elev_edge      = Color{0.0f, 0.0f, 0.0f, 0.16f}; // Bottom elevation shadow line
                    idle_text      = Color{0.0f, 0.0f, 0.0f, 0.90f};
                    pressed_text   = Color{0.0f, 0.0f, 0.0f, 0.61f};
                    disabled_bg    = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.30f};
                    disabled_border= Color{0.0f, 0.0f, 0.0f, 0.058f};
                    disabled_text  = Color{0.0f, 0.0f, 0.0f, 0.36f};
                    style.ripple   = Color{0.0f, 0.0f, 0.0f, 0.07f};
                    break;
            }
            style.focus_ring = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
        }

        // 3. 覆盖自定义参数 (若外部显式指定则优先采用)
        if (custom_bg) idle_bg = *custom_bg;
        if (custom_hover) hover_bg = *custom_hover;
        if (custom_active) pressed_bg = *custom_active;
        if (custom_border) idle_border = *custom_border;
        if (custom_text) { idle_text = *custom_text; pressed_text = *custom_text; }
        if (custom_height) style.height = *custom_height;
        if (custom_radius) style.corner_radius = *custom_radius;
        if (custom_padding) style.padding = *custom_padding;
        if (custom_font_size) style.font_size = *custom_font_size;

        // 4. 根据当前交互状态选取最终色值
        if (state.is_disabled || state.is_loading) {
            style.background = disabled_bg;
            style.border = disabled_border;
            style.text = disabled_text;
            style.elevation_edge = Color::transparent();
        } else if (state.is_pressed) {
            style.background = pressed_bg;
            style.border = pressed_border;
            style.text = pressed_text;
            style.elevation_edge = Color::transparent(); // WinUI 3 按下时压平立体边
        } else if (state.is_hovered) {
            style.background = hover_bg;
            style.border = hover_border;
            style.text = idle_text;
            style.elevation_edge = elev_edge;
        } else {
            style.background = idle_bg;
            style.border = idle_border;
            style.text = idle_text;
            style.elevation_edge = elev_edge;
        }

        return style;
    }
};

} // namespace ktui
