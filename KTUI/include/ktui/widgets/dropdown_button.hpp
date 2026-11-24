#pragma once

#include "menu_flyout.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// =============================================================================
// 1. WinUI 3 DropDownButton (带右侧下拉箭头的菜单触发按钮)
// =============================================================================
class RenderDropDownButton : public RenderBox {
public:
    RenderDropDownButton() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderDropDownButton() override = default;

    void set_props(std::string text, std::string svg_path, std::vector<MenuFlyoutItem> items, bool disabled) {
        bool layout_changed = (m_text != text) || (m_svg_path != svg_path);
        m_text = std::move(text);
        m_svg_path = std::move(svg_path);
        m_items = std::move(items);
        m_disabled = disabled;
        cursor_shape = disabled ? CursorShape::Forbidden : CursorShape::PointingHand;
        if (layout_changed) mark_needs_layout();
        else mark_needs_paint();
    }

    void perform_layout() override {
        Vec2 tsz = TextMeasure::measure(m_text, 13.0f);
        float icon_w = m_svg_path.empty() ? 0.0f : 22.0f;
        float w = std::ceil(12.0f + icon_w + tsz.x + 28.0f);
        set_size(m_constraints.constrain(Vec2{std::max(84.0f, w), 32.0f}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        Rect r = bounds();

        Color bg, border, bottom_bd, fg;
        if (theme.is_dark) {
            bg = m_disabled ? Color{1.0f, 1.0f, 1.0f, 0.04f}
               : (m_pressed || m_menu_open ? Color{1.0f, 1.0f, 1.0f, 0.035f}
               : (m_hovered ? Color{1.0f, 1.0f, 1.0f, 0.088f} : Color{1.0f, 1.0f, 1.0f, 0.060f}));
            border = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.05f : 0.085f};
            bottom_bd = m_disabled ? Color::transparent() : Color{1.0f, 1.0f, 1.0f, 0.14f};
            fg = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.95f};
        } else {
            bg = m_disabled ? Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.35f}
               : (m_pressed || m_menu_open ? Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.35f}
               : (m_hovered ? Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.58f} : Color{1.0f, 1.0f, 1.0f, 0.78f}));
            border = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.06f : 0.09f};
            bottom_bd = m_disabled ? Color::transparent() : Color{0.0f, 0.0f, 0.0f, 0.16f};
            fg = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
        }

        ctx.draw_rect(r, bg, 4.0f, 0.0f);
        ctx.draw_rect(r, border, 4.0f, 1.0f);
        if (bottom_bd.a > 0.001f) {
            ctx.draw_rect(Rect::from_xywh(4.0f, r.height() - 1.0f, std::max(0.0f, r.width() - 8.0f), 1.0f), bottom_bd, 0.0f, 0.0f);
        }

        float tx = 12.0f;
        if (!m_svg_path.empty()) {
            Rect ir = Rect::from_xywh(tx, std::round((r.height() - 15.0f) * 0.5f), 15.0f, 15.0f);
            SvgEngine::draw_svg_path(ctx, m_svg_path, 24.0f, 24.0f, ir, fg);
            tx += 21.0f;
        }
        Vec2 tsz = TextMeasure::measure(m_text, 13.0f);
        ctx.draw_text(m_text, PointF{std::round(tx), std::round((r.height() - tsz.y) * 0.5f)}, 13.0f, fg);

        // 右侧精致 V 形下拉箭头
        float cx = std::round(r.width() - 15.0f);
        float cy = std::round(r.height() * 0.5f) + (m_pressed ? 1.0f : 0.0f);
        ctx.draw_line(PointF{cx - 3.6f, cy - 1.8f}, PointF{cx, cy + 1.8f}, fg, 1.2f);
        ctx.draw_line(PointF{cx, cy + 1.8f}, PointF{cx + 3.6f, cy - 1.8f}, fg, 1.2f);
    }

private:
    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) { m_hovered = true; mark_needs_paint(); };
        on_pointer_exit  = [this](PointerEvent&) { m_hovered = false; m_pressed = false; mark_needs_paint(); };
        on_pointer_down  = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            auto now = std::chrono::steady_clock::now();
            if (m_last_dismiss.time_since_epoch().count() != 0 &&
                std::chrono::duration<float>(now - m_last_dismiss).count() < 0.06f) {
                e.stop_propagation();
                return;
            }
            m_pressed = true;
            if (Window* win = Window::active_window()) {
                Vec2 anchor = global_offset();
                anchor.y += win->titlebar_height() + size().y + 4.0f;
                m_menu_open = true;
                MenuFlyout::show_at(m_items, anchor, [this]() {
                    m_menu_open = false;
                    m_last_dismiss = std::chrono::steady_clock::now();
                    mark_needs_paint();
                });
            }
            e.stop_propagation();
            mark_needs_paint();
        };
        on_pointer_up = [this](PointerEvent&) {
            if (m_pressed) { m_pressed = false; mark_needs_paint(); }
        };
    }

    std::string m_text;
    std::string m_svg_path;
    std::vector<MenuFlyoutItem> m_items;
    bool m_disabled{false};
    bool m_hovered{false};
    bool m_pressed{false};
    bool m_menu_open{false};
    std::chrono::steady_clock::time_point m_last_dismiss;
};

struct DropDownButton {
    Bindable<std::string> text{"操作"};
    std::string svg_path{""};
    std::vector<MenuFlyoutItem> items;
    Bindable<bool> disabled{false};

    operator WidgetPtr() const;
};

class DropDownButtonWidget : public Widget {
public:
    explicit DropDownButtonWidget(const DropDownButton& desc) : m_desc(desc) {}
    ~DropDownButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderDropDownButton>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto b = std::dynamic_pointer_cast<RenderDropDownButton>(node)) apply(b);
    }

private:
    void apply(const std::shared_ptr<RenderDropDownButton>& node) {
        node->set_props(m_desc.text.get(), m_desc.svg_path, m_desc.items, m_desc.disabled.get());
    }

    DropDownButton m_desc;
};

inline DropDownButton::operator WidgetPtr() const {
    return std::make_shared<DropDownButtonWidget>(*this);
}

// =============================================================================
// 2. WinUI 3 SplitButton (左主操作区 + 1px 竖分割线 + 右独立下拉区)
// =============================================================================
class RenderSplitButton : public RenderBox {
public:
    RenderSplitButton() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderSplitButton() override = default;

    void set_props(std::string text, std::string svg_path, bool accent,
                   std::vector<MenuFlyoutItem> items, bool disabled, std::function<void()> on_click) {
        bool layout_changed = (m_text != text) || (m_svg_path != svg_path);
        m_text = std::move(text);
        m_svg_path = std::move(svg_path);
        m_accent = accent;
        m_items = std::move(items);
        m_disabled = disabled;
        m_on_click = std::move(on_click);
        cursor_shape = disabled ? CursorShape::Forbidden : CursorShape::PointingHand;
        if (layout_changed) mark_needs_layout();
        else mark_needs_paint();
    }

    void perform_layout() override {
        Vec2 tsz = TextMeasure::measure(m_text, 13.0f);
        float icon_w = m_svg_path.empty() ? 0.0f : 21.0f;
        float w = std::ceil(12.0f + icon_w + tsz.x + 12.0f + kDropW);
        set_size(m_constraints.constrain(Vec2{std::max(100.0f, w), 32.0f}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        Rect r = bounds();
        const float split_x = std::max(0.0f, r.width() - kDropW);
        Rect left_r = Rect::from_xywh(0.0f, 0.0f, split_x, r.height());
        Rect right_r = Rect::from_xywh(split_x, 0.0f, kDropW, r.height());

        auto zone_bg = [&](bool hov, bool prs) -> Color {
            if (m_accent) {
                if (theme.is_dark) {
                    if (m_disabled) return Color{1.0f, 1.0f, 1.0f, 0.12f};
                    if (prs) return Color{76.0f / 255.0f, 180.0f / 255.0f, 229.0f / 255.0f, 1.0f};
                    if (hov) return Color{108.0f / 255.0f, 210.0f / 255.0f, 255.0f / 255.0f, 1.0f};
                    return Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};
                } else {
                    if (m_disabled) return Color{0.0f, 0.0f, 0.0f, 0.16f};
                    if (prs) return Color{49.0f / 255.0f, 126.0f / 255.0f, 200.0f / 255.0f, 1.0f};
                    if (hov) return Color{25.0f / 255.0f, 110.0f / 255.0f, 192.0f / 255.0f, 1.0f};
                    return Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
                }
            } else {
                if (theme.is_dark) {
                    if (m_disabled) return Color{1.0f, 1.0f, 1.0f, 0.04f};
                    if (prs) return Color{1.0f, 1.0f, 1.0f, 0.035f};
                    if (hov) return Color{1.0f, 1.0f, 1.0f, 0.088f};
                    return Color{1.0f, 1.0f, 1.0f, 0.060f};
                } else {
                    if (m_disabled) return Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.35f};
                    if (prs) return Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.35f};
                    if (hov) return Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.58f};
                    return Color{1.0f, 1.0f, 1.0f, 0.78f};
                }
            }
        };

        Color fg = m_accent
            ? (theme.is_dark ? Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.40f : 0.90f} : Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.55f : 1.0f})
            : (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.95f} : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f});

        // 左右两区独立背景与悬停态
        ctx.draw_rounded_rect(left_r,  zone_bg(m_hover_zone == 1, m_press_zone == 1), 4.0f, 0.0f, 0.0f, 4.0f);
        ctx.draw_rounded_rect(right_r, zone_bg(m_hover_zone == 2, m_press_zone == 2 || m_menu_open), 0.0f, 4.0f, 4.0f, 0.0f);

        // 中间 1px 竖向分割线
        Color div_col = m_accent
            ? (theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.20f} : Color{1.0f, 1.0f, 1.0f, 0.25f})
            : (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.12f} : Color{0.0f, 0.0f, 0.0f, 0.12f});
        ctx.draw_rect(Rect::from_xywh(split_x, 1.0f, 1.0f, std::max(0.0f, r.height() - 2.0f)), div_col, 0.0f, 0.0f);

        // 外边框
        if (!m_accent) {
            Color border = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.085f} : Color{0.0f, 0.0f, 0.0f, 0.09f};
            ctx.draw_rect(r, border, 4.0f, 1.0f);
        }

        // 左区图标与文字
        float tx = 12.0f;
        if (!m_svg_path.empty()) {
            Rect ir = Rect::from_xywh(tx, std::round((r.height() - 15.0f) * 0.5f), 15.0f, 15.0f);
            SvgEngine::draw_svg_path(ctx, m_svg_path, 24.0f, 24.0f, ir, fg);
            tx += 21.0f;
        }
        Vec2 tsz = TextMeasure::measure(m_text, 13.0f);
        ctx.draw_text(m_text, PointF{std::round(tx), std::round((r.height() - tsz.y) * 0.5f)}, 13.0f, fg);

        // 右区下拉箭头
        float cx = std::round(split_x + kDropW * 0.5f);
        float cy = std::round(r.height() * 0.5f) + (m_press_zone == 2 ? 1.0f : 0.0f);
        ctx.draw_line(PointF{cx - 3.6f, cy - 1.8f}, PointF{cx, cy + 1.8f}, fg, 1.2f);
        ctx.draw_line(PointF{cx, cy + 1.8f}, PointF{cx + 3.6f, cy - 1.8f}, fg, 1.2f);
    }

private:
    static constexpr float kDropW = 30.0f;

    int zone_at(float lx) const noexcept {
        return (lx >= size().x - kDropW) ? 2 : 1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            if (m_disabled) return;
            int z = zone_at(e.local_position.x);
            if (z != m_hover_zone) {
                m_hover_zone = z;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hover_zone != 0 || m_press_zone != 0) {
                m_hover_zone = 0;
                m_press_zone = 0;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            int z = zone_at(e.local_position.x);
            if (z == 2) {
                auto now = std::chrono::steady_clock::now();
                if (m_last_dismiss.time_since_epoch().count() != 0 &&
                    std::chrono::duration<float>(now - m_last_dismiss).count() < 0.06f) {
                    e.stop_propagation();
                    return;
                }
                m_press_zone = 2;
                if (Window* win = Window::active_window()) {
                    Vec2 anchor = global_offset();
                    anchor.y += win->titlebar_height() + size().y + 4.0f;
                    m_menu_open = true;
                    MenuFlyout::show_at(m_items, anchor, [this]() {
                        m_menu_open = false;
                        m_last_dismiss = std::chrono::steady_clock::now();
                        mark_needs_paint();
                    });
                }
            } else {
                m_press_zone = 1;
            }
            e.stop_propagation();
            mark_needs_paint();
        };
        on_pointer_up = [this](PointerEvent& e) {
            int p = m_press_zone;
            m_press_zone = 0;
            if (p == 1 && zone_at(e.local_position.x) == 1 && !m_disabled) {
                if (m_on_click) m_on_click();
            }
            mark_needs_paint();
        };
    }

    std::string m_text;
    std::string m_svg_path;
    bool m_accent{false};
    std::vector<MenuFlyoutItem> m_items;
    bool m_disabled{false};
    std::function<void()> m_on_click;
    int m_hover_zone{0}; // 0=none, 1=left, 2=right
    int m_press_zone{0};
    bool m_menu_open{false};
    std::chrono::steady_clock::time_point m_last_dismiss;
};

struct SplitButton {
    Bindable<std::string> text{"保存"};
    std::string svg_path{""};
    bool accent{false};
    std::vector<MenuFlyoutItem> items;
    Bindable<bool> disabled{false};
    std::function<void()> on_click{nullptr};

    operator WidgetPtr() const;
};

class SplitButtonWidget : public Widget {
public:
    explicit SplitButtonWidget(const SplitButton& desc) : m_desc(desc) {}
    ~SplitButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderSplitButton>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto b = std::dynamic_pointer_cast<RenderSplitButton>(node)) apply(b);
    }

private:
    void apply(const std::shared_ptr<RenderSplitButton>& node) {
        node->set_props(m_desc.text.get(), m_desc.svg_path, m_desc.accent,
                        m_desc.items, m_desc.disabled.get(), m_desc.on_click);
    }

    SplitButton m_desc;
};

inline SplitButton::operator WidgetPtr() const {
    return std::make_shared<SplitButtonWidget>(*this);
}

} // namespace ktui
