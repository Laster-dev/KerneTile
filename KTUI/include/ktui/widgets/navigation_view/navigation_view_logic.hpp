#pragma once

#include <ktui/core/strings.hpp>

#include "navigation_view_data.hpp"
#include "navigation_view_style.hpp"
#include <ktui/widgets/auto_suggest_box/auto_suggest_box_logic.hpp>
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

#include "navigation_view_submenu.hpp"

namespace ktui {

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

    [[nodiscard]] const NavigationViewData& data() const noexcept { return m_data; }

    void set_search_options(bool visible, std::string placeholder) {
        m_data.is_search_box_visible = visible;
        m_data.search_placeholder = std::move(placeholder);
        if (visible && !m_search_box) {
            m_search_box = make_render_node<RenderAutoSuggestBox>();
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
    [[nodiscard]] bool pointer_reveal_enabled() const noexcept { return ThemeContext::current().resolve_pointer_reveal(m_data.enable_pointer_reveal); }
    void set_pointer_reveal_enabled(std::optional<bool> enabled) {
        if (m_data.enable_pointer_reveal != enabled) {
            m_data.enable_pointer_reveal = enabled;
            if (!ThemeContext::current().resolve_pointer_reveal(m_data.enable_pointer_reveal)) {
                m_has_reveal_cursor = false;
            }
            mark_needs_paint();
        }
    }

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
            m_has_reveal_cursor = false;
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
            m_has_reveal_cursor = false;
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

    void set_callbacks(Fn<void(const std::string&)> on_sel,
                       Fn<void()> on_back,
                       Fn<void(bool)> on_toggle) {
        m_data.on_selection_changed = std::move(on_sel);
        m_data.on_back_requested = std::move(on_back);
        m_data.on_pane_toggled = std::move(on_toggle);
    }

    void set_page_builder(Fn<WidgetPtr(const std::string&)> builder) {
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

    void paint(PaintContext& ctx) override;

private:
    struct FlatRow {
        const NavigationViewItem* item{nullptr};
        const NavigationViewItem* parent{nullptr};
        int depth{0};
        bool is_footer{false};
        bool is_settings{false};
        float height{36.0f};
    };

    [[nodiscard]] float get_expand_anim(const std::string& id, bool fallback_expanded) const noexcept {
        for (const auto& kv : m_expand_anims) {
            if (kv.first == id) return kv.second;
        }
        return fallback_expanded ? 1.0f : 0.0f;
    }

    void sync_expand_anims() {
        for (const auto& item : m_data.menu_items) {
            if (item.kind == NavigationItemKind::Item && !item.children.empty()) {
                const std::string& id = item.effective_id();
                bool exists = false;
                for (const auto& kv : m_expand_anims) {
                    if (kv.first == id) { exists = true; break; }
                }
                if (!exists) {
                    m_expand_anims.emplace_back(id, item.expanded ? 1.0f : 0.0f);
                }
            }
        }
    }

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
        sync_expand_anims();
        m_main_rows.clear();
        m_footer_rows.clear();
        m_settings_item = NavigationViewItem{};
        m_settings_item.id = m_data.settings_id;
        m_settings_item.label = m_data.settings_label;
        m_settings_item.icon = NavigationIcon::Settings;

        for (const auto& item : m_data.menu_items) {
            float rh = (item.kind == NavigationItemKind::Separator) ? 13.0f
                     : (item.kind == NavigationItemKind::Header ? 30.0f : 36.0f);
            m_main_rows.push_back(FlatRow{&item, nullptr, 0, false, false, rh});
            if (item.kind == NavigationItemKind::Item && !item.children.empty()) {
                for (const auto& child : item.children) {
                    m_main_rows.push_back(FlatRow{&child, &item, 1, false, false, 34.0f});
                }
            }
        }

        for (const auto& item : m_data.footer_items) {
            float rh = (item.kind == NavigationItemKind::Separator) ? 13.0f : 36.0f;
            m_footer_rows.push_back(FlatRow{&item, nullptr, 0, true, false, rh});
        }
        if (m_data.is_settings_visible) {
            m_footer_rows.push_back(FlatRow{&m_settings_item, nullptr, 0, true, true, 36.0f});
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
            if (r.depth > 0 && r.parent) {
                float ef = get_expand_anim(r.parent->effective_id(), r.parent->expanded);
                if (ef <= 0.004f) continue;
                y += (r.height + 2.0f) * ef;
            } else {
                y += r.height + 2.0f;
            }
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

        auto flyout = make_render_node<RenderNavigationSubmenuFlyout>(
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

    void paint_content_area(PaintContext& ctx, const ResolvedNavigationViewStyle& style, NavigationPaneDisplayMode eff);

    void paint_item_pointer_reveal(PaintContext& ctx, Rect r, float corner_radius, bool enabled, float alpha_mul = 1.0f) const {
        if (!ThemeContext::current().resolve_pointer_reveal(m_data.enable_pointer_reveal) || !m_has_reveal_cursor || !enabled || r.is_empty() || alpha_mul <= 0.01f) {
            return;
        }
        constexpr float kRevealRadius = 72.0f;
        if (r.max_x < m_reveal_cursor.x - kRevealRadius || r.min_x > m_reveal_cursor.x + kRevealRadius ||
            r.max_y < m_reveal_cursor.y - kRevealRadius || r.min_y > m_reveal_cursor.y + kRevealRadius) {
            return;
        }
        const bool is_dark = ThemeContext::current().is_dark;
        const Color surface = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.085f * alpha_mul}
                                      : Color{0.0f, 0.0f, 0.0f, 0.055f * alpha_mul};
        const Color edge = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.28f * alpha_mul}
                                   : Color{0.0f, 0.0f, 0.0f, 0.18f * alpha_mul};
        ctx.draw_pointer_reveal(r, m_reveal_cursor, kRevealRadius, surface, edge, corner_radius, 1.0f);
    }

    void paint_chrome_button(PaintContext& ctx, Rect r, ChromeButton btn,
                             const ResolvedNavigationViewStyle& style, bool enabled) const;

    void paint_compact_tooltip(PaintContext& ctx, float pane_w, float item_cy,
                               const std::string& label, const ResolvedNavigationViewStyle& style) const;

    void paint_left_pane(PaintContext& ctx, const ResolvedNavigationViewStyle& style, NavigationPaneDisplayMode eff);

    void paint_left_row(PaintContext& ctx, Rect r, const FlatRow& row,
                        const ResolvedNavigationViewStyle& style,
                        bool is_selected, bool is_hov, bool is_prs, float label_alpha) const;

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

    void paint_top_nav(PaintContext& ctx, const ResolvedNavigationViewStyle& style);

    ChromeButton hit_test_chrome(PointF p, NavigationPaneDisplayMode eff) const noexcept;

    void hit_test_left_row(PointF p, NavigationPaneDisplayMode eff, int& out_idx, bool& out_footer, float* out_y = nullptr) const noexcept {
        out_idx = -1;
        out_footer = false;
        const float pane_w = current_pane_width(eff);
        if (p.x < 0.0f || p.x > pane_w) return;

        Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
        const bool compact_hide = (m_pane_open_anim < 0.35f);

        if (menu_vp.contains(p)) {
            float y = menu_vp.min_y + 2.0f - m_menu_scroll.offset();
            const int main_count = static_cast<int>(m_main_rows.size());
            for (int i = 0; i < main_count; ) {
                const auto& row = m_main_rows[static_cast<size_t>(i)];
                if (!row.item) { ++i; continue; }
                if (compact_hide && (row.depth > 0 || row.item->kind == NavigationItemKind::Header)) { ++i; continue; }

                if (row.depth > 0 && row.parent) {
                    const NavigationViewItem* grp_parent = row.parent;
                    int j = i;
                    float full_h = 0.0f;
                    while (j < main_count && m_main_rows[static_cast<size_t>(j)].depth > 0 &&
                           m_main_rows[static_cast<size_t>(j)].parent == grp_parent) {
                        full_h += m_main_rows[static_cast<size_t>(j)].height + 2.0f;
                        ++j;
                    }
                    float ef = get_expand_anim(grp_parent->effective_id(), grp_parent->expanded);
                    if (ef > 0.004f) {
                        float vis_h = full_h * ef;
                        if (p.y >= y && p.y < y + vis_h) {
                            float slide_up = (1.0f - ef) * std::min(16.0f, full_h * 0.25f);
                            float cy = y - slide_up;
                            for (int k = i; k < j; ++k) {
                                const auto& crow = m_main_rows[static_cast<size_t>(k)];
                                if (crow.item && p.y >= cy && p.y <= cy + crow.height) {
                                    if (crow.item->kind == NavigationItemKind::Item && ef > 0.5f) {
                                        out_idx = k;
                                        out_footer = false;
                                        if (out_y) *out_y = cy;
                                    }
                                    return;
                                }
                                cy += crow.height + 2.0f;
                            }
                            return;
                        }
                        y += vis_h;
                    }
                    i = j;
                    continue;
                }

                if (p.y >= y && p.y <= y + row.height) {
                    if (row.item->kind == NavigationItemKind::Item) {
                        out_idx = i;
                        out_footer = false;
                        if (out_y) *out_y = y;
                    }
                    return;
                }
                y += row.height + 2.0f;
                ++i;
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
                       bool is_settings, PointF flyout_anchor, bool flyout_below);

    void setup_behavior();

    void update_animations();

    NavigationViewData m_data;
    NavigationViewItem m_settings_item;
    std::vector<FlatRow> m_main_rows;
    std::vector<FlatRow> m_footer_rows;
    std::vector<std::pair<std::string, float>> m_expand_anims;
    std::vector<std::string> m_history;

    WidgetPtr m_content_widget;
    std::shared_ptr<RenderBox> m_content_box;
    Fn<WidgetPtr(const std::string&)> m_page_builder;

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
    bool m_has_reveal_cursor{false};
    PointF m_reveal_cursor{0.0f, 0.0f};

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

} // namespace ktui

#include "navigation_view_paint.hpp"
#include "navigation_view_behavior.hpp"
#include "navigation_view_widget.hpp"
