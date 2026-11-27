#pragma once

#include "scrub_number_box_data.hpp"
#include "scrub_number_box_style.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ktui {


// Blender / Figma 风格微调 Scrubbing 数字输入框 (Drag-to-Scrub Input)
// - 帧间增量拖拽：按住左右拖动无级增减，中途切换 Shift (10×) / Alt (0.1×) 平滑无跳变
// - 悬停滚轮微调 + 两侧极简 ‹ / › 单步微调按钮
// - 单击数值进入全选编辑（直接打字覆盖），双击恢复默认值
class RenderScrubNumberBox : public RenderBox {
public:
    RenderScrubNumberBox(std::string label = "Value",
                         double value = 0.0,
                         double min_val = -10000.0,
                         double max_val = 10000.0,
                         double step = 1.0,
                         int precision = 1,
                         std::string unit = "",
                         Fn<void(double)> on_changed = nullptr)
        : m_label(std::move(label)),
          m_value(std::clamp(value, min_val, max_val)),
          m_default_value(std::clamp(value, min_val, max_val)),
          m_min(min_val),
          m_max(std::max(min_val, max_val)),
          m_step(std::max(1e-6, step)),
          m_precision(std::clamp(precision, 0, 4)),
          m_unit(std::move(unit)),
          m_on_changed(std::move(on_changed)) {
        is_focusable = true;
        cursor_shape = CursorShape::ResizeEW;
        setup_behavior();
    }

    [[nodiscard]] double value() const noexcept { return m_value; }

    void set_value(double v, bool notify = false) {
        double clamped = std::clamp(v, m_min, m_max);
        if (std::abs(m_value - clamped) > 1e-7) {
            m_value = clamped;
            mark_needs_paint();
            if (notify && m_on_changed) m_on_changed(m_value);
        }
    }

    void set_on_changed(Fn<void(double)> cb) { m_on_changed = std::move(cb); }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(m_constraints.min_width, 190.0f);
        float h = m_constraints.has_bounded_height() ? std::min(m_constraints.max_height, 32.0f) : 32.0f;
        m_size = m_constraints.constrain(Vec2{w, std::max(28.0f, h)});
        m_btn_dec = Rect::from_xywh(m_size.x - 42.0f, 3.0f, 18.0f, m_size.y - 6.0f);
        m_btn_inc = Rect::from_xywh(m_size.x - 22.0f, 3.0f, 18.0f, m_size.y - 6.0f);
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        if (m_editing) {
            cursor_shape = CursorShape::IBeam;
        } else if (!m_scrubbing && m_hovered && (m_btn_dec.contains(position) || m_btn_inc.contains(position))) {
            cursor_shape = CursorShape::PointingHand;
        } else {
            cursor_shape = CursorShape::ResizeEW;
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        const Theme& t = ThemeContext::current();
        const auto resolved_style = ScrubNumberBoxStyleResolver::resolve(t);
        const bool is_dark = t.is_dark;
        const Color accent = resolved_style.accent;
        const bool active = m_scrubbing || m_editing || m_focused;

        Color bg = is_dark
            ? Color{1.0f, 1.0f, 1.0f, active ? 0.075f : (m_hovered ? 0.06f : 0.042f)}
            : Color{1.0f, 1.0f, 1.0f, active ? 0.96f : (m_hovered ? 0.90f : 0.80f)};
        Color bd = active ? accent : (is_dark ? Color{1.0f, 1.0f, 1.0f, 0.12f} : Color{0.0f, 0.0f, 0.0f, 0.14f});

        context.draw_rect(bounds(), bg, 5.0f, 0.0f);

        // 有限区间内绘制柔和进度底槽
        if (m_max > m_min && (m_max - m_min) <= 100000.0) {
            float ratio = static_cast<float>(std::clamp((m_value - m_min) / (m_max - m_min), 0.0, 1.0));
            if (ratio > 0.002f) {
                context.push_clip(bounds(), 5.0f);
                context.draw_rect(Rect::from_xywh(0.0f, 0.0f, m_size.x * ratio, m_size.y),
                                  Color{accent.r, accent.g, accent.b, m_scrubbing ? 0.24f : 0.11f}, 0.0f, 0.0f);
                context.pop_clip();
            }
        }

        context.draw_rect(bounds(), bd, 5.0f, active ? 1.5f : 1.0f);

        // 左侧标签 + 极简拖拽提示符
        Vec2 ls = TextMeasure::measure(m_label, 12.0f, FontWeight::Medium);
        context.draw_text(m_label, PointF{10.0f, std::round((m_size.y - ls.y) * 0.5f)},
                          12.0f, m_scrubbing ? accent : resolved_style.text_secondary, FontWeight::Medium);

        // 拖拽倍率实时提示角标 (Shift 10× / Alt 0.1×)
        if (m_scrubbing && m_active_mult != 1.0) {
            std::string mb = (m_active_mult > 1.0) ? "10×" : "0.1×";
            Vec2 ms = TextMeasure::measure(mb, 10.0f, FontWeight::SemiBold);
            Rect mr = Rect::from_xywh(14.0f + ls.x, (m_size.y - 16.0f) * 0.5f, ms.x + 8.0f, 16.0f);
            context.draw_rect(mr, Color{accent.r, accent.g, accent.b, 0.22f}, 8.0f, 0.0f);
            context.draw_text(mb, PointF{std::round(mr.min_x + 4.0f), std::round(mr.min_y + (16.0f - ms.y) * 0.5f)},
                              10.0f, accent, FontWeight::SemiBold);
        }

        const bool show_steppers = m_hovered && !m_scrubbing && !m_editing;
        const float right_margin = show_steppers ? 48.0f : 10.0f;

        // 右侧数值 + 单位 (或全选编辑态，默认启用等宽数字避免拖拽抖动)
        std::string disp = m_editing ? m_edit_buf : format_value();
        Vec2 ds = TextMeasure::measure(disp, 12.0f, FontWeight::SemiBold, false, "", true);
        float rx = std::max(ls.x + 20.0f, m_size.x - ds.x - right_margin);
        float ty = std::round((m_size.y - ds.y) * 0.5f);

        if (m_editing && m_select_all && !disp.empty()) {
            context.draw_rect(Rect::from_xywh(rx - 2.0f, ty - 1.0f, ds.x + 4.0f, ds.y + 2.0f),
                              Color{accent.r, accent.g, accent.b, 0.28f}, 3.0f, 0.0f);
        }
        context.draw_text(disp, PointF{std::round(rx), ty}, 12.0f, resolved_style.text_primary, FontWeight::SemiBold, false, "", true);

        if (m_editing && !m_select_all) {
            context.draw_rect(Rect::from_xywh(std::round(rx + ds.x + 1.5f), (m_size.y - 14.0f) * 0.5f, 1.5f, 14.0f),
                              accent, 0.75f, 0.0f);
        }

        // 悬停时右侧浮现极简 ‹ / › 单步微调按钮
        if (show_steppers) {
            auto draw_step_btn = [&](Rect r, const char* glyph, bool hov) {
                if (hov) {
                    context.draw_rect(r, is_dark ? Color{1, 1, 1, 0.10f} : Color{0, 0, 0, 0.07f}, 3.0f, 0.0f);
                }
                Vec2 gs = TextMeasure::measure(glyph, 12.0f, FontWeight::SemiBold);
                context.draw_text(glyph,
                                  PointF{std::round(r.min_x + (r.width() - gs.x) * 0.5f),
                                         std::round(r.min_y + (r.height() - gs.y) * 0.5f - 0.5f)},
                                  12.0f, hov ? accent : resolved_style.text_secondary, FontWeight::SemiBold);
            };
            draw_step_btn(m_btn_dec, "‹", m_hover_step == -1);
            draw_step_btn(m_btn_inc, "›", m_hover_step == 1);
        }
    }

private:
    [[nodiscard]] std::string format_value() const {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", m_precision, m_value);
        std::string s(buf);
        if (!m_unit.empty()) {
            s += ' ';
            s += m_unit;
        }
        return s;
    }

    void commit_edit() {
        if (!m_editing) return;
        m_editing = false;
        m_select_all = false;
        if (!m_edit_buf.empty()) {
            char* endp = nullptr;
            double parsed = std::strtod(m_edit_buf.c_str(), &endp);
            if (endp && endp != m_edit_buf.c_str()) {
                set_value(parsed, true);
            }
        }
        mark_needs_paint();
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) { m_hovered = true; mark_needs_paint(); };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_hover_step = 0;
            mark_needs_paint();
        };

        on_focus = [this](FocusEvent& ev) {
            m_focused = ev.gained_focus;
            if (!m_focused && m_editing) commit_edit();
            mark_needs_paint();
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;

            if (!m_editing && m_hovered) {
                double mult = ev.modifiers.shift ? 10.0 : (ev.modifiers.alt ? 0.1 : 1.0);
                if (m_btn_dec.contains(ev.local_position)) {
                    set_value(m_value - m_step * mult, true);
                    ev.stop_propagation();
                    return;
                }
                if (m_btn_inc.contains(ev.local_position)) {
                    set_value(m_value + m_step * mult, true);
                    ev.stop_propagation();
                    return;
                }
            }

            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click).count();
            if (ms <= 340 && !m_editing) {
                m_last_click = {};
                set_value(m_default_value, true);
                ev.stop_propagation();
                return;
            }
            m_last_click = now;
            m_pressed = true;
            m_scrubbing = false;
            m_drag_start_x = ev.local_position.x;
            m_prev_x = ev.local_position.x;
            m_active_mult = 1.0;
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& ev) {
            // 鼠标滚轮直接微调数值 (悬停态与聚焦编辑态均支持，且阻止冒泡避免外层页面误滚动)
            if (ev.type == PlatformEventType::PointerScroll) {
                float delta = (std::abs(ev.delta.y) > 0.0001f) ? ev.delta.y : ev.delta.x;
                float dir = (delta > 0.0f) ? 1.0f : (delta < 0.0f ? -1.0f : 0.0f);
                if (dir != 0.0f) {
                    if (m_editing) commit_edit();
                    double mult = ev.modifiers.shift ? 10.0 : (ev.modifiers.alt ? 0.1 : 1.0);
                    set_value(m_value + dir * m_step * mult, true);
                    ev.stop_propagation();
                }
                return;
            }

            if (m_pressed && !m_editing) {
                if (!m_scrubbing && std::abs(ev.local_position.x - m_drag_start_x) >= 3.0f) {
                    m_scrubbing = true;
                    m_prev_x = ev.local_position.x;
                }
                if (m_scrubbing) {
                    float frame_dx = ev.local_position.x - m_prev_x;
                    m_prev_x = ev.local_position.x;
                    double mult = ev.modifiers.shift ? 10.0 : (ev.modifiers.alt ? 0.1 : 1.0);
                    m_active_mult = mult;
                    if (std::abs(frame_dx) > 0.001f) {
                        set_value(m_value + static_cast<double>(frame_dx) * 0.25 * mult * m_step, true);
                    } else {
                        mark_needs_paint();
                    }
                    ev.stop_propagation();
                }
                return;
            }

            int hs = 0;
            if (m_btn_dec.contains(ev.local_position)) hs = -1;
            else if (m_btn_inc.contains(ev.local_position)) hs = 1;
            if (hs != m_hover_step) {
                m_hover_step = hs;
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (!m_pressed) return;
            bool was_scrub = m_scrubbing;
            m_pressed = false;
            m_scrubbing = false;
            m_active_mult = 1.0;
            if (!was_scrub && bounds().contains(ev.local_position)) {
                m_editing = true;
                m_select_all = true;
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.*f", m_precision, m_value);
                m_edit_buf = buf;
            }
            mark_needs_paint();
            ev.stop_propagation();
        };

        on_key = [this](KeyEvent& ev) {
            if (!m_editing) {
                if (ev.type == PlatformEventType::KeyDown && (ev.key_code == 0x26 || ev.key_code == 0x28)) {
                    double mult = ev.modifiers.shift ? 10.0 : (ev.modifiers.alt ? 0.1 : 1.0);
                    set_value(m_value + (ev.key_code == 0x26 ? 1.0 : -1.0) * m_step * mult, true);
                    ev.stop_propagation();
                }
                return;
            }
            if (ev.type == PlatformEventType::ImeCommit) {
                for (char c : ev.text) {
                    if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
                        if (m_select_all) { m_edit_buf.clear(); m_select_all = false; }
                        m_edit_buf.push_back(c);
                    }
                }
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (ev.type != PlatformEventType::KeyDown) return;
            if (ev.key_code == 0x08) {
                if (m_select_all) { m_edit_buf.clear(); m_select_all = false; }
                else if (!m_edit_buf.empty()) m_edit_buf.pop_back();
                mark_needs_paint();
                ev.stop_propagation();
            } else if (ev.key_code == 0x0D) {
                commit_edit();
                ev.stop_propagation();
            } else if (ev.key_code == 0x1B) {
                m_editing = false;
                m_select_all = false;
                mark_needs_paint();
                ev.stop_propagation();
            }
        };
    }

    std::string m_label;
    double m_value{0.0};
    double m_default_value{0.0};
    double m_min{-10000.0};
    double m_max{10000.0};
    double m_step{1.0};
    int m_precision{1};
    std::string m_unit;
    Fn<void(double)> m_on_changed;

    Rect m_btn_dec{};
    Rect m_btn_inc{};
    int m_hover_step{0};
    bool m_hovered{false};
    bool m_focused{false};
    bool m_pressed{false};
    bool m_scrubbing{false};
    bool m_editing{false};
    bool m_select_all{false};
    float m_drag_start_x{0.0f};
    float m_prev_x{0.0f};
    double m_active_mult{1.0};
    std::string m_edit_buf;
    std::chrono::steady_clock::time_point m_last_click{};
};

struct ScrubNumberBox : BindableMixin<ScrubNumberBox, RenderScrubNumberBox> {
    std::string label{"Value"};
    double value{0.0};
    double min_value{-10000.0};
    double max_value{10000.0};
    double step{1.0};
    int precision{1};
    std::string unit{""};
    Fn<void(double)> on_changed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class ScrubNumberBoxWidget : public Widget {
public:
    explicit ScrubNumberBoxWidget(const ScrubNumberBox& desc) : m_desc(desc) {}
    explicit ScrubNumberBoxWidget(ScrubNumberBox&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderScrubNumberBox>(
            m_desc.label, m_desc.value, m_desc.min_value, m_desc.max_value,
            m_desc.step, m_desc.precision, m_desc.unit, m_desc.on_changed);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderScrubNumberBox>(node)) {
            n->set_on_changed(m_desc.on_changed);
        }
    }
private:
    ScrubNumberBox m_desc;
};

inline ScrubNumberBox::operator WidgetPtr() const& { return make_widget<ScrubNumberBoxWidget>(*this); }
inline ScrubNumberBox::operator WidgetPtr() && { return make_widget<ScrubNumberBoxWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
