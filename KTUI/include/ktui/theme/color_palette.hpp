#pragma once

#include <ktui/pal/platform_types.hpp>

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

    bool operator==(const ColorPalette&) const noexcept = default;
};

} // namespace ktui
