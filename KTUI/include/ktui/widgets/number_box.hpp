#pragma once

#include "common/fluent_icons.hpp"
#include "textbox/textbox_logic.hpp"
#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace ktui {

// WinUI 3 NumberBoxSpinButtonPlacementMode
enum class NumberBoxSpinButtonPlacementMode : uint8_t {
    Inline = 0,   // 默认：右侧内嵌上下步进微调按钮
    Compact = 1,  // 悬停或聚焦时显示步进按钮
    Hidden = 2    // 隐藏步进按钮，仅纯数字输入
};

// 轻量级递归下降算术表达式求值器 (支持 +, -, *, /, 括号与一元负号，对标 WinUI 3 NumberBox AcceptsExpression)
namespace detail {
class NumberBoxExprParser {
public:
    static std::optional<double> evaluate(const std::string& expr) {
        NumberBoxExprParser p(expr);
        auto val = p.parse_expr();
        p.skip_ws();
        if (!val.has_value() || p.m_pos != p.m_src.size() || !std::isfinite(*val)) {
            return std::nullopt;
        }
        return val;
    }

private:
    explicit NumberBoxExprParser(const std::string& s) : m_src(s) {}

    void skip_ws() noexcept {
        while (m_pos < m_src.size() && (m_src[m_pos] == ' ' || m_src[m_pos] == '\t')) ++m_pos;
    }

    std::optional<double> parse_expr() {
        auto lhs = parse_term();
        if (!lhs) return std::nullopt;
        while (true) {
            skip_ws();
            if (m_pos >= m_src.size()) break;
            char op = m_src[m_pos];
            if (op != '+' && op != '-') break;
            ++m_pos;
            auto rhs = parse_term();
            if (!rhs) return std::nullopt;
            lhs = (op == '+') ? (*lhs + *rhs) : (*lhs - *rhs);
        }
        return lhs;
    }

    std::optional<double> parse_term() {
        auto lhs = parse_unary();
        if (!lhs) return std::nullopt;
        while (true) {
            skip_ws();
            if (m_pos >= m_src.size()) break;
            char op = m_src[m_pos];
            if (op != '*' && op != '/') break;
            ++m_pos;
            auto rhs = parse_unary();
            if (!rhs) return std::nullopt;
            if (op == '/') {
                if (std::abs(*rhs) < 1e-12) return std::nullopt;
                lhs = *lhs / *rhs;
            } else {
                lhs = *lhs * *rhs;
            }
        }
        return lhs;
    }

    std::optional<double> parse_unary() {
        skip_ws();
        if (m_pos < m_src.size() && (m_src[m_pos] == '+' || m_src[m_pos] == '-')) {
            char op = m_src[m_pos++];
            auto v = parse_unary();
            if (!v) return std::nullopt;
            return (op == '-') ? -(*v) : *v;
        }
        return parse_primary();
    }

    std::optional<double> parse_primary() {
        skip_ws();
        if (m_pos >= m_src.size()) return std::nullopt;
        if (m_src[m_pos] == '(') {
            ++m_pos;
            auto v = parse_expr();
            skip_ws();
            if (!v || m_pos >= m_src.size() || m_src[m_pos] != ')') return std::nullopt;
            ++m_pos;
            return v;
        }
        const char* start = m_src.c_str() + m_pos;
        char* end = nullptr;
        double val = std::strtod(start, &end);
        if (end == start) return std::nullopt;
        m_pos = static_cast<size_t>(end - m_src.c_str());
        return val;
    }

    const std::string& m_src;
    size_t m_pos{0};
};
} // namespace detail

// WinUI 3 NumberBox 渲染盒
// 直接组合复用真实 RenderTextBox 子节点，完整继承选区、撤销/重做、WinUI 3 文本选中样式与右键菜单，
// 并叠加右侧上下步进微调按钮、滚轮/键盘步进与算术表达式求值
class RenderNumberBox : public RenderBox {
public:
    RenderNumberBox() noexcept {
        m_input = make_render_node<RenderTextBox>();
        append_child(m_input.get());
        setup_input_bridge();
        sync_input_properties();
    }

    ~RenderNumberBox() override {
        if (m_input && m_input->parent() == this) {
            remove_child(m_input.get());
        }
    }

    void set_properties(double value,
                        double minimum,
                        double maximum,
                        double small_change,
                        double large_change,
                        int precision,
                        bool accepts_expression,
                        NumberBoxSpinButtonPlacementMode spin_mode,
                        const std::string& placeholder,
                        const std::string& header,
                        float width,
                        bool disabled,
                        Fn<void(double)> on_value_changed) {
        m_minimum = minimum;
        m_maximum = std::max(minimum, maximum);
        m_small_change = std::max(1e-6, small_change);
        m_large_change = std::max(m_small_change, large_change);
        m_precision = std::clamp(precision, 0, 6);
        m_accepts_expression = accepts_expression;
        if (m_spin_mode != spin_mode) m_spin_mode = spin_mode;
        if (m_placeholder != placeholder) m_placeholder = placeholder;
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) m_disabled = disabled;
        m_on_value_changed = std::move(on_value_changed);

        double clamped = clamp_and_round(value);
        if (!m_initialized || std::abs(m_value - clamped) > 1e-9) {
            m_initialized = true;
            m_value = clamped;
            if (!m_editing) {
                m_text = format_value(m_value);
            }
        }
        sync_input_properties();
        mark_needs_paint();
    }

    void set_numeric_value(double v) {
        double clamped = clamp_and_round(v);
        if (std::abs(m_value - clamped) > 1e-9) {
            m_value = clamped;
            m_editing = false;
            m_text = format_value(m_value);
            if (m_input) {
                m_input->set_text(m_text);
                m_input->set_cursor_index(m_text.size());
            }
            mark_needs_paint();
        }
    }

    [[nodiscard]] double value() const noexcept { return m_value; }
    [[nodiscard]] const std::string& display_text() const noexcept { return m_text; }
    [[nodiscard]] const std::shared_ptr<RenderTextBox>& input_box() const noexcept { return m_input; }
    void on_ime_commit(const std::string& utf8_text) {
        if (!m_disabled && m_input) m_input->on_ime_commit(utf8_text);
    }

    void step(double delta) {
        if (m_disabled) return;
        commit_text_if_editing();
        apply_new_value(m_value + delta, true);
    }

    void perform_layout() override {
        float w = (m_width > 0.0f) ? m_width : 180.0f;
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        set_size(m_constraints.constrain(Vec2{w, header_h + 32.0f}));
        if (m_input) {
            float box_h = std::max(28.0f, size().y - header_h);
            m_input->layout(BoxConstraints::tight(Vec2{size().x, box_h}), false);
            m_input->set_offset(Vec2{0.0f, header_h});
        }
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();

        if (!m_header.empty()) {
            Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f}
                                           : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, text_col);
        }

        if (m_input) {
            PaintContext::OffsetScope scope(ctx, m_input->offset());
            m_input->paint(ctx);
        }

        // 右侧上下步进按钮 (Fluent Chevron Up / Down SVG)
        if (spin_buttons_visible()) {
            Rect box = box_rect();
            Rect up_r = up_button_rect();
            Rect dn_r = down_button_rect();
            Color div = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
            ctx.draw_rect(Rect::from_xywh(up_r.min_x - 1.0f, box.min_y + 4.0f, 1.0f, box.height() - 8.0f), div, 0.0f, 0.0f);

            auto paint_spin = [&](Rect r, SpinPart part, bool at_limit, const char* svg) {
                bool hov = (m_hover_spin == part && !at_limit);
                bool prs = (m_press_spin == part && !at_limit);
                if (prs) {
                    ctx.draw_rect(r, theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.09f} : Color{0.0f, 0.0f, 0.0f, 0.06f}, 3.0f, 0.0f);
                } else if (hov) {
                    ctx.draw_rect(r, theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.06f} : Color{0.0f, 0.0f, 0.0f, 0.04f}, 3.0f, 0.0f);
                }
                Color ic = theme.is_dark
                    ? Color{1.0f, 1.0f, 1.0f, at_limit ? 0.25f : 0.80f}
                    : Color{0.0f, 0.0f, 0.0f, at_limit ? 0.25f : 0.72f};
                fluent_icons::draw_centered(ctx, svg, r, 11.0f, ic);
            };

            paint_spin(up_r, SpinPart::Up, m_value >= m_maximum - 1e-9, fluent_icons::kChevronUp);
            paint_spin(dn_r, SpinPart::Down, m_value <= m_minimum + 1e-9, fluent_icons::kChevronDown);
        }
    }

private:
    enum class SpinPart : uint8_t { None, Up, Down };

    [[nodiscard]] Rect box_rect() const noexcept {
        float y = m_header.empty() ? 0.0f : 22.0f;
        return Rect::from_xywh(0.0f, y, size().x, std::max(28.0f, size().y - y));
    }

    [[nodiscard]] bool spin_buttons_visible() const noexcept {
        if (m_disabled || m_spin_mode == NumberBoxSpinButtonPlacementMode::Hidden) return false;
        if (m_spin_mode == NumberBoxSpinButtonPlacementMode::Compact) {
            return m_input && (m_input->state().is_hovered || m_input->state().is_focused);
        }
        return true;
    }

    [[nodiscard]] Rect up_button_rect() const noexcept {
        Rect b = box_rect();
        float half_h = std::floor((b.height() - 4.0f) * 0.5f);
        return Rect::from_xywh(b.max_x - 28.0f, b.min_y + 2.0f, 25.0f, half_h);
    }

    [[nodiscard]] Rect down_button_rect() const noexcept {
        Rect b = box_rect();
        float half_h = std::floor((b.height() - 4.0f) * 0.5f);
        return Rect::from_xywh(b.max_x - 28.0f, b.min_y + 2.0f + half_h, 25.0f, half_h);
    }

    [[nodiscard]] Rect input_local_up_rect() const noexcept {
        Vec2 s = m_input ? m_input->size() : Vec2{size().x, 32.0f};
        float half_h = std::floor((s.y - 4.0f) * 0.5f);
        return Rect::from_xywh(s.x - 28.0f, 2.0f, 25.0f, half_h);
    }

    [[nodiscard]] Rect input_local_down_rect() const noexcept {
        Vec2 s = m_input ? m_input->size() : Vec2{size().x, 32.0f};
        float half_h = std::floor((s.y - 4.0f) * 0.5f);
        return Rect::from_xywh(s.x - 28.0f, 2.0f + half_h, 25.0f, half_h);
    }

    [[nodiscard]] double clamp_and_round(double v) const noexcept {
        if (!std::isfinite(v)) v = m_minimum;
        v = std::clamp(v, m_minimum, m_maximum);
        double factor = std::pow(10.0, m_precision);
        double rounded = std::round(v * factor) / factor;
        return (std::abs(rounded) < 1e-12) ? 0.0 : rounded;
    }

    [[nodiscard]] std::string format_value(double v) const {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", m_precision, v);
        return std::string(buf);
    }

    [[nodiscard]] std::string filter_numeric_text(const std::string& raw) const {
        std::string allowed;
        allowed.reserve(raw.size());
        for (char c : raw) {
            bool ok = (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+';
            if (m_accepts_expression && (c == '*' || c == '/' || c == '(' || c == ')' || c == ' ')) ok = true;
            if (ok) allowed.push_back(c);
        }
        return allowed;
    }

    void update_input_padding() {
        if (!m_input) return;
        float right_pad = spin_buttons_visible() ? 34.0f : 10.0f;
        m_input->apply_custom_overrides(
            std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            32.0f, 4.0f, EdgeInsets{6.0f, right_pad, 6.0f, 10.0f}, 13.5f);
    }

    void sync_input_properties() {
        if (!m_input) return;
        m_input->set_properties(
            m_text, m_placeholder,
            32.0f, 4.0f,
            EdgeInsets{6.0f, spin_buttons_visible() ? 34.0f : 10.0f, 6.0f, 10.0f},
            Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1},
            Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1}, 13.5f,
            [this](const std::string& updated) {
                std::string filtered = filter_numeric_text(updated);
                if (filtered != updated) {
                    m_input->set_text(filtered);
                }
                m_text = std::move(filtered);
                m_editing = true;
                mark_needs_paint();
            },
            [this](const std::string&) {
                commit_text_if_editing();
            });
        m_input->set_disabled(m_disabled);
    }

    void apply_new_value(double raw, bool fire_cb) {
        double next = clamp_and_round(raw);
        bool changed = (std::abs(next - m_value) > 1e-9);
        m_value = next;
        m_editing = false;
        m_text = format_value(m_value);
        if (m_input) {
            m_input->set_text(m_text);
            m_input->set_cursor_index(m_text.size());
        }
        mark_needs_paint();
        if (changed && fire_cb && m_on_value_changed) {
            m_on_value_changed(m_value);
        }
    }

    void commit_text_if_editing() {
        if (!m_editing) return;
        std::optional<double> parsed;
        if (m_accepts_expression) {
            parsed = detail::NumberBoxExprParser::evaluate(m_text);
        } else {
            char* end = nullptr;
            double v = std::strtod(m_text.c_str(), &end);
            if (end != m_text.c_str() && *end == '\0' && std::isfinite(v)) parsed = v;
        }
        if (parsed.has_value()) {
            apply_new_value(*parsed, true);
        } else {
            // 非法输入回滚到当前有效值
            m_editing = false;
            m_text = format_value(m_value);
            if (m_input) {
                m_input->set_text(m_text);
                m_input->set_cursor_index(m_text.size());
            }
            mark_needs_paint();
        }
    }

    void setup_input_bridge() {
        auto orig_enter = m_input->on_pointer_enter;
        auto orig_exit = m_input->on_pointer_exit;
        auto orig_move = m_input->on_pointer_move;
        auto orig_down = m_input->on_pointer_down;
        auto orig_up = m_input->on_pointer_up;
        auto orig_focus = m_input->on_focus;
        auto orig_key = m_input->on_key;

        m_input->on_pointer_enter = [this, orig_enter](PointerEvent& e) {
            if (orig_enter) orig_enter(e);
            update_input_padding();
            mark_needs_paint();
        };

        m_input->on_pointer_exit = [this, orig_exit](PointerEvent& e) {
            m_hover_spin = SpinPart::None;
            m_press_spin = SpinPart::None;
            if (orig_exit) orig_exit(e);
            update_input_padding();
            mark_needs_paint();
        };

        m_input->on_pointer_move = [this, orig_move](PointerEvent& e) {
            if (m_disabled) return;
            if (e.type == PlatformEventType::PointerScroll && m_input->state().is_focused) {
                step(e.delta.y > 0.0f ? -m_small_change : m_small_change);
                e.stop_propagation();
                return;
            }
            SpinPart hp = SpinPart::None;
            if (spin_buttons_visible()) {
                if (input_local_up_rect().contains(e.local_position)) hp = SpinPart::Up;
                else if (input_local_down_rect().contains(e.local_position)) hp = SpinPart::Down;
            }
            if (hp != m_hover_spin) {
                m_hover_spin = hp;
                m_input->cursor_shape = (hp != SpinPart::None) ? CursorShape::PointingHand : CursorShape::IBeam;
                mark_needs_paint();
            }
            if (orig_move) orig_move(e);
        };

        m_input->on_pointer_down = [this, orig_down](PointerEvent& e) {
            if (m_disabled) return;
            if (e.button == PointerButton::Primary && spin_buttons_visible()) {
                if (input_local_up_rect().contains(e.local_position)) {
                    m_press_spin = SpinPart::Up;
                    step(m_small_change);
                    e.stop_propagation();
                    return;
                }
                if (input_local_down_rect().contains(e.local_position)) {
                    m_press_spin = SpinPart::Down;
                    step(-m_small_change);
                    e.stop_propagation();
                    return;
                }
            }
            if (orig_down) orig_down(e);
        };

        m_input->on_pointer_up = [this, orig_up](PointerEvent& e) {
            if (m_press_spin != SpinPart::None) {
                m_press_spin = SpinPart::None;
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (orig_up) orig_up(e);
        };

        m_input->on_focus = [this, orig_focus](FocusEvent& e) {
            if (orig_focus) orig_focus(e);
            if (!m_disabled) {
                if (e.gained_focus) {
                    m_input->select_all();
                } else {
                    commit_text_if_editing();
                }
                update_input_padding();
                mark_needs_paint();
            }
        };

        m_input->on_key = [this, orig_key](KeyEvent& e) {
            if (m_disabled) return;
            if (e.type == PlatformEventType::KeyDown) {
                switch (e.key_code) {
                    case 38: // Up
                        step(m_small_change);
                        e.stop_propagation();
                        return;
                    case 40: // Down
                        step(-m_small_change);
                        e.stop_propagation();
                        return;
                    case 33: // PageUp
                        step(m_large_change);
                        e.stop_propagation();
                        return;
                    case 34: // PageDown
                        step(-m_large_change);
                        e.stop_propagation();
                        return;
                    default:
                        break;
                }
            }
            if (orig_key) orig_key(e);
        };
    }

    std::shared_ptr<RenderTextBox> m_input;
    bool m_initialized{false};
    double m_value{0.0};
    double m_minimum{-1e9};
    double m_maximum{1e9};
    double m_small_change{1.0};
    double m_large_change{10.0};
    int m_precision{0};
    bool m_accepts_expression{true};
    NumberBoxSpinButtonPlacementMode m_spin_mode{NumberBoxSpinButtonPlacementMode::Inline};
    std::string m_placeholder;
    std::string m_header;
    float m_width{180.0f};
    bool m_disabled{false};
    Fn<void(double)> m_on_value_changed;

    std::string m_text{"0"};
    bool m_editing{false};
    SpinPart m_hover_spin{SpinPart::None};
    SpinPart m_press_spin{SpinPart::None};

public:
    void wrap_on_value_changed(Fn<void(double)> extra) {
        auto prev = std::move(m_on_value_changed);
        m_on_value_changed = [prev = std::move(prev), extra = std::move(extra)](double v) {
            if (extra) extra(v);
            if (prev) prev(v);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 NumberBox
struct NumberBox : BindableMixin<NumberBox, RenderNumberBox> {
    double value{0.0};
    double minimum{-1e9};
    double maximum{1e9};
    double small_change{1.0};
    double large_change{10.0};
    int precision{0};
    bool accepts_expression{true};
    NumberBoxSpinButtonPlacementMode spin_button_placement{NumberBoxSpinButtonPlacementMode::Inline};
    std::string placeholder{"输入数字或表达式..."};
    std::string header{""};
    float width{180.0f};
    bool disabled{false};
    Fn<void(double)> on_value_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderNumberBox>& node, Property<double>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_numeric_value(prop.get());
    std::weak_ptr<RenderNumberBox> weak = node;
    bg.subscribe(prop, [weak, guard](double v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_numeric_value(v);
        }
    });
    Property<double>* p = &prop;
    node->wrap_on_value_changed([p, guard](double v) {
        *guard = true;
        p->set(v);
        *guard = false;
    });
}

class NumberBoxWidget : public Widget {
public:
    explicit NumberBoxWidget(const NumberBox& desc) : m_desc(desc) {}
    explicit NumberBoxWidget(NumberBox&& desc) noexcept : m_desc(std::move(desc)) {}
    ~NumberBoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderNumberBox>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto nb = std::static_pointer_cast<RenderNumberBox>(node)) {
            apply(nb);
        }
    }

private:
    void apply(const std::shared_ptr<RenderNumberBox>& nb) const {
        nb->set_properties(
            m_desc.value, m_desc.minimum, m_desc.maximum,
            m_desc.small_change, m_desc.large_change, m_desc.precision,
            m_desc.accepts_expression, m_desc.spin_button_placement,
            m_desc.placeholder, m_desc.header, m_desc.width, m_desc.disabled,
            m_desc.on_value_changed);
    }

    NumberBox m_desc;
};

inline NumberBox::operator WidgetPtr() const& {
    return make_widget<NumberBoxWidget>(*this);
}
inline NumberBox::operator WidgetPtr() && {
    return make_widget<NumberBoxWidget>(std::move(*this));
}

} // namespace ktui
