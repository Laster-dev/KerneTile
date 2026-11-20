#pragma once

#include "textbox_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/theme/theme.hpp>
#include <optional>

namespace ktui {

// 文本框完全解析后的视觉样式快照 (Resolved Visual Tokens)
// 参照 D:\CUI\CUI.Core\ui 规范：默认四周零边框 (ApplyBorderThickness(0.0f))，仅保留底部静止底线与动态展开高光线
struct ResolvedTextBoxStyle {
    Color background{Color{0.14f, 0.14f, 0.16f, 1.0f}};
    Color border{Color::transparent()};                // 四周描边默认完全透明 (无包围边框)
    Color underline{Color{1.0f, 1.0f, 1.0f, 0.08f}};   // CUI 底部 1px 静止底线 (ThemeTokenId::InputBorder)
    Color text{Color{0.95f, 0.95f, 0.98f, 1.0f}};
    Color placeholder{Color{0.50f, 0.50f, 0.55f, 1.0f}};
    Color caret{Color{0.0f, 0.525f, 0.941f, 1.0f}};
    Color selection{Color{0.0f, 0.525f, 0.941f, 0.35f}};
    Color focus_indicator{Color{0.0f, 0.525f, 0.941f, 1.0f}}; // Fluent 底部 2px 扩展高光线 (ThemeTokenId::AccentColor)

    float height{28.0f};            // 桌面高信息密度标准高度 28px
    float corner_radius{3.0f};      // 桌面硬朗微圆角 3px (完全匹配 Fluent / CUI)
    float border_width{0.0f};       // 默认 0px 边框 (严格匹配 CUI ApplyBorderThickness(0.0f))
    float underline_height{1.0f};   // 底部静止底线厚度 1px
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
        style.border_width = 0.0f;       // 默认 0px 边框 (严格看齐 CUI ApplyBorderThickness(0.0f))
        style.underline_height = 1.0f;
        style.focus_line_height = 2.0f;
        style.padding = EdgeInsets(4.0f, 8.0f);
        style.font_size = data.font_size > 0.0f ? data.font_size : 13.0f;
        style.caret_width = 1.5f;
        style.caret_blink_ms = 530;

        Color idle_bg, hover_bg, focus_bg, disabled_bg;
        Color idle_text, disabled_text;

        if (theme.is_dark) {
            // 暗色模式 (Fluent Dark Palette)
            idle_bg         = Color{0.14f, 0.14f, 0.16f, 1.0f};   // #242428 深色卡片表面
            hover_bg        = Color{0.18f, 0.18f, 0.21f, 1.0f};   // 微弱提亮
            focus_bg        = Color{0.11f, 0.11f, 0.13f, 1.0f};   // 聚焦微深下沉
            disabled_bg     = Color{0.12f, 0.12f, 0.14f, 0.50f};

            style.border           = Color::transparent();                // 默认绝无四周描边
            style.underline        = Color{1.0f, 1.0f, 1.0f, 0.08f};     // CUI Dark InputBorder (8% 白色半透底线)
            style.focus_indicator  = Color{0.0f, 0.525f, 0.941f, 1.0f};  // Fluent 底部活动高亮线 (#0086F0)

            idle_text       = Color{0.95f, 0.95f, 0.98f, 1.0f};  // TextPrimary
            disabled_text   = Color{0.50f, 0.50f, 0.53f, 0.60f};  // TextMuted
            style.placeholder = Color{0.50f, 0.50f, 0.55f, 1.0f};

            style.caret            = Color{0.0f, 0.525f, 0.941f, 1.0f};  // #0086F0 AccentColor
            style.selection        = Color{0.0f, 0.525f, 0.941f, 0.35f}; // 半透明选区
        } else {
            // 浅色模式 (Fluent Light Palette)
            idle_bg         = Color{0.98f, 0.98f, 0.99f, 1.0f};
            hover_bg        = Color{1.0f, 1.0f, 1.0f, 1.0f};
            focus_bg        = Color{1.0f, 1.0f, 1.0f, 1.0f};
            disabled_bg     = Color{0.94f, 0.94f, 0.95f, 0.60f};

            style.border           = Color::transparent();                // 默认绝无四周描边
            style.underline        = Color{0.0f, 0.0f, 0.0f, 0.10f};     // CUI Light InputBorder (10% 黑色半透底线)
            style.focus_indicator  = Color{0.0f, 0.47f, 0.84f, 1.0f};

            idle_text       = Color{0.12f, 0.12f, 0.15f, 1.0f};
            disabled_text   = Color{0.65f, 0.65f, 0.70f, 0.65f};
            style.placeholder = Color{0.52f, 0.52f, 0.56f, 1.0f};

            style.caret            = Color{0.0f, 0.47f, 0.84f, 1.0f};
            style.selection        = Color{0.0f, 0.47f, 0.84f, 0.28f};
        }

        // 覆盖自定义显式参数 (若外部显式传入边框，才启用四周描边)
        if (custom_bg) idle_bg = *custom_bg;
        if (custom_border) {
            style.border = *custom_border;
            style.border_width = 1.0f;
        }
        if (custom_focused_border) {
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
            style.text = disabled_text;
            style.underline = Color::transparent();
        } else if (state.is_focused) {
            style.background = focus_bg;
            style.text = idle_text;
            // 聚焦时依然没有四周蓝框！聚焦指示 100% 由底部 Fluent 动态展开线承担
        } else if (state.is_hovered) {
            style.background = hover_bg;
            style.text = idle_text;
            if (theme.is_dark) {
                style.underline = Color{1.0f, 1.0f, 1.0f, 0.14f}; // 悬停底线微弱增强
            } else {
                style.underline = Color{0.0f, 0.0f, 0.0f, 0.16f};
            }
        } else {
            style.background = idle_bg;
            style.text = idle_text;
        }

        return style;
    }
};

} // namespace ktui
