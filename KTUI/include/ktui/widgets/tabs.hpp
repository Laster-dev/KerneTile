#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <algorithm>

namespace ktui {

// 标签页条目定义 (TabItem)
struct TabItem {
    std::string title{""};
    WidgetPtr content{nullptr};
};

// 桌面高信息密度标签栏渲染盒 (RenderTabBar)
class RenderTabBar : public RenderBox {
public:
    RenderTabBar(std::vector<std::string> titles, size_t active_idx, float h,
                 std::function<void(size_t)> on_tab_click)
        : m_titles(std::move(titles)), m_active_index(active_idx),
          m_height(h), m_on_tab_click(std::move(on_tab_click)) {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    void set_active_index(size_t idx) {
        if (m_active_index != idx) {
            m_active_index = idx;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float total_w = 0.0f;
        m_tab_rects.clear();
        for (const auto& t : m_titles) {
            float tw = TextMeasure::measure_width(t, m_font_size) + 24.0f;
            m_tab_rects.push_back(Rect::from_xywh(total_w, 0.0f, tw, m_height));
            total_w += tw + 2.0f;
        }
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(total_w, 120.0f);
        m_size = Vec2{w, m_height};
    }

    void paint(PaintContext& context) override {
        // 底层底栏背景与下边框线
        context.draw_rect(bounds(), m_bg_color, 0.0f, 0.0f);
        context.draw_rect(Rect::from_xywh(0.0f, m_size.y - 1.0f, m_size.x, 1.0f), m_border_color, 0.0f, 0.0f);

        for (size_t i = 0; i < m_titles.size() && i < m_tab_rects.size(); ++i) {
            const auto& r = m_tab_rects[i];
            bool is_tab_active = (i == m_active_index);
            bool is_hov = (i == m_hovered_index);

            // 选项卡底色
            Color tab_bg = is_tab_active ? m_active_bg : (is_hov ? m_hover_bg : m_bg_color);
            context.draw_rect(Rect::from_xywh(r.min_x, r.min_y + 2.0f, r.width(), r.height() - 2.0f), tab_bg, 2.0f, 0.0f);

            // 活跃选项卡下部高亮指示条 (2px 桌面蓝条)
            if (is_tab_active) {
                context.draw_rect(Rect::from_xywh(r.min_x, r.max_y - 2.0f, r.width(), 2.0f), m_accent_color, 0.0f, 0.0f);
            }

            // 文本
            Color text_col = is_tab_active ? Color::white() : (is_hov ? Color{0.90f, 0.90f, 0.92f, 1.0f} : m_text_color);
            float text_y = (m_height - m_font_size * 1.30f) * 0.5f;
            float text_w = TextMeasure::measure_width(m_titles[i], m_font_size);
            float text_x = r.min_x + (r.width() - text_w) * 0.5f;
            context.draw_text(m_titles[i], PointF{text_x, text_y}, m_font_size, text_col);
        }
    }

private:
    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& ev) {
            size_t new_hov = static_cast<size_t>(-1);
            for (size_t i = 0; i < m_tab_rects.size(); ++i) {
                if (m_tab_rects[i].contains(ev.local_position)) {
                    new_hov = i;
                    break;
                }
            }
            if (new_hov != m_hovered_index) {
                m_hovered_index = new_hov;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            for (size_t i = 0; i < m_tab_rects.size(); ++i) {
                if (m_tab_rects[i].contains(ev.local_position)) {
                    set_active_index(i);
                    if (m_on_tab_click) {
                        m_on_tab_click(i);
                    }
                    break;
                }
            }
        };
    }

    std::vector<std::string> m_titles;
    size_t m_active_index{0};
    float m_height{28.0f};
    std::function<void(size_t)> m_on_tab_click{nullptr};
    std::vector<Rect> m_tab_rects;
    size_t m_hovered_index{static_cast<size_t>(-1)};
    float m_font_size{12.5f};

    Color m_bg_color{Color{0.12f, 0.12f, 0.14f, 1.0f}};
    Color m_hover_bg{Color{0.18f, 0.18f, 0.22f, 1.0f}};
    Color m_active_bg{Color{0.20f, 0.20f, 0.24f, 1.0f}};
    Color m_border_color{Color{0.24f, 0.24f, 0.28f, 1.0f}};
    Color m_accent_color{Color{0.20f, 0.55f, 0.95f, 1.0f}};
    Color m_text_color{Color{0.68f, 0.68f, 0.72f, 1.0f}};
};

// 桌面多标签视图容器渲染盒 (RenderTabsView)
class RenderTabsView : public RenderBox {
public:
    RenderTabsView(std::vector<TabItem> items, size_t active_idx, float tab_bar_h,
                   std::function<void(size_t)> on_change)
        : m_items(std::move(items)), m_active_index(active_idx),
          m_tab_bar_height(tab_bar_h), m_on_change(std::move(on_change)) {
        init_children();
    }

    void set_active_index(size_t idx) {
        if (idx < m_items.size() && m_active_index != idx) {
            m_active_index = idx;
            if (m_tab_bar) {
                m_tab_bar->set_active_index(idx);
            }
            update_active_content();
            mark_needs_layout();
        }
    }

    [[nodiscard]] size_t active_index() const noexcept { return m_active_index; }

    void perform_layout() override {
        Vec2 sz = m_constraints.constrain(Vec2{m_constraints.max_width, m_constraints.max_height});
        m_size = sz;

        if (m_tab_bar) {
            m_tab_bar->layout(BoxConstraints::tight(Vec2{sz.x, m_tab_bar_height}));
            m_tab_bar->set_offset(Vec2{0.0f, 0.0f});
        }

        if (m_content_box) {
            float content_h = std::max(0.0f, sz.y - m_tab_bar_height);
            m_content_box->layout(BoxConstraints::tight(Vec2{sz.x, content_h}));
            m_content_box->set_offset(Vec2{0.0f, m_tab_bar_height});
        }
    }

private:
    void init_children() {
        std::vector<std::string> titles;
        for (const auto& item : m_items) {
            titles.push_back(item.title);
        }
        m_tab_bar = std::make_shared<RenderTabBar>(titles, m_active_index, m_tab_bar_height, [this](size_t idx) {
            set_active_index(idx);
            if (m_on_change) m_on_change(idx);
        });
        append_child(m_tab_bar.get());
        update_active_content();
    }

    void update_active_content() {
        if (m_content_box) {
            remove_child(m_content_box.get());
            m_content_box = nullptr;
        }
        if (m_active_index < m_items.size() && m_items[m_active_index].content) {
            m_content_box = m_items[m_active_index].content->create_render_node();
            if (m_content_box) {
                append_child(m_content_box.get());
            }
        }
    }

    std::vector<TabItem> m_items;
    size_t m_active_index{0};
    float m_tab_bar_height{28.0f};
    std::function<void(size_t)> m_on_change{nullptr};

    std::shared_ptr<RenderTabBar> m_tab_bar{nullptr};
    std::shared_ptr<RenderBox> m_content_box{nullptr};
};

class TabsWidget;

// C++20 指定初始化器声明式标签页视图 (Tabs)
struct Tabs {
    std::vector<TabItem> tabs;
    size_t active_index{0};
    BindRef<size_t> bind_active_index{nullptr};
    float tab_bar_height{28.0f};
    std::function<void(size_t)> on_tab_changed{nullptr};

    operator WidgetPtr() const;
};

class TabsWidget : public Widget {
public:
    explicit TabsWidget(const Tabs& desc) : m_desc(desc) {}
    ~TabsWidget() override { unsubscribe(); }

    std::shared_ptr<RenderBox> create_render_node() override {
        size_t init_idx = m_desc.bind_active_index ? m_desc.bind_active_index->get() : m_desc.active_index;
        auto on_ch = [this, node_weak = std::weak_ptr<RenderTabsView>()](size_t idx) {
            if (m_desc.bind_active_index) {
                m_desc.bind_active_index->set(idx);
            }
            if (m_desc.on_tab_changed) {
                m_desc.on_tab_changed(idx);
            }
        };

        auto node = std::make_shared<RenderTabsView>(m_desc.tabs, init_idx, m_desc.tab_bar_height, on_ch);
        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto tabs_view = std::dynamic_pointer_cast<RenderTabsView>(node)) {
            size_t cur_idx = m_desc.bind_active_index ? m_desc.bind_active_index->get() : m_desc.active_index;
            tabs_view->set_active_index(cur_idx);
            setup_binding(tabs_view);
        }
    }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_active_index) {
            m_desc.bind_active_index->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderTabsView> node) {
        unsubscribe();
        if (m_desc.bind_active_index) {
            m_binding_token = m_desc.bind_active_index->subscribe([node_weak = std::weak_ptr<RenderTabsView>(node)](size_t idx) {
                if (auto n = node_weak.lock()) {
                    n->set_active_index(idx);
                }
            });
        }
    }

    Tabs m_desc;
    uint64_t m_binding_token{0};
};

inline Tabs::operator WidgetPtr() const {
    return std::make_shared<TabsWidget>(*this);
}

} // namespace ktui
