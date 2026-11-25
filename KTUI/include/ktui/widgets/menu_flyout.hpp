#pragma once

#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/common/fluent_chrome.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// WinUI 3 MenuFlyout / ContextMenu 菜单项
struct MenuFlyoutItem {
    std::string label;
    std::string shortcut;      // 右对齐快捷键提示 (如 "Ctrl+C")
    std::string svg_path;      // 可选 24x24 Fluent SVG Path 图标
    bool is_separator{false};
    bool is_checkable{false};
    bool checked{false};
    bool disabled{false};
    Fn<void()> on_click{nullptr};

    static MenuFlyoutItem separator() {
        MenuFlyoutItem it;
        it.is_separator = true;
        return it;
    }
};

// WinUI 3 MenuFlyout 弹出菜单渲染盒 (RenderMenuFlyout)
class RenderMenuFlyout : public RenderBox {
public:
    RenderMenuFlyout(std::vector<MenuFlyoutItem> items, Fn<void()> on_close)
        : m_items(std::move(items)), m_on_close(std::move(on_close)) {
        cursor_shape = CursorShape::Arrow;
        for (const auto& it : m_items) {
            if (!it.svg_path.empty() || it.is_checkable) {
                m_has_leading_col = true;
                break;
            }
        }
        setup_behavior();
    }

    ~RenderMenuFlyout() override = default;

    [[nodiscard]] Vec2 compute_ideal_size() const {
        float max_w = 160.0f;
        float total_h = 8.0f; // 上下各 4px 内边距
        const float leading = m_has_leading_col ? 26.0f : 0.0f;

        for (const auto& it : m_items) {
            if (it.is_separator) {
                total_h += kSepH;
                continue;
            }
            total_h += kItemH;
            float lw = TextMeasure::measure_width(it.label, 13.0f);
            float sw = it.shortcut.empty() ? 0.0f : (TextMeasure::measure_width(it.shortcut, 11.5f) + 28.0f);
            max_w = std::max(max_w, 24.0f + leading + lw + sw + 16.0f);
        }
        return Vec2{std::ceil(std::min(max_w, 340.0f)), total_h};
    }

    void perform_layout() override {
        set_size(compute_ideal_size());
    }

    void paint(PaintContext& ctx) override {
        auto st = m_surface.tick();
        if (st.animating) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        float anim_h = std::max(16.0f, size().y * (0.76f + 0.24f * st.ease));
        float alpha = st.alpha;

        Color sep_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.08f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.08f * alpha};
        Color hov_bg = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.065f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.042f * alpha};
        Color prs_bg = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.04f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.025f * alpha};
        Color text_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.95f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.90f * alpha};
        Color muted_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.54f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.52f * alpha};
        Color dis_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.36f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.36f * alpha};

        // 1. 复用 FluentPopupSurface 绘制 32dp 真高斯阴影 + 8px 圆角浮层底板
        FluentPopupSurface::paint_panel(ctx, theme, Rect::from_xywh(0.0f, 0.0f, size().x, anim_h), alpha);

        // 2. 菜单项绘制
        ctx.push_clip(Rect::from_xywh(4.0f, 4.0f, std::max(0.0f, size().x - 8.0f), std::max(0.0f, anim_h - 8.0f)), 4.0f);
        float y = 4.0f;
        const float leading = m_has_leading_col ? 26.0f : 0.0f;

        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            const auto& it = m_items[static_cast<size_t>(i)];
            if (it.is_separator) {
                ctx.draw_rect(Rect::from_xywh(4.0f, std::round(y + kSepH * 0.5f),
                                              std::max(0.0f, size().x - 8.0f), 1.0f),
                              sep_col, 0.0f, 0.0f);
                y += kSepH;
                continue;
            }

            Rect r = Rect::from_xywh(4.0f, y + 1.0f, std::max(0.0f, size().x - 8.0f), kItemH - 2.0f);
            if (!it.disabled) {
                if (m_pressed_index == i) ctx.draw_rect(r, prs_bg, 4.0f, 0.0f);
                else if (m_hovered_index == i) ctx.draw_rect(r, hov_bg, 4.0f, 0.0f);
            }

            Color tc = it.disabled ? dis_col : text_col;
            if (m_has_leading_col) {
                Rect icon_r = Rect::from_xywh(14.0f, std::round(y + (kItemH - 15.0f) * 0.5f), 15.0f, 15.0f);
                if (it.is_checkable && it.checked) {
                    static constexpr const char* kCheckSvg =
                        "M4.53 12.97a.75.75 0 0 0-1.06 1.06l4.5 4.5a.75.75 0 0 0 1.06 0l11-11a.75.75 0 0 0-1.06-1.06L8.5 16.94l-3.97-3.97Z";
                    SvgEngine::draw_svg_path(ctx, kCheckSvg, 24.0f, 24.0f, icon_r, tc);
                } else if (!it.svg_path.empty()) {
                    SvgEngine::draw_svg_path(ctx, it.svg_path, 24.0f, 24.0f, icon_r, tc);
                }
            }

            Vec2 lsz = TextMeasure::measure(it.label, 13.0f);
            ctx.draw_text(it.label,
                          PointF{std::round(12.0f + leading), std::round(y + (kItemH - lsz.y) * 0.5f)},
                          13.0f, tc);

            if (!it.shortcut.empty()) {
                Vec2 ssz = TextMeasure::measure(it.shortcut, 11.5f);
                ctx.draw_text(it.shortcut,
                              PointF{std::round(size().x - 14.0f - ssz.x), std::round(y + (kItemH - ssz.y) * 0.5f)},
                              11.5f, it.disabled ? dis_col : muted_col);
            }

            y += kItemH;
        }
        ctx.pop_clip();
    }

private:
    static constexpr float kItemH = 32.0f;
    static constexpr float kSepH = 9.0f;

    int hit_index(float ly) const noexcept {
        float y = 4.0f;
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            float h = m_items[static_cast<size_t>(i)].is_separator ? kSepH : kItemH;
            if (ly >= y && ly < y + h) {
                return m_items[static_cast<size_t>(i)].is_separator ? -1 : i;
            }
            y += h;
        }
        return -1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int idx = hit_index(e.local_position.y);
            if (idx != m_hovered_index) {
                m_hovered_index = idx;
                mark_needs_paint();
            }
            e.stop_propagation();
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_index != -1 || m_pressed_index != -1) {
                m_hovered_index = -1;
                m_pressed_index = -1;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int idx = hit_index(e.local_position.y);
            if (idx >= 0 && !m_items[static_cast<size_t>(idx)].disabled) {
                m_pressed_index = idx;
                mark_needs_paint();
            }
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& e) {
            int idx = hit_index(e.local_position.y);
            int pressed = m_pressed_index;
            m_pressed_index = -1;
            if (idx >= 0 && idx == pressed && !m_items[static_cast<size_t>(idx)].disabled) {
                auto cb = m_items[static_cast<size_t>(idx)].on_click;
                auto close_cb = m_on_close;
                e.stop_propagation();
                if (close_cb) close_cb(); // 关闭浮层会移除当前节点，此后严禁再访问 this
                if (cb) cb();
                return;
            }
            mark_needs_paint();
            e.stop_propagation();
        };
    }

    std::vector<MenuFlyoutItem> m_items;
    Fn<void()> m_on_close;
    bool m_has_leading_col{false};
    int m_hovered_index{-1};
    int m_pressed_index{-1};
    FluentPopupSurface m_surface;
};

// 静态便捷菜单弹出器 (MenuFlyout::show_at)
struct MenuFlyout {
    static uint64_t show_at(std::vector<MenuFlyoutItem> items,
                            Vec2 window_pos,
                            Fn<void()> on_dismiss = nullptr) {
        Window* win = Window::active_window();
        if (!win || items.empty()) return 0;

        auto overlay_id = std::make_shared<uint64_t>(0);
        auto menu = make_render_node<RenderMenuFlyout>(
            std::move(items),
            [overlay_id]() {
                if (Window* w = Window::active_window(); w && *overlay_id != 0) {
                    uint64_t id = *overlay_id;
                    *overlay_id = 0;
                    w->hide_overlay(id);
                }
            });

        Vec2 sz = menu->compute_ideal_size();
        Vec2 win_sz = win->size();
        float gx = (window_pos.x + sz.x > win_sz.x - 6.0f) ? std::max(6.0f, window_pos.x - sz.x) : std::max(6.0f, window_pos.x);
        float gy = (window_pos.y + sz.y > win_sz.y - 6.0f) ? std::max(win->titlebar_height() + 6.0f, window_pos.y - sz.y) : std::max(win->titlebar_height() + 6.0f, window_pos.y);

        OverlayEntry entry;
        entry.content = menu;
        entry.global_pos = Vec2{std::round(gx), std::round(gy)};
        entry.size = sz;
        entry.dismiss_on_outside_click = true;
        entry.is_modal = false;
        entry.on_dismiss = std::move(on_dismiss);

        *overlay_id = win->show_overlay(std::move(entry));
        return *overlay_id;
    }
};

// 右键上下文菜单容器 (ContextMenuArea): 为任意子控件绑定右键 / 触控长按弹出 MenuFlyout
class RenderContextMenuArea : public RenderBox {
public:
    RenderContextMenuArea() noexcept {
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button == PointerButton::Secondary && !m_items.empty()) {
                Window* win = Window::active_window();
                Vec2 win_pos = e.position;
                if (win) win_pos.y += win->titlebar_height();
                MenuFlyout::show_at(m_items, win_pos);
                e.stop_propagation();
            }
        };
    }

    ~RenderContextMenuArea() override = default;

    void set_items(std::vector<MenuFlyoutItem> items) { m_items = std::move(items); }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child_box == child) return;
        if (m_child_box) remove_child(m_child_box.get());
        m_child_box = std::move(child);
        if (m_child_box) append_child(m_child_box.get());
        mark_needs_layout();
    }

    [[nodiscard]] const std::shared_ptr<RenderBox>& child_box() const noexcept { return m_child_box; }

    void perform_layout() override {
        if (m_child_box) {
            m_child_box->layout(m_constraints, true);
            m_child_box->set_offset(Vec2{0.0f, 0.0f});
            set_size(m_child_box->size());
        } else {
            set_size(m_constraints.constrain(Vec2{0.0f, 0.0f}));
        }
    }

    void paint(PaintContext& ctx) override {
        if (m_child_box) m_child_box->paint(ctx);
    }

private:
    std::vector<MenuFlyoutItem> m_items;
    std::shared_ptr<RenderBox> m_child_box;
};

struct ContextMenuArea {
    std::vector<MenuFlyoutItem> items;
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class ContextMenuAreaWidget : public Widget {
public:
    explicit ContextMenuAreaWidget(const ContextMenuArea& desc) : m_desc(desc) {}
    explicit ContextMenuAreaWidget(ContextMenuArea&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ContextMenuAreaWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderContextMenuArea>();
        node->set_items(m_desc.items);
        if (m_desc.child) node->set_child_box(m_desc.child->create_render_node());
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto area = std::static_pointer_cast<RenderContextMenuArea>(node)) {
            area->set_items(m_desc.items);
            if (m_desc.child) {
                if (area->child_box()) m_desc.child->update_render_node(area->child_box());
                else area->set_child_box(m_desc.child->create_render_node());
            }
        }
    }

private:
    ContextMenuArea m_desc;
};

inline ContextMenuArea::operator WidgetPtr() const& {
    return make_widget<ContextMenuAreaWidget>(*this);
}
inline ContextMenuArea::operator WidgetPtr() && {
    return make_widget<ContextMenuAreaWidget>(std::move(*this));
}

} // namespace ktui
