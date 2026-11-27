#pragma once

#include "menu_flyout_data.hpp"
#include "menu_flyout_style.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/common/fluent_chrome.hpp>
#include <ktui/widgets/common/fluent_icons.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

// WinUI 3 MenuFlyout 弹出菜单渲染盒 (支持多级级联子菜单、预测锥与任意真实控件嵌套)
class RenderMenuFlyout : public RenderBox {
public:
    RenderMenuFlyout(std::vector<MenuFlyoutItem> items,
                     Fn<void()> on_close,
                     Fn<void()> on_close_all = nullptr,
                     Fn<void()> on_pointer_activity = nullptr)
        : m_items(std::move(items)),
          m_on_close(std::move(on_close)),
          m_on_close_all(std::move(on_close_all)),
          m_on_pointer_activity(std::move(on_pointer_activity)) {
        cursor_shape = CursorShape::Arrow;
        m_custom_boxes.resize(m_items.size());
        m_trailing_boxes.resize(m_items.size());
        for (size_t i = 0; i < m_items.size(); ++i) {
            const auto& it = m_items[i];
            if (!it.svg_path.empty() || it.is_checkable || it.is_radio) {
                m_has_leading_col = true;
            }
            if (it.custom_widget) {
                auto box = it.custom_widget->create_render_node();
                if (box) {
                    append_child(box.get());
                    m_custom_boxes[i] = std::move(box);
                }
            }
            if (it.trailing_widget) {
                auto tbox = it.trailing_widget->create_render_node();
                if (tbox) {
                    append_child(tbox.get());
                    m_trailing_boxes[i] = std::move(tbox);
                }
            }
        }
        setup_behavior();
    }

    ~RenderMenuFlyout() override {
        *m_alive = false;
        close_submenu();
        for (auto& b : m_custom_boxes) {
            if (b && b->parent() == this) remove_child(b.get());
        }
        for (auto& b : m_trailing_boxes) {
            if (b && b->parent() == this) remove_child(b.get());
        }
    }

    [[nodiscard]] Vec2 compute_ideal_size() {
        float max_w = 172.0f;
        float total_h = 8.0f; // 上下各 4px 内边距
        const float leading = m_has_leading_col ? 26.0f : 0.0f;

        for (size_t i = 0; i < m_items.size(); ++i) {
            const auto& it = m_items[i];
            if (it.is_separator) {
                total_h += kSepH;
                continue;
            }
            if (m_custom_boxes[i]) {
                m_custom_boxes[i]->layout(BoxConstraints::loose(Vec2{320.0f, 260.0f}), true);
                float cw = m_custom_boxes[i]->size().x + 20.0f;
                float ch = (it.custom_height > 0.0f) ? it.custom_height : (m_custom_boxes[i]->size().y + 8.0f);
                max_w = std::max(max_w, cw);
                total_h += std::max(28.0f, ch);
                continue;
            }
            float row_h = kItemH;
            float lw = TextMeasure::measure_width(it.label, 13.0f);
            float trail_w = 0.0f;
            if (m_trailing_boxes[i]) {
                m_trailing_boxes[i]->layout(BoxConstraints::loose(Vec2{200.0f, 60.0f}), true);
                trail_w = m_trailing_boxes[i]->size().x + 20.0f;
                row_h = std::max(kItemH, m_trailing_boxes[i]->size().y + 8.0f);
            } else if (!it.sub_items.empty()) {
                trail_w = 24.0f;
            } else if (!it.shortcut.empty()) {
                trail_w = TextMeasure::measure_width(it.shortcut, 11.5f) + 28.0f;
            }
            max_w = std::max(max_w, 24.0f + leading + lw + trail_w + 16.0f);
            total_h += row_h;
        }
        return Vec2{std::ceil(std::min(max_w, 360.0f)), std::ceil(total_h)};
    }

    void perform_layout() override {
        Vec2 sz = compute_ideal_size();
        set_size(sz);

        m_row_rects.resize(m_items.size());
        float y = 4.0f;
        for (size_t i = 0; i < m_items.size(); ++i) {
            const auto& it = m_items[i];
            if (it.is_separator) {
                m_row_rects[i] = Rect::from_xywh(4.0f, y, std::max(0.0f, sz.x - 8.0f), kSepH);
                y += kSepH;
                continue;
            }
            if (m_custom_boxes[i]) {
                float avail_w = std::max(40.0f, sz.x - 20.0f);
                m_custom_boxes[i]->layout(BoxConstraints{avail_w, avail_w, 0.0f, 260.0f}, true);
                float ch = (it.custom_height > 0.0f) ? it.custom_height : (m_custom_boxes[i]->size().y + 8.0f);
                ch = std::max(28.0f, ch);
                float wy = y + std::max(0.0f, (ch - m_custom_boxes[i]->size().y) * 0.5f);
                m_custom_boxes[i]->set_offset(Vec2{10.0f, std::round(wy)});
                m_row_rects[i] = Rect::from_xywh(4.0f, y, std::max(0.0f, sz.x - 8.0f), ch);
                y += ch;
                continue;
            }
            float row_h = kItemH;
            if (m_trailing_boxes[i]) {
                m_trailing_boxes[i]->layout(BoxConstraints::loose(Vec2{200.0f, 60.0f}), true);
                row_h = std::max(kItemH, m_trailing_boxes[i]->size().y + 8.0f);
                float tx = sz.x - 12.0f - m_trailing_boxes[i]->size().x;
                float ty = y + (row_h - m_trailing_boxes[i]->size().y) * 0.5f;
                m_trailing_boxes[i]->set_offset(Vec2{std::round(tx), std::round(ty)});
            }
            m_row_rects[i] = Rect::from_xywh(4.0f, y, std::max(0.0f, sz.x - 8.0f), row_h);
            y += row_h;
        }
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (!bounds().contains(local_position)) return false;
        // 优先命中内嵌整行控件与右侧伴随控件，确保所有真实 KTUI 控件交互与动效 100% 原生工作
        for (size_t i = 0; i < m_custom_boxes.size(); ++i) {
            if (m_custom_boxes[i]) {
                PointF cp = local_position - m_custom_boxes[i]->offset();
                if (m_custom_boxes[i]->hit_test(result, cp)) {
                    result.add(this, local_position);
                    return true;
                }
            }
            if (m_trailing_boxes[i]) {
                PointF cp = local_position - m_trailing_boxes[i]->offset();
                if (m_trailing_boxes[i]->hit_test(result, cp)) {
                    result.add(this, local_position);
                    return true;
                }
            }
        }
        result.add(this, local_position);
        return true;
    }

    bool hit_test_touch(HitTestResult& result, PointF local_position) override {
        if (!bounds().contains(local_position)) return false;
        for (size_t i = 0; i < m_custom_boxes.size(); ++i) {
            if (m_custom_boxes[i]) {
                PointF cp = local_position - m_custom_boxes[i]->offset();
                if (m_custom_boxes[i]->hit_test_touch(result, cp)) {
                    result.add(this, local_position);
                    return true;
                }
            }
            if (m_trailing_boxes[i]) {
                PointF cp = local_position - m_trailing_boxes[i]->offset();
                if (m_trailing_boxes[i]->hit_test_touch(result, cp)) {
                    result.add(this, local_position);
                    return true;
                }
            }
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        auto st = m_surface.tick();
        if (st.animating) mark_needs_paint();

        // 预测锥停留超时检测：鼠标停在三角形内满 300ms 后自动切换至待切换菜单项
        if (m_aim_active && m_pending_hover_idx >= 0 && m_open_sub_index >= 0) {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_aim_last_move).count();
            if (elapsed_ms >= kAimDwellMs) {
                int target = m_pending_hover_idx;
                clear_aim_cone();
                apply_hover_target(target);
            } else {
                mark_needs_paint();
            }
        }

        const Theme& theme = ThemeContext::current();
        const auto resolved_style = MenuFlyoutStyleResolver::resolve(theme);
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
        Color accent = resolved_style.accent;
        accent.a *= alpha;

        // 1. 复用 FluentPopupSurface 绘制 32dp 真高斯阴影 + 8px 圆角浮层底板
        FluentPopupSurface::paint_panel(ctx, theme, Rect::from_xywh(0.0f, 0.0f, size().x, anim_h), alpha);

        // 2. 菜单项与内嵌控件绘制
        ctx.push_clip(Rect::from_xywh(4.0f, 4.0f, std::max(0.0f, size().x - 8.0f), std::max(0.0f, anim_h - 8.0f)), 4.0f);
        const float leading = m_has_leading_col ? 26.0f : 0.0f;

        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            const size_t ui = static_cast<size_t>(i);
            const auto& it = m_items[ui];
            const Rect row_r = (ui < m_row_rects.size()) ? m_row_rects[ui] : Rect{};

            if (it.is_separator) {
                ctx.draw_rect(Rect::from_xywh(4.0f, std::round(row_r.min_y + kSepH * 0.5f),
                                              std::max(0.0f, size().x - 8.0f), 1.0f),
                              sep_col, 0.0f, 0.0f);
                continue;
            }

            if (m_custom_boxes[ui]) {
                PaintContext::OffsetScope scope(ctx, m_custom_boxes[ui]->offset());
                m_custom_boxes[ui]->paint(ctx);
                continue;
            }

            const float y = row_r.min_y;
            const float rh = (row_r.height() > 0.0f) ? row_r.height() : kItemH;
            Rect r = Rect::from_xywh(4.0f, y + 1.0f, std::max(0.0f, size().x - 8.0f), std::max(0.0f, rh - 2.0f));
            if (!it.disabled) {
                if (m_pressed_index == i) ctx.draw_rect(r, prs_bg, 4.0f, 0.0f);
                else if (m_hovered_index == i || m_open_sub_index == i) ctx.draw_rect(r, hov_bg, 4.0f, 0.0f);
            }

            Color tc = it.disabled ? dis_col : text_col;
            if (m_has_leading_col) {
                Rect icon_r = Rect::from_xywh(14.0f, std::round(y + (rh - 15.0f) * 0.5f), 15.0f, 15.0f);
                if (it.is_radio) {
                    if (it.checked) {
                        ctx.draw_rect(Rect::from_xywh(18.5f, std::round(y + (rh - 6.0f) * 0.5f), 6.0f, 6.0f),
                                      accent, 3.0f, 0.0f);
                    }
                } else if (it.is_checkable && it.checked) {
                    static constexpr const char* kCheckSvg =
                        "M4.53 12.97a.75.75 0 0 0-1.06 1.06l4.5 4.5a.75.75 0 0 0 1.06 0l11-11a.75.75 0 0 0-1.06-1.06L8.5 16.94l-3.97-3.97Z";
                    SvgEngine::draw_svg_path(ctx, kCheckSvg, 24.0f, 24.0f, icon_r, accent);
                } else if (!it.svg_path.empty()) {
                    SvgEngine::draw_svg_path(ctx, it.svg_path, 24.0f, 24.0f, icon_r, tc);
                }
            }

            Vec2 lsz = TextMeasure::measure(it.label, 13.0f);
            ctx.draw_text(it.label,
                          PointF{std::round(12.0f + leading), std::round(y + (rh - lsz.y) * 0.5f)},
                          13.0f, tc);

            // 右侧尾部：级联子菜单箭头 / 任意内嵌真实 Widget (trailing_widget) / 快捷键提示
            if (!it.sub_items.empty()) {
                Rect cr = Rect::from_xywh(size().x - 24.0f, y, 16.0f, rh);
                fluent_icons::draw_centered(ctx, fluent_icons::kChevronRight, cr, 10.0f, it.disabled ? dis_col : muted_col);
            } else if (ui < m_trailing_boxes.size() && m_trailing_boxes[ui]) {
                PaintContext::OffsetScope scope(ctx, m_trailing_boxes[ui]->offset());
                m_trailing_boxes[ui]->paint(ctx);
            } else if (!it.shortcut.empty()) {
                Vec2 ssz = TextMeasure::measure(it.shortcut, 11.5f);
                ctx.draw_text(it.shortcut,
                              PointF{std::round(size().x - 14.0f - ssz.x), std::round(y + (rh - ssz.y) * 0.5f)},
                              11.5f, it.disabled ? dis_col : muted_col);
            }
        }
        ctx.pop_clip();
    }

    [[nodiscard]] static bool point_in_triangle(PointF p, PointF a, PointF b, PointF c) noexcept {
        auto cross = [](PointF o, PointF u, PointF v) noexcept -> float {
            return (u.x - o.x) * (v.y - o.y) - (u.y - o.y) * (v.x - o.x);
        };
        if (std::abs(cross(a, b, c)) < 1.0f) return false;
        float d1 = cross(p, a, b);
        float d2 = cross(p, b, c);
        float d3 = cross(p, c, a);
        bool has_neg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
        bool has_pos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
        return !(has_neg && has_pos);
    }

private:
    static constexpr float kItemH = 32.0f;
    static constexpr float kSepH = 9.0f;
    static constexpr int64_t kAimDwellMs = 300;

    [[nodiscard]] bool is_in_submenu_aim_cone(PointF cur_pos) const noexcept {
        if (m_open_sub_index < 0 || m_sub_client_rect.width() <= 0.0f || m_sub_client_rect.height() <= 0.0f) {
            return false;
        }
        // 取子菜单近侧边缘（位于右侧时取左边缘，位于左侧时取右边缘）的上下两个角
        float my_center_x = global_offset().x + size().x * 0.5f;
        float sub_center_x = m_sub_client_rect.min_x + m_sub_client_rect.width() * 0.5f;
        float near_x = (sub_center_x >= my_center_x) ? m_sub_client_rect.min_x : m_sub_client_rect.max_x;
        PointF b{near_x, m_sub_client_rect.min_y};
        PointF c{near_x, m_sub_client_rect.max_y};
        return point_in_triangle(cur_pos, m_aim_apex, b, c);
    }

    void clear_aim_cone() noexcept {
        m_aim_active = false;
        m_pending_hover_idx = -1;
    }

    void apply_hover_target(int idx) {
        m_hovered_index = idx;
        if (idx >= 0 && !m_items[static_cast<size_t>(idx)].sub_items.empty()) {
            open_submenu_for(idx);
        } else if (idx >= 0) {
            close_submenu();
        }
        mark_needs_paint();
    }

    int hit_index(float ly) const noexcept {
        for (int i = 0; i < static_cast<int>(m_row_rects.size()); ++i) {
            const size_t ui = static_cast<size_t>(i);
            const Rect& rr = m_row_rects[ui];
            if (ly >= rr.min_y && ly < rr.max_y) {
                if (m_items[ui].is_separator || m_custom_boxes[ui]) return -1;
                return i;
            }
        }
        return -1;
    }

    void close_submenu() {
        clear_aim_cone();
        m_sub_client_rect = Rect{};
        if (m_sub_overlay_id != 0) {
            uint64_t sid = m_sub_overlay_id;
            m_sub_overlay_id = 0;
            m_open_sub_index = -1;
            if (Window* win = Window::active_window()) {
                win->hide_overlay(sid);
            }
        }
    }

    void open_submenu_for(int idx) {
        if (idx == m_open_sub_index) return;
        close_submenu();
        if (idx < 0 || idx >= static_cast<int>(m_items.size())) return;
        const auto& it = m_items[static_cast<size_t>(idx)];
        if (it.disabled || it.sub_items.empty()) return;

        Window* win = Window::active_window();
        if (!win) return;

        auto sub_id = std::make_shared<uint64_t>(0);
        auto close_all = m_on_close_all ? m_on_close_all : m_on_close;
        auto alive = m_alive;
        auto notify_parent = [this, alive]() {
            if (!*alive) return;
            clear_aim_cone();
            if (m_on_pointer_activity) m_on_pointer_activity();
        };
        auto sub = make_render_node<RenderMenuFlyout>(
            it.sub_items,
            [sub_id]() {
                if (Window* w = Window::active_window(); w && *sub_id != 0) {
                    uint64_t id = *sub_id;
                    *sub_id = 0;
                    w->hide_overlay(id);
                }
            },
            close_all,
            std::move(notify_parent));

        Vec2 sub_sz = sub->compute_ideal_size();
        Vec2 win_sz = win->size();
        Vec2 my_pos = global_offset();
        float row_y = (static_cast<size_t>(idx) < m_row_rects.size()) ? m_row_rects[static_cast<size_t>(idx)].min_y : 4.0f;

        float gx = my_pos.x + size().x - 4.0f;
        if (gx + sub_sz.x > win_sz.x - 6.0f) {
            gx = std::max(6.0f, my_pos.x - sub_sz.x + 4.0f);
        }
        float gy = my_pos.y + win->titlebar_height() + row_y - 4.0f;
        if (gy + sub_sz.y > win_sz.y - 6.0f) {
            gy = std::max(win->titlebar_height() + 6.0f, win_sz.y - sub_sz.y - 6.0f);
        }

        OverlayEntry entry;
        entry.content = sub;
        entry.global_pos = Vec2{std::round(gx), std::round(gy)};
        entry.size = sub_sz;
        entry.dismiss_on_outside_click = false;
        entry.is_modal = false;

        m_sub_client_rect = Rect::from_xywh(std::round(gx), std::round(gy) - win->titlebar_height(), sub_sz.x, sub_sz.y);
        m_sub_overlay_id = win->show_overlay(std::move(entry));
        *sub_id = m_sub_overlay_id;
        m_open_sub_index = idx;
        mark_needs_paint();
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (m_on_pointer_activity) m_on_pointer_activity();
        };
        on_pointer_move = [this](PointerEvent& e) {
            if (m_on_pointer_activity) m_on_pointer_activity();
            int idx = hit_index(e.local_position.y);
            const PointF cur_pos = e.position;
            const bool is_mouse = (e.device_kind != PointerDeviceKind::Touch);

            // 仅对鼠标/笔悬停启用子菜单预测锥，保持触控与键盘行为完全不变
            if (is_mouse && m_open_sub_index >= 0 && m_sub_overlay_id != 0 && idx >= 0 && idx != m_open_sub_index) {
                if (!m_aim_active && m_has_prev_mouse) {
                    m_aim_apex = m_prev_mouse_pos;
                    if (is_in_submenu_aim_cone(cur_pos)) {
                        m_aim_active = true;
                        m_pending_hover_idx = idx;
                        m_aim_last_move = std::chrono::steady_clock::now();
                        m_prev_mouse_pos = cur_pos;
                        m_has_prev_mouse = true;
                        mark_needs_paint();
                        e.stop_propagation();
                        return;
                    }
                } else if (m_aim_active) {
                    if (is_in_submenu_aim_cone(cur_pos)) {
                        // 鼠标仍在预测三角形内：保持子菜单打开，刷新停留计时（停满 300ms 再切换）
                        m_pending_hover_idx = idx;
                        m_aim_last_move = std::chrono::steady_clock::now();
                        m_prev_mouse_pos = cur_pos;
                        m_has_prev_mouse = true;
                        mark_needs_paint();
                        e.stop_propagation();
                        return;
                    }
                    // 鼠标移出预测三角形：立即切换
                    clear_aim_cone();
                }
            } else {
                clear_aim_cone();
            }

            m_prev_mouse_pos = cur_pos;
            m_has_prev_mouse = true;

            if (is_mouse && idx != m_hovered_index) {
                apply_hover_target(idx);
            }
            e.stop_propagation();
        };
        on_pointer_exit = [this](PointerEvent&) {
            clear_aim_cone();
            m_has_prev_mouse = false;
            if (m_hovered_index != -1 || m_pressed_index != -1) {
                m_hovered_index = -1;
                m_pressed_index = -1;
                mark_needs_paint();
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            clear_aim_cone();
            m_has_prev_mouse = false;
            if (m_hovered_index != -1 || m_pressed_index != -1) {
                m_hovered_index = -1;
                m_pressed_index = -1;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (m_on_pointer_activity) m_on_pointer_activity();
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
                auto& item = m_items[static_cast<size_t>(idx)];
                if (!item.sub_items.empty()) {
                    open_submenu_for(idx);
                    e.stop_propagation();
                    return;
                }
                if (item.keep_open_on_click) {
                    if (item.is_checkable) item.checked = !item.checked;
                    auto tcb = item.on_toggle;
                    auto ccb = item.on_click;
                    mark_needs_paint();
                    e.stop_propagation();
                    if (tcb) tcb(item.checked);
                    if (ccb) ccb();
                    return;
                }
                auto cb = item.on_click;
                auto tcb = item.on_toggle;
                bool next_chk = item.is_checkable ? !item.checked : item.checked;
                auto close_cb = m_on_close_all ? m_on_close_all : m_on_close;
                e.stop_propagation();
                if (close_cb) close_cb(); // 关闭整条菜单链后严禁再访问 this
                if (tcb) tcb(next_chk);
                if (cb) cb();
                return;
            }
            mark_needs_paint();
            e.stop_propagation();
        };
    }

    std::vector<MenuFlyoutItem> m_items;
    std::vector<std::shared_ptr<RenderBox>> m_custom_boxes;
    std::vector<std::shared_ptr<RenderBox>> m_trailing_boxes;
    std::vector<Rect> m_row_rects;
    Fn<void()> m_on_close;
    Fn<void()> m_on_close_all;
    Fn<void()> m_on_pointer_activity;
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
    bool m_has_leading_col{false};
    int m_hovered_index{-1};
    int m_pressed_index{-1};
    int m_open_sub_index{-1};
    uint64_t m_sub_overlay_id{0};
    Rect m_sub_client_rect{};
    PointF m_prev_mouse_pos{0.0f, 0.0f};
    bool m_has_prev_mouse{false};
    PointF m_aim_apex{0.0f, 0.0f};
    bool m_aim_active{false};
    int m_pending_hover_idx{-1};
    std::chrono::steady_clock::time_point m_aim_last_move{};
    FluentPopupSurface m_surface;
};

// 静态便捷菜单弹出器 (MenuFlyout::show_at)
struct MenuFlyout {
    static uint64_t show_at(std::vector<MenuFlyoutItem> items,
                            Vec2 window_pos,
                            Fn<void()> on_dismiss = nullptr,
                            Fn<void()> on_pointer_activity = nullptr,
                            Rect* out_client_rect = nullptr) {
        Window* win = Window::active_window();
        if (!win || items.empty()) return 0;

        auto overlay_id = std::make_shared<uint64_t>(0);
        auto close_fn = [overlay_id]() {
            if (Window* w = Window::active_window(); w && *overlay_id != 0) {
                uint64_t id = *overlay_id;
                *overlay_id = 0;
                w->hide_overlay(id);
            }
        };
        auto menu = make_render_node<RenderMenuFlyout>(
            std::move(items), close_fn, close_fn, std::move(on_pointer_activity));

        Vec2 sz = menu->compute_ideal_size();
        Vec2 win_sz = win->size();
        float gx = (window_pos.x + sz.x > win_sz.x - 6.0f) ? std::max(6.0f, window_pos.x - sz.x) : std::max(6.0f, window_pos.x);
        float gy = (window_pos.y + sz.y > win_sz.y - 6.0f) ? std::max(win->titlebar_height() + 6.0f, window_pos.y - sz.y) : std::max(win->titlebar_height() + 6.0f, window_pos.y);

        if (out_client_rect) {
            *out_client_rect = Rect::from_xywh(std::round(gx), std::round(gy) - win->titlebar_height(), sz.x, sz.y);
        }

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

// 右键上下文菜单容器 (ContextMenuArea): 为任意子控件绑定右键弹出 MenuFlyout (支持静态 items 或动态 items_builder)
class RenderContextMenuArea : public RenderBox {
public:
    RenderContextMenuArea() noexcept {
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button == PointerButton::Secondary) {
                std::vector<MenuFlyoutItem> resolved = m_items_builder ? m_items_builder() : m_items;
                if (!resolved.empty()) {
                    Window* win = Window::active_window();
                    Vec2 win_pos = e.position;
                    if (win) win_pos.y += win->titlebar_height();
                    MenuFlyout::show_at(std::move(resolved), win_pos);
                    e.stop_propagation();
                }
            }
        };
    }

    ~RenderContextMenuArea() override = default;

    void set_items(std::vector<MenuFlyoutItem> items, Fn<std::vector<MenuFlyoutItem>()> builder = nullptr) {
        m_items = std::move(items);
        m_items_builder = std::move(builder);
    }

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
    Fn<std::vector<MenuFlyoutItem>()> m_items_builder;
    std::shared_ptr<RenderBox> m_child_box;
};

struct ContextMenuArea {
    std::vector<MenuFlyoutItem> items;
    WidgetPtr child{nullptr};
    Fn<std::vector<MenuFlyoutItem>()> items_builder{nullptr};

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
        node->set_items(m_desc.items, m_desc.items_builder);
        if (m_desc.child) node->set_child_box(m_desc.child->create_render_node());
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto area = std::static_pointer_cast<RenderContextMenuArea>(node)) {
            area->set_items(m_desc.items, m_desc.items_builder);
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
 // namespace ktui
