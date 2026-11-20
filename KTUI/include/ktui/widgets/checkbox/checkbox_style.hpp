#pragma once

#include "checkbox_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <optional>
#include <algorithm>

namespace ktui {

// 复选框解析后的视觉样式快照 (Resolved Visual Tokens)
struct ResolvedCheckboxStyle {
    Color box_background{Color::transparent()};
    Color box_border{Color{1.0f, 1.0f, 1.0f, 0.45f}};
    Color check_mark{Color::white()};
    Color text{Color{0.95f, 0.95f, 0.98f, 1.0f}};
    Color focus_ring{Color{0.0f, 0.525f, 0.941f, 1.0f}};

    float box_size{18.0f};
    float corner_radius{3.0f};
    float border_width{1.2f};      // CUI 规范 1.2f
    float stroke_width{1.9f};      // 对勾笔触厚度 1.9f (完全对齐 CUI)
    float font_size{13.0f};
    float gap{8.0f};
    EdgeInsets padding{EdgeInsets(2.0f, 4.0f)};
};

// 复选框样式解析器 (Style Layer)
// 参照 D:\CUI\CUI.Core\ui 规范将主题、数据与交互状态解析为最终色值
class CheckboxStyleResolver {
public:
    static ResolvedCheckboxStyle resolve(const CheckboxData& data,
                                         const CheckboxInteractiveState& state,
                                         const Theme& theme,
                                         std::optional<Color> custom_bg_normal = std::nullopt,
                                         std::optional<Color> custom_bg_checked = std::nullopt,
                                         std::optional<Color> custom_border = std::nullopt,
                                         std::optional<Color> custom_check_mark = std::nullopt,
                                         std::optional<Color> custom_text = std::nullopt,
                                         std::optional<float> custom_box_size = std::nullopt,
                                         std::optional<float> custom_radius = std::nullopt,
                                         std::optional<float> custom_font_size = std::nullopt) {
        ResolvedCheckboxStyle style;

        style.box_size = custom_box_size ? *custom_box_size : (data.box_size > 0.0f ? data.box_size : 18.0f);
        style.corner_radius = custom_radius ? *custom_radius : (data.corner_radius >= 0.0f ? data.corner_radius : 3.0f);
        style.font_size = custom_font_size ? *custom_font_size : (data.font_size > 0.0f ? data.font_size : 13.0f);
        style.gap = data.gap > 0.0f ? data.gap : 8.0f;
        style.border_width = 1.2f;

        // 根据方框尺寸自适应缩放笔画
        float scale = style.box_size / 18.0f;
        style.stroke_width = std::max(1.2f, 1.9f * scale);

        Color idle_unchecked_bg, hover_unchecked_bg;
        Color idle_unchecked_border, hover_unchecked_border;
        Color checked_bg, checked_hover_bg;
        Color idle_text, disabled_text;

        if (theme.is_dark) {
            // 暗色模式 (CUI Dark Palette)
            idle_unchecked_bg       = Color{0.14f, 0.14f, 0.16f, 0.40f};  // inputBackground
            hover_unchecked_bg      = Color{0.18f, 0.18f, 0.22f, 0.70f};
            idle_unchecked_border   = Color{1.0f, 1.0f, 1.0f, 0.45f};    // 柔和半透明浅边框
            hover_unchecked_border  = Color{1.0f, 1.0f, 1.0f, 0.75f};

            checked_bg              = Color{0.0f, 0.525f, 0.941f, 1.0f};  // #0086F0 AccentColor
            checked_hover_bg        = Color{0.10f, 0.58f, 0.96f, 1.0f};

            style.check_mark        = Color::white();
            idle_text               = Color{0.95f, 0.95f, 0.98f, 1.0f};   // TextPrimary
            disabled_text           = Color{0.50f, 0.50f, 0.53f, 0.60f};   // TextMuted
            style.focus_ring        = Color{0.0f, 0.525f, 0.941f, 1.0f};
        } else {
            // 浅色模式 (CUI Light Palette)
            idle_unchecked_bg       = Color{1.0f, 1.0f, 1.0f, 1.0f};
            hover_unchecked_bg      = Color{0.96f, 0.96f, 0.98f, 1.0f};
            idle_unchecked_border   = Color{0.0f, 0.0f, 0.0f, 0.40f};
            hover_unchecked_border  = Color{0.0f, 0.0f, 0.0f, 0.70f};

            checked_bg              = Color{0.0f, 0.47f, 0.84f, 1.0f};
            checked_hover_bg        = Color{0.08f, 0.54f, 0.92f, 1.0f};

            style.check_mark        = Color::white();
            idle_text               = Color{0.12f, 0.12f, 0.15f, 1.0f};
            disabled_text           = Color{0.65f, 0.65f, 0.70f, 0.65f};
            style.focus_ring        = Color{0.0f, 0.47f, 0.84f, 1.0f};
        }

        // 显式外部参数覆盖
        if (custom_bg_normal) idle_unchecked_bg = *custom_bg_normal;
        if (custom_bg_checked) checked_bg = *custom_bg_checked;
        if (custom_border) idle_unchecked_border = *custom_border;
        if (custom_check_mark) style.check_mark = *custom_check_mark;
        if (custom_text) idle_text = *custom_text;

        bool is_checked_or_ind = (data.state != CheckState::Unchecked);

        if (state.is_disabled) {
            style.box_background = is_checked_or_ind ? Color{checked_bg.r, checked_bg.g, checked_bg.b, 0.40f} : Color{idle_unchecked_bg.r, idle_unchecked_bg.g, idle_unchecked_bg.b, 0.30f};
            style.box_border = is_checked_or_ind ? Color{checked_bg.r, checked_bg.g, checked_bg.b, 0.40f} : Color{idle_unchecked_border.r, idle_unchecked_border.g, idle_unchecked_border.b, 0.20f};
            style.check_mark = Color{style.check_mark.r, style.check_mark.g, style.check_mark.b, 0.45f};
            style.text = disabled_text;
        } else if (state.is_hovered) {
            style.box_background = is_checked_or_ind ? checked_hover_bg : hover_unchecked_bg;
            style.box_border = is_checked_or_ind ? checked_hover_bg : hover_unchecked_border;
            style.text = idle_text;
        } else {
            style.box_background = is_checked_or_ind ? checked_bg : idle_unchecked_bg;
            style.box_border = is_checked_or_ind ? checked_bg : idle_unchecked_border;
            style.text = idle_text;
        }

        return style;
    }
};

} // namespace ktui
