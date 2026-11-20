#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <optional>
#include <algorithm>

namespace ktui {

// 桌面紧凑滑块渲染盒 (RenderSlider)
class RenderSlider : public RenderBox {
public:
    RenderSlider() noexcept {
        setup_behavior();
    }

    ~RenderSlider() override = default;

    void set_properties(float val, float min_v, float max_v, float track_h, float thumb_sz,
                        Color track_empty, Color track_filled, Color thumb_col,
                        std::function<void(float)> on_ch) {
        m_value = std::clamp(val, min_v, max_v);
        m_min_value = min_v;
        m_max_value = max_v;
        m_track_height = track_h;
        m_thumb_size = thumb_sz;
        m_track_empty = track_empty;
        m_track_filled = track_filled;
        m_thumb_col = thumb_col;
        m_on_change = std::move(on_ch);

        mark_needs_layout();
    }

    void set_value(float val) {
        float clamped = std::clamp(val, m_min_value, m_max_value);
        if (m_value != clamped) {
            m_value = clamped;
            mark_needs_paint();
        }
    }

    [[nodiscard]] float value() const noexcept { return m_value; }

    void perform_layout() override {
        // 默认滑块在宽度上拉伸，高度为 20px
        float desired_w = m_constraints.has_bounded_width() ? m_constraints.max_width : 120.0f;
        float desired_h = std::max(20.0f, m_thumb_size + 4.0f);

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
    }

    void paint(PaintContext& context) override {
        float available_w = m_size.x - m_thumb_size;
        float progress = (m_max_value > m_min_value)
            ? (m_value - m_min_value) / (m_max_value - m_min_value)
            : 0.0f;
        progress = std::clamp(progress, 0.0f, 1.0f);

        float track_y = (m_size.y - m_track_height) * 0.5f;

        // 1. 底层未填充细轨道 (4px 细轨，微圆角 2px)
        Rect empty_rect = Rect::from_xywh(0.0f, track_y, m_size.x, m_track_height);
        context.draw_rect(empty_rect, m_track_empty, 2.0f, 0.0f);

        // 2. 高亮填充轨道
        float filled_w = progress * available_w + m_thumb_size * 0.5f;
        Rect filled_rect = Rect::from_xywh(0.0f, track_y, filled_w, m_track_height);
        context.draw_rect(filled_rect, m_track_filled, 2.0f, 0.0f);

        // 3. 滑块 (12px 精致硬朗小滑块，圆角 2px)
        float thumb_x = progress * available_w;
        float thumb_y = (m_size.y - m_thumb_size) * 0.5f;
        Rect thumb_rect = Rect::from_xywh(thumb_x, thumb_y, m_thumb_size, m_thumb_size);
        context.draw_rect(thumb_rect, m_thumb_col, 2.0f, 1.0f);
    }

    // 交互拖拽或点击位置更新
    void update_position(float local_x) {
        float available_w = m_size.x - m_thumb_size;
        if (available_w <= 0.0f) return;

        float clamped_x = std::clamp(local_x - m_thumb_size * 0.5f, 0.0f, available_w);
        float progress = clamped_x / available_w;
        float new_val = m_min_value + progress * (m_max_value - m_min_value);

        if (m_value != new_val) {
            m_value = new_val;
            mark_needs_paint();
            if (m_on_change) {
                m_on_change(m_value);
            }
        }
    }

private:
    void setup_behavior() {
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary) {
                m_is_dragging = true;
                update_position(ev.position.x);
            }
        };

        on_pointer_move = [this](PointerEvent& ev) {
            if (m_is_dragging) {
                update_position(ev.position.x);
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary) {
                m_is_dragging = false;
            }
        };
    }

    float m_value{0.0f};
    float m_min_value{0.0f};
    float m_max_value{1.0f};
    float m_track_height{4.0f};    // 桌面高信息密度 4px 细轨
    float m_thumb_size{12.0f};     // 桌面紧凑 12px 小滑块
    Color m_track_empty{Color{0.18f, 0.18f, 0.20f, 1.0f}};
    Color m_track_filled{Color{0.20f, 0.55f, 0.95f, 1.0f}};
    Color m_thumb_col{Color{0.90f, 0.90f, 0.92f, 1.0f}};
    std::function<void(float)> m_on_change{nullptr};
    bool m_is_dragging{false};
};

class SliderWidget;

// C++20 指定初始化器精密桌面滑块 (Slider)
// 桌面专业标准：4px 细轨道，12px 紧凑微滑块，双向数据绑定
struct Slider {
    float value{0.0f};
    float min_value{0.0f};
    float max_value{1.0f};
    BindRef<float> bind_value{nullptr};
    float track_height{4.0f};   // 桌面紧凑 4px
    float thumb_size{12.0f};    // 桌面紧凑 12px
    Color track_empty_color{Color{0.18f, 0.18f, 0.20f, 1.0f}};
    Color track_filled_color{Color{0.20f, 0.55f, 0.95f, 1.0f}};
    Color thumb_color{Color{0.92f, 0.92f, 0.95f, 1.0f}};
    std::function<void(float)> on_change{nullptr};

    operator WidgetPtr() const;
};

// SliderWidget 包装实现
class SliderWidget : public Widget {
public:
    explicit SliderWidget(const Slider& desc) : m_desc(desc) {}

    ~SliderWidget() override {
        unsubscribe();
    }

    std::shared_ptr<RenderBox> create_render_node() override {
        float init_val = m_desc.bind_value ? m_desc.bind_value->get() : m_desc.value;
        auto node = std::make_shared<RenderSlider>();

        auto on_ch = [this](float val) {
            m_is_internal_update = true;
            if (m_desc.bind_value) {
                m_desc.bind_value->set(val);
            }
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
            m_is_internal_update = false;
        };

        node->set_properties(init_val, m_desc.min_value, m_desc.max_value,
                             m_desc.track_height, m_desc.thumb_size,
                             m_desc.track_empty_color, m_desc.track_filled_color,
                             m_desc.thumb_color, on_ch);

        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sld = std::dynamic_pointer_cast<RenderSlider>(node)) {
            float current_val = m_desc.bind_value ? m_desc.bind_value->get() : m_desc.value;
            auto on_ch = [this](float val) {
                m_is_internal_update = true;
                if (m_desc.bind_value) {
                    m_desc.bind_value->set(val);
                }
                if (m_desc.on_change) {
                    m_desc.on_change(val);
                }
                m_is_internal_update = false;
            };

            sld->set_properties(current_val, m_desc.min_value, m_desc.max_value,
                                m_desc.track_height, m_desc.thumb_size,
                                m_desc.track_empty_color, m_desc.track_filled_color,
                                m_desc.thumb_color, on_ch);

            setup_binding(sld);
        }
    }

    [[nodiscard]] const Slider& desc() const noexcept { return m_desc; }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_value) {
            m_desc.bind_value->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderSlider> sld) {
        unsubscribe();
        if (m_desc.bind_value) {
            std::weak_ptr<RenderSlider> weak_box = sld;
            m_binding_token = m_desc.bind_value->subscribe([this, weak_box](float new_val) {
                if (m_is_internal_update) return;
                if (auto box = weak_box.lock()) {
                    box->set_value(new_val);
                }
            });
        }
    }

    Slider m_desc;
    uint64_t m_binding_token{0};
    bool m_is_internal_update{false};
};

inline Slider::operator WidgetPtr() const {
    return std::make_shared<SliderWidget>(*this);
}

} // namespace ktui
