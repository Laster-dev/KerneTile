#pragma once

#include "textbox_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <optional>

namespace ktui {

// 文本框完全解析后的视觉样式快照 (Resolved Visual Tokens)
struct ResolvedTextBoxStyle {
    Color background{Color{0.14f, 0.14f, 0.16f, 1.0f}};
    Color border{Color{0.25f, 0.25f, 0.28f, 0.8f}};
    Color text{Color{0.95f, 0.95f, 0.98f, 1.0f}};
    Color placeholder{Color{0.50f, 0.50f, 0.55f, 1.0f}};
    Color caret{Color{0.0f, 0.525f, 0.941f, 1.0f}};
    Color selection{Color{0.0f, 0.525f, 0.941f, 0.35f}};
    Color focus_border{Color{0.15f, 0.60f, 0.98f, 0.5f}};
    Color focus_indicator{Color{0.0f, 0.525f, 0.941f, 1.0f}}; // Fluent 底部 2px 扩展高光线

    float height{28.0f};            // 桌面高信息密度标准高度 28px
    float corner_radius{3.0f};      // 桌面硬朗微圆角 3px (完全匹配 Fluent / CUI)
    float border_width{1.0f};       // 1px 细线边框
    float focus_line_height{2.0f};  // 底部活动高亮线厚度 2px
    EdgeInsets padding{EdgeInsets(4.0f, 8.0f)};
    float font_size{13.0f};         // 桌面 IDE 基准 13px
    float caret_width{1.5f};        // 1.5px 精致光标
    uint32_t caret_blink_ms{530};   // 530ms 工业标准光标闪烁周期
};

// 文本框样式解析器 (TextBox Style Layer)
// 参照 D:\CUI\CUI.Core\ui 规范，将主题、数据和交互状态解析为最终渲染色值
class TextBoxStyleResolver {
public:
    static ResolvedTextBoxStyle resolve(const TextBoxData& data,
                                        const TextBoxInteractiveState& state,
                                        const Theme& theme,
                                        std::optional<Color> custom_bg = std::nullopt,
                                        std::optional<Color> custom_border = std::nullopt,
                                        std::optional<Color> custom_focused_border = std::nullopt,
                                        std::optional<Color> custom_text = std::nullopt,
                                        std::optional<Color> custom_placeholder = std::nullopt,
                                        std::optional<float> custom_height = std::nullopt,
                                        std::optional<float> custom_radius = std::nullopt,
                                        std::optional<EdgeInsets> custom_padding = std::nullopt,
                                        std::optional<float> custom_font_size = std::nullopt) {
        ResolvedTextBoxStyle style;

        style.height = 28.0f;
        style.corner_radius = 3.0f;
        style.border_width = 1.0f;
        style.focus_line_height = 2.0f;
        style.padding = EdgeInsets(4.0f, 8.0f);
        style.font_size = data.font_size > 0.0f ? data.font_size : 13.0f;
        style.caret_width = 1.5f;
        style.caret_blink_ms = 530;

        Color idle_bg, hover_bg, focus_bg, disabled_bg;
        Color idle_border, hover_border, focus_border, disabled_border;
        Color idle_text, disabled_text;

        if (theme.is_dark) {
            // 暗色模式 (Fluent Dark Palette)
            idle_bg         = Color{0.14f, 0.14f, 0.16f, 1.0f};   // #242428 深色卡片表面
            hover_bg        = Color{0.18f, 0.18f, 0.21f, 1.0f};   // 微弱提亮
            focus_bg        = Color{0.11f, 0.11f, 0.13f, 1.0f};   // 聚焦微深下沉
            disabled_bg     = Color{0.12f, 0.12f, 0.14f, 0.50f};

            idle_border     = Color{0.25f, 0.25f, 0.28f, 0.80f};  // CUI InputBorder
            hover_border    = Color{0.36f, 0.36f, 0.40f, 0.90f};
            focus_border    = Color{0.15f, 0.60f, 0.98f, 0.50f};
            disabled_border = Color{1.0f, 1.0f, 1.0f, 0.04f};

            idle_text       = Color{0.95f, 0.95f, 0.98f, 1.0f};  // TextPrimary
            disabled_text   = Color{0.50f, 0.50f, 0.53f, 0.60f};  // TextMuted
            style.placeholder = Color{0.50f, 0.50f, 0.55f, 1.0f};

            style.caret            = Color{0.0f, 0.525f, 0.941f, 1.0f};  // #0086F0 AccentColor
            style.selection        = Color{0.0f, 0.525f, 0.941f, 0.35f}; // 半透明选区
            style.focus_indicator  = Color{0.0f, 0.525f, 0.941f, 1.0f};  // Fluent 底部活动高亮线
        } else {
            // 浅色模式 (Fluent Light Palette)
            idle_bg         = Color{0.98f, 0.98f, 0.99f, 1.0f};
            hover_bg        = Color{1.0f, 1.0f, 1.0f, 1.0f};
            focus_bg        = Color{1.0f, 1.0f, 1.0f, 1.0f};
            disabled_bg     = Color{0.94f, 0.94f, 0.95f, 0.60f};

            idle_border     = Color{0.0f, 0.0f, 0.0f, 0.16f};
            hover_border    = Color{0.0f, 0.0f, 0.0f, 0.26f};
            focus_border    = Color{0.0f, 0.47f, 0.84f, 0.70f};
            disabled_border = Color{0.0f, 0.0f, 0.0f, 0.06f};

            idle_text       = Color{0.12f, 0.12f, 0.15f, 1.0f};
            disabled_text   = Color{0.65f, 0.65f, 0.70f, 0.65f};
            style.placeholder = Color{0.52f, 0.52f, 0.56f, 1.0f};

            style.caret            = Color{0.0f, 0.47f, 0.84f, 1.0f};
            style.selection        = Color{0.0f, 0.47f, 0.84f, 0.28f};
            style.focus_indicator  = Color{0.0f, 0.47f, 0.84f, 1.0f};
        }

        // 覆盖自定义显式参数
        if (custom_bg) idle_bg = *custom_bg;
        if (custom_border) idle_border = *custom_border;
        if (custom_focused_border) {
            focus_border = *custom_focused_border;
            style.focus_indicator = *custom_focused_border;
            style.caret = *custom_focused_border;
        }
        if (custom_text) idle_text = *custom_text;
        if (custom_placeholder) style.placeholder = *custom_placeholder;
        if (custom_height) style.height = *custom_height;
        if (custom_radius) style.corner_radius = *custom_radius;
        if (custom_padding) style.padding = *custom_padding;
        if (custom_font_size) style.font_size = *custom_font_size;

        // 根据状态选取最终颜色
        if (state.is_disabled) {
            style.background = disabled_bg;
            style.border = disabled_border;
            style.text = disabled_text;
        } else if (state.is_focused) {
            style.background = focus_bg;
            style.border = focus_border;
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
