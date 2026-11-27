#pragma once

#include "design_tokens.hpp"
#include "color_palette.hpp"

namespace ktui {

// 完整主题聚合对象 (严格对齐 Windows 11 Fluent 2 / WinUI 3 官方设计令牌体系)
struct Theme {
    bool is_dark = true;
    bool enable_pointer_reveal = false; // 全局 Opt-in Fluent 指针高光跟随 (默认关闭，宿主可全局开启)
    ColorPalette colors;
    TypographyTokens typography;
    SpacingTokens spacing;
    CornerRadiusTokens radii;
    ShadowTokens shadows;

    // 全局指针高光默认开关：供应用启动时一键开启，且在深/浅色主题切换时自动保持
    [[nodiscard]] static bool global_pointer_reveal() noexcept {
        return global_pointer_reveal_storage();
    }
    static void set_global_pointer_reveal(bool enabled) noexcept {
        global_pointer_reveal_storage() = enabled;
    }

    // Windows 11 Fluent 2 / WinUI 3 官方深色主题 (Dark Theme)
    [[nodiscard]] static Theme dark() noexcept {
        Theme t;
        t.is_dark = true;
        t.enable_pointer_reveal = global_pointer_reveal_storage();

        // 中性表面与边界 (WinUI 3 SolidBackground / Layer / SurfaceStroke)
        t.colors.background = Color{32.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f, 1.0f};          // #202020 SolidBackgroundFillColorBase
        t.colors.surface = Color{44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f};             // #2C2C2C SolidBackgroundFillColorQuaternary
        t.colors.surface_variant = Color{50.0f / 255.0f, 50.0f / 255.0f, 50.0f / 255.0f, 1.0f};     // #323232 Elevated Surface
        t.colors.border = Color{1.0f, 1.0f, 1.0f, 0.0837f};                                         // SurfaceStrokeColorDefault
        t.colors.border_subtle = Color{1.0f, 1.0f, 1.0f, 0.0578f};                                  // CardStrokeColorDefault

        // 系统强调色 (WinUI 3 Dark: SystemAccentColorLight2 #60CDFF)
        t.colors.primary = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};           // #60CDFF
        t.colors.primary_hover = Color{88.0f / 255.0f, 188.0f / 255.0f, 235.0f / 255.0f, 1.0f};     // #58BCEB
        t.colors.primary_active = Color{78.0f / 255.0f, 168.0f / 255.0f, 212.0f / 255.0f, 1.0f};    // #4EA8D4
        t.colors.on_primary = Color{0.0f, 0.0f, 0.0f, 0.90f};                                       // TextOnAccentFillColorPrimary (Dark)

        t.colors.secondary = Color{1.0f, 1.0f, 1.0f, 0.0605f};                                      // ControlFillColorDefault
        t.colors.secondary_hover = Color{1.0f, 1.0f, 1.0f, 0.0837f};                                // ControlFillColorSecondary
        t.colors.on_secondary = Color{1.0f, 1.0f, 1.0f, 1.0f};

        // 文本层级 (WinUI 3 TextFillColorPrimary / Secondary / Tertiary / Disabled)
        t.colors.text_primary = Color{1.0f, 1.0f, 1.0f, 1.0f};                                      // #FFFFFF
        t.colors.text_secondary = Color{1.0f, 1.0f, 1.0f, 0.786f};                                  // ~#C8C8C8
        t.colors.text_muted = Color{1.0f, 1.0f, 1.0f, 0.5442f};                                     // ~#8B8B8B
        t.colors.text_disabled = Color{1.0f, 1.0f, 1.0f, 0.3628f};                                  // TextFillColorDisabled

        // 系统语义状态色 (WinUI 3 Dark SystemFillColor)
        t.colors.success = Color{108.0f / 255.0f, 203.0f / 255.0f, 95.0f / 255.0f, 1.0f};           // #6CCB5F
        t.colors.on_success = Color{0.0f, 0.0f, 0.0f, 0.90f};
        t.colors.warning = Color{252.0f / 255.0f, 225.0f / 255.0f, 0.0f / 255.0f, 1.0f};            // #FCE100
        t.colors.on_warning = Color{0.0f, 0.0f, 0.0f, 0.90f};
        t.colors.error = Color{255.0f / 255.0f, 153.0f / 255.0f, 164.0f / 255.0f, 1.0f};            // #FF99A4
        t.colors.on_error = Color{0.0f, 0.0f, 0.0f, 0.90f};
        t.colors.info = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};              // #60CDFF
        t.colors.on_info = Color{0.0f, 0.0f, 0.0f, 0.90f};

        t.colors.hover_overlay = Color{1.0f, 1.0f, 1.0f, 0.0605f};
        t.colors.active_overlay = Color{1.0f, 1.0f, 1.0f, 0.0419f};
        t.colors.focus_ring = Color{1.0f, 1.0f, 1.0f, 0.85f};                                       // FocusStrokeColorOuter (Dark)

        // WinUI 3 / Fluent 2 Dark 精细语义令牌
        t.colors.control_fill_default      = Color{1.0f, 1.0f, 1.0f, 0.0605f};
        t.colors.control_fill_secondary    = Color{1.0f, 1.0f, 1.0f, 0.0837f};
        t.colors.control_fill_tertiary     = Color{1.0f, 1.0f, 1.0f, 0.0326f};
        t.colors.control_fill_input_active = Color{30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 0.70f};
        t.colors.control_fill_disabled     = Color{1.0f, 1.0f, 1.0f, 0.0419f};
        t.colors.control_stroke_default    = Color{1.0f, 1.0f, 1.0f, 0.0698f};
        t.colors.control_stroke_bottom     = Color{1.0f, 1.0f, 1.0f, 0.5442f};
        t.colors.flyout_surface            = Color{44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f};
        t.colors.flyout_stroke             = Color{1.0f, 1.0f, 1.0f, 0.0837f};
        t.colors.card_surface              = Color{1.0f, 1.0f, 1.0f, 0.0512f};
        t.colors.card_stroke               = Color{0.0f, 0.0f, 0.0f, 0.10f};
        t.colors.subtle_fill_secondary     = Color{1.0f, 1.0f, 1.0f, 0.0605f};
        t.colors.subtle_fill_tertiary      = Color{1.0f, 1.0f, 1.0f, 0.0419f};
        t.colors.divider_stroke            = Color{1.0f, 1.0f, 1.0f, 0.0837f};

        return t;
    }

    // Windows 11 Fluent 2 / WinUI 3 官方浅色主题 (Light Theme)
    [[nodiscard]] static Theme light() noexcept {
        Theme t;
        t.is_dark = false;
        t.enable_pointer_reveal = global_pointer_reveal_storage();

        // 中性表面与边界 (WinUI 3 Mica Base #F3F3F3 / Layer #FFFFFF)
        t.colors.background = Color{243.0f / 255.0f, 243.0f / 255.0f, 243.0f / 255.0f, 1.0f};       // #F3F3F3 SolidBackgroundFillColorBase
        t.colors.surface = Color{1.0f, 1.0f, 1.0f, 1.0f};                                           // #FFFFFF LayerFillColorDefault
        t.colors.surface_variant = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 1.0f};  // #F9F9F9 SolidBackgroundFillColorTertiary
        t.colors.border = Color{0.0f, 0.0f, 0.0f, 0.0578f};                                         // CardStrokeColorDefault
        t.colors.border_subtle = Color{0.0f, 0.0f, 0.0f, 0.04f};

        // 系统强调色 (WinUI 3 Light: SystemAccentColorDark1 #005FB8)
        t.colors.primary = Color{0.0f / 255.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};             // #005FB8
        t.colors.primary_hover = Color{25.0f / 255.0f, 110.0f / 255.0f, 192.0f / 255.0f, 1.0f};     // #196EC0
        t.colors.primary_active = Color{50.0f / 255.0f, 126.0f / 255.0f, 200.0f / 255.0f, 1.0f};    // #327EC8
        t.colors.on_primary = Color{1.0f, 1.0f, 1.0f, 1.0f};                                        // TextOnAccentFillColorPrimary (Light)

        t.colors.secondary = Color{1.0f, 1.0f, 1.0f, 0.70f};
        t.colors.secondary_hover = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.50f};
        t.colors.on_secondary = Color{0.0f, 0.0f, 0.0f, 0.8956f};

        // 文本层级 (WinUI 3 Light TextFillColor)
        t.colors.text_primary = Color{0.0f, 0.0f, 0.0f, 0.8956f};                                   // ~#1A1A1A
        t.colors.text_secondary = Color{0.0f, 0.0f, 0.0f, 0.6063f};                                 // ~#606060
        t.colors.text_muted = Color{0.0f, 0.0f, 0.0f, 0.4458f};                                     // ~#717171
        t.colors.text_disabled = Color{0.0f, 0.0f, 0.0f, 0.3614f};

        // 系统语义状态色 (WinUI 3 Light SystemFillColor)
        t.colors.success = Color{15.0f / 255.0f, 123.0f / 255.0f, 15.0f / 255.0f, 1.0f};            // #0F7B0F
        t.colors.on_success = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.warning = Color{157.0f / 255.0f, 93.0f / 255.0f, 0.0f / 255.0f, 1.0f};             // #9D5D00
        t.colors.on_warning = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.error = Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 1.0f};              // #C42B1C
        t.colors.on_error = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.info = Color{0.0f / 255.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};                // #005FB8
        t.colors.on_info = Color{1.0f, 1.0f, 1.0f, 1.0f};

        t.colors.hover_overlay = Color{0.0f, 0.0f, 0.0f, 0.0373f};
        t.colors.active_overlay = Color{0.0f, 0.0f, 0.0f, 0.0241f};
        t.colors.focus_ring = Color{0.0f, 0.0f, 0.0f, 0.8956f};                                     // FocusStrokeColorOuter (Light)

        // WinUI 3 / Fluent 2 Light 精细语义令牌
        t.colors.control_fill_default      = Color{1.0f, 1.0f, 1.0f, 0.70f};
        t.colors.control_fill_secondary    = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.50f};
        t.colors.control_fill_tertiary     = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.30f};
        t.colors.control_fill_input_active = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.control_fill_disabled     = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.30f};
        t.colors.control_stroke_default    = Color{0.0f, 0.0f, 0.0f, 0.0578f};
        t.colors.control_stroke_bottom     = Color{0.0f, 0.0f, 0.0f, 0.4458f};
        t.colors.flyout_surface            = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 1.0f};
        t.colors.flyout_stroke             = Color{0.0f, 0.0f, 0.0f, 0.0578f};
        t.colors.card_surface              = Color{1.0f, 1.0f, 1.0f, 0.70f};
        t.colors.card_stroke               = Color{0.0f, 0.0f, 0.0f, 0.0578f};
        t.colors.subtle_fill_secondary     = Color{0.0f, 0.0f, 0.0f, 0.0373f};
        t.colors.subtle_fill_tertiary      = Color{0.0f, 0.0f, 0.0f, 0.0241f};
        t.colors.divider_stroke            = Color{0.0f, 0.0f, 0.0f, 0.0578f};

        return t;
    }

    [[nodiscard]] static Theme lerp(const Theme& a, const Theme& b, float t) noexcept {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        Theme out = (t >= 0.5f) ? b : a;
        out.colors = ColorPalette::lerp(a.colors, b.colors, t);
        return out;
    }

    bool operator==(const Theme&) const noexcept = default;

private:
    static bool& global_pointer_reveal_storage() noexcept {
        static bool s_enabled = false;
        return s_enabled;
    }
};

} // namespace ktui
