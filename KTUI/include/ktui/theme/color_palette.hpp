#pragma once

#include <ktui/pal/platform_types.hpp>
#include <kernetile/math/color_space.hpp>

namespace ktui {

// 语义化调色盘 (Semantic Color Palette)
// 统一解耦视觉外观与控件逻辑，支持无重排微秒级平滑换肤
struct ColorPalette {
    // ==================== 容器与表面色 ====================
    Color background;           // 窗口主底色
    Color surface;              // 面板、卡片、菜单主背景
    Color surface_variant;      // 悬停条目、内嵌次级面板背景
    Color border;               // 分割线、控件描边
    Color border_subtle;        // 弱分割线、外发光微描边

    // ==================== 品牌与高亮色 ====================
    Color primary;              // 品牌主色 (强调按钮、选中勾选框)
    Color primary_hover;        // 主色悬停态
    Color primary_active;       // 主色按下态
    Color on_primary;           // 位于 primary 之上的前景文本色 (通常为纯白)

    Color secondary;            // 次级强调色
    Color secondary_hover;      // 次级强调色悬停态
    Color on_secondary;         // 次级强调色之上的前景文本色

    // ==================== 文本与图标内容色 ====================
    Color text_primary;         // 主正文、标题 (最高对比度)
    Color text_secondary;       // 辅助文本、小标签 (中对比度)
    Color text_muted;           // 占位符、次要说明 (低对比度)
    Color text_disabled;        // 禁用状态文本

    // ==================== 语义功能反馈色 ====================
    Color success;              // 成功绿色
    Color on_success;           // 成功前景文本色
    Color warning;              // 警告黄色
    Color on_warning;           // 警告前景文本色
    Color error;                // 危险/错误红色
    Color on_error;             // 危险前景文本色
    Color info;                 // 提示蓝色
    Color on_info;              // 提示前景文本色

    // ==================== 交互与焦点环 ====================
    Color hover_overlay;        // 通用悬停微遮罩
    Color active_overlay;       // 通用按下微遮罩
    Color focus_ring;           // 键盘焦点高亮环

    // ==================== WinUI 3 / Fluent 2 语义化控件令牌 ====================
    Color control_fill_default;      // 控件默认填充 (输入框/按钮)
    Color control_fill_secondary;    // 控件悬停填充
    Color control_fill_tertiary;     // 控件按下填充
    Color control_fill_input_active; // 输入框聚焦填充
    Color control_fill_disabled;     // 控件禁用填充
    Color control_stroke_default;    // 控件默认描边
    Color control_stroke_bottom;     // 输入框底部 1px 抬升线
    Color flyout_surface;            // Flyout / Menu / Calendar 浮层底板
    Color flyout_stroke;             // Flyout 浮层 1px 描边
    Color card_surface;              // 卡片容器背景
    Color card_stroke;               // 卡片容器 1px 描边
    Color subtle_fill_secondary;     // 透明列表项/导航项悬停底色
    Color subtle_fill_tertiary;      // 透明列表项/导航项按下底色
    Color divider_stroke;            // 细分隔线

    // OkLab 感知均匀色彩空间调色盘插值 (用于主题切换平滑过渡)
    [[nodiscard]] static ColorPalette lerp(const ColorPalette& a, const ColorPalette& b, float t) noexcept {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        auto mix_c = [t](const Color& c0, const Color& c1) noexcept {
            return kernetile::math::lerp_oklab(c0, c1, t);
        };
        ColorPalette out;
        out.background                = mix_c(a.background, b.background);
        out.surface                   = mix_c(a.surface, b.surface);
        out.surface_variant           = mix_c(a.surface_variant, b.surface_variant);
        out.border                    = mix_c(a.border, b.border);
        out.border_subtle             = mix_c(a.border_subtle, b.border_subtle);
        out.primary                   = mix_c(a.primary, b.primary);
        out.primary_hover             = mix_c(a.primary_hover, b.primary_hover);
        out.primary_active            = mix_c(a.primary_active, b.primary_active);
        out.on_primary                = mix_c(a.on_primary, b.on_primary);
        out.secondary                 = mix_c(a.secondary, b.secondary);
        out.secondary_hover           = mix_c(a.secondary_hover, b.secondary_hover);
        out.on_secondary              = mix_c(a.on_secondary, b.on_secondary);
        out.text_primary              = mix_c(a.text_primary, b.text_primary);
        out.text_secondary            = mix_c(a.text_secondary, b.text_secondary);
        out.text_muted                = mix_c(a.text_muted, b.text_muted);
        out.text_disabled             = mix_c(a.text_disabled, b.text_disabled);
        out.success                   = mix_c(a.success, b.success);
        out.on_success                = mix_c(a.on_success, b.on_success);
        out.warning                   = mix_c(a.warning, b.warning);
        out.on_warning                = mix_c(a.on_warning, b.on_warning);
        out.error                     = mix_c(a.error, b.error);
        out.on_error                  = mix_c(a.on_error, b.on_error);
        out.info                      = mix_c(a.info, b.info);
        out.on_info                   = mix_c(a.on_info, b.on_info);
        out.hover_overlay             = mix_c(a.hover_overlay, b.hover_overlay);
        out.active_overlay            = mix_c(a.active_overlay, b.active_overlay);
        out.focus_ring                = mix_c(a.focus_ring, b.focus_ring);
        out.control_fill_default      = mix_c(a.control_fill_default, b.control_fill_default);
        out.control_fill_secondary    = mix_c(a.control_fill_secondary, b.control_fill_secondary);
        out.control_fill_tertiary     = mix_c(a.control_fill_tertiary, b.control_fill_tertiary);
        out.control_fill_input_active = mix_c(a.control_fill_input_active, b.control_fill_input_active);
        out.control_fill_disabled     = mix_c(a.control_fill_disabled, b.control_fill_disabled);
        out.control_stroke_default    = mix_c(a.control_stroke_default, b.control_stroke_default);
        out.control_stroke_bottom     = mix_c(a.control_stroke_bottom, b.control_stroke_bottom);
        out.flyout_surface            = mix_c(a.flyout_surface, b.flyout_surface);
        out.flyout_stroke             = mix_c(a.flyout_stroke, b.flyout_stroke);
        out.card_surface              = mix_c(a.card_surface, b.card_surface);
        out.card_stroke               = mix_c(a.card_stroke, b.card_stroke);
        out.subtle_fill_secondary     = mix_c(a.subtle_fill_secondary, b.subtle_fill_secondary);
        out.subtle_fill_tertiary      = mix_c(a.subtle_fill_tertiary, b.subtle_fill_tertiary);
        out.divider_stroke            = mix_c(a.divider_stroke, b.divider_stroke);
        return out;
    }

    bool operator==(const ColorPalette&) const noexcept = default;
};

} // namespace ktui
