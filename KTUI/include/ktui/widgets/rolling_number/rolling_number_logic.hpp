#pragma once

#include "rolling_number_data.hpp"
#include "rolling_number_style.hpp"
#include <ktui/animation/motion.hpp>
#include <ktui/animation/spring_curve.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace ktui {


// ============================================================================
// 滚动翻滚数值组件 (Rolling Counter Digits)
// 配合等宽数字排版 tabular_nums，每个发生变化的数位在局部 ClipRect 窗口内
// 沿数值增减方向（上/下）进行二阶物理弹簧滚动切换，并可选叠加 OkLab 增减瞬时微光
// ============================================================================
class RenderRollingNumber final : public RenderBox {
public:
    RenderRollingNumber() noexcept
        : m_flash_spring(0.0f, SpringConfig::with_damping_ratio(280.0f, 1.0f, 1.0f, 0.01f, 0.05f)) {}

    void set_properties(double value,
                        int decimals,
                        std::string prefix,
                        std::string suffix,
                        float font_size,
                        FontWeight weight,
                        Color color,
                        bool show_delta_flash,
                        float stiffness,
                        float damping) {
        const bool fmt_changed = (m_decimals != decimals || m_prefix != prefix || m_suffix != suffix ||
                                  std::abs(m_font_size - font_size) > 0.01f || m_weight != weight);
        m_decimals = std::clamp(decimals, 0, 6);
        m_prefix = std::move(prefix);
        m_suffix = std::move(suffix);
        m_font_size = std::max(8.0f, font_size);
        m_weight = weight;
        m_color = color;
        m_show_delta_flash = show_delta_flash;
        m_spring_cfg = SpringConfig(std::max(120.0f, stiffness), std::max(12.0f, damping), 1.0f, 0.01f, 0.05f);

        const std::string target_str = format_number(value);
        if (!m_initialized) {
            m_initialized = true;
            m_value = value;
            m_last_text = target_str;
            m_slots.clear();
            m_slots.reserve(target_str.size());
            for (char ch : target_str) {
                SlotAnim s{};
                s.prev_char = ch;
                s.curr_char = ch;
                s.spring = SpringSolver(1.0f, m_spring_cfg);
                m_slots.push_back(s);
            }
            mark_needs_layout();
            return;
        }

        if (target_str != m_last_text || fmt_changed) {
            const float dir = (value >= m_value) ? 1.0f : -1.0f;
            if (value != m_value && m_show_delta_flash) {
                m_delta_sign = dir;
                m_flash_spring.rebind(1.0f, 0.0f, 0.0f);
                wake();
            }
            m_value = value;
            rebuild_slots(target_str, dir);
            mark_needs_layout();
            mark_needs_paint();
        }
    }

    void set_value(double v) {
        set_properties(v, m_decimals, m_prefix, m_suffix, m_font_size, m_weight,
                       m_color, m_show_delta_flash, m_spring_cfg.stiffness, m_spring_cfg.damping);
    }

    [[nodiscard]] double value() const noexcept { return m_value; }
    [[nodiscard]] bool is_animating() const noexcept { return m_animating; }

    void perform_layout() override {
        const Vec2 cell = TextMeasure::measure("0", m_font_size, m_weight, false, "", true);
        m_glyph_h = std::max(m_font_size, std::ceil(cell.y));
        const float pad_y = std::max(8.0f, std::ceil(m_font_size * 0.25f));
        m_cell_h = m_glyph_h + pad_y * 2.0f;
        m_digit_w = std::max(4.0f, cell.x);

        float total_w = 0.0f;
        m_slot_widths.resize(m_slots.size());
        for (size_t i = 0; i < m_slots.size(); ++i) {
            const char c = m_slots[i].curr_char;
            float w = m_digit_w;
            if (c < '0' || c > '9') {
                char buf[2]{c, '\0'};
                w = std::max(3.0f, TextMeasure::measure_width(buf, m_font_size, m_weight, false, "", true));
            }
            m_slot_widths[i] = w;
            total_w += w;
        }
        m_size = m_constraints.constrain(Vec2{std::ceil(total_w), m_cell_h});
    }

    void paint(PaintContext& ctx) override {
        advance_springs();
        if (m_animating) {
            mark_needs_paint();
        }

        const Theme& theme = ThemeContext::current();
        const auto resolved_style = RollingNumberStyleResolver::resolve(theme);
        Color base_col = (m_color.a >= 0.0f) ? m_color : resolved_style.text_primary;
        const float flash = m_show_delta_flash ? std::clamp(m_flash_spring.value(), 0.0f, 1.0f) : 0.0f;
        if (flash > 0.01f) {
            const Color accent = (m_delta_sign >= 0.0f)
                ? (theme.is_dark ? Color{0.25f, 0.90f, 0.58f, 1.0f} : Color{0.05f, 0.58f, 0.34f, 1.0f})
                : (theme.is_dark ? Color{0.98f, 0.45f, 0.40f, 1.0f} : Color{0.82f, 0.18f, 0.16f, 1.0f});
            base_col = Motion::lerp(base_col, accent, flash * 0.85f);
        }

        float x = 0.0f;
        const float top_y = std::round((m_size.y - m_glyph_h) * 0.5f);
        const float travel_h = std::round(std::clamp(m_glyph_h * 0.48f, 10.0f, m_glyph_h * 0.65f));
        const float clip_slack_y = std::max(4.0f, std::ceil(m_font_size * 0.15f));
        for (size_t i = 0; i < m_slots.size(); ++i) {
            const float w = (i < m_slot_widths.size()) ? m_slot_widths[i] : m_digit_w;
            const auto& slot = m_slots[i];
            const float p = slot.spring.value();
            const float pc = std::clamp(p, 0.0f, 1.0f);

            if (!slot.spring.is_settled() && slot.prev_char != slot.curr_char) {
                const Rect slot_clip = Rect::from_xywh(
                    x - 4.0f, -clip_slack_y, w + 8.0f, m_size.y + clip_slack_y * 2.0f);
                ctx.push_clip(slot_clip, 0.0f);
                if (slot.prev_char != '\0') {
                    Color prev_c = base_col;
                    prev_c.a *= (1.0f - pc);
                    if (prev_c.a > 0.01f) {
                        char b[2]{slot.prev_char, '\0'};
                        ctx.draw_text(b, PointF{std::round(x), std::round(top_y - slot.direction * p * travel_h)},
                                      m_font_size, prev_c, m_weight, false, "", true);
                    }
                }
                if (slot.curr_char != '\0') {
                    Color curr_c = base_col;
                    curr_c.a *= pc;
                    if (curr_c.a > 0.01f) {
                        char b[2]{slot.curr_char, '\0'};
                        ctx.draw_text(b, PointF{std::round(x), std::round(top_y + slot.direction * (1.0f - p) * travel_h)},
                                      m_font_size, curr_c, m_weight, false, "", true);
                    }
                }
                ctx.pop_clip();
            } else if (slot.curr_char != '\0') {
                char b[2]{slot.curr_char, '\0'};
                ctx.draw_text(b, PointF{std::round(x), top_y}, m_font_size, base_col, m_weight, false, "", true);
            }
            x += w;
        }
    }

private:
    struct SlotAnim {
        char prev_char{'0'};
        char curr_char{'0'};
        float direction{1.0f};
        SpringSolver spring{1.0f};
    };

    [[nodiscard]] std::string format_number(double v) const {
        char num_buf[64];
        std::snprintf(num_buf, sizeof(num_buf), "%.*f", m_decimals, v);
        return m_prefix + num_buf + m_suffix;
    }

    void rebuild_slots(const std::string& target, float dir) {
        std::vector<SlotAnim> next;
        next.resize(target.size());
        const size_t old_n = m_slots.size();
        const size_t new_n = target.size();
        for (size_t i = 0; i < new_n; ++i) {
            const size_t rev = new_n - 1 - i;
            const char tc = target[i];
            auto& dst = next[i];
            dst.curr_char = tc;
            dst.direction = dir;
            if (rev < old_n) {
                const char oc = m_slots[old_n - 1 - rev].curr_char;
                if (oc == tc) {
                    dst.prev_char = tc;
                    dst.spring = SpringSolver(1.0f, m_spring_cfg);
                } else {
                    dst.prev_char = oc;
                    dst.spring = SpringSolver(0.0f, m_spring_cfg);
                    dst.spring.rebind(0.0f, 0.0f, 1.0f);
                }
            } else {
                dst.prev_char = '\0';
                dst.spring = SpringSolver(0.0f, m_spring_cfg);
                dst.spring.rebind(0.0f, 0.0f, 1.0f);
            }
        }
        m_slots = std::move(next);
        m_last_text = target;
        wake();
    }

    void wake() noexcept {
        m_animating = true;
        if (!m_has_tick) {
            m_last_tick = std::chrono::steady_clock::now();
            m_has_tick = true;
        }
    }

    void advance_springs() noexcept {
        if (!m_animating) return;
        const auto now = std::chrono::steady_clock::now();
        float dt = 1.0f / 60.0f;
        if (m_has_tick) {
            dt = std::clamp(std::chrono::duration<float>(now - m_last_tick).count(), 0.001f, 0.05f);
        }
        m_last_tick = now;
        m_has_tick = true;

        bool any_active = false;
        for (auto& s : m_slots) {
            s.spring.update(dt);
            if (!s.spring.is_settled()) any_active = true;
        }
        m_flash_spring.update(dt);
        if (!m_flash_spring.is_settled()) any_active = true;

        m_animating = any_active;
        if (!any_active) {
            m_has_tick = false;
        }
    }

    bool m_initialized{false};
    bool m_animating{false};
    bool m_has_tick{false};
    double m_value{0.0};
    int m_decimals{0};
    std::string m_prefix;
    std::string m_suffix;
    std::string m_last_text;
    float m_font_size{24.0f};
    FontWeight m_weight{FontWeight::Bold};
    Color m_color{-1.0f, -1.0f, -1.0f, -1.0f};
    bool m_show_delta_flash{false};
    float m_delta_sign{1.0f};
    float m_digit_w{14.0f};
    float m_glyph_h{24.0f};
    float m_cell_h{28.0f};
    SpringConfig m_spring_cfg{480.0f, 44.0f, 1.0f, 0.01f, 0.05f};
    SpringSolver m_flash_spring;
    std::vector<SlotAnim> m_slots;
    std::vector<float> m_slot_widths;
    std::chrono::steady_clock::time_point m_last_tick{};
};

struct RollingNumber : BindableMixin<RollingNumber, RenderRollingNumber> {
    double value{0.0};
    int decimals{0};
    std::string prefix{""};
    std::string suffix{""};
    float font_size{24.0f};
    FontWeight font_weight{FontWeight::Bold};
    Color color{-1.0f, -1.0f, -1.0f, -1.0f};
    bool show_delta_flash{false};
    float spring_stiffness{480.0f};
    float spring_damping{44.0f};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class RollingNumberWidget final : public Widget {
public:
    explicit RollingNumberWidget(const RollingNumber& d) : m_desc(d) {}
    explicit RollingNumberWidget(RollingNumber&& d) noexcept : m_desc(std::move(d)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderRollingNumber>();
        apply(n);
        return n;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderRollingNumber>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderRollingNumber>& n) const {
        n->set_properties(m_desc.value, m_desc.decimals, m_desc.prefix, m_desc.suffix,
                          m_desc.font_size, m_desc.font_weight, m_desc.color,
                          m_desc.show_delta_flash, m_desc.spring_stiffness, m_desc.spring_damping);
    }

    RollingNumber m_desc;
};

inline RollingNumber::operator WidgetPtr() const& { return make_widget<RollingNumberWidget>(*this); }
inline RollingNumber::operator WidgetPtr() && { return make_widget<RollingNumberWidget>(std::move(*this)); }

// ============================================================================
// 状态微交叉平滑切换文案 (CrossfadeText)
// ============================================================================
class RenderCrossfadeText final : public RenderBox {
public:
    RenderCrossfadeText() noexcept
        : m_spring(1.0f, SpringConfig::with_damping_ratio(440.0f, 1.0f, 1.0f, 0.01f, 0.05f)) {}

    void set_properties(std::string text, float font_size, FontWeight weight, Color color) {
        m_font_size = std::max(8.0f, font_size);
        m_weight = weight;
        m_color = color;
        if (!m_initialized) {
            m_initialized = true;
            m_curr = std::move(text);
            m_prev = m_curr;
            m_spring.reset(1.0f);
            mark_needs_layout();
            return;
        }
        if (m_curr != text) {
            m_prev = std::move(m_curr);
            m_curr = std::move(text);
            m_spring.rebind(0.0f, 0.0f, 1.0f);
            m_animating = true;
            m_last_tick = std::chrono::steady_clock::now();
            m_has_tick = true;
            mark_needs_layout();
            mark_needs_paint();
        }
    }

    void set_text(std::string text) {
        set_properties(std::move(text), m_font_size, m_weight, m_color);
    }

    void perform_layout() override {
        const Vec2 sc = TextMeasure::measure(m_curr.empty() ? " " : m_curr, m_font_size, m_weight, false);
        const Vec2 sp = TextMeasure::measure(m_prev.empty() ? " " : m_prev, m_font_size, m_weight, false);
        m_glyph_h = std::max(m_font_size, std::ceil(std::max(sc.y, sp.y)));
        const float pad_y = std::max(6.0f, std::ceil(m_font_size * 0.25f));
        m_size = m_constraints.constrain(Vec2{std::ceil(std::max(sc.x, sp.x)) + 8.0f, m_glyph_h + pad_y * 2.0f});
    }

    void paint(PaintContext& ctx) override {
        if (m_animating) {
            const auto now = std::chrono::steady_clock::now();
            const float dt = m_has_tick
                ? std::clamp(std::chrono::duration<float>(now - m_last_tick).count(), 0.001f, 0.05f)
                : (1.0f / 60.0f);
            m_last_tick = now;
            m_has_tick = true;
            m_spring.update(dt);
            m_animating = !m_spring.is_settled();
            if (!m_animating) m_has_tick = false;
            else mark_needs_paint();
        }

        const Theme& theme = ThemeContext::current();
        const auto resolved_style = RollingNumberStyleResolver::resolve(theme);
        const Color col = (m_color.a >= 0.0f) ? m_color : resolved_style.text_primary;
        const float p = std::clamp(m_spring.value(), 0.0f, 1.0f);
        const float base_y = std::round((m_size.y - m_glyph_h) * 0.5f);
        const bool switching = (!m_spring.is_settled() && m_prev != m_curr);
        const float slide_px = 6.0f;
        const float clip_slack_y = std::max(4.0f, std::ceil(m_font_size * 0.20f));

        if (switching) {
            ctx.push_clip(Rect::from_xywh(-3.0f, -clip_slack_y, m_size.x + 6.0f, m_size.y + clip_slack_y * 2.0f), 0.0f);
            if (!m_prev.empty()) {
                Color c_old = col;
                c_old.a *= (1.0f - p);
                if (c_old.a > 0.01f) {
                    ctx.draw_text(m_prev, PointF{0.0f, std::round(base_y - slide_px * p)}, m_font_size, c_old, m_weight, false);
                }
            }
        }
        if (!m_curr.empty()) {
            Color c_new = col;
            c_new.a *= (switching ? p : 1.0f);
            if (c_new.a > 0.01f) {
                const float y_off = switching ? (slide_px * (1.0f - p)) : 0.0f;
                ctx.draw_text(m_curr, PointF{0.0f, std::round(base_y + y_off)}, m_font_size, c_new, m_weight, false);
            }
        }
        if (switching) {
            ctx.pop_clip();
        }
    }

private:
    bool m_initialized{false};
    bool m_animating{false};
    bool m_has_tick{false};
    std::string m_prev;
    std::string m_curr;
    float m_font_size{14.0f};
    float m_glyph_h{18.0f};
    FontWeight m_weight{FontWeight::SemiBold};
    Color m_color{-1.0f, -1.0f, -1.0f, -1.0f};
    SpringSolver m_spring;
    std::chrono::steady_clock::time_point m_last_tick{};
};

struct CrossfadeText : BindableMixin<CrossfadeText, RenderCrossfadeText> {
    std::string text{""};
    float font_size{14.0f};
    FontWeight font_weight{FontWeight::SemiBold};
    Color color{-1.0f, -1.0f, -1.0f, -1.0f};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class CrossfadeTextWidget final : public Widget {
public:
    explicit CrossfadeTextWidget(const CrossfadeText& d) : m_desc(d) {}
    explicit CrossfadeTextWidget(CrossfadeText&& d) noexcept : m_desc(std::move(d)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderCrossfadeText>();
        n->set_properties(m_desc.text, m_desc.font_size, m_desc.font_weight, m_desc.color);
        return n;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderCrossfadeText>(node)) {
            n->set_properties(m_desc.text, m_desc.font_size, m_desc.font_weight, m_desc.color);
        }
    }

private:
    CrossfadeText m_desc;
};

inline CrossfadeText::operator WidgetPtr() const& { return make_widget<CrossfadeTextWidget>(*this); }
inline CrossfadeText::operator WidgetPtr() && { return make_widget<CrossfadeTextWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
