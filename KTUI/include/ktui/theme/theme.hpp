#pragma once

#include "design_tokens.hpp"
#include "color_palette.hpp"

namespace ktui {

// 完整主题聚合对象
struct Theme {
    bool is_dark = true;
    ColorPalette colors;
    TypographyTokens typography;
    SpacingTokens spacing;
    CornerRadiusTokens radii;
    ShadowTokens shadows;

    // 默认标准深色主题 (Dark Theme)
    [[nodiscard]] static Theme dark() noexcept {
        Theme t;
        t.is_dark = true;

        t.colors.background = Color{15.0f / 255.0f, 23.0f / 255.0f, 42.0f / 255.0f, 1.0f};          // #0f172a
        t.colors.surface = Color{30.0f / 255.0f, 41.0f / 255.0f, 59.0f / 255.0f, 1.0f};             // #1e293b
        t.colors.surface_variant = Color{51.0f / 255.0f, 65.0f / 255.0f, 85.0f / 255.0f, 1.0f};     // #334155
        t.colors.border = Color{51.0f / 255.0f, 65.0f / 255.0f, 85.0f / 255.0f, 1.0f};              // #334155
        t.colors.border_subtle = Color{30.0f / 255.0f, 41.0f / 255.0f, 59.0f / 255.0f, 1.0f};       // #1e293b

        t.colors.primary = Color{59.0f / 255.0f, 130.0f / 255.0f, 246.0f / 255.0f, 1.0f};          // #3b82f6
        t.colors.primary_hover = Color{96.0f / 255.0f, 165.0f / 255.0f, 250.0f / 255.0f, 1.0f};    // #60a5fa
        t.colors.primary_active = Color{37.0f / 255.0f, 99.0f / 255.0f, 235.0f / 255.0f, 1.0f};    // #2563eb
        t.colors.on_primary = Color{1.0f, 1.0f, 1.0f, 1.0f};

        t.colors.secondary = Color{100.0f / 255.0f, 116.0f / 255.0f, 139.0f / 255.0f, 1.0f};        // #64748b
        t.colors.secondary_hover = Color{148.0f / 255.0f, 163.0f / 255.0f, 184.0f / 255.0f, 1.0f};  // #94a3b8
        t.colors.on_secondary = Color{1.0f, 1.0f, 1.0f, 1.0f};

        t.colors.text_primary = Color{248.0f / 255.0f, 250.0f / 255.0f, 252.0f / 255.0f, 1.0f};     // #f8fafc
        t.colors.text_secondary = Color{148.0f / 255.0f, 163.0f / 255.0f, 184.0f / 255.0f, 1.0f};   // #94a3b8
        t.colors.text_muted = Color{100.0f / 255.0f, 116.0f / 255.0f, 139.0f / 255.0f, 1.0f};       // #64748b
        t.colors.text_disabled = Color{71.0f / 255.0f, 85.0f / 255.0f, 105.0f / 255.0f, 1.0f};       // #475569

        t.colors.success = Color{16.0f / 255.0f, 185.0f / 255.0f, 129.0f / 255.0f, 1.0f};           // #10b981
        t.colors.on_success = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.warning = Color{245.0f / 255.0f, 158.0f / 255.0f, 11.0f / 255.0f, 1.0f};           // #f59e0b
        t.colors.on_warning = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.error = Color{239.0f / 255.0f, 68.0f / 255.0f, 68.0f / 255.0f, 1.0f};              // #ef4444
        t.colors.on_error = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.info = Color{6.0f / 255.0f, 182.0f / 255.0f, 212.0f / 255.0f, 1.0f};              // #06b6d4
        t.colors.on_info = Color{1.0f, 1.0f, 1.0f, 1.0f};

        t.colors.hover_overlay = Color{1.0f, 1.0f, 1.0f, 0.06f};
        t.colors.active_overlay = Color{1.0f, 1.0f, 1.0f, 0.12f};
        t.colors.focus_ring = Color{59.0f / 255.0f, 130.0f / 255.0f, 246.0f / 255.0f, 0.5f};

        // WinUI 3 / Fluent 2 Dark 语义令牌
        t.colors.control_fill_default      = Color{1.0f, 1.0f, 1.0f, 0.055f};
        t.colors.control_fill_secondary    = Color{1.0f, 1.0f, 1.0f, 0.080f};
        t.colors.control_fill_tertiary     = Color{1.0f, 1.0f, 1.0f, 0.040f};
        t.colors.control_fill_input_active = Color{30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 0.92f};
        t.colors.control_fill_disabled     = Color{1.0f, 1.0f, 1.0f, 0.030f};
        t.colors.control_stroke_default    = Color{1.0f, 1.0f, 1.0f, 0.100f};
        t.colors.control_stroke_bottom     = Color{1.0f, 1.0f, 1.0f, 0.420f};
        t.colors.flyout_surface            = Color{44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f};
        t.colors.flyout_stroke             = Color{1.0f, 1.0f, 1.0f, 0.090f};
        t.colors.card_surface              = Color{1.0f, 1.0f, 1.0f, 0.045f};
        t.colors.card_stroke               = Color{1.0f, 1.0f, 1.0f, 0.070f};
        t.colors.subtle_fill_secondary     = Color{1.0f, 1.0f, 1.0f, 0.065f};
        t.colors.subtle_fill_tertiary      = Color{1.0f, 1.0f, 1.0f, 0.040f};
        t.colors.divider_stroke            = Color{1.0f, 1.0f, 1.0f, 0.080f};

        return t;
    }

    // 默认标准浅色主题 (Light Theme)
    [[nodiscard]] static Theme light() noexcept {
        Theme t;
        t.is_dark = false;

        t.colors.background = Color{248.0f / 255.0f, 250.0f / 255.0f, 252.0f / 255.0f, 1.0f};       // #f8fafc
        t.colors.surface = Color{1.0f, 1.0f, 1.0f, 1.0f};                                            // #ffffff
        t.colors.surface_variant = Color{241.0f / 255.0f, 245.0f / 255.0f, 249.0f / 255.0f, 1.0f}; // #f1f5f9
        t.colors.border = Color{226.0f / 255.0f, 232.0f / 255.0f, 240.0f / 255.0f, 1.0f};          // #e2e8f0
        t.colors.border_subtle = Color{241.0f / 255.0f, 245.0f / 255.0f, 249.0f / 255.0f, 1.0f};   // #f1f5f9

        t.colors.primary = Color{37.0f / 255.0f, 99.0f / 255.0f, 235.0f / 255.0f, 1.0f};           // #2563eb
        t.colors.primary_hover = Color{29.0f / 255.0f, 78.0f / 255.0f, 216.0f / 255.0f, 1.0f};     // #1d4ed8
        t.colors.primary_active = Color{30.0f / 255.0f, 64.0f / 255.0f, 175.0f / 255.0f, 1.0f};    // #1e40af
        t.colors.on_primary = Color{1.0f, 1.0f, 1.0f, 1.0f};

        t.colors.secondary = Color{100.0f / 255.0f, 116.0f / 255.0f, 139.0f / 255.0f, 1.0f};        // #64748b
        t.colors.secondary_hover = Color{71.0f / 255.0f, 85.0f / 255.0f, 105.0f / 255.0f, 1.0f};    // #475569
        t.colors.on_secondary = Color{1.0f, 1.0f, 1.0f, 1.0f};

        t.colors.text_primary = Color{15.0f / 255.0f, 23.0f / 255.0f, 42.0f / 255.0f, 1.0f};        // #0f172a
        t.colors.text_secondary = Color{100.0f / 255.0f, 116.0f / 255.0f, 139.0f / 255.0f, 1.0f};   // #64748b
        t.colors.text_muted = Color{148.0f / 255.0f, 163.0f / 255.0f, 184.0f / 255.0f, 1.0f};       // #94a3b8
        t.colors.text_disabled = Color{203.0f / 255.0f, 213.0f / 255.0f, 225.0f / 255.0f, 1.0f};    // #cbd5e1

        t.colors.success = Color{5.0f / 255.0f, 150.0f / 255.0f, 105.0f / 255.0f, 1.0f};            // #059669
        t.colors.on_success = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.warning = Color{217.0f / 255.0f, 119.0f / 255.0f, 6.0f / 255.0f, 1.0f};            // #d97706
        t.colors.on_warning = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.error = Color{220.0f / 255.0f, 38.0f / 255.0f, 38.0f / 255.0f, 1.0f};             // #dc2626
        t.colors.on_error = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.info = Color{8.0f / 255.0f, 145.0f / 255.0f, 178.0f / 255.0f, 1.0f};               // #0891b2
        t.colors.on_info = Color{1.0f, 1.0f, 1.0f, 1.0f};

        t.colors.hover_overlay = Color{0.0f, 0.0f, 0.0f, 0.04f};
        t.colors.active_overlay = Color{0.0f, 0.0f, 0.0f, 0.08f};
        t.colors.focus_ring = Color{37.0f / 255.0f, 99.0f / 255.0f, 235.0f / 255.0f, 0.5f};

        // WinUI 3 / Fluent 2 Light 语义令牌
        t.colors.control_fill_default      = Color{1.0f, 1.0f, 1.0f, 0.720f};
        t.colors.control_fill_secondary    = Color{1.0f, 1.0f, 1.0f, 0.920f};
        t.colors.control_fill_tertiary     = Color{0.0f, 0.0f, 0.0f, 0.030f};
        t.colors.control_fill_input_active = Color{1.0f, 1.0f, 1.0f, 1.0f};
        t.colors.control_fill_disabled     = Color{0.0f, 0.0f, 0.0f, 0.025f};
        t.colors.control_stroke_default    = Color{0.0f, 0.0f, 0.0f, 0.110f};
        t.colors.control_stroke_bottom     = Color{0.0f, 0.0f, 0.0f, 0.420f};
        t.colors.flyout_surface            = Color{250.0f / 255.0f, 250.0f / 255.0f, 250.0f / 255.0f, 1.0f};
        t.colors.flyout_stroke             = Color{0.0f, 0.0f, 0.0f, 0.090f};
        t.colors.card_surface              = Color{1.0f, 1.0f, 1.0f, 0.650f};
        t.colors.card_stroke               = Color{0.0f, 0.0f, 0.0f, 0.070f};
        t.colors.subtle_fill_secondary     = Color{0.0f, 0.0f, 0.0f, 0.045f};
        t.colors.subtle_fill_tertiary      = Color{0.0f, 0.0f, 0.0f, 0.028f};
        t.colors.divider_stroke            = Color{0.0f, 0.0f, 0.0f, 0.080f};

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
};

} // namespace ktui
