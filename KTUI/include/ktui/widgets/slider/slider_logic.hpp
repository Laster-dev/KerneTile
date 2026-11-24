#pragma once

#include "slider_data.hpp"
#include "slider_style.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>

namespace ktui {

// 滑块逻辑与渲染盒 (RenderSlider - Logic Layer)
class RenderSlider : public RenderBox {
public:
    RenderSlider() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderSlider() override = default;

    [[nodiscard]] float value() const noexcept { return m_data.value; }
    [[nodiscard]] float min_value() const noexcept { return m_data.min_value; }
    [[nodiscard]] float max_value() const noexcept { return m_data.max_value; }

    [[nodiscard]] AccessibilityProperties compute_accessibility() const override {
        auto p = RenderBox::compute_accessibility();
        p.role = AccessibilityRole::Slider;
        p.range_min = m_data.min_value;
        p.range_max = m_data.max_value;
        p.range_value = m_data.value;
        p.disabled = m_state.is_disabled;
        return p;
    }
    [[nodiscard]] float step() const noexcept { return m_data.step; }
    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    [[nodiscard]] const SliderData& data() const noexcept { return m_data; }
    [[nodiscard]] const SliderInteractiveState& state() const noexcept { return m_state; }

    [[nodiscard]] float normalized_value() const noexcept {
        float span = m_data.max_value - m_data.min_value;
        if (span <= 0.0001f) return 0.0f;
        return std::clamp((m_data.value - m_data.min_value) / span, 0.0f, 1.0f);
    }

    void set_value(float val, bool notify = true) {
        float clamped = std::clamp(val, m_data.min_value, m_data.max_value);
        if (m_data.step > 0.0f) {
            float steps = std::round((clamped - m_data.min_value) / m_data.step);
            clamped = std::clamp(m_data.min_value + steps * m_data.step, m_data.min_value, m_data.max_value);
        }
        if (std::abs(m_data.value - clamped) > 0.0001f) {
            m_data.value = clamped;
            if (notify && m_data.on_change) {
                m_data.on_change(m_data.value);
            }
            mark_needs_paint();
        }
    }

    void set_range(float min_val, float max_val, float step_val = 0.0f) {
        m_data.min_value = min_val;
        m_data.max_value = std::max(min_val, max_val);
        m_data.step = std::max(0.0f, step_val);
        set_value(m_data.value, false);
        mark_needs_paint();
    }

    void set_track_size(float track_h, float thumb_r, float width) {
        bool changed = false;
        if (track_h > 0.0f && std::abs(m_data.track_height - track_h) > 0.001f) {
            m_data.track_height = track_h;
            changed = true;
        }
        if (thumb_r > 0.0f && std::abs(m_data.thumb_radius - thumb_r) > 0.001f) {
            m_data.thumb_radius = thumb_r;
            changed = true;
        }
        if (width > 0.0f && std::abs(m_data.width - width) > 0.001f) {
            m_data.width = width;
            changed = true;
        }
        if (changed) {
            mark_needs_layout();
        }
    }

    void set_disabled(bool disabled) {
        if (m_state.is_disabled != disabled) {
            m_state.is_disabled = disabled;
            m_data.disabled = disabled;
            is_focusable = !disabled;
            cursor_shape = disabled ? CursorShape::Forbidden : CursorShape::PointingHand;
            mark_needs_paint();
        }
    }

    void set_on_change(std::function<void(float)> cb) { m_data.on_change = std::move(cb); }
    void set_on_change_end(std::function<void(float)> cb) { m_data.on_change_end = std::move(cb); }

    void perform_layout() override {
        float w = m_constraints.constrain_width(m_data.width > 0.0f ? m_data.width : 180.0f);
        float h = m_constraints.constrain_height(m_data.height > 0.0f ? m_data.height : 32.0f);
        set_size(Vec2{w, h});
    }

    void paint(PaintContext& ctx) override {
        update_animations();

        const Theme& theme = ThemeContext::current();
        ResolvedSliderStyle style = SliderStyleResolver::resolve(
            m_data, m_state, theme, m_hover_progress, m_press_progress);

        const float thumb_r = style.thumb_radius;
        const float track_x = thumb_r;
        const float track_w = std::max(0.0f, size().x - thumb_r * 2.0f);
        const float track_y = (size().y - style.track_height) * 0.5f;
        // 拖拽细步进或连续滑块时直接紧跟鼠标浮点位置，消除离散步进跳帧卡顿感
        const float span = m_data.max_value - m_data.min_value;
        const bool smooth_drag = m_state.is_dragging && (m_data.step <= 0.0f || (span > 0.0f && m_data.step <= span * 0.02f));
        const float norm = smooth_drag ? m_drag_norm : normalized_value();
        const float thumb_cx = track_x + track_w * norm;
        const float thumb_cy = size().y * 0.5f;

        // 1. 未激活底槽 (WinUI 3 SliderTrackFill: 4px 胶囊导轨)
        Rect inactive_rect = Rect::from_xywh(track_x, track_y, track_w, style.track_height);
        ctx.draw_rect(inactive_rect, style.track_inactive, style.track_height * 0.5f, 0.0f);

        // 2. 激活高亮轨 (WinUI 3 SliderTrackValueFill)
        if (norm > 0.001f) {
            float active_w = track_w * norm;
            Rect active_rect = Rect::from_xywh(track_x, track_y, active_w, style.track_height);
            ctx.draw_rect(active_rect, style.track_active, style.track_height * 0.5f, 0.0f);
        }

        // 3. 步进刻度线 (WinUI 3 SliderTickBar: 位于导轨下方 4px，1px x 4px 细微刻度，绝不遮挡导轨)
        if (m_data.step > 0.0f) {
            int step_count = static_cast<int>(std::round(span / m_data.step));
            if (step_count >= 2 && step_count <= 24) {
                float tick_y = track_y + style.track_height + 4.0f;
                for (int i = 0; i <= step_count; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(step_count);
                    float tick_x = track_x + track_w * t;
                    ctx.draw_rect(Rect::from_xywh(tick_x - 0.5f, tick_y, 1.0f, 4.0f),
                                  style.tick_color, 0.5f, 0.0f);
                }
            }
        }

        // 4. 拖动手柄 (WinUI 3 规范: 恒定 20px 外环 + 柔和圆形投影 + 悬停扩张/按下收缩同心强调色内圆)
        float cur_r = thumb_r * style.thumb_scale;
        Rect thumb_rect = Rect::from_xywh(thumb_cx - cur_r, thumb_cy - cur_r, cur_r * 2.0f, cur_r * 2.0f);

        // 柔和圆形投影 (纯圆多层低透明衰减，彻底避免矩形阴影伪影)
        if (!m_state.is_disabled && style.shadow_color.a > 0.001f) {
            Color shadow_outer = style.shadow_color;
            shadow_outer.a *= 0.45f;
            float sr1 = cur_r + 1.5f;
            ctx.draw_rect(Rect::from_xywh(thumb_cx - sr1, thumb_cy - sr1 + 1.5f, sr1 * 2.0f, sr1 * 2.0f),
                          shadow_outer, sr1, 0.0f);

            Color shadow_inner = style.shadow_color;
            shadow_inner.a *= 0.80f;
            float sr2 = cur_r + 0.5f;
            ctx.draw_rect(Rect::from_xywh(thumb_cx - sr2, thumb_cy - sr2 + 0.8f, sr2 * 2.0f, sr2 * 2.0f),
                          shadow_inner, sr2, 0.0f);
        }

        // 手柄外环实体 (Dark: #454545, Light: #FFFFFF)
        ctx.draw_rect(thumb_rect, style.thumb_fill, cur_r, 0.0f);

        // 外环精致 1px 立体微边框 (ControlElevationBorder)
        if (style.border_width > 0.0f && style.thumb_border.a > 0.001f) {
            ctx.draw_rect(thumb_rect, style.thumb_border, cur_r, style.border_width);
        }

        // 同心强调色内圆 (WinUI 3 核心微交互: 静止 5.5r -> 悬停 7.0r -> 按下 4.5r)
        float core_r = style.core_radius;
        if (core_r > 0.1f) {
            Rect core_rect = Rect::from_xywh(thumb_cx - core_r, thumb_cy - core_r, core_r * 2.0f, core_r * 2.0f);
            ctx.draw_rect(core_rect, style.thumb_core, core_r, 0.0f);
        }
    }

private:
    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            m_state.is_hovered = true;
            m_last_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_state.is_hovered = false;
            m_last_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        };

        on_pointer_down = [this](PointerEvent& e) {
            if (m_state.is_disabled) return;
            m_state.is_pressed = true;
            m_state.is_dragging = true;
            m_last_anim_time = std::chrono::steady_clock::now();
            update_value_from_pos(e.local_position.x);
            e.stop_propagation();
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (m_state.is_disabled || !m_state.is_dragging) return;
            update_value_from_pos(e.local_position.x);
            e.stop_propagation();
            mark_needs_paint();
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (!m_state.is_dragging && !m_state.is_pressed) return;
            m_state.is_pressed = false;
            m_state.is_dragging = false;
            m_last_anim_time = std::chrono::steady_clock::now();
            if (m_data.on_change_end) {
                m_data.on_change_end(m_data.value);
            }
            e.stop_propagation();
            mark_needs_paint();
        };

        on_focus = [this](FocusEvent& e) {
            m_state.is_focused = e.gained_focus;
            mark_needs_paint();
        };

        on_key = [this](KeyEvent& e) {
            if (e.type != PlatformEventType::KeyDown || m_state.is_disabled) return;
            float step_amt = m_data.step > 0.0f ? m_data.step : (m_data.max_value - m_data.min_value) * 0.02f;
            if (e.key_code == 37 || e.key_code == 40) { // Left / Down
                set_value(m_data.value - step_amt);
                if (m_data.on_change_end) m_data.on_change_end(m_data.value);
            } else if (e.key_code == 39 || e.key_code == 38) { // Right / Up
                set_value(m_data.value + step_amt);
                if (m_data.on_change_end) m_data.on_change_end(m_data.value);
            } else if (e.key_code == 36) { // Home
                set_value(m_data.min_value);
                if (m_data.on_change_end) m_data.on_change_end(m_data.value);
            } else if (e.key_code == 35) { // End
                set_value(m_data.max_value);
                if (m_data.on_change_end) m_data.on_change_end(m_data.value);
            }
        };
    }

    void update_value_from_pos(float local_x) {
        float thumb_r = m_data.thumb_radius > 0.0f ? m_data.thumb_radius : 10.0f;
        float track_w = std::max(1.0f, size().x - thumb_r * 2.0f);
        float norm = std::clamp((local_x - thumb_r) / track_w, 0.0f, 1.0f);
        m_drag_norm = norm;
        float new_val = m_data.min_value + norm * (m_data.max_value - m_data.min_value);
        set_value(new_val, true);
    }

    void update_animations() {
        auto now = std::chrono::steady_clock::now();
        const float target_hover = (m_state.is_hovered || m_state.is_dragging) ? 1.0f : 0.0f;
        const float target_press = (m_state.is_pressed || m_state.is_dragging) ? 1.0f : 0.0f;

        if (m_last_anim_time.time_since_epoch().count() == 0) {
            m_last_anim_time = now;
            if (std::abs(m_hover_progress - target_hover) > 0.005f ||
                std::abs(m_press_progress - target_press) > 0.005f) {
                mark_needs_paint();
            }
            return;
        }
        float dt = std::chrono::duration<float>(now - m_last_anim_time).count();
        m_last_anim_time = now;
        dt = std::clamp(dt, 0.0f, 0.05f);

        bool animating = false;
        if (std::abs(m_hover_progress - target_hover) > 0.005f) {
            m_hover_progress += (target_hover - m_hover_progress) * std::min(1.0f, dt * 18.0f);
            animating = true;
        } else {
            m_hover_progress = target_hover;
        }

        if (std::abs(m_press_progress - target_press) > 0.005f) {
            m_press_progress += (target_press - m_press_progress) * std::min(1.0f, dt * 22.0f);
            animating = true;
        } else {
            m_press_progress = target_press;
        }

        if (animating) {
            mark_needs_paint();
        }
    }

    SliderData m_data;
    SliderInteractiveState m_state;
    float m_drag_norm{0.0f};
    float m_hover_progress{0.0f};
    float m_press_progress{0.0f};
    std::chrono::steady_clock::time_point m_last_anim_time;
};

// C++20 指定初始化器声明式滑块控件 (Slider DSL Wrapper)
struct Slider {
    Bindable<float> value{0.0f};
    Bindable<float> min_value{0.0f};
    Bindable<float> max_value{100.0f};
    Bindable<float> step{0.0f};
    Bindable<float> track_height{4.0f};
    Bindable<float> thumb_radius{10.0f};
    Bindable<float> width{180.0f};
    Bindable<bool> disabled{false};

    std::function<void(float)> on_change{nullptr};
    std::function<void(float)> on_change_end{nullptr};

    operator WidgetPtr() const;
};

class SliderWidget : public Widget {
public:
    explicit SliderWidget(const Slider& desc) : m_desc(desc) {}
    ~SliderWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderSlider>();
        apply_desc_to_node(node);
        setup_bindings(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sl = std::dynamic_pointer_cast<RenderSlider>(node)) {
            apply_desc_to_node(sl);
            setup_bindings(sl);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderSlider>& node) {
        node->set_range(m_desc.min_value.get(), m_desc.max_value.get(), m_desc.step.get());
        node->set_value(m_desc.value.get(), false);
        node->set_disabled(m_desc.disabled.get());
        node->set_track_size(m_desc.track_height.get(), m_desc.thumb_radius.get(), m_desc.width.get());
        node->set_on_change([this](float val) {
            m_is_internal_update = true;
            m_desc.value.set_bound(val);
            if (m_desc.on_change) m_desc.on_change(val);
            m_is_internal_update = false;
        });
        node->set_on_change_end(m_desc.on_change_end);
    }

    void setup_bindings(const std::shared_ptr<RenderSlider>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.value, node, [this](RenderSlider& n, float v) {
            if (!m_is_internal_update) n.set_value(v, false);
        });
        m_bindings.attach(m_desc.min_value, node, [this](RenderSlider& n, float) {
            n.set_range(m_desc.min_value.get(), m_desc.max_value.get(), m_desc.step.get());
        });
        m_bindings.attach(m_desc.max_value, node, [this](RenderSlider& n, float) {
            n.set_range(m_desc.min_value.get(), m_desc.max_value.get(), m_desc.step.get());
        });
        m_bindings.attach(m_desc.step, node, [this](RenderSlider& n, float) {
            n.set_range(m_desc.min_value.get(), m_desc.max_value.get(), m_desc.step.get());
        });
        m_bindings.attach(m_desc.disabled, node, [](RenderSlider& n, bool v) { n.set_disabled(v); });
    }

    Slider m_desc;
    BindingGroup m_bindings;
    bool m_is_internal_update{false};
};

inline Slider::operator WidgetPtr() const {
    return std::make_shared<SliderWidget>(*this);
}

} // namespace ktui
