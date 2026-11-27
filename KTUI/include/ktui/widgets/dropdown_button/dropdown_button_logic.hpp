#pragma once

#include "dropdown_button_data.hpp"
#include "dropdown_button_style.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {


// =============================================================================
// 1. WinUI 3 DropDownButton (复用真实 RenderButton + 右侧下拉箭头)
// =============================================================================
class RenderDropDownButton : public RenderBox {
public:
    RenderDropDownButton() noexcept {
        m_btn = make_render_node<RenderButton>();
        m_btn->set_variant(ButtonVariant::Standard);
        m_btn->set_custom_padding(EdgeInsets{5.0f, 28.0f, 5.0f, 12.0f});
        m_btn->set_click_handler([this]() { open_menu(); });
        append_child(m_btn.get());
    }

    ~RenderDropDownButton() override {
        if (m_btn && m_btn->parent() == this) {
            remove_child(m_btn.get());
        }
    }

    void set_props(std::string text, std::string svg_path, std::vector<MenuFlyoutItem> items, bool disabled) {
        m_items = std::move(items);
        m_disabled = disabled;
        if (m_btn) {
            m_btn->set_text(std::move(text));
            m_btn->set_svg_path(std::move(svg_path));
            m_btn->set_disabled(disabled);
        }
        mark_needs_layout();
    }

    [[nodiscard]] const std::shared_ptr<RenderButton>& button() const noexcept { return m_btn; }

    void perform_layout() override {
        if (m_btn) {
            m_btn->layout(m_constraints, true);
            float w = std::max(84.0f, m_btn->size().x);
            float h = m_btn->size().y;
            Vec2 final_sz = m_constraints.constrain(Vec2{w, h});
            m_btn->layout(BoxConstraints::tight(final_sz), false);
            m_btn->set_offset(Vec2{0.0f, 0.0f});
            set_size(final_sz);
        } else {
            set_size(m_constraints.constrain(Vec2{84.0f, 32.0f}));
        }
    }

    void paint(PaintContext& ctx) override {
        if (!m_btn) return;
        {
            PaintContext::OffsetScope scope(ctx, m_btn->offset());
            m_btn->paint(ctx);
        }
        // 右侧精致 V 形下拉箭头 (颜色与 RenderButton 当前文本色完全同步)
        Color fg = m_btn->resolve_current_style().text;
        float cx = std::round(size().x - 15.0f);
        float cy = std::round(size().y * 0.5f) + (m_btn->state().is_pressed ? 1.0f : 0.0f);
        ctx.draw_line(PointF{cx - 3.6f, cy - 1.8f}, PointF{cx, cy + 1.8f}, fg, 1.2f);
        ctx.draw_line(PointF{cx, cy + 1.8f}, PointF{cx + 3.6f, cy - 1.8f}, fg, 1.2f);
    }

private:
    void open_menu() {
        if (m_disabled) return;
        auto now = std::chrono::steady_clock::now();
        if (m_last_dismiss.time_since_epoch().count() != 0 &&
            std::chrono::duration<float>(now - m_last_dismiss).count() < 0.06f) {
            return;
        }
        if (Window* win = Window::active_window()) {
            Vec2 anchor = global_offset();
            anchor.y += win->titlebar_height() + size().y + 4.0f;
            m_menu_open = true;
            if (m_btn) m_btn->set_forced_pressed(true);
            MenuFlyout::show_at(m_items, anchor, [this]() {
                m_menu_open = false;
                m_last_dismiss = std::chrono::steady_clock::now();
                if (m_btn) m_btn->set_forced_pressed(false);
                mark_needs_paint();
            });
        }
    }

    std::shared_ptr<RenderButton> m_btn;
    std::vector<MenuFlyoutItem> m_items;
    bool m_disabled{false};
    bool m_menu_open{false};
    std::chrono::steady_clock::time_point m_last_dismiss;
};

struct DropDownButton : BindableMixin<DropDownButton, RenderDropDownButton> {
    std::string text{FrameworkStrings::current().action};
    std::string svg_path{""};
    std::vector<MenuFlyoutItem> items;
    bool disabled{false};
    std::optional<bool> enable_pointer_reveal{std::nullopt};
    bool tactile_press{false};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class DropDownButtonWidget : public Widget {
public:
    explicit DropDownButtonWidget(const DropDownButton& desc) : m_desc(desc) {}
    explicit DropDownButtonWidget(DropDownButton&& desc) noexcept : m_desc(std::move(desc)) {}
    ~DropDownButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderDropDownButton>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto b = std::static_pointer_cast<RenderDropDownButton>(node)) apply(b);
    }

private:
    void apply(const std::shared_ptr<RenderDropDownButton>& node) {
        node->set_props(m_desc.text, m_desc.svg_path, m_desc.items, m_desc.disabled);
        if (node->button()) {
            node->button()->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
            node->button()->set_tactile_press(m_desc.tactile_press);
        }
    }

    DropDownButton m_desc;
};

inline DropDownButton::operator WidgetPtr() const& {
    return make_widget<DropDownButtonWidget>(*this);
}
inline DropDownButton::operator WidgetPtr() && {
    return make_widget<DropDownButtonWidget>(std::move(*this));
}

// =============================================================================
// 2. WinUI 3 SplitButton (复用真实 RenderButton + 完整涟漪 + 一体化外框与中间竖分隔线)
// =============================================================================
class RenderSplitButton : public RenderBox {
public:
    RenderSplitButton() noexcept {
        m_main_btn = make_render_node<RenderButton>();
        m_main_btn->set_corner_radii(4.0f, 0.0f, 0.0f, 4.0f);
        m_main_btn->set_border_width(0.0f);
        m_main_btn->set_click_handler([this]() {
            if (!m_disabled && m_on_click) m_on_click();
        });

        m_drop_btn = make_render_node<RenderButton>();
        m_drop_btn->set_corner_radii(0.0f, 4.0f, 4.0f, 0.0f);
        m_drop_btn->set_border_width(0.0f);
        m_drop_btn->set_custom_padding(EdgeInsets{0.0f, 0.0f, 0.0f, 0.0f});
        m_drop_btn->set_click_handler([this]() { open_menu(); });

        append_child(m_main_btn.get());
        append_child(m_drop_btn.get());

        // 兼容直接向 RenderSplitButton 派发指针事件的调用方，按点击区域转发给对应子 RenderButton
        on_pointer_down = [this](PointerEvent& e) {
            float drop_w = (e.device_kind == PointerDeviceKind::Touch) ? std::max(kDropW, 40.0f) : kDropW;
            float split_x = std::max(0.0f, size().x - drop_w);
            auto& target = (e.local_position.x >= split_x) ? m_drop_btn : m_main_btn;
            if (target && target->on_pointer_down) {
                PointerEvent child_e = e;
                child_e.local_position = e.local_position - target->offset();
                target->on_pointer_down(child_e);
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            float drop_w = (e.device_kind == PointerDeviceKind::Touch) ? std::max(kDropW, 40.0f) : kDropW;
            float split_x = std::max(0.0f, size().x - drop_w);
            auto& target = (e.local_position.x >= split_x) ? m_drop_btn : m_main_btn;
            if (target && target->on_pointer_up) {
                PointerEvent child_e = e;
                child_e.local_position = e.local_position - target->offset();
                target->on_pointer_up(child_e);
            }
        };
        on_pointer_cancel = [this](PointerEvent& e) {
            if (m_main_btn && m_main_btn->on_pointer_cancel) m_main_btn->on_pointer_cancel(e);
            if (m_drop_btn && m_drop_btn->on_pointer_cancel) m_drop_btn->on_pointer_cancel(e);
        };
    }

    ~RenderSplitButton() override {
        if (m_main_btn && m_main_btn->parent() == this) remove_child(m_main_btn.get());
        if (m_drop_btn && m_drop_btn->parent() == this) remove_child(m_drop_btn.get());
    }

    void set_props(std::string text, std::string svg_path, bool accent,
                   std::vector<MenuFlyoutItem> items, bool disabled, Fn<void()> on_click) {
        m_accent = accent;
        m_items = std::move(items);
        m_disabled = disabled;
        m_on_click = std::move(on_click);

        ButtonVariant v = accent ? ButtonVariant::Accent : ButtonVariant::Standard;
        if (m_main_btn) {
            m_main_btn->set_text(std::move(text));
            m_main_btn->set_svg_path(std::move(svg_path));
            m_main_btn->set_variant(v);
            m_main_btn->set_disabled(disabled);
        }
        if (m_drop_btn) {
            m_drop_btn->set_variant(v);
            m_drop_btn->set_disabled(disabled);
        }
        mark_needs_layout();
    }

    [[nodiscard]] const std::shared_ptr<RenderButton>& main_button() const noexcept { return m_main_btn; }
    [[nodiscard]] const std::shared_ptr<RenderButton>& drop_button() const noexcept { return m_drop_btn; }

    void perform_layout() override {
        float h = 32.0f;
        float main_w = 70.0f;
        if (m_main_btn) {
            m_main_btn->layout(BoxConstraints::loose(Vec2{1000.0f, h}), true);
            main_w = std::max(70.0f, m_main_btn->size().x);
            h = m_main_btn->size().y;
        }
        Vec2 total = m_constraints.constrain(Vec2{main_w + kDropW, h});
        set_size(total);

        float left_w = std::max(0.0f, total.x - kDropW);
        if (m_main_btn) {
            m_main_btn->layout(BoxConstraints::tight(Vec2{left_w, total.y}), false);
            m_main_btn->set_offset(Vec2{0.0f, 0.0f});
        }
        if (m_drop_btn) {
            m_drop_btn->layout(BoxConstraints::tight(Vec2{kDropW, total.y}), false);
            m_drop_btn->set_offset(Vec2{left_w, 0.0f});
        }
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;
        const float split_x = std::round(std::max(0.0f, w - kDropW));
        const Rect full_r = Rect::from_xywh(0.0f, 0.0f, w, h);

        // 1. 直接复用真实 RenderButton 渲染左右两区（完整保留独立 Hover/Press/点击涟漪/图标文字排版，无内部拼缝边框）
        if (m_main_btn) {
            PaintContext::OffsetScope scope(ctx, m_main_btn->offset());
            m_main_btn->paint(ctx);
        }
        if (m_drop_btn) {
            PaintContext::OffsetScope scope(ctx, m_drop_btn->offset());
            m_drop_btn->paint(ctx);
        }

        // 2. 统一一体化 1px 外描边 + WinUI 3 ControlElevationBorder
        auto main_style = m_main_btn ? m_main_btn->resolve_current_style() : ResolvedButtonStyle{};
        if (main_style.border.a > 0.001f) {
            ctx.draw_rect(full_r, main_style.border, 4.0f, 1.0f);
        }
        if (!m_disabled && main_style.elevation_edge.a > 0.001f) {
            bool any_pressed = (m_main_btn && m_main_btn->state().is_pressed) ||
                               (m_drop_btn && m_drop_btn->state().is_pressed) || m_menu_open;
            if (!any_pressed) {
                float ey = main_style.elevation_on_top ? 0.0f : (h - 1.0f);
                ctx.draw_rect(Rect::from_xywh(3.0f, ey, std::max(0.0f, w - 6.0f), 1.0f),
                              main_style.elevation_edge, 0.0f, 0.0f);
            }
        }

        // 3. 中间 1px 一体化竖分隔线
        Color div = m_accent
            ? (theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.24f} : Color{1.0f, 1.0f, 1.0f, 0.30f})
            : (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.11f} : Color{0.0f, 0.0f, 0.0f, 0.10f});
        ctx.draw_rect(Rect::from_xywh(split_x, 1.0f, 1.0f, std::max(0.0f, h - 2.0f)), div, 0.0f, 0.0f);

        // 4. 右侧 V 形下拉箭头（颜色与右按钮当前样式完全同步）
        if (m_drop_btn) {
            Color fg = m_drop_btn->resolve_current_style().text;
            float cx = std::round(split_x + (w - split_x) * 0.5f);
            float cy = std::round(h * 0.5f) + (m_drop_btn->state().is_pressed ? 1.0f : 0.0f);
            ctx.draw_line(PointF{cx - 3.6f, cy - 1.8f}, PointF{cx, cy + 1.8f}, fg, 1.25f);
            ctx.draw_line(PointF{cx, cy + 1.8f}, PointF{cx + 3.6f, cy - 1.8f}, fg, 1.25f);
        }
    }

    void set_visual_polish(std::optional<bool> pointer_reveal, bool tactile_press) {
        if (m_main_btn) {
            m_main_btn->set_pointer_reveal_enabled(pointer_reveal);
            m_main_btn->set_tactile_press(tactile_press);
        }
        if (m_drop_btn) {
            m_drop_btn->set_pointer_reveal_enabled(pointer_reveal);
            m_drop_btn->set_tactile_press(tactile_press);
        }
    }

private:
    static constexpr float kDropW = 30.0f;

    void open_menu() {
        if (m_disabled) return;
        auto now = std::chrono::steady_clock::now();
        if (m_last_dismiss.time_since_epoch().count() != 0 &&
            std::chrono::duration<float>(now - m_last_dismiss).count() < 0.06f) {
            return;
        }
        if (Window* win = Window::active_window()) {
            Vec2 anchor = global_offset();
            anchor.y += win->titlebar_height() + size().y + 4.0f;
            m_menu_open = true;
            if (m_drop_btn) m_drop_btn->set_forced_pressed(true);
            MenuFlyout::show_at(m_items, anchor, [this]() {
                m_menu_open = false;
                m_last_dismiss = std::chrono::steady_clock::now();
                if (m_drop_btn) m_drop_btn->set_forced_pressed(false);
                mark_needs_paint();
            });
        }
    }

    std::shared_ptr<RenderButton> m_main_btn;
    std::shared_ptr<RenderButton> m_drop_btn;
    bool m_accent{false};
    std::vector<MenuFlyoutItem> m_items;
    bool m_disabled{false};
    Fn<void()> m_on_click;
    bool m_menu_open{false};
    std::chrono::steady_clock::time_point m_last_dismiss;
};

struct SplitButton : BindableMixin<SplitButton, RenderSplitButton> {
    std::string text{FrameworkStrings::current().save};
    std::string svg_path{""};
    bool accent{false};
    std::vector<MenuFlyoutItem> items;
    bool disabled{false};
    std::optional<bool> enable_pointer_reveal{std::nullopt};
    bool tactile_press{false};
    Fn<void()> on_click{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class SplitButtonWidget : public Widget {
public:
    explicit SplitButtonWidget(const SplitButton& desc) : m_desc(desc) {}
    explicit SplitButtonWidget(SplitButton&& desc) noexcept : m_desc(std::move(desc)) {}
    ~SplitButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderSplitButton>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto b = std::static_pointer_cast<RenderSplitButton>(node)) apply(b);
    }

private:
    void apply(const std::shared_ptr<RenderSplitButton>& node) {
        node->set_props(m_desc.text, m_desc.svg_path, m_desc.accent,
                        m_desc.items, m_desc.disabled, m_desc.on_click);
        node->set_visual_polish(m_desc.enable_pointer_reveal, m_desc.tactile_press);
    }

    SplitButton m_desc;
};

inline SplitButton::operator WidgetPtr() const& {
    return make_widget<SplitButtonWidget>(*this);
}
inline SplitButton::operator WidgetPtr() && {
    return make_widget<SplitButtonWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
