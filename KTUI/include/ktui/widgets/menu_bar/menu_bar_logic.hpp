#pragma once

#include "menu_bar_data.hpp"
#include "menu_bar_style.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

// WinUI 3 MenuBar 顶部菜单栏渲染盒
// 严格对齐 Windows 11 Fluent 2 规范：
// - 32px 无边框透明横条，无下拉箭头，无外框描边
// - 柔和 4px 圆角 SubtleFill 悬停/激活滑块，带平滑水平滑动过渡动画
// - 展开任意菜单后，鼠标悬停切换相邻顶层项自动无缝切换下拉菜单 (WinUI 3 / Win32 Hot-Track)
class RenderMenuBar : public RenderBox {
public:
    RenderMenuBar() noexcept {
        cursor_shape = CursorShape::Arrow;
        setup_behavior();
    }

    ~RenderMenuBar() override {
        *m_alive = false;
        close_active_menu();
    }

    void set_items(std::vector<MenuBarItem> items) {
        m_items = std::move(items);
        mark_needs_layout();
    }

    [[nodiscard]] int open_index() const noexcept { return m_open_index; }

    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 480.0f;
        m_item_rects.clear();
        float x = 2.0f;
        for (const auto& it : m_items) {
            float tw = std::ceil(TextMeasure::measure_width(it.title, 13.5f)) + 24.0f;
            m_item_rects.push_back(Rect::from_xywh(x, 2.0f, std::max(44.0f, tw), kBarH - 4.0f));
            x += std::max(44.0f, tw) + 2.0f;
        }
        float desired_w = std::max(x + 2.0f, w);
        set_size(m_constraints.constrain(Vec2{desired_w, kBarH}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();

        // 预测锥停留超时检测：鼠标停在三角形内满 300ms 后自动切换至待切换顶层项
        if (m_aim_active && m_pending_open_idx >= 0 && m_open_index >= 0) {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_aim_last_move).count();
            if (elapsed_ms >= kAimDwellMs) {
                int target = m_pending_open_idx;
                clear_aim_cone();
                m_hovered_index = target;
                open_menu_at(target);
            } else {
                mark_needs_paint();
            }
        }

        // 平滑滑动的悬停/激活底板胶囊
        int active_idx = (m_open_index >= 0) ? m_open_index : m_hovered_index;
        bool animating = false;
        if (active_idx >= 0 && active_idx < static_cast<int>(m_item_rects.size())) {
            const Rect& tr = m_item_rects[static_cast<size_t>(active_idx)];
            auto now = std::chrono::steady_clock::now();
            float dt = m_has_anim_time
                ? std::clamp(std::chrono::duration<float>(now - m_last_anim_time).count(), 0.001f, 0.05f)
                : (1.0f / 60.0f);
            m_last_anim_time = now;
            m_has_anim_time = true;

            if (!m_pill_init) {
                m_pill_x = tr.min_x;
                m_pill_w = tr.width();
                m_pill_alpha = 1.0f;
                m_pill_init = true;
            } else {
                float k = 1.0f - std::exp(-dt * 26.0f);
                if (std::abs(m_pill_x - tr.min_x) > 0.15f || std::abs(m_pill_w - tr.width()) > 0.15f) {
                    m_pill_x += (tr.min_x - m_pill_x) * k;
                    m_pill_w += (tr.width() - m_pill_w) * k;
                    animating = true;
                } else {
                    m_pill_x = tr.min_x;
                    m_pill_w = tr.width();
                }
                if (m_pill_alpha < 0.99f) {
                    m_pill_alpha += (1.0f - m_pill_alpha) * k;
                    animating = true;
                } else {
                    m_pill_alpha = 1.0f;
                }
            }
        } else if (m_pill_alpha > 0.01f) {
            auto now = std::chrono::steady_clock::now();
            float dt = m_has_anim_time
                ? std::clamp(std::chrono::duration<float>(now - m_last_anim_time).count(), 0.001f, 0.05f)
                : (1.0f / 60.0f);
            m_last_anim_time = now;
            m_has_anim_time = true;
            m_pill_alpha *= std::exp(-dt * 22.0f);
            if (m_pill_alpha < 0.01f) {
                m_pill_alpha = 0.0f;
                m_pill_init = false;
            } else {
                animating = true;
            }
        } else {
            m_has_anim_time = false;
        }

        if (animating) mark_needs_paint();

        if (m_pill_alpha > 0.01f && m_pill_w > 1.0f) {
            Color pill_bg = (m_open_index >= 0) ? theme.colors.subtle_fill_tertiary : theme.colors.subtle_fill_secondary;
            pill_bg.a *= m_pill_alpha;
            ctx.draw_rect(Rect::from_xywh(m_pill_x, 2.0f, m_pill_w, kBarH - 4.0f), pill_bg, 4.0f, 0.0f);
        }

        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.94f} : Color{0.0f, 0.0f, 0.0f, 0.90f};
        Color dis_c  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.36f} : Color{0.0f, 0.0f, 0.0f, 0.36f};

        for (size_t i = 0; i < m_items.size() && i < m_item_rects.size(); ++i) {
            const Rect& r = m_item_rects[i];
            Vec2 ts = TextMeasure::measure(m_items[i].title, 13.5f);
            ctx.draw_text(m_items[i].title,
                          PointF{std::round(r.min_x + (r.width() - ts.x) * 0.5f),
                                 std::round(r.min_y + (r.height() - ts.y) * 0.5f)},
                          13.5f, m_items[i].disabled ? dis_c : text_c);
        }
    }

private:
    static constexpr float kBarH = 32.0f;
    static constexpr int64_t kAimDwellMs = 300;

    void clear_aim_cone() noexcept {
        m_aim_active = false;
        m_pending_open_idx = -1;
    }

    [[nodiscard]] bool is_in_dropdown_aim_cone(PointF cur_pos) const noexcept {
        if (m_open_index < 0 || m_popup_client_rect.width() <= 0.0f || m_popup_client_rect.height() <= 0.0f) {
            return false;
        }
        // 取下拉菜单近侧边缘（位于菜单栏下方取顶边，向上翻转时取底边）的左右两角
        float bar_cy = global_offset().y + kBarH * 0.5f;
        float pop_cy = m_popup_client_rect.min_y + m_popup_client_rect.height() * 0.5f;
        float near_y = (pop_cy >= bar_cy) ? m_popup_client_rect.min_y : m_popup_client_rect.max_y;
        PointF b{m_popup_client_rect.min_x, near_y};
        PointF c{m_popup_client_rect.max_x, near_y};
        return RenderMenuFlyout::point_in_triangle(cur_pos, m_aim_apex, b, c);
    }

    int hit_item(PointF local) const noexcept {
        for (size_t i = 0; i < m_item_rects.size(); ++i) {
            if (m_item_rects[i].contains(local)) return static_cast<int>(i);
        }
        return -1;
    }

    void close_active_menu() {
        clear_aim_cone();
        m_popup_client_rect = Rect{};
        if (m_overlay_id != 0) {
            uint64_t id = m_overlay_id;
            m_overlay_id = 0;
            m_open_index = -1;
            if (Window* win = Window::active_window()) {
                win->hide_overlay(id);
            }
            mark_needs_paint();
        }
    }

    void open_menu_at(int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_items.size())) return;
        const auto& item = m_items[static_cast<size_t>(idx)];
        if (item.disabled) return;

        std::vector<MenuFlyoutItem> resolved = item.items_builder ? item.items_builder() : item.items;
        if (resolved.empty()) return;

        Window* win = Window::active_window();
        if (!win) return;

        clear_aim_cone();

        // 切换菜单前先静默关闭旧菜单，避免触发旧菜单的 on_dismiss 清除新索引
        if (m_overlay_id != 0) {
            uint64_t old_id = m_overlay_id;
            m_overlay_id = 0;
            m_switching = true;
            win->hide_overlay(old_id);
            m_switching = false;
        }

        Vec2 bar_pos = global_offset();
        const Rect& r = m_item_rects[static_cast<size_t>(idx)];
        Vec2 anchor{bar_pos.x + r.min_x, bar_pos.y + win->titlebar_height() + kBarH + 2.0f};

        m_open_index = idx;
        m_last_anim_time = std::chrono::steady_clock::now();
        m_has_anim_time = true;

        auto alive = m_alive;
        m_overlay_id = MenuFlyout::show_at(
            std::move(resolved),
            anchor,
            [this, alive, idx]() {
                if (!*alive || m_switching) return;
                if (m_open_index == idx) {
                    clear_aim_cone();
                    m_popup_client_rect = Rect{};
                    m_overlay_id = 0;
                    m_open_index = -1;
                    m_last_dismiss = std::chrono::steady_clock::now();
                    m_last_dismiss_idx = idx;
                    mark_needs_paint();
                }
            },
            [this, alive]() {
                if (!*alive) return;
                clear_aim_cone();
            },
            &m_popup_client_rect);
        mark_needs_paint();
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int idx = hit_item(e.local_position);
            const PointF cur_pos = e.position;
            const bool is_mouse = (e.device_kind != PointerDeviceKind::Touch);

            // 展开态下对鼠标斜向移入弹出菜单启用预测锥，避免掠过左右相邻项时误切换
            if (is_mouse && m_open_index >= 0 && m_overlay_id != 0 && idx >= 0 && idx != m_open_index &&
                !m_items[static_cast<size_t>(idx)].disabled) {
                if (!m_aim_active && m_has_prev_mouse) {
                    m_aim_apex = m_prev_mouse_pos;
                    if (is_in_dropdown_aim_cone(cur_pos)) {
                        m_aim_active = true;
                        m_pending_open_idx = idx;
                        m_aim_last_move = std::chrono::steady_clock::now();
                        m_prev_mouse_pos = cur_pos;
                        m_has_prev_mouse = true;
                        mark_needs_paint();
                        return;
                    }
                } else if (m_aim_active) {
                    if (is_in_dropdown_aim_cone(cur_pos)) {
                        m_pending_open_idx = idx;
                        m_aim_last_move = std::chrono::steady_clock::now();
                        m_prev_mouse_pos = cur_pos;
                        m_has_prev_mouse = true;
                        mark_needs_paint();
                        return;
                    }
                    clear_aim_cone();
                }
            } else {
                clear_aim_cone();
            }

            m_prev_mouse_pos = cur_pos;
            m_has_prev_mouse = true;

            if (idx != m_hovered_index) {
                m_hovered_index = idx;
                cursor_shape = (idx >= 0 && !m_items[static_cast<size_t>(idx)].disabled)
                    ? CursorShape::PointingHand : CursorShape::Arrow;
                // WinUI 3 Hot-Track: 移出预测锥后，悬停其他顶层菜单项立即无缝切换
                if (m_open_index >= 0 && idx >= 0 && idx != m_open_index && !m_items[static_cast<size_t>(idx)].disabled) {
                    open_menu_at(idx);
                } else {
                    mark_needs_paint();
                }
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            clear_aim_cone();
            m_has_prev_mouse = false;
            if (m_hovered_index != -1) {
                m_hovered_index = -1;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int idx = hit_item(e.local_position);
            if (idx < 0) return;
            clear_aim_cone();

            auto now = std::chrono::steady_clock::now();
            if (m_last_dismiss_idx == idx && m_last_dismiss.time_since_epoch().count() != 0 &&
                std::chrono::duration<float>(now - m_last_dismiss).count() < 0.08f) {
                m_last_dismiss_idx = -1;
                e.stop_propagation();
                return;
            }
            if (m_open_index == idx) {
                close_active_menu();
            } else {
                open_menu_at(idx);
            }
            e.stop_propagation();
        };
    }

    std::vector<MenuBarItem> m_items;
    std::vector<Rect> m_item_rects;
    int m_hovered_index{-1};
    int m_open_index{-1};
    uint64_t m_overlay_id{0};
    Rect m_popup_client_rect{};
    PointF m_prev_mouse_pos{0.0f, 0.0f};
    bool m_has_prev_mouse{false};
    PointF m_aim_apex{0.0f, 0.0f};
    bool m_aim_active{false};
    int m_pending_open_idx{-1};
    std::chrono::steady_clock::time_point m_aim_last_move{};
    bool m_switching{false};
    int m_last_dismiss_idx{-1};
    std::chrono::steady_clock::time_point m_last_dismiss{};

    float m_pill_x{0.0f};
    float m_pill_w{0.0f};
    float m_pill_alpha{0.0f};
    bool m_pill_init{false};
    bool m_has_anim_time{false};
    std::chrono::steady_clock::time_point m_last_anim_time{};
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
};

// C++20 指定初始化器声明式 WinUI 3 MenuBar
struct MenuBar : BindableMixin<MenuBar, RenderMenuBar> {
    std::vector<MenuBarItem> items{};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class MenuBarWidget : public Widget {
public:
    explicit MenuBarWidget(const MenuBar& desc) : m_desc(desc) {}
    explicit MenuBarWidget(MenuBar&& desc) noexcept : m_desc(std::move(desc)) {}
    ~MenuBarWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderMenuBar>();
        node->set_items(m_desc.items);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderMenuBar>(node)) {
            n->set_items(m_desc.items);
        }
    }

private:
    MenuBar m_desc;
};

inline MenuBar::operator WidgetPtr() const& { return make_widget<MenuBarWidget>(*this); }
inline MenuBar::operator WidgetPtr() && { return make_widget<MenuBarWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
