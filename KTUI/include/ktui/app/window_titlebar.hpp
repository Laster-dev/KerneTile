#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace ktui {

// 标题栏自定义操作按钮描述 (对齐 CUI.Core TitleBarAction)
struct TitlebarAction {
    std::string glyph;              // 图标文本/符号 (如 "+", "⚙", "?")
    std::string tooltip;            // 提示说明
    std::function<void()> on_click; // 点击回调
    bool enabled{true};
};

// 框架内置无边框窗口自绘标题栏 (严格对齐 CUI.Core WindowTitleBar 布局与视觉规范)
// 布局顺序 (左 -> 右):
//   [左侧图标] [窗口标题] [中间留白拖拽区] [自定义操作按钮...] [置顶 Pin] [主题 🌙/☀] [最小化 ─] [最大化 □/❐] [关闭 ×]
class WindowTitlebar {
public:
    static constexpr float kHeight = 32.0f;
    static constexpr float kButtonWidth = 46.0f;
    static constexpr float kIconSize = 16.0f;
    static constexpr float kResizeBorder = 8.0f;
    static constexpr float kRightResizeBorder = 3.0f;

    std::function<void()> on_minimize;
    std::function<void()> on_maximize_toggle;
    std::function<void()> on_close;
    std::function<void()> on_pin_toggle;
    std::function<void()> on_theme_toggle;

    [[nodiscard]] float height() const noexcept { return kHeight; }

    // ==================== 自定义扩展接口 ====================
    void set_icon_text(std::string icon) { m_icon_text = std::move(icon); }
    [[nodiscard]] const std::string& icon_text() const noexcept { return m_icon_text; }

    void add_action(std::string glyph, std::string tooltip, std::function<void()> on_click) {
        m_actions.push_back(TitlebarAction{std::move(glyph), std::move(tooltip), std::move(on_click), true});
    }
    void add_action(std::string glyph, std::function<void()> on_click) {
        add_action(std::move(glyph), "", std::move(on_click));
    }
    void clear_actions() { m_actions.clear(); }
    [[nodiscard]] const std::vector<TitlebarAction>& actions() const noexcept { return m_actions; }

    void set_pin_button_visible(bool v) noexcept { m_show_pin = v; }
    void set_theme_button_visible(bool v) noexcept { m_show_theme = v; }
    void set_minimize_button_visible(bool v) noexcept { m_show_minimize = v; }
    void set_maximize_button_visible(bool v) noexcept { m_show_maximize = v; }
    void set_close_button_visible(bool v) noexcept { m_show_close = v; }

    [[nodiscard]] bool is_pin_button_visible() const noexcept { return m_show_pin; }
    [[nodiscard]] bool is_theme_button_visible() const noexcept { return m_show_theme; }

    // ==================== 按钮几何计算 (右对齐依次排布) ====================
    [[nodiscard]] Rect close_rect(Vec2 win_size) const noexcept {
        if (!m_show_close) return Rect{};
        return Rect::from_xywh(win_size.x - kButtonWidth, 0.0f, kButtonWidth, kHeight);
    }

    [[nodiscard]] Rect maximize_rect(Vec2 win_size, bool resizable) const noexcept {
        if (!m_show_maximize || !resizable) return Rect{};
        float offset = kButtonWidth + (m_show_close ? kButtonWidth : 0.0f);
        return Rect::from_xywh(win_size.x - offset, 0.0f, kButtonWidth, kHeight);
    }

    [[nodiscard]] Rect minimize_rect(Vec2 win_size, bool resizable) const noexcept {
        if (!m_show_minimize) return Rect{};
        float offset = kButtonWidth;
        if (m_show_close) offset += kButtonWidth;
        if (m_show_maximize && resizable) offset += kButtonWidth;
        return Rect::from_xywh(win_size.x - offset, 0.0f, kButtonWidth, kHeight);
    }

    [[nodiscard]] Rect theme_rect(Vec2 win_size, bool resizable) const noexcept {
        if (!m_show_theme) return Rect{};
        float offset = kButtonWidth;
        if (m_show_close) offset += kButtonWidth;
        if (m_show_maximize && resizable) offset += kButtonWidth;
        if (m_show_minimize) offset += kButtonWidth;
        return Rect::from_xywh(win_size.x - offset, 0.0f, kButtonWidth, kHeight);
    }

    [[nodiscard]] Rect pin_rect(Vec2 win_size, bool resizable) const noexcept {
        if (!m_show_pin) return Rect{};
        float offset = kButtonWidth;
        if (m_show_close) offset += kButtonWidth;
        if (m_show_maximize && resizable) offset += kButtonWidth;
        if (m_show_minimize) offset += kButtonWidth;
        if (m_show_theme) offset += kButtonWidth;
        return Rect::from_xywh(win_size.x - offset, 0.0f, kButtonWidth, kHeight);
    }

    [[nodiscard]] Rect action_rect(size_t index, Vec2 win_size, bool resizable) const noexcept {
        if (index >= m_actions.size()) return Rect{};
        float offset = 0.0f;
        if (m_show_close) offset += kButtonWidth;
        if (m_show_maximize && resizable) offset += kButtonWidth;
        if (m_show_minimize) offset += kButtonWidth;
        if (m_show_theme) offset += kButtonWidth;
        if (m_show_pin) offset += kButtonWidth;
        offset += static_cast<float>(m_actions.size() - index) * kButtonWidth;
        return Rect::from_xywh(win_size.x - offset, 0.0f, kButtonWidth, kHeight);
    }

    [[nodiscard]] float buttons_left(Vec2 win_size, bool resizable) const noexcept {
        float total = 0.0f;
        if (m_show_close) total += kButtonWidth;
        if (m_show_maximize && resizable) total += kButtonWidth;
        if (m_show_minimize) total += kButtonWidth;
        if (m_show_theme) total += kButtonWidth;
        if (m_show_pin) total += kButtonWidth;
        total += static_cast<float>(m_actions.size()) * kButtonWidth;
        return win_size.x - total;
    }

    // 坐标是否命中交互型标题栏按钮 (Pin / Theme / Custom Actions -> 返回 HTCLIENT 以接收点击并显示手型光标)
    [[nodiscard]] bool is_interactive_button_hit(PointF local_pos, Vec2 win_size, bool resizable) const noexcept {
        if (local_pos.y < 0.0f || local_pos.y >= kHeight) return false;
        if (m_show_theme && theme_rect(win_size, resizable).contains(local_pos)) return true;
        if (m_show_pin && pin_rect(win_size, resizable).contains(local_pos)) return true;
        for (size_t i = 0; i < m_actions.size(); ++i) {
            if (action_rect(i, win_size, resizable).contains(local_pos)) return true;
        }
        return false;
    }

    // ==================== 命中测试 (对齐 CUI.Core Win11 Snap Layout 优先级) ====================
    [[nodiscard]] TitlebarHitTest hit_test(PointF local_pos, Vec2 win_size,
                                           bool resizable, bool maximized) const noexcept {
        if (win_size.x <= 0.0f || win_size.y <= 0.0f) {
            return TitlebarHitTest::Client;
        }

        // 1. 优先测试标题栏右侧所有按钮 (必须在顶部/右侧缩放边界之前，确保 Win11 Snap Layout 准确命中 HTMAXBUTTON!)
        if (local_pos.y >= 0.0f && local_pos.y < kHeight &&
            local_pos.x >= 0.0f && local_pos.x <= win_size.x) {
            if (m_show_close && close_rect(win_size).contains(local_pos)) {
                return TitlebarHitTest::CloseBtn;
            }
            if (m_show_maximize && resizable && maximize_rect(win_size, resizable).contains(local_pos)) {
                return TitlebarHitTest::MaximizeBtn;
            }
            if (m_show_minimize && minimize_rect(win_size, resizable).contains(local_pos)) {
                return TitlebarHitTest::MinimizeBtn;
            }
            if (is_interactive_button_hit(local_pos, win_size, resizable)) {
                return TitlebarHitTest::Client;
            }
        }

        // 2. 八向边缘拉伸命中测试 (非最大化且允许缩放时生效)
        if (resizable && !maximized) {
            const bool left   = local_pos.x >= 0.0f && local_pos.x < kResizeBorder;
            const bool right  = local_pos.x <= win_size.x && local_pos.x > win_size.x - kRightResizeBorder;
            const bool top    = local_pos.y >= 0.0f && local_pos.y < kResizeBorder;
            const bool bottom = local_pos.y <= win_size.y && local_pos.y > win_size.y - kResizeBorder;

            if (top && left)     return TitlebarHitTest::BorderTopLeft;
            if (top && right)    return TitlebarHitTest::BorderTopRight;
            if (bottom && left)  return TitlebarHitTest::BorderBottomLeft;
            if (bottom && right) return TitlebarHitTest::BorderBottomRight;
            if (left)            return TitlebarHitTest::BorderLeft;
            if (right)           return TitlebarHitTest::BorderRight;
            if (top)             return TitlebarHitTest::BorderTop;
            if (bottom)          return TitlebarHitTest::BorderBottom;
        }

        // 3. 标题栏左侧图标、标题与中间留白拖拽区
        if (local_pos.y >= 0.0f && local_pos.y < kHeight &&
            local_pos.x >= 0.0f && local_pos.x <= win_size.x) {
            return TitlebarHitTest::Caption;
        }

        return TitlebarHitTest::Client;
    }

    // ==================== 指针交互 ====================
    bool handle_pointer_move(PointF local_pos, Vec2 win_size, bool resizable) {
        int next = hit_region(local_pos, win_size, resizable);
        if (next != m_hover_region) {
            m_hover_region = next;
            return true;
        }
        return false;
    }

    bool handle_pointer_leave() {
        if (m_hover_region != -1 || m_pressed_region != -1) {
            m_hover_region = -1;
            m_pressed_region = -1;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool is_hand_cursor_region() const noexcept {
        return m_hover_region == kRegionTheme || m_hover_region == kRegionPin || m_hover_region >= kRegionActionBase;
    }

    bool handle_pointer_down(PointF local_pos, PointerButton button,
                             Vec2 win_size, bool resizable, bool maximized,
                             bool& out_needs_redraw) {
        out_needs_redraw = false;
        if (button != PointerButton::Primary) return false;

        int reg = hit_region(local_pos, win_size, resizable);
        if (reg != -1) {
            m_pressed_region = reg;
            m_hover_region = reg;
            out_needs_redraw = true;
            return true;
        }

        TitlebarHitTest ht = hit_test(local_pos, win_size, resizable, maximized);
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
                           Vec2 win_size, bool resizable,
                           bool& out_needs_redraw) {
        out_needs_redraw = false;
        if (button != PointerButton::Primary || m_pressed_region == -1) {
            return false;
        }

        int pressed = m_pressed_region;
        m_pressed_region = -1;
        out_needs_redraw = true;

        int released = hit_region(local_pos, win_size, resizable);
        m_hover_region = released;

        if (released == pressed) {
            if (pressed == kRegionMin && on_minimize) {
                on_minimize();
            } else if (pressed == kRegionMax && on_maximize_toggle) {
                on_maximize_toggle();
            } else if (pressed == kRegionClose && on_close) {
                on_close();
            } else if (pressed == kRegionPin && on_pin_toggle) {
                on_pin_toggle();
            } else if (pressed == kRegionTheme && on_theme_toggle) {
                on_theme_toggle();
            } else if (pressed >= kRegionActionBase) {
                size_t idx = static_cast<size_t>(pressed - kRegionActionBase);
                if (idx < m_actions.size() && m_actions[idx].enabled && m_actions[idx].on_click) {
                    m_actions[idx].on_click();
                }
            }
        }
        return true;
    }

    // ==================== CUI.Core 风格自绘管线 ====================
    void paint(PaintContext& ctx, Vec2 win_size, const std::string& title,
               bool is_active, bool resizable, bool maximized, bool is_pinned) const {
        if (win_size.x <= 0.0f || win_size.y <= 0.0f) return;

        const Theme& theme = ThemeContext::current();
        const bool is_dark = theme.is_dark;

        // 严格对齐 CUI.Core ThemeManager.cpp 的 WinUI Fluent 色板：
        // 标题栏与窗口底色完全无缝融合 (#202020 / #EAEDF1)，不画生硬的色带或分隔线
        const Color bar_bg = is_dark
            ? Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f}
            : Color{0xEA / 255.0f, 0xED / 255.0f, 0xF1 / 255.0f, 1.0f};
        const Color text_primary = is_dark
            ? (is_active ? Color{1.0f, 1.0f, 1.0f, 1.0f} : Color{0.62f, 0.62f, 0.62f, 1.0f})
            : (is_active ? Color{0x1F / 255.0f, 0x23 / 255.0f, 0x28 / 255.0f, 1.0f} : Color{0x6E / 255.0f, 0x76 / 255.0f, 0x81 / 255.0f, 1.0f});
        const Color accent = is_dark
            ? Color{0x00 / 255.0f, 0x86 / 255.0f, 0xF0 / 255.0f, 1.0f}
            : Color{0x09 / 255.0f, 0x69 / 255.0f, 0xDA / 255.0f, 1.0f};
        const float hover_peak = is_dark ? 0.14f : 0.08f;
        const float press_peak = is_dark ? 0.20f : 0.12f;
        const Color btn_hover = Color{text_primary.r, text_primary.g, text_primary.b, hover_peak};
        const Color btn_press = Color{text_primary.r, text_primary.g, text_primary.b, press_peak};
        const Color close_hover = is_dark
            ? Color{0xC4 / 255.0f, 0x2B / 255.0f, 0x1C / 255.0f, 1.0f}
            : Color{0xCF / 255.0f, 0x22 / 255.0f, 0x2E / 255.0f, 1.0f};
        const Color close_press = Color{close_hover.r * 0.85f, close_hover.g * 0.85f, close_hover.b * 0.85f, 1.0f};

        const float cy = kHeight * 0.5f;

        // 1. 标题栏平直底盘 (与窗口底色一致，无缝衔接)
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, win_size.x, kHeight), bar_bg, 0.0f, 0.0f);

        // 2. 左侧应用图标 (16x16 @ x=10) + 左对齐标题文本 (裁剪在按钮区左侧 - 8px)
        const float title_right = buttons_left(win_size, resizable) - 8.0f;
        if (title_right > 20.0f) {
            ctx.push_clip(Rect::from_xywh(0.0f, 0.0f, title_right, kHeight), 0.0f);

            const Rect icon_rect = Rect::from_xywh(10.0f, (kHeight - kIconSize) * 0.5f, kIconSize, kIconSize);
            if (!m_icon_text.empty()) {
                ctx.draw_text(m_icon_text, PointF{icon_rect.min_x, icon_rect.min_y}, 13.0f, text_primary);
            } else {
                // 默认几何双层窗格徽标
                const Color emblem = is_active ? accent : Color{0.46f, 0.48f, 0.54f, 1.0f};
                ctx.draw_rect(Rect::from_xywh(icon_rect.min_x + 1.0f, icon_rect.min_y + 1.0f, 14.0f, 14.0f),
                              emblem, 3.0f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(icon_rect.min_x + 4.0f, icon_rect.min_y + 4.0f, 3.5f, 3.5f),
                              Color::white(), 0.8f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(icon_rect.min_x + 8.5f, icon_rect.min_y + 4.0f, 3.5f, 3.5f),
                              Color{1.0f, 1.0f, 1.0f, 0.72f}, 0.8f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(icon_rect.min_x + 4.0f, icon_rect.min_y + 8.5f, 8.0f, 3.5f),
                              Color{1.0f, 1.0f, 1.0f, 0.85f}, 0.8f, 0.0f);
            }

            if (!title.empty()) {
                constexpr float kTitleFontSize = 12.5f;
                const float title_left = icon_rect.max_x + 8.0f;
                const float text_y = (kHeight - kTitleFontSize * 1.35f) * 0.5f;
                ctx.draw_text(title, PointF{title_left, text_y}, kTitleFontSize, text_primary);
            }
            ctx.pop_clip();
        }

        // 3. 自定义操作按钮 (Actions)
        for (size_t i = 0; i < m_actions.size(); ++i) {
            const auto& act = m_actions[i];
            const Rect r = action_rect(i, win_size, resizable);
            const int reg = static_cast<int>(kRegionActionBase + i);
            if (act.enabled) {
                if (m_pressed_region == reg) ctx.draw_rect(r, btn_press, 0.0f, 0.0f);
                else if (m_hover_region == reg) ctx.draw_rect(r, btn_hover, 0.0f, 0.0f);
            }
            const Color fg = act.enabled ? text_primary : theme.colors.text_muted;
            const float cx = r.min_x + r.width() * 0.5f;
            if (act.glyph == "+") {
                ctx.draw_line(PointF{cx - 4.5f, cy}, PointF{cx + 4.5f, cy}, fg, 1.3f);
                ctx.draw_line(PointF{cx, cy - 4.5f}, PointF{cx, cy + 4.5f}, fg, 1.3f);
            } else if (!act.glyph.empty()) {
                constexpr float fs = 13.0f;
                ctx.draw_text(act.glyph, PointF{cx - 4.5f, (kHeight - fs * 1.35f) * 0.5f}, fs, fg);
            }
        }

        // 4. 窗口置顶按钮 (Pin)
        if (m_show_pin) {
            const Rect r = pin_rect(win_size, resizable);
            if (m_pressed_region == kRegionPin) ctx.draw_rect(r, btn_press, 0.0f, 0.0f);
            else if (m_hover_region == kRegionPin) ctx.draw_rect(r, btn_hover, 0.0f, 0.0f);

            const Color pin_col = is_pinned ? accent : text_primary;
            const float cx = r.min_x + r.width() * 0.5f;
            // 矢量图钉图标 (置顶时实心高亮 + 水平底座)
            if (is_pinned) {
                ctx.draw_rect(Rect::from_xywh(cx - 3.5f, cy - 5.0f, 7.0f, 5.5f), pin_col, 1.2f, 0.0f);
                ctx.draw_line(PointF{cx - 5.0f, cy + 1.0f}, PointF{cx + 5.0f, cy + 1.0f}, pin_col, 1.5f);
                ctx.draw_line(PointF{cx, cy + 1.0f}, PointF{cx, cy + 6.0f}, pin_col, 1.4f);
            } else {
                // 未置顶时 45° 斜向空心图钉
                ctx.draw_line(PointF{cx - 1.0f, cy - 5.0f}, PointF{cx + 5.0f, cy + 1.0f}, pin_col, 1.3f);
                ctx.draw_line(PointF{cx - 3.0f, cy - 1.5f}, PointF{cx + 1.5f, cy + 3.0f}, pin_col, 1.3f);
                ctx.draw_line(PointF{cx - 1.0f, cy - 4.0f}, PointF{cx - 2.5f, cy - 1.0f}, pin_col, 1.2f);
                ctx.draw_line(PointF{cx + 4.0f, cy + 1.0f}, PointF{cx + 1.0f, cy + 2.5f}, pin_col, 1.2f);
                ctx.draw_line(PointF{cx - 0.8f, cy + 0.8f}, PointF{cx - 5.0f, cy + 5.0f}, pin_col, 1.3f);
            }
        }

        // 5. 主题切换按钮 (暗色模式月亮 / 浅色模式太阳)
        if (m_show_theme) {
            const Rect r = theme_rect(win_size, resizable);
            if (m_pressed_region == kRegionTheme) ctx.draw_rect(r, btn_press, 0.0f, 0.0f);
            else if (m_hover_region == kRegionTheme) ctx.draw_rect(r, btn_hover, 0.0f, 0.0f);

            const float cx = r.min_x + r.width() * 0.5f;
            if (is_dark) {
                // 弯月图标 (外圆 + 右上遮挡圆)
                const Color cut_bg = (m_pressed_region == kRegionTheme) ? Color{0.24f, 0.25f, 0.28f, 1.0f}
                                   : (m_hover_region == kRegionTheme) ? Color{0.20f, 0.21f, 0.24f, 1.0f}
                                   : bar_bg;
                ctx.draw_rect(Rect::from_xywh(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f), text_primary, 5.5f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(cx - 2.0f, cy - 7.0f, 9.5f, 9.5f), cut_bg, 4.75f, 0.0f);
            } else {
                // 太阳图标 (中心圆 + 8 向光芒)
                ctx.draw_rect(Rect::from_xywh(cx - 3.2f, cy - 3.2f, 6.4f, 6.4f), text_primary, 3.2f, 1.2f);
                constexpr float kDirs[8][2] = {
                    {0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f},
                    {0.707f, -0.707f}, {0.707f, 0.707f}, {-0.707f, -0.707f}, {-0.707f, 0.707f}
                };
                for (const auto& d : kDirs) {
                    ctx.draw_line(PointF{cx + d[0] * 4.6f, cy + d[1] * 4.6f},
                                  PointF{cx + d[0] * 6.4f, cy + d[1] * 6.4f}, text_primary, 1.15f);
                }
            }
        }

        // 6. 最小化按钮 (─)
        if (m_show_minimize) {
            const Rect r = minimize_rect(win_size, resizable);
            if (m_pressed_region == kRegionMin) ctx.draw_rect(r, btn_press, 0.0f, 0.0f);
            else if (m_hover_region == kRegionMin) ctx.draw_rect(r, btn_hover, 0.0f, 0.0f);
            ctx.draw_line(PointF{r.min_x + 18.0f, cy}, PointF{r.min_x + 28.0f, cy}, text_primary, 1.0f);
        }

        // 7. 最大化 / 还原按钮 (严格对齐 Windows 11 双框无穿透折角)
        if (m_show_maximize && resizable) {
            const Rect r = maximize_rect(win_size, resizable);
            if (m_pressed_region == kRegionMax) ctx.draw_rect(r, btn_press, 0.0f, 0.0f);
            else if (m_hover_region == kRegionMax) ctx.draw_rect(r, btn_hover, 0.0f, 0.0f);
            if (maximized) {
                const float front_x = r.min_x + 18.0f;
                const float front_y = cy - 3.0f;
                // 后层窗口仅露右上 L 形折角
                ctx.draw_line(PointF{front_x + 2.0f, front_y - 2.0f}, PointF{front_x + 10.0f, front_y - 2.0f}, text_primary, 1.0f);
                ctx.draw_line(PointF{front_x + 10.0f, front_y - 2.0f}, PointF{front_x + 10.0f, front_y + 6.0f}, text_primary, 1.0f);
                // 前层主窗口
                ctx.draw_rect(Rect::from_xywh(front_x, front_y, 8.0f, 8.0f), text_primary, 0.5f, 1.0f);
            } else {
                ctx.draw_rect(Rect::from_xywh(r.min_x + 18.0f, cy - 5.0f, 10.0f, 10.0f), text_primary, 0.5f, 1.0f);
            }
        }

        // 8. 关闭按钮 (×)
        if (m_show_close) {
            const Rect r = close_rect(win_size);
            Color fg = text_primary;
            if (m_pressed_region == kRegionClose) {
                ctx.draw_rect(r, close_press, 0.0f, 0.0f);
                fg = Color::white();
            } else if (m_hover_region == kRegionClose) {
                ctx.draw_rect(r, close_hover, 0.0f, 0.0f);
                fg = Color::white();
            }
            const float cx = r.min_x + r.width() * 0.5f;
            ctx.draw_line(PointF{cx - 5.0f, cy - 5.0f}, PointF{cx + 5.0f, cy + 5.0f}, fg, 1.2f);
            ctx.draw_line(PointF{cx + 5.0f, cy - 5.0f}, PointF{cx - 5.0f, cy + 5.0f}, fg, 1.2f);
        }
    }

    // 整窗外框 1px 描边 (Windows 下交由 DWM 原生圆角阴影负责，仅非 Win32 如 Web 桌面绘制极淡微框)
    void paint_outer_border(PaintContext& ctx, Vec2 win_size, bool is_active, bool maximized) const {
#if defined(_WIN32)
        (void)ctx; (void)win_size; (void)is_active; (void)maximized;
        return;
#else
        if (maximized || win_size.x <= 2.0f || win_size.y <= 2.0f) return;
        const Theme& theme = ThemeContext::current();
        const Color border_col = theme.is_dark
            ? (is_active ? Color{1.0f, 1.0f, 1.0f, 0.12f} : Color{1.0f, 1.0f, 1.0f, 0.06f})
            : (is_active ? Color{0.0f, 0.0f, 0.0f, 0.14f} : Color{0.0f, 0.0f, 0.0f, 0.07f});
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, win_size.x, win_size.y), border_col, 0.0f, 1.0f);
#endif
    }

private:
    static constexpr int kRegionMin = 1;
    static constexpr int kRegionMax = 2;
    static constexpr int kRegionClose = 3;
    static constexpr int kRegionTheme = 4;
    static constexpr int kRegionPin = 5;
    static constexpr int kRegionActionBase = 100;

    [[nodiscard]] int hit_region(PointF local_pos, Vec2 win_size, bool resizable) const noexcept {
        if (local_pos.y < 0.0f || local_pos.y >= kHeight) return -1;
        if (m_show_close && close_rect(win_size).contains(local_pos)) return kRegionClose;
        if (m_show_maximize && resizable && maximize_rect(win_size, resizable).contains(local_pos)) return kRegionMax;
        if (m_show_minimize && minimize_rect(win_size, resizable).contains(local_pos)) return kRegionMin;
        if (m_show_theme && theme_rect(win_size, resizable).contains(local_pos)) return kRegionTheme;
        if (m_show_pin && pin_rect(win_size, resizable).contains(local_pos)) return kRegionPin;
        for (size_t i = 0; i < m_actions.size(); ++i) {
            if (m_actions[i].enabled && action_rect(i, win_size, resizable).contains(local_pos)) {
                return static_cast<int>(kRegionActionBase + i);
            }
        }
        return -1;
    }

    std::string m_icon_text;
    std::vector<TitlebarAction> m_actions;
    bool m_show_pin{true};
    bool m_show_theme{true};
    bool m_show_minimize{true};
    bool m_show_maximize{true};
    bool m_show_close{true};
    int m_hover_region{-1};
    int m_pressed_region{-1};
    std::chrono::steady_clock::time_point m_last_caption_click{};
    PointF m_last_caption_pos{0.0f, 0.0f};
};

} // namespace ktui
