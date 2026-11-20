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

struct RadioOption {
    std::string label{""};
    std::string value{""};
};

// 桌面高信息密度单选按钮组渲染盒 (RenderRadioGroup)
class RenderRadioGroup : public RenderBox {
public:
    RenderRadioGroup(std::vector<RadioOption> options, std::string selected,
                     bool horiz, float spacing, float sz, float font_sz,
                     std::function<void(const std::string&)> on_ch)
        : m_options(std::move(options)), m_selected(std::move(selected)),
          m_horizontal(horiz), m_spacing(spacing), m_circle_size(sz),
          m_font_size(font_sz), m_on_change(std::move(on_ch)) {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    void set_selected(std::string sel) {
        if (m_selected != sel) {
            m_selected = std::move(sel);
            mark_needs_paint();
        }
    }

    [[nodiscard]] const std::string& selected() const noexcept { return m_selected; }

    void perform_layout() override {
        m_item_rects.clear();
        float cur_x = 0.0f;
        float cur_y = 0.0f;
        float max_w = 0.0f;
        float max_h = 0.0f;

        float row_h = std::max(m_circle_size, m_font_size * 1.30f) + 4.0f;

        for (const auto& opt : m_options) {
            float label_w = TextMeasure::measure_width(opt.label, m_font_size);
            float item_w = m_circle_size + 6.0f + label_w + 8.0f;

            if (m_horizontal) {
                m_item_rects.push_back(Rect::from_xywh(cur_x, 0.0f, item_w, row_h));
                cur_x += item_w + m_spacing;
                max_h = std::max(max_h, row_h);
            } else {
                m_item_rects.push_back(Rect::from_xywh(0.0f, cur_y, item_w, row_h));
                cur_y += row_h + m_spacing;
                max_w = std::max(max_w, item_w);
            }
        }

        float total_w = m_horizontal ? std::max(0.0f, cur_x - m_spacing) : max_w;
        float total_h = m_horizontal ? max_h : std::max(0.0f, cur_y - m_spacing);

        m_size = m_constraints.constrain(Vec2{total_w, total_h});
    }

    void paint(PaintContext& context) override {
        for (size_t i = 0; i < m_options.size() && i < m_item_rects.size(); ++i) {
            const auto& opt = m_options[i];
            const auto& r = m_item_rects[i];

            bool is_sel = (opt.value == m_selected);
            bool is_hov = (i == m_hovered_index);

            // 1. 外圆圈
            float circle_y = r.min_y + (r.height() - m_circle_size) * 0.5f;
            Rect circle_rect = Rect::from_xywh(r.min_x, circle_y, m_circle_size, m_circle_size);
            float radius = m_circle_size * 0.5f;

            Color circle_bg = is_sel ? Color{0.14f, 0.22f, 0.35f, 1.0f} : Color{0.14f, 0.14f, 0.16f, 1.0f};
            Color border_col = is_sel ? m_accent_color : (is_hov ? Color{0.40f, 0.40f, 0.45f, 1.0f} : m_border_color);

            context.draw_rect(circle_rect, circle_bg, radius, 0.0f);
            context.draw_rect(circle_rect, border_col, radius, 1.0f);

            // 2. 内实心圆点 (选中状态)
            if (is_sel) {
                float inner_sz = m_circle_size * 0.45f;
                float inner_x = r.min_x + (m_circle_size - inner_sz) * 0.5f;
                float inner_y = circle_y + (m_circle_size - inner_sz) * 0.5f;
                context.draw_rect(Rect::from_xywh(inner_x, inner_y, inner_sz, inner_sz),
                                  m_accent_color, inner_sz * 0.5f, 0.0f);
            }

            // 3. 标签文本
            float text_x = r.min_x + m_circle_size + 6.0f;
            float text_y = r.min_y + (r.height() - m_font_size * 1.30f) * 0.5f;
            Color text_col = is_sel ? Color::white() : (is_hov ? Color{0.92f, 0.92f, 0.95f, 1.0f} : m_text_color);
            context.draw_text(opt.label, PointF{text_x, text_y}, m_font_size, text_col);
        }
    }

private:
    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& ev) {
            size_t new_hov = static_cast<size_t>(-1);
            for (size_t i = 0; i < m_item_rects.size(); ++i) {
                if (m_item_rects[i].contains(ev.local_position)) {
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
            for (size_t i = 0; i < m_item_rects.size(); ++i) {
                if (m_item_rects[i].contains(ev.local_position)) {
                    std::string val = m_options[i].value;
                    set_selected(val);
                    if (m_on_change) {
                        m_on_change(val);
                    }
                    break;
                }
            }
        };
    }

    std::vector<RadioOption> m_options;
    std::string m_selected;
    bool m_horizontal{false};
    float m_spacing{8.0f};
    float m_circle_size{14.0f};
    float m_font_size{13.0f};
    std::function<void(const std::string&)> m_on_change{nullptr};

    std::vector<Rect> m_item_rects;
    size_t m_hovered_index{static_cast<size_t>(-1)};

    Color m_border_color{Color{0.28f, 0.28f, 0.32f, 1.0f}};
    Color m_accent_color{Color{0.20f, 0.55f, 0.95f, 1.0f}};
    Color m_text_color{Color{0.82f, 0.82f, 0.86f, 1.0f}};
};

class RadioGroupWidget;

// C++20 指定初始化器声明式单选组 (RadioGroup)
struct RadioGroup {
    std::vector<RadioOption> options;
    std::string selected{""};
    BindRef<std::string> bind_selected{nullptr};
    bool horizontal{false};
    float spacing{8.0f};
    float circle_size{14.0f};
    float font_size{13.0f};
    std::function<void(const std::string&)> on_change{nullptr};

    operator WidgetPtr() const;
};

class RadioGroupWidget : public Widget {
public:
    explicit RadioGroupWidget(const RadioGroup& desc) : m_desc(desc) {}
    ~RadioGroupWidget() override { unsubscribe(); }

    std::shared_ptr<RenderBox> create_render_node() override {
        std::string init_val = m_desc.bind_selected ? m_desc.bind_selected->get() : m_desc.selected;
        if (init_val.empty() && !m_desc.options.empty()) {
            init_val = m_desc.options.front().value;
        }

        auto on_ch = [this](const std::string& val) {
            if (m_desc.bind_selected) {
                m_desc.bind_selected->set(val);
            }
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
        };

        auto node = std::make_shared<RenderRadioGroup>(
            m_desc.options, init_val, m_desc.horizontal, m_desc.spacing,
            m_desc.circle_size, m_desc.font_size, on_ch);

        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto rg = std::dynamic_pointer_cast<RenderRadioGroup>(node)) {
            std::string cur_val = m_desc.bind_selected ? m_desc.bind_selected->get() : m_desc.selected;
            rg->set_selected(cur_val);
            setup_binding(rg);
        }
    }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_selected) {
            m_desc.bind_selected->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderRadioGroup> node) {
        unsubscribe();
        if (m_desc.bind_selected) {
            m_binding_token = m_desc.bind_selected->subscribe([node_weak = std::weak_ptr<RenderRadioGroup>(node)](const std::string& val) {
                if (auto n = node_weak.lock()) {
                    n->set_selected(val);
                }
            });
        }
    }

    RadioGroup m_desc;
    uint64_t m_binding_token{0};
};

inline RadioGroup::operator WidgetPtr() const {
    return std::make_shared<RadioGroupWidget>(*this);
}

} // namespace ktui
