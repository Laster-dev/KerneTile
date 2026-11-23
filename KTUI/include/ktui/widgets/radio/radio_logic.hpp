#pragma once

// From Uiverse.io by mahiatlinux - https://uiverse.io/mahiatlinux/neat-donkey-46
// MIT License, Copyright (c) Yaya12085 (Yaya Mohamed), mahiatlinux (Maheswar Kumar)

#include "radio_data.hpp"
#include "radio_style.hpp"
#include <ktui/animation/cubic_bezier.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/widgets/widget.hpp>
#include <ktui/widgets/hstack.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// 单选卡片渲染盒 (RenderRadioButton - Logic Layer)
// 复刻 Uiverse.io (mahiatlinux/neat-donkey-46) 交互与三路过渡动效：
//   1. 卡片 2px 边框颜色过渡 (150ms ease)
//   2. 左上角圆形单选指示点 :before 缩放 + 淡入 + 选中实心填充 (250ms ease)
//   3. 图标与文字标签颜色平滑过渡 (375ms ease)
class RenderRadioButton : public RenderBox {
public:
    RenderRadioButton() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderRadioButton() override = default;

    [[nodiscard]] bool checked() const noexcept { return m_data.checked; }
    [[nodiscard]] int value() const noexcept { return m_data.value; }
    [[nodiscard]] const std::string& label() const noexcept { return m_data.label; }
    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }

    void set_checked(bool checked, bool animate = true) {
        if (m_data.checked == checked && m_initialized) return;
        m_data.checked = checked;
        if (!animate || !m_initialized) {
            m_initialized = true;
            sync_targets_immediate();
        } else {
            update_anim_targets();
        }
        mark_needs_paint();
    }

    void set_value(int val) noexcept { m_data.value = val; }

    void set_label(std::string label) {
        if (m_data.label != label) {
            m_data.label = std::move(label);
            mark_needs_layout();
        }
    }

    void set_icon_text(std::string icon) {
        if (m_data.icon_text != icon) {
            m_data.icon_text = std::move(icon);
            mark_needs_paint();
        }
    }

    void set_icon_painter(std::function<void(PaintContext&, Rect, Color)> painter) {
        m_data.icon_painter = std::move(painter);
        mark_needs_paint();
    }

    void set_disabled(bool disabled) {
        if (m_state.is_disabled != disabled) {
            m_state.is_disabled = disabled;
            m_data.disabled = disabled;
            is_focusable = !disabled;
            cursor_shape = disabled ? CursorShape::Forbidden : CursorShape::PointingHand;
            update_anim_targets();
            mark_needs_paint();
        }
    }

    void set_tile_size(float w, float h) {
        bool changed = false;
        if (w > 0.0f && m_data.tile_width != w) { m_data.tile_width = w; changed = true; }
        if (h > 0.0f && m_data.tile_height != h) { m_data.tile_height = h; changed = true; }
        if (changed) mark_needs_layout();
    }

    void set_corner_radius(float r) {
        if (r > 0.0f && m_data.corner_radius != r) {
            m_data.corner_radius = r;
            mark_needs_paint();
        }
    }

    void set_font_size(float fs) {
        if (fs > 0.0f && m_data.font_size != fs) {
            m_data.font_size = fs;
            mark_needs_layout();
        }
    }

    void set_on_select(std::function<void(int)> cb) {
        m_data.on_select = std::move(cb);
    }

    void select() {
        if (m_state.is_disabled) return;
        if (!m_data.checked) {
            set_checked(true, true);
        }
        if (m_data.on_select) {
            m_data.on_select(m_data.value);
        }
    }

    void perform_layout() override {
        const Theme& theme = ThemeContext::current();
        auto style = RadioStyleResolver::resolve(m_data, m_state, theme);
        m_size = m_constraints.constrain(Vec2{style.tile_width, style.tile_height});
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        auto style = RadioStyleResolver::resolve(m_data, m_state, theme);

        // 1. 推进三路过渡动效 (border 150ms, dot 250ms, label/icon 375ms)
        auto now = std::chrono::steady_clock::now();
        float dt = 0.0f;
        if (m_last_tick_time.time_since_epoch().count() != 0) {
            dt = std::clamp(std::chrono::duration<float>(now - m_last_tick_time).count(), 0.0f, 0.1f);
        }
        m_last_tick_time = now;

        bool active = false;
        active |= step_track(m_border_from, m_border_to, m_border_val, m_border_elapsed, kBorderDurSec, dt);
        active |= step_track(m_dot_from, m_dot_to, m_dot_val, m_dot_elapsed, kDotDurSec, dt);
        active |= step_track(m_checked_from, m_checked_to, m_checked_val, m_checked_elapsed, kLabelDurSec, dt);
        if (active) {
            mark_needs_paint();
        }

        const Rect b = bounds();

        // 2. 键盘聚焦外环 (对应 :focus 的 0 0 0 4px 外光圈)
        if (m_state.is_focused && !m_state.is_disabled) {
            Rect halo = Rect::from_xywh(b.min_x - 3.0f, b.min_y - 3.0f, b.width() + 6.0f, b.height() + 6.0f);
            ctx.draw_rect(halo, style.focus_ring, style.corner_radius + 3.0f, 3.0f);
        }

        // 3. 卡片底色与 2px 边框 (.radio-tile)
        ctx.draw_rect(b, style.background, style.corner_radius, 0.0f);
        const Color border_col = lerp_color(style.border_idle, style.accent, m_border_val);
        ctx.draw_rect(b, border_col, style.corner_radius, style.border_width);

        // 4. 左上角圆形指示点 (:before — scale 0->1, opacity 0->1, 250ms ease)
        if (m_dot_val > 0.01f) {
            const float full_d = style.dot_diameter;
            const float d = full_d * m_dot_val;
            const float cx = b.min_x + style.dot_margin + full_d * 0.5f;
            const float cy = b.min_y + style.dot_margin + full_d * 0.5f;
            const Rect dot_rect = Rect::from_xywh(cx - d * 0.5f, cy - d * 0.5f, d, d);

            Color dot_fill = lerp_color(style.dot_bg, style.accent, m_checked_val);
            dot_fill.a *= m_dot_val;
            Color dot_border = lerp_color(style.border_idle, style.accent, m_checked_val);
            dot_border.a *= m_dot_val;

            ctx.draw_rect(dot_rect, dot_fill, d * 0.5f, 0.0f);
            ctx.draw_rect(dot_rect, dot_border, d * 0.5f, std::min(2.0f, d * 0.35f));
        }

        // 5. 居中图标与文字 (.radio-icon + .radio-label, 375ms ease 色彩过渡)
        const bool has_icon = static_cast<bool>(m_data.icon_painter) || !m_data.icon_text.empty();
        const bool has_label = !m_data.label.empty();
        const Color icon_col = lerp_color(style.icon_idle, style.accent, m_checked_val);
        const Color label_col = lerp_color(style.label_idle, style.accent, m_checked_val);

        constexpr float kIconSize = 24.0f;
        constexpr float kIconLabelGap = 4.0f;
        Vec2 label_sz = has_label ? TextMeasure::measure(m_data.label, style.font_size) : Vec2{0.0f, 0.0f};
        float content_h = (has_icon ? kIconSize : 0.0f) +
                          ((has_icon && has_label) ? kIconLabelGap : 0.0f) +
                          (has_label ? label_sz.y : 0.0f);
        float cur_y = b.min_y + (b.height() - content_h) * 0.5f;

        if (has_icon) {
            Rect icon_rect = Rect::from_xywh(b.min_x + (b.width() - kIconSize) * 0.5f, cur_y, kIconSize, kIconSize);
            if (m_data.icon_painter) {
                m_data.icon_painter(ctx, icon_rect, icon_col);
            } else {
                Vec2 glyph_sz = TextMeasure::measure(m_data.icon_text, 18.0f);
                ctx.draw_text(m_data.icon_text,
                              PointF{icon_rect.min_x + (icon_rect.width() - glyph_sz.x) * 0.5f,
                                     icon_rect.min_y + (icon_rect.height() - glyph_sz.y) * 0.5f},
                              18.0f, icon_col);
            }
            cur_y += kIconSize + kIconLabelGap;
        }

        if (has_label) {
            float label_x = b.min_x + (b.width() - label_sz.x) * 0.5f;
            ctx.draw_text(m_data.label, PointF{label_x, cur_y}, style.font_size, label_col);
        }
    }

private:
    static Color lerp_color(const Color& a, const Color& b, float t) noexcept {
        float c = std::clamp(t, 0.0f, 1.0f);
        return Color{
            a.r + (b.r - a.r) * c,
            a.g + (b.g - a.g) * c,
            a.b + (b.b - a.b) * c,
            a.a + (b.a - a.a) * c
        };
    }

    static bool step_track(float from, float to, float& val, float& elapsed, float dur, float dt) noexcept {
        if (elapsed < dur) {
            elapsed = std::min(dur, elapsed + dt);
            float t = kEaseCurve.evaluate(elapsed / dur);
            val = from + (to - from) * t;
            return elapsed < dur;
        }
        val = to;
        return false;
    }

    void sync_targets_immediate() noexcept {
        const float b_target = (!m_state.is_disabled && (m_data.checked || m_state.is_hovered || m_state.is_focused)) ? 1.0f : 0.0f;
        const float c_target = m_data.checked ? 1.0f : 0.0f;
        m_border_from = m_border_to = m_border_val = b_target;
        m_border_elapsed = kBorderDurSec;
        m_dot_from = m_dot_to = m_dot_val = b_target;
        m_dot_elapsed = kDotDurSec;
        m_checked_from = m_checked_to = m_checked_val = c_target;
        m_checked_elapsed = kLabelDurSec;
    }

    void update_anim_targets() {
        const float b_target = (!m_state.is_disabled && (m_data.checked || m_state.is_hovered || m_state.is_focused)) ? 1.0f : 0.0f;
        const float c_target = m_data.checked ? 1.0f : 0.0f;

        if (m_border_to != b_target) {
            m_border_from = m_border_val;
            m_border_to = b_target;
            m_border_elapsed = 0.0f;
        }
        if (m_dot_to != b_target) {
            m_dot_from = m_dot_val;
            m_dot_to = b_target;
            m_dot_elapsed = 0.0f;
        }
        if (m_checked_to != c_target) {
            m_checked_from = m_checked_val;
            m_checked_to = c_target;
            m_checked_elapsed = 0.0f;
        }
        m_last_tick_time = std::chrono::steady_clock::now();
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = true;
                is_hovered = true;
                update_anim_targets();
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = false;
                m_state.is_pressed = false;
                is_hovered = false;
                update_anim_targets();
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && ev.button == PointerButton::Primary) {
                m_state.is_pressed = true;
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && ev.button == PointerButton::Primary && m_state.is_pressed) {
                m_state.is_pressed = false;
                select();
            }
        };

        on_focus = [this](FocusEvent& ev) {
            m_state.is_focused = ev.gained_focus;
            update_anim_targets();
            mark_needs_paint();
        };

        on_key = [this](KeyEvent& ev) {
            if (ev.type == PlatformEventType::KeyDown && !m_state.is_disabled) {
                if (ev.key_code == 32 || ev.key_code == 13) { // Space or Enter
                    select();
                }
            }
        };
    }

    static constexpr float kBorderDurSec = 0.15f; // 150ms ease
    static constexpr float kDotDurSec = 0.25f;    // 250ms ease
    static constexpr float kLabelDurSec = 0.375f; // 375ms ease
    static constexpr CubicBezier kEaseCurve{0.25f, 0.10f, 0.25f, 1.00f};

    RadioButtonData m_data;
    RadioButtonInteractiveState m_state;

    bool m_initialized{false};
    float m_border_from{0.0f}, m_border_to{0.0f}, m_border_val{0.0f}, m_border_elapsed{kBorderDurSec};
    float m_dot_from{0.0f}, m_dot_to{0.0f}, m_dot_val{0.0f}, m_dot_elapsed{kDotDurSec};
    float m_checked_from{0.0f}, m_checked_to{0.0f}, m_checked_val{0.0f}, m_checked_elapsed{kLabelDurSec};
    std::chrono::steady_clock::time_point m_last_tick_time{};
};

// ==================== 声明式单个单选卡片 (RadioButton) ====================
struct RadioButton {
    Bindable<int> selected_value{0};
    int value{0};
    Bindable<std::string> label{""};
    std::string icon_text{""};
    std::function<void(PaintContext&, Rect, Color)> icon_painter{nullptr};
    Bindable<bool> disabled{false};
    float tile_width{80.0f};
    float tile_height{72.0f};
    float corner_radius{8.0f};
    float font_size{13.0f};

    std::function<void(int)> on_change{nullptr};

    operator WidgetPtr() const;
};

class RadioButtonWidget : public Widget {
public:
    explicit RadioButtonWidget(const RadioButton& desc) : m_desc(desc) {}
    ~RadioButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderRadioButton>();
        apply_desc_to_node(node, false);
        setup_bindings(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto rb = std::dynamic_pointer_cast<RenderRadioButton>(node)) {
            apply_desc_to_node(rb, true);
            setup_bindings(rb);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderRadioButton>& node, bool animate) {
        node->set_value(m_desc.value);
        node->set_checked(m_desc.selected_value.get() == m_desc.value, animate);
        node->set_label(m_desc.label.get());
        node->set_icon_text(m_desc.icon_text);
        node->set_icon_painter(m_desc.icon_painter);
        node->set_disabled(m_desc.disabled.get());
        node->set_tile_size(m_desc.tile_width, m_desc.tile_height);
        node->set_corner_radius(m_desc.corner_radius);
        node->set_font_size(m_desc.font_size);
        node->set_on_select([this](int val) {
            m_is_internal_update = true;
            m_desc.selected_value.set_bound(val);
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
            m_is_internal_update = false;
        });
    }

    void setup_bindings(const std::shared_ptr<RenderRadioButton>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.selected_value, node, [this](RenderRadioButton& n, int cur_selected) {
            if (!m_is_internal_update) {
                n.set_checked(cur_selected == m_desc.value, true);
            }
        });
        m_bindings.attach(m_desc.label, node, [](RenderRadioButton& n, const std::string& v) { n.set_label(v); });
        m_bindings.attach(m_desc.disabled, node, [](RenderRadioButton& n, bool v) { n.set_disabled(v); });
    }

    RadioButton m_desc;
    BindingGroup m_bindings;
    bool m_is_internal_update{false};
};

inline RadioButton::operator WidgetPtr() const {
    return std::make_shared<RadioButtonWidget>(*this);
}

// ==================== 声明式单选卡片组 (RadioGroup) ====================
struct RadioGroup {
    Bindable<int> selected_value{0};
    std::vector<RadioOption> options{};
    float spacing{12.0f};
    float tile_width{80.0f};
    float tile_height{72.0f};
    float corner_radius{8.0f};
    float font_size{13.0f};
    std::function<void(int)> on_change{nullptr};

    operator WidgetPtr() const {
        std::vector<WidgetPtr> items;
        items.reserve(options.size());
        for (const auto& opt : options) {
            items.push_back(RadioButton {
                .selected_value = selected_value,
                .value = opt.value,
                .label = opt.label,
                .icon_text = opt.icon_text,
                .icon_painter = opt.icon_painter,
                .disabled = opt.disabled,
                .tile_width = tile_width,
                .tile_height = tile_height,
                .corner_radius = corner_radius,
                .font_size = font_size,
                .on_change = on_change
            });
        }
        return HStack {
            .align_items = AlignItems::Center,
            .spacing = spacing,
            .content = std::move(items)
        };
    }
};

} // namespace ktui
