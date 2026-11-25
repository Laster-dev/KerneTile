#pragma once

#include "common/fluent_icons.hpp"
#include "menu_flyout.hpp"
#include "widget.hpp"
#include <ktui/app/window.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
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

struct BreadcrumbItem {
    std::string id;
    std::string label;
    std::string svg_path;
    std::vector<std::string> siblings{}; // 可选：同级兄弟节点，点击段右侧 › / ▾ 弹出 MenuFlyout 快速横跳

    bool operator==(const BreadcrumbItem&) const = default;
};

// WinUI 3 BreadcrumbBar 面包屑导航渲染盒
// 支持层级点击回溯、Fluent 右箭头分隔符、末项加亮、窄宽度自动折叠前段路径至 "..." 浮层菜单，
// 以及可选的路径段同级兄弟节点下拉横跳（点击 › / ▾ 弹出 MenuFlyout）。
class RenderBreadcrumbBar : public RenderBox {
public:
    RenderBreadcrumbBar() noexcept {
        setup_behavior();
    }

    ~RenderBreadcrumbBar() override = default;

    void set_properties(std::vector<BreadcrumbItem> items,
                        float font_size,
                        Fn<void(size_t, const BreadcrumbItem&)> on_item_clicked,
                        Fn<void(size_t, const std::string&)> on_sibling_selected = nullptr) {
        if (m_items != items || std::abs(m_font_size - font_size) > 0.001f) {
            m_items = std::move(items);
            m_font_size = font_size;
            m_hovered_slot = -1;
            m_hovered_chev = false;
            m_pressed_slot = -1;
            m_pressed_chev = false;
            mark_needs_layout();
        }
        m_on_item_clicked = std::move(on_item_clicked);
        m_on_sibling_selected = std::move(on_sibling_selected);
    }

    void set_items_value(const std::vector<BreadcrumbItem>& items) {
        if (m_items != items) {
            m_items = items;
            m_hovered_slot = -1;
            m_hovered_chev = false;
            m_pressed_slot = -1;
            m_pressed_chev = false;
            mark_needs_layout();
        }
    }

    [[nodiscard]] const std::vector<BreadcrumbItem>& items() const noexcept { return m_items; }
    [[nodiscard]] size_t collapsed_count() const noexcept { return m_collapsed_count; }

    [[nodiscard]] std::string full_path(const std::string& sep = "/") const {
        std::string out;
        for (size_t i = 0; i < m_items.size(); ++i) {
            if (i > 0) out += sep;
            out += m_items[i].label;
        }
        return out;
    }

    void perform_layout() override {
        float max_w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 600.0f;
        float h = std::max(32.0f, m_font_size + 16.0f);
        rebuild_layout(max_w, h);
        float used_w = 0.0f;
        if (!m_slots.empty()) {
            const auto& back = m_slots.back();
            used_w = back.chev_rect.is_empty() ? back.rect.max_x : back.chev_rect.max_x;
        }
        float final_w = std::isfinite(m_constraints.max_width) ? std::min(max_w, std::max(used_w, m_constraints.min_width)) : used_w;
        set_size(m_constraints.constrain(Vec2{final_w, h}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float h = size().y;

        Color active_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.96f} : Color{0.0f, 0.0f, 0.0f, 0.92f};
        Color link_col   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.68f} : Color{0.0f, 0.0f, 0.0f, 0.62f};
        Color chev_col   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.48f} : Color{0.0f, 0.0f, 0.0f, 0.44f};
        Color hov_bg     = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.065f} : Color{0.0f, 0.0f, 0.0f, 0.045f};
        Color prs_bg     = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.04f}  : Color{0.0f, 0.0f, 0.0f, 0.028f};

        for (size_t i = 0; i < m_slots.size(); ++i) {
            const auto& slot = m_slots[i];
            bool is_last = (!slot.is_ellipsis && slot.item_idx + 1 == m_items.size());
            bool hov_lbl = (m_hovered_slot == static_cast<int>(i) && !m_hovered_chev);
            bool prs_lbl = (m_pressed_slot == static_cast<int>(i) && !m_pressed_chev);

            if (!is_last) {
                if (prs_lbl) ctx.draw_rect(slot.rect, prs_bg, 4.0f, 0.0f);
                else if (hov_lbl) ctx.draw_rect(slot.rect, hov_bg, 4.0f, 0.0f);
            }

            Color tc = (is_last || hov_lbl) ? active_col : link_col;

            if (slot.is_ellipsis) {
                Vec2 es = TextMeasure::measure("...", m_font_size);
                ctx.draw_text("...",
                              PointF{std::round(slot.rect.min_x + (slot.rect.width() - es.x) * 0.5f),
                                     std::round((h - es.y) * 0.5f)},
                              m_font_size, tc);
            } else {
                const auto& item = m_items[slot.item_idx];
                float tx = slot.rect.min_x + 6.0f;
                if (!item.svg_path.empty()) {
                    float isz = std::clamp(m_font_size, 13.0f, 16.0f);
                    Rect ir = Rect::from_xywh(std::round(tx), std::round((h - isz) * 0.5f), isz, isz);
                    SvgEngine::draw_svg_path(ctx, item.svg_path, 24.0f, 24.0f, ir, tc);
                    tx += isz + 6.0f;
                }
                Vec2 ts = TextMeasure::measure(item.label, m_font_size);
                ctx.draw_text(item.label, PointF{std::round(tx), std::round((h - ts.y) * 0.5f)}, m_font_size, tc);
            }

            // 绘制分隔右箭头或末项兄弟节点下拉箭头
            if (!slot.chev_rect.is_empty()) {
                bool has_sibs = (!slot.is_ellipsis && slot.item_idx < m_items.size() && !m_items[slot.item_idx].siblings.empty());
                bool hov_c = (has_sibs && m_hovered_slot == static_cast<int>(i) && m_hovered_chev);
                bool prs_c = (has_sibs && m_pressed_slot == static_cast<int>(i) && m_pressed_chev);
                if (prs_c) ctx.draw_rect(slot.chev_rect, prs_bg, 4.0f, 0.0f);
                else if (hov_c) ctx.draw_rect(slot.chev_rect, hov_bg, 4.0f, 0.0f);

                const char* icon = (is_last && has_sibs) ? fluent_icons::kChevronDown : fluent_icons::kChevronRight;
                Color cc = hov_c ? theme.colors.primary : chev_col;
                fluent_icons::draw_centered(ctx, icon, slot.chev_rect, 11.0f, cc);
            }
        }
    }

private:
    static constexpr float kChevGap = 18.0f;

    struct Slot {
        bool is_ellipsis{false};
        size_t item_idx{0};
        Rect rect;
        Rect chev_rect;
    };

    [[nodiscard]] float measure_item_w(const BreadcrumbItem& item) const {
        float w = 12.0f + std::ceil(TextMeasure::measure(item.label, m_font_size).x);
        if (!item.svg_path.empty()) w += std::clamp(m_font_size, 13.0f, 16.0f) + 6.0f;
        return w;
    }

    void rebuild_layout(float max_w, float h) {
        m_slots.clear();
        m_collapsed_count = 0;
        if (m_items.empty()) return;

        const bool last_has_sibs = !m_items.back().siblings.empty();
        std::vector<float> widths(m_items.size(), 0.0f);
        float total = 0.0f;
        for (size_t i = 0; i < m_items.size(); ++i) {
            widths[i] = measure_item_w(m_items[i]);
            total += widths[i] + (i > 0 ? kChevGap : 0.0f);
        }
        if (last_has_sibs) total += kChevGap;

        size_t start_idx = 0;
        const float ellipsis_w = 28.0f;
        if (total > max_w && m_items.size() > 1) {
            // 从最左侧开始折叠前段路径，直到剩余路径 + "..." 放得下 (至少保留最后 1 项)
            float rem = widths.back() + (last_has_sibs ? kChevGap : 0.0f);
            size_t first_keep = m_items.size() - 1;
            for (size_t i = m_items.size() - 1; i > 0; --i) {
                float cand = rem + kChevGap + widths[i - 1];
                if (cand + kChevGap + ellipsis_w <= max_w) {
                    rem = cand;
                    first_keep = i - 1;
                } else {
                    break;
                }
            }
            if (first_keep > 0) {
                start_idx = first_keep;
                m_collapsed_count = first_keep;
            }
        }

        float cur_x = 0.0f;
        if (m_collapsed_count > 0) {
            Rect er = Rect::from_xywh(cur_x, 2.0f, ellipsis_w, h - 4.0f);
            Rect cr = Rect::from_xywh(cur_x + ellipsis_w, 2.0f, kChevGap, h - 4.0f);
            m_slots.push_back(Slot{true, 0, er, cr});
            cur_x += ellipsis_w + kChevGap;
        }
        for (size_t i = start_idx; i < m_items.size(); ++i) {
            Rect ir = Rect::from_xywh(cur_x, 2.0f, widths[i], h - 4.0f);
            cur_x += widths[i];
            Rect cr{};
            if (i + 1 < m_items.size() || !m_items[i].siblings.empty()) {
                cr = Rect::from_xywh(cur_x, 2.0f, kChevGap, h - 4.0f);
                cur_x += kChevGap;
            }
            m_slots.push_back(Slot{false, i, ir, cr});
        }
    }

    void open_collapsed_menu(const Rect& ell_rect) {
        Window* win = Window::active_window();
        if (!win || m_collapsed_count == 0) return;

        std::vector<MenuFlyoutItem> menu_items;
        for (size_t i = 0; i < m_collapsed_count && i < m_items.size(); ++i) {
            const auto& item = m_items[i];
            MenuFlyoutItem mi;
            mi.label = item.label;
            mi.svg_path = item.svg_path;
            size_t idx = i;
            mi.on_click = [this, idx]() {
                if (idx < m_items.size() && m_on_item_clicked) {
                    m_on_item_clicked(idx, m_items[idx]);
                }
            };
            menu_items.push_back(std::move(mi));
        }

        Vec2 g = global_offset();
        g.y += win->titlebar_height();
        MenuFlyout::show_at(menu_items, Vec2{g.x + ell_rect.min_x, g.y + size().y + 4.0f});
    }

    void open_sibling_menu(size_t item_idx, const Rect& anchor) {
        Window* win = Window::active_window();
        if (!win || item_idx >= m_items.size()) return;
        const auto& sibs = m_items[item_idx].siblings;
        if (sibs.empty()) return;

        std::vector<MenuFlyoutItem> menu_items;
        menu_items.reserve(sibs.size());
        for (const auto& sib : sibs) {
            MenuFlyoutItem mi;
            bool is_cur = (sib == m_items[item_idx].label);
            mi.label = is_cur ? ("✓ " + sib) : sib;
            std::string chosen = sib;
            mi.on_click = [this, item_idx, chosen]() {
                if (item_idx >= m_items.size()) return;
                m_items[item_idx].id = chosen;
                m_items[item_idx].label = chosen;
                if (item_idx + 1 < m_items.size()) m_items.resize(item_idx + 1);
                mark_needs_layout();
                mark_needs_paint();
                if (m_on_sibling_selected) m_on_sibling_selected(item_idx, chosen);
            };
            menu_items.push_back(std::move(mi));
        }

        Vec2 g = global_offset();
        g.y += win->titlebar_height();
        MenuFlyout::show_at(menu_items, Vec2{g.x + anchor.min_x, g.y + size().y + 4.0f});
    }

    void hit_test_slot(PointF p, int& out_slot, bool& out_chev) const noexcept {
        out_slot = -1;
        out_chev = false;
        for (size_t i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].rect.contains(p)) {
                out_slot = static_cast<int>(i);
                out_chev = false;
                return;
            }
            if (!m_slots[i].chev_rect.is_empty() && m_slots[i].chev_rect.contains(p)) {
                if (!m_slots[i].is_ellipsis && m_slots[i].item_idx < m_items.size() &&
                    !m_items[m_slots[i].item_idx].siblings.empty()) {
                    out_slot = static_cast<int>(i);
                    out_chev = true;
                    return;
                }
            }
        }
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int hs = -1;
            bool hc = false;
            hit_test_slot(e.local_position, hs, hc);
            if (hs >= 0 && !hc && !m_slots[static_cast<size_t>(hs)].is_ellipsis &&
                m_slots[static_cast<size_t>(hs)].item_idx + 1 == m_items.size()) {
                hs = -1; // 末项标签本身不可点击（但末项右侧兄弟下拉箭头可点击）
            }
            if (hs != m_hovered_slot || hc != m_hovered_chev) {
                m_hovered_slot = hs;
                m_hovered_chev = hc;
                cursor_shape = (hs >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_slot != -1 || m_pressed_slot != -1) {
                m_hovered_slot = -1;
                m_hovered_chev = false;
                m_pressed_slot = -1;
                m_pressed_chev = false;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int hs = -1;
            bool hc = false;
            hit_test_slot(e.local_position, hs, hc);
            if (hs >= 0) {
                const auto& slot = m_slots[static_cast<size_t>(hs)];
                if (hc || slot.is_ellipsis || slot.item_idx + 1 < m_items.size()) {
                    m_pressed_slot = hs;
                    m_pressed_chev = hc;
                    mark_needs_paint();
                    e.stop_propagation();
                }
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            int ps = m_pressed_slot;
            bool pc = m_pressed_chev;
            m_pressed_slot = -1;
            m_pressed_chev = false;
            int hs = -1;
            bool hc = false;
            hit_test_slot(e.local_position, hs, hc);
            if (ps >= 0 && hs == ps && hc == pc && ps < static_cast<int>(m_slots.size())) {
                const auto& slot = m_slots[static_cast<size_t>(ps)];
                if (pc) {
                    open_sibling_menu(slot.item_idx, slot.chev_rect);
                } else if (slot.is_ellipsis) {
                    open_collapsed_menu(slot.rect);
                } else if (slot.item_idx < m_items.size() && m_on_item_clicked) {
                    m_on_item_clicked(slot.item_idx, m_items[slot.item_idx]);
                }
                e.stop_propagation();
            }
            mark_needs_paint();
        };
    }

    std::vector<BreadcrumbItem> m_items;
    float m_font_size{14.0f};
    Fn<void(size_t, const BreadcrumbItem&)> m_on_item_clicked;
    Fn<void(size_t, const std::string&)> m_on_sibling_selected;

    std::vector<Slot> m_slots;
    size_t m_collapsed_count{0};
    int m_hovered_slot{-1};
    bool m_hovered_chev{false};
    int m_pressed_slot{-1};
    bool m_pressed_chev{false};
};

// C++20 指定初始化器声明式 WinUI 3 BreadcrumbBar
struct BreadcrumbBar : BindableMixin<BreadcrumbBar, RenderBreadcrumbBar> {
    std::vector<BreadcrumbItem> items{};
    float font_size{14.0f};
    Fn<void(size_t, const BreadcrumbItem&)> on_item_clicked{nullptr};
    Fn<void(size_t, const std::string&)> on_sibling_selected{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class BreadcrumbBarWidget : public Widget {
public:
    explicit BreadcrumbBarWidget(const BreadcrumbBar& desc) : m_desc(desc) {}
    explicit BreadcrumbBarWidget(BreadcrumbBar&& desc) noexcept : m_desc(std::move(desc)) {}
    ~BreadcrumbBarWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderBreadcrumbBar>();
        node->set_properties(m_desc.items, m_desc.font_size, m_desc.on_item_clicked, m_desc.on_sibling_selected);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto bb = std::static_pointer_cast<RenderBreadcrumbBar>(node)) {
            bb->set_properties(m_desc.items, m_desc.font_size, m_desc.on_item_clicked, m_desc.on_sibling_selected);
        }
    }

private:
    BreadcrumbBar m_desc;
};

inline BreadcrumbBar::operator WidgetPtr() const& {
    return make_widget<BreadcrumbBarWidget>(*this);
}

inline BreadcrumbBar::operator WidgetPtr() && {
    return make_widget<BreadcrumbBarWidget>(std::move(*this));
}

} // namespace ktui
