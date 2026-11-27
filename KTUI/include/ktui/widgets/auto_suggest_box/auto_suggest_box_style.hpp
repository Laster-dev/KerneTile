#pragma once

#include "auto_suggest_box_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>

namespace ktui {

// AutoSuggestBox 解析后视觉样式快照 (Fluent 2 Resolved Visual Tokens)
struct ResolvedAutoSuggestBoxStyle {
    Color background{Color::transparent()};
    Color surface{Color::transparent()};
    Color border{Color::transparent()};
    Color text_primary{Color::white()};
    Color text_secondary{Color::white()};
    Color accent{Color::transparent()};
    Color hover_overlay{Color::transparent()};
    Color active_overlay{Color::transparent()};
    Color focus_ring{Color::transparent()};
    float corner_radius{4.0f};
    float font_size{13.0f};
    EdgeInsets padding{EdgeInsets(4.0f, 8.0f)};
};

// AutoSuggestBox 主题样式解析器 (Style Layer)
class AutoSuggestBoxStyleResolver {
public:
    [[nodiscard]] static ResolvedAutoSuggestBoxStyle resolve(const Theme& theme) noexcept {
        ResolvedAutoSuggestBoxStyle style;
        style.background = theme.colors.background;
        style.surface = theme.colors.surface;
        style.border = theme.colors.border;
        style.text_primary = theme.colors.text_primary;
        style.text_secondary = theme.colors.text_secondary;
        style.accent = theme.colors.primary;
        style.hover_overlay = theme.colors.hover_overlay;
        style.active_overlay = theme.colors.active_overlay;
        style.focus_ring = theme.colors.focus_ring;
        style.corner_radius = theme.radii.md;
        style.font_size = theme.typography.base;
        style.padding = EdgeInsets(theme.spacing.xs, theme.spacing.md);
        return style;
    }
};

} // namespace ktui
