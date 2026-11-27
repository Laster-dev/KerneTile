#pragma once

// Classic WinUI 3 RadioButton + RadioGroup
// Plus sharpened RadioCard + RadioCardGroup (单选卡片 / 单选卡片组)

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
#include <ktui/widgets/hstack/hstack_logic.hpp>
#include <ktui/widgets/vstack/vstack_logic.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// =============================================================================
// 1. 经典 WinUI 3 单选按钮 (RenderRadioButton / RadioButton / RadioGroup)
// =============================================================================
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
    [[nodiscard]] const RadioButtonData& data() const noexcept { return m_data; }

    void set_checked(bool checked, bool animate = true) {
        if (m_data.checked == checked && m_initialized) return;
        m_data.checked = checked;
        if (!animate || !m_initialized) {
            m_initialized = true;
            m_check_progress = checked ? 1.0f : 0.0f;
        } else {
            m_last_anim_time = std::chrono::steady_clock::now();
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

    void set_disabled(bool disabled) {
        if (m_state.is_disabled != disabled) {
            m_state.is_disabled = disabled;
            m_data.disabled = disabled;
            is_focusable = !disabled;
            cursor_shape = disabled ? CursorShape::Forbidden : CursorShape::PointingHand;
            mark_needs_paint();
        }
    }

    void set_font_size(float fs) {
        if (fs > 0.0f && m_data.font_size != fs) {
            m_data.font_size = fs;
            mark_needs_layout();
        }
    }

    void set_on_select(Fn<void(int)> cb) { m_data.on_select = std::move(cb); }

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
        auto s = WinUIRadioStyleResolver::resolve(m_data, m_state, theme);
        Vec2 label_sz = m_data.label.empty() ? Vec2{0.0f, 0.0f} : TextMeasure::measure(m_data.label, s.font_size);
        float w = s.outer_diameter + (m_data.label.empty() ? 0.0f : (s.gap + label_sz.x + 4.0f));
        float h = std::max(s.row_height, label_sz.y + 6.0f);
        m_size = m_constraints.constrain(Vec2{std::ceil(w), std::ceil(h)});
    }

    void paint(PaintContext& ctx) override {
        update_animations();

        const Theme& theme = ThemeContext::current();
        auto s = WinUIRadioStyleResolver::resolve(m_data, m_state, theme, m_hover_progress, m_press_progress);

        const float d = s.outer_diameter;
        const float r = d * 0.5f;
        const float cy = std::round(size().y * 0.5f);
        const float cx = r;

        Rect outer_rect = Rect::from_xywh(cx - r, cy - r, d, d);

        // 1. 外圆底色 + 未选中 1px 精致描边
        if (s.outer_fill.a > 0.001f) {
            ctx.draw_rect(outer_rect, s.outer_fill, r, 0.0f);
        }
        if (s.outer_border.a > 0.001f && s.border_width > 0.0f) {
            ctx.draw_rect(outer_rect, s.outer_border, r, s.border_width);
        }

        // 2. 选中态同心圆点 (WinUI 3 标志性悬停舒张 / 按下收紧微动效)
        if (m_check_progress > 0.01f && s.inner_dot.a > 0.001f) {
            float ir = s.inner_radius * m_check_progress;
            Rect inner_rect = Rect::from_xywh(cx - ir, cy - ir, ir * 2.0f, ir * 2.0f);
            ctx.draw_rect(inner_rect, s.inner_dot, ir, 0.0f);
        }

        // 3. 整数像素对齐锐利标签文字
        if (!m_data.label.empty()) {
            Vec2 label_sz = TextMeasure::measure(m_data.label, s.font_size);
            float tx = std::round(d + s.gap);
            float ty = std::round((size().y - label_sz.y) * 0.5f);
            ctx.draw_text(m_data.label, PointF{tx, ty}, s.font_size, s.text);
        }
    }

private:
    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = true;
                is_hovered = true;
                m_last_anim_time = std::chrono::steady_clock::now();
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = false;
                m_state.is_pressed = false;
                is_hovered = false;
                m_last_anim_time = std::chrono::steady_clock::now();
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && ev.button == PointerButton::Primary) {
                m_state.is_pressed = true;
                m_last_anim_time = std::chrono::steady_clock::now();
                mark_needs_paint();
            }
        };
        on_pointer_up = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && ev.button == PointerButton::Primary && m_state.is_pressed) {
                m_state.is_pressed = false;
                m_last_anim_time = std::chrono::steady_clock::now();
                select();
            }
        };
        on_key = [this](KeyEvent& ev) {
            if (ev.type == PlatformEventType::KeyDown && !m_state.is_disabled) {
                if (ev.key_code == 32 || ev.key_code == 13) select();
            }
        };
    }

    void update_animations() {
        auto now = std::chrono::steady_clock::now();
        float target_hover = m_state.is_hovered ? 1.0f : 0.0f;
        float target_press = m_state.is_pressed ? 1.0f : 0.0f;
        float target_check = m_data.checked ? 1.0f : 0.0f;

        if (m_last_anim_time.time_since_epoch().count() == 0) {
            m_last_anim_time = now;
            if (std::abs(m_hover_progress - target_hover) > 0.005f ||
                std::abs(m_press_progress - target_press) > 0.005f ||
                std::abs(m_check_progress - target_check) > 0.005f) {
                mark_needs_paint();
            }
            return;
        }
        float dt = std::clamp(std::chrono::duration<float>(now - m_last_anim_time).count(), 0.0f, 0.05f);
        m_last_anim_time = now;

        bool animating = false;
        auto step = [&](float& cur, float tgt, float speed) {
            if (std::abs(cur - tgt) > 0.005f) {
                cur += (tgt - cur) * std::min(1.0f, dt * speed);
                animating = true;
            } else {
                cur = tgt;
            }
        };
        step(m_hover_progress, target_hover, 20.0f);
        step(m_press_progress, target_press, 24.0f);
        step(m_check_progress, target_check, 22.0f);

        if (animating) mark_needs_paint();
    }

    RadioButtonData m_data;
    RadioButtonInteractiveState m_state;
    bool m_initialized{false};
    float m_hover_progress{0.0f};
    float m_press_progress{0.0f};
    float m_check_progress{0.0f};
    std::chrono::steady_clock::time_point m_last_anim_time{};
};

// 声明式经典 WinUI 3 单选按钮 (RadioButton)
struct RadioButton : BindableMixin<RadioButton, RenderRadioButton> {
    int selected_value{0};
    int value{0};
    std::string label{""};
    bool disabled{false};
    float font_size{13.0f};
    Fn<void(int)> on_change{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderRadioButton>& node, Property<int>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_checked(prop.get() == node->value(), false);
    std::weak_ptr<RenderRadioButton> weak = node;
    bg.subscribe(prop, [weak, guard](int cur) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_checked(cur == n->value(), true);
        }
    });
    auto prev = node->data().on_select;
    Property<int>* p = &prop;
    node->set_on_select([prev = std::move(prev), p, guard](int val) {
        *guard = true;
        p->set(val);
        if (prev) prev(val);
        *guard = false;
    });
}

class RadioButtonWidget : public Widget {
public:
    explicit RadioButtonWidget(const RadioButton& desc) : m_desc(desc) {}
    explicit RadioButtonWidget(RadioButton&& desc) noexcept : m_desc(std::move(desc)) {}
    ~RadioButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderRadioButton>();
        apply_desc_to_node(node, false);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto rb = std::static_pointer_cast<RenderRadioButton>(node)) {
            apply_desc_to_node(rb, true);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderRadioButton>& node, bool animate) {
        node->set_value(m_desc.value);
        node->set_checked(m_desc.selected_value == m_desc.value, animate);
        node->set_label(m_desc.label);
        node->set_disabled(m_desc.disabled);
        node->set_font_size(m_desc.font_size);
        node->set_on_select(m_desc.on_change);
    }

    RadioButton m_desc;
};

inline RadioButton::operator WidgetPtr() const& {
    return make_widget<RadioButtonWidget>(*this);
}
inline RadioButton::operator WidgetPtr() && {
    return make_widget<RadioButtonWidget>(std::move(*this));
}

// 声明式经典 WinUI 3 单选按钮组 (RadioGroup)
struct RadioGroup {
    int selected_value{0};
    std::vector<RadioOption> options{};
    bool horizontal{false};
    float spacing{6.0f};
    float font_size{13.0f};
    Fn<void(int)> on_change{nullptr};

    WidgetPtr bind_two_way(Property<int>& prop) const {
        std::vector<WidgetPtr> items;
        items.reserve(options.size());
        for (const auto& opt : options) {
            items.push_back(RadioButton {
                .selected_value = prop.get(),
                .value = opt.value,
                .label = opt.label,
                .disabled = opt.disabled,
                .font_size = font_size,
                .on_change = on_change
            }.bind_two_way(prop));
        }
        if (horizontal) {
            return HStack {
                .align_items = AlignItems::Center,
                .spacing = spacing > 0.0f ? spacing : 16.0f,
                .content = std::move(items)
            };
        }
        return VStack {
            .align_items = AlignItems::FlexStart,
            .spacing = spacing,
            .content = std::move(items)
        };
    }

    operator WidgetPtr() const {
        std::vector<WidgetPtr> items;
        items.reserve(options.size());
        for (const auto& opt : options) {
            items.push_back(RadioButton {
                .selected_value = selected_value,
                .value = opt.value,
                .label = opt.label,
                .disabled = opt.disabled,
                .font_size = font_size,
                .on_change = on_change
            });
        }
        if (horizontal) {
            return HStack {
                .align_items = AlignItems::Center,
                .spacing = spacing > 0.0f ? spacing : 16.0f,
                .content = std::move(items)
            };
        }
        return VStack {
            .align_items = AlignItems::FlexStart,
            .spacing = spacing,
            .content = std::move(items)
        };
    }
};

// =============================================================================
// 2. 单选卡片 (RenderRadioCard / RadioCard / RadioCardGroup) — 像素锐化版
// =============================================================================
class RenderRadioCard : public RenderBox {
public:
    RenderRadioCard() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderRadioCard() override = default;

    [[nodiscard]] bool checked() const noexcept { return m_data.checked; }
    [[nodiscard]] int value() const noexcept { return m_data.value; }
    [[nodiscard]] const std::string& label() const noexcept { return m_data.label; }
    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    [[nodiscard]] const RadioCardData& data() const noexcept { return m_data; }

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

    void set_icon_painter(Fn<void(PaintContext&, Rect, Color)> painter) {
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

    void set_on_select(Fn<void(int)> cb) { m_data.on_select = std::move(cb); }

    void select() {
        if (m_state.is_disabled) return;
        if (!m_data.checked) set_checked(true, true);
        if (m_data.on_select) m_data.on_select(m_data.value);
    }

    void perform_layout() override {
        const Theme& theme = ThemeContext::current();
        auto style = RadioStyleResolver::resolve(m_data, m_state, theme);
        m_size = m_constraints.constrain(Vec2{std::round(style.tile_width), std::round(style.tile_height)});
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        auto style = RadioStyleResolver::resolve(m_data, m_state, theme);

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
        if (active) mark_needs_paint();

        const Rect b = bounds();

        // 1. 卡片底色与锐利 1.5px 边框 (移除模糊外光圈)
        ctx.draw_rect(b, style.background, style.corner_radius, 0.0f);
        const Color border_col = lerp_color(style.border_idle, style.accent, m_border_val);
        ctx.draw_rect(b, border_col, style.corner_radius, style.border_width);

        // 2. 左上角圆形选中指示点
        if (m_dot_val > 0.01f) {
            const float full_d = style.dot_diameter;
            const float d = full_d * m_dot_val;
            const float cx = std::round(b.min_x + style.dot_margin + full_d * 0.5f);
            const float cy = std::round(b.min_y + style.dot_margin + full_d * 0.5f);
            const Rect dot_rect = Rect::from_xywh(cx - d * 0.5f, cy - d * 0.5f, d, d);

            Color dot_fill = lerp_color(style.dot_bg, style.accent, m_checked_val);
            dot_fill.a *= m_dot_val;
            ctx.draw_rect(dot_rect, dot_fill, d * 0.5f, 0.0f);
            if (m_checked_val < 0.99f) {
                Color dot_border = lerp_color(style.border_idle, style.accent, m_checked_val);
                dot_border.a *= m_dot_val;
                ctx.draw_rect(dot_rect, dot_border, d * 0.5f, 1.2f);
            }
        }

        // 3. 居中图标与整数像素对齐锐利文字
        const bool has_icon = static_cast<bool>(m_data.icon_painter) || !m_data.icon_text.empty();
        const bool has_label = !m_data.label.empty();
        const Color icon_col = lerp_color(style.icon_idle, style.accent, m_checked_val);
        const Color label_col = lerp_color(style.label_idle, style.accent, m_checked_val);

        constexpr float kIconSize = 22.0f;
        constexpr float kIconLabelGap = 4.0f;
        Vec2 label_sz = has_label ? TextMeasure::measure(m_data.label, style.font_size) : Vec2{0.0f, 0.0f};
        float content_h = (has_icon ? kIconSize : 0.0f) +
                          ((has_icon && has_label) ? kIconLabelGap : 0.0f) +
                          (has_label ? label_sz.y : 0.0f);
        float cur_y = std::round(b.min_y + (b.height() - content_h) * 0.5f);

        if (has_icon) {
            Rect icon_rect = Rect::from_xywh(std::round(b.min_x + (b.width() - kIconSize) * 0.5f), cur_y, kIconSize, kIconSize);
            if (m_data.icon_painter) {
                m_data.icon_painter(ctx, icon_rect, icon_col);
            } else {
                Vec2 glyph_sz = TextMeasure::measure(m_data.icon_text, 18.0f);
                ctx.draw_text(m_data.icon_text,
                              PointF{std::round(icon_rect.min_x + (icon_rect.width() - glyph_sz.x) * 0.5f),
                                     std::round(icon_rect.min_y + (icon_rect.height() - glyph_sz.y) * 0.5f)},
                              18.0f, icon_col);
            }
            cur_y = std::round(cur_y + kIconSize + kIconLabelGap);
        }

        if (has_label) {
            float label_x = std::round(b.min_x + (b.width() - label_sz.x) * 0.5f);
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
        const float b_target = (!m_state.is_disabled && (m_data.checked || m_state.is_hovered)) ? 1.0f : 0.0f;
        const float c_target = m_data.checked ? 1.0f : 0.0f;
        m_border_from = m_border_to = m_border_val = b_target;
        m_border_elapsed = kBorderDurSec;
        m_dot_from = m_dot_to = m_dot_val = b_target;
        m_dot_elapsed = kDotDurSec;
        m_checked_from = m_checked_to = m_checked_val = c_target;
        m_checked_elapsed = kLabelDurSec;
    }

    void update_anim_targets() {
        const float b_target = (!m_state.is_disabled && (m_data.checked || m_state.is_hovered)) ? 1.0f : 0.0f;
        const float c_target = m_data.checked ? 1.0f : 0.0f;

        if (m_border_to != b_target) { m_border_from = m_border_val; m_border_to = b_target; m_border_elapsed = 0.0f; }
        if (m_dot_to != b_target)    { m_dot_from = m_dot_val;       m_dot_to = b_target;    m_dot_elapsed = 0.0f; }
        if (m_checked_to != c_target){ m_checked_from = m_checked_val; m_checked_to = c_target; m_checked_elapsed = 0.0f; }
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
        on_key = [this](KeyEvent& ev) {
            if (ev.type == PlatformEventType::KeyDown && !m_state.is_disabled) {
                if (ev.key_code == 32 || ev.key_code == 13) select();
            }
        };
    }

    static constexpr float kBorderDurSec = 0.15f;
    static constexpr float kDotDurSec = 0.22f;
    static constexpr float kLabelDurSec = 0.28f;
    static constexpr CubicBezier kEaseCurve{0.25f, 0.10f, 0.25f, 1.00f};

    RadioCardData m_data;
    RadioButtonInteractiveState m_state;

    bool m_initialized{false};
    float m_border_from{0.0f}, m_border_to{0.0f}, m_border_val{0.0f}, m_border_elapsed{kBorderDurSec};
    float m_dot_from{0.0f}, m_dot_to{0.0f}, m_dot_val{0.0f}, m_dot_elapsed{kDotDurSec};
    float m_checked_from{0.0f}, m_checked_to{0.0f}, m_checked_val{0.0f}, m_checked_elapsed{kLabelDurSec};
    std::chrono::steady_clock::time_point m_last_tick_time{};
};

// 声明式单个单选卡片 (RadioCard)
struct RadioCard : BindableMixin<RadioCard, RenderRadioCard> {
    int selected_value{0};
    int value{0};
    std::string label{""};
    std::string icon_text{""};
    Fn<void(PaintContext&, Rect, Color)> icon_painter{nullptr};
    bool disabled{false};
    float tile_width{84.0f};
    float tile_height{68.0f};
    float corner_radius{8.0f};
    float font_size{13.0f};
    Fn<void(int)> on_change{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderRadioCard>& node, Property<int>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_checked(prop.get() == node->value(), false);
    std::weak_ptr<RenderRadioCard> weak = node;
    bg.subscribe(prop, [weak, guard](int cur) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_checked(cur == n->value(), true);
        }
    });
    auto prev = node->data().on_select;
    Property<int>* p = &prop;
    node->set_on_select([prev = std::move(prev), p, guard](int val) {
        *guard = true;
        p->set(val);
        if (prev) prev(val);
        *guard = false;
    });
}

class RadioCardWidget : public Widget {
public:
    explicit RadioCardWidget(const RadioCard& desc) : m_desc(desc) {}
    explicit RadioCardWidget(RadioCard&& desc) noexcept : m_desc(std::move(desc)) {}
    ~RadioCardWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderRadioCard>();
        apply_desc_to_node(node, false);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto rc = std::static_pointer_cast<RenderRadioCard>(node)) {
            apply_desc_to_node(rc, true);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderRadioCard>& node, bool animate) {
        node->set_value(m_desc.value);
        node->set_checked(m_desc.selected_value == m_desc.value, animate);
        node->set_label(m_desc.label);
        node->set_icon_text(m_desc.icon_text);
        node->set_icon_painter(m_desc.icon_painter);
        node->set_disabled(m_desc.disabled);
        node->set_tile_size(m_desc.tile_width, m_desc.tile_height);
        node->set_corner_radius(m_desc.corner_radius);
        node->set_font_size(m_desc.font_size);
        node->set_on_select(m_desc.on_change);
    }

    RadioCard m_desc;
};

inline RadioCard::operator WidgetPtr() const& {
    return make_widget<RadioCardWidget>(*this);
}
inline RadioCard::operator WidgetPtr() && {
    return make_widget<RadioCardWidget>(std::move(*this));
}

// 声明式单选卡片组 (RadioCardGroup)
struct RadioCardGroup {
    int selected_value{0};
    std::vector<RadioCardOption> options{};
    float spacing{12.0f};
    float tile_width{84.0f};
    float tile_height{68.0f};
    float corner_radius{8.0f};
    float font_size{13.0f};
    Fn<void(int)> on_change{nullptr};

    WidgetPtr bind_two_way(Property<int>& prop) const {
        std::vector<WidgetPtr> items;
        items.reserve(options.size());
        for (const auto& opt : options) {
            items.push_back(RadioCard {
                .selected_value = prop.get(),
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
            }.bind_two_way(prop));
        }
        return HStack {
            .align_items = AlignItems::Center,
            .spacing = spacing,
            .content = std::move(items)
        };
    }

    operator WidgetPtr() const {
        std::vector<WidgetPtr> items;
        items.reserve(options.size());
        for (const auto& opt : options) {
            items.push_back(RadioCard {
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
