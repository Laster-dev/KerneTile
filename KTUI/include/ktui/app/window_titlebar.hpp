#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <string>

namespace ktui {

// 框架内置无边框窗口自绘标题栏 (Fluent 风格)
// 职责：标题栏绘制、最小化/最大化/关闭矢量按钮交互、双击最大化、边缘拉伸命中测试与外框描边
class WindowTitlebar {
public:
    static constexpr float kHeight = 34.0f;
    static constexpr float kButtonWidth = 44.0f;
    static constexpr float kResizeBorder = 6.0f;

    std::function<void()> on_minimize;
    std::function<void()> on_maximize_toggle;
    std::function<void()> on_close;

    [[nodiscard]] float height() const noexcept { return kHeight; }

    [[nodiscard]] TitlebarHitTest hit_test(PointF local_pos, Vec2 win_size,
                                           bool resizable, bool maximized) const noexcept {
        if (win_size.x <= 0.0f || win_size.y <= 0.0f) {
            return TitlebarHitTest::Client;
        }

        // 1. 边缘拉伸命中测试 (非最大化且允许缩放时生效)
        if (resizable && !maximized) {
            const float b = kResizeBorder;
            const bool left   = local_pos.x >= 0.0f && local_pos.x < b;
            const bool right  = local_pos.x <= win_size.x && local_pos.x > win_size.x - b;
            const bool top    = local_pos.y >= 0.0f && local_pos.y < b;
            const bool bottom = local_pos.y <= win_size.y && local_pos.y > win_size.y - b;

            if (top && left)     return TitlebarHitTest::BorderTopLeft;
            if (top && right)    return TitlebarHitTest::BorderTopRight;
            if (bottom && left)  return TitlebarHitTest::BorderBottomLeft;
            if (bottom && right) return TitlebarHitTest::BorderBottomRight;
            if (left)            return TitlebarHitTest::BorderLeft;
            if (right)           return TitlebarHitTest::BorderRight;
            if (top)             return TitlebarHitTest::BorderTop;
            if (bottom)          return TitlebarHitTest::BorderBottom;
        }

        // 2. 标题栏区域命中测试
        if (local_pos.y >= 0.0f && local_pos.y < kHeight &&
            local_pos.x >= 0.0f && local_pos.x <= win_size.x) {
            if (close_rect(win_size).contains(local_pos)) {
                return TitlebarHitTest::CloseBtn;
            }
            if (resizable && maximize_rect(win_size).contains(local_pos)) {
                return TitlebarHitTest::MaximizeBtn;
            }
            if (minimize_rect(win_size, resizable).contains(local_pos)) {
                return TitlebarHitTest::MinimizeBtn;
            }
            return TitlebarHitTest::Caption;
        }

        return TitlebarHitTest::Client;
    }

    // 返回是否需要重绘
    bool handle_pointer_move(PointF local_pos, Vec2 win_size, bool resizable, bool maximized) {
        TitlebarHitTest ht = hit_test(local_pos, win_size, resizable, maximized);
        ButtonKind next = button_from_hit(ht);
        if (next != m_hovered) {
            m_hovered = next;
            return true;
        }
        return false;
    }

    bool handle_pointer_leave() {
        if (m_hovered != ButtonKind::None || m_pressed != ButtonKind::None) {
            m_hovered = ButtonKind::None;
            m_pressed = ButtonKind::None;
            return true;
        }
        return false;
    }

    // 返回 true 表示事件已被标题栏按钮/双击消费
    bool handle_pointer_down(PointF local_pos, PointerButton button,
                             Vec2 win_size, bool resizable, bool maximized,
                             bool& out_needs_redraw) {
        out_needs_redraw = false;
        if (button != PointerButton::Primary) return false;

        TitlebarHitTest ht = hit_test(local_pos, win_size, resizable, maximized);
        ButtonKind kind = button_from_hit(ht);
        if (kind != ButtonKind::None) {
            m_pressed = kind;
            m_hovered = kind;
            out_needs_redraw = true;
            return true;
        }

        if (ht == TitlebarHitTest::Caption && resizable) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_last_caption_click).count();
            float dist = std::hypot(local_pos.x - m_last_caption_pos.x,
                                    local_pos.y - m_last_caption_pos.y);
            m_last_caption_click = now;
            m_last_caption_pos = local_pos;
            if (elapsed_ms > 0 && elapsed_ms <= 360 && dist <= 8.0f) {
                m_last_caption_click = {};
                if (on_maximize_toggle) on_maximize_toggle();
                out_needs_redraw = true;
                return true;
            }
        }
        return false;
    }

    bool handle_pointer_up(PointF local_pos, PointerButton button,
                           Vec2 win_size, bool resizable, bool maximized,
                           bool& out_needs_redraw) {
        out_needs_redraw = false;
        if (button != PointerButton::Primary || m_pressed == ButtonKind::None) {
            return false;
        }

        ButtonKind pressed = m_pressed;
        m_pressed = ButtonKind::None;
        out_needs_redraw = true;

        TitlebarHitTest ht = hit_test(local_pos, win_size, resizable, maximized);
        ButtonKind released_on = button_from_hit(ht);
        m_hovered = released_on;

        if (released_on == pressed) {
            if (pressed == ButtonKind::Minimize && on_minimize) {
                on_minimize();
            } else if (pressed == ButtonKind::Maximize && on_maximize_toggle) {
                on_maximize_toggle();
            } else if (pressed == ButtonKind::Close && on_close) {
                on_close();
            }
        }
        return true;
    }

    void paint(PaintContext& ctx, Vec2 win_size, const std::string& title,
               bool is_active, bool resizable, bool maximized) const {
        if (win_size.x <= 0.0f || win_size.y <= 0.0f) return;

        const Theme& theme = ThemeContext::current();
        const bool is_dark = theme.is_dark;

        // 标题栏底色与分隔线色值
        const Color bar_bg = is_dark
            ? (is_active ? Color{0.13f, 0.135f, 0.16f, 1.0f} : Color{0.115f, 0.118f, 0.138f, 1.0f})
            : (is_active ? Color{0.94f, 0.945f, 0.96f, 1.0f} : Color{0.91f, 0.915f, 0.93f, 1.0f});
        const Color sep_col = is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.07f}
            : Color{0.0f, 0.0f, 0.0f, 0.08f};
        const Color title_col = is_dark
            ? (is_active ? Color{0.92f, 0.93f, 0.96f, 1.0f} : Color{0.58f, 0.60f, 0.65f, 1.0f})
            : (is_active ? Color{0.12f, 0.13f, 0.16f, 1.0f} : Color{0.48f, 0.50f, 0.54f, 1.0f});
        const Color icon_col = is_dark
            ? (is_active ? Color{0.86f, 0.88f, 0.92f, 1.0f} : Color{0.55f, 0.57f, 0.62f, 1.0f})
            : (is_active ? Color{0.18f, 0.19f, 0.22f, 1.0f} : Color{0.50f, 0.52f, 0.56f, 1.0f});
        const Color btn_hover = is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.09f}
            : Color{0.0f, 0.0f, 0.0f, 0.06f};
        const Color btn_press = is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.15f}
            : Color{0.0f, 0.0f, 0.0f, 0.11f};
        const Color close_hover = Color{0.77f, 0.17f, 0.11f, 1.0f}; // #C42B1C
        const Color close_press = Color{0.64f, 0.14f, 0.09f, 1.0f};

        // 1. 标题栏背景与底部分隔线
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, win_size.x, kHeight), bar_bg, 0.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, kHeight - 1.0f, win_size.x, 1.0f), sep_col, 0.0f, 0.0f);

        // 2. 左侧应用徽标指示点 + 标题文本 (裁剪防止压住右侧按钮)
        const float controls_w = (resizable ? 3.0f : 2.0f) * kButtonWidth;
        const float title_avail_w = std::max(0.0f, win_size.x - controls_w - 12.0f);
        if (title_avail_w > 16.0f) {
            ctx.push_clip(Rect::from_xywh(0.0f, 0.0f, title_avail_w, kHeight), 0.0f);
            const Color accent = is_active ? Color{0.17f, 0.50f, 1.0f, 1.0f} : Color{0.42f, 0.44f, 0.50f, 1.0f};
            ctx.draw_rect(Rect::from_xywh(12.0f, (kHeight - 8.0f) * 0.5f, 8.0f, 8.0f), accent, 4.0f, 0.0f);
            if (!title.empty()) {
                constexpr float kTitleFontSize = 12.5f;
                float text_y = (kHeight - kTitleFontSize * 1.35f) * 0.5f;
                ctx.draw_text(title, PointF{28.0f, text_y}, kTitleFontSize, title_col);
            }
            ctx.pop_clip();
        }

        // 3. 最小化按钮 (─)
        {
            Rect r = minimize_rect(win_size, resizable);
            if (m_pressed == ButtonKind::Minimize) {
                ctx.draw_rect(r, btn_press, 0.0f, 0.0f);
            } else if (m_hovered == ButtonKind::Minimize) {
                ctx.draw_rect(r, btn_hover, 0.0f, 0.0f);
            }
            float cx = r.min_x + r.width() * 0.5f;
            float cy = r.min_y + r.height() * 0.5f;
            ctx.draw_line(PointF{cx - 5.0f, cy}, PointF{cx + 5.0f, cy}, icon_col, 1.2f);
        }

        // 4. 最大化 / 还原按钮 (□ / ❐)
        if (resizable) {
            Rect r = maximize_rect(win_size);
            if (m_pressed == ButtonKind::Maximize) {
                ctx.draw_rect(r, btn_press, 0.0f, 0.0f);
            } else if (m_hovered == ButtonKind::Maximize) {
                ctx.draw_rect(r, btn_hover, 0.0f, 0.0f);
            }
            float cx = r.min_x + r.width() * 0.5f;
            float cy = r.min_y + r.height() * 0.5f;
            if (!maximized) {
                ctx.draw_rect(Rect::from_xywh(cx - 4.5f, cy - 4.5f, 9.0f, 9.0f), icon_col, 1.0f, 1.2f);
            } else {
                // 还原双框图标
                ctx.draw_rect(Rect::from_xywh(cx - 2.5f, cy - 5.0f, 7.5f, 7.5f), icon_col, 1.0f, 1.1f);
                ctx.draw_rect(Rect::from_xywh(cx - 5.0f, cy - 2.5f, 7.5f, 7.5f), bar_bg, 1.0f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(cx - 5.0f, cy - 2.5f, 7.5f, 7.5f), icon_col, 1.0f, 1.2f);
            }
        }

        // 5. 关闭按钮 (×)
        {
            Rect r = close_rect(win_size);
            Color glyph_col = icon_col;
            if (m_pressed == ButtonKind::Close) {
                ctx.draw_rect(r, close_press, 0.0f, 0.0f);
                glyph_col = Color::white();
            } else if (m_hovered == ButtonKind::Close) {
                ctx.draw_rect(r, close_hover, 0.0f, 0.0f);
                glyph_col = Color::white();
            }
            float cx = r.min_x + r.width() * 0.5f;
            float cy = r.min_y + r.height() * 0.5f;
            ctx.draw_line(PointF{cx - 4.5f, cy - 4.5f}, PointF{cx + 4.5f, cy + 4.5f}, glyph_col, 1.3f);
            ctx.draw_line(PointF{cx + 4.5f, cy - 4.5f}, PointF{cx - 4.5f, cy + 4.5f}, glyph_col, 1.3f);
        }
    }

    // 在整窗最外层绘制 1px 精致无边框描边 (最大化时省略)
    void paint_outer_border(PaintContext& ctx, Vec2 win_size, bool is_active, bool maximized) const {
        if (maximized || win_size.x <= 2.0f || win_size.y <= 2.0f) return;
        const Theme& theme = ThemeContext::current();
        const Color border_col = theme.is_dark
            ? (is_active ? Color{1.0f, 1.0f, 1.0f, 0.16f} : Color{1.0f, 1.0f, 1.0f, 0.08f})
            : (is_active ? Color{0.0f, 0.0f, 0.0f, 0.18f} : Color{0.0f, 0.0f, 0.0f, 0.09f});
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, win_size.x, win_size.y), border_col, 0.0f, 1.0f);
    }

private:
    enum class ButtonKind { None, Minimize, Maximize, Close };

    static ButtonKind button_from_hit(TitlebarHitTest ht) noexcept {
        switch (ht) {
            case TitlebarHitTest::MinimizeBtn: return ButtonKind::Minimize;
            case TitlebarHitTest::MaximizeBtn: return ButtonKind::Maximize;
            case TitlebarHitTest::CloseBtn:    return ButtonKind::Close;
            default:                           return ButtonKind::None;
        }
    }

    static Rect close_rect(Vec2 win_size) noexcept {
        return Rect::from_xywh(win_size.x - kButtonWidth, 0.0f, kButtonWidth, kHeight);
    }

    static Rect maximize_rect(Vec2 win_size) noexcept {
        return Rect::from_xywh(win_size.x - kButtonWidth * 2.0f, 0.0f, kButtonWidth, kHeight);
    }

    static Rect minimize_rect(Vec2 win_size, bool resizable) noexcept {
        float slots = resizable ? 3.0f : 2.0f;
        return Rect::from_xywh(win_size.x - kButtonWidth * slots, 0.0f, kButtonWidth, kHeight);
    }

    ButtonKind m_hovered{ButtonKind::None};
    ButtonKind m_pressed{ButtonKind::None};
    std::chrono::steady_clock::time_point m_last_caption_click{};
    PointF m_last_caption_pos{0.0f, 0.0f};
};

} // namespace ktui
