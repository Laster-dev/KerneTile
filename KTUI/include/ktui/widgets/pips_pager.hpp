#pragma once

#include "common/fluent_icons.hpp"
#include "widget.hpp"
#include <ktui/animation/frame_clock.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// WinUI 3 PipsPager 圆点分页指示器渲染盒
// 切换页码时当前圆点平滑放大至 22px 并在圆内淡入显示页码数字，停留 1 秒后数字淡出、圆点平滑缩回
class RenderPipsPager : public RenderBox {
public:
    RenderPipsPager() noexcept {
        cursor_shape = CursorShape::Arrow;
        setup_behavior();
    }

    ~RenderPipsPager() override = default;

    void set_properties(int number_of_pages,
                        int selected_index,
                        int max_visible_pips,
                        bool show_nav_buttons,
                        Fn<void(int)> on_selected_changed) {
        m_pages = std::max(1, number_of_pages);
        m_max_visible = std::clamp(max_visible_pips, 3, 15);
        m_show_nav = show_nav_buttons;
        int clamped = std::clamp(selected_index, 0, m_pages - 1);
        if (!m_initialized) {
            m_initialized = true;
            m_selected = clamped;
            sync_anim_arrays(true);
        } else if (m_selected != clamped) {
            m_selected = clamped;
            trigger_badge_animation();
        }
        m_on_selected = std::move(on_selected_changed);
        mark_needs_layout();
    }

    void set_selected_index(int idx) {
        int clamped = std::clamp(idx, 0, m_pages - 1);
        if (m_selected != clamped) {
            m_selected = clamped;
            trigger_badge_animation();
        }
    }

    [[nodiscard]] int selected_index() const noexcept { return m_selected; }
    [[nodiscard]] int number_of_pages() const noexcept { return m_pages; }
    [[nodiscard]] bool is_badge_active() const noexcept { return m_badge_active; }
    [[nodiscard]] float badge_alpha() const noexcept { return m_badge_alpha; }
    [[nodiscard]] float active_pip_diameter() const noexcept {
        int slot = m_selected - visible_window_start();
        if (slot >= 0 && slot < static_cast<int>(m_pip_diameters.size())) {
            return m_pip_diameters[static_cast<size_t>(slot)];
        }
        return kActiveRestD;
    }

    void select_page(int idx) {
        int clamped = std::clamp(idx, 0, m_pages - 1);
        if (clamped != m_selected) {
            m_selected = clamped;
            trigger_badge_animation();
            if (m_on_selected) m_on_selected(m_selected);
        }
    }

    // 推进一帧圆点缩放、数字淡入淡出与槽位间距动画 (返回是否仍需重绘)
    bool step_animations() {
        sync_anim_arrays(false);
        const int vis = std::min(m_pages, m_max_visible);
        const int win_start = visible_window_start();
        const double now = FrameClock::global().frame_time_sec();
        const float dt = std::clamp(FrameClock::global().dt(), 1.0f / 240.0f, 0.05f);

        bool animating = false;
        if (m_badge_active) {
            double elapsed = now - m_badge_trigger_time;
            if (elapsed >= kBadgeHoldSeconds) {
                m_badge_active = false;
            } else {
                // 保持重绘以便 1 秒后自动触发平滑缩小
                animating = true;
            }
        }

        const float badge_target = m_badge_active ? 1.0f : 0.0f;
        const float badge_speed = m_badge_active ? 16.0f : 10.0f;
        if (std::abs(m_badge_alpha - badge_target) > 0.004f) {
            m_badge_alpha += (badge_target - m_badge_alpha) * std::min(1.0f, dt * badge_speed);
            if (std::abs(m_badge_alpha - badge_target) <= 0.004f) m_badge_alpha = badge_target;
            else animating = true;
        } else {
            m_badge_alpha = badge_target;
        }

        for (int i = 0; i < vis; ++i) {
            const size_t idx = static_cast<size_t>(i);
            const int page = win_start + i;
            const bool active = (page == m_selected);
            const bool hovered = (page == m_hover_page);
            const bool is_edge = ((i == 0 && win_start > 0) || (i == vis - 1 && win_start + vis < m_pages));

            float target_d = active
                ? (kActiveRestD + (kActiveExpandedD - kActiveRestD) * (m_badge_active ? 1.0f : 0.0f))
                : (is_edge ? 4.5f : (hovered ? 7.0f : 5.5f));
            float target_slot = active
                ? (kPipStep + (kExpandedSlotW - kPipStep) * (m_badge_active ? 1.0f : 0.0f))
                : kPipStep;
            float target_blend = active ? 1.0f : 0.0f;

            float k_d = std::min(1.0f, dt * (m_badge_active ? 18.0f : 12.0f));
            float k_s = std::min(1.0f, dt * 15.0f);

            if (std::abs(m_pip_diameters[idx] - target_d) > 0.03f) {
                m_pip_diameters[idx] += (target_d - m_pip_diameters[idx]) * k_d;
                animating = true;
            } else {
                m_pip_diameters[idx] = target_d;
            }

            if (std::abs(m_pip_slots[idx] - target_slot) > 0.03f) {
                m_pip_slots[idx] += (target_slot - m_pip_slots[idx]) * k_s;
                animating = true;
            } else {
                m_pip_slots[idx] = target_slot;
            }

            if (std::abs(m_pip_active_blend[idx] - target_blend) > 0.005f) {
                m_pip_active_blend[idx] += (target_blend - m_pip_active_blend[idx]) * k_s;
                animating = true;
            } else {
                m_pip_active_blend[idx] = target_blend;
            }
        }
        return animating;
    }

    void perform_layout() override {
        int vis = std::min(m_pages, m_max_visible);
        float nav_w = m_show_nav ? (kNavBtnW * 2.0f + 8.0f) : 0.0f;
        float track_w = static_cast<float>(std::max(0, vis - 1)) * kPipStep + kExpandedSlotW;
        set_size(m_constraints.constrain(Vec2{std::ceil(nav_w + track_w), 30.0f}));
    }

    void paint(PaintContext& ctx) override {
        if (step_animations()) {
            mark_needs_paint();
        }

        const Theme& theme = ThemeContext::current();
        const float h = size().y;

        Color icon_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.82f} : Color{0.0f, 0.0f, 0.0f, 0.74f};
        Color dis_c  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.24f} : Color{0.0f, 0.0f, 0.0f, 0.22f};
        Color pip_c  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.34f} : Color{0.0f, 0.0f, 0.0f, 0.28f};
        Color pip_h  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.72f} : Color{0.0f, 0.0f, 0.0f, 0.62f};
        Color active_c = theme.is_dark
            ? Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}
            : Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
        Color num_c = theme.is_dark
            ? Color{0.06f, 0.08f, 0.12f, 1.0f}
            : Color{1.0f, 1.0f, 1.0f, 1.0f};

        if (m_show_nav) {
            Rect pr = prev_btn_rect();
            Rect nr = next_btn_rect();
            bool can_prev = (m_selected > 0);
            bool can_next = (m_selected + 1 < m_pages);
            if (m_hover_prev && can_prev) ctx.draw_rect(pr, theme.colors.subtle_fill_secondary, 4.0f, 0.0f);
            if (m_hover_next && can_next) ctx.draw_rect(nr, theme.colors.subtle_fill_secondary, 4.0f, 0.0f);
            fluent_icons::draw_centered(ctx, fluent_icons::kChevronLeft, pr, 11.0f, can_prev ? icon_c : dis_c);
            fluent_icons::draw_centered(ctx, fluent_icons::kChevronRight, nr, 11.0f, can_next ? icon_c : dis_c);
        }

        const int win_start = visible_window_start();
        const int vis = std::min(m_pages, m_max_visible);
        const float track_left = m_show_nav ? (kNavBtnW + 4.0f) : 0.0f;
        const float track_right = m_show_nav ? (size().x - kNavBtnW - 4.0f) : size().x;
        const float track_w = std::max(0.0f, track_right - track_left);

        float total_slots_w = 0.0f;
        for (int i = 0; i < vis; ++i) total_slots_w += m_pip_slots[static_cast<size_t>(i)];

        float cur_x = track_left + (track_w - total_slots_w) * 0.5f;
        const float cy = h * 0.5f;

        for (int i = 0; i < vis; ++i) {
            const size_t idx = static_cast<size_t>(i);
            const int page = win_start + i;
            const bool active = (page == m_selected);
            const bool hovered = (page == m_hover_page);
            const float slot_w = m_pip_slots[idx];
            const float d = m_pip_diameters[idx];
            const float blend = m_pip_active_blend[idx];

            Color base = hovered ? pip_h : pip_c;
            Color dot_c{
                base.r + (active_c.r - base.r) * blend,
                base.g + (active_c.g - base.g) * blend,
                base.b + (active_c.b - base.b) * blend,
                base.a + (active_c.a - base.a) * blend
            };

            float cx = cur_x + slot_w * 0.5f;
            Rect dot_r = Rect::from_xywh(cx - d * 0.5f, cy - d * 0.5f, d, d);
            ctx.draw_rect(dot_r, dot_c, d * 0.5f, 0.0f);

            // 当前激活圆点在放大期间平滑显示 1-based 页码数字
            if (active && m_badge_alpha > 0.02f && d >= 13.0f) {
                std::string label = std::to_string(page + 1);
                float font_sz = (label.size() >= 2) ? 10.0f : 11.0f;
                Vec2 ts = TextMeasure::measure(label, font_sz);
                Color tc = num_c;
                tc.a = m_badge_alpha * std::clamp((d - 13.0f) / 6.0f, 0.0f, 1.0f);
                ctx.draw_text(label,
                              PointF{std::round(cx - ts.x * 0.5f), std::round(cy - ts.y * 0.5f)},
                              font_sz, tc);
            }

            cur_x += slot_w;
        }
    }

private:
    static constexpr float kNavBtnW = 24.0f;
    static constexpr float kPipStep = 14.0f;
    static constexpr float kActiveRestD = 8.5f;
    static constexpr float kActiveExpandedD = 22.0f;
    static constexpr float kExpandedSlotW = 28.0f;
    static constexpr double kBadgeHoldSeconds = 1.0;

    void trigger_badge_animation() {
        m_badge_active = true;
        m_badge_trigger_time = FrameClock::global().frame_time_sec();
        mark_needs_paint();
    }

    void sync_anim_arrays(bool snap_immediate) {
        const size_t vis = static_cast<size_t>(std::min(m_pages, m_max_visible));
        if (m_pip_diameters.size() == vis && !snap_immediate) return;
        m_pip_diameters.assign(vis, 5.5f);
        m_pip_slots.assign(vis, kPipStep);
        m_pip_active_blend.assign(vis, 0.0f);
        int win_start = visible_window_start();
        for (size_t i = 0; i < vis; ++i) {
            if (win_start + static_cast<int>(i) == m_selected) {
                m_pip_diameters[i] = kActiveRestD;
                m_pip_active_blend[i] = 1.0f;
            }
        }
    }

    int visible_window_start() const noexcept {
        if (m_pages <= m_max_visible) return 0;
        int half = m_max_visible / 2;
        return std::clamp(m_selected - half, 0, m_pages - m_max_visible);
    }

    Rect prev_btn_rect() const noexcept { return Rect::from_xywh(0.0f, 3.0f, kNavBtnW, size().y - 6.0f); }
    Rect next_btn_rect() const noexcept { return Rect::from_xywh(size().x - kNavBtnW, 3.0f, kNavBtnW, size().y - 6.0f); }

    int hit_pip(Vec2 local) const noexcept {
        const int win_start = visible_window_start();
        const int vis = std::min(m_pages, m_max_visible);
        const float track_left = m_show_nav ? (kNavBtnW + 4.0f) : 0.0f;
        const float track_right = m_show_nav ? (size().x - kNavBtnW - 4.0f) : size().x;
        const float track_w = std::max(0.0f, track_right - track_left);

        float total_slots_w = 0.0f;
        for (int i = 0; i < vis; ++i) {
            total_slots_w += (i < static_cast<int>(m_pip_slots.size())) ? m_pip_slots[static_cast<size_t>(i)] : kPipStep;
        }
        float cur_x = track_left + (track_w - total_slots_w) * 0.5f;
        for (int i = 0; i < vis; ++i) {
            int page = win_start + i;
            float slot_w = (i < static_cast<int>(m_pip_slots.size())) ? m_pip_slots[static_cast<size_t>(i)] : kPipStep;
            if (local.x >= cur_x && local.x < cur_x + slot_w) return page;
            cur_x += slot_w;
        }
        return -1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            bool hp = m_show_nav && prev_btn_rect().contains(e.local_position);
            bool hn = m_show_nav && next_btn_rect().contains(e.local_position);
            int pg = hit_pip(e.local_position);
            cursor_shape = (hp || hn || pg >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (hp != m_hover_prev || hn != m_hover_next || pg != m_hover_page) {
                m_hover_prev = hp;
                m_hover_next = hn;
                m_hover_page = pg;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hover_prev = m_hover_next = false;
            m_hover_page = -1;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            if (m_show_nav && prev_btn_rect().contains(e.local_position)) {
                select_page(m_selected - 1);
                e.stop_propagation();
                return;
            }
            if (m_show_nav && next_btn_rect().contains(e.local_position)) {
                select_page(m_selected + 1);
                e.stop_propagation();
                return;
            }
            int pg = hit_pip(e.local_position);
            if (pg >= 0) {
                select_page(pg);
                e.stop_propagation();
            }
        };
    }

    int m_pages{5};
    int m_selected{0};
    int m_max_visible{7};
    bool m_show_nav{true};
    bool m_initialized{false};
    bool m_hover_prev{false};
    bool m_hover_next{false};
    int m_hover_page{-1};

    bool m_badge_active{false};
    double m_badge_trigger_time{0.0};
    float m_badge_alpha{0.0f};
    std::vector<float> m_pip_diameters;
    std::vector<float> m_pip_slots;
    std::vector<float> m_pip_active_blend;

    Fn<void(int)> m_on_selected;

public:
    void wrap_on_selected(Fn<void(int)> extra) {
        auto prev = std::move(m_on_selected);
        m_on_selected = [prev = std::move(prev), extra = std::move(extra)](int idx) {
            if (extra) extra(idx);
            if (prev) prev(idx);
        };
    }
};

struct PipsPager : BindableMixin<PipsPager, RenderPipsPager> {
    int number_of_pages{5};
    int selected_index{0};
    int max_visible_pips{7};
    bool show_nav_buttons{true};
    Fn<void(int)> on_selected_index_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderPipsPager>& node, Property<int>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_selected_index(prop.get());
    std::weak_ptr<RenderPipsPager> weak = node;
    bg.subscribe(prop, [weak, guard](int idx) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_selected_index(idx);
        }
    });
    Property<int>* p = &prop;
    node->wrap_on_selected([p, guard](int idx) {
        *guard = true;
        p->set(idx);
        *guard = false;
    });
}

class PipsPagerWidget : public Widget {
public:
    explicit PipsPagerWidget(const PipsPager& desc) : m_desc(desc) {}
    explicit PipsPagerWidget(PipsPager&& desc) noexcept : m_desc(std::move(desc)) {}
    ~PipsPagerWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderPipsPager>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderPipsPager>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderPipsPager>& node) {
        node->set_properties(
            m_desc.number_of_pages, m_desc.selected_index,
            m_desc.max_visible_pips, m_desc.show_nav_buttons,
            m_desc.on_selected_index_changed);
    }

    PipsPager m_desc;
};

inline PipsPager::operator WidgetPtr() const& {
    return make_widget<PipsPagerWidget>(*this);
}
inline PipsPager::operator WidgetPtr() && {
    return make_widget<PipsPagerWidget>(std::move(*this));
}

} // namespace ktui
