#pragma once

// From Uiverse.io by Galahhad - https://uiverse.io/Galahhad/modern-quail-75
// MIT License, Copyright (c) Galahhad (Galahad)

#include "toggle_switch_data.hpp"
#include "toggle_switch_style.hpp"
#include <ktui/animation/cubic_bezier.hpp>
#include <ktui/animation/motion.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
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

// 开关逻辑与渲染盒 (RenderToggleSwitch - Logic Layer)
// 复刻 Galahhad/modern-quail-75 四路弹性动效：
//   1. 轨道颜色过渡 (200ms cubic-bezier(0.27, 0.2, 0.25, 1.51))
//   2. 圆球弹性回弹滑动 (200ms cubic-bezier(0.27, 0.2, 0.25, 1.51))
//   3. 尾随拉伸高光条 ::before (200ms ease-in-out)
//   4. 圆球内 × 与 ✓ 缩放交替切换 (200ms cubic-bezier(0.27, 0.2, 0.25, 1.51))
class RenderToggleSwitch : public RenderBox {
public:
    RenderToggleSwitch() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderToggleSwitch() override = default;

    [[nodiscard]] bool checked() const noexcept { return m_data.checked; }
    [[nodiscard]] const std::string& label() const noexcept { return m_data.label; }
    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    [[nodiscard]] const ToggleSwitchData& data() const noexcept { return m_data; }
    [[nodiscard]] const ToggleSwitchInteractiveState& state() const noexcept { return m_state; }

    void set_checked(bool checked, bool animate = true) {
        if (m_data.checked == checked && m_anim_initialized) return;
        m_data.checked = checked;
        const float target = checked ? 1.0f : 0.0f;
        if (!animate || !m_anim_initialized) {
            m_anim_initialized = true;
            m_from = target;
            m_to = target;
            m_bounce_val = target;
            m_effect_val = target;
            m_elapsed = kDurationSec;
        } else {
            m_from = std::clamp(m_bounce_val, 0.0f, 1.0f);
            m_to = target;
            m_elapsed = 0.0f;
            m_last_tick_time = std::chrono::steady_clock::now();
        }
        mark_needs_paint();
    }

    void toggle() {
        if (m_state.is_disabled) return;
        set_checked(!m_data.checked, true);
        if (m_data.on_change) {
            m_data.on_change(m_data.checked);
        }
    }

    void set_label(std::string label) {
        if (m_data.label != label) {
            m_data.label = std::move(label);
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

    void set_track_size(float width, float height) {
        bool changed = false;
        if (width > 0.0f && m_data.track_width != width) {
            m_data.track_width = width;
            changed = true;
        }
        if (height > 0.0f && m_data.track_height != height) {
            m_data.track_height = height;
            changed = true;
        }
        if (changed) mark_needs_layout();
    }

    void set_font_size(float fs) {
        if (fs > 0.0f && m_data.font_size != fs) {
            m_data.font_size = fs;
            mark_needs_layout();
        }
    }

    void set_gap(float gap) {
        if (m_data.gap != gap) {
            m_data.gap = gap;
            mark_needs_layout();
        }
    }

    void set_on_change(Fn<void(bool)> cb) {
        m_data.on_change = std::move(cb);
    }

    void set_inset_shadow(bool enabled) {
        if (m_data.inset_shadow != enabled) {
            m_data.inset_shadow = enabled;
            mark_needs_paint();
        }
    }

    void set_liquid_thumb(bool enabled) {
        if (m_data.liquid_thumb != enabled) {
            m_data.liquid_thumb = enabled;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        const Theme& theme = ThemeContext::current();
        auto style = ToggleSwitchStyleResolver::resolve(m_data, m_state, theme);
        float desired_w = style.track_width;
        float desired_h = style.track_height;

        if (!m_data.label.empty()) {
            Vec2 txt_sz = TextMeasure::measure(m_data.label, style.font_size);
            desired_w += style.gap + txt_sz.x;
            desired_h = std::max(desired_h, txt_sz.y);
        }

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        auto style = ToggleSwitchStyleResolver::resolve(m_data, m_state, theme);

        // 1. 推进 200ms 弹性曲线与尾随 ease-in-out 曲线
        auto now = std::chrono::steady_clock::now();
        float dt = 0.0f;
        if (m_last_tick_time.time_since_epoch().count() != 0) {
            dt = std::clamp(std::chrono::duration<float>(now - m_last_tick_time).count(), 0.0f, 0.1f);
        }
        m_last_tick_time = now;

        if (!m_touch_dragging) {
            if (m_elapsed < kDurationSec) {
                m_elapsed = std::min(kDurationSec, m_elapsed + dt);
                const float norm = m_elapsed / kDurationSec;
                const float b_t = kBounceCurve.evaluate(norm);
                const float e_t = kEffectCurve.evaluate(norm);
                m_bounce_val = m_from + (m_to - m_from) * b_t;
                m_effect_val = m_from + (m_to - m_from) * e_t;
                if (m_elapsed < kDurationSec) {
                    mark_needs_paint();
                }
            } else {
                m_bounce_val = m_to;
                m_effect_val = m_to;
            }
        }

        const Rect b = bounds();
        const float track_x = b.min_x;
        const float track_y = b.min_y + (b.height() - style.track_height) * 0.5f;
        const float track_r = style.track_height * 0.5f;

        // 2. 绘制胶囊轨道 (背景色按弹性进度平滑过渡，可选解析高斯内阴影呈现凹槽纵深)
        const Color track_col = lerp_color(style.track_off, style.track_on, std::clamp(m_bounce_val, 0.0f, 1.0f));
        const Rect track_rect = Rect::from_xywh(track_x, track_y, style.track_width, style.track_height);
        ctx.draw_rect(track_rect, track_col, track_r, 0.0f);
        if (m_data.inset_shadow) {
            const Color inner_shadow = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.42f} : Color{0.0f, 0.04f, 0.12f, 0.22f};
            ctx.draw_inset_shadow(track_rect, inner_shadow, track_r, 3.2f, 0.5f, Vec2{0.0f, 1.4f});
        }

        // 3. 绘制尾随拉伸小条 (::before, 200ms ease-in-out 略微滞后于圆球产生弹性拖尾感)
        const float effect_w = style.thumb_diameter * 0.5f;
        const float effect_h = std::max(2.0f, effect_w * 0.5f - 1.0f);
        const float effect_left_off = style.thumb_offset + effect_w * 0.5f;
        const float effect_left_on = style.track_width - effect_w - effect_w * 0.5f - style.thumb_offset;
        const float effect_x = track_x + effect_left_off + (effect_left_on - effect_left_off) * m_effect_val;
        const float effect_y = track_y + (style.track_height - effect_h) * 0.5f;

        // 4. 绘制弹性滑动圆球 (.circle, 200ms cubic-bezier(0.27, 0.2, 0.25, 1.51))
        const float thumb_left_off = style.thumb_offset;
        const float thumb_left_on = style.track_width - style.thumb_diameter - style.thumb_offset;
        const float thumb_x = track_x + thumb_left_off + (thumb_left_on - thumb_left_off) * m_bounce_val;
        const float thumb_y = track_y + style.thumb_offset;
        const float thumb_r = style.thumb_diameter * 0.5f;
        const Rect thumb_rect = Rect::from_xywh(thumb_x, thumb_y, style.thumb_diameter, style.thumb_diameter);

        if (m_data.liquid_thumb && std::abs(m_effect_val - m_bounce_val) > 0.01f) {
            const Rect trail_rect = Rect::from_xywh(effect_x, track_y + (style.track_height - style.thumb_diameter * 0.52f) * 0.5f,
                                                   style.thumb_diameter * 0.55f, style.thumb_diameter * 0.52f);
            ctx.draw_metaball(thumb_rect, thumb_r, trail_rect, trail_rect.height() * 0.5f, 8.0f, style.thumb, 0.0f);
        } else {
            ctx.draw_rect(Rect::from_xywh(effect_x, effect_y, effect_w, effect_h), style.thumb, 1.0f, 0.0f);
            ctx.draw_rect(thumb_rect, style.thumb, thumb_r, 0.0f);
        }

        // 5. 圆球内部 × 与 ✓ 缩放交替动画 (跟随弹性曲线回弹缩放)
        const float cx = thumb_x + thumb_r;
        const float cy = thumb_y + thumb_r;

        // 5.1 关闭态 × (scale: 1 -> 0)
        const float cross_scale = std::max(0.0f, 1.0f - m_bounce_val);
        if (cross_scale > 0.04f) {
            const float half = (style.thumb_diameter * 0.17f) * cross_scale;
            const float stroke = std::max(1.0f, 1.35f * std::min(1.1f, cross_scale));
            ctx.draw_line(PointF{cx - half, cy - half}, PointF{cx + half, cy + half}, style.cross_color, stroke);
            ctx.draw_line(PointF{cx + half, cy - half}, PointF{cx - half, cy + half}, style.cross_color, stroke);
        }

        // 5.2 开启态 ✓ (scale: 0 -> 1，带轻微过冲弹跳)
        const float check_scale = std::max(0.0f, m_bounce_val);
        if (check_scale > 0.04f) {
            const float s = (style.thumb_diameter * 0.26f) * check_scale;
            const float stroke = std::max(1.1f, 1.45f * std::min(1.15f, check_scale));
            const PointF p0{cx - s * 0.95f, cy + s * 0.05f};
            const PointF p1{cx - s * 0.20f, cy + s * 0.80f};
            const PointF p2{cx + s * 1.05f, cy - s * 0.72f};
            ctx.draw_line(p0, p1, style.check_color, stroke);
            ctx.draw_line(p1, p2, style.check_color, stroke);
        }

        // 6. 可选右侧标签文字
        if (!m_data.label.empty()) {
            Vec2 txt_sz = TextMeasure::measure(m_data.label, style.font_size);
            float label_x = track_x + style.track_width + style.gap;
            float label_y = b.min_y + (b.height() - txt_sz.y) * 0.5f;
            ctx.draw_text(m_data.label, PointF{label_x, label_y}, style.font_size, style.label_color);
        }
    }

    bool hit_test_touch(HitTestResult& result, PointF position) override {
        if (touch::expand_hit_rect_for_touch(bounds(), touch::kMinTargetSize).contains(position)) {
            result.add(this, position);
            return true;
        }
        return false;
    }

private:
    static Color lerp_color(const Color& a, const Color& b, float t) noexcept {
        return Motion::lerp(a, b, std::clamp(t, 0.0f, 1.0f));
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = true;
                is_hovered = true;
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = false;
                m_state.is_pressed = false;
                is_hovered = false;
                mark_needs_paint();
            }
        };

        on_pointer_preview = [this](PointerEvent& ev) {
            if (m_state.is_disabled || ev.device_kind != PointerDeviceKind::Touch) return;
            if (ev.type == PlatformEventType::PointerDown) {
                m_touch_down = true;
                m_touch_dragging = false;
                m_touch_start_x = ev.position.x;
                m_touch_start_y = ev.position.y;
            } else if (ev.type == PlatformEventType::PointerMove && m_touch_down && !m_touch_dragging) {
                float dx = ev.position.x - m_touch_start_x;
                float dy = ev.position.y - m_touch_start_y;
                if (touch::is_horizontal_drag(dx, dy, 8.0f)) {
                    m_touch_dragging = true;
                    m_state.is_pressed = true;
                    float base = m_data.checked ? 1.0f : 0.0f;
                    float norm = std::clamp(base + dx / 22.0f, 0.0f, 1.0f);
                    m_bounce_val = norm;
                    m_effect_val = norm;
                    ev.gesture_capture_target = this;
                    ev.stop_propagation();
                    mark_needs_paint();
                }
            }
        };

        on_pointer_move = [this](PointerEvent& ev) {
            if (m_state.is_disabled || !m_touch_dragging) return;
            float dx = ev.position.x - m_touch_start_x;
            float base = m_data.checked ? 1.0f : 0.0f;
            float norm = std::clamp(base + dx / 22.0f, 0.0f, 1.0f);
            m_bounce_val = norm;
            m_effect_val = norm;
            ev.stop_propagation();
            mark_needs_paint();
        };

        on_pointer_cancel = [this](PointerEvent&) {
            m_touch_down = false;
            m_touch_dragging = false;
            m_state.is_hovered = false;
            m_state.is_pressed = false;
            is_hovered = false;
            m_bounce_val = m_data.checked ? 1.0f : 0.0f;
            m_effect_val = m_bounce_val;
            mark_needs_paint();
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && ev.button == PointerButton::Primary) {
                m_state.is_pressed = true;
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && ev.button == PointerButton::Primary && (m_state.is_pressed || m_touch_dragging)) {
                const bool was_drag = m_touch_dragging;
                m_touch_down = false;
                m_touch_dragging = false;
                m_state.is_pressed = false;
                if (ev.device_kind == PointerDeviceKind::Touch) {
                    m_state.is_hovered = false;
                    is_hovered = false;
                }
                if (was_drag) {
                    bool want = (m_bounce_val >= 0.5f);
                    if (want != m_data.checked) {
                        toggle();
                    } else {
                        set_checked(m_data.checked, true);
                    }
                } else {
                    toggle();
                }
            }
        };

        on_focus = [this](FocusEvent& ev) {
            m_state.is_focused = ev.gained_focus;
            mark_needs_paint();
        };

        on_key = [this](KeyEvent& ev) {
            if (ev.type == PlatformEventType::KeyDown && !m_state.is_disabled) {
                if (ev.key_code == 32 || ev.key_code == 13) { // Space or Enter
                    toggle();
                }
            }
        };
    }

    static constexpr float kDurationSec = 0.20f; // 200ms
    static constexpr CubicBezier kBounceCurve{0.27f, 0.20f, 0.25f, 1.51f};
    static constexpr CubicBezier kEffectCurve{0.42f, 0.00f, 0.58f, 1.00f};

    ToggleSwitchData m_data;
    ToggleSwitchInteractiveState m_state;
    bool m_touch_down{false};
    bool m_touch_dragging{false};
    float m_touch_start_x{0.0f};
    float m_touch_start_y{0.0f};

    bool m_anim_initialized{false};
    float m_from{0.0f};
    float m_to{0.0f};
    float m_bounce_val{0.0f};
    float m_effect_val{0.0f};
    float m_elapsed{kDurationSec};
    std::chrono::steady_clock::time_point m_last_tick_time{};
};

// C++20 指定初始化器声明式开关控件 (ToggleSwitch DSL Wrapper)
struct ToggleSwitch : BindableMixin<ToggleSwitch, RenderToggleSwitch> {
    bool checked{false};
    std::string label{""};
    bool disabled{false};
    float track_width{40.0f};
    float track_height{20.0f};
    float font_size{13.0f};
    float gap{8.0f};
    bool inset_shadow{false};
    bool liquid_thumb{false};

    Fn<void(bool)> on_change{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderToggleSwitch>& node, Property<bool>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_checked(prop.get(), false);
    std::weak_ptr<RenderToggleSwitch> weak = node;
    bg.subscribe(prop, [weak, guard](bool v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_checked(v, true);
        }
    });
    auto prev = node->data().on_change;
    Property<bool>* p = &prop;
    node->set_on_change([prev = std::move(prev), p, guard](bool val) {
        *guard = true;
        p->set(val);
        if (prev) prev(val);
        *guard = false;
    });
}

class ToggleSwitchWidget : public Widget {
public:
    explicit ToggleSwitchWidget(const ToggleSwitch& desc) : m_desc(desc) {}
    explicit ToggleSwitchWidget(ToggleSwitch&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ToggleSwitchWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderToggleSwitch>();
        apply_desc_to_node(node, false);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sw = std::static_pointer_cast<RenderToggleSwitch>(node)) {
            apply_desc_to_node(sw, true);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderToggleSwitch>& node, bool animate) {
        node->set_checked(m_desc.checked, animate);
        node->set_label(m_desc.label);
        node->set_disabled(m_desc.disabled);
        node->set_track_size(m_desc.track_width, m_desc.track_height);
        node->set_font_size(m_desc.font_size);
        node->set_gap(m_desc.gap);
        node->set_inset_shadow(m_desc.inset_shadow);
        node->set_liquid_thumb(m_desc.liquid_thumb);
        node->set_on_change(m_desc.on_change);
    }

    ToggleSwitch m_desc;
};

inline ToggleSwitch::operator WidgetPtr() const& {
    return make_widget<ToggleSwitchWidget>(*this);
}
inline ToggleSwitch::operator WidgetPtr() && {
    return make_widget<ToggleSwitchWidget>(std::move(*this));
}

} // namespace ktui
