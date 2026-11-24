#pragma once

#include "navigation_view_data.hpp"
#include "navigation_view_style.hpp"
#include <ktui/widgets/auto_suggest_box.hpp>
#include <ktui/widgets/common/fluent_icons.hpp>
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

// WinUI 3 Compact / Top 模式子菜单浮层 (RenderNavigationSubmenuFlyout)
// 挂载于 OverlayManager 顶层，折叠状态下点击含子项的父导航项直接弹出选择子项，无需强行展开侧边栏
class RenderNavigationSubmenuFlyout : public RenderBox {
public:
    RenderNavigationSubmenuFlyout(std::vector<NavigationViewItem> children,
                                  std::string selected_id,
                                  std::function<void(const std::string&)> on_select)
        : m_children(std::move(children)),
          m_selected_id(std::move(selected_id)),
          m_on_select(std::move(on_select)) {
        cursor_shape = CursorShape::Arrow;
        m_open_start = std::chrono::steady_clock::now();
        setup_behavior();
    }

    ~RenderNavigationSubmenuFlyout() override = default;

    [[nodiscard]] Vec2 compute_ideal_size() const {
        float max_w = 160.0f;
        for (const auto& item : m_children) {
            Vec2 ts = TextMeasure::measure(item.label, 13.0f);
            float icon_w = (item.icon != NavigationIcon::None || item.icon_painter) ? 26.0f : 0.0f;
            max_w = std::max(max_w, ts.x + icon_w + 38.0f);
        }
        float h = static_cast<float>(std::max<size_t>(1, m_children.size())) * kItemH + 8.0f;
        return Vec2{std::ceil(std::min(max_w, 280.0f)), h};
    }

    void perform_layout() override {
        Vec2 ideal = compute_ideal_size();
        float w = std::isfinite(m_constraints.max_width) ? std::min(ideal.x, m_constraints.max_width) : ideal.x;
        float h = std::isfinite(m_constraints.max_height) ? std::min(ideal.y, m_constraints.max_height) : ideal.y;
        set_size(Vec2{w, h});
    }

    void paint(PaintContext& ctx) override {
        if (m_open_progress < 1.0f) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - m_open_start).count();
            m_open_progress = std::clamp(elapsed / 0.13f, 0.0f, 1.0f);
            if (m_open_progress < 1.0f) mark_needs_paint();
        }

        const Theme& theme = ThemeContext::current();
        const auto style = NavigationViewStyleResolver::resolve(theme);

        float ease = 1.0f - std::pow(1.0f - m_open_progress, 3.0f);
        float anim_h = std::max(16.0f, size().y * (0.76f + 0.24f * ease));
        float alpha_mul = std::clamp(ease * 1.15f, 0.0f, 1.0f);

        Rect panel = Rect::from_xywh(0.0f, 0.0f, size().x, anim_h);

        // 1. WinUI 3 Flyout 32dp 双层真实高斯软阴影
        ctx.draw_elevation_shadow(panel, 8.0f, theme.is_dark, 32.0f, alpha_mul);

        // 2. 8px 圆角浮层底板与 1px 描边
        Color bg = style.pane_overlay_bg;
        bg.a *= alpha_mul;
        Color bd = style.pane_overlay_border;
        bd.a *= alpha_mul;
        ctx.draw_rect(panel, bg, 8.0f, 0.0f);
        ctx.draw_rect(panel, bd, 8.0f, 1.0f);

        // 3. 子项列表
        ctx.push_clip(Rect::from_xywh(4.0f, 4.0f, std::max(0.0f, size().x - 8.0f), std::max(0.0f, anim_h - 8.0f)), 4.0f);
        for (int i = 0; i < static_cast<int>(m_children.size()); ++i) {
            const auto& item = m_children[static_cast<size_t>(i)];
            float iy = 4.0f + static_cast<float>(i) * kItemH;
            Rect r = Rect::from_xywh(4.0f, iy + 1.0f, std::max(0.0f, size().x - 8.0f), kItemH - 2.0f);

            bool is_sel = (item.effective_id() == m_selected_id);
            bool is_hov = (i == m_hovered_index && !item.disabled);
            bool is_prs = (i == m_pressed_index && !item.disabled);

            if (is_prs) {
                Color c = style.item_pressed_bg; c.a *= alpha_mul;
                ctx.draw_rect(r, c, 4.0f, 0.0f);
            } else if (is_hov) {
                Color c = style.item_hover_bg; c.a *= alpha_mul;
                ctx.draw_rect(r, c, 4.0f, 0.0f);
            } else if (is_sel) {
                Color c = style.item_selected_bg; c.a *= alpha_mul;
                ctx.draw_rect(r, c, 4.0f, 0.0f);
            }

            if (is_sel) {
                Color pill = style.accent_pill; pill.a *= alpha_mul;
                ctx.draw_rect(Rect::from_xywh(5.0f, iy + (kItemH - 16.0f) * 0.5f, 3.0f, 16.0f), pill, 1.5f, 0.0f);
            }

            float tx = r.min_x + 12.0f;
            Color ic = item.disabled ? style.item_disabled_text : (is_sel ? style.icon_selected_color : style.icon_color);
            ic.a *= alpha_mul;
            if (item.icon != NavigationIcon::None || item.icon_painter) {
                Rect ir = Rect::from_xywh(tx, std::round(iy + (kItemH - 16.0f) * 0.5f), 16.0f, 16.0f);
                if (item.icon_painter) item.icon_painter(ctx, ir, ic);
                else NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, ir, ic);
                tx += 26.0f;
            }

            Color tc = item.disabled ? style.item_disabled_text : (is_sel ? style.item_selected_text : style.item_text);
            tc.a *= alpha_mul;
            Vec2 ts = TextMeasure::measure(item.label, 13.0f);
            ctx.draw_text(item.label, PointF{std::round(tx), std::round(iy + (kItemH - ts.y) * 0.5f)}, 13.0f, tc);
        }
        ctx.pop_clip();
    }

private:
    static constexpr float kItemH = 34.0f;

    int hit_index(float ly) const noexcept {
        float y = ly - 4.0f;
        if (y < 0.0f) return -1;
        int idx = static_cast<int>(std::floor(y / kItemH));
        if (idx < 0 || idx >= static_cast<int>(m_children.size())) return -1;
        return idx;
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
            int idx = hit_index(e.local_position.y);
            if (idx >= 0 && !m_children[static_cast<size_t>(idx)].disabled) {
                m_pressed_index = idx;
                mark_needs_paint();
            }
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& e) {
            int idx = hit_index(e.local_position.y);
            int pressed = m_pressed_index;
            m_pressed_index = -1;
            if (idx >= 0 && idx == pressed && !m_children[static_cast<size_t>(idx)].disabled) {
                if (m_on_select) m_on_select(m_children[static_cast<size_t>(idx)].effective_id());
            }
            mark_needs_paint();
            e.stop_propagation();
        };
    }

    std::vector<NavigationViewItem> m_children;
    std::string m_selected_id;
    std::function<void(const std::string&)> m_on_select;
    int m_hovered_index{-1};
    int m_pressed_index{-1};
    float m_open_progress{0.0f};
    std::chrono::steady_clock::time_point m_open_start;
};

// =============================================================================
// WinUI 3 NavigationView 核心渲染与交互盒
// =============================================================================
class RenderNavigationView : public RenderBox {
public:
    RenderNavigationView() noexcept {
        is_focusable = true;
        setup_behavior();
    }

    ~RenderNavigationView() override {
        close_submenu_flyout();
        if (m_search_box && m_search_box->parent() == this) {
            remove_child(m_search_box.get());
        }
    }

    void set_search_options(bool visible, std::string placeholder) {
        m_data.is_search_box_visible = visible;
        m_data.search_placeholder = std::move(placeholder);
        if (visible && !m_search_box) {
            m_search_box = std::make_shared<RenderAutoSuggestBox>();
            append_child(m_search_box.get());
        } else if (!visible && m_search_box) {
            if (m_search_box->parent() == this) remove_child(m_search_box.get());
            m_search_box.reset();
        }
        rebuild_search_items();
        mark_needs_layout();
    }

    [[nodiscard]] const std::string& selected_id() const noexcept { return m_data.selected_id; }
    [[nodiscard]] bool is_pane_open() const noexcept { return m_data.is_pane_open; }
    [[nodiscard]] NavigationPaneDisplayMode pane_display_mode() const noexcept { return m_data.pane_display_mode; }
    [[nodiscard]] NavigationPaneDisplayMode effective_display_mode() const noexcept { return compute_effective_mode(size().x); }
    [[nodiscard]] bool can_go_back() const noexcept { return m_data.is_back_enabled || m_history.size() > 1; }
    [[nodiscard]] const std::shared_ptr<RenderBox>& content_box() const noexcept { return m_content_box; }
    [[nodiscard]] bool is_submenu_open() const noexcept { return m_submenu_overlay_id != 0; }

    void set_menu_items(std::vector<NavigationViewItem> items) {
        m_data.menu_items = std::move(items);
        if (m_data.selected_id.empty()) {
            for (const auto& it : m_data.menu_items) {
                if (it.kind == NavigationItemKind::Item && !it.disabled) {
                    m_data.selected_id = it.effective_id();
                    m_history.clear();
                    m_history.push_back(m_data.selected_id);
                    break;
                }
            }
        }
        rebuild_flat_items();
        rebuild_search_items();
        mark_needs_layout();
    }

    void set_footer_items(std::vector<NavigationViewItem> items) {
        m_data.footer_items = std::move(items);
        rebuild_flat_items();
        rebuild_search_items();
        mark_needs_layout();
    }

    void set_selected_id(const std::string& id, bool notify = true, bool record_history = true) {
        if (id.empty() || m_data.selected_id == id) return;
        m_data.selected_id = id;
        if (record_history) {
            if (m_history.empty() || m_history.back() != id) {
                m_history.push_back(id);
                if (m_history.size() > 32) m_history.erase(m_history.begin());
            }
        }
        m_page_enter_progress = 0.0f;
        m_last_anim_time = std::chrono::steady_clock::now();

        if (m_page_builder) {
            auto w = m_page_builder(id);
            if (w) {
                set_content_widget(std::move(w));
            }
        }
        if (notify && m_data.on_selection_changed) {
            m_data.on_selection_changed(id);
        }
        mark_needs_paint();
    }

    bool go_back() {
        if (m_data.on_back_requested) {
            m_data.on_back_requested();
            return true;
        }
        if (m_history.size() > 1) {
            m_history.pop_back();
            std::string prev = m_history.back();
            set_selected_id(prev, true, false);
            return true;
        }
        return false;
    }

    void set_pane_display_mode(NavigationPaneDisplayMode mode) {
        if (m_data.pane_display_mode != mode) {
            close_submenu_flyout();
            m_data.pane_display_mode = mode;
            m_last_auto_mode = NavigationPaneDisplayMode::Auto;
            if (mode == NavigationPaneDisplayMode::LeftCompact || mode == NavigationPaneDisplayMode::LeftMinimal) {
                m_data.is_pane_open = false;
                m_pane_open_anim = 0.0f;
            } else if (mode == NavigationPaneDisplayMode::Left) {
                m_data.is_pane_open = true;
                m_pane_open_anim = 1.0f;
            }
            m_pill_initialized = false;
            mark_needs_layout();
            mark_needs_paint();
        }
    }

    void set_pane_open(bool open, bool notify = false) {
        if (m_data.is_pane_open != open) {
            close_submenu_flyout();
            if (!open) {
                m_saved_expanded_scroll_y = m_menu_scroll.offset();
                m_restore_expanded_scroll = false;
            } else {
                m_restore_expanded_scroll = true;
            }
            m_data.is_pane_open = open;
            m_last_anim_time = std::chrono::steady_clock::now();
            if (notify && m_data.on_pane_toggled) {
                m_data.on_pane_toggled(open);
            }
            mark_needs_layout();
        }
    }

    void toggle_pane() {
        set_pane_open(!m_data.is_pane_open, true);
    }

    void set_chrome_options(bool back_visible, bool back_enabled, bool toggle_visible,
                            bool settings_visible, std::string settings_label,
                            std::string pane_title, std::string header,
                            float open_len, float compact_len,
                            float compact_thresh, float expanded_thresh) {
        m_data.is_back_button_visible = back_visible;
        m_data.is_back_enabled = back_enabled;
        m_data.is_pane_toggle_button_visible = toggle_visible;
        m_data.is_settings_visible = settings_visible;
        m_data.settings_label = std::move(settings_label);
        m_data.pane_title = std::move(pane_title);
        m_data.header = std::move(header);
        if (open_len > 100.0f) m_data.open_pane_length = open_len;
        if (compact_len >= 36.0f) m_data.compact_pane_length = compact_len;
        if (compact_thresh > 0.0f) m_data.compact_mode_threshold_width = compact_thresh;
        if (expanded_thresh > 0.0f) m_data.expanded_mode_threshold_width = expanded_thresh;
        rebuild_flat_items();
        rebuild_search_items();
        mark_needs_layout();
    }

    void set_callbacks(std::function<void(const std::string&)> on_sel,
                       std::function<void()> on_back,
                       std::function<void(bool)> on_toggle) {
        m_data.on_selection_changed = std::move(on_sel);
        m_data.on_back_requested = std::move(on_back);
        m_data.on_pane_toggled = std::move(on_toggle);
    }

    void set_page_builder(std::function<WidgetPtr(const std::string&)> builder) {
        m_page_builder = std::move(builder);
        if (m_page_builder && !m_data.selected_id.empty()) {
            auto w = m_page_builder(m_data.selected_id);
            if (w) set_content_widget(std::move(w));
        }
    }

    void set_content_widget(WidgetPtr widget) {
        m_content_widget = std::move(widget);
        if (m_content_widget) {
            set_content_box(m_content_widget->create_render_node());
        } else {
            set_content_box(nullptr);
        }
    }

    void set_content_box(std::shared_ptr<RenderBox> box) {
        if (m_content_box == box) return;
        if (m_content_box) {
            remove_child(m_content_box.get());
        }
        m_content_box = std::move(box);
        if (m_content_box) {
            append_child(m_content_box.get());
        }
        mark_needs_layout();
    }

    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 800.0f;
        float h = std::isfinite(m_constraints.max_height) ? m_constraints.max_height : 600.0f;
        set_size(Vec2{w, h});

        NavigationPaneDisplayMode eff = compute_effective_mode(w);
        if (m_data.pane_display_mode == NavigationPaneDisplayMode::Auto && eff != m_last_auto_mode) {
            m_last_auto_mode = eff;
            bool should_open = (eff == NavigationPaneDisplayMode::Left);
            m_data.is_pane_open = should_open;
            m_pane_open_anim = should_open ? 1.0f : 0.0f;
        }

        if (eff != NavigationPaneDisplayMode::Top) {
            float pw = current_pane_width(eff);
            if (m_search_box && m_data.is_search_box_visible) {
                float sy = compute_left_search_top();
                m_search_box->layout(BoxConstraints::tight(Vec2{std::max(60.0f, pw - 20.0f), 32.0f}), false);
                m_search_box->set_offset(Vec2{10.0f, sy});
            }
            Rect menu_vp = compute_left_menu_viewport(h, pw);
            float total_menu_h = compute_left_menu_total_height();
            m_menu_scroll.set_max_scroll(std::max(0.0f, total_menu_h - menu_vp.height()));
        }

        Rect content_rect = compute_content_body_rect(w, h, eff);
        if (m_content_box) {
            BoxConstraints cc = BoxConstraints::tight(Vec2{
                std::max(0.0f, content_rect.width()),
                std::max(0.0f, content_rect.height())
            });
            m_content_box->layout(cc);
            m_content_box->set_offset(Vec2{content_rect.min_x, content_rect.min_y});
        }
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (local_position.x < 0.0f || local_position.y < 0.0f ||
            local_position.x > size().x || local_position.y > size().y) {
            return false;
        }

        const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);
        const bool overlay_active = is_overlay_pane(eff) && m_pane_open_anim > 0.05f;
        const float pane_w = current_pane_width(eff);

        if (eff == NavigationPaneDisplayMode::Top) {
            if (local_position.y <= 44.0f) {
                result.add(this, local_position);
                return true;
            }
        } else {
            if (m_search_box && m_data.is_search_box_visible && m_pane_open_anim > 0.5f && local_position.x <= pane_w) {
                Vec2 soff = m_search_box->offset();
                PointF spos{local_position.x - soff.x, local_position.y - soff.y};
                if (m_search_box->hit_test(result, spos)) {
                    return true;
                }
            }
            if (local_position.x <= pane_w || overlay_active) {
                result.add(this, local_position);
                return true;
            }
            if (eff == NavigationPaneDisplayMode::LeftMinimal && local_position.x <= 88.0f && local_position.y <= 44.0f) {
                result.add(this, local_position);
                return true;
            }
        }

        if (m_content_box) {
            Vec2 off = m_content_box->offset();
            PointF child_pos{local_position.x - off.x, local_position.y - off.y};
            if (m_content_box->hit_test(result, child_pos)) {
                return true;
            }
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        update_animations();

        const Theme& theme = ThemeContext::current();
        const ResolvedNavigationViewStyle style = NavigationViewStyleResolver::resolve(theme);
        const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

        // 1. 内容区卡片表面与页面内容
        paint_content_area(ctx, style, eff);

        // 2. 导航栏 (盖在内容区上方)
        if (eff == NavigationPaneDisplayMode::Top) {
            paint_top_nav(ctx, style);
        } else {
            paint_left_pane(ctx, style, eff);
        }
    }

private:
    struct FlatRow {
        const NavigationViewItem* item{nullptr};
        int depth{0};
        bool is_footer{false};
        bool is_settings{false};
        float height{36.0f};
    };

    enum class ChromeButton {
        None,
        Back,
        Toggle
    };

    [[nodiscard]] NavigationPaneDisplayMode compute_effective_mode(float total_w) const noexcept {
        if (m_data.pane_display_mode != NavigationPaneDisplayMode::Auto) {
            return m_data.pane_display_mode;
        }
        if (total_w >= m_data.expanded_mode_threshold_width) {
            return NavigationPaneDisplayMode::Left;
        }
        if (total_w >= m_data.compact_mode_threshold_width) {
            return NavigationPaneDisplayMode::LeftCompact;
        }
        return NavigationPaneDisplayMode::LeftMinimal;
    }

    [[nodiscard]] bool is_overlay_pane(NavigationPaneDisplayMode eff) const noexcept {
        return eff == NavigationPaneDisplayMode::LeftCompact ||
               eff == NavigationPaneDisplayMode::LeftMinimal;
    }

    [[nodiscard]] float current_pane_width(NavigationPaneDisplayMode eff) const noexcept {
        if (eff == NavigationPaneDisplayMode::Top) return 0.0f;
        float closed_w = (eff == NavigationPaneDisplayMode::LeftMinimal) ? 0.0f : m_data.compact_pane_length;
        return closed_w + (m_data.open_pane_length - closed_w) * m_pane_open_anim;
    }

    [[nodiscard]] float inline_reserved_left(NavigationPaneDisplayMode eff) const noexcept {
        if (eff == NavigationPaneDisplayMode::Top || eff == NavigationPaneDisplayMode::LeftMinimal) return 0.0f;
        if (eff == NavigationPaneDisplayMode::LeftCompact) return m_data.compact_pane_length;
        return current_pane_width(eff);
    }

    [[nodiscard]] Rect compute_content_surface_rect(float w, float h, NavigationPaneDisplayMode eff) const noexcept {
        if (eff == NavigationPaneDisplayMode::Top) {
            return Rect::from_xywh(0.0f, 44.0f, w, std::max(0.0f, h - 44.0f));
        }
        float left = inline_reserved_left(eff);
        float top = (eff == NavigationPaneDisplayMode::LeftMinimal) ? 44.0f : 4.0f;
        return Rect::from_xywh(left, top, std::max(0.0f, w - left), std::max(0.0f, h - top));
    }

    [[nodiscard]] Rect compute_content_body_rect(float w, float h, NavigationPaneDisplayMode eff) const noexcept {
        Rect surf = compute_content_surface_rect(w, h, eff);
        float header_h = m_data.header.empty() ? 0.0f : 48.0f;
        return Rect::from_xywh(surf.min_x, surf.min_y + header_h,
                               surf.width(), std::max(0.0f, surf.height() - header_h));
    }

    // 检查某导航项或其任意子项是否处于选中态 (折叠态父项继承选中高亮与 3px 强调条)
    [[nodiscard]] bool is_item_or_child_selected(const NavigationViewItem& item, bool children_visible) const noexcept {
        if (item.kind != NavigationItemKind::Item) return false;
        if (item.effective_id() == m_data.selected_id) return true;
        if (!children_visible && !item.children.empty()) {
            for (const auto& c : item.children) {
                if (c.effective_id() == m_data.selected_id) return true;
            }
        }
        return false;
    }

    void rebuild_flat_items() {
        m_main_rows.clear();
        m_footer_rows.clear();
        m_settings_item = NavigationViewItem{};
        m_settings_item.id = m_data.settings_id;
        m_settings_item.label = m_data.settings_label;
        m_settings_item.icon = NavigationIcon::Settings;

        for (const auto& item : m_data.menu_items) {
            float rh = (item.kind == NavigationItemKind::Separator) ? 13.0f
                     : (item.kind == NavigationItemKind::Header ? 30.0f : 36.0f);
            m_main_rows.push_back(FlatRow{&item, 0, false, false, rh});
            if (item.kind == NavigationItemKind::Item && !item.children.empty() && item.expanded) {
                for (const auto& child : item.children) {
                    m_main_rows.push_back(FlatRow{&child, 1, false, false, 34.0f});
                }
            }
        }

        for (const auto& item : m_data.footer_items) {
            float rh = (item.kind == NavigationItemKind::Separator) ? 13.0f : 36.0f;
            m_footer_rows.push_back(FlatRow{&item, 0, true, false, rh});
        }
        if (m_data.is_settings_visible) {
            m_footer_rows.push_back(FlatRow{&m_settings_item, 0, true, true, 36.0f});
        }
    }

    [[nodiscard]] bool has_left_top_row() const noexcept {
        return m_data.is_back_button_visible || m_data.is_pane_toggle_button_visible || !m_data.pane_title.empty();
    }

    [[nodiscard]] float compute_left_search_top() const noexcept {
        return has_left_top_row() ? 46.0f : 10.0f;
    }

    // 紧凑单行顶部操作栏: Back + Hamburger + PaneTitle 合并为同一行 (44px)；
    // 侧边折叠后搜索行平滑收起至 0px，不占用空间
    [[nodiscard]] float compute_left_chrome_height() const noexcept {
        float base = has_left_top_row() ? 44.0f : 8.0f;
        if (m_data.is_search_box_visible) {
            base += 44.0f * m_pane_open_anim;
        }
        return base;
    }

    void collect_searchable(const std::vector<NavigationViewItem>& items,
                            std::vector<std::string>& labels,
                            std::vector<std::pair<std::string, std::string>>& pairs) const {
        for (const auto& it : items) {
            if (it.kind == NavigationItemKind::Item && !it.disabled) {
                if (it.children.empty()) {
                    labels.push_back(it.label);
                    pairs.emplace_back(it.label, it.effective_id());
                } else {
                    collect_searchable(it.children, labels, pairs);
                }
            }
        }
    }

    void rebuild_search_items() {
        if (!m_search_box || !m_data.is_search_box_visible) return;
        std::vector<std::string> labels;
        m_search_map.clear();
        collect_searchable(m_data.menu_items, labels, m_search_map);
        collect_searchable(m_data.footer_items, labels, m_search_map);

        auto navigate_by_label = [this](const std::string& chosen) {
            std::string target_id;
            for (const auto& kv : m_search_map) {
                if (kv.first == chosen) { target_id = kv.second; break; }
            }
            if (target_id.empty() && !chosen.empty()) {
                // 子串回退匹配首项
                std::string q = chosen;
                for (char& c : q) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                for (const auto& kv : m_search_map) {
                    std::string low = kv.first;
                    for (char& c : low) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                    if (low.find(q) != std::string::npos) { target_id = kv.second; break; }
                }
            }
            if (!target_id.empty()) {
                set_selected_id(target_id, true, true);
                if (m_search_box) m_search_box->set_text_value("");
            }
        };

        m_search_box->set_properties(
            m_search_box->text(),
            m_data.search_placeholder,
            "",
            std::move(labels),
            std::max(80.0f, m_data.open_pane_length - 20.0f),
            false,
            nullptr,
            navigate_by_label,
            navigate_by_label);
    }

    [[nodiscard]] float compute_left_footer_height() const noexcept {
        if (m_footer_rows.empty()) return 8.0f;
        float h = 13.0f;
        for (const auto& r : m_footer_rows) h += r.height + 2.0f;
        return h + 6.0f;
    }

    [[nodiscard]] Rect compute_left_menu_viewport(float total_h, float pane_w) const noexcept {
        float top = compute_left_chrome_height();
        float bottom = std::max(top + 20.0f, total_h - compute_left_footer_height());
        return Rect::from_xywh(0.0f, top, std::max(0.0f, pane_w), std::max(0.0f, bottom - top));
    }

    [[nodiscard]] float compute_left_menu_total_height() const noexcept {
        float y = 2.0f;
        const bool compact_hide = (m_pane_open_anim < 0.35f);
        for (const auto& r : m_main_rows) {
            if (compact_hide && (r.depth > 0 || (r.item && r.item->kind == NavigationItemKind::Header))) {
                continue;
            }
            y += r.height + 2.0f;
        }
        return y + 4.0f;
    }

    void close_submenu_flyout() {
        if (m_submenu_overlay_id != 0) {
            uint64_t id = m_submenu_overlay_id;
            m_submenu_overlay_id = 0;
            m_submenu_parent_id.clear();
            if (Window* win = Window::active_window()) {
                win->hide_overlay(id);
            }
        }
    }

    void open_submenu_flyout(const NavigationViewItem& parent, PointF anchor_local, bool below_anchor) {
        if (parent.children.empty()) return;
        std::string pid = parent.effective_id();
        if (m_submenu_overlay_id != 0 && m_submenu_parent_id == pid) {
            close_submenu_flyout();
            return;
        }
        close_submenu_flyout();

        Window* win = Window::active_window();
        if (!win) return;

        auto flyout = std::make_shared<RenderNavigationSubmenuFlyout>(
            parent.children,
            m_data.selected_id,
            [this](const std::string& chosen_id) {
                set_selected_id(chosen_id, true, true);
                close_submenu_flyout();
            });

        Vec2 fsz = flyout->compute_ideal_size();
        Vec2 base = global_offset();
        base.y += win->titlebar_height();
        Vec2 win_sz = win->size();

        float gx = base.x + anchor_local.x;
        float gy = base.y + anchor_local.y;
        if (!below_anchor) {
            // Compact 模式: 弹在 48px 图标栏右侧
            gx = std::clamp(gx, 4.0f, std::max(4.0f, win_sz.x - fsz.x - 4.0f));
            gy = std::clamp(gy, win->titlebar_height() + 4.0f, std::max(win->titlebar_height() + 4.0f, win_sz.y - fsz.y - 4.0f));
        } else {
            // Top 模式: 弹在顶部导航项正下方
            gx = std::clamp(gx, 4.0f, std::max(4.0f, win_sz.x - fsz.x - 4.0f));
            gy = std::clamp(gy, win->titlebar_height() + 4.0f, std::max(win->titlebar_height() + 4.0f, win_sz.y - fsz.y - 4.0f));
        }

        OverlayEntry entry;
        entry.content = flyout;
        entry.global_pos = Vec2{gx, gy};
        entry.size = fsz;
        entry.dismiss_on_outside_click = true;
        entry.is_modal = false;
        entry.on_dismiss = [this]() {
            m_submenu_overlay_id = 0;
            m_submenu_parent_id.clear();
            m_last_submenu_dismiss = std::chrono::steady_clock::now();
            mark_needs_paint();
        };

        m_submenu_parent_id = pid;
        m_submenu_overlay_id = win->show_overlay(std::move(entry));
    }

    void paint_content_area(PaintContext& ctx, const ResolvedNavigationViewStyle& style, NavigationPaneDisplayMode eff) {
        Rect surf = compute_content_surface_rect(size().x, size().y, eff);
        float tl_rad = (eff == NavigationPaneDisplayMode::Top || eff == NavigationPaneDisplayMode::LeftMinimal)
            ? 0.0f : style.content_radius;
        ctx.draw_rounded_rect(surf, style.content_surface_bg, tl_rad, 0.0f, 0.0f, 0.0f);

        if (style.content_surface_border.a > 0.001f) {
            if (tl_rad > 0.0f) {
                // 使用带左上角独立圆角的 1px 完整描边，右/下侧外扩 4px 并裁剪到 surf，
                // 让 1px 边框顺着左上圆角弧线连续无缝绘制，绝不在圆角处截断
                ctx.push_clip(surf, 0.0f);
                ctx.draw_rounded_rect(
                    Rect::from_xywh(surf.min_x, surf.min_y, surf.width() + 4.0f, surf.height() + 4.0f),
                    style.content_surface_border,
                    tl_rad, 0.0f, 0.0f, 0.0f,
                    0.0f, Vec2{0.0f, 0.0f}, 1.0f);
                ctx.pop_clip();
            } else {
                ctx.draw_rect(Rect::from_xywh(surf.min_x, surf.min_y, surf.width(), 1.0f),
                              style.content_surface_border, 0.0f, 0.0f);
            }
        }

        if (!m_data.header.empty()) {
            ctx.draw_text(m_data.header, PointF{std::round(surf.min_x + 24.0f), std::round(surf.min_y + 14.0f)},
                          style.page_header_font_size, style.page_header_text, FontWeight::SemiBold);
        }

        if (m_content_box) {
            Rect body = compute_content_body_rect(size().x, size().y, eff);
            float ease = 1.0f - std::pow(1.0f - m_page_enter_progress, 3.0f);
            float slide_y = std::round((1.0f - ease) * 10.0f);

            ctx.push_clip(body, 0.0f);
            PaintContext::OffsetScope scope(ctx, Vec2{m_content_box->offset().x, m_content_box->offset().y + slide_y});
            m_content_box->paint(ctx);
            ctx.pop_clip();
        }
    }

    void paint_chrome_button(PaintContext& ctx, Rect r, ChromeButton btn,
                             const ResolvedNavigationViewStyle& style, bool enabled) const {
        bool hov = (m_hovered_chrome == btn && enabled);
        bool prs = (m_pressed_chrome == btn && enabled);
        if (prs) {
            ctx.draw_rect(r, style.item_pressed_bg, style.item_radius, 0.0f);
        } else if (hov) {
            ctx.draw_rect(r, style.item_hover_bg, style.item_radius, 0.0f);
        }
        Color col = enabled ? style.icon_color : style.item_disabled_text;
        float cx = std::round(r.min_x + r.width() * 0.5f);
        float cy = std::round(r.min_y + r.height() * 0.5f);

        if (btn == ChromeButton::Back) {
            // WinUI 3 AnimatedBackVisualSource: 按下轻推向左 -2px
            float ox = prs ? -2.0f : 0.0f;
            ctx.draw_line(PointF{cx - 5.5f + ox, cy}, PointF{cx + 5.5f + ox, cy}, col, 1.3f);
            ctx.draw_line(PointF{cx - 5.5f + ox, cy}, PointF{cx - 1.2f + ox, cy - 4.2f}, col, 1.3f);
            ctx.draw_line(PointF{cx - 5.5f + ox, cy}, PointF{cx - 1.2f + ox, cy + 4.2f}, col, 1.3f);
        } else if (btn == ChromeButton::Toggle) {
            // WinUI 3 AnimatedGlobalNavigationButtonVisualSource: 按下微缩收紧
            float hw = prs ? 4.8f : 6.0f;
            ctx.draw_line(PointF{cx - hw, cy - 4.0f}, PointF{cx + hw, cy - 4.0f}, col, 1.25f);
            ctx.draw_line(PointF{cx - hw, cy},        PointF{cx + hw, cy},        col, 1.25f);
            ctx.draw_line(PointF{cx - hw, cy + 4.0f}, PointF{cx + hw, cy + 4.0f}, col, 1.25f);
        }
    }

    void paint_compact_tooltip(PaintContext& ctx, float pane_w, float item_cy,
                               const std::string& label, const ResolvedNavigationViewStyle& style) const {
        if (label.empty()) return;
        Vec2 ts = TextMeasure::measure(label, 12.0f);
        float tw = std::ceil(ts.x + 18.0f);
        float th = 28.0f;
        float tx = pane_w + 6.0f;
        float ty = std::round(item_cy - th * 0.5f);
        Rect tr = Rect::from_xywh(tx, ty, tw, th);

        const bool is_dark = ThemeContext::current().is_dark;
        ctx.draw_elevation_shadow(tr, 4.0f, is_dark, 16.0f, 1.0f);
        ctx.draw_rect(tr, style.pane_overlay_bg, 4.0f, 0.0f);
        ctx.draw_rect(tr, style.pane_overlay_border, 4.0f, 1.0f);
        ctx.draw_text(label, PointF{std::round(tx + 9.0f), std::round(ty + (th - ts.y) * 0.5f)}, 12.0f, style.item_selected_text);
    }

    void paint_left_pane(PaintContext& ctx, const ResolvedNavigationViewStyle& style, NavigationPaneDisplayMode eff) {
        const float pane_w = current_pane_width(eff);
        const bool overlay = is_overlay_pane(eff) && m_pane_open_anim > 0.02f;

        if (eff == NavigationPaneDisplayMode::LeftMinimal && m_pane_open_anim <= 0.02f) {
            float bx = 4.0f;
            if (m_data.is_back_button_visible) {
                paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 40.0f, 36.0f), ChromeButton::Back, style, can_go_back());
                bx += 42.0f;
            }
            if (m_data.is_pane_toggle_button_visible) {
                paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 40.0f, 36.0f), ChromeButton::Toggle, style, true);
            }
            return;
        }

        Rect pane_rect = Rect::from_xywh(0.0f, 0.0f, pane_w, size().y);

        if (overlay) {
            const bool is_dark = ThemeContext::current().is_dark;
            ctx.draw_elevation_shadow(pane_rect, 8.0f, is_dark, 48.0f, m_pane_open_anim, Vec2{1.0f, 0.0f});
            ctx.draw_rounded_rect(pane_rect, style.pane_overlay_bg, 0.0f, 8.0f, 8.0f, 0.0f);
            ctx.draw_rect(Rect::from_xywh(pane_w - 1.0f, 0.0f, 1.0f, size().y), style.pane_overlay_border, 0.0f, 0.0f);
        }

        // 1. 紧凑单行顶部操作区: Back + Hamburger + PaneTitle 合并为同一行 (y = 4, h = 36)
        //    折叠态仅保留单个 40px 按钮 (优先 Toggle)，展开态 Back(4..40) + Toggle(42..78) + Title(84..)
        {
            const float cy = 4.0f;
            float title_x = 12.0f;
            if (m_data.is_back_button_visible && m_data.is_pane_toggle_button_visible) {
                if (m_pane_open_anim > 0.35f) {
                    paint_chrome_button(ctx, Rect::from_xywh(4.0f, cy, 36.0f, 36.0f), ChromeButton::Back, style, can_go_back());
                }
                float tx = 4.0f + 38.0f * m_pane_open_anim;
                float tw = 40.0f - 4.0f * m_pane_open_anim;
                paint_chrome_button(ctx, Rect::from_xywh(tx, cy, tw, 36.0f), ChromeButton::Toggle, style, true);
                title_x = 84.0f;
            } else if (m_data.is_back_button_visible) {
                paint_chrome_button(ctx, Rect::from_xywh(4.0f, cy, 40.0f, 36.0f), ChromeButton::Back, style, can_go_back());
                title_x = 48.0f;
            } else if (m_data.is_pane_toggle_button_visible) {
                paint_chrome_button(ctx, Rect::from_xywh(4.0f, cy, 40.0f, 36.0f), ChromeButton::Toggle, style, true);
                title_x = 48.0f;
            }

            if (!m_data.pane_title.empty() && m_pane_open_anim > 0.45f) {
                float ta = std::clamp((m_pane_open_anim - 0.45f) / 0.55f, 0.0f, 1.0f);
                Color tc = style.item_selected_text; tc.a *= ta;
                Vec2 ts = TextMeasure::measure(m_data.pane_title, 13.5f, FontWeight::SemiBold);
                float clip_w = std::max(0.0f, pane_w - title_x - 8.0f);
                if (clip_w > 2.0f) {
                    ctx.push_clip(Rect::from_xywh(title_x, cy, clip_w, 36.0f), 0.0f);
                    ctx.draw_text(m_data.pane_title, PointF{title_x, std::round(cy + (36.0f - ts.y) * 0.5f)},
                                  13.5f, tc, FontWeight::SemiBold);
                    ctx.pop_clip();
                }
            }
        }

        // 1.5 内置左侧搜索框 (仅在展开态渲染；侧边折叠后自动隐藏且不占任何高度空间)
        if (m_data.is_search_box_visible && m_pane_open_anim > 0.55f && m_search_box) {
            float sy = compute_left_search_top();
            float sw = std::max(80.0f, pane_w - 20.0f);
            m_search_box->layout(BoxConstraints::tight(Vec2{sw, 32.0f}), false);
            m_search_box->set_offset(Vec2{10.0f, sy});
            PaintContext::OffsetScope scope(ctx, m_search_box->offset());
            m_search_box->paint(ctx);
        }

        // 2. 可滚动主菜单区域
        Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
        const float total_menu_h = compute_left_menu_total_height();
        m_menu_scroll.set_max_scroll(std::max(0.0f, total_menu_h - menu_vp.height()));
        if (m_restore_expanded_scroll && m_pane_open_anim >= 0.35f) {
            m_menu_scroll.jump_to(m_saved_expanded_scroll_y);
            m_pill_last_scroll_y = m_menu_scroll.offset();
            m_pill_initialized = false;
            m_restore_expanded_scroll = false;
        } else if (m_data.is_pane_open && m_pane_open_anim >= 0.99f) {
            m_saved_expanded_scroll_y = m_menu_scroll.offset();
        }
        const float scroll_y = m_menu_scroll.offset();
        const float label_alpha = std::clamp((m_pane_open_anim - 0.35f) / 0.65f, 0.0f, 1.0f);
        const bool is_compact = (m_pane_open_anim < 0.25f);

        // 同步菜单顶部偏移变化 (如搜索行展开/收起)，防止小蓝条出现滞后拖尾
        if (m_pill_initialized && m_pill_in_menu) {
            float dtop = menu_vp.min_y - m_pill_last_menu_top;
            if (std::abs(dtop) > 0.001f) {
                m_pill_head += dtop;
                m_pill_tail += dtop;
                m_pill_target_pos += dtop;
            }
        }
        m_pill_last_menu_top = menu_vp.min_y;

        float target_pill_y = -1.0f;
        bool target_pill_in_menu = false;
        std::string tooltip_label;
        float tooltip_cy = 0.0f;

        ctx.push_clip(menu_vp, 0.0f);
        float y = menu_vp.min_y + 2.0f - scroll_y;
        for (int i = 0; i < static_cast<int>(m_main_rows.size()); ++i) {
            const auto& row = m_main_rows[static_cast<size_t>(i)];
            if (!row.item) continue;
            if (label_alpha <= 0.01f && (row.item->kind == NavigationItemKind::Header || row.depth > 0)) {
                continue;
            }

            bool children_vis = (label_alpha > 0.01f && row.item->expanded);
            bool is_sel = is_item_or_child_selected(*row.item, children_vis);
            if (is_sel) {
                target_pill_y = y + (row.height - 16.0f) * 0.5f;
                target_pill_in_menu = true;
            }

            Rect row_rect = Rect::from_xywh(4.0f, y, std::max(0.0f, pane_w - 8.0f), row.height);
            if (y + row.height >= menu_vp.min_y && y <= menu_vp.max_y) {
                bool is_hov = (m_hovered_row == i && !m_hovered_in_footer);
                bool is_prs = (m_pressed_row == i && !m_pressed_in_footer);
                if (is_compact && is_hov && row.item->kind == NavigationItemKind::Item && m_submenu_overlay_id == 0) {
                    tooltip_label = row.item->label;
                    tooltip_cy = y + row.height * 0.5f;
                }
                paint_left_row(ctx, row_rect, row, style, is_sel, is_hov, is_prs, label_alpha);
            }
            y += row.height + 2.0f;
        }
        m_menu_scrollbar.paint(ctx, menu_vp, total_menu_h, scroll_y, ThemeContext::current());
        ctx.pop_clip();

        // 3. 底部固定 Footer 区域 (分割线 + footer_items + Settings 旋转齿轮)
        if (!m_footer_rows.empty()) {
            float fy = menu_vp.max_y + 4.0f;
            ctx.draw_rect(Rect::from_xywh(12.0f, fy, std::max(0.0f, pane_w - 24.0f), 1.0f),
                          style.separator_color, 0.0f, 0.0f);
            fy += 7.0f;

            for (int i = 0; i < static_cast<int>(m_footer_rows.size()); ++i) {
                const auto& row = m_footer_rows[static_cast<size_t>(i)];
                if (!row.item) continue;
                Rect row_rect = Rect::from_xywh(4.0f, fy, std::max(0.0f, pane_w - 8.0f), row.height);
                bool is_sel = (row.item->kind == NavigationItemKind::Item &&
                               row.item->effective_id() == m_data.selected_id);
                bool is_hov = (m_hovered_row == i && m_hovered_in_footer);
                bool is_prs = (m_pressed_row == i && m_pressed_in_footer);
                if (is_sel) {
                    target_pill_y = fy + (row.height - 16.0f) * 0.5f;
                    target_pill_in_menu = false;
                }
                if (is_compact && is_hov && row.item->kind == NavigationItemKind::Item && m_submenu_overlay_id == 0) {
                    tooltip_label = row.item->label;
                    tooltip_cy = fy + row.height * 0.5f;
                }
                paint_left_row(ctx, row_rect, row, style, is_sel, is_hov, is_prs, label_alpha);
                fy += row.height + 2.0f;
            }
        }

        // 4. WinUI 3 弹性拉伸滑动强调条: 切换选中项时弹性动画，滚动列表时严格同步不拖尾
        if (target_pill_y >= -2000.0f) {
            if (!m_pill_initialized) {
                m_pill_head = m_pill_tail = target_pill_y;
                m_pill_size = 16.0f;
                m_pill_initialized = true;
            }
            m_pill_in_menu = target_pill_in_menu;
            m_pill_target_pos = target_pill_y;
            m_pill_target_size = 16.0f;
            if (std::abs(m_pill_head - m_pill_target_pos) > 0.25f ||
                std::abs(m_pill_tail - m_pill_target_pos) > 0.25f ||
                std::abs(m_pill_size - m_pill_target_size) > 0.25f) {
                mark_needs_paint();
            }
            float pill_top = std::min(m_pill_head, m_pill_tail);
            float pill_bot = std::max(m_pill_head, m_pill_tail) + m_pill_size;
            if (target_pill_in_menu) {
                ctx.push_clip(menu_vp, 0.0f);
                ctx.draw_rect(Rect::from_xywh(4.0f, pill_top, 3.0f, std::max(16.0f, pill_bot - pill_top)),
                              style.accent_pill, 1.5f, 0.0f);
                ctx.pop_clip();
            } else {
                ctx.draw_rect(Rect::from_xywh(4.0f, pill_top, 3.0f, std::max(16.0f, pill_bot - pill_top)),
                              style.accent_pill, 1.5f, 0.0f);
            }
        }

        // 5. Compact 模式悬停 Tooltip (悬浮在内容区左侧上方)
        if (!tooltip_label.empty()) {
            paint_compact_tooltip(ctx, pane_w, tooltip_cy, tooltip_label, style);
        }
    }

    void paint_left_row(PaintContext& ctx, Rect r, const FlatRow& row,
                        const ResolvedNavigationViewStyle& style,
                        bool is_selected, bool is_hov, bool is_prs, float label_alpha) const {
        const auto& item = *row.item;
        if (item.kind == NavigationItemKind::Separator) {
            ctx.draw_rect(Rect::from_xywh(r.min_x + 8.0f, std::round(r.min_y + r.height() * 0.5f),
                                          std::max(0.0f, r.width() - 16.0f), 1.0f),
                          style.separator_color, 0.0f, 0.0f);
            return;
        }
        if (item.kind == NavigationItemKind::Header) {
            Color hc = style.header_text;
            hc.a *= label_alpha;
            Vec2 ts = TextMeasure::measure(item.label, style.header_font_size);
            ctx.draw_text(item.label, PointF{r.min_x + 12.0f, std::round(r.min_y + (r.height() - ts.y) * 0.5f + 3.0f)},
                          style.header_font_size, hc);
            return;
        }

        if (!item.disabled) {
            if (is_prs) ctx.draw_rect(r, style.item_pressed_bg, style.item_radius, 0.0f);
            else if (is_hov) ctx.draw_rect(r, style.item_hover_bg, style.item_radius, 0.0f);
            else if (is_selected) ctx.draw_rect(r, style.item_selected_bg, style.item_radius, 0.0f);
        }

        const float indent = static_cast<float>(row.depth) * 24.0f;
        const float icon_cx = std::round(r.min_x + 20.0f + indent);
        const float icon_cy = std::round(r.min_y + r.height() * 0.5f);
        Rect icon_rect = Rect::from_xywh(icon_cx - 8.0f, icon_cy - 8.0f, 16.0f, 16.0f);
        Color ic = item.disabled ? style.item_disabled_text
                 : (is_selected ? style.icon_selected_color : style.icon_color);

        if (row.is_settings) {
            // WinUI 3 AnimatedSettingsVisualSource: 悬停微转 + 点击顺滑旋转 90° + 按下微缩
            float gear_scale = is_prs ? 0.90f : 1.0f;
            NavigationViewStyleResolver::paint_settings_gear(ctx, icon_rect, ic, m_settings_gear_angle, gear_scale);
        } else if (item.icon_painter) {
            item.icon_painter(ctx, icon_rect, ic);
        } else if (item.icon != NavigationIcon::None) {
            NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, icon_rect, ic);
        }

        // Compact 模式下: 有子项时在右下角绘制精致小指示箭头，有徽标时在右上角绘制小圆点
        if (label_alpha < 0.3f) {
            if (!item.badge.empty()) {
                ctx.draw_rect(Rect::from_xywh(icon_cx + 4.0f, icon_cy - 8.0f, 6.0f, 6.0f),
                              style.badge_bg, 3.0f, 0.0f);
            }
            if (!item.children.empty()) {
                Color dot_c = ic; dot_c.a *= 0.6f;
                ctx.draw_line(PointF{r.max_x - 6.0f, icon_cy - 2.0f}, PointF{r.max_x - 4.0f, icon_cy}, dot_c, 1.1f);
                ctx.draw_line(PointF{r.max_x - 4.0f, icon_cy}, PointF{r.max_x - 6.0f, icon_cy + 2.0f}, dot_c, 1.1f);
            }
        }

        if (label_alpha > 0.01f) {
            float right_reserve = 8.0f;
            if (!item.children.empty()) {
                float cx = r.max_x - 14.0f;
                float cy = std::round(r.min_y + r.height() * 0.5f);
                float dir = item.expanded ? -1.0f : 1.0f;
                Color ch_col = ic;
                ch_col.a *= label_alpha * 0.75f;
                ctx.draw_line(PointF{cx - 3.2f, cy - 1.6f * dir}, PointF{cx, cy + 1.6f * dir}, ch_col, 1.2f);
                ctx.draw_line(PointF{cx, cy + 1.6f * dir}, PointF{cx + 3.2f, cy - 1.6f * dir}, ch_col, 1.2f);
                right_reserve += 20.0f;
            }

            if (!item.badge.empty()) {
                Vec2 bsz = TextMeasure::measure(item.badge, 10.5f);
                float bw = std::max(18.0f, std::ceil(bsz.x + 10.0f));
                float bh = 16.0f;
                float bx = std::round(r.max_x - right_reserve - bw);
                float by = std::round(r.min_y + (r.height() - bh) * 0.5f);
                Color bg = style.badge_bg; bg.a *= label_alpha;
                Color bt = style.badge_text; bt.a *= label_alpha;
                ctx.draw_rect(Rect::from_xywh(bx, by, bw, bh), bg, bh * 0.5f, 0.0f);
                ctx.draw_text(item.badge, PointF{std::round(bx + (bw - bsz.x) * 0.5f), std::round(by + (bh - bsz.y) * 0.5f)}, 10.5f, bt);
                right_reserve += bw + 6.0f;
            }

            float text_x = (item.icon != NavigationIcon::None || item.icon_painter || row.is_settings)
                ? (r.min_x + 40.0f + indent)
                : (row.depth > 0 ? (r.min_x + 40.0f + static_cast<float>(row.depth - 1) * 16.0f) : (r.min_x + 14.0f));
            float clip_w = std::max(0.0f, r.max_x - right_reserve - text_x);
            if (clip_w > 2.0f) {
                Color tc = item.disabled ? style.item_disabled_text
                         : (is_selected ? style.item_selected_text : style.item_text);
                tc.a *= label_alpha;
                Vec2 ts = TextMeasure::measure(item.label, style.font_size);
                ctx.push_clip(Rect::from_xywh(text_x, r.min_y, clip_w, r.height()), 0.0f);
                ctx.draw_text(item.label, PointF{std::round(text_x), std::round(r.min_y + (r.height() - ts.y) * 0.5f)}, style.font_size, tc);
                ctx.pop_clip();
            }
        }
    }

    struct TopItemLayout {
        const NavigationViewItem* item{nullptr};
        Rect rect;
        bool is_settings{false};
    };

    std::vector<TopItemLayout> compute_top_layouts() const {
        std::vector<TopItemLayout> out;
        float x = 8.0f;
        if (m_data.is_back_button_visible) x += 42.0f;

        for (const auto& item : m_data.menu_items) {
            if (item.kind != NavigationItemKind::Item) continue;
            Vec2 ts = TextMeasure::measure(item.label, 13.0f);
            float icon_w = (item.icon != NavigationIcon::None || item.icon_painter) ? 22.0f : 0.0f;
            float chev_w = !item.children.empty() ? 14.0f : 0.0f;
            float w = std::ceil(ts.x + icon_w + chev_w + 24.0f);
            out.push_back(TopItemLayout{&item, Rect::from_xywh(x, 4.0f, w, 36.0f), false});
            x += w + 4.0f;
        }

        float rx = size().x - 8.0f;
        if (m_data.is_settings_visible) {
            float w = 38.0f;
            rx -= w;
            out.push_back(TopItemLayout{&m_settings_item, Rect::from_xywh(rx, 4.0f, w, 36.0f), true});
            rx -= 4.0f;
        }
        return out;
    }

    void paint_top_nav(PaintContext& ctx, const ResolvedNavigationViewStyle& style) {
        if (m_data.is_back_button_visible) {
            paint_chrome_button(ctx, Rect::from_xywh(6.0f, 4.0f, 38.0f, 36.0f), ChromeButton::Back, style, can_go_back());
        }

        auto layouts = compute_top_layouts();
        float target_pill_x = -1.0f;
        float target_pill_w = 16.0f;

        for (int i = 0; i < static_cast<int>(layouts.size()); ++i) {
            const auto& tl = layouts[static_cast<size_t>(i)];
            const auto& item = *tl.item;
            bool is_sel = is_item_or_child_selected(item, false);
            bool is_hov = (m_hovered_top == i && !item.disabled);
            bool is_prs = (m_pressed_top == i && !item.disabled);

            if (is_prs) ctx.draw_rect(tl.rect, style.item_pressed_bg, style.item_radius, 0.0f);
            else if (is_hov) ctx.draw_rect(tl.rect, style.item_hover_bg, style.item_radius, 0.0f);

            if (is_sel) {
                target_pill_w = std::max(16.0f, tl.rect.width() - 24.0f);
                target_pill_x = tl.rect.min_x + (tl.rect.width() - target_pill_w) * 0.5f;
            }

            Color ic = item.disabled ? style.item_disabled_text
                     : (is_sel ? style.icon_selected_color : style.icon_color);
            if (tl.is_settings) {
                Rect ir = Rect::from_xywh(std::round(tl.rect.min_x + (tl.rect.width() - 16.0f) * 0.5f),
                                          std::round(tl.rect.min_y + (tl.rect.height() - 16.0f) * 0.5f), 16.0f, 16.0f);
                NavigationViewStyleResolver::paint_settings_gear(ctx, ir, ic, m_settings_gear_angle, is_prs ? 0.90f : 1.0f);
            } else {
                float tx = tl.rect.min_x + 12.0f;
                if (item.icon != NavigationIcon::None || item.icon_painter) {
                    Rect ir = Rect::from_xywh(std::round(tx), std::round(tl.rect.min_y + (tl.rect.height() - 16.0f) * 0.5f), 16.0f, 16.0f);
                    if (item.icon_painter) item.icon_painter(ctx, ir, ic);
                    else NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, ir, ic);
                    tx += 22.0f;
                }
                Color tc = item.disabled ? style.item_disabled_text
                         : (is_sel ? style.item_selected_text : style.item_text);
                Vec2 ts = TextMeasure::measure(item.label, style.font_size);
                ctx.draw_text(item.label, PointF{std::round(tx), std::round(tl.rect.min_y + (tl.rect.height() - ts.y) * 0.5f)},
                              style.font_size, tc);
                if (!item.children.empty()) {
                    float cx = tl.rect.max_x - 11.0f;
                    float cy = std::round(tl.rect.min_y + tl.rect.height() * 0.5f);
                    ctx.draw_line(PointF{cx - 3.0f, cy - 1.5f}, PointF{cx, cy + 1.5f}, ic, 1.15f);
                    ctx.draw_line(PointF{cx, cy + 1.5f}, PointF{cx + 3.0f, cy - 1.5f}, ic, 1.15f);
                }
            }
        }

        // Top 模式底部 3px 弹性拉伸水平滑动强调条
        if (target_pill_x >= 0.0f) {
            if (!m_pill_initialized) {
                m_pill_head = m_pill_tail = target_pill_x;
                m_pill_size = target_pill_w;
                m_pill_initialized = true;
            }
            m_pill_target_pos = target_pill_x;
            m_pill_target_size = target_pill_w;
            if (std::abs(m_pill_head - m_pill_target_pos) > 0.25f ||
                std::abs(m_pill_tail - m_pill_target_pos) > 0.25f ||
                std::abs(m_pill_size - m_pill_target_size) > 0.25f) {
                mark_needs_paint();
            }
            float pill_l = std::min(m_pill_head, m_pill_tail);
            float pill_r = std::max(m_pill_head, m_pill_tail) + m_pill_size;
            ctx.draw_rect(Rect::from_xywh(pill_l, 41.0f, std::max(16.0f, pill_r - pill_l), 3.0f),
                          style.accent_pill, 1.5f, 0.0f);
        }
    }

    ChromeButton hit_test_chrome(PointF p, NavigationPaneDisplayMode eff) const noexcept {
        if (eff == NavigationPaneDisplayMode::Top) {
            if (m_data.is_back_button_visible && Rect::from_xywh(6.0f, 4.0f, 38.0f, 36.0f).contains(p)) {
                return ChromeButton::Back;
            }
            return ChromeButton::None;
        }
        if (eff == NavigationPaneDisplayMode::LeftMinimal && m_pane_open_anim <= 0.02f) {
            float bx = 4.0f;
            if (m_data.is_back_button_visible) {
                if (Rect::from_xywh(bx, 4.0f, 40.0f, 36.0f).contains(p)) return ChromeButton::Back;
                bx += 42.0f;
            }
            if (m_data.is_pane_toggle_button_visible && Rect::from_xywh(bx, 4.0f, 40.0f, 36.0f).contains(p)) {
                return ChromeButton::Toggle;
            }
            return ChromeButton::None;
        }
        if (m_data.is_back_button_visible && m_data.is_pane_toggle_button_visible) {
            if (m_pane_open_anim > 0.35f && Rect::from_xywh(4.0f, 4.0f, 36.0f, 36.0f).contains(p)) {
                return ChromeButton::Back;
            }
            float tx = 4.0f + 38.0f * m_pane_open_anim;
            float tw = 40.0f - 4.0f * m_pane_open_anim;
            if (Rect::from_xywh(tx, 4.0f, tw, 36.0f).contains(p)) {
                return ChromeButton::Toggle;
            }
            return ChromeButton::None;
        }
        if (m_data.is_back_button_visible && Rect::from_xywh(4.0f, 4.0f, 40.0f, 36.0f).contains(p)) {
            return ChromeButton::Back;
        }
        if (m_data.is_pane_toggle_button_visible && Rect::from_xywh(4.0f, 4.0f, 40.0f, 36.0f).contains(p)) {
            return ChromeButton::Toggle;
        }
        return ChromeButton::None;
    }

    void hit_test_left_row(PointF p, NavigationPaneDisplayMode eff, int& out_idx, bool& out_footer, float* out_y = nullptr) const noexcept {
        out_idx = -1;
        out_footer = false;
        const float pane_w = current_pane_width(eff);
        if (p.x < 0.0f || p.x > pane_w) return;

        Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
        const bool compact_hide = (m_pane_open_anim < 0.35f);

        if (menu_vp.contains(p)) {
            float y = menu_vp.min_y + 2.0f - m_menu_scroll.offset();
            for (int i = 0; i < static_cast<int>(m_main_rows.size()); ++i) {
                const auto& row = m_main_rows[static_cast<size_t>(i)];
                if (!row.item) continue;
                if (compact_hide && (row.depth > 0 || row.item->kind == NavigationItemKind::Header)) continue;
                if (p.y >= y && p.y <= y + row.height) {
                    if (row.item->kind == NavigationItemKind::Item) {
                        out_idx = i;
                        out_footer = false;
                        if (out_y) *out_y = y;
                    }
                    return;
                }
                y += row.height + 2.0f;
            }
            return;
        }

        if (!m_footer_rows.empty()) {
            float fy = menu_vp.max_y + 11.0f;
            for (int i = 0; i < static_cast<int>(m_footer_rows.size()); ++i) {
                const auto& row = m_footer_rows[static_cast<size_t>(i)];
                if (!row.item) continue;
                if (p.y >= fy && p.y <= fy + row.height) {
                    if (row.item->kind == NavigationItemKind::Item) {
                        out_idx = i;
                        out_footer = true;
                        if (out_y) *out_y = fy;
                    }
                    return;
                }
                fy += row.height + 2.0f;
            }
        }
    }

    void activate_item(const NavigationViewItem& item, NavigationPaneDisplayMode eff,
                       bool is_settings, PointF flyout_anchor, bool flyout_below) {
        if (item.disabled || item.kind != NavigationItemKind::Item) return;

        if (is_settings) {
            // WinUI 3 AnimatedSettingsVisualSource: 每次点击顺滑旋转 90° (π/2 rad)
            m_settings_spin_base += 1.5707963f;
            m_last_anim_time = std::chrono::steady_clock::now();
        }

        // 含子项时:
        // - Compact / Top 模式下直接弹出 WinUI 3 浮层子菜单，绝不强行展开侧边栏！
        // - Expanded 模式下在侧边栏内就地展开/折叠子项
        if (!item.children.empty()) {
            if (eff == NavigationPaneDisplayMode::Top || m_pane_open_anim < 0.5f) {
                auto now = std::chrono::steady_clock::now();
                if (m_last_submenu_dismiss.time_since_epoch().count() != 0 &&
                    std::chrono::duration<float>(now - m_last_submenu_dismiss).count() < 0.06f) {
                    return;
                }
                open_submenu_flyout(item, flyout_anchor, flyout_below);
                return;
            }
            for (auto& mi : m_data.menu_items) {
                if (mi.effective_id() == item.effective_id()) {
                    mi.expanded = !mi.expanded;
                    rebuild_flat_items();
                    m_last_anim_time = std::chrono::steady_clock::now();
                    mark_needs_layout();
                    mark_needs_paint();
                    return;
                }
            }
        }

        close_submenu_flyout();
        set_selected_id(item.effective_id(), true, true);
        if (is_overlay_pane(eff) && m_data.is_pane_open) {
            set_pane_open(false, true);
        }
    }

    void setup_behavior() {
        // 隧道预览：识别 Android / Windows / Linux 触控边缘右滑展开、左滑收起与侧边栏列表触控滚动
        on_pointer_preview = [this](PointerEvent& e) {
            if (e.device_kind != PointerDeviceKind::Touch) return;
            const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);
            if (eff == NavigationPaneDisplayMode::Top) return;

            if (e.type == PlatformEventType::PointerDown) {
                m_touch_tracking = true;
                m_touch_mode = TouchGestureMode::None;
                m_touch_start_local = e.local_position;
                m_touch_last_local = e.local_position;
                if (m_menu_scroll.max_scroll() > 0.0f) {
                    m_menu_scroll.begin_touch_drag();
                }
            } else if (e.type == PlatformEventType::PointerMove && m_touch_tracking && m_touch_mode == TouchGestureMode::None) {
                float dx = e.local_position.x - m_touch_start_local.x;
                float dy = e.local_position.y - m_touch_start_local.y;
                const float pane_w = current_pane_width(eff);

                // 1. 左边缘右滑直接展开侧边栏 (任意模式关闭态从左侧 <= 36px 右滑 > 20px)
                if (!m_data.is_pane_open && m_touch_start_local.x <= 36.0f &&
                    dx > 20.0f && std::abs(dx) > std::abs(dy) * 1.2f) {
                    m_touch_mode = TouchGestureMode::EdgeSwipe;
                    set_pane_open(true, true);
                    e.gesture_capture_target = this;
                    e.stop_propagation();
                    return;
                }
                // 2. 侧边栏展开态向左滑直接收起 (dx < -28px)
                if (m_data.is_pane_open && m_touch_start_local.x <= pane_w + 24.0f &&
                    dx < -28.0f && std::abs(dx) > std::abs(dy) * 1.2f) {
                    m_touch_mode = TouchGestureMode::EdgeSwipe;
                    set_pane_open(false, true);
                    e.gesture_capture_target = this;
                    e.stop_propagation();
                    return;
                }
                // 3. 侧边栏菜单列表垂直触控拖拽滚动 (避开滚动条区域)
                Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
                auto sb_m = m_menu_scrollbar.compute_metrics(menu_vp, compute_left_menu_total_height(), m_menu_scroll.offset());
                bool on_sb = (m_menu_scrollbar.hit_test(m_touch_start_local, sb_m) != WinUIScrollBar::HitPart::None);
                if (!on_sb && !m_menu_scrollbar.is_dragging() &&
                    menu_vp.contains(m_touch_start_local) && m_menu_scroll.max_scroll() > 0.0f &&
                    std::abs(dy) > 8.0f && std::abs(dy) > std::abs(dx) * 1.15f) {
                    m_touch_mode = TouchGestureMode::MenuScroll;
                    m_menu_scroll.apply_touch_drag(e.local_position.y - m_touch_last_local.y);
                    m_touch_last_local = e.local_position;
                    m_menu_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.gesture_capture_target = this;
                    e.stop_propagation();
                    return;
                }
            } else if (e.type == PlatformEventType::PointerUp) {
                m_touch_tracking = false;
                m_touch_mode = TouchGestureMode::None;
            }
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (e.device_kind == PointerDeviceKind::Touch && m_touch_mode != TouchGestureMode::None) {
                if (m_touch_mode == TouchGestureMode::MenuScroll) {
                    float dy = e.local_position.y - m_touch_last_local.y;
                    m_touch_last_local = e.local_position;
                    m_menu_scroll.apply_touch_drag(dy);
                    m_menu_scrollbar.notify_activity();
                    mark_needs_paint();
                }
                e.stop_propagation();
                return;
            }
            const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

            if (e.type == PlatformEventType::PointerScroll && eff != NavigationPaneDisplayMode::Top) {
                Rect menu_vp = compute_left_menu_viewport(size().y, current_pane_width(eff));
                m_menu_scroll.set_max_scroll(std::max(0.0f, compute_left_menu_total_height() - menu_vp.height()));
                if (menu_vp.contains(e.local_position) && m_menu_scroll.max_scroll() > 0.0f) {
                    m_menu_scroll.add_wheel_delta(e.delta.y);
                    m_menu_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.stop_propagation();
                }
                return;
            }

            if (eff == NavigationPaneDisplayMode::Top) {
                ChromeButton cb = hit_test_chrome(e.local_position, eff);
                int top_idx = -1;
                bool hov_settings = false;
                if (cb == ChromeButton::None) {
                    auto layouts = compute_top_layouts();
                    for (int i = 0; i < static_cast<int>(layouts.size()); ++i) {
                        if (layouts[static_cast<size_t>(i)].rect.contains(e.local_position)) {
                            top_idx = i;
                            hov_settings = layouts[static_cast<size_t>(i)].is_settings;
                            break;
                        }
                    }
                }
                if (cb != m_hovered_chrome || top_idx != m_hovered_top || hov_settings != m_settings_hovered) {
                    m_hovered_chrome = cb;
                    m_hovered_top = top_idx;
                    m_settings_hovered = hov_settings;
                    m_last_anim_time = std::chrono::steady_clock::now();
                    mark_needs_paint();
                }
                return;
            }

            Rect menu_vp = compute_left_menu_viewport(size().y, current_pane_width(eff));
            bool sb_consumed = false;
            if (m_menu_scrollbar.handle_pointer_move(e.local_position, menu_vp, compute_left_menu_total_height(),
                                                     m_menu_scroll, sb_consumed)) {
                mark_needs_paint();
            }
            if (sb_consumed) {
                e.stop_propagation();
                return;
            }

            ChromeButton cb = hit_test_chrome(e.local_position, eff);
            int r_idx = -1;
            bool r_footer = false;
            if (cb == ChromeButton::None) {
                hit_test_left_row(e.local_position, eff, r_idx, r_footer);
            }
            bool hov_settings = (r_footer && r_idx >= 0 && r_idx < static_cast<int>(m_footer_rows.size()) &&
                                 m_footer_rows[static_cast<size_t>(r_idx)].is_settings);
            if (cb != m_hovered_chrome || r_idx != m_hovered_row ||
                r_footer != m_hovered_in_footer || hov_settings != m_settings_hovered) {
                m_hovered_chrome = cb;
                m_hovered_row = r_idx;
                m_hovered_in_footer = r_footer;
                m_settings_hovered = hov_settings;
                m_last_anim_time = std::chrono::steady_clock::now();
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_menu_scrollbar.handle_pointer_exit();
            if (m_hovered_chrome != ChromeButton::None || m_hovered_row != -1 || m_hovered_top != -1 || m_settings_hovered) {
                m_hovered_chrome = ChromeButton::None;
                m_hovered_row = -1;
                m_hovered_top = -1;
                m_settings_hovered = false;
                m_last_anim_time = std::chrono::steady_clock::now();
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& e) {
            const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

            if (eff == NavigationPaneDisplayMode::Top) {
                ChromeButton cb = hit_test_chrome(e.local_position, eff);
                if (cb != ChromeButton::None) {
                    m_pressed_chrome = cb;
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
                auto layouts = compute_top_layouts();
                for (int i = 0; i < static_cast<int>(layouts.size()); ++i) {
                    if (layouts[static_cast<size_t>(i)].rect.contains(e.local_position)) {
                        m_pressed_top = i;
                        mark_needs_paint();
                        e.stop_propagation();
                        return;
                    }
                }
                return;
            }

            const float pane_w = current_pane_width(eff);
            if (is_overlay_pane(eff) && m_data.is_pane_open && e.local_position.x > pane_w) {
                set_pane_open(false, true);
                e.stop_propagation();
                return;
            }

            Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
            if (m_menu_scrollbar.handle_pointer_down(e.local_position, menu_vp, compute_left_menu_total_height(),
                                                     m_menu_scroll, 36.0f)) {
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            ChromeButton cb = hit_test_chrome(e.local_position, eff);
            if (cb != ChromeButton::None) {
                m_pressed_chrome = cb;
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            int r_idx = -1;
            bool r_footer = false;
            hit_test_left_row(e.local_position, eff, r_idx, r_footer);
            if (r_idx >= 0) {
                m_pressed_row = r_idx;
                m_pressed_in_footer = r_footer;
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_pointer_up = [this](PointerEvent& e) {
            bool sb_released = m_menu_scrollbar.handle_pointer_up();
            if (m_touch_mode != TouchGestureMode::None) {
                if (m_touch_mode == TouchGestureMode::MenuScroll) {
                    m_menu_scroll.end_touch_drag();
                    m_menu_scrollbar.notify_activity();
                    mark_needs_paint();
                }
                m_touch_mode = TouchGestureMode::None;
                m_touch_tracking = false;
                e.stop_propagation();
                return;
            }
            m_touch_tracking = false;

            if (sb_released) {
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

            if (m_pressed_chrome != ChromeButton::None) {
                ChromeButton pressed = m_pressed_chrome;
                m_pressed_chrome = ChromeButton::None;
                if (hit_test_chrome(e.local_position, eff) == pressed) {
                    if (pressed == ChromeButton::Back && can_go_back()) {
                        go_back();
                    } else if (pressed == ChromeButton::Toggle) {
                        toggle_pane();
                    }
                }
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            if (eff == NavigationPaneDisplayMode::Top && m_pressed_top >= 0) {
                int pressed = m_pressed_top;
                m_pressed_top = -1;
                auto layouts = compute_top_layouts();
                if (pressed < static_cast<int>(layouts.size()) &&
                    layouts[static_cast<size_t>(pressed)].rect.contains(e.local_position)) {
                    const auto& tl = layouts[static_cast<size_t>(pressed)];
                    activate_item(*tl.item, eff, tl.is_settings,
                                  PointF{tl.rect.min_x, tl.rect.max_y + 4.0f}, true);
                }
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            if (m_pressed_row >= 0) {
                int pr = m_pressed_row;
                bool pf = m_pressed_in_footer;
                m_pressed_row = -1;
                int hr = -1;
                bool hf = false;
                float row_y = 0.0f;
                hit_test_left_row(e.local_position, eff, hr, hf, &row_y);
                if (hr == pr && hf == pf) {
                    const auto& list = pf ? m_footer_rows : m_main_rows;
                    if (pr >= 0 && pr < static_cast<int>(list.size()) && list[static_cast<size_t>(pr)].item) {
                        const auto& row = list[static_cast<size_t>(pr)];
                        activate_item(*row.item, eff, row.is_settings,
                                      PointF{current_pane_width(eff) + 4.0f, row_y}, false);
                    }
                }
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_key = [this](KeyEvent& e) {
            if (e.type != PlatformEventType::KeyDown) return;
            const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);
            if (e.key_code == 27) { // Esc 收起浮层面板
                if (is_overlay_pane(eff) && m_data.is_pane_open) {
                    set_pane_open(false, true);
                    e.stop_propagation();
                }
            }
        };
    }

    void update_animations() {
        auto now = std::chrono::steady_clock::now();
        if (m_last_anim_time.time_since_epoch().count() == 0) {
            m_last_anim_time = now;
        }
        float dt = std::clamp(std::chrono::duration<float>(now - m_last_anim_time).count(), 0.0f, 0.05f);
        m_last_anim_time = now;

        bool animating = false;

        // 1. 面板展开/收起动画 (200ms)
        float target_open = m_data.is_pane_open ? 1.0f : 0.0f;
        if (std::abs(m_pane_open_anim - target_open) > 0.004f) {
            m_pane_open_anim += (target_open - m_pane_open_anim) * std::min(1.0f, dt * 18.0f);
            if (std::abs(m_pane_open_anim - target_open) <= 0.004f) {
                m_pane_open_anim = target_open;
            }
            NavigationPaneDisplayMode eff = compute_effective_mode(size().x);
            if (eff == NavigationPaneDisplayMode::Left && m_content_box) {
                Rect body = compute_content_body_rect(size().x, size().y, eff);
                m_content_box->layout(BoxConstraints::tight(Vec2{std::max(0.0f, body.width()), std::max(0.0f, body.height())}));
                m_content_box->set_offset(Vec2{body.min_x, body.min_y});
            }
            animating = true;
        } else {
            m_pane_open_anim = target_open;
        }

        // 1.5 先推进菜单列表滚动并将滚动位移直接同步至小蓝条，确保滚动时零拖尾、零延迟
        if (m_menu_scroll.update()) animating = true;
        if (m_menu_scrollbar.update_animations(m_hovered_row >= 0)) animating = true;
        {
            float cur_scroll = m_menu_scroll.offset();
            float dscroll = cur_scroll - m_pill_last_scroll_y;
            if (m_pill_initialized && m_pill_in_menu && std::abs(dscroll) > 0.0001f) {
                m_pill_head -= dscroll;
                m_pill_tail -= dscroll;
                m_pill_target_pos -= dscroll;
            }
            m_pill_last_scroll_y = cur_scroll;
        }

        // 2. WinUI 3 弹性拉伸滑动强调条 (仅在切换选中项时生效: head 30x 先行、tail 15x 追赶)
        if (m_pill_initialized) {
            if (std::abs(m_pill_head - m_pill_target_pos) > 0.25f) {
                m_pill_head += (m_pill_target_pos - m_pill_head) * std::min(1.0f, dt * 30.0f);
                animating = true;
            } else {
                m_pill_head = m_pill_target_pos;
            }
            if (std::abs(m_pill_tail - m_pill_target_pos) > 0.25f) {
                m_pill_tail += (m_pill_target_pos - m_pill_tail) * std::min(1.0f, dt * 15.0f);
                animating = true;
            } else {
                m_pill_tail = m_pill_target_pos;
            }
            if (std::abs(m_pill_size - m_pill_target_size) > 0.25f) {
                m_pill_size += (m_pill_target_size - m_pill_size) * std::min(1.0f, dt * 22.0f);
                animating = true;
            } else {
                m_pill_size = m_pill_target_size;
            }
        }

        // 3. WinUI 3 设置齿轮旋转动效 (悬停微转 +18°, 点击顺滑旋转 +90°)
        float gear_target = m_settings_spin_base + (m_settings_hovered ? 0.314f : 0.0f);
        if (std::abs(m_settings_gear_angle - gear_target) > 0.005f) {
            m_settings_gear_angle += (gear_target - m_settings_gear_angle) * std::min(1.0f, dt * 12.0f);
            animating = true;
        } else {
            m_settings_gear_angle = gear_target;
        }

        // 4. 页面切换进场动画 (140ms)
        if (m_page_enter_progress < 1.0f) {
            m_page_enter_progress = std::min(1.0f, m_page_enter_progress + dt / 0.14f);
            animating = true;
        }

        if (animating) {
            mark_needs_paint();
        }
    }

    NavigationViewData m_data;
    NavigationViewItem m_settings_item;
    std::vector<FlatRow> m_main_rows;
    std::vector<FlatRow> m_footer_rows;
    std::vector<std::string> m_history;

    WidgetPtr m_content_widget;
    std::shared_ptr<RenderBox> m_content_box;
    std::function<WidgetPtr(const std::string&)> m_page_builder;

    NavigationPaneDisplayMode m_last_auto_mode{NavigationPaneDisplayMode::Auto};
    ChromeButton m_hovered_chrome{ChromeButton::None};
    ChromeButton m_pressed_chrome{ChromeButton::None};
    int m_hovered_row{-1};
    bool m_hovered_in_footer{false};
    int m_pressed_row{-1};
    bool m_pressed_in_footer{false};
    int m_hovered_top{-1};
    int m_pressed_top{-1};
    bool m_settings_hovered{false};

    uint64_t m_submenu_overlay_id{0};
    std::string m_submenu_parent_id;
    std::chrono::steady_clock::time_point m_last_submenu_dismiss;

    float m_pane_open_anim{1.0f};
    bool m_pill_initialized{false};
    bool m_pill_in_menu{true};
    float m_pill_last_scroll_y{0.0f};
    float m_pill_last_menu_top{0.0f};
    float m_saved_expanded_scroll_y{0.0f};
    bool m_restore_expanded_scroll{false};
    float m_pill_head{0.0f};
    float m_pill_tail{0.0f};
    float m_pill_size{16.0f};
    float m_pill_target_pos{0.0f};
    float m_pill_target_size{16.0f};
    float m_settings_spin_base{0.0f};
    float m_settings_gear_angle{0.0f};
    float m_page_enter_progress{1.0f};

    enum class TouchGestureMode : uint8_t { None, EdgeSwipe, MenuScroll };
    bool m_touch_tracking{false};
    TouchGestureMode m_touch_mode{TouchGestureMode::None};
    PointF m_touch_start_local{0.0f, 0.0f};
    PointF m_touch_last_local{0.0f, 0.0f};

    SmoothScrollPhysics m_menu_scroll;
    WinUIScrollBar m_menu_scrollbar;
    std::shared_ptr<RenderAutoSuggestBox> m_search_box;
    std::vector<std::pair<std::string, std::string>> m_search_map;
    std::chrono::steady_clock::time_point m_last_anim_time;
};

// C++20 指定初始化器声明式 WinUI 3 NavigationView 控件
struct NavigationView {
    std::vector<NavigationViewItem> menu_items;
    std::vector<NavigationViewItem> footer_items;

    Bindable<std::string> selected_id{""};
    Bindable<std::string> pane_title{""};
    Bindable<std::string> header{""};
    Bindable<NavigationPaneDisplayMode> pane_display_mode{NavigationPaneDisplayMode::Auto};
    Bindable<bool> is_pane_open{true};

    bool is_pane_toggle_button_visible{true};
    bool is_back_button_visible{true};
    bool is_back_enabled{false};
    bool is_settings_visible{true};
    bool is_search_box_visible{false};
    std::string search_placeholder{"搜索控件"};
    std::string settings_label{"设置"};

    float open_pane_length{240.0f};
    float compact_pane_length{48.0f};
    float compact_mode_threshold_width{641.0f};
    float expanded_mode_threshold_width{900.0f};

    WidgetPtr content{nullptr};
    std::function<WidgetPtr(const std::string&)> page_builder{nullptr};
    std::function<void(const std::string&)> on_selection_changed{nullptr};
    std::function<void()> on_back_requested{nullptr};
    std::function<void(bool)> on_pane_toggled{nullptr};

    operator WidgetPtr() const;
};

class NavigationViewWidget : public Widget {
public:
    explicit NavigationViewWidget(const NavigationView& desc) : m_desc(desc) {}
    ~NavigationViewWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderNavigationView>();
        apply_desc_to_node(node);
        setup_bindings(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto nav = std::dynamic_pointer_cast<RenderNavigationView>(node)) {
            apply_desc_to_node(nav);
            setup_bindings(nav);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderNavigationView>& node) {
        node->set_chrome_options(
            m_desc.is_back_button_visible, m_desc.is_back_enabled,
            m_desc.is_pane_toggle_button_visible, m_desc.is_settings_visible,
            m_desc.settings_label, m_desc.pane_title.get(), m_desc.header.get(),
            m_desc.open_pane_length, m_desc.compact_pane_length,
            m_desc.compact_mode_threshold_width, m_desc.expanded_mode_threshold_width);
        node->set_pane_display_mode(m_desc.pane_display_mode.get());
        node->set_pane_open(m_desc.is_pane_open.get(), false);
        node->set_menu_items(m_desc.menu_items);
        node->set_footer_items(m_desc.footer_items);
        node->set_search_options(m_desc.is_search_box_visible, m_desc.search_placeholder);

        node->set_callbacks(
            [this](const std::string& id) {
                m_is_internal_update = true;
                m_desc.selected_id.set_bound(id);
                if (m_desc.on_selection_changed) m_desc.on_selection_changed(id);
                m_is_internal_update = false;
            },
            m_desc.on_back_requested,
            [this](bool open) {
                m_is_internal_update = true;
                m_desc.is_pane_open.set_bound(open);
                if (m_desc.on_pane_toggled) m_desc.on_pane_toggled(open);
                m_is_internal_update = false;
            });

        if (!m_desc.selected_id.get().empty()) {
            node->set_selected_id(m_desc.selected_id.get(), false, true);
        }
        if (m_desc.page_builder) {
            node->set_page_builder(m_desc.page_builder);
        } else if (m_desc.content) {
            node->set_content_widget(m_desc.content);
        }
    }

    void setup_bindings(const std::shared_ptr<RenderNavigationView>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.selected_id, node, [this](RenderNavigationView& n, const std::string& id) {
            if (!m_is_internal_update) n.set_selected_id(id, false, true);
        });
        m_bindings.attach(m_desc.pane_display_mode, node, [](RenderNavigationView& n, NavigationPaneDisplayMode m) {
            n.set_pane_display_mode(m);
        });
        m_bindings.attach(m_desc.is_pane_open, node, [this](RenderNavigationView& n, bool open) {
            if (!m_is_internal_update) n.set_pane_open(open, false);
        });
    }

    NavigationView m_desc;
    BindingGroup m_bindings;
    bool m_is_internal_update{false};
};

inline NavigationView::operator WidgetPtr() const {
    return std::make_shared<NavigationViewWidget>(*this);
}

} // namespace ktui
