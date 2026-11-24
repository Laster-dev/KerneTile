#pragma once

#include "navigation_view_data.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
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

    // 绘制内置 WinUI 3 Fluent 矢量线性图标 (16x16 居中)
    static void paint_builtin_icon(PaintContext& ctx, NavigationIcon icon, Rect r, Color col) {
        if (icon == NavigationIcon::None || col.a <= 0.001f) return;
        const float cx = r.min_x + r.width() * 0.5f;
        const float cy = r.min_y + r.height() * 0.5f;
        constexpr float sw = 1.25f;

        switch (icon) {
            case NavigationIcon::Home:
                ctx.draw_line(PointF{cx - 6.0f, cy - 0.5f}, PointF{cx, cy - 5.5f}, col, sw);
                ctx.draw_line(PointF{cx, cy - 5.5f}, PointF{cx + 6.0f, cy - 0.5f}, col, sw);
                ctx.draw_line(PointF{cx - 4.2f, cy - 1.5f}, PointF{cx - 4.2f, cy + 5.2f}, col, sw);
                ctx.draw_line(PointF{cx + 4.2f, cy - 1.5f}, PointF{cx + 4.2f, cy + 5.2f}, col, sw);
                ctx.draw_line(PointF{cx - 4.2f, cy + 5.2f}, PointF{cx + 4.2f, cy + 5.2f}, col, sw);
                break;
            case NavigationIcon::Controls:
                ctx.draw_rect(Rect::from_xywh(cx - 5.5f, cy - 5.5f, 4.8f, 4.8f), col, 1.2f, sw);
                ctx.draw_rect(Rect::from_xywh(cx + 0.7f, cy - 5.5f, 4.8f, 4.8f), col, 1.2f, sw);
                ctx.draw_rect(Rect::from_xywh(cx - 5.5f, cy + 0.7f, 4.8f, 4.8f), col, 1.2f, sw);
                ctx.draw_rect(Rect::from_xywh(cx + 0.7f, cy + 0.7f, 4.8f, 4.8f), col, 1.2f, sw);
                break;
            case NavigationIcon::Inputs:
                ctx.draw_rect(Rect::from_xywh(cx - 6.0f, cy - 4.2f, 12.0f, 8.4f), col, 2.0f, sw);
                ctx.draw_line(PointF{cx - 3.2f, cy}, PointF{cx + 1.2f, cy}, col, sw);
                ctx.draw_line(PointF{cx + 2.8f, cy - 2.0f}, PointF{cx + 2.8f, cy + 2.0f}, col, sw);
                break;
            case NavigationIcon::Progress:
                ctx.draw_line(PointF{cx - 6.0f, cy - 2.8f}, PointF{cx + 6.0f, cy - 2.8f}, col, sw);
                ctx.draw_rect(Rect::from_xywh(cx - 1.0f, cy - 4.8f, 4.0f, 4.0f), col, 2.0f, 0.0f);
                ctx.draw_line(PointF{cx - 6.0f, cy + 3.0f}, PointF{cx + 6.0f, cy + 3.0f}, col, sw);
                ctx.draw_rect(Rect::from_xywh(cx - 4.0f, cy + 1.0f, 4.0f, 4.0f), col, 2.0f, 0.0f);
                break;
            case NavigationIcon::ComboBox:
                ctx.draw_rect(Rect::from_xywh(cx - 6.0f, cy - 4.8f, 12.0f, 9.6f), col, 2.0f, sw);
                ctx.draw_line(PointF{cx - 3.5f, cy - 1.2f}, PointF{cx + 0.5f, cy - 1.2f}, col, sw);
                ctx.draw_line(PointF{cx + 1.8f, cy - 1.2f}, PointF{cx + 3.5f, cy + 0.8f}, col, sw);
                ctx.draw_line(PointF{cx + 3.5f, cy + 0.8f}, PointF{cx + 5.0f, cy - 1.2f}, col, sw);
                break;
            case NavigationIcon::Layout:
                ctx.draw_rect(Rect::from_xywh(cx - 6.0f, cy - 5.2f, 12.0f, 10.4f), col, 1.8f, sw);
                ctx.draw_line(PointF{cx - 2.0f, cy - 5.2f}, PointF{cx - 2.0f, cy + 5.2f}, col, sw);
                break;
            case NavigationIcon::Palette:
                ctx.draw_rect(Rect::from_xywh(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f), col, 5.5f, sw);
                ctx.draw_rect(Rect::from_xywh(cx - 2.5f, cy - 2.5f, 2.2f, 2.2f), col, 1.1f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(cx + 0.8f, cy - 2.5f, 2.2f, 2.2f), col, 1.1f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(cx - 2.5f, cy + 0.8f, 2.2f, 2.2f), col, 1.1f, 0.0f);
                break;
            case NavigationIcon::Settings: {
                // WinUI 3 齿轮外圈 + 内孔 + 6 齿
                ctx.draw_rect(Rect::from_xywh(cx - 4.2f, cy - 4.2f, 8.4f, 8.4f), col, 4.2f, sw);
                ctx.draw_rect(Rect::from_xywh(cx - 1.8f, cy - 1.8f, 3.6f, 3.6f), col, 1.8f, sw);
                for (int i = 0; i < 6; ++i) {
                    float a = static_cast<float>(i) * (3.14159265f / 3.0f);
                    float x1 = cx + std::cos(a) * 4.2f;
                    float y1 = cy + std::sin(a) * 4.2f;
                    float x2 = cx + std::cos(a) * 6.0f;
                    float y2 = cy + std::sin(a) * 6.0f;
                    ctx.draw_line(PointF{x1, y1}, PointF{x2, y2}, col, 1.4f);
                }
                break;
            }
            case NavigationIcon::Info:
                ctx.draw_rect(Rect::from_xywh(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f), col, 5.5f, sw);
                ctx.draw_rect(Rect::from_xywh(cx - 0.8f, cy - 3.2f, 1.6f, 1.6f), col, 0.8f, 0.0f);
                ctx.draw_line(PointF{cx, cy - 0.6f}, PointF{cx, cy + 3.2f}, col, 1.4f);
                break;
            case NavigationIcon::Star:
                ctx.draw_line(PointF{cx, cy - 5.5f}, PointF{cx + 3.5f, cy + 4.8f}, col, sw);
                ctx.draw_line(PointF{cx + 3.5f, cy + 4.8f}, PointF{cx - 5.5f, cy - 1.4f}, col, sw);
                ctx.draw_line(PointF{cx - 5.5f, cy - 1.4f}, PointF{cx + 5.5f, cy - 1.4f}, col, sw);
                ctx.draw_line(PointF{cx + 5.5f, cy - 1.4f}, PointF{cx - 3.5f, cy + 4.8f}, col, sw);
                ctx.draw_line(PointF{cx - 3.5f, cy + 4.8f}, PointF{cx, cy - 5.5f}, col, sw);
                break;
            case NavigationIcon::Search:
                ctx.draw_rect(Rect::from_xywh(cx - 5.2f, cy - 5.2f, 8.0f, 8.0f), col, 4.0f, sw);
                ctx.draw_line(PointF{cx + 1.6f, cy + 1.6f}, PointF{cx + 5.5f, cy + 5.5f}, col, 1.4f);
                break;
            case NavigationIcon::Folder:
                ctx.draw_rect(Rect::from_xywh(cx - 6.0f, cy - 3.2f, 12.0f, 8.2f), col, 1.5f, sw);
                ctx.draw_line(PointF{cx - 5.8f, cy - 3.2f}, PointF{cx - 4.5f, cy - 5.0f}, col, sw);
                ctx.draw_line(PointF{cx - 4.5f, cy - 5.0f}, PointF{cx - 0.5f, cy - 5.0f}, col, sw);
                ctx.draw_line(PointF{cx - 0.5f, cy - 5.0f}, PointF{cx + 0.8f, cy - 3.2f}, col, sw);
                break;
            case NavigationIcon::Code:
                ctx.draw_line(PointF{cx - 2.2f, cy - 3.5f}, PointF{cx - 5.6f, cy}, col, sw);
                ctx.draw_line(PointF{cx - 5.6f, cy}, PointF{cx - 2.2f, cy + 3.5f}, col, sw);
                ctx.draw_line(PointF{cx + 2.2f, cy - 3.5f}, PointF{cx + 5.6f, cy}, col, sw);
                ctx.draw_line(PointF{cx + 5.6f, cy}, PointF{cx + 2.2f, cy + 3.5f}, col, sw);
                break;
            case NavigationIcon::Sparkles:
                ctx.draw_line(PointF{cx, cy - 5.8f}, PointF{cx, cy + 5.8f}, col, sw);
                ctx.draw_line(PointF{cx - 5.8f, cy}, PointF{cx + 5.8f, cy}, col, sw);
                ctx.draw_line(PointF{cx - 3.2f, cy - 3.2f}, PointF{cx + 3.2f, cy + 3.2f}, col, 1.0f);
                ctx.draw_line(PointF{cx + 3.2f, cy - 3.2f}, PointF{cx - 3.2f, cy + 3.2f}, col, 1.0f);
                break;
            default:
                break;
        }
    }
};

} // namespace ktui
