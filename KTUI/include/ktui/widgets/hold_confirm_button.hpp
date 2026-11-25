#pragma once

#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace ktui {

// 长按确认按钮外观形态：胶囊进度条 / 圆形进度环
enum class HoldConfirmShape : uint8_t {
    PillBar = 0,
    CircularRing = 1
};

// 防误触“长按确认”按钮 (Hold-to-Confirm Button)
// 支持条形进度填充 (PillBar) 与圆形 360° 进度环 (CircularRing) 两种形态
class RenderHoldConfirmButton : public RenderBox {
public:
    RenderHoldConfirmButton(std::string text = "长按 1.5s 确认删除",
                            std::string holding_text = "继续按住确认...",
                            std::string confirmed_text = "✓ 已执行",
                            float hold_duration_sec = 1.5f,
                            bool danger = true,
                            HoldConfirmShape shape = HoldConfirmShape::PillBar,
                            float diameter = 64.0f,
                            Fn<void()> on_confirmed = nullptr)
        : m_text(std::move(text)),
          m_holding_text(std::move(holding_text)),
          m_confirmed_text(std::move(confirmed_text)),
          m_hold_duration(std::max(0.2f, hold_duration_sec)),
          m_danger(danger),
          m_shape(shape),
          m_diameter(std::max(40.0f, diameter)),
          m_on_confirmed(std::move(on_confirmed)) {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    void set_on_confirmed(Fn<void()> cb) { m_on_confirmed = std::move(cb); }
    [[nodiscard]] float progress() const noexcept { return m_progress; }
    [[nodiscard]] bool is_holding() const noexcept { return m_holding; }

    void advance_by(float dt_sec) {
        if (dt_sec <= 0.0f) return;
        if (m_holding) {
            m_progress = std::min(1.0f, m_progress + dt_sec / m_hold_duration);
            if (m_progress >= 1.0f && !m_fired_this_hold) {
                m_fired_this_hold = true;
                m_holding = false;
                m_confirm_flash = 1.0f;
                if (m_on_confirmed) m_on_confirmed();
            }
            mark_needs_paint();
        } else if (m_progress > 0.0f) {
            m_progress = std::max(0.0f, m_progress - dt_sec * 3.8f);
            mark_needs_paint();
        }
        if (m_confirm_flash > 0.0f) {
            m_confirm_flash = std::max(0.0f, m_confirm_flash - dt_sec * 1.1f);
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        if (m_shape == HoldConfirmShape::CircularRing) {
            m_size = m_constraints.constrain(Vec2{m_diameter, m_diameter});
            return;
        }
        Vec2 ts = TextMeasure::measure(m_text, 13.0f, FontWeight::Medium);
        float ideal_w = std::max(160.0f, ts.x + 36.0f);
        float w = m_constraints.has_bounded_width() ? std::min(m_constraints.max_width, std::max(m_constraints.min_width, ideal_w)) : ideal_w;
        float h = m_constraints.has_bounded_height() ? std::min(m_constraints.max_height, 36.0f) : 36.0f;
        m_size = m_constraints.constrain(Vec2{w, std::max(32.0f, h)});
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        if (m_shape == HoldConfirmShape::CircularRing) {
            float r = std::min(m_size.x, m_size.y) * 0.5f;
            if (std::hypot(position.x - r, position.y - r) > r) return false;
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        auto now = std::chrono::steady_clock::now();
        if (m_last_tick.time_since_epoch().count() != 0) {
            float dt = std::clamp(std::chrono::duration<float>(now - m_last_tick).count(), 0.0f, 0.05f);
            if (m_holding || m_progress > 0.0f || m_confirm_flash > 0.0f) {
                advance_by(dt);
            }
        }
        m_last_tick = now;

        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const Color base_accent = m_danger
            ? Color{0xD1 / 255.0f, 0x34 / 255.0f, 0x38 / 255.0f, 1.0f}
            : t.colors.primary;
        const Color ok_col = Color{0x10 / 255.0f, 0x89 / 255.0f, 0x3E / 255.0f, 1.0f};

        if (m_shape == HoldConfirmShape::CircularRing) {
            paint_circular(context, t, is_dark, base_accent, ok_col);
            return;
        }

        Color bg = is_dark
            ? Color{base_accent.r, base_accent.g, base_accent.b, m_hovered ? 0.22f : 0.14f}
            : Color{base_accent.r, base_accent.g, base_accent.b, m_hovered ? 0.16f : 0.10f};
        if (m_confirm_flash > 0.01f) {
            bg = Color{ok_col.r, ok_col.g, ok_col.b, 0.25f + 0.55f * m_confirm_flash};
        }

        context.draw_rect(bounds(), bg, 6.0f, 0.0f);

        if (m_progress > 0.002f && m_confirm_flash <= 0.01f) {
            context.push_clip(bounds(), 6.0f);
            context.draw_rect(Rect::from_xywh(0.0f, 0.0f, m_size.x * m_progress, m_size.y),
                              Color{base_accent.r, base_accent.g, base_accent.b, 0.90f}, 0.0f, 0.0f);
            context.pop_clip();
        }

        Color bd = (m_confirm_flash > 0.01f) ? ok_col : Color{base_accent.r, base_accent.g, base_accent.b, m_holding ? 1.0f : 0.55f};
        context.draw_rect(bounds(), bd, 6.0f, m_holding ? 1.5f : 1.0f);

        std::string label = m_text;
        if (m_confirm_flash > 0.01f) {
            label = m_confirmed_text;
        } else if (m_holding) {
            float rem = std::max(0.0f, m_hold_duration * (1.0f - m_progress));
            char buf[32];
            std::snprintf(buf, sizeof(buf), " (%.1fs)", rem);
            label = m_holding_text + buf;
        }

        Color text_c = (m_progress > 0.45f || m_confirm_flash > 0.2f)
            ? Color::white()
            : (m_danger ? (is_dark ? Color{1.0f, 0.58f, 0.58f, 1.0f} : Color{0xB0 / 255.0f, 0x1E / 255.0f, 0x24 / 255.0f, 1.0f})
                        : t.colors.text_primary);
        Vec2 ts = TextMeasure::measure(label, 12.5f, FontWeight::SemiBold);
        context.draw_text(label,
                          PointF{std::round((m_size.x - ts.x) * 0.5f), std::round((m_size.y - ts.y) * 0.5f)},
                          12.5f, text_c, FontWeight::SemiBold);
    }

private:
    void paint_circular(PaintContext& context, const Theme& t, bool is_dark, Color base_accent, Color ok_col) {
        constexpr float kPi = 3.14159265358979323846f;
        const float d = std::min(m_size.x, m_size.y);
        const float cx = m_size.x * 0.5f;
        const float cy = m_size.y * 0.5f;
        const float stroke = 3.5f;
        const float outer_r = d * 0.5f - 1.0f;
        const float ring_r = outer_r - stroke * 0.5f - 1.0f;

        Color fill_c = (m_confirm_flash > 0.01f)
            ? Color{ok_col.r, ok_col.g, ok_col.b, 0.25f + 0.65f * m_confirm_flash}
            : Color{base_accent.r, base_accent.g, base_accent.b, m_holding ? 0.24f : (m_hovered ? 0.18f : 0.11f)};
        Rect circle_r = Rect::from_xywh(cx - outer_r, cy - outer_r, outer_r * 2.0f, outer_r * 2.0f);
        context.draw_rect(circle_r, fill_c, outer_r, 0.0f);

        // 环形底槽
        Color track_c = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.14f} : Color{0.0f, 0.0f, 0.0f, 0.12f};
        Rect track_r = Rect::from_xywh(cx - ring_r, cy - ring_r, ring_r * 2.0f, ring_r * 2.0f);
        context.draw_rect(track_r, (m_confirm_flash > 0.01f) ? ok_col : track_c, ring_r, stroke);

        // 顺时针 360° 矢量进度弧
        if (m_progress > 0.002f && m_confirm_flash <= 0.01f) {
            float start_ang = -kPi * 0.5f;
            float span = 2.0f * kPi * m_progress;
            int segs = std::clamp(static_cast<int>(span * 14.0f), 4, 48);
            for (int i = 0; i < segs; ++i) {
                float a1 = start_ang + span * (static_cast<float>(i) / static_cast<float>(segs));
                float a2 = start_ang + span * (static_cast<float>(i + 1) / static_cast<float>(segs));
                context.draw_line(PointF{cx + ring_r * std::cos(a1), cy + ring_r * std::sin(a1)},
                                  PointF{cx + ring_r * std::cos(a2), cy + ring_r * std::sin(a2)},
                                  base_accent, stroke);
            }
        }

        // 中心文案 / 倒计时 / 完成勾
        std::string center_txt = m_text;
        if (m_confirm_flash > 0.01f) {
            center_txt = "✓";
        } else if (m_holding) {
            float rem = std::max(0.0f, m_hold_duration * (1.0f - m_progress));
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.1fs", rem);
            center_txt = buf;
        }
        Color tc = (m_confirm_flash > 0.2f)
            ? Color::white()
            : (m_danger ? (is_dark ? Color{1.0f, 0.60f, 0.60f, 1.0f} : Color{0xB0 / 255.0f, 0x1E / 255.0f, 0x24 / 255.0f, 1.0f})
                        : t.colors.text_primary);
        float fsz = (m_confirm_flash > 0.01f) ? 18.0f : 12.0f;
        Vec2 ts = TextMeasure::measure(center_txt, fsz, FontWeight::SemiBold);
        context.draw_text(center_txt,
                          PointF{std::round(cx - ts.x * 0.5f), std::round(cy - ts.y * 0.5f)},
                          fsz, tc, FontWeight::SemiBold);
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) { m_hovered = true; mark_needs_paint(); };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            if (m_holding) m_holding = false;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            m_holding = true;
            m_fired_this_hold = false;
            m_confirm_flash = 0.0f;
            m_last_tick = std::chrono::steady_clock::now();
            mark_needs_paint();
            ev.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& ev) {
            if (m_holding) {
                m_holding = false;
                mark_needs_paint();
            }
            ev.stop_propagation();
        };
    }

    std::string m_text;
    std::string m_holding_text;
    std::string m_confirmed_text;
    float m_hold_duration{1.5f};
    bool m_danger{true};
    HoldConfirmShape m_shape{HoldConfirmShape::PillBar};
    float m_diameter{64.0f};
    Fn<void()> m_on_confirmed;

    bool m_hovered{false};
    bool m_holding{false};
    bool m_fired_this_hold{false};
    float m_progress{0.0f};
    float m_confirm_flash{0.0f};
    std::chrono::steady_clock::time_point m_last_tick{};
};

struct HoldConfirmButton : BindableMixin<HoldConfirmButton, RenderHoldConfirmButton> {
    std::string text{"长按 1.5s 确认删除"};
    std::string holding_text{"继续按住确认..."};
    std::string confirmed_text{"✓ 已执行"};
    float hold_duration_sec{1.5f};
    bool danger{true};
    HoldConfirmShape shape{HoldConfirmShape::PillBar};
    float diameter{64.0f};
    Fn<void()> on_confirmed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class HoldConfirmButtonWidget : public Widget {
public:
    explicit HoldConfirmButtonWidget(const HoldConfirmButton& desc) : m_desc(desc) {}
    explicit HoldConfirmButtonWidget(HoldConfirmButton&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderHoldConfirmButton>(
            m_desc.text, m_desc.holding_text, m_desc.confirmed_text,
            m_desc.hold_duration_sec, m_desc.danger, m_desc.shape, m_desc.diameter, m_desc.on_confirmed);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderHoldConfirmButton>(node)) {
            n->set_on_confirmed(m_desc.on_confirmed);
        }
    }
private:
    HoldConfirmButton m_desc;
};

inline HoldConfirmButton::operator WidgetPtr() const& { return make_widget<HoldConfirmButtonWidget>(*this); }
inline HoldConfirmButton::operator WidgetPtr() && { return make_widget<HoldConfirmButtonWidget>(std::move(*this)); }

} // namespace ktui
