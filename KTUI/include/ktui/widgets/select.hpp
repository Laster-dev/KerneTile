#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/app/window.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <algorithm>

namespace ktui {

// 下拉选框选项渲染列表盒 (RenderSelectDropdown)
class RenderSelectDropdown : public RenderBox {
public:
    RenderSelectDropdown(std::vector<std::string> options, std::string selected,
                         float item_h, float font_sz,
                         std::function<void(const std::string&)> on_select,
                         std::function<void()> on_close)
        : m_options(std::move(options)), m_selected(std::move(selected)),
          m_item_height(item_h), m_font_size(font_sz),
          m_on_select(std::move(on_select)), m_on_close(std::move(on_close)) {
        setup_behavior();
    }

    void perform_layout() override {
        float total_h = m_options.size() * m_item_height + 4.0f;
        float w = m_constraints.min_width;
        if (m_constraints.has_bounded_width()) {
            w = m_constraints.max_width;
        }
        m_size = Vec2{w, total_h};
    }

    void paint(PaintContext& context) override {
        // 背景与 1px 细边框
        context.draw_rect(bounds(), m_bg_color, 3.0f, 0.0f);
        context.draw_rect(bounds(), m_border_color, 3.0f, 1.0f);

        float cur_y = 2.0f;
        for (size_t i = 0; i < m_options.size(); ++i) {
            const auto& opt = m_options[i];
            Rect item_rect = Rect::from_xywh(2.0f, cur_y, m_size.x - 4.0f, m_item_height);

            bool is_cur = (opt == m_selected);
            bool is_hov = (i == m_hovered_index);

            if (is_hov) {
                context.draw_rect(item_rect, m_hover_color, 2.0f, 0.0f);
            } else if (is_cur) {
                context.draw_rect(item_rect, Color{0.18f, 0.28f, 0.44f, 1.0f}, 2.0f, 0.0f);
            }

            Color txt_col = (is_hov || is_cur) ? Color::white() : Color{0.88f, 0.88f, 0.92f, 1.0f};
            float text_y = cur_y + (m_item_height - m_font_size * 1.30f) * 0.5f;
            context.draw_text(opt, PointF{item_rect.min_x + 8.0f, text_y}, m_font_size, txt_col);

            cur_y += m_item_height;
        }
    }

private:
    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& ev) {
            float y = ev.local_position.y - 2.0f;
            size_t idx = static_cast<size_t>(y / m_item_height);
            if (idx < m_options.size() && idx != m_hovered_index) {
                m_hovered_index = idx;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            float y = ev.local_position.y - 2.0f;
            size_t idx = static_cast<size_t>(y / m_item_height);
            if (idx < m_options.size()) {
                std::string chosen = m_options[idx];
                if (m_on_close) m_on_close();
                if (m_on_select) m_on_select(chosen);
            }
        };
    }

    std::vector<std::string> m_options;
    std::string m_selected;
    float m_item_height{24.0f};
    float m_font_size{12.5f};
    std::function<void(const std::string&)> m_on_select{nullptr};
    std::function<void()> m_on_close{nullptr};
    size_t m_hovered_index{static_cast<size_t>(-1)};

    Color m_bg_color{Color{0.14f, 0.14f, 0.16f, 0.98f}};
    Color m_border_color{Color{0.28f, 0.28f, 0.32f, 1.0f}};
    Color m_hover_color{Color{0.20f, 0.46f, 0.88f, 1.0f}};
};

// 桌面紧凑下拉选框渲染盒 (RenderSelect)
class RenderSelect : public RenderBox {
public:
    RenderSelect() noexcept {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderSelect() override = default;

    void set_properties(std::vector<std::string> options, std::string selected,
                         float h, float radius, EdgeInsets pad,
                         Color bg, Color border, Color text_col, float font_sz,
                         std::function<void(const std::string&)> on_ch) {
        m_options = std::move(options);
        m_selected = std::move(selected);
        m_height = h;
        m_corner_radius = radius;
        m_padding = pad;
        m_bg_color = bg;
        m_border_color = border;
        m_text_color = text_col;
        m_font_size = font_sz;
        m_on_change = std::move(on_ch);

        mark_needs_layout();
    }

    void set_selected(std::string sel) {
        if (m_selected != sel) {
            m_selected = std::move(sel);
            mark_needs_paint();
        }
    }

    [[nodiscard]] const std::string& selected() const noexcept { return m_selected; }

    void perform_layout() override {
        float desired_w = 140.0f;
        if (m_constraints.has_bounded_width()) {
            desired_w = m_constraints.max_width;
        }
        m_size = m_constraints.constrain(Vec2{desired_w, m_height});
    }

    void paint(PaintContext& context) override {
        // 1. 底框与 CUI CardBorder 微弱微光 (非高对比度生硬边框)
        Color border_col = is_hovered ? Color{1.0f, 1.0f, 1.0f, 0.12f} : m_border_color;
        context.draw_rect(bounds(), m_bg_color, m_corner_radius, 0.0f);
        if (border_col.a > 0.001f) {
            context.draw_rect(bounds(), border_col, m_corner_radius, 1.0f);
        }

        // 2. 当前选中项文本 (裁切防溢出)
        float text_y = (m_size.y - m_font_size * 1.30f) * 0.5f;
        float max_txt_w = std::max(0.0f, m_size.x - m_padding.horizontal() - 16.0f);
        context.push_clip(Rect::from_xywh(m_padding.left, 0.0f, max_txt_w, m_size.y), 0.0f);
        context.draw_text(m_selected.empty() ? "(未选择)" : m_selected,
                          PointF{m_padding.left, text_y}, m_font_size, m_text_color);
        context.pop_clip();

        // 3. 右侧微型下拉倒三角箭头
        float arrow_x = m_size.x - 14.0f;
        float arrow_y = (m_size.y - 4.0f) * 0.5f;
        // 简单绘制 4px 高度的小 V 符号
        context.draw_text("v", PointF{arrow_x, arrow_y - 2.0f}, 10.0f, Color{0.65f, 0.65f, 0.70f, 1.0f});
    }

    void toggle_dropdown() {
        Window* win = Window::active_window();
        if (!win) return;

        if (m_overlay_id != 0) {
            win->hide_overlay(m_overlay_id);
            m_overlay_id = 0;
            return;
        }

        Vec2 abs_pos = global_offset();
        float item_h = 24.0f;
        float drop_h = m_options.size() * item_h + 4.0f;
        float drop_w = m_size.x;

        Vec2 win_sz = win->size();
        float drop_y = abs_pos.y + m_size.y + 2.0f;
        // 若靠近底部，自动向上展开
        if (drop_y + drop_h > win_sz.y && abs_pos.y >= drop_h) {
            drop_y = abs_pos.y - drop_h - 2.0f;
        }

        auto dropdown_node = std::make_shared<RenderSelectDropdown>(
            m_options, m_selected, item_h, m_font_size,
            [this](const std::string& chosen) {
                set_selected(chosen);
                if (m_on_change) m_on_change(chosen);
            },
            [this, win]() {
                if (m_overlay_id != 0) {
                    uint64_t id = m_overlay_id;
                    m_overlay_id = 0;
                    win->hide_overlay(id);
                }
            }
        );

        OverlayEntry entry;
        entry.content = dropdown_node;
        entry.global_pos = Vec2{abs_pos.x, drop_y};
        entry.size = Vec2{drop_w, drop_h};
        entry.dismiss_on_outside_click = true;
        entry.is_modal = false;
        entry.on_dismiss = [this]() {
            m_overlay_id = 0;
        };

        m_overlay_id = win->show_overlay(std::move(entry));
    }

private:
    void setup_behavior() {
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary) {
                toggle_dropdown();
            }
        };
    }

    std::vector<std::string> m_options;
    std::string m_selected;
    float m_height{26.0f};
    float m_corner_radius{3.0f};
    EdgeInsets m_padding{EdgeInsets(4.0f, 8.0f)};
    Color m_bg_color{Color{0.14f, 0.14f, 0.16f, 1.0f}};
    Color m_border_color{Color{1.0f, 1.0f, 1.0f, 0.05f}};
    Color m_text_color{Color{0.92f, 0.92f, 0.95f, 1.0f}};
    float m_font_size{12.5f};
    std::function<void(const std::string&)> m_on_change{nullptr};

    uint64_t m_overlay_id{0};
};

class SelectWidget;

// C++20 指定初始化器声明式下拉选框 (Select)
struct Select {
    std::vector<std::string> options;
    std::string selected{""};
    BindRef<std::string> bind_selected{nullptr};
    float height{26.0f};
    float corner_radius{3.0f};
    EdgeInsets padding{EdgeInsets(4.0f, 8.0f)};
    Color background_color{Color{0.14f, 0.14f, 0.16f, 1.0f}};
    Color border_color{Color{1.0f, 1.0f, 1.0f, 0.05f}};
    Color text_color{Color{0.92f, 0.92f, 0.95f, 1.0f}};
    float font_size{12.5f};
    std::function<void(const std::string&)> on_change{nullptr};

    operator WidgetPtr() const;
};

class SelectWidget : public Widget {
public:
    explicit SelectWidget(const Select& desc) : m_desc(desc) {}
    ~SelectWidget() override { unsubscribe(); }

    std::shared_ptr<RenderBox> create_render_node() override {
        std::string init_val = m_desc.bind_selected ? m_desc.bind_selected->get() : m_desc.selected;
        if (init_val.empty() && !m_desc.options.empty()) {
            init_val = m_desc.options.front();
        }

        auto node = std::make_shared<RenderSelect>();
        auto on_ch = [this, node_weak = std::weak_ptr<RenderSelect>(node)](const std::string& val) {
            if (m_desc.bind_selected) {
                m_desc.bind_selected->set(val);
            }
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
        };

        node->set_properties(m_desc.options, init_val, m_desc.height, m_desc.corner_radius,
                             m_desc.padding, m_desc.background_color, m_desc.border_color,
                             m_desc.text_color, m_desc.font_size, on_ch);

        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sel_box = std::dynamic_pointer_cast<RenderSelect>(node)) {
            std::string cur_val = m_desc.bind_selected ? m_desc.bind_selected->get() : m_desc.selected;
            auto on_ch = [this](const std::string& val) {
                if (m_desc.bind_selected) {
                    m_desc.bind_selected->set(val);
                }
                if (m_desc.on_change) {
                    m_desc.on_change(val);
                }
            };
            sel_box->set_properties(m_desc.options, cur_val, m_desc.height, m_desc.corner_radius,
                                    m_desc.padding, m_desc.background_color, m_desc.border_color,
                                    m_desc.text_color, m_desc.font_size, on_ch);
            setup_binding(sel_box);
        }
    }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_selected) {
            m_desc.bind_selected->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderSelect> node) {
        unsubscribe();
        if (m_desc.bind_selected) {
            m_binding_token = m_desc.bind_selected->subscribe([node_weak = std::weak_ptr<RenderSelect>(node)](const std::string& val) {
                if (auto n = node_weak.lock()) {
                    n->set_selected(val);
                }
            });
        }
    }

    Select m_desc;
    uint64_t m_binding_token{0};
};

inline Select::operator WidgetPtr() const {
    return std::make_shared<SelectWidget>(*this);
}

} // namespace ktui
