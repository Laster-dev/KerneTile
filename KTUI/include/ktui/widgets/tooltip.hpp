#pragma once

#include "common/fluent_chrome.hpp"
#include "widget.hpp"
#include <ktui/animation/frame_clock.hpp>
#include <ktui/app/window.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/popup_handle.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace ktui {

// WinUI 3 ToolTip 悬浮提示浮层 (无交互穿透，120ms 平滑淡入，4px 圆角 + 柔和投影)
class RenderTooltipPopup : public RenderBox {
public:
    explicit RenderTooltipPopup(std::string text)
        : m_text(std::move(text)) {
        cursor_shape = CursorShape::Arrow;
    }

    ~RenderTooltipPopup() override = default;

    [[nodiscard]] Vec2 compute_ideal_size() const {
        if (m_text.empty()) return Vec2{0.0f, 0.0f};
        Vec2 ts = TextMeasure::measure(m_text, kFontSize);
        float w = std::clamp(std::ceil(ts.x + 20.0f), 32.0f, 320.0f);
        float h = std::max(26.0f, std::ceil(ts.y + 10.0f));
        return Vec2{w, h};
    }

    bool hit_test(HitTestResult&, PointF) override {
        // ToolTip 绝不拦截鼠标事件
        return false;
    }

    void perform_layout() override {
        set_size(compute_ideal_size());
    }

    void paint(PaintContext& ctx) override {
        auto st = m_surface.tick(0.11f);
        if (st.animating) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        const float alpha = st.alpha;
        Rect r = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);

        ctx.draw_elevation_shadow(r, 4.0f, theme.is_dark, 14.0f, alpha);
        Color bg = theme.colors.flyout_surface;
        bg.a *= alpha;
        Color stroke = theme.colors.flyout_stroke;
        stroke.a *= alpha;
        Color fg = theme.colors.text_primary;
        fg.a *= alpha;

        ctx.draw_rect(r, bg, 4.0f, 0.0f);
        ctx.draw_rect(r, stroke, 4.0f, 1.0f);

        Vec2 ts = TextMeasure::measure(m_text, kFontSize);
        ctx.push_clip(Rect::from_xywh(8.0f, 2.0f, std::max(0.0f, size().x - 16.0f), std::max(0.0f, size().y - 4.0f)), 0.0f);
        ctx.draw_text(m_text, PointF{10.0f, std::round((size().y - ts.y) * 0.5f)}, kFontSize, fg);
        ctx.pop_clip();
    }

private:
    static constexpr float kFontSize = 12.0f;
    std::string m_text;
    FluentPopupSurface m_surface;
};

// WinUI 3 Tooltip 容器 (悬停 400ms 自动弹出，移出/点击/卸载自动关闭)
class RenderTooltipArea : public RenderBox {
public:
    RenderTooltipArea() noexcept {
        setup_behavior();
    }

    ~RenderTooltipArea() override {
        dismiss();
        if (m_child && m_child->parent() == this) {
            remove_child(m_child.get());
        }
    }

    void set_properties(std::string text, float delay_sec) {
        if (m_text != text) {
            m_text = std::move(text);
            if (m_text.empty()) dismiss();
        }
        m_delay_sec = std::clamp(delay_sec, 0.05f, 3.0f);
    }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child == child) return;
        if (m_child && m_child->parent() == this) remove_child(m_child.get());
        m_child = std::move(child);
        if (m_child) append_child(m_child.get());
        mark_needs_layout();
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] bool is_tooltip_open() const noexcept { return m_popup_handle.is_open(); }

    void on_unmounted() override {
        dismiss();
    }

    void perform_layout() override {
        if (m_child) {
            m_child->layout(m_constraints, true);
            m_child->set_offset(Vec2{0.0f, 0.0f});
            set_size(m_child->size());
        } else {
            set_size(m_constraints.constrain(Vec2{0.0f, 0.0f}));
        }
    }

    void paint(PaintContext& ctx) override {
        if (m_hovered && !m_text.empty() && !m_popup_handle.is_open()) {
            double elapsed = FrameClock::global().frame_time_sec() - m_hover_start_sec;
            if (elapsed >= static_cast<double>(m_delay_sec)) {
                show_tooltip();
            } else {
                mark_needs_paint();
            }
        }
        if (m_child) {
            PaintContext::OffsetScope scope(ctx, m_child->offset());
            m_child->paint(ctx);
        }
    }

    void dismiss() {
        m_hovered = false;
        m_popup_handle.close();
    }

private:
    void show_tooltip() {
        if (m_text.empty() || m_popup_handle.is_open()) return;
        Window* win = Window::active_window();
        if (!win) return;

        auto popup = make_render_node<RenderTooltipPopup>(m_text);
        Vec2 sz = popup->compute_ideal_size();
        Vec2 g = global_offset();
        float th = win->titlebar_height();
        Vec2 win_sz = win->size();

        // 默认居中显示在目标控件正上方，顶部空间不足时自动翻转至正下方
        float gx = g.x + (size().x - sz.x) * 0.5f;
        gx = std::clamp(gx, 6.0f, std::max(6.0f, win_sz.x - sz.x - 6.0f));
        float gy = g.y + th - sz.y - 6.0f;
        if (gy < th + 6.0f) {
            gy = g.y + th + size().y + 6.0f;
        }
        gy = std::clamp(gy, th + 6.0f, std::max(th + 6.0f, win_sz.y - sz.y - 6.0f));

        OverlayEntry entry;
        entry.content = popup;
        entry.global_pos = Vec2{std::round(gx), std::round(gy)};
        entry.size = sz;
        entry.dismiss_on_outside_click = false;
        entry.is_modal = false;

        m_popup_handle.open(*win, std::move(entry));
    }

    void setup_behavior() {
        on_pointer_preview = [this](PointerEvent& e) {
            if (e.type == PlatformEventType::PointerMove) {
                if (!m_hovered && !m_text.empty()) {
                    m_hovered = true;
                    m_hover_start_sec = FrameClock::global().frame_time_sec();
                    mark_needs_paint();
                }
            } else if (e.type == PlatformEventType::PointerDown) {
                dismiss();
            }
        };
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_text.empty()) {
                m_hovered = true;
                m_hover_start_sec = FrameClock::global().frame_time_sec();
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            dismiss();
        };
    }

    std::shared_ptr<RenderBox> m_child;
    std::string m_text;
    float m_delay_sec{0.40f};
    bool m_hovered{false};
    double m_hover_start_sec{0.0};
    PopupHandle m_popup_handle;
};

// C++20 指定初始化器声明式 WinUI 3 Tooltip 包装器
struct Tooltip : BindableMixin<Tooltip, RenderTooltipArea> {
    std::string text{""};
    float delay_sec{0.40f};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class TooltipWidget : public Widget {
public:
    explicit TooltipWidget(const Tooltip& desc) : m_desc(desc) {}
    explicit TooltipWidget(Tooltip&& desc) noexcept : m_desc(std::move(desc)) {}
    ~TooltipWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTooltipArea>();
        node->set_properties(m_desc.text, m_desc.delay_sec);
        if (m_desc.child) node->set_child_box(m_desc.child->create_render_node());
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderTooltipArea>(node)) {
            n->set_properties(m_desc.text, m_desc.delay_sec);
        }
    }

private:
    Tooltip m_desc;
};

inline Tooltip::operator WidgetPtr() const& {
    return make_widget<TooltipWidget>(*this);
}
inline Tooltip::operator WidgetPtr() && {
    return make_widget<TooltipWidget>(std::move(*this));
}

} // namespace ktui
