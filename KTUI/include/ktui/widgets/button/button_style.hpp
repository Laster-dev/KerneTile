#pragma once

#include "button_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <optional>

namespace ktui {

// 按钮完全解析后的视觉样式快照 (Resolved Visual Tokens)
struct ResolvedButtonStyle {
    Color background{Color::transparent()};
    Color border{Color::transparent()};
    Color text{Color::white()};
    Color focus_ring{Color{0.0f, 0.525f, 0.941f, 1.0f}};
    Color ripple{Color{1.0f, 1.0f, 1.0f, 0.35f}};

    float height{28.0f};
    float corner_radius{3.0f};      // 桌面硬朗微圆角 3px (完全参照 CUI / WinUI 标准)
    float border_width{1.0f};       // 1px 精致微光描边
    float font_size{13.0f};
    float icon_gap{6.0f};           // 图标与文字间隙 6px
    EdgeInsets padding{EdgeInsets(4.0f, 12.0f)};
    float ripple_duration{0.65f};   // 水波纹扩散持续时间
};

// 按钮样式解析器 (Style Layer)
// 参照 D:\CUI\CUI.Core\ui 规范，将主题、变体、尺寸和当前交互状态解析为最终渲染色值
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

        // 1. 基础度量尺寸解析 (Size Metrics)
        switch (data.size) {
            case ButtonSize::Compact:
                style.height = 24.0f;
                style.font_size = 12.0f;
                style.padding = EdgeInsets(3.0f, 8.0f);
                break;
            case ButtonSize::Normal:
            default:
                style.height = 28.0f;
                style.font_size = 13.0f;
                style.padding = EdgeInsets(4.0f, 12.0f);
                break;
            case ButtonSize::Large:
                style.height = 32.0f;
                style.font_size = 14.5f;
                style.padding = EdgeInsets(5.0f, 16.0f);
                break;
        }

        style.corner_radius = 3.0f;
        style.border_width = 1.0f;
        style.icon_gap = 6.0f;
        style.ripple_duration = 0.65f;

        // 2. 状态色彩表解析 (完全匹配 CUI Fluent Palette)
        Color idle_bg, hover_bg, pressed_bg, disabled_bg;
        Color idle_border, hover_border, pressed_border, disabled_border;
        Color idle_text, disabled_text;

        if (theme.is_dark) {
            // 暗色模式 (CUI Dark Palette)
            switch (data.variant) {
                case ButtonVariant::Accent:
                    idle_bg        = Color{0.0f, 0.525f, 0.941f, 1.0f};  // #0086F0 AccentColor
                    hover_bg       = Color{0.10f, 0.58f, 0.96f, 1.0f};  // #1A94F5
                    pressed_bg     = Color{0.0f, 0.46f, 0.82f, 1.0f};   // #0075D1
                    idle_border    = Color{0.15f, 0.60f, 0.98f, 0.6f};
                    hover_border   = Color{0.25f, 0.68f, 1.0f, 0.8f};
                    pressed_border = Color{0.0f, 0.40f, 0.75f, 0.9f};
                    idle_text      = Color::white();
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.18f}; // 半透明微光扩散 (非纯白)
                    break;
                case ButtonVariant::Subtle:
                    idle_bg        = Color::transparent();
                    hover_bg       = Color{1.0f, 1.0f, 1.0f, 0.08f};    // CUI HoverBackground
                    pressed_bg     = Color{1.0f, 1.0f, 1.0f, 0.12f};    // CUI PressedBackground
                    idle_border    = Color::transparent();
                    hover_border   = Color::transparent();
                    pressed_border = Color::transparent();
                    idle_text      = Color{0.88f, 0.88f, 0.92f, 1.0f};
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.10f};
                    break;
                case ButtonVariant::Danger:
                    idle_bg        = Color{0.768f, 0.168f, 0.110f, 1.0f}; // #C42B1C DangerColor
                    hover_bg       = Color{0.85f, 0.22f, 0.15f, 1.0f};
                    pressed_bg     = Color{0.65f, 0.14f, 0.09f, 1.0f};
                    idle_border    = Color{0.88f, 0.25f, 0.18f, 0.6f};
                    hover_border   = Color{0.95f, 0.30f, 0.22f, 0.8f};
                    pressed_border = Color{0.55f, 0.10f, 0.06f, 0.9f};
                    idle_text      = Color::white();
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.16f};
                    break;
                case ButtonVariant::Standard:
                default:
                    idle_bg        = Color{0.18f, 0.19f, 0.21f, 1.0f};   // #2E3036 CardBackground
                    hover_bg       = Color{0.24f, 0.25f, 0.28f, 1.0f};   // Base + Hover overlay
                    pressed_bg     = Color{0.15f, 0.15f, 0.17f, 1.0f};   // Pressed state
                    idle_border    = Color{1.0f, 1.0f, 1.0f, 0.08f};    // CUI InputBorder/CardBorder
                    hover_border   = Color{1.0f, 1.0f, 1.0f, 0.15f};
                    pressed_border = Color{1.0f, 1.0f, 1.0f, 0.06f};
                    idle_text      = Color{0.95f, 0.95f, 0.98f, 1.0f};  // TextPrimary
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.12f};
                    break;
            }
            disabled_bg     = Color{0.14f, 0.14f, 0.16f, 0.50f};
            disabled_border = Color{1.0f, 1.0f, 1.0f, 0.04f};
            disabled_text   = Color{0.53f, 0.53f, 0.53f, 0.65f};       // CUI TextMuted
            style.focus_ring= Color{0.0f, 0.525f, 0.941f, 1.0f};       // CUI FocusedBorder
        } else {
            // 浅色模式 (CUI Light Palette)
            switch (data.variant) {
                case ButtonVariant::Accent:
                    idle_bg        = Color{0.0f, 0.47f, 0.84f, 1.0f};
                    hover_bg       = Color{0.08f, 0.54f, 0.92f, 1.0f};
                    pressed_bg     = Color{0.0f, 0.40f, 0.72f, 1.0f};
                    idle_border    = Color{0.0f, 0.45f, 0.80f, 0.8f};
                    hover_border   = Color{0.10f, 0.55f, 0.95f, 0.9f};
                    pressed_border = Color{0.0f, 0.35f, 0.65f, 1.0f};
                    idle_text      = Color::white();
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.20f};
                    break;
                case ButtonVariant::Subtle:
                    idle_bg        = Color::transparent();
                    hover_bg       = Color{0.0f, 0.0f, 0.0f, 0.06f};
                    pressed_bg     = Color{0.0f, 0.0f, 0.0f, 0.10f};
                    idle_border    = Color::transparent();
                    hover_border   = Color::transparent();
                    pressed_border = Color::transparent();
                    idle_text      = Color{0.12f, 0.12f, 0.14f, 1.0f};
                    style.ripple   = Color{0.0f, 0.0f, 0.0f, 0.08f};
                    break;
                case ButtonVariant::Danger:
                    idle_bg        = Color{0.78f, 0.15f, 0.12f, 1.0f};
                    hover_bg       = Color{0.86f, 0.20f, 0.16f, 1.0f};
                    pressed_bg     = Color{0.68f, 0.12f, 0.09f, 1.0f};
                    idle_border    = Color{0.75f, 0.15f, 0.12f, 0.8f};
                    hover_border   = Color{0.85f, 0.22f, 0.18f, 0.9f};
                    pressed_border = Color{0.60f, 0.10f, 0.08f, 1.0f};
                    idle_text      = Color::white();
                    style.ripple   = Color{1.0f, 1.0f, 1.0f, 0.18f};
                    break;
                case ButtonVariant::Standard:
                default:
                    idle_bg        = Color{0.96f, 0.96f, 0.97f, 1.0f};
                    hover_bg       = Color{0.92f, 0.92f, 0.94f, 1.0f};
                    pressed_bg     = Color{0.86f, 0.86f, 0.88f, 1.0f};
                    idle_border    = Color{0.0f, 0.0f, 0.0f, 0.12f};
                    hover_border   = Color{0.0f, 0.0f, 0.0f, 0.18f};
                    pressed_border = Color{0.0f, 0.0f, 0.0f, 0.10f};
                    idle_text      = Color{0.12f, 0.12f, 0.15f, 1.0f};
                    style.ripple   = Color{0.0f, 0.0f, 0.0f, 0.08f}; // 浅色下为半透明暗影
                    break;
            }
            disabled_bg     = Color{0.92f, 0.92f, 0.94f, 0.50f};
            disabled_border = Color{0.0f, 0.0f, 0.0f, 0.05f};
            disabled_text   = Color{0.60f, 0.60f, 0.65f, 0.70f};
            style.focus_ring= Color{0.0f, 0.47f, 0.84f, 1.0f};
        }

        // 3. 覆盖自定义参数 (若外部显式指定则优先采用)
        if (custom_bg) idle_bg = *custom_bg;
        if (custom_hover) hover_bg = *custom_hover;
        if (custom_active) pressed_bg = *custom_active;
        if (custom_border) idle_border = *custom_border;
        if (custom_text) idle_text = *custom_text;
        if (custom_height) style.height = *custom_height;
        if (custom_radius) style.corner_radius = *custom_radius;
        if (custom_padding) style.padding = *custom_padding;
        if (custom_font_size) style.font_size = *custom_font_size;

        // 4. 根据当前具体状态选取激活样式
        if (state.is_disabled || state.is_loading) {
            style.background = disabled_bg;
            style.border = disabled_border;
            style.text = disabled_text;
        } else if (state.is_pressed) {
            style.background = pressed_bg;
            style.border = pressed_border;
            style.text = idle_text;
        } else if (state.is_hovered) {
            style.background = hover_bg;
            style.border = hover_border;
            style.text = idle_text;
        } else {
            style.background = idle_bg;
            style.border = idle_border;
            style.text = idle_text;
        }

        return style;
    }
};

} // namespace ktui
