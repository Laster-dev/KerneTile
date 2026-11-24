#pragma once

#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <chrono>
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

// WinUI 3 NumberBox 渲染盒 (支持上下步进、键盘/滚轮微调、算术表达式求值、范围钳位与精度格式化)
class RenderNumberBox : public RenderBox, public ITextInputClient {
public:
    RenderNumberBox() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::IBeam;
        setup_behavior();
    }

    ~RenderNumberBox() override = default;

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
                        std::function<void(double)> on_value_changed) {
        m_minimum = minimum;
        m_maximum = std::max(minimum, maximum);
        m_small_change = std::max(1e-6, small_change);
        m_large_change = std::max(m_small_change, large_change);
        m_precision = std::clamp(precision, 0, 6);
        m_accepts_expression = accepts_expression;
        if (m_spin_mode != spin_mode) { m_spin_mode = spin_mode; mark_needs_paint(); }
        if (m_placeholder != placeholder) { m_placeholder = placeholder; mark_needs_paint(); }
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            is_focusable = !disabled;
            cursor_shape = disabled ? CursorShape::Arrow : CursorShape::IBeam;
            if (disabled) m_focused = false;
            mark_needs_paint();
        }
        m_on_value_changed = std::move(on_value_changed);

        double clamped = clamp_and_round(value);
        if (!m_initialized || std::abs(m_value - clamped) > 1e-9) {
            m_initialized = true;
            m_value = clamped;
            if (!m_editing) {
                m_text = format_value(m_value);
                m_caret = m_text.size();
            }
            mark_needs_paint();
        }
    }

    void set_numeric_value(double v) {
        double clamped = clamp_and_round(v);
        if (std::abs(m_value - clamped) > 1e-9) {
            m_value = clamped;
            m_editing = false;
            m_text = format_value(m_value);
            m_caret = m_text.size();
            mark_needs_paint();
        }
    }

    [[nodiscard]] double value() const noexcept { return m_value; }
    [[nodiscard]] const std::string& display_text() const noexcept { return m_text; }

    void step(double delta) {
        if (m_disabled) return;
        commit_text_if_editing();
        apply_new_value(m_value + delta, true);
    }

    // ==================== ITextInputClient ====================
    [[nodiscard]] bool accepts_text_input() const noexcept override { return !m_disabled; }

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        Vec2 g = global_offset();
        Rect box = box_rect();
        float cx = box.min_x + 11.0f + TextMeasure::measure(m_text.substr(0, std::min(m_caret, m_text.size())), 13.5f).x;
        return Rect::from_xywh(g.x + cx, g.y + box.min_y + 6.0f, 2.0f, 20.0f);
    }

    void on_ime_preedit(const std::string&, size_t) override {}

    void on_ime_commit(const std::string& utf8_text) override {
        if (m_disabled || utf8_text.empty()) return;
        insert_text(utf8_text);
    }

    bool tick_cursor_blink() override {
        if (!m_focused || m_disabled) return false;
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_blink_time).count();
        if (ms >= 530) {
            m_caret_visible = !m_caret_visible;
            m_blink_time = now;
            mark_needs_paint();
            return true;
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept override {
        if (!m_focused || m_disabled) return 1000;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_blink_time).count();
        return (elapsed >= 530) ? 16 : static_cast<uint32_t>(530 - elapsed);
    }

    void perform_layout() override {
        float w = (m_width > 0.0f) ? m_width : 180.0f;
        float h = (m_header.empty() ? 0.0f : 22.0f) + 32.0f;
        set_size(m_constraints.constrain(Vec2{w, h}));
    }

    void paint(PaintContext& ctx) override {
        update_focus_anim();
        const Theme& theme = ThemeContext::current();

        // 1. 可选 Header
        if (!m_header.empty()) {
            Color hc = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.92f}
                : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.88f};
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, hc);
        }

        Rect box = box_rect();
        Color bg, border, bottom_line, text_col, ph_col, accent;
        if (theme.is_dark) {
            bg = m_disabled ? Color{1.0f, 1.0f, 1.0f, 0.03f}
               : (m_focused ? Color{30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 0.92f}
                            : (m_hovered ? Color{1.0f, 1.0f, 1.0f, 0.075f} : Color{1.0f, 1.0f, 1.0f, 0.055f}));
            border = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.05f : 0.085f};
            bottom_line = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.06f : 0.48f};
            text_col = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f};
            ph_col = Color{1.0f, 1.0f, 1.0f, 0.50f};
            accent = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};
        } else {
            bg = m_disabled ? Color{0.96f, 0.96f, 0.96f, 0.55f}
               : (m_focused ? Color{1.0f, 1.0f, 1.0f, 1.0f}
                            : (m_hovered ? Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.65f}
                                         : Color{1.0f, 1.0f, 1.0f, 0.78f}));
            border = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.05f : 0.08f};
            bottom_line = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.06f : 0.42f};
            text_col = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
            ph_col = Color{0.0f, 0.0f, 0.0f, 0.50f};
            accent = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
        }

        // 2. 底板、边框与 1px 抬升底线
        ctx.draw_rect(box, bg, 4.0f, 0.0f);
        ctx.draw_rect(box, border, 4.0f, 1.0f);
        ctx.draw_rect(Rect::from_xywh(box.min_x + 1.0f, box.max_y - 1.0f, std::max(0.0f, box.width() - 2.0f), 1.0f),
                      bottom_line, 0.0f, 0.0f);

        // 3. 2px 中心展开聚焦底线
        if (m_focus_anim > 0.01f) {
            float full_w = std::max(0.0f, box.width() - 2.0f);
            float line_w = full_w * m_focus_anim;
            float lx = box.min_x + 1.0f + (full_w - line_w) * 0.5f;
            ctx.draw_rect(Rect::from_xywh(lx, box.max_y - 2.0f, line_w, 2.0f), accent, 1.0f, 0.0f);
        }

        // 4. 数值文本绘制
        const bool show_spins = spin_buttons_visible();
        const float right_reserve = show_spins ? 34.0f : 10.0f;
        Rect text_clip = Rect::from_xywh(box.min_x + 10.0f, box.min_y + 2.0f,
                                         std::max(0.0f, box.width() - 10.0f - right_reserve),
                                         box.height() - 4.0f);
        ctx.push_clip(text_clip, 0.0f);
        if (m_text.empty()) {
            if (!m_placeholder.empty()) {
                Vec2 ps = TextMeasure::measure(m_placeholder, 13.5f);
                ctx.draw_text(m_placeholder,
                              PointF{std::round(text_clip.min_x), std::round(box.min_y + (box.height() - ps.y) * 0.5f)},
                              13.5f, ph_col);
            }
            if (m_focused && m_caret_visible && !m_disabled) {
                ctx.draw_rect(Rect::from_xywh(std::round(text_clip.min_x), box.min_y + 7.0f, 1.5f, box.height() - 14.0f),
                              text_col, 0.5f, 0.0f);
            }
        } else {
            Vec2 ts = TextMeasure::measure(m_text, 13.5f);
            float ty = std::round(box.min_y + (box.height() - ts.y) * 0.5f);
            float caret_px = TextMeasure::measure(m_text.substr(0, std::min(m_caret, m_text.size())), 13.5f).x;
            float scroll_x = (caret_px > text_clip.width() - 4.0f) ? (caret_px - (text_clip.width() - 4.0f)) : 0.0f;
            if (m_select_all && m_focused) {
                Color sel = accent;
                sel.a = theme.is_dark ? 0.35f : 0.25f;
                ctx.draw_rect(Rect::from_xywh(text_clip.min_x - scroll_x, box.min_y + 6.0f,
                                             ts.x + 2.0f, box.height() - 12.0f), sel, 2.0f, 0.0f);
            }
            ctx.draw_text(m_text, PointF{std::round(text_clip.min_x - scroll_x), ty}, 13.5f, text_col);
            if (m_focused && m_caret_visible && !m_disabled) {
                ctx.draw_rect(Rect::from_xywh(std::round(text_clip.min_x + caret_px - scroll_x),
                                             box.min_y + 7.0f, 1.5f, box.height() - 14.0f),
                              text_col, 0.5f, 0.0f);
            }
        }
        ctx.pop_clip();

        // 5. 右侧上下步进按钮 (Fluent Chevron Up / Down SVG)
        if (show_spins) {
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
                float isz = 11.0f;
                Rect ir = Rect::from_xywh(std::round(r.min_x + (r.width() - isz) * 0.5f),
                                          std::round(r.min_y + (r.height() - isz) * 0.5f), isz, isz);
                SvgEngine::draw_svg_xml(ctx, svg, ir, ic);
            };

            static constexpr const char* kChevronUpSvg =
                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
                "<path fill='#FFFFFF' d='M4.29 15.71a1 1 0 0 1 0-1.42l7-7a1 1 0 0 1 1.42 0l7 7a1 1 0 1 1-1.42 1.42L12 9.41l-6.29 6.3a1 1 0 0 1-1.42 0z'/>"
                "</svg>";
            static constexpr const char* kChevronDownSvg =
                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
                "<path fill='#FFFFFF' d='M4.29 8.29a1 1 0 0 1 1.42 0L12 14.59l6.29-6.3a1 1 0 1 1 1.42 1.42l-7 7a1 1 0 0 1-1.42 0l-7-7a1 1 0 0 1 0-1.42z'/>"
                "</svg>";

            paint_spin(up_r, SpinPart::Up, m_value >= m_maximum - 1e-9, kChevronUpSvg);
            paint_spin(dn_r, SpinPart::Down, m_value <= m_minimum + 1e-9, kChevronDownSvg);
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
        if (m_spin_mode == NumberBoxSpinButtonPlacementMode::Compact) return m_hovered || m_focused;
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

    void apply_new_value(double raw, bool fire_cb) {
        double next = clamp_and_round(raw);
        bool changed = (std::abs(next - m_value) > 1e-9);
        m_value = next;
        m_editing = false;
        m_text = format_value(m_value);
        m_caret = m_text.size();
        m_select_all = false;
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
            m_caret = m_text.size();
            mark_needs_paint();
        }
    }

    void insert_text(const std::string& raw) {
        std::string allowed;
        for (char c : raw) {
            bool ok = (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+';
            if (m_accepts_expression && (c == '*' || c == '/' || c == '(' || c == ')' || c == ' ')) ok = true;
            if (ok) allowed.push_back(c);
        }
        if (allowed.empty()) return;
        if (m_select_all) {
            m_text.clear();
            m_caret = 0;
            m_select_all = false;
        }
        m_text.insert(m_caret, allowed);
        m_caret += allowed.size();
        m_editing = true;
        reset_blink();
        mark_needs_paint();
    }

    void reset_blink() noexcept {
        m_caret_visible = true;
        m_blink_time = std::chrono::steady_clock::now();
    }

    void update_focus_anim() {
        auto now = std::chrono::steady_clock::now();
        if (m_anim_time.time_since_epoch().count() == 0) m_anim_time = now;
        float dt = std::clamp(std::chrono::duration<float>(now - m_anim_time).count(), 0.0f, 0.05f);
        m_anim_time = now;
        float target = (m_focused && !m_disabled) ? 1.0f : 0.0f;
        if (std::abs(m_focus_anim - target) > 0.005f) {
            m_focus_anim += (target - m_focus_anim) * std::min(1.0f, dt * 22.0f);
            if (std::abs(m_focus_anim - target) <= 0.005f) m_focus_anim = target;
            mark_needs_paint();
        }
    }

    void setup_behavior() {
        on_focus = [this](FocusEvent& e) {
            m_focused = e.gained_focus && !m_disabled;
            if (m_focused) {
                m_caret = m_text.size();
                m_select_all = true;
                reset_blink();
            } else {
                commit_text_if_editing();
                m_select_all = false;
            }
            m_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        };

        on_pointer_enter = [this](PointerEvent&) {
            if (!m_disabled) { m_hovered = true; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_hover_spin = SpinPart::None;
            m_press_spin = SpinPart::None;
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (m_disabled) return;
            if (e.type == PlatformEventType::PointerScroll && m_focused) {
                step(e.delta.y > 0.0f ? -m_small_change : m_small_change);
                e.stop_propagation();
                return;
            }
            SpinPart hp = SpinPart::None;
            if (spin_buttons_visible()) {
                if (up_button_rect().contains(e.local_position)) hp = SpinPart::Up;
                else if (down_button_rect().contains(e.local_position)) hp = SpinPart::Down;
            }
            if (hp != m_hover_spin) {
                m_hover_spin = hp;
                cursor_shape = (hp != SpinPart::None) ? CursorShape::PointingHand : CursorShape::IBeam;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            if (spin_buttons_visible()) {
                if (up_button_rect().contains(e.local_position)) {
                    m_press_spin = SpinPart::Up;
                    step(m_small_change);
                    e.stop_propagation();
                    return;
                }
                if (down_button_rect().contains(e.local_position)) {
                    m_press_spin = SpinPart::Down;
                    step(-m_small_change);
                    e.stop_propagation();
                    return;
                }
            }
            m_select_all = false;
            m_caret = m_text.size();
            reset_blink();
            mark_needs_paint();
            e.stop_propagation();
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (m_press_spin != SpinPart::None) {
                m_press_spin = SpinPart::None;
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_key = [this](KeyEvent& e) {
            if (m_disabled || e.type != PlatformEventType::KeyDown) return;
            if (e.modifiers.ctrl) {
                if (e.key_code == 'A' || e.key_code == 'a') {
                    m_select_all = !m_text.empty();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
                if (e.key_code == 'C' || e.key_code == 'c') {
                    Clipboard::set_text(m_text);
                    e.stop_propagation();
                    return;
                }
                if (e.key_code == 'V' || e.key_code == 'v') {
                    insert_text(Clipboard::get_text());
                    e.stop_propagation();
                    return;
                }
            }

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
                case 13: // Enter -> 提交并求值表达式
                    commit_text_if_editing();
                    e.stop_propagation();
                    return;
                case 8: // Backspace
                    if (m_select_all) {
                        m_text.clear();
                        m_caret = 0;
                        m_select_all = false;
                        m_editing = true;
                    } else if (m_caret > 0) {
                        m_text.erase(m_caret - 1, 1);
                        --m_caret;
                        m_editing = true;
                    }
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 46: // Delete
                    if (m_select_all) {
                        m_text.clear();
                        m_caret = 0;
                        m_select_all = false;
                        m_editing = true;
                    } else if (m_caret < m_text.size()) {
                        m_text.erase(m_caret, 1);
                        m_editing = true;
                    }
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 37: // Left
                    m_select_all = false;
                    if (m_caret > 0) --m_caret;
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 39: // Right
                    m_select_all = false;
                    if (m_caret < m_text.size()) ++m_caret;
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                default:
                    if (!e.text.empty()) {
                        insert_text(e.text);
                        e.stop_propagation();
                    }
                    break;
            }
        };
    }

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
    std::function<void(double)> m_on_value_changed;

    std::string m_text{"0"};
    size_t m_caret{1};
    bool m_editing{false};
    bool m_select_all{false};
    bool m_focused{false};
    bool m_hovered{false};
    bool m_caret_visible{true};
    SpinPart m_hover_spin{SpinPart::None};
    SpinPart m_press_spin{SpinPart::None};
    float m_focus_anim{0.0f};
    std::chrono::steady_clock::time_point m_blink_time{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point m_anim_time{};
};

// C++20 指定初始化器声明式 WinUI 3 NumberBox
struct NumberBox {
    Bindable<double> value{0.0};
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
    std::function<void(double)> on_value_changed{nullptr};

    operator WidgetPtr() const;
};

class NumberBoxWidget : public Widget {
public:
    explicit NumberBoxWidget(const NumberBox& desc) : m_desc(desc) {}
    ~NumberBoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderNumberBox>();
        apply(node);
        bind(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto nb = std::dynamic_pointer_cast<RenderNumberBox>(node)) {
            apply(nb);
            bind(nb);
        }
    }

private:
    void apply(const std::shared_ptr<RenderNumberBox>& nb) const {
        auto desc_val = m_desc.value;
        auto user_cb = m_desc.on_value_changed;
        nb->set_properties(
            m_desc.value.get(), m_desc.minimum, m_desc.maximum,
            m_desc.small_change, m_desc.large_change, m_desc.precision,
            m_desc.accepts_expression, m_desc.spin_button_placement,
            m_desc.placeholder, m_desc.header, m_desc.width, m_desc.disabled,
            [desc_val, user_cb](double v) mutable {
                desc_val.set_bound(v);
                if (user_cb) user_cb(v);
            });
    }

    void bind(const std::shared_ptr<RenderNumberBox>& nb) {
        m_bindings.clear();
        m_bindings.attach(m_desc.value, nb, [](RenderNumberBox& n, double v) {
            n.set_numeric_value(v);
        });
    }

    NumberBox m_desc;
    BindingGroup m_bindings;
};

inline NumberBox::operator WidgetPtr() const {
    return std::make_shared<NumberBoxWidget>(*this);
}

} // namespace ktui
