#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <algorithm>
#include <memory>

namespace ktui {

// 桌面紧凑进度条渲染盒 (RenderProgressBar)
class RenderProgressBar : public RenderBox {
public:
    RenderProgressBar(float val, float h, float radius, Color empty_col, Color fill_col)
        : m_value(std::clamp(val, 0.0f, 1.0f)), m_height(h), m_corner_radius(radius),
          m_empty_color(empty_col), m_fill_color(fill_col) {}

    void set_value(float val) {
        float clamped = std::clamp(val, 0.0f, 1.0f);
        if (m_value != clamped) {
            m_value = clamped;
            mark_needs_paint();
        }
    }

    [[nodiscard]] float value() const noexcept { return m_value; }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 120.0f;
        m_size = m_constraints.constrain(Vec2{w, m_height});
    }

    void paint(PaintContext& context) override {
        // 1. 底层空轨
        context.draw_rect(bounds(), m_empty_color, m_corner_radius, 0.0f);

        // 2. 进度填充条
        if (m_value > 0.0f) {
            float fill_w = std::max(m_corner_radius * 2.0f, m_size.x * m_value);
            fill_w = std::min(fill_w, m_size.x);
            Rect fill_rect = Rect::from_xywh(0.0f, 0.0f, fill_w, m_size.y);
            context.draw_rect(fill_rect, m_fill_color, m_corner_radius, 0.0f);
        }
    }

private:
    float m_value{0.0f};
    float m_height{5.0f};
    float m_corner_radius{2.0f};
    Color m_empty_color{Color{0.18f, 0.18f, 0.22f, 1.0f}};
    Color m_fill_color{Color{0.20f, 0.55f, 0.95f, 1.0f}};
};

class ProgressBarWidget;

// C++20 指定初始化器声明式进度条 (ProgressBar)
struct ProgressBar {
    float value{0.0f};
    BindRef<float> bind_value{nullptr};
    float height{5.0f};
    float corner_radius{2.0f};
    Color empty_color{Color{0.18f, 0.18f, 0.22f, 1.0f}};
    Color fill_color{Color{0.20f, 0.55f, 0.95f, 1.0f}};

    operator WidgetPtr() const;
};

class ProgressBarWidget : public Widget {
public:
    explicit ProgressBarWidget(const ProgressBar& desc) : m_desc(desc) {}
    ~ProgressBarWidget() override { unsubscribe(); }

    std::shared_ptr<RenderBox> create_render_node() override {
        float init_val = m_desc.bind_value ? m_desc.bind_value->get() : m_desc.value;
        auto node = std::make_shared<RenderProgressBar>(init_val, m_desc.height, m_desc.corner_radius,
                                                        m_desc.empty_color, m_desc.fill_color);
        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto pb = std::dynamic_pointer_cast<RenderProgressBar>(node)) {
            float cur_val = m_desc.bind_value ? m_desc.bind_value->get() : m_desc.value;
            pb->set_value(cur_val);
            setup_binding(pb);
        }
    }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_value) {
            m_desc.bind_value->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderProgressBar> node) {
        unsubscribe();
        if (m_desc.bind_value) {
            m_binding_token = m_desc.bind_value->subscribe([node_weak = std::weak_ptr<RenderProgressBar>(node)](float val) {
                if (auto n = node_weak.lock()) {
                    n->set_value(val);
                }
            });
        }
    }

    ProgressBar m_desc;
    uint64_t m_binding_token{0};
};

inline ProgressBar::operator WidgetPtr() const {
    return std::make_shared<ProgressBarWidget>(*this);
}

} // namespace ktui
