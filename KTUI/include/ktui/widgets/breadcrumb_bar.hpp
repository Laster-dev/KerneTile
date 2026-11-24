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

    bool operator==(const BreadcrumbItem&) const = default;
};

// WinUI 3 BreadcrumbBar 面包屑导航渲染盒
// 支持层级点击回溯、Fluent 右箭头分隔符、末项加亮，以及窄宽度自动折叠前段路径至 "..." 浮层菜单
class RenderBreadcrumbBar : public RenderBox {
public:
    RenderBreadcrumbBar() noexcept {
        setup_behavior();
    }

    ~RenderBreadcrumbBar() override = default;

    void set_properties(std::vector<BreadcrumbItem> items,
                        float font_size,
                        std::function<void(size_t, const BreadcrumbItem&)> on_item_clicked) {
        if (m_items != items || std::abs(m_font_size - font_size) > 0.001f) {
            m_items = std::move(items);
            m_font_size = font_size;
            m_hovered_slot = -1;
            m_pressed_slot = -1;
            mark_needs_layout();
        }
        m_on_item_clicked = std::move(on_item_clicked);
    }

    void set_items_value(const std::vector<BreadcrumbItem>& items) {
        if (m_items != items) {
            m_items = items;
            m_hovered_slot = -1;
            m_pressed_slot = -1;
            mark_needs_layout();
        }
    }

    [[nodiscard]] size_t collapsed_count() const noexcept { return m_collapsed_count; }

    void perform_layout() override {
        float max_w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 600.0f;
        float h = std::max(32.0f, m_font_size + 16.0f);
        rebuild_layout(max_w, h);
        float used_w = m_slots.empty() ? 0.0f : m_slots.back().rect.max_x;
        float final_w = std::isfinite(m_constraints.max_width) ? std::min(max_w, std::max(used_w, m_constraints.min_width)) : used_w;
        set_size(m_constraints.constrain(Vec2{final_w, h}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float h = size().y;

        Color active_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.96f} : Color{0.0f, 0.0f, 0.0f, 0.92f};
        Color link_col   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.68f} : Color{0.0f, 0.0f, 0.0f, 0.62f};
        Color chev_col   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.42f} : Color{0.0f, 0.0f, 0.0f, 0.38f};
        Color hov_bg     = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.065f} : Color{0.0f, 0.0f, 0.0f, 0.045f};
        Color prs_bg     = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.04f}  : Color{0.0f, 0.0f, 0.0f, 0.028f};

        for (size_t i = 0; i < m_slots.size(); ++i) {
            const auto& slot = m_slots[i];
            bool is_last = (!slot.is_ellipsis && slot.item_idx + 1 == m_items.size());
            bool hov = (m_hovered_slot == static_cast<int>(i));
            bool prs = (m_pressed_slot == static_cast<int>(i));

            if (!is_last) {
                if (prs) ctx.draw_rect(slot.rect, prs_bg, 4.0f, 0.0f);
                else if (hov) ctx.draw_rect(slot.rect, hov_bg, 4.0f, 0.0f);
            }

            Color tc = (is_last || hov) ? active_col : link_col;

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

            // 绘制分隔右箭头 (最后一项后不画)
            if (i + 1 < m_slots.size()) {
                Rect gap_r = Rect::from_xywh(slot.rect.max_x, 0.0f, kChevGap, h);
                fluent_icons::draw_centered(ctx, fluent_icons::kChevronRight, gap_r, 11.0f, chev_col);
            }
        }
    }

private:
    static constexpr float kChevGap = 18.0f;

    struct Slot {
        bool is_ellipsis{false};
        size_t item_idx{0};
        Rect rect;
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

        std::vector<float> widths(m_items.size(), 0.0f);
        float total = 0.0f;
        for (size_t i = 0; i < m_items.size(); ++i) {
            widths[i] = measure_item_w(m_items[i]);
            total += widths[i] + (i > 0 ? kChevGap : 0.0f);
        }

        size_t start_idx = 0;
        const float ellipsis_w = 28.0f;
        if (total > max_w && m_items.size() > 1) {
            // 从最左侧开始折叠前段路径，直到剩余路径 + "..." 放得下 (至少保留最后 1 项)
            float rem = widths.back();
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
            m_slots.push_back(Slot{true, 0, Rect::from_xywh(cur_x, 2.0f, ellipsis_w, h - 4.0f)});
            cur_x += ellipsis_w + kChevGap;
        }
        for (size_t i = start_idx; i < m_items.size(); ++i) {
            m_slots.push_back(Slot{false, i, Rect::from_xywh(cur_x, 2.0f, widths[i], h - 4.0f)});
            cur_x += widths[i] + kChevGap;
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

    int hit_test_slot(PointF p) const noexcept {
        for (size_t i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].rect.contains(p)) return static_cast<int>(i);
        }
        return -1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int hs = hit_test_slot(e.local_position);
            if (hs >= 0 && !m_slots[static_cast<size_t>(hs)].is_ellipsis &&
                m_slots[static_cast<size_t>(hs)].item_idx + 1 == m_items.size()) {
                hs = -1; // 末项不可点击
            }
            if (hs != m_hovered_slot) {
                m_hovered_slot = hs;
                cursor_shape = (hs >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_slot != -1 || m_pressed_slot != -1) {
                m_hovered_slot = -1;
                m_pressed_slot = -1;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int hs = hit_test_slot(e.local_position);
            if (hs >= 0) {
                const auto& slot = m_slots[static_cast<size_t>(hs)];
                if (slot.is_ellipsis || slot.item_idx + 1 < m_items.size()) {
                    m_pressed_slot = hs;
                    mark_needs_paint();
                    e.stop_propagation();
                }
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            int ps = m_pressed_slot;
            m_pressed_slot = -1;
            if (ps >= 0 && hit_test_slot(e.local_position) == ps && ps < static_cast<int>(m_slots.size())) {
                const auto& slot = m_slots[static_cast<size_t>(ps)];
                if (slot.is_ellipsis) {
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
    std::function<void(size_t, const BreadcrumbItem&)> m_on_item_clicked;

    std::vector<Slot> m_slots;
    size_t m_collapsed_count{0};
    int m_hovered_slot{-1};
    int m_pressed_slot{-1};
};

// C++20 指定初始化器声明式 WinUI 3 BreadcrumbBar
struct BreadcrumbBar {
    Bindable<std::vector<BreadcrumbItem>> items{};
    float font_size{14.0f};
    std::function<void(size_t, const BreadcrumbItem&)> on_item_clicked{nullptr};

    operator WidgetPtr() const;
};

class BreadcrumbBarWidget : public Widget {
public:
    explicit BreadcrumbBarWidget(const BreadcrumbBar& desc) : m_desc(desc) {}
    ~BreadcrumbBarWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderBreadcrumbBar>();
        node->set_properties(m_desc.items.get(), m_desc.font_size, m_desc.on_item_clicked);
        bind(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto bb = std::dynamic_pointer_cast<RenderBreadcrumbBar>(node)) {
            bb->set_properties(m_desc.items.get(), m_desc.font_size, m_desc.on_item_clicked);
            bind(bb);
        }
    }

private:
    void bind(const std::shared_ptr<RenderBreadcrumbBar>& bb) {
        m_bindings.clear();
        m_bindings.attach(m_desc.items, bb, [](RenderBreadcrumbBar& n, const std::vector<BreadcrumbItem>& v) {
            n.set_items_value(v);
        });
    }

    BreadcrumbBar m_desc;
    BindingGroup m_bindings;
};

inline BreadcrumbBar::operator WidgetPtr() const {
    return std::make_shared<BreadcrumbBarWidget>(*this);
}

} // namespace ktui
