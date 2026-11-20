#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <optional>
#include <string>

namespace ktui {

// 桌面紧凑复选框渲染盒 (RenderCheckbox)
class RenderCheckbox : public RenderBox {
public:
    RenderCheckbox() noexcept {
        setup_behavior();
    }

    ~RenderCheckbox() override = default;

    void set_properties(bool checked, std::string label, float box_sz, float radius,
                        Color bg_normal, Color bg_checked, Color border_col,
                        Color check_mark_col, Color txt_col, float font_sz,
                        std::function<void(bool)> on_ch) {
        m_checked = checked;
        m_label = std::move(label);
        m_box_size = box_sz;
        m_corner_radius = radius;
        m_bg_normal = bg_normal;
        m_bg_checked = bg_checked;
        m_border_col = border_col;
        m_check_mark_col = check_mark_col;
        m_text_col = txt_col;
        m_font_size = font_sz;
        m_on_change = std::move(on_ch);

        mark_needs_layout();
    }

    void set_checked(bool checked) {
        if (m_checked != checked) {
            m_checked = checked;
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool checked() const noexcept { return m_checked; }

    void perform_layout() override {
        float label_w = m_label.empty() ? 0.0f : (static_cast<float>(m_label.length()) * m_font_size * 0.58f + 6.0f);
        float desired_w = m_box_size + label_w;
        float desired_h = std::max(m_box_size, m_font_size * 1.35f);

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
    }

    void paint(PaintContext& context) override {
        // 1. 方框垂直居中
        float box_y = (m_size.y - m_box_size) * 0.5f;
        Rect box_rect = Rect::from_xywh(0.0f, box_y, m_box_size, m_box_size);

        Color current_bg = m_checked ? m_bg_checked : m_bg_normal;
        Color current_border = m_checked ? m_bg_checked : m_border_col;

        context.draw_rect(box_rect, current_bg, m_corner_radius, 1.0f);

        // 2. 勾选标记 (绘制紧凑勾选内芯或微对勾)
        if (m_checked) {
            float inner_margin = 3.0f;
            Rect inner_rect = Rect::from_xywh(inner_margin, box_y + inner_margin,
                                              m_box_size - inner_margin * 2.0f,
                                              m_box_size - inner_margin * 2.0f);
            context.draw_rect(inner_rect, m_check_mark_col, 1.0f, 0.0f);
        }

        // 3. 标签文字
        if (!m_label.empty()) {
            float text_x = m_box_size + 6.0f;
            float text_y = std::max(0.0f, (m_size.y - m_font_size * 1.35f) * 0.5f);
            context.draw_text(m_label, PointF{text_x, text_y}, m_font_size, m_text_col);
        }
    }

    void toggle() {
        m_checked = !m_checked;
        mark_needs_paint();
        if (m_on_change) {
            m_on_change(m_checked);
        }
    }

private:
    void setup_behavior() {
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary) {
                toggle();
            }
        };
    }

    bool m_checked{false};
    std::string m_label;
    float m_box_size{14.0f};        // 桌面高信息密度 14px 方块
    float m_corner_radius{2.0f};    // 硬朗微圆角 2px
    Color m_bg_normal{Color{0.18f, 0.18f, 0.20f, 1.0f}};
    Color m_bg_checked{Color{0.20f, 0.55f, 0.95f, 1.0f}}; // 桌面强调蓝
    Color m_border_col{Color{0.32f, 0.32f, 0.36f, 1.0f}};
    Color m_check_mark_col{Color::white()};
    Color m_text_col{Color{0.90f, 0.90f, 0.92f, 1.0f}};
    float m_font_size{13.0f};
    std::function<void(bool)> m_on_change{nullptr};
};

class CheckboxWidget;

// C++20 指定初始化器桌面复选框 (Checkbox)
// 桌面专业标准：14px 紧凑方块，2px 硬朗微圆角，响应式双向绑定
struct Checkbox {
    bool checked{false};
    BindRef<bool> bind_checked{nullptr};
    std::string label{""};
    float size{14.0f};          // 桌面标准 14px
    float corner_radius{2.0f};  // 桌面微圆角 2px
    Color background_color{Color{0.18f, 0.18f, 0.20f, 1.0f}};
    Color checked_color{Color{0.20f, 0.55f, 0.95f, 1.0f}};
    Color border_color{Color{0.32f, 0.32f, 0.36f, 1.0f}};
    Color text_color{Color{0.90f, 0.90f, 0.92f, 1.0f}};
    float font_size{13.0f};
    std::function<void(bool)> on_change{nullptr};

    operator WidgetPtr() const;
};

// CheckboxWidget 包装实现
class CheckboxWidget : public Widget {
public:
    explicit CheckboxWidget(const Checkbox& desc) : m_desc(desc) {}

    ~CheckboxWidget() override {
        unsubscribe();
    }

    std::shared_ptr<RenderBox> create_render_node() override {
        bool init_val = m_desc.bind_checked ? m_desc.bind_checked->get() : m_desc.checked;
        auto node = std::make_shared<RenderCheckbox>();

        auto on_ch = [this](bool val) {
            m_is_internal_update = true;
            if (m_desc.bind_checked) {
                m_desc.bind_checked->set(val);
            }
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
            m_is_internal_update = false;
        };

        node->set_properties(init_val, m_desc.label, m_desc.size, m_desc.corner_radius,
                             m_desc.background_color, m_desc.checked_color,
                             m_desc.border_color, Color::white(),
                             m_desc.text_color, m_desc.font_size, on_ch);

        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto chk = std::dynamic_pointer_cast<RenderCheckbox>(node)) {
            bool current_val = m_desc.bind_checked ? m_desc.bind_checked->get() : m_desc.checked;
            auto on_ch = [this](bool val) {
                m_is_internal_update = true;
                if (m_desc.bind_checked) {
                    m_desc.bind_checked->set(val);
                }
                if (m_desc.on_change) {
                    m_desc.on_change(val);
                }
                m_is_internal_update = false;
            };

            chk->set_properties(current_val, m_desc.label, m_desc.size, m_desc.corner_radius,
                                m_desc.background_color, m_desc.checked_color,
                                m_desc.border_color, Color::white(),
                                m_desc.text_color, m_desc.font_size, on_ch);

            setup_binding(chk);
        }
    }

    [[nodiscard]] const Checkbox& desc() const noexcept { return m_desc; }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_checked) {
            m_desc.bind_checked->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderCheckbox> chk) {
        unsubscribe();
        if (m_desc.bind_checked) {
            std::weak_ptr<RenderCheckbox> weak_box = chk;
            m_binding_token = m_desc.bind_checked->subscribe([this, weak_box](bool new_val) {
                if (m_is_internal_update) return;
                if (auto box = weak_box.lock()) {
                    box->set_checked(new_val);
                }
            });
        }
    }

    Checkbox m_desc;
    uint64_t m_binding_token{0};
    bool m_is_internal_update{false};
};

inline Checkbox::operator WidgetPtr() const {
    return std::make_shared<CheckboxWidget>(*this);
}

} // namespace ktui
