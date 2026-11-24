#pragma once

#include "navigation_view_data.hpp"
#include "navigation_view_style.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
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

// WinUI 3 NavigationView 核心渲染与交互盒
class RenderNavigationView : public RenderBox {
public:
    RenderNavigationView() noexcept {
        is_focusable = true;
        setup_behavior();
    }

    ~RenderNavigationView() override = default;

    [[nodiscard]] const std::string& selected_id() const noexcept { return m_data.selected_id; }
    [[nodiscard]] bool is_pane_open() const noexcept { return m_data.is_pane_open; }
    [[nodiscard]] NavigationPaneDisplayMode pane_display_mode() const noexcept { return m_data.pane_display_mode; }
    [[nodiscard]] NavigationPaneDisplayMode effective_display_mode() const noexcept { return compute_effective_mode(size().x); }
    [[nodiscard]] bool can_go_back() const noexcept { return m_data.is_back_enabled || m_history.size() > 1; }
    [[nodiscard]] const std::shared_ptr<RenderBox>& content_box() const noexcept { return m_content_box; }

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
        mark_needs_layout();
    }

    void set_footer_items(std::vector<NavigationViewItem> items) {
        m_data.footer_items = std::move(items);
        rebuild_flat_items();
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
            m_data.pane_display_mode = mode;
            m_last_auto_mode = NavigationPaneDisplayMode::Auto;
            // 切换到 Compact / Minimal 时默认收起浮层，切换到 Left 时默认展开
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

    // 持久化持有当前页面 Widget 树，保证页面内控件的 BindingGroup 与回调生命周期与页面一致
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

        // 响应式 Auto 断点自动调整面板默认开合状态
        NavigationPaneDisplayMode eff = compute_effective_mode(w);
        if (m_data.pane_display_mode == NavigationPaneDisplayMode::Auto && eff != m_last_auto_mode) {
            m_last_auto_mode = eff;
            bool should_open = (eff == NavigationPaneDisplayMode::Left);
            m_data.is_pane_open = should_open;
            m_pane_open_anim = should_open ? 1.0f : 0.0f;
        }

        // 更新左侧菜单滚动上限
        if (eff != NavigationPaneDisplayMode::Top) {
            Rect menu_vp = compute_left_menu_viewport(h, current_pane_width(eff));
            float total_menu_h = compute_left_menu_total_height();
            m_menu_scroll.set_max_scroll(std::max(0.0f, total_menu_h - menu_vp.height()));
        }

        // 布局右侧/下方内容区域
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

        // 若点在导航栏或浮层面板区域内，直接由 NavigationView 拦截
        if (eff == NavigationPaneDisplayMode::Top) {
            if (local_position.y <= 44.0f) {
                result.add(this, local_position);
                return true;
            }
        } else {
            if (local_position.x <= pane_w || overlay_active) {
                result.add(this, local_position);
                return true;
            }
            if (eff == NavigationPaneDisplayMode::LeftMinimal && local_position.x <= 88.0f && local_position.y <= 44.0f) {
                result.add(this, local_position);
                return true;
            }
        }

        // 否则转发命中测试到内容页
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

        // 1. 先绘制内容区卡片表面与当前页面内容
        paint_content_area(ctx, style, eff);

        // 2. 再绘制导航栏 (使浮层模式下的侧边栏自然盖在内容区上方)
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
        if (eff == NavigationPaneDisplayMode::Top) return 0.0f;
        if (eff == NavigationPaneDisplayMode::LeftMinimal) return 0.0f;
        if (eff == NavigationPaneDisplayMode::LeftCompact) return m_data.compact_pane_length;
        // Left 模式平滑推动内容区
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

    [[nodiscard]] float compute_left_chrome_height() const noexcept {
        bool has_top_btn = m_data.is_back_button_visible || m_data.is_pane_toggle_button_visible;
        return has_top_btn ? 44.0f : 8.0f;
    }

    [[nodiscard]] float compute_left_footer_height() const noexcept {
        if (m_footer_rows.empty()) return 8.0f;
        float h = 13.0f; // 顶部分割线 + 留白
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
        const bool compact_hide_headers = (m_pane_open_anim < 0.35f);
        for (const auto& r : m_main_rows) {
            if (compact_hide_headers && r.item && r.item->kind == NavigationItemKind::Header) {
                continue;
            }
            if (compact_hide_headers && r.depth > 0) {
                continue;
            }
            y += r.height + 2.0f;
        }
        return y + 4.0f;
    }

    void paint_content_area(PaintContext& ctx, const ResolvedNavigationViewStyle& style, NavigationPaneDisplayMode eff) {
        Rect surf = compute_content_surface_rect(size().x, size().y, eff);

        // WinUI 3 右侧内容区 LayerFillColorDefault 圆角卡片 (左上角 8px 圆角)
        float tl_rad = (eff == NavigationPaneDisplayMode::Top || eff == NavigationPaneDisplayMode::LeftMinimal)
            ? 0.0f : style.content_radius;
        ctx.draw_rounded_rect(surf, style.content_surface_bg, tl_rad, 0.0f, 0.0f, 0.0f);

        // 顶部与左侧 1px 细腻边框
        if (style.content_surface_border.a > 0.001f) {
            ctx.draw_rect(Rect::from_xywh(surf.min_x + tl_rad, surf.min_y, std::max(0.0f, surf.width() - tl_rad), 1.0f),
                          style.content_surface_border, 0.0f, 0.0f);
            if (eff != NavigationPaneDisplayMode::Top && eff != NavigationPaneDisplayMode::LeftMinimal) {
                ctx.draw_rect(Rect::from_xywh(surf.min_x, surf.min_y + tl_rad, 1.0f, std::max(0.0f, surf.height() - tl_rad)),
                              style.content_surface_border, 0.0f, 0.0f);
            }
        }

        // 页面大标题 (Header)
        if (!m_data.header.empty()) {
            ctx.draw_text(m_data.header, PointF{surf.min_x + 24.0f, surf.min_y + 14.0f},
                          style.page_header_font_size, style.page_header_text);
        }

        // 页面内容 (附带 WinUI 3 EntranceNavigationTransition 10px 柔和上滑进场)
        if (m_content_box) {
            Rect body = compute_content_body_rect(size().x, size().y, eff);
            float ease = 1.0f - std::pow(1.0f - m_page_enter_progress, 3.0f);
            float slide_y = (1.0f - ease) * 10.0f;

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
        float cx = r.min_x + r.width() * 0.5f;
        float cy = r.min_y + r.height() * 0.5f;

        if (btn == ChromeButton::Back) {
            // WinUI 3 左箭头 (←)
            ctx.draw_line(PointF{cx - 5.5f, cy}, PointF{cx + 5.5f, cy}, col, 1.3f);
            ctx.draw_line(PointF{cx - 5.5f, cy}, PointF{cx - 1.2f, cy - 4.2f}, col, 1.3f);
            ctx.draw_line(PointF{cx - 5.5f, cy}, PointF{cx - 1.2f, cy + 4.2f}, col, 1.3f);
        } else if (btn == ChromeButton::Toggle) {
            // WinUI 3 汉堡三横线 (☰)
            ctx.draw_line(PointF{cx - 6.0f, cy - 4.0f}, PointF{cx + 6.0f, cy - 4.0f}, col, 1.25f);
            ctx.draw_line(PointF{cx - 6.0f, cy},        PointF{cx + 6.0f, cy},        col, 1.25f);
            ctx.draw_line(PointF{cx - 6.0f, cy + 4.0f}, PointF{cx + 6.0f, cy + 4.0f}, col, 1.25f);
        }
    }

    void paint_left_pane(PaintContext& ctx, const ResolvedNavigationViewStyle& style, NavigationPaneDisplayMode eff) {
        const float pane_w = current_pane_width(eff);
        const bool overlay = is_overlay_pane(eff) && m_pane_open_anim > 0.02f;

        // Minimal 模式收起态: 仅在左上角绘制 Back + Hamburger 按钮
        if (eff == NavigationPaneDisplayMode::LeftMinimal && m_pane_open_anim <= 0.02f) {
            float bx = 4.0f;
            if (m_data.is_back_button_visible) {
                paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f), ChromeButton::Back, style, can_go_back());
                bx += 40.0f;
            }
            if (m_data.is_pane_toggle_button_visible) {
                paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f), ChromeButton::Toggle, style, true);
            }
            return;
        }

        Rect pane_rect = Rect::from_xywh(0.0f, 0.0f, pane_w, size().y);

        // 浮层滑出态: 绘制柔和投影 + 亚克力实色面板 + 右侧圆角边框
        if (overlay) {
            Color sh = style.pane_overlay_shadow;
            sh.a *= m_pane_open_anim * 0.45f;
            ctx.draw_rect(Rect::from_xywh(pane_w, 0.0f, 8.0f, size().y), sh, 0.0f, 0.0f);
            ctx.draw_rounded_rect(pane_rect, style.pane_overlay_bg, 0.0f, 8.0f, 8.0f, 0.0f);
            ctx.draw_rect(Rect::from_xywh(pane_w - 1.0f, 0.0f, 1.0f, size().y), style.pane_overlay_border, 0.0f, 0.0f);
        }

        // 1. 顶部操作区 (Back + Toggle + PaneTitle)
        float bx = 4.0f;
        if (m_data.is_pane_toggle_button_visible) {
            if (m_data.is_back_button_visible && m_pane_open_anim > 0.5f) {
                paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f), ChromeButton::Back, style, can_go_back());
                bx += 40.0f;
            }
            paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f), ChromeButton::Toggle, style, true);
            bx += 42.0f;
        } else if (m_data.is_back_button_visible) {
            paint_chrome_button(ctx, Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f), ChromeButton::Back, style, can_go_back());
            bx += 42.0f;
        }

        if (!m_data.pane_title.empty() && m_pane_open_anim > 0.4f) {
            float label_alpha = std::clamp((m_pane_open_anim - 0.4f) / 0.6f, 0.0f, 1.0f);
            Color tc = style.item_selected_text;
            tc.a *= label_alpha;
            Vec2 ts = TextMeasure::measure(m_data.pane_title, 13.5f);
            ctx.push_clip(Rect::from_xywh(bx, 4.0f, std::max(0.0f, pane_w - bx - 8.0f), 36.0f), 0.0f);
            ctx.draw_text(m_data.pane_title, PointF{bx + 2.0f, 4.0f + (36.0f - ts.y) * 0.5f}, 13.5f, tc);
            ctx.pop_clip();
        }

        // 2. 可滚动主菜单区域
        Rect menu_vp = compute_left_menu_viewport(size().y, pane_w);
        const float scroll_y = m_menu_scroll.offset();
        const float total_menu_h = compute_left_menu_total_height();
        const float label_alpha = std::clamp((m_pane_open_anim - 0.35f) / 0.65f, 0.0f, 1.0f);

        float target_pill_y = -1.0f;
        float target_pill_h = 16.0f;

        ctx.push_clip(menu_vp, 0.0f);
        float y = menu_vp.min_y + 2.0f - scroll_y;
        for (int i = 0; i < static_cast<int>(m_main_rows.size()); ++i) {
            const auto& row = m_main_rows[static_cast<size_t>(i)];
            if (!row.item) continue;
            if (label_alpha <= 0.01f && (row.item->kind == NavigationItemKind::Header || row.depth > 0)) {
                continue;
            }

            Rect row_rect = Rect::from_xywh(4.0f, y, std::max(0.0f, pane_w - 8.0f), row.height);
            if (y + row.height >= menu_vp.min_y && y <= menu_vp.max_y) {
                bool is_sel = (row.item->kind == NavigationItemKind::Item &&
                               row.item->effective_id() == m_data.selected_id);
                if (is_sel) {
                    target_pill_y = y + (row.height - 16.0f) * 0.5f;
                    target_pill_h = 16.0f;
                }
                paint_left_row(ctx, row_rect, row, style, is_sel,
                               (m_hovered_row == i && !m_hovered_in_footer),
                               (m_pressed_row == i && !m_pressed_in_footer),
                               label_alpha);
            }
            y += row.height + 2.0f;
        }
        m_menu_scrollbar.paint(ctx, menu_vp, total_menu_h, scroll_y, ThemeContext::current());
        ctx.pop_clip();

        // 3. 底部固定 Footer 区域 (分割线 + footer_items + Settings)
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
                if (is_sel) {
                    target_pill_y = fy + (row.height - 16.0f) * 0.5f;
                    target_pill_h = 16.0f;
                }
                paint_left_row(ctx, row_rect, row, style, is_sel,
                               (m_hovered_row == i && m_hovered_in_footer),
                               (m_pressed_row == i && m_pressed_in_footer),
                               label_alpha);
                fy += row.height + 2.0f;
            }
        }

        // 4. WinUI 3 标志性 3px x 16px 滑动强调色选中指示条 (跨菜单与 Footer 丝滑跟随)
        if (target_pill_y >= 0.0f) {
            if (!m_pill_initialized) {
                m_pill_pos = target_pill_y;
                m_pill_size = target_pill_h;
                m_pill_initialized = true;
            }
            m_pill_target_pos = target_pill_y;
            m_pill_target_size = target_pill_h;
            ctx.draw_rect(Rect::from_xywh(4.0f, m_pill_pos, 3.0f, m_pill_size),
                          style.accent_pill, 1.5f, 0.0f);
        }
    }

    void paint_left_row(PaintContext& ctx, Rect r, const FlatRow& row,
                        const ResolvedNavigationViewStyle& style,
                        bool is_selected, bool is_hov, bool is_prs, float label_alpha) const {
        const auto& item = *row.item;
        if (item.kind == NavigationItemKind::Separator) {
            ctx.draw_rect(Rect::from_xywh(r.min_x + 8.0f, r.min_y + r.height() * 0.5f,
                                          std::max(0.0f, r.width() - 16.0f), 1.0f),
                          style.separator_color, 0.0f, 0.0f);
            return;
        }
        if (item.kind == NavigationItemKind::Header) {
            Color hc = style.header_text;
            hc.a *= label_alpha;
            Vec2 ts = TextMeasure::measure(item.label, style.header_font_size);
            ctx.draw_text(item.label, PointF{r.min_x + 12.0f, r.min_y + (r.height() - ts.y) * 0.5f + 3.0f},
                          style.header_font_size, hc);
            return;
        }

        // Item 背景
        if (!item.disabled) {
            if (is_prs) {
                ctx.draw_rect(r, style.item_pressed_bg, style.item_radius, 0.0f);
            } else if (is_hov) {
                ctx.draw_rect(r, style.item_hover_bg, style.item_radius, 0.0f);
            } else if (is_selected) {
                ctx.draw_rect(r, style.item_selected_bg, style.item_radius, 0.0f);
            }
        }

        const float indent = static_cast<float>(row.depth) * 24.0f;
        const float icon_cx = r.min_x + 20.0f + indent;
        const float icon_cy = r.min_y + r.height() * 0.5f;
        Rect icon_rect = Rect::from_xywh(icon_cx - 8.0f, icon_cy - 8.0f, 16.0f, 16.0f);
        Color ic = item.disabled ? style.item_disabled_text
                 : (is_selected ? style.icon_selected_color : style.icon_color);

        if (item.icon_painter) {
            item.icon_painter(ctx, icon_rect, ic);
        } else if (item.icon != NavigationIcon::None) {
            NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, icon_rect, ic);
        }

        // Compact 模式下徽标以精致小圆点提示在图标右上角
        if (!item.badge.empty() && label_alpha < 0.3f) {
            ctx.draw_rect(Rect::from_xywh(icon_cx + 4.0f, icon_cy - 8.0f, 6.0f, 6.0f),
                          style.badge_bg, 3.0f, 0.0f);
        }

        if (label_alpha > 0.01f) {
            float right_reserve = 8.0f;
            // 子项展开/收起箭头
            if (!item.children.empty()) {
                float cx = r.max_x - 14.0f;
                float cy = r.min_y + r.height() * 0.5f;
                float dir = item.expanded ? -1.0f : 1.0f;
                Color ch_col = ic;
                ch_col.a *= label_alpha * 0.75f;
                ctx.draw_line(PointF{cx - 3.2f, cy - 1.6f * dir}, PointF{cx, cy + 1.6f * dir}, ch_col, 1.2f);
                ctx.draw_line(PointF{cx, cy + 1.6f * dir}, PointF{cx + 3.2f, cy - 1.6f * dir}, ch_col, 1.2f);
                right_reserve += 20.0f;
            }

            // WinUI 3 InfoBadge 徽标胶囊
            if (!item.badge.empty()) {
                Vec2 bsz = TextMeasure::measure(item.badge, 10.5f);
                float bw = std::max(18.0f, bsz.x + 10.0f);
                float bh = 16.0f;
                float bx = r.max_x - right_reserve - bw;
                float by = r.min_y + (r.height() - bh) * 0.5f;
                Color bg = style.badge_bg;
                bg.a *= label_alpha;
                Color bt = style.badge_text;
                bt.a *= label_alpha;
                ctx.draw_rect(Rect::from_xywh(bx, by, bw, bh), bg, bh * 0.5f, 0.0f);
                ctx.draw_text(item.badge, PointF{bx + (bw - bsz.x) * 0.5f, by + (bh - bsz.y) * 0.5f}, 10.5f, bt);
                right_reserve += bw + 6.0f;
            }

            // 文本标签
            float text_x = (item.icon != NavigationIcon::None || item.icon_painter)
                ? (r.min_x + 40.0f + indent) : (r.min_x + 14.0f + indent);
            float clip_w = std::max(0.0f, r.max_x - right_reserve - text_x);
            if (clip_w > 2.0f) {
                Color tc = item.disabled ? style.item_disabled_text
                         : (is_selected ? style.item_selected_text : style.item_text);
                tc.a *= label_alpha;
                Vec2 ts = TextMeasure::measure(item.label, style.font_size);
                ctx.push_clip(Rect::from_xywh(text_x, r.min_y, clip_w, r.height()), 0.0f);
                ctx.draw_text(item.label, PointF{text_x, r.min_y + (r.height() - ts.y) * 0.5f}, style.font_size, tc);
                ctx.pop_clip();
            }
        }
    }

    // Top 水平导航栏布局与绘制
    struct TopItemLayout {
        const NavigationViewItem* item{nullptr};
        Rect rect;
    };

    std::vector<TopItemLayout> compute_top_layouts() const {
        std::vector<TopItemLayout> out;
        float x = 8.0f;
        if (m_data.is_back_button_visible) x += 42.0f;

        for (const auto& item : m_data.menu_items) {
            if (item.kind != NavigationItemKind::Item) continue;
            Vec2 ts = TextMeasure::measure(item.label, 13.0f);
            float icon_w = (item.icon != NavigationIcon::None || item.icon_painter) ? 22.0f : 0.0f;
            float w = ts.x + icon_w + 24.0f;
            out.push_back(TopItemLayout{&item, Rect::from_xywh(x, 4.0f, w, 36.0f)});
            x += w + 4.0f;
        }

        // 右对齐 Settings / Footer
        float rx = size().x - 8.0f;
        if (m_data.is_settings_visible) {
            float w = 38.0f;
            rx -= w;
            out.push_back(TopItemLayout{&m_settings_item, Rect::from_xywh(rx, 4.0f, w, 36.0f)});
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
            bool is_sel = (item.effective_id() == m_data.selected_id);
            bool is_hov = (m_hovered_top == i && !item.disabled);
            bool is_prs = (m_pressed_top == i && !item.disabled);

            if (is_prs) {
                ctx.draw_rect(tl.rect, style.item_pressed_bg, style.item_radius, 0.0f);
            } else if (is_hov) {
                ctx.draw_rect(tl.rect, style.item_hover_bg, style.item_radius, 0.0f);
            }

            if (is_sel) {
                target_pill_w = std::max(16.0f, tl.rect.width() - 24.0f);
                target_pill_x = tl.rect.min_x + (tl.rect.width() - target_pill_w) * 0.5f;
            }

            Color ic = item.disabled ? style.item_disabled_text
                     : (is_sel ? style.icon_selected_color : style.icon_color);
            bool icon_only = (item.effective_id() == m_data.settings_id);
            if (icon_only) {
                Rect ir = Rect::from_xywh(tl.rect.min_x + (tl.rect.width() - 16.0f) * 0.5f,
                                          tl.rect.min_y + (tl.rect.height() - 16.0f) * 0.5f, 16.0f, 16.0f);
                NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, ir, ic);
            } else {
                float tx = tl.rect.min_x + 12.0f;
                if (item.icon != NavigationIcon::None || item.icon_painter) {
                    Rect ir = Rect::from_xywh(tx, tl.rect.min_y + (tl.rect.height() - 16.0f) * 0.5f, 16.0f, 16.0f);
                    if (item.icon_painter) item.icon_painter(ctx, ir, ic);
                    else NavigationViewStyleResolver::paint_builtin_icon(ctx, item.icon, ir, ic);
                    tx += 22.0f;
                }
                Color tc = item.disabled ? style.item_disabled_text
                         : (is_sel ? style.item_selected_text : style.item_text);
                Vec2 ts = TextMeasure::measure(item.label, style.font_size);
                ctx.draw_text(item.label, PointF{tx, tl.rect.min_y + (tl.rect.height() - ts.y) * 0.5f},
                              style.font_size, tc);
            }
        }

        // Top 模式底部 3px 水平滑动强调条
        if (target_pill_x >= 0.0f) {
            if (!m_pill_initialized) {
                m_pill_pos = target_pill_x;
                m_pill_size = target_pill_w;
                m_pill_initialized = true;
            }
            m_pill_target_pos = target_pill_x;
            m_pill_target_size = target_pill_w;
            ctx.draw_rect(Rect::from_xywh(m_pill_pos, 41.0f, m_pill_size, 3.0f),
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
        float bx = 4.0f;
        if (eff == NavigationPaneDisplayMode::LeftMinimal && m_pane_open_anim <= 0.02f) {
            if (m_data.is_back_button_visible) {
                if (Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f).contains(p)) return ChromeButton::Back;
                bx += 40.0f;
            }
            if (m_data.is_pane_toggle_button_visible && Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f).contains(p)) {
                return ChromeButton::Toggle;
            }
            return ChromeButton::None;
        }
        if (m_data.is_pane_toggle_button_visible) {
            if (m_data.is_back_button_visible && m_pane_open_anim > 0.5f) {
                if (Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f).contains(p)) return ChromeButton::Back;
                bx += 40.0f;
            }
            if (Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f).contains(p)) return ChromeButton::Toggle;
        } else if (m_data.is_back_button_visible) {
            if (Rect::from_xywh(bx, 4.0f, 38.0f, 36.0f).contains(p)) return ChromeButton::Back;
        }
        return ChromeButton::None;
    }

    void hit_test_left_row(PointF p, NavigationPaneDisplayMode eff, int& out_idx, bool& out_footer) const noexcept {
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
                if (compact_hide && (row.item->kind == NavigationItemKind::Header || row.depth > 0)) continue;
                if (p.y >= y && p.y <= y + row.height) {
                    if (row.item->kind == NavigationItemKind::Item) {
                        out_idx = i;
                        out_footer = false;
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
                    }
                    return;
                }
                fy += row.height + 2.0f;
            }
        }
    }

    void activate_item(const NavigationViewItem& item, NavigationPaneDisplayMode eff) {
        if (item.disabled || item.kind != NavigationItemKind::Item) return;

        // 若含子项，先切换其展开/折叠状态 (或在紧凑模式下展开面板)
        if (!item.children.empty()) {
            if (m_pane_open_anim < 0.5f) {
                set_pane_open(true, true);
            }
            for (auto& mi : m_data.menu_items) {
                if (mi.effective_id() == item.effective_id()) {
                    mi.expanded = !mi.expanded;
                    rebuild_flat_items();
                    mark_needs_layout();
                    return;
                }
            }
        }

        set_selected_id(item.effective_id(), true, true);
        // 浮层模式下选中后自动收起侧边栏
        if (is_overlay_pane(eff) && m_data.is_pane_open) {
            set_pane_open(false, true);
        }
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            const NavigationPaneDisplayMode eff = compute_effective_mode(size().x);

            if (e.type == PlatformEventType::PointerScroll && eff != NavigationPaneDisplayMode::Top) {
                Rect menu_vp = compute_left_menu_viewport(size().y, current_pane_width(eff));
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
                if (cb == ChromeButton::None) {
                    auto layouts = compute_top_layouts();
                    for (int i = 0; i < static_cast<int>(layouts.size()); ++i) {
                        if (layouts[static_cast<size_t>(i)].rect.contains(e.local_position)) {
                            top_idx = i;
                            break;
                        }
                    }
                }
                if (cb != m_hovered_chrome || top_idx != m_hovered_top) {
                    m_hovered_chrome = cb;
                    m_hovered_top = top_idx;
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
            if (cb != m_hovered_chrome || r_idx != m_hovered_row || r_footer != m_hovered_in_footer) {
                m_hovered_chrome = cb;
                m_hovered_row = r_idx;
                m_hovered_in_footer = r_footer;
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_menu_scrollbar.handle_pointer_exit();
            if (m_hovered_chrome != ChromeButton::None || m_hovered_row != -1 || m_hovered_top != -1) {
                m_hovered_chrome = ChromeButton::None;
                m_hovered_row = -1;
                m_hovered_top = -1;
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
            // 浮层展开状态下点击面板外部直接收起
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
            if (m_menu_scrollbar.handle_pointer_up()) {
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
                    activate_item(*layouts[static_cast<size_t>(pressed)].item, eff);
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
                hit_test_left_row(e.local_position, eff, hr, hf);
                if (hr == pr && hf == pf) {
                    const auto& list = pf ? m_footer_rows : m_main_rows;
                    if (pr >= 0 && pr < static_cast<int>(list.size()) && list[static_cast<size_t>(pr)].item) {
                        activate_item(*list[static_cast<size_t>(pr)].item, eff);
                    }
                }
                mark_needs_paint();
                e.stop_propagation();
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
            // Left 嵌入模式宽度变化时同步刷新内容区布局
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

        // 2. WinUI 3 强调色选中条滑动跟随动画
        if (m_pill_initialized) {
            if (std::abs(m_pill_pos - m_pill_target_pos) > 0.25f) {
                m_pill_pos += (m_pill_target_pos - m_pill_pos) * std::min(1.0f, dt * 22.0f);
                animating = true;
            } else {
                m_pill_pos = m_pill_target_pos;
            }
            if (std::abs(m_pill_size - m_pill_target_size) > 0.25f) {
                m_pill_size += (m_pill_target_size - m_pill_size) * std::min(1.0f, dt * 22.0f);
                animating = true;
            } else {
                m_pill_size = m_pill_target_size;
            }
        }

        // 3. 页面切换进场动画 (140ms)
        if (m_page_enter_progress < 1.0f) {
            m_page_enter_progress = std::min(1.0f, m_page_enter_progress + dt / 0.14f);
            animating = true;
        }

        // 4. 菜单列表滚动与滚动条动画
        if (m_menu_scroll.update()) animating = true;
        if (m_menu_scrollbar.update_animations(m_hovered_row >= 0)) animating = true;

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

    float m_pane_open_anim{1.0f};
    bool m_pill_initialized{false};
    float m_pill_pos{0.0f};
    float m_pill_size{16.0f};
    float m_pill_target_pos{0.0f};
    float m_pill_target_size{16.0f};
    float m_page_enter_progress{1.0f};

    SmoothScrollPhysics m_menu_scroll;
    WinUIScrollBar m_menu_scrollbar;
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
