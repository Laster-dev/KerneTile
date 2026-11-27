#pragma once

#include <ktui/core/strings.hpp>

#include "combobox_data.hpp"
#include "combobox_style.hpp"
#include <ktui/app/window.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/scroll/scrollbar.hpp>
#include <ktui/widgets/scroll/smooth_scroll_physics.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// 下拉弹出面板渲染盒 (WinUI 3 Flyout Surface - 挂载于 OverlayManager 顶层)
class RenderComboBoxPopup : public RenderBox {
public:
    RenderComboBoxPopup(std::vector<ComboBoxItem> items,
                        int selected_index,
                        float item_height,
                        float font_size,
                        int max_visible_items,
                        bool open_upward,
                        Fn<void(int)> on_select)
        : m_items(std::move(items)),
          m_selected_index(selected_index),
          m_item_height(item_height > 0.0f ? item_height : 32.0f),
          m_font_size(font_size > 0.0f ? font_size : 13.0f),
          m_max_visible_items(std::max(1, max_visible_items)),
          m_open_upward(open_upward),
          m_on_select(std::move(on_select)) {
        cursor_shape = CursorShape::Arrow;
        m_open_start = std::chrono::steady_clock::now();
        m_scrollbar.notify_activity();
        setup_behavior();
    }

    ~RenderComboBoxPopup() override = default;

    [[nodiscard]] int hovered_index() const noexcept { return m_hovered_index; }
    [[nodiscard]] float scroll_offset() const noexcept { return m_scroll.offset(); }
    [[nodiscard]] bool is_closing() const noexcept { return m_closing; }

    void start_close(Fn<void()> on_finished) {
        if (m_closing) return;
        m_closing = true;
        m_close_start = std::chrono::steady_clock::now();
        m_close_from = m_open_progress;
        m_on_close_finished = std::move(on_finished);
        mark_needs_paint();
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (m_closing) return false;
        return RenderBox::hit_test(result, position);
    }

    void perform_layout() override {
        float w = m_constraints.max_width;
        float h = m_constraints.max_height;
        if (!std::isfinite(w)) w = 180.0f;
        if (!std::isfinite(h)) h = compute_ideal_height();
        set_size(Vec2{w, h});
        float view_h = std::max(1.0f, h - 8.0f);
        float total_h = static_cast<float>(m_items.size()) * m_item_height;
        m_scroll.set_max_scroll(std::max(0.0f, total_h - view_h));
        if (!m_initialized_scroll) {
            m_initialized_scroll = true;
            ensure_selected_visible(view_h);
        }
    }

    [[nodiscard]] float compute_ideal_height() const noexcept {
        int count = static_cast<int>(m_items.size());
        int visible = std::min(std::max(1, count), m_max_visible_items);
        return static_cast<float>(visible) * m_item_height + 8.0f; // 上下各 4px 内边距
    }

    void paint(PaintContext& ctx) override {
        update_open_animation();
        bool anim_scroll = m_scroll.update();
        bool anim_bar = m_scrollbar.update_animations(m_popup_hovered);
        if (anim_scroll || anim_bar) {
            mark_needs_paint();
        }

        const Theme& theme = ThemeContext::current();
        ComboBoxData dummy;
        dummy.item_height = m_item_height;
        dummy.font_size = m_font_size;
        ComboBoxInteractiveState dummy_state;
        ResolvedComboBoxStyle style = ComboBoxStyleResolver::resolve(dummy, dummy_state, theme);

        // 流畅缓出展开动画 (140ms)
        float ease = 1.0f - std::pow(1.0f - m_open_progress, 3.0f);
        float anim_h = std::max(12.0f, size().y * (0.72f + 0.28f * ease));
        float anim_y = m_open_upward ? (size().y - anim_h) : 0.0f;
        float alpha_mul = std::clamp(ease * 1.15f, 0.0f, 1.0f);

        Rect panel_rect = Rect::from_xywh(0.0f, anim_y, size().x, anim_h);

        // 1. WinUI 3 Flyout 32dp 双层真实高斯软阴影
        ctx.draw_elevation_shadow(panel_rect, style.popup_corner_radius, theme.is_dark, 32.0f, alpha_mul);

        // 2. 超高模糊半透亚克力浮层背景板 (8px 圆角 + 磨砂颗粒 + 1px 玻璃高光描边)
        Color bg = style.popup_bg;
        bg.a *= alpha_mul;
        ctx.draw_acrylic_panel(panel_rect, bg, style.popup_corner_radius, 0.026f, 1.0f);

        Color border = style.popup_border;
        border.a *= alpha_mul;
        ctx.draw_rect(panel_rect, border, style.popup_corner_radius, style.border_width);
        {
            const Color top_hi = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.22f * alpha_mul} : Color{1.0f, 1.0f, 1.0f, 0.72f * alpha_mul};
            const Color bot_sh = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.28f * alpha_mul} : Color{0.0f, 0.0f, 0.0f, 0.12f * alpha_mul};
            ctx.draw_directional_rim(panel_rect, top_hi, bot_sh, style.popup_corner_radius, 1.0f, Vec2{-0.18f, -0.98f}, 0.26f);
        }

        // 3. 选项列表裁剪与绘制
        const float pad = style.popup_padding;
        Rect clip_rect = Rect::from_xywh(
            pad, anim_y + pad,
            std::max(0.0f, size().x - pad * 2.0f),
            std::max(0.0f, anim_h - pad * 2.0f));
        ctx.push_clip(clip_rect, style.item_corner_radius);

        const float content_w = std::max(0.0f, size().x - pad * 2.0f);
        const float total_content_h = static_cast<float>(m_items.size()) * m_item_height;
        const float view_h = std::max(1.0f, size().y - pad * 2.0f);
        const bool scrollable = total_content_h > view_h + 0.5f;
        const float item_w = scrollable ? std::max(0.0f, content_w - 10.0f) : content_w;
        const float cur_scroll = m_scroll.offset();

        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            float iy = anim_y + pad + static_cast<float>(i) * m_item_height - cur_scroll;
            if (iy + m_item_height < anim_y || iy > anim_y + anim_h) {
                continue;
            }

            const auto& item = m_items[static_cast<size_t>(i)];
            const bool item_selected = (i == m_selected_index);
            const bool item_hovered = (i == m_hovered_index && !item.disabled && !m_scrollbar.is_hovered() && !m_scrollbar.is_dragging());
            const bool item_pressed = (i == m_pressed_index && !item.disabled);

            Rect item_rect = Rect::from_xywh(pad, iy + 1.0f, item_w, std::max(0.0f, m_item_height - 2.0f));

            // 悬停 / 选中 / 按下底色
            if (item_pressed) {
                Color c = style.item_pressed_bg;
                c.a *= alpha_mul;
                ctx.draw_rect(item_rect, c, style.item_corner_radius, 0.0f);
            } else if (item_hovered) {
                Color c = style.item_hover_bg;
                c.a *= alpha_mul;
                ctx.draw_rect(item_rect, c, style.item_corner_radius, 0.0f);
            } else if (item_selected) {
                Color c = style.item_selected_bg;
                c.a *= alpha_mul;
                ctx.draw_rect(item_rect, c, style.item_corner_radius, 0.0f);
            }

            // WinUI 3 标志性左侧 3px x 16px 强调色圆角选中指示竖条
            if (item_selected) {
                float pill_h = std::min(16.0f, std::max(8.0f, m_item_height * 0.5f));
                float pill_y = iy + (m_item_height - pill_h) * 0.5f;
                Color pill_col = style.accent_pill;
                pill_col.a *= alpha_mul;
                ctx.draw_rect(Rect::from_xywh(pad + 1.0f, pill_y, 3.0f, pill_h), pill_col, 1.5f, 0.0f);
            }

            // 选项文本
            Color text_col = item.disabled ? style.item_disabled_text : style.item_text;
            text_col.a *= alpha_mul;
            Vec2 text_sz = TextMeasure::measure(item.label, style.font_size);
            float tx = pad + 11.0f;
            float ty = iy + (m_item_height - text_sz.y) * 0.5f;
            ctx.draw_text(item.label, PointF{tx, ty}, style.font_size, text_col);
        }

        ctx.pop_clip();

        // 4. WinUI 3 像素级对齐垂直滚动条 (自动收起 2px -> 悬停/拖拽平滑展开 12px + 6px 滑块 + 上下箭头)
        if (scrollable) {
            Rect sb_vp = Rect::from_xywh(0.0f, anim_y + pad, size().x, std::max(1.0f, anim_h - pad * 2.0f));
            m_scrollbar.paint(ctx, sb_vp, total_content_h, cur_scroll, theme, alpha_mul);
        }
    }

private:
    void ensure_selected_visible(float view_h) {
        float total_h = static_cast<float>(m_items.size()) * m_item_height;
        if (total_h <= view_h + 0.5f || m_selected_index < 0) {
            m_scroll.jump_to(0.0f);
            return;
        }
        float item_top = static_cast<float>(m_selected_index) * m_item_height;
        float target = item_top - (view_h - m_item_height) * 0.5f;
        m_scroll.jump_to(target);
    }

    [[nodiscard]] Rect scrollbar_viewport() const noexcept {
        constexpr float kPad = 4.0f;
        return Rect::from_xywh(0.0f, kPad, size().x, std::max(1.0f, size().y - kPad * 2.0f));
    }

    int hit_test_item_index(float local_y) const noexcept {
        constexpr float kPad = 4.0f;
        float y = local_y - kPad + m_scroll.offset();
        if (y < 0.0f) return -1;
        int idx = static_cast<int>(std::floor(y / m_item_height));
        if (idx < 0 || idx >= static_cast<int>(m_items.size())) return -1;
        return idx;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            m_popup_hovered = true;
            float total_h = static_cast<float>(m_items.size()) * m_item_height;
            Rect sb_vp = scrollbar_viewport();

            if (e.device_kind == PointerDeviceKind::Touch && m_touch_down) {
                float dy_total = e.local_position.y - m_touch_start_y;
                if (!m_touch_dragging && std::abs(dy_total) > 8.0f && m_scroll.max_scroll() > 0.0f) {
                    m_touch_dragging = true;
                    m_pressed_index = -1;
                }
                if (m_touch_dragging) {
                    float dy = e.local_position.y - m_touch_last_y;
                    m_touch_last_y = e.local_position.y;
                    m_scroll.apply_touch_drag(dy);
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
            }

            if (e.type == PlatformEventType::PointerScroll) {
                if (m_scroll.max_scroll() > 0.0f) {
                    m_scroll.add_wheel_delta(e.delta.y);
                    m_scrollbar.notify_activity();
                    m_hovered_index = hit_test_item_index(e.local_position.y);
                    mark_needs_paint();
                }
                e.stop_propagation();
                return;
            }

            bool sb_consumed = false;
            if (m_scrollbar.handle_pointer_move(e.local_position, sb_vp, total_h, m_scroll, sb_consumed)) {
                mark_needs_paint();
            }
            if (sb_consumed) {
                if (m_hovered_index != -1) {
                    m_hovered_index = -1;
                    mark_needs_paint();
                }
                e.stop_propagation();
                return;
            }

            int idx = hit_test_item_index(e.local_position.y);
            if (idx != m_hovered_index) {
                m_hovered_index = idx;
                mark_needs_paint();
            }
            e.stop_propagation();
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_popup_hovered = false;
            m_scrollbar.handle_pointer_exit();
            if (m_hovered_index != -1 || m_pressed_index != -1) {
                m_hovered_index = -1;
                m_pressed_index = -1;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& e) {
            float total_h = static_cast<float>(m_items.size()) * m_item_height;
            Rect sb_vp = scrollbar_viewport();
            if (m_scrollbar.handle_pointer_down(e.local_position, sb_vp, total_h, m_scroll, m_item_height)) {
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (e.device_kind == PointerDeviceKind::Touch) {
                m_touch_down = true;
                m_touch_dragging = false;
                m_touch_start_y = e.local_position.y;
                m_touch_last_y = e.local_position.y;
                m_scroll.begin_touch_drag();
            }
            int idx = hit_test_item_index(e.local_position.y);
            if (idx >= 0 && !m_items[static_cast<size_t>(idx)].disabled) {
                m_pressed_index = idx;
                mark_needs_paint();
            }
            e.stop_propagation();
        };

        on_pointer_up = [this](PointerEvent& e) {
            bool sb_released = m_scrollbar.handle_pointer_up();
            if (m_touch_down) {
                bool was_dragging = m_touch_dragging;
                m_touch_down = false;
                m_touch_dragging = false;
                if (was_dragging) {
                    m_scroll.end_touch_drag();
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
            }
            if (sb_released) {
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            int idx = hit_test_item_index(e.local_position.y);
            int pressed = m_pressed_index;
            m_pressed_index = -1;
            if (idx >= 0 && idx == pressed && !m_items[static_cast<size_t>(idx)].disabled) {
                if (m_on_select) {
                    m_on_select(idx);
                }
            }
            mark_needs_paint();
            e.stop_propagation();
        };
    }

    void update_open_animation() {
        auto now = std::chrono::steady_clock::now();
        if (m_closing) {
            float elapsed = std::chrono::duration<float>(now - m_close_start).count();
            float t = std::clamp(elapsed / 0.11f, 0.0f, 1.0f);
            m_open_progress = m_close_from * (1.0f - t);
            if (t < 1.0f) {
                mark_needs_paint();
            } else {
                m_open_progress = 0.0f;
                if (auto cb = std::move(m_on_close_finished)) {
                    cb();
                }
            }
            return;
        }
        if (m_open_progress >= 1.0f) return;
        float elapsed = std::chrono::duration<float>(now - m_open_start).count();
        m_open_progress = std::clamp(elapsed / 0.14f, 0.0f, 1.0f);
        if (m_open_progress < 1.0f) {
            mark_needs_paint();
        }
    }

    std::vector<ComboBoxItem> m_items;
    int m_selected_index{-1};
    int m_hovered_index{-1};
    int m_pressed_index{-1};
    float m_item_height{32.0f};
    float m_font_size{13.0f};
    int m_max_visible_items{8};
    bool m_open_upward{false};
    bool m_popup_hovered{false};
    bool m_initialized_scroll{false};
    bool m_touch_down{false};
    bool m_touch_dragging{false};
    float m_touch_start_y{0.0f};
    float m_touch_last_y{0.0f};
    SmoothScrollPhysics m_scroll;
    WinUIScrollBar m_scrollbar;
    float m_open_progress{0.0f};
    std::chrono::steady_clock::time_point m_open_start;
    bool m_closing{false};
    float m_close_from{1.0f};
    std::chrono::steady_clock::time_point m_close_start;
    Fn<void()> m_on_close_finished;
    Fn<void(int)> m_on_select;
};

// 下拉选择框逻辑与渲染盒 (RenderComboBox - Logic Layer)
class RenderComboBox : public RenderBox {
public:
    RenderComboBox() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderComboBox() override {
        close_popup(true);
    }

    [[nodiscard]] int selected_index() const noexcept { return m_data.selected_index; }
    [[nodiscard]] const std::vector<ComboBoxItem>& items() const noexcept { return m_data.items; }
    [[nodiscard]] bool is_open() const noexcept { return m_state.is_open; }
    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    [[nodiscard]] const ComboBoxData& data() const noexcept { return m_data; }
    [[nodiscard]] const ComboBoxInteractiveState& state() const noexcept { return m_state; }

    [[nodiscard]] std::string selected_label() const {
        if (m_data.selected_index >= 0 && m_data.selected_index < static_cast<int>(m_data.items.size())) {
            return m_data.items[static_cast<size_t>(m_data.selected_index)].label;
        }
        return "";
    }

    void set_items(std::vector<ComboBoxItem> items) {
        m_data.items = std::move(items);
        if (m_data.selected_index >= static_cast<int>(m_data.items.size())) {
            m_data.selected_index = m_data.items.empty() ? -1 : 0;
        }
        mark_needs_paint();
    }

    void set_selected_index(int idx, bool notify = true) {
        int clamped = idx;
        if (clamped < -1 || clamped >= static_cast<int>(m_data.items.size())) {
            clamped = m_data.items.empty() ? -1 : std::clamp(clamped, -1, static_cast<int>(m_data.items.size()) - 1);
        }
        if (clamped >= 0 && m_data.items[static_cast<size_t>(clamped)].disabled) {
            return;
        }
        if (m_data.selected_index != clamped) {
            m_data.selected_index = clamped;
            if (notify && m_data.on_change) {
                m_data.on_change(m_data.selected_index, selected_label());
            }
            mark_needs_paint();
        }
    }

    void set_placeholder(std::string text) {
        if (m_data.placeholder != text) {
            m_data.placeholder = std::move(text);
            mark_needs_paint();
        }
    }

    void set_dimensions(float w, float h, float item_h, float font_sz, int max_vis, float max_dd_h = 280.0f) {
        bool layout_changed = false;
        if (w > 0.0f && std::abs(m_data.width - w) > 0.001f) {
            m_data.width = w;
            layout_changed = true;
        }
        if (h > 0.0f && std::abs(m_data.height - h) > 0.001f) {
            m_data.height = h;
            layout_changed = true;
        }
        if (item_h > 0.0f) m_data.item_height = item_h;
        if (font_sz > 0.0f) m_data.font_size = font_sz;
        if (max_vis > 0) m_data.max_visible_items = max_vis;
        if (max_dd_h > 0.0f) m_data.max_dropdown_height = max_dd_h;
        if (layout_changed) {
            mark_needs_layout();
        }
    }

    void set_disabled(bool disabled) {
        if (m_state.is_disabled != disabled) {
            m_state.is_disabled = disabled;
            m_data.disabled = disabled;
            is_focusable = !disabled;
            cursor_shape = disabled ? CursorShape::Forbidden : CursorShape::PointingHand;
            if (disabled) {
                close_popup(true);
            }
            mark_needs_paint();
        }
    }

    void set_on_change(Fn<void(int, const std::string&)> cb) {
        m_data.on_change = std::move(cb);
    }

    void set_pointer_reveal_enabled(std::optional<bool> enabled) noexcept {
        if (m_enable_pointer_reveal != enabled) {
            m_enable_pointer_reveal = enabled;
            if (!ThemeContext::current().resolve_pointer_reveal(m_enable_pointer_reveal)) m_reveal_active = false;
            mark_needs_paint();
        }
    }

    void open_popup() {
        if (m_state.is_disabled || m_state.is_open || m_data.items.empty()) return;
        m_state.is_open = true;
        m_last_anim_time = std::chrono::steady_clock::now();

        Window* win = Window::active_window();
        if (win) {
            int visible_count = std::min(static_cast<int>(m_data.items.size()), std::max(1, m_data.max_visible_items));
            float ideal_h = static_cast<float>(visible_count) * m_data.item_height + 8.0f;
            float max_dd_h = m_data.max_dropdown_height > 0.0f ? m_data.max_dropdown_height : 280.0f;
            float popup_h = std::min(ideal_h, max_dd_h);
            float popup_w = std::max(size().x, 120.0f);

            Vec2 anchor = global_offset();
            anchor.y += win->titlebar_height();
            Vec2 win_sz = win->size();

            float space_below = std::max(0.0f, win_sz.y - (anchor.y + size().y + 4.0f) - 8.0f);
            float space_above = std::max(0.0f, anchor.y - 4.0f - win->titlebar_height() - 8.0f);
            bool open_upward = (space_below < popup_h && space_above > space_below);
            float avail_space = open_upward ? space_above : space_below;
            // 严格限制弹出面板高度绝不超过窗口可用空间，超出自动启用 WinUI 3 滚动条
            if (avail_space >= m_data.item_height + 8.0f) {
                popup_h = std::min(popup_h, avail_space);
            }

            float popup_y = open_upward
                ? std::max(win->titlebar_height() + 4.0f, anchor.y - popup_h - 4.0f)
                : (anchor.y + size().y + 4.0f);
            float popup_x = std::clamp(anchor.x, 4.0f, std::max(4.0f, win_sz.x - popup_w - 4.0f));

            auto popup = make_render_node<RenderComboBoxPopup>(
                m_data.items,
                m_data.selected_index,
                m_data.item_height,
                m_data.font_size,
                m_data.max_visible_items,
                open_upward,
                [this](int chosen_idx) {
                    set_selected_index(chosen_idx, true);
                    close_popup(false);
                });

            m_popup_weak = popup;

            auto entry_id_box = std::make_shared<uint64_t>(0);
            OverlayEntry entry;
            entry.content = popup;
            entry.global_pos = Vec2{popup_x, popup_y};
            entry.size = Vec2{popup_w, popup_h};
            entry.dismiss_on_outside_click = true;
            entry.is_modal = false;
            entry.on_request_dismiss = [this, entry_id_box]() -> bool {
                if (m_overlay_id == *entry_id_box && m_overlay_id != 0) {
                    m_last_dismiss_time = std::chrono::steady_clock::now();
                    close_popup(false);
                    return true;
                }
                return false;
            };
            entry.on_dismiss = [this, entry_id_box]() {
                if (m_overlay_id == *entry_id_box) {
                    m_overlay_id = 0;
                    m_popup_weak.reset();
                    m_state.is_open = false;
                    m_last_dismiss_time = std::chrono::steady_clock::now();
                    m_last_anim_time = m_last_dismiss_time;
                    mark_needs_paint();
                }
            };

            m_overlay_id = win->show_overlay(std::move(entry));
            *entry_id_box = m_overlay_id;
        }

        mark_needs_paint();
    }

    void close_popup(bool immediate = false) {
        if (m_overlay_id != 0) {
            uint64_t id = m_overlay_id;
            m_overlay_id = 0;
            auto popup = m_popup_weak.lock();
            m_popup_weak.reset();
            if (!immediate && popup && !popup->is_closing()) {
                popup->start_close([id]() {
                    if (Window* win = Window::active_window()) {
                        win->hide_overlay(id);
                    }
                });
            } else if (Window* win = Window::active_window()) {
                win->hide_overlay(id);
            }
        }
        if (m_state.is_open) {
            m_state.is_open = false;
            m_last_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        }
    }

    void toggle_popup() {
        if (m_state.is_open) {
            close_popup();
        } else {
            open_popup();
        }
    }

    void perform_layout() override {
        float w = m_constraints.constrain_width(m_data.width > 0.0f ? m_data.width : 180.0f);
        float h = m_constraints.constrain_height(m_data.height > 0.0f ? m_data.height : 32.0f);
        set_size(Vec2{w, h});
    }

    void paint(PaintContext& ctx) override {
        update_animations();

        const Theme& theme = ThemeContext::current();
        ResolvedComboBoxStyle style = ComboBoxStyleResolver::resolve(m_data, m_state, theme);

        Rect box_rect = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);

        // 1. 触发框背景 (WinUI 3 ControlFillColor)
        ctx.draw_rect(box_rect, style.background, style.corner_radius, 0.0f);

        // 2. 1px 外边框 + 底部立体微描边 (ControlElevationBorder)
        if (style.border.a > 0.001f) {
            ctx.draw_rect(box_rect, style.border, style.corner_radius, style.border_width);
        }
        if (!m_state.is_disabled && style.bottom_border.a > 0.001f) {
            float line_x = style.corner_radius;
            float line_w = std::max(0.0f, size().x - style.corner_radius * 2.0f);
            ctx.draw_rect(Rect::from_xywh(line_x, size().y - 1.0f, line_w, 1.0f), style.bottom_border, 0.0f, 0.0f);
        }

        // 2.5 Fluent Pointer Reveal (控件级覆写最高优先级，未设置时继承全局 Theme::enable_pointer_reveal)
        if (theme.resolve_pointer_reveal(m_enable_pointer_reveal) && m_reveal_active && !m_state.is_disabled) {
            const Color surf = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.10f} : Color{0.0f, 0.0f, 0.0f, 0.055f};
            const Color edge = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.32f} : Color{0.0f, 0.0f, 0.0f, 0.22f};
            ctx.draw_pointer_reveal(box_rect, m_reveal_pos, 68.0f, surf, edge, style.corner_radius, style.border_width);
        }

        // 3. 当前选中文本或占位文本 (左侧对齐，右侧预留 28px 给下拉箭头)
        const bool has_selection =
            (m_data.selected_index >= 0 && m_data.selected_index < static_cast<int>(m_data.items.size()));
        const std::string& display_text = has_selection
            ? m_data.items[static_cast<size_t>(m_data.selected_index)].label
            : m_data.placeholder;
        Color text_col = has_selection ? style.text : style.placeholder_text;

        const float text_clip_w = std::max(0.0f, size().x - style.padding_x - 28.0f);
        if (text_clip_w > 1.0f && !display_text.empty()) {
            ctx.push_clip(Rect::from_xywh(style.padding_x, 0.0f, text_clip_w, size().y), 0.0f);
            Vec2 text_sz = TextMeasure::measure(display_text, style.font_size);
            float ty = (size().y - text_sz.y) * 0.5f;
            ctx.draw_text(display_text, PointF{style.padding_x, ty}, style.font_size, text_col);
            ctx.pop_clip();
        }

        // 4. WinUI 3 右侧精致下拉箭头 (Chevron Down，按下/展开时轻微弹动)
        const float cx = size().x - 15.0f;
        const float cy = size().y * 0.5f + m_chevron_offset;
        const float half_w = 3.8f;
        const float half_h = 2.0f;
        // 展开时平滑翻转向上
        float dir = 1.0f - 2.0f * m_open_anim;
        PointF left_pt{cx - half_w, cy - half_h * dir};
        PointF mid_pt{cx, cy + half_h * dir};
        PointF right_pt{cx + half_w, cy - half_h * dir};
        ctx.draw_line(left_pt, mid_pt, style.chevron, 1.25f);
        ctx.draw_line(mid_pt, right_pt, style.chevron, 1.25f);
    }

private:
    void step_selection(int delta) {
        int count = static_cast<int>(m_data.items.size());
        if (count == 0) return;
        int cur = m_data.selected_index;
        for (int tries = 0; tries < count; ++tries) {
            cur = std::clamp(cur + delta, 0, count - 1);
            if (!m_data.items[static_cast<size_t>(cur)].disabled) {
                set_selected_index(cur, true);
                return;
            }
            if ((delta < 0 && cur == 0) || (delta > 0 && cur == count - 1)) break;
        }
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            m_state.is_hovered = true;
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (!m_state.is_disabled && e.type == PlatformEventType::PointerMove &&
                e.device_kind != PointerDeviceKind::Touch &&
                ThemeContext::current().resolve_pointer_reveal(m_enable_pointer_reveal)) {
                m_reveal_pos = e.local_position;
                m_reveal_active = true;
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_state.is_hovered = false;
            m_state.is_pressed = false;
            m_reveal_active = false;
            m_last_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        };

        on_pointer_down = [this](PointerEvent& e) {
            if (m_state.is_disabled) return;
            // 若刚刚在本次点击中被 OverlayManager 外部点击关闭，则不再立即重新打开
            auto now = std::chrono::steady_clock::now();
            if (m_last_dismiss_time.time_since_epoch().count() != 0 &&
                std::chrono::duration<float>(now - m_last_dismiss_time).count() < 0.06f) {
                e.stop_propagation();
                return;
            }
            m_state.is_pressed = true;
            m_last_anim_time = now;
            toggle_popup();
            e.stop_propagation();
            mark_needs_paint();
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (m_state.is_pressed) {
                m_state.is_pressed = false;
                m_last_anim_time = std::chrono::steady_clock::now();
                mark_needs_paint();
            }
            e.stop_propagation();
        };

        on_focus = [this](FocusEvent& e) {
            m_state.is_focused = e.gained_focus;
            if (!e.gained_focus && m_state.is_open) {
                close_popup();
            }
            mark_needs_paint();
        };

        on_key = [this](KeyEvent& e) {
            if (e.type != PlatformEventType::KeyDown || m_state.is_disabled) return;
            if (e.key_code == 38) {        // Up
                step_selection(-1);
                e.stop_propagation();
            } else if (e.key_code == 40) { // Down
                step_selection(+1);
                e.stop_propagation();
            } else if (e.key_code == 36) { // Home
                if (!m_data.items.empty()) set_selected_index(0, true);
                e.stop_propagation();
            } else if (e.key_code == 35) { // End
                if (!m_data.items.empty()) set_selected_index(static_cast<int>(m_data.items.size()) - 1, true);
                e.stop_propagation();
            } else if (e.key_code == 13 || e.key_code == 32) { // Enter / Space
                toggle_popup();
                e.stop_propagation();
            } else if (e.key_code == 27 && m_state.is_open) {  // Esc
                close_popup();
                e.stop_propagation();
            }
        };
    }

    void update_animations() {
        auto now = std::chrono::steady_clock::now();
        const float target_open = m_state.is_open ? 1.0f : 0.0f;
        const float target_chevron = m_state.is_pressed ? 1.5f : 0.0f;

        if (m_last_anim_time.time_since_epoch().count() == 0) {
            m_last_anim_time = now;
            if (std::abs(m_open_anim - target_open) > 0.005f ||
                std::abs(m_chevron_offset - target_chevron) > 0.02f) {
                mark_needs_paint();
            }
            return;
        }
        float dt = std::chrono::duration<float>(now - m_last_anim_time).count();
        m_last_anim_time = now;
        dt = std::clamp(dt, 0.0f, 0.05f);

        bool animating = false;
        if (std::abs(m_open_anim - target_open) > 0.005f) {
            m_open_anim += (target_open - m_open_anim) * std::min(1.0f, dt * 20.0f);
            animating = true;
        } else {
            m_open_anim = target_open;
        }

        if (std::abs(m_chevron_offset - target_chevron) > 0.02f) {
            m_chevron_offset += (target_chevron - m_chevron_offset) * std::min(1.0f, dt * 24.0f);
            animating = true;
        } else {
            m_chevron_offset = target_chevron;
        }

        if (animating) {
            mark_needs_paint();
        }
    }

    ComboBoxData m_data;
    ComboBoxInteractiveState m_state;
    uint64_t m_overlay_id{0};
    std::weak_ptr<RenderComboBoxPopup> m_popup_weak;
    float m_open_anim{0.0f};
    float m_chevron_offset{0.0f};
    std::optional<bool> m_enable_pointer_reveal{std::nullopt};
    bool m_reveal_active{false};
    PointF m_reveal_pos{0.0f, 0.0f};
    std::chrono::steady_clock::time_point m_last_anim_time;
    std::chrono::steady_clock::time_point m_last_dismiss_time;
};

// C++20 指定初始化器声明式下拉选择框控件 (ComboBox DSL Wrapper)
struct ComboBox : BindableMixin<ComboBox, RenderComboBox> {
    std::vector<ComboBoxItem> items;
    int selected_index{-1};
    std::string placeholder{FrameworkStrings::current().select_placeholder};
    float width{180.0f};
    float height{32.0f};
    float item_height{32.0f};
    float font_size{13.0f};
    int max_visible_items{8};
    float max_dropdown_height{280.0f};
    bool disabled{false};
    std::optional<bool> enable_pointer_reveal{std::nullopt};

    Fn<void(int, const std::string&)> on_change{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderComboBox>& node, Property<int>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_selected_index(prop.get(), false);
    std::weak_ptr<RenderComboBox> weak = node;
    bg.subscribe(prop, [weak, guard](int idx) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_selected_index(idx, false);
        }
    });
    auto prev = node->data().on_change;
    Property<int>* p = &prop;
    node->set_on_change([prev = std::move(prev), p, guard](int idx, const std::string& label) {
        *guard = true;
        p->set(idx);
        if (prev) prev(idx, label);
        *guard = false;
    });
}

class ComboBoxWidget : public Widget {
public:
    explicit ComboBoxWidget(const ComboBox& desc) : m_desc(desc) {}
    explicit ComboBoxWidget(ComboBox&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ComboBoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderComboBox>();
        apply_desc_to_node(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto cb = std::static_pointer_cast<RenderComboBox>(node)) {
            apply_desc_to_node(cb);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderComboBox>& node) {
        node->set_items(m_desc.items);
        node->set_selected_index(m_desc.selected_index, false);
        node->set_placeholder(m_desc.placeholder);
        node->set_dimensions(m_desc.width, m_desc.height,
                             m_desc.item_height, m_desc.font_size,
                             m_desc.max_visible_items, m_desc.max_dropdown_height);
        node->set_disabled(m_desc.disabled);
        node->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
        node->set_on_change(m_desc.on_change);
    }

    ComboBox m_desc;
};

inline ComboBox::operator WidgetPtr() const& {
    return make_widget<ComboBoxWidget>(*this);
}
inline ComboBox::operator WidgetPtr() && {
    return make_widget<ComboBoxWidget>(std::move(*this));
}

} // namespace ktui
