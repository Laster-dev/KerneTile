#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <memory>
#include <functional>
#include <algorithm>

namespace ktui {

// 桌面紧凑开关渲染盒 (RenderSwitch)
class RenderSwitch : public RenderBox {
public:
    RenderSwitch(bool checked, float w, float h,
                 Color track_off, Color track_on, Color thumb_col,
                 std::function<void(bool)> on_ch)
        : m_checked(checked), m_width(w), m_height(h),
          m_track_off(track_off), m_track_on(track_on), m_thumb_color(thumb_col),
          m_on_change(std::move(on_ch)) {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    void set_checked(bool checked) {
        if (m_checked != checked) {
            m_checked = checked;
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool checked() const noexcept { return m_checked; }

    void perform_layout() override {
        m_size = m_constraints.constrain(Vec2{m_width, m_height});
    }

    void paint(PaintContext& context) override {
        float radius = m_size.y * 0.5f;
        Color track_col = m_checked ? m_track_on : (is_hovered ? Color{0.25f, 0.25f, 0.30f, 1.0f} : m_track_off);

        // 1. 胶囊轨道背景与 1px 细边框
        context.draw_rect(bounds(), track_col, radius, 0.0f);
        context.draw_rect(bounds(), Color{0.30f, 0.30f, 0.35f, 0.8f}, radius, 1.0f);

        // 2. 滑块
        float thumb_sz = m_size.y - 4.0f;
        float thumb_x = m_checked ? (m_size.x - thumb_sz - 2.0f) : 2.0f;
        float thumb_y = 2.0f;
        Rect thumb_rect = Rect::from_xywh(thumb_x, thumb_y, thumb_sz, thumb_sz);
        context.draw_rect(thumb_rect, m_thumb_color, thumb_sz * 0.5f, 0.0f);
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
    float m_width{32.0f};
    float m_height{16.0f};
    Color m_track_off{Color{0.18f, 0.18f, 0.22f, 1.0f}};
    Color m_track_on{Color{0.18f, 0.48f, 0.90f, 1.0f}};
    Color m_thumb_color{Color::white()};
    std::function<void(bool)> m_on_change{nullptr};
};

class SwitchWidget;

// C++20 指定初始化器声明式开关 (Switch)
struct Switch {
    bool checked{false};
    BindRef<bool> bind_checked{nullptr};
    float width{32.0f};
    float height{16.0f};
    Color track_off_color{Color{0.18f, 0.18f, 0.22f, 1.0f}};
    Color track_on_color{Color{0.18f, 0.48f, 0.90f, 1.0f}};
    Color thumb_color{Color::white()};
    std::function<void(bool)> on_change{nullptr};

    operator WidgetPtr() const;
};

class SwitchWidget : public Widget {
public:
    explicit SwitchWidget(const Switch& desc) : m_desc(desc) {}
    ~SwitchWidget() override { unsubscribe(); }

    std::shared_ptr<RenderBox> create_render_node() override {
        bool init_val = m_desc.bind_checked ? m_desc.bind_checked->get() : m_desc.checked;
        auto on_ch = [this](bool val) {
            if (m_desc.bind_checked) {
                m_desc.bind_checked->set(val);
            }
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
        };

        auto node = std::make_shared<RenderSwitch>(init_val, m_desc.width, m_desc.height,
                                                   m_desc.track_off_color, m_desc.track_on_color,
                                                   m_desc.thumb_color, on_ch);
        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sw = std::dynamic_pointer_cast<RenderSwitch>(node)) {
            bool cur_val = m_desc.bind_checked ? m_desc.bind_checked->get() : m_desc.checked;
            sw->set_checked(cur_val);
            setup_binding(sw);
        }
    }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_checked) {
            m_desc.bind_checked->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderSwitch> node) {
        unsubscribe();
        if (m_desc.bind_checked) {
            m_binding_token = m_desc.bind_checked->subscribe([node_weak = std::weak_ptr<RenderSwitch>(node)](bool val) {
                if (auto n = node_weak.lock()) {
                    n->set_checked(val);
                }
            });
        }
    }

    Switch m_desc;
    uint64_t m_binding_token{0};
};

inline Switch::operator WidgetPtr() const {
    return std::make_shared<SwitchWidget>(*this);
}

} // namespace ktui
