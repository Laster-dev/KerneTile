#pragma once

#include "dock_panel_data.hpp"
#include "dock_panel_style.hpp"
#include <ktui/core/strings.hpp>
#include "../split_view/split_view_logic.hpp"
#include <ktui/app/application.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>

#include "dock_panel_detached.hpp"

namespace ktui {

class RenderDockPanel : public RenderBox {
public:
    static constexpr float kHeaderHeight = 32.0f;
    static constexpr float kSplitterHit  = 6.0f;
    static constexpr float kMinPaneSize  = 110.0f;

    struct LivePane {
        std::string id;
        std::string title;
        DockZone zone{DockZone::Center};
        DockZone initial_zone{DockZone::Center};
        bool closable{false};
        bool closed{false};
        bool detached{false};
        std::shared_ptr<RenderBox> content_box;
        std::weak_ptr<Window> detached_window;
        std::weak_ptr<detail::RenderDetachedPaneShell> detached_shell;
    };

    using DetachCallback = Fn<bool(const std::string& id, const std::string& title, std::shared_ptr<RenderBox> content_box, Fn<void()> on_redock)>;

    RenderDockPanel(float left_w = 200.0f,
                    float right_w = 200.0f,
                    float top_h = 110.0f,
                    float bottom_h = 120.0f,
                    EdgeInsets padding = EdgeInsets::zero(),
                    bool allow_detach_window = true,
                    Fn<void(const std::string&, DockZone)> on_dock_changed = nullptr,
                    Fn<void(const std::string&, bool)> on_detach_changed = nullptr,
                    DetachCallback on_detach_pane = nullptr) noexcept
        : m_left_w(left_w), m_right_w(right_w), m_top_h(top_h), m_bottom_h(bottom_h),
          m_init_left_w(left_w), m_init_right_w(right_w), m_init_top_h(top_h), m_init_bottom_h(bottom_h),
          m_padding(padding),
          m_allow_detach_window(allow_detach_window),
          m_on_dock_changed(std::move(on_dock_changed)),
          m_on_detach_changed(std::move(on_detach_changed)),
          m_on_detach_pane(std::move(on_detach_pane)) {
        setup_behavior();
    }

    ~RenderDockPanel() override {
        if (m_alive) *m_alive = false;
        // 离开页面或销毁工作区时，延迟安全关闭所有仍打开的独立弹出窗口
        std::vector<std::weak_ptr<Window>> to_close;
        for (auto& p : m_panes) {
            if (!p.detached_window.expired()) {
                to_close.push_back(p.detached_window);
            }
        }
        if (!to_close.empty()) {
            if (auto* app = Application::current()) {
                app->dispatcher().post_task([wins = std::move(to_close)]() {
                    for (const auto& w : wins) {
                        if (auto win = w.lock(); win && !win->is_closed()) {
                            win->close();
                        }
                    }
                });
            }
        }
    }

    void set_panes(std::vector<LivePane> panes) {
        while (first_child()) remove_child(first_child());
        m_panes = std::move(panes);
        for (auto& p : m_panes) {
            if (p.content_box && !p.detached) {
                if (p.content_box->parent()) p.content_box->parent()->remove_child(p.content_box.get());
                append_child(p.content_box.get());
            }
        }
        m_active_tab.fill(0);
        mark_needs_layout();
    }

    void set_allow_detach_window(bool allow) noexcept {
        if (m_allow_detach_window != allow) {
            m_allow_detach_window = allow;
            mark_needs_paint();
        }
    }
    void set_on_dock_changed(Fn<void(const std::string&, DockZone)> cb) { m_on_dock_changed = std::move(cb); }
    void set_on_detach_changed(Fn<void(const std::string&, bool)> cb) { m_on_detach_changed = std::move(cb); }
    void set_on_detach_pane(DetachCallback cb) { m_on_detach_pane = std::move(cb); }
    void set_padding(EdgeInsets p) noexcept {
        if (!(m_padding == p)) { m_padding = p; mark_needs_layout(); }
    }

    // 编程式将指定面板停靠至目标区域 (若已弹出独立窗口则安全收回原 RenderBox)
    bool dock_pane(const std::string& id, DockZone target) {
        for (size_t i = 0; i < m_panes.size(); ++i) {
            auto& p = m_panes[i];
            if (p.id != id) continue;

            const bool was_detached = p.detached;
            if (auto sh = p.detached_shell.lock()) {
                auto reclaimed = sh->release_content();
                if (reclaimed) p.content_box = std::move(reclaimed);
            }
            p.detached_shell.reset();

            auto old_win = p.detached_window.lock();
            p.detached_window.reset();

            if (p.content_box) {
                detail::forget_render_subtree(p.content_box.get());
                if (p.content_box->parent() != this) {
                    if (p.content_box->parent()) p.content_box->parent()->remove_child(p.content_box.get());
                    append_child(p.content_box.get());
                }
            }

            p.zone = target;
            p.closed = false;
            p.detached = false;
            auto list = panes_in_zone(target);
            for (size_t k = 0; k < list.size(); ++k) {
                if (list[k] == i) {
                    const size_t tz = static_cast<size_t>(target);
                    if (m_active_tab[tz] != k) {
                        m_active_tab[tz] = k;
                        m_tab_anim_t[tz] = 0.0f;
                    }
                    break;
                }
            }

            // 延迟关闭原独立窗口，绝不在回调栈内同步销毁 HWND
            if (old_win && !old_win->is_closed()) {
                if (auto* app = Application::current()) {
                    app->dispatcher().post_task([old_win]() {
                        if (!old_win->is_closed()) old_win->close();
                    });
                }
            }

            mark_needs_layout();
            mark_needs_paint();
            if (was_detached && m_on_detach_changed) m_on_detach_changed(id, false);
            if (m_on_dock_changed) m_on_dock_changed(id, target);
            return true;
        }
        return false;
    }

    // 将指定面板安全撕出为独立 OS 窗口 (内置默认实现，无需业务层编写任何样板代码)
    bool detach_pane(const std::string& id);

    // 一键恢复初始停靠布局与尺寸 (自动收回所有已弹出窗口)
    void reset_layout() {
        m_left_w = m_init_left_w;
        m_right_w = m_init_right_w;
        m_top_h = m_init_top_h;
        m_bottom_h = m_init_bottom_h;

        std::vector<std::shared_ptr<Window>> to_close;
        for (auto& p : m_panes) {
            if (auto sh = p.detached_shell.lock()) {
                auto reclaimed = sh->release_content();
                if (reclaimed) p.content_box = std::move(reclaimed);
            }
            p.detached_shell.reset();
            if (auto w = p.detached_window.lock(); w && !w->is_closed()) {
                to_close.push_back(std::move(w));
            }
            p.detached_window.reset();
            p.zone = p.initial_zone;
            p.closed = false;
            p.detached = false;
            if (p.content_box) {
                detail::forget_render_subtree(p.content_box.get());
                if (p.content_box->parent() != this) {
                    if (p.content_box->parent()) p.content_box->parent()->remove_child(p.content_box.get());
                    append_child(p.content_box.get());
                }
            }
        }
        m_active_tab.fill(0);
        m_dragging_pane.reset();
        m_dragging_splitter.reset();
        m_drop_target.reset();
        m_drag_started = false;
        m_will_detach_on_drop = false;
        m_external_drag_active = false;

        if (!to_close.empty()) {
            if (auto* app = Application::current()) {
                app->dispatcher().post_task([wins = std::move(to_close)]() {
                    for (const auto& w : wins) {
                        if (w && !w->is_closed()) w->close();
                    }
                });
            }
        }
        mark_needs_layout();
        mark_needs_paint();
    }

    [[nodiscard]] std::optional<DockZone> pane_zone(const std::string& id) const noexcept {
        for (const auto& p : m_panes) {
            if (p.id == id && !p.closed && !p.detached) return p.zone;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool is_pane_detached(const std::string& id) const noexcept {
        for (const auto& p : m_panes) {
            if (p.id == id) return p.detached;
        }
        return false;
    }

    [[nodiscard]] Rect zone_rect(DockZone z) const noexcept {
        return m_zone_rects[static_cast<size_t>(z)];
    }

    void perform_layout() override;

    bool hit_test(HitTestResult& result, PointF position) override {
        if (m_dragging_pane.has_value() || m_dragging_splitter.has_value()) {
            result.add(this, position);
            return true;
        }
        if (!bounds().contains(position)) return false;

        for (const auto& sp : m_splitters) {
            if (sp.hit_rect.contains(position)) {
                cursor_shape = (sp.kind == SplitterKind::Left || sp.kind == SplitterKind::Right)
                    ? CursorShape::ResizeEW : CursorShape::ResizeNS;
                result.add(this, position);
                return true;
            }
        }

        for (size_t z = 0; z < 5; ++z) {
            Rect zr = m_zone_rects[z];
            if (!zr.is_empty() && Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight).contains(position)) {
                cursor_shape = CursorShape::PointingHand;
                result.add(this, position);
                return true;
            }
        }

        cursor_shape = CursorShape::Arrow;
        for (size_t z = 0; z < 5; ++z) {
            auto list = panes_in_zone(static_cast<DockZone>(z));
            if (list.empty()) continue;
            size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
            auto& pane = m_panes[list[active_idx]];
            if (pane.content_box && pane.content_box->parent() == this) {
                PointF child_pos{position.x - pane.content_box->offset().x, position.y - pane.content_box->offset().y};
                if (pane.content_box->hit_test(result, child_pos)) {
                    result.add(this, position);
                    return true;
                }
            }
        }
        result.add(this, position);
        return true;
    }

    bool hit_test_touch(HitTestResult& result, PointF position) override {
        if (m_dragging_pane.has_value() || m_dragging_splitter.has_value()) {
            result.add(this, position);
            return true;
        }
        if (!bounds().contains(position)) return false;
        for (const auto& sp : m_splitters) {
            if (touch::expand_hit_rect(sp.hit_rect, touch::kSplitterTargetSize).contains(position)) {
                result.add(this, position);
                return true;
            }
        }
        for (size_t z = 0; z < 5; ++z) {
            Rect zr = m_zone_rects[z];
            if (!zr.is_empty() && Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight).contains(position)) {
                result.add(this, position);
                return true;
            }
        }
        for (size_t z = 0; z < 5; ++z) {
            auto list = panes_in_zone(static_cast<DockZone>(z));
            if (list.empty()) continue;
            size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
            auto& pane = m_panes[list[active_idx]];
            if (pane.content_box && pane.content_box->parent() == this) {
                PointF child_pos{position.x - pane.content_box->offset().x, position.y - pane.content_box->offset().y};
                if (pane.content_box->hit_test_touch(result, child_pos)) {
                    result.add(this, position);
                    return true;
                }
            }
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override;

private:
    enum class SplitterKind : uint8_t { Left, Right, Top, Bottom };
    struct Splitter { SplitterKind kind; Rect hit_rect; Rect visual_rect; };
    struct TabHit {
        DockZone zone;
        size_t tab_index;
        size_t pane_index;
        Rect rect;
        Rect close_rect;
    };
    struct CompassButton { DockZone zone; Rect rect; };

    [[nodiscard]] bool can_detach() const noexcept {
        return m_on_detach_pane != nullptr || (m_allow_detach_window && Application::current() != nullptr);
    }

    [[nodiscard]] static Rect detach_btn_rect(Rect zr) noexcept {
        return Rect::from_xywh(zr.max_x - 24.0f, zr.min_y + (kHeaderHeight - 20.0f) * 0.5f, 20.0f, 20.0f);
    }

    [[nodiscard]] std::vector<size_t> panes_in_zone(DockZone z) const {
        std::vector<size_t> out;
        for (size_t i = 0; i < m_panes.size(); ++i) {
            if (!m_panes[i].closed && !m_panes[i].detached && m_panes[i].zone == z) out.push_back(i);
        }
        return out;
    }

    [[nodiscard]] std::array<CompassButton, 5> compute_compass() const noexcept {
        const float cx = m_size.x * 0.5f;
        const float cy = m_size.y * 0.5f;
        const float s = 30.0f;
        const float g = 5.0f;
        return {{
            {DockZone::Center, Rect::from_xywh(cx - s * 0.5f, cy - s * 0.5f, s, s)},
            {DockZone::Top,    Rect::from_xywh(cx - s * 0.5f, cy - s * 1.5f - g, s, s)},
            {DockZone::Bottom, Rect::from_xywh(cx - s * 0.5f, cy + s * 0.5f + g, s, s)},
            {DockZone::Left,   Rect::from_xywh(cx - s * 1.5f - g, cy - s * 0.5f, s, s)},
            {DockZone::Right,  Rect::from_xywh(cx + s * 0.5f + g, cy - s * 0.5f, s, s)}
        }};
    }

    [[nodiscard]] std::optional<DockZone> hit_test_drop_target(PointF p, bool& out_detach) const noexcept {
        out_detach = false;
        if (can_detach() && !bounds().contains(p)) {
            out_detach = true;
            return std::nullopt;
        }
        if (!bounds().contains(p)) return std::nullopt;

        for (const auto& cb : compute_compass()) {
            if (cb.rect.contains(p)) return cb.zone;
        }

        for (size_t z = 0; z < 5; ++z) {
            Rect zr = m_zone_rects[z];
            if (!zr.is_empty() && Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight).contains(p)) {
                return static_cast<DockZone>(z);
            }
        }

        const float nx = p.x / std::max(1.0f, m_size.x);
        const float ny = p.y / std::max(1.0f, m_size.y);
        const float dl = nx, dr = 1.0f - nx, dt = ny, db = 1.0f - ny;
        const float min_edge = std::min({dl, dr, dt, db});
        if (min_edge <= 0.26f) {
            if (min_edge == dl) return DockZone::Left;
            if (min_edge == dr) return DockZone::Right;
            if (min_edge == dt) return DockZone::Top;
            return DockZone::Bottom;
        }
        return DockZone::Center;
    }

    [[nodiscard]] Rect compute_drop_preview(DockZone z) const noexcept {
        const float ox = m_padding.left + 4.0f;
        const float oy = m_padding.top + 4.0f;
        const float iw = std::max(0.0f, m_size.x - m_padding.horizontal() - 8.0f);
        const float ih = std::max(0.0f, m_size.y - m_padding.vertical() - 8.0f);
        switch (z) {
            case DockZone::Left:   return Rect::from_xywh(ox, oy, std::min(m_left_w, iw * 0.36f), ih);
            case DockZone::Right: {
                float rw = std::min(m_right_w, iw * 0.36f);
                return Rect::from_xywh(ox + iw - rw, oy, rw, ih);
            }
            case DockZone::Top:    return Rect::from_xywh(ox, oy, iw, std::min(m_top_h, ih * 0.34f));
            case DockZone::Bottom: {
                float bh = std::min(m_bottom_h, ih * 0.34f);
                return Rect::from_xywh(ox, oy + ih - bh, iw, bh);
            }
            case DockZone::Center: {
                Rect cr = m_zone_rects[static_cast<size_t>(DockZone::Center)];
                if (!cr.is_empty()) return cr.inset(3.0f);
                return Rect::from_xywh(ox + iw * 0.2f, oy + ih * 0.2f, iw * 0.6f, ih * 0.6f);
            }
        }
        return Rect{};
    }

    void paint_docking_compass(PaintContext& context, bool is_dark, Color accent, Color text_pri) const {
        const float cx = m_size.x * 0.5f;
        const float cy = m_size.y * 0.5f;
        Rect backplate = Rect::from_xywh(cx - 56.0f, cy - 56.0f, 112.0f, 112.0f);
        Color plate_bg = is_dark ? Color{0.10f, 0.11f, 0.14f, 0.82f} : Color{0.98f, 0.99f, 1.0f, 0.88f};
        Color plate_bd = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.12f} : Color{0.0f, 0.0f, 0.0f, 0.10f};
        context.draw_rect(backplate, plate_bg, 10.0f, 0.0f);
        context.draw_rect(backplate, plate_bd, 10.0f, 1.0f);

        for (const auto& cb : compute_compass()) {
            const bool hot = (m_drop_target == cb.zone);
            Color btn_bg = hot
                ? Color{accent.r, accent.g, accent.b, 0.92f}
                : (is_dark ? Color{1.0f, 1.0f, 1.0f, 0.07f} : Color{0.0f, 0.0f, 0.0f, 0.05f});
            context.draw_rect(cb.rect, btn_bg, 5.0f, 0.0f);

            Rect icon_box = Rect::from_xywh(cb.rect.min_x + 6.0f, cb.rect.min_y + 6.0f, 18.0f, 18.0f);
            Color wire = hot ? Color::white() : Color{text_pri.r, text_pri.g, text_pri.b, 0.45f};
            Color fill = hot ? Color::white() : accent;
            context.draw_rect(icon_box, wire, 2.0f, 1.0f);
            switch (cb.zone) {
                case DockZone::Left:   context.draw_rect(Rect::from_xywh(icon_box.min_x + 2.0f, icon_box.min_y + 2.0f, 5.0f, 14.0f), fill, 1.0f, 0.0f); break;
                case DockZone::Right:  context.draw_rect(Rect::from_xywh(icon_box.max_x - 7.0f, icon_box.min_y + 2.0f, 5.0f, 14.0f), fill, 1.0f, 0.0f); break;
                case DockZone::Top:    context.draw_rect(Rect::from_xywh(icon_box.min_x + 2.0f, icon_box.min_y + 2.0f, 14.0f, 5.0f), fill, 1.0f, 0.0f); break;
                case DockZone::Bottom: context.draw_rect(Rect::from_xywh(icon_box.min_x + 2.0f, icon_box.max_y - 7.0f, 14.0f, 5.0f), fill, 1.0f, 0.0f); break;
                case DockZone::Center: context.draw_rect(Rect::from_xywh(icon_box.min_x + 4.0f, icon_box.min_y + 4.0f, 10.0f, 10.0f), fill, 1.5f, 0.0f); break;
            }
        }
    }

    void advance_animations();

    void update_external_drag(PointF local_pos, Window* parent_win) {
        const bool inside = bounds().contains(local_pos);
        bool next_active = inside;
        std::optional<DockZone> next_target;
        if (inside) {
            bool dummy_detach = false;
            next_target = hit_test_drop_target(local_pos, dummy_detach);
        }
        if (m_external_drag_active != next_active || m_drop_target != next_target) {
            m_external_drag_active = next_active;
            m_drop_target = next_target;
            mark_needs_paint();
            if (parent_win) {
                parent_win->request_redraw();
                if (parent_win->platform_window()) parent_win->platform_window()->invalidate();
            }
        }
    }

    std::optional<DockZone> finish_external_drag(PointF local_pos, Window* parent_win) {
        std::optional<DockZone> target;
        if (bounds().contains(local_pos)) {
            bool dummy_detach = false;
            target = hit_test_drop_target(local_pos, dummy_detach);
        }
        if (m_external_drag_active || m_drop_target.has_value()) {
            m_external_drag_active = false;
            m_drop_target.reset();
            mark_needs_paint();
            if (parent_win) {
                parent_win->request_redraw();
                if (parent_win->platform_window()) parent_win->platform_window()->invalidate();
            }
        }
        return target;
    }

    void open_pane_context_menu(size_t pane_index, PointF event_window_pos);

    void setup_behavior();

    float m_left_w{200.0f}, m_right_w{200.0f}, m_top_h{110.0f}, m_bottom_h{120.0f};
    float m_init_left_w{200.0f}, m_init_right_w{200.0f}, m_init_top_h{110.0f}, m_init_bottom_h{120.0f};
    EdgeInsets m_padding{EdgeInsets::zero()};
    bool m_allow_detach_window{true};
    Fn<void(const std::string&, DockZone)> m_on_dock_changed{nullptr};
    Fn<void(const std::string&, bool)> m_on_detach_changed{nullptr};
    DetachCallback m_on_detach_pane{nullptr};

    std::vector<LivePane> m_panes;
    std::array<Rect, 5> m_zone_rects{};
    std::array<size_t, 5> m_active_tab{};
    std::vector<Splitter> m_splitters;
    std::vector<TabHit> m_tab_hits;

    std::optional<SplitterKind> m_hovered_splitter;
    std::optional<SplitterKind> m_dragging_splitter;
    std::optional<DockZone> m_hovered_detach_zone;
    size_t m_hovered_pane_index{SIZE_MAX};
    std::optional<size_t> m_dragging_pane;
    bool m_drag_started{false};
    bool m_will_detach_on_drop{false};
    bool m_external_drag_active{false};
    PointF m_drag_origin{};
    PointF m_drag_pos{};
    std::optional<DockZone> m_drop_target;

    std::chrono::steady_clock::time_point m_anim_last_time{};
    std::array<Rect, 5> m_anim_zone_rects{};
    std::array<bool, 5> m_anim_zone_init{};
    std::array<float, 5> m_tab_ind_x{};
    std::array<float, 5> m_tab_ind_w{};
    std::array<bool, 5> m_tab_ind_init{};
    std::array<float, 5> m_tab_anim_t{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
};

} // namespace ktui

#include "dock_panel_paint.hpp"
#include "dock_panel_behavior.hpp"
#include "dock_panel_widget.hpp"
