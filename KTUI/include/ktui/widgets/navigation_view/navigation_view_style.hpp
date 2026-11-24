#pragma once

#include "navigation_view_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/theme/theme.hpp>
#include <cmath>

namespace ktui {

// WinUI 3 NavigationView 样式快照 (严格对齐 Windows 11 Settings / WinUI 3 Gallery)
struct ResolvedNavigationViewStyle {
    Color pane_bg;               // 侧边栏背景 (展开嵌入态透明透出 Mica，浮层态为实色/亚克力面板)
    Color pane_overlay_bg;       // 浮层滑出态侧边栏底色
    Color pane_overlay_border;
    Color pane_overlay_shadow;
    Color content_surface_bg;    // 右侧内容区 LayerFillColorDefault 圆角卡片底色
    Color content_surface_border;// 内容区顶部/左侧 1px 柔和分割描边

    Color item_hover_bg;         // SubtleFillColorSecondary
    Color item_pressed_bg;       // SubtleFillColorTertiary
    Color item_selected_bg;      // SubtleFillColorSecondary
    Color item_text;
    Color item_selected_text;
    Color item_disabled_text;
    Color icon_color;
    Color icon_selected_color;
    Color header_text;           // 分组标题色
    Color separator_color;       // 分割线色
    Color accent_pill;           // WinUI 3 标志性 3px 强调色滑动选中条
    Color badge_bg;              // InfoBadge 徽标背景
    Color badge_text;            // InfoBadge 徽标文字
    Color page_header_text;      // 右侧页面大标题色

    float item_height{36.0f};    // WinUI 3 NavigationViewItem 高 36px
    float top_bar_height{44.0f}; // Top 模式水平导航栏高度 44px
    float item_radius{4.0f};     // 4px 圆角
    float content_radius{8.0f};  // 内容区左上角 8px 圆角
    float font_size{13.0f};
    float header_font_size{11.5f};
    float page_header_font_size{22.0f};
};

class NavigationViewStyleResolver {
public:
    static ResolvedNavigationViewStyle resolve(const Theme& theme) {
        ResolvedNavigationViewStyle s;
        if (theme.is_dark) {
            s.pane_bg                = Color::transparent();
            s.pane_overlay_bg        = Color{38.0f / 255.0f, 38.0f / 255.0f, 40.0f / 255.0f, 0.98f};
            s.pane_overlay_border    = Color{1.0f, 1.0f, 1.0f, 0.09f};
            s.pane_overlay_shadow    = Color{0.0f, 0.0f, 0.0f, 0.42f};
            s.content_surface_bg     = Color{1.0f, 1.0f, 1.0f, 0.032f};
            s.content_surface_border = Color{1.0f, 1.0f, 1.0f, 0.065f};

            s.item_hover_bg          = Color{1.0f, 1.0f, 1.0f, 0.0605f};
            s.item_pressed_bg        = Color{1.0f, 1.0f, 1.0f, 0.0419f};
            s.item_selected_bg       = Color{1.0f, 1.0f, 1.0f, 0.0605f};
            s.item_text              = Color{1.0f, 1.0f, 1.0f, 0.92f};
            s.item_selected_text     = Color{1.0f, 1.0f, 1.0f, 1.0f};
            s.item_disabled_text     = Color{1.0f, 1.0f, 1.0f, 0.36f};
            s.icon_color             = Color{1.0f, 1.0f, 1.0f, 0.85f};
            s.icon_selected_color    = Color{1.0f, 1.0f, 1.0f, 1.0f};
            s.header_text            = Color{1.0f, 1.0f, 1.0f, 0.56f};
            s.separator_color        = Color{1.0f, 1.0f, 1.0f, 0.08f};
            s.accent_pill            = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}; // #60CDFF
            s.badge_bg               = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};
            s.badge_text             = Color{0.05f, 0.08f, 0.12f, 1.0f};
            s.page_header_text       = Color{1.0f, 1.0f, 1.0f, 0.96f};
        } else {
            s.pane_bg                = Color::transparent();
            s.pane_overlay_bg        = Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.985f};
            s.pane_overlay_border    = Color{0.0f, 0.0f, 0.0f, 0.09f};
            s.pane_overlay_shadow    = Color{0.0f, 0.0f, 0.0f, 0.16f};
            s.content_surface_bg     = Color{1.0f, 1.0f, 1.0f, 0.55f};
            s.content_surface_border = Color{0.0f, 0.0f, 0.0f, 0.065f};

            s.item_hover_bg          = Color{0.0f, 0.0f, 0.0f, 0.038f};
            s.item_pressed_bg        = Color{0.0f, 0.0f, 0.0f, 0.025f};
            s.item_selected_bg       = Color{0.0f, 0.0f, 0.0f, 0.042f};
            s.item_text              = Color{0.0f, 0.0f, 0.0f, 0.88f};
            s.item_selected_text     = Color{0.0f, 0.0f, 0.0f, 0.96f};
            s.item_disabled_text     = Color{0.0f, 0.0f, 0.0f, 0.36f};
            s.icon_color             = Color{0.0f, 0.0f, 0.0f, 0.78f};
            s.icon_selected_color    = Color{0.0f, 0.0f, 0.0f, 0.95f};
            s.header_text            = Color{0.0f, 0.0f, 0.0f, 0.54f};
            s.separator_color        = Color{0.0f, 0.0f, 0.0f, 0.08f};
            s.accent_pill            = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};            // #005FB8
            s.badge_bg               = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
            s.badge_text             = Color::white();
            s.page_header_text       = Color{0.0f, 0.0f, 0.0f, 0.92f};
        }
        return s;
    }

    // 使用 NanoSVG 渲染 WinUI 3 Fluent 24x24 设置齿轮 SVG (支持旋转角 angle_rad 与按压缩放)
    static void paint_settings_gear(PaintContext& ctx, Rect r, Color col, float angle_rad = 0.0f, float scale = 1.0f) {
        if (col.a <= 0.001f) return;
        // Microsoft Fluent UI System Icons (Settings 24 Regular) SVG Path
        static constexpr const char* kSettingsSvgPath =
            "M12.012 2.25c.734.008 1.465.093 2.182.253a.75.75 0 0 1 .582.649l.17 1.527a1.384 1.384 0 0 0 1.927 1.116l1.401-.615a.75.75 0 0 1 .85.174 9.792 9.792 0 0 1 2.204 3.792.75.75 0 0 1-.271.825l-1.242.916a1.381 1.381 0 0 0 0 2.226l1.243.915a.75.75 0 0 1 .272.826 9.797 9.797 0 0 1-2.204 3.792.75.75 0 0 1-.848.175l-1.407-.617a1.38 1.38 0 0 0-1.926 1.114l-.169 1.526a.75.75 0 0 1-.572.647 9.518 9.518 0 0 1-4.406 0 .75.75 0 0 1-.572-.647l-.168-1.524a1.382 1.382 0 0 0-1.926-1.11l-1.406.616a.75.75 0 0 1-.849-.175 9.798 9.798 0 0 1-2.204-3.796.75.75 0 0 1 .272-.826l1.243-.916a1.38 1.38 0 0 0 0-2.226l-1.243-.914a.75.75 0 0 1-.271-.826 9.793 9.793 0 0 1 2.204-3.792.75.75 0 0 1 .85-.174l1.4.615a1.387 1.387 0 0 0 1.93-1.118l.17-1.526a.75.75 0 0 1 .583-.65c.717-.159 1.45-.243 2.201-.252Zm0 1.5a9.135 9.135 0 0 0-1.354.117l-.109.977A2.886 2.886 0 0 1 6.525 7.17l-.898-.394a8.293 8.293 0 0 0-1.348 2.317l.798.587a2.881 2.881 0 0 1 0 4.643l-.799.588c.32.842.776 1.626 1.348 2.322l.905-.397a2.882 2.882 0 0 1 4.017 2.318l.11.984c.889.15 1.798.15 2.687 0l.11-.984a2.881 2.881 0 0 1 4.018-2.322l.905.396a8.296 8.296 0 0 0 1.347-2.318l-.798-.588a2.881 2.881 0 0 1 0-4.643l.796-.587a8.293 8.293 0 0 0-1.348-2.317l-.896.393a2.884 2.884 0 0 1-4.023-2.324l-.11-.976a8.988 8.988 0 0 0-1.333-.117ZM12 8.25a3.75 3.75 0 1 1 0 7.5 3.75 3.75 0 0 1 0-7.5Zm0 1.5a2.25 2.25 0 1 0 0 4.5 2.25 2.25 0 0 0 0-4.5Z";

        Rect dst = r;
        if (std::abs(scale - 1.0f) > 0.005f) {
            float cx = r.min_x + r.width() * 0.5f;
            float cy = r.min_y + r.height() * 0.5f;
            float hw = r.width() * 0.5f * scale;
            float hh = r.height() * 0.5f * scale;
            dst = Rect::from_xywh(cx - hw, cy - hh, hw * 2.0f, hh * 2.0f);
        }
        SvgEngine::draw_svg_path(ctx, kSettingsSvgPath, 24.0f, 24.0f, dst, col, 0.0f, true, angle_rad);
    }

    // 使用 NanoSVG 渲染内置 Fluent UI System Icons 24x24 矢量图标
    static void paint_builtin_icon(PaintContext& ctx, NavigationIcon icon, Rect r, Color col, float angle_rad = 0.0f) {
        if (icon == NavigationIcon::None || col.a <= 0.001f) return;
        if (icon == NavigationIcon::Settings) {
            paint_settings_gear(ctx, r, col, angle_rad, 1.0f);
            return;
        }

        const char* path_d = nullptr;
        switch (icon) {
            case NavigationIcon::Home:
                path_d = "M10.55 2.533a2.25 2.25 0 0 1 2.9 0l6.75 5.695c.508.427.8 1.056.8 1.72v9.802a1.75 1.75 0 0 1-1.75 1.75h-3a1.75 1.75 0 0 1-1.75-1.75v-5a.75.75 0 0 0-.75-.75h-3.5a.75.75 0 0 0-.75.75v5a1.75 1.75 0 0 1-1.75 1.75h-3A1.75 1.75 0 0 1 3 19.75V9.947c0-.663.292-1.292.8-1.72l6.75-5.694Zm1.933 1.147a.75.75 0 0 0-.966 0l-6.75 5.695a.75.75 0 0 0-.267.573v9.802c0 .138.112.25.25.25h3a.25.25 0 0 0 .25-.25v-5a2.25 2.25 0 0 1 2.25-2.25h3.5a2.25 2.25 0 0 1 2.25 2.25v5c0 .138.112.25.25.25h3a.25.25 0 0 0 .25-.25V9.948a.75.75 0 0 0-.267-.573l-6.75-5.695Z";
                break;
            case NavigationIcon::Controls:
                path_d = "M5.25 3A2.25 2.25 0 0 0 3 5.25v4.5A2.25 2.25 0 0 0 5.25 12h4.5A2.25 2.25 0 0 0 12 9.75v-4.5A2.25 2.25 0 0 0 9.75 3h-4.5Zm-.75 2.25a.75.75 0 0 1 .75-.75h4.5a.75.75 0 0 1 .75.75v4.5a.75.75 0 0 1-.75.75h-4.5a.75.75 0 0 1-.75-.75v-4.5ZM14.25 3A2.25 2.25 0 0 0 12 5.25v4.5A2.25 2.25 0 0 0 14.25 12h4.5A2.25 2.25 0 0 0 21 9.75v-4.5A2.25 2.25 0 0 0 18.75 3h-4.5Zm-.75 2.25a.75.75 0 0 1 .75-.75h4.5a.75.75 0 0 1 .75.75v4.5a.75.75 0 0 1-.75.75h-4.5a.75.75 0 0 1-.75-.75v-4.5ZM5.25 12A2.25 2.25 0 0 0 3 14.25v4.5A2.25 2.25 0 0 0 5.25 21h4.5A2.25 2.25 0 0 0 12 18.75v-4.5A2.25 2.25 0 0 0 9.75 12h-4.5Zm-.75 2.25a.75.75 0 0 1 .75-.75h4.5a.75.75 0 0 1 .75.75v4.5a.75.75 0 0 1-.75.75h-4.5a.75.75 0 0 1-.75-.75v-4.5ZM14.25 12A2.25 2.25 0 0 0 12 14.25v4.5A2.25 2.25 0 0 0 14.25 21h4.5A2.25 2.25 0 0 0 21 18.75v-4.5A2.25 2.25 0 0 0 18.75 12h-4.5Zm-.75 2.25a.75.75 0 0 1 .75-.75h4.5a.75.75 0 0 1 .75.75v4.5a.75.75 0 0 1-.75.75h-4.5a.75.75 0 0 1-.75-.75v-4.5Z";
                break;
            case NavigationIcon::Inputs:
                path_d = "M4.75 5A2.75 2.75 0 0 0 2 7.75v8.5A2.75 2.75 0 0 0 4.75 19h14.5A2.75 2.75 0 0 0 22 16.25v-8.5A2.75 2.75 0 0 0 19.25 5H4.75ZM3.5 7.75c0-.69.56-1.25 1.25-1.25h14.5c.69 0 1.25.56 1.25 1.25v8.5c0 .69-.56 1.25-1.25 1.25H4.75c-.69 0-1.25-.56-1.25-1.25v-8.5ZM6.75 9a.75.75 0 0 0 0 1.5h6.5a.75.75 0 0 0 0-1.5h-6.5Zm0 4.5a.75.75 0 0 0 0 1.5h4.5a.75.75 0 0 0 0-1.5h-4.5ZM16.5 9a.75.75 0 0 1 .75.75v4.5a.75.75 0 0 1-1.5 0v-4.5A.75.75 0 0 1 16.5 9Z";
                break;
            case NavigationIcon::Progress:
                path_d = "M3 8a.75.75 0 0 1 .75-.75h5.34a3.251 3.251 0 0 1 5.82 0h5.34a.75.75 0 0 1 0 1.5h-5.34a3.251 3.251 0 0 1-5.82 0H3.75A.75.75 0 0 1 3 8Zm9 1.75a1.75 1.75 0 1 0 0-3.5 1.75 1.75 0 0 0 0 3.5ZM3.75 15.25a.75.75 0 0 0 0 1.5h2.34a3.251 3.251 0 0 0 5.82 0h8.34a.75.75 0 0 0 0-1.5h-8.34a3.251 3.251 0 0 0-5.82 0H3.75ZM9 17.75a1.75 1.75 0 1 1 0-3.5 1.75 1.75 0 0 1 0 3.5Z";
                break;
            case NavigationIcon::ComboBox:
                path_d = "M5.25 4A2.25 2.25 0 0 0 3 6.25v11.5A2.25 2.25 0 0 0 5.25 20h13.5A2.25 2.25 0 0 0 21 17.75V6.25A2.25 2.25 0 0 0 18.75 4H5.25Zm-.75 2.25a.75.75 0 0 1 .75-.75h13.5a.75.75 0 0 1 .75.75v11.5a.75.75 0 0 1-.75.75H5.25a.75.75 0 0 1-.75-.75V6.25Zm3.25 3a.75.75 0 0 0 0 1.5h5.5a.75.75 0 0 0 0-1.5h-5.5Zm0 4a.75.75 0 0 0 0 1.5h5.5a.75.75 0 0 0 0-1.5h-5.5Zm8.03-2.53a.75.75 0 0 1 1.06 0l.97.97a.75.75 0 0 1 0 1.06l-.97.97a.75.75 0 1 1-1.06-1.06l.44-.44-.44-.44a.75.75 0 0 1 0-1.06Z";
                break;
            case NavigationIcon::Layout:
                path_d = "M5.25 4A2.25 2.25 0 0 0 3 6.25v11.5A2.25 2.25 0 0 0 5.25 20h13.5A2.25 2.25 0 0 0 21 17.75V6.25A2.25 2.25 0 0 0 18.75 4H5.25Zm-.75 2.25a.75.75 0 0 1 .75-.75H8.5v13H5.25a.75.75 0 0 1-.75-.75V6.25Zm5.5 12.25v-13h8.75a.75.75 0 0 1 .75.75v11.5a.75.75 0 0 1-.75.75H10Z";
                break;
            case NavigationIcon::Palette:
                path_d = "M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10c1.38 0 2.5-1.12 2.5-2.5 0-.61-.23-1.17-.6-1.6-.36-.42-.58-.96-.58-1.55 0-1.24 1.01-2.25 2.25-2.25H17c2.76 0 5-2.24 5-5 0-3.87-4.48-7.1-10-7.1Zm-5.5 9a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3Zm3-4a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3Zm5 0a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3Zm3 4a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3Z";
                break;
            case NavigationIcon::Info:
                path_d = "M12 2c5.523 0 10 4.477 10 10s-4.477 10-10 10S2 17.523 2 12 6.477 2 12 2Zm0 1.5a8.5 8.5 0 1 0 0 17 8.5 8.5 0 0 0 0-17Zm0 6.75a.75.75 0 0 1 .75.75v5.5a.75.75 0 0 1-1.5 0V11a.75.75 0 0 1 .75-.75Zm0-3.25a1 1 0 1 1 0 2 1 1 0 0 1 0-2Z";
                break;
            case NavigationIcon::Star:
                path_d = "M10.788 3.103c.495-1.004 1.926-1.004 2.421 0l2.358 4.777 5.273.766c1.107.161 1.549 1.522.748 2.303l-3.816 3.72.901 5.25c.19 1.103-.968 1.944-1.959 1.424l-4.716-2.48-4.715 2.48c-.99.52-2.148-.32-1.96-1.424l.901-5.25-3.815-3.72c-.801-.78-.359-2.142.748-2.303L8.43 7.88l2.358-4.777Z";
                break;
            case NavigationIcon::Search:
                path_d = "M10 2.5a7.5 7.5 0 0 1 5.964 12.05l4.743 4.743a.75.75 0 0 1-1.06 1.06l-4.743-4.742A7.5 7.5 0 1 1 10 2.5Zm0 1.5a6 6 0 1 0 0 12 6 6 0 0 0 0-12Z";
                break;
            case NavigationIcon::Folder:
                path_d = "M4.25 4A2.25 2.25 0 0 0 2 6.25v11.5A2.25 2.25 0 0 0 4.25 20h15.5A2.25 2.25 0 0 0 22 17.75V8.75A2.25 2.25 0 0 0 19.75 6.5h-7.03l-1.9-1.9A2.25 2.25 0 0 0 9.23 4H4.25Zm-.75 2.25a.75.75 0 0 1 .75-.75h4.98c.2 0 .39.08.53.22L12.1 8h7.65a.75.75 0 0 1 .75.75v9a.75.75 0 0 1-.75.75H4.25a.75.75 0 0 1-.75-.75V6.25Z";
                break;
            case NavigationIcon::Code:
                path_d = "M8.03 7.47a.75.75 0 0 1 0 1.06L4.56 12l3.47 3.47a.75.75 0 1 1-1.06 1.06l-4-4a.75.75 0 0 1 0-1.06l4-4a.75.75 0 0 1 1.06 0Zm7.94 0a.75.75 0 0 1 1.06 0l4 4a.75.75 0 0 1 0 1.06l-4 4a.75.75 0 1 1-1.06-1.06L19.44 12l-3.47-3.47a.75.75 0 0 1 0-1.06Zm-2.55-2.2a.75.75 0 0 1 .55.9l-2.5 10a.75.75 0 1 1-1.45-.36l2.5-10a.75.75 0 0 1 .9-.54Z";
                break;
            case NavigationIcon::Sparkles:
                path_d = "M12 2.5a.75.75 0 0 1 .71.51l1.62 4.86 4.86 1.62a.75.75 0 0 1 0 1.42l-4.86 1.62-1.62 4.86a.75.75 0 0 1-1.42 0l-1.62-4.86-4.86-1.62a.75.75 0 0 1 0-1.42l4.86-1.62 1.62-4.86A.75.75 0 0 1 12 2.5Zm0 3.12-1.06 3.18a.75.75 0 0 1-.47.47L7.29 10.33l3.18 1.06a.75.75 0 0 1 .47.47L12 15.04l1.06-3.18a.75.75 0 0 1 .47-.47l3.18-1.06-3.18-1.06a.75.75 0 0 1-.47-.47L12 5.62Z";
                break;
            default:
                break;
        }

        if (path_d) {
            SvgEngine::draw_svg_path(ctx, path_d, 24.0f, 24.0f, r, col, 0.0f, true, angle_rad);
        }
    }
};

} // namespace ktui
