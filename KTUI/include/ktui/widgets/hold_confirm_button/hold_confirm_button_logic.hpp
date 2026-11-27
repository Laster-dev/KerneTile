#pragma once

#include <ktui/core/strings.hpp>

#include "hold_confirm_button_data.hpp"
#include "hold_confirm_button_style.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace ktui {

// 防误触“长按确认”按钮 (WinUI 3 / Fluent 2 风格 Hold-to-Confirm Button)
// - 采用 WinUI 3 ControlFillColorDefault 中性磨砂表面 + 4px ControlCornerRadius + 1px ControlElevationBorder
// - 长按时呈现克制柔和的内部着色洗层 + 底部 2.5px 精准进度指示条（或圆形 360° 平滑矢量弧）
// - 完成瞬间柔和过渡至 Fluent 2 Success 态并绘制矢量对勾，松手立即平滑回弹
class RenderHoldConfirmButton : public RenderBox {
public:
    RenderHoldConfirmButton(std::string text = FrameworkStrings::current().hold_confirm_default,
                            std::string holding_text = FrameworkStrings::current().hold_confirm_holding,
                            std::string confirmed_text = FrameworkStrings::current().hold_confirm_done,
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
          m_diameter(std::max(44.0f, diameter)),
          m_on_confirmed(std::move(on_confirmed)) {
        is_focusable = true;
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    void set_on_confirmed(Fn<void()> cb) { m_on_confirmed = std::move(cb); }
    void set_pointer_reveal_enabled(std::optional<bool> enabled) noexcept {
        if (m_enable_pointer_reveal != enabled) {
            m_enable_pointer_reveal = enabled;
            if (!ThemeContext::current().resolve_pointer_reveal(m_enable_pointer_reveal)) m_reveal_active = false;
            mark_needs_paint();
        }
    }
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
            m_progress = std::max(0.0f, m_progress - dt_sec * 4.2f);
            mark_needs_paint();
        }
        if (m_confirm_flash > 0.0f) {
            m_confirm_flash = std::max(0.0f, m_confirm_flash - dt_sec * 0.9f);
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        if (m_shape == HoldConfirmShape::CircularRing) {
            m_size = m_constraints.constrain(Vec2{m_diameter, m_diameter});
            return;
        }
        Vec2 ts = TextMeasure::measure(m_text, 13.0f, FontWeight::Regular);
        float ideal_w = std::max(148.0f, ts.x + 36.0f);
        float w = m_constraints.has_bounded_width() ? std::min(m_constraints.max_width, std::max(m_constraints.min_width, ideal_w)) : ideal_w;
        float h = m_constraints.has_bounded_height() ? std::min(m_constraints.max_height, 32.0f) : 32.0f;
        m_size = m_constraints.constrain(Vec2{w, std::max(30.0f, h)});
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

    bool hit_test_touch(HitTestResult& result, PointF position) override {
        if (m_shape == HoldConfirmShape::CircularRing) {
            float r = std::max(std::min(m_size.x, m_size.y) * 0.5f, touch::kMinTargetSize * 0.5f);
            float cx = m_size.x * 0.5f, cy = m_size.y * 0.5f;
            if (std::hypot(position.x - cx, position.y - cy) > r) return false;
            result.add(this, position);
            return true;
        }
        if (!touch::expand_hit_rect_for_touch(bounds(), touch::kMinTargetSize).contains(position)) return false;
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
        const auto resolved_style = HoldConfirmButtonStyleResolver::resolve(t);
        const bool is_dark = t.is_dark;
        // WinUI 3 官方语义色：Critical (#FF99A4 文本 / #C42B1C 填充) 与 Success (#6CCB5F / #0F7B0F)
        const Color danger_accent = is_dark ? Color{0xE8 / 255.0f, 0x4C / 255.0f, 0x52 / 255.0f, 1.0f}
                                            : Color{0xC4 / 255.0f, 0x2B / 255.0f, 0x1C / 255.0f, 1.0f};
        const Color danger_text   = is_dark ? Color{1.0f, 0x99 / 255.0f, 0xA4 / 255.0f, 1.0f}
                                            : Color{0xC4 / 255.0f, 0x2B / 255.0f, 0x1C / 255.0f, 1.0f};
        const Color ok_col        = is_dark ? Color{0x6C / 255.0f, 0xCB / 255.0f, 0x5F / 255.0f, 1.0f}
                                            : Color{0x0F / 255.0f, 0x7B / 255.0f, 0x0F / 255.0f, 1.0f};
        const Color accent        = m_danger ? danger_accent : resolved_style.accent;

        if (m_shape == HoldConfirmShape::CircularRing) {
            paint_circular(context, t, is_dark, accent, danger_text, ok_col);
            return;
        }

        // 1. WinUI 3 标准 ControlFillColorDefault 表面（4px ControlCornerRadius）
        constexpr float kRad = 4.0f;
        Color surface_bg = is_dark
            ? (m_holding ? Color{1, 1, 1, 0.0326f} : (m_hovered ? Color{1, 1, 1, 0.0837f} : Color{1, 1, 1, 0.0605f}))
            : (m_holding ? Color{0.96f, 0.96f, 0.96f, 0.45f} : (m_hovered ? Color{0.976f, 0.976f, 0.976f, 0.65f} : Color{1, 1, 1, 0.75f}));

        context.draw_rect(bounds(), surface_bg, kRad, 0.0f);

        // 2. 内部柔和进度洗色 + 底部 2.5px Fluent 进度指示线
        context.push_clip(bounds(), kRad);
        if (m_confirm_flash > 0.01f) {
            context.draw_rect(bounds(), Color{ok_col.r, ok_col.g, ok_col.b, (is_dark ? 0.16f : 0.12f) * m_confirm_flash}, 0.0f, 0.0f);
            context.draw_rect(Rect::from_xywh(0.0f, m_size.y - 2.5f, m_size.x, 2.5f),
                              Color{ok_col.r, ok_col.g, ok_col.b, std::min(1.0f, m_confirm_flash * 1.4f)}, 1.25f, 0.0f);
        } else if (m_progress > 0.002f) {
            float fill_w = m_size.x * m_progress;
            context.draw_rect(Rect::from_xywh(0.0f, 0.0f, fill_w, m_size.y),
                              Color{accent.r, accent.g, accent.b, is_dark ? 0.20f : 0.12f}, 0.0f, 0.0f);
            context.draw_rect(Rect::from_xywh(0.0f, m_size.y - 2.5f, fill_w, 2.5f), accent, 1.25f, 0.0f);
        }

        // 3. WinUI 3 ControlElevationBorder（深色顶部微光 / 浅色底部暗边）
        if (!m_holding) {
            if (is_dark) {
                context.draw_rect(Rect::from_xywh(1.0f, 0.0f, std::max(0.0f, m_size.x - 2.0f), 1.0f),
                                  Color{1, 1, 1, 0.095f}, 0.0f, 0.0f);
            } else {
                context.draw_rect(Rect::from_xywh(1.0f, m_size.y - 1.0f, std::max(0.0f, m_size.x - 2.0f), 1.0f),
                                  Color{0, 0, 0, 0.15f}, 0.0f, 0.0f);
            }
        }
        context.pop_clip();

        // 4. 精致 1px 外轮廓描边 + Fluent Pointer Reveal
        Color bd = (m_confirm_flash > 0.01f)
            ? Color{ok_col.r, ok_col.g, ok_col.b, 0.65f}
            : (m_holding
                ? Color{accent.r, accent.g, accent.b, 0.60f}
                : (is_dark ? Color{1, 1, 1, 0.075f} : Color{0, 0, 0, 0.075f}));
        context.draw_rect(bounds(), bd, kRad, 1.0f);
        if (t.resolve_pointer_reveal(m_enable_pointer_reveal) && m_reveal_active) {
            const Color surf = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.10f} : Color{0.0f, 0.0f, 0.0f, 0.055f};
            const Color edge = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.32f} : Color{0.0f, 0.0f, 0.0f, 0.22f};
            context.draw_pointer_reveal(bounds(), m_reveal_pos, 68.0f, surf, edge, kRad, 1.0f);
        }

        // 5. 文案与精致矢量图标（危险点 / 完成对勾）
        std::string label = m_text;
        if (m_confirm_flash > 0.01f) {
            label = m_confirmed_text;
        } else if (m_holding) {
            float rem = std::max(0.0f, m_hold_duration * (1.0f - m_progress));
            char buf[24];
            std::snprintf(buf, sizeof(buf), " %.1fs", rem);
            label = m_holding_text + buf;
        }

        Color text_c = (m_confirm_flash > 0.01f) ? ok_col
                     : (m_danger ? danger_text : resolved_style.text_primary);
        Vec2 ts = TextMeasure::measure(label, 13.0f, FontWeight::Regular);
        const float icon_w = (m_confirm_flash > 0.01f || m_danger) ? 14.0f : 0.0f;
        const float total_w = icon_w + ts.x;
        const float start_x = std::round((m_size.x - total_w) * 0.5f);
        const float cy = m_size.y * 0.5f;

        if (m_confirm_flash > 0.01f) {
            context.draw_line(PointF{start_x + 1.0f, cy + 0.5f}, PointF{start_x + 4.0f, cy + 3.5f}, ok_col, 1.5f);
            context.draw_line(PointF{start_x + 4.0f, cy + 3.5f}, PointF{start_x + 9.5f, cy - 3.0f}, ok_col, 1.5f);
        } else if (m_danger) {
            // 6px Fluent 危险语义指示圆点
            context.draw_rect(Rect::from_xywh(start_x + 2.0f, std::round(cy - 3.0f), 6.0f, 6.0f), accent, 3.0f, 0.0f);
        }

        context.draw_text(label,
                          PointF{start_x + icon_w, std::round((m_size.y - ts.y) * 0.5f)},
                          13.0f, text_c, FontWeight::Regular);
    }

private:
    void paint_circular(PaintContext& context, const Theme& t, bool is_dark, Color accent, Color danger_text, Color ok_col) {
        constexpr float kPi = 3.14159265358979323846f;
        const float d = std::min(m_size.x, m_size.y);
        const float cx = m_size.x * 0.5f;
        const float cy = m_size.y * 0.5f;
        const float stroke = 2.6f;
        const float outer_r = d * 0.5f - 1.5f;
        const float ring_r = outer_r - 2.0f;
        Rect circle_r = Rect::from_xywh(cx - outer_r, cy - outer_r, outer_r * 2.0f, outer_r * 2.0f);

        // 1. 精致 Fluent 2 中性圆形按钮表面 + 轻微按压反馈
        Color fill_c = is_dark
            ? (m_holding ? Color{1, 1, 1, 0.035f} : (m_hovered ? Color{1, 1, 1, 0.085f} : Color{1, 1, 1, 0.060f}))
            : (m_holding ? Color{0.95f, 0.95f, 0.95f, 0.65f} : (m_hovered ? Color{0.98f, 0.98f, 0.98f, 0.90f} : Color{1, 1, 1, 0.82f}));
        context.draw_rect(circle_r, fill_c, outer_r, 0.0f);

        if (m_confirm_flash > 0.01f) {
            context.draw_rect(circle_r, Color{ok_col.r, ok_col.g, ok_col.b, (is_dark ? 0.18f : 0.12f) * m_confirm_flash}, outer_r, 0.0f);
        } else if (m_progress > 0.002f) {
            context.draw_rect(circle_r, Color{accent.r, accent.g, accent.b, (is_dark ? 0.14f : 0.09f) * m_progress}, outer_r, 0.0f);
        }

        // 2. 细腻 1px 外边框 + 内嵌纤细圆环底槽
        Color track_c = (m_confirm_flash > 0.01f)
            ? ok_col
            : (is_dark ? Color{1.0f, 1.0f, 1.0f, 0.10f} : Color{0.0f, 0.0f, 0.0f, 0.09f});
        Rect track_r = Rect::from_xywh(cx - ring_r, cy - ring_r, ring_r * 2.0f, ring_r * 2.0f);
        context.draw_rect(track_r, track_c, ring_r, (m_confirm_flash > 0.01f) ? stroke : 1.2f);
        if (t.resolve_pointer_reveal(m_enable_pointer_reveal) && m_reveal_active) {
            const Color surf = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.10f} : Color{0.0f, 0.0f, 0.0f, 0.055f};
            const Color edge = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.34f} : Color{0.0f, 0.0f, 0.0f, 0.22f};
            context.draw_pointer_reveal(circle_r, m_reveal_pos, std::max(48.0f, d * 0.9f), surf, edge, outer_r, 1.0f);
        }

        // 3. 高密度平滑 360° 矢量进度弧（64 段无折线锯齿）
        if (m_progress > 0.002f && m_confirm_flash <= 0.01f) {
            float start_ang = -kPi * 0.5f;
            float span = 2.0f * kPi * m_progress;
            int segs = std::clamp(static_cast<int>(span * 18.0f), 6, 64);
            for (int i = 0; i < segs; ++i) {
                float a1 = start_ang + span * (static_cast<float>(i) / static_cast<float>(segs));
                float a2 = start_ang + span * (static_cast<float>(i + 1) / static_cast<float>(segs));
                context.draw_line(PointF{cx + ring_r * std::cos(a1), cy + ring_r * std::sin(a1)},
                                  PointF{cx + ring_r * std::cos(a2), cy + ring_r * std::sin(a2)},
                                  accent, stroke);
            }
        }

        // 4. 中心内容：完成绘制干净矢量对勾，按住显示倒计时，空闲显示标签
        if (m_confirm_flash > 0.01f) {
            context.draw_line(PointF{cx - 6.0f, cy + 0.5f}, PointF{cx - 1.5f, cy + 5.0f}, ok_col, 2.0f);
            context.draw_line(PointF{cx - 1.5f, cy + 5.0f}, PointF{cx + 6.5f, cy - 4.5f}, ok_col, 2.0f);
            return;
        }

        std::string center_txt = m_text;
        if (m_holding) {
            float rem = std::max(0.0f, m_hold_duration * (1.0f - m_progress));
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.1fs", rem);
            center_txt = buf;
        }
        Color tc = m_danger ? danger_text : t.colors.text_primary;
        Vec2 ts = TextMeasure::measure(center_txt, 12.5f, FontWeight::Medium);
        context.draw_text(center_txt,
                          PointF{std::round(cx - ts.x * 0.5f), std::round(cy - ts.y * 0.5f)},
                          12.5f, tc, FontWeight::Medium);
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) { m_hovered = true; mark_needs_paint(); };
        on_pointer_move = [this](PointerEvent& ev) {
            if (m_holding && ev.device_kind == PointerDeviceKind::Touch) {
                Rect tol{ -touch::kTouchCancelPadPx, -touch::kTouchCancelPadPx,
                          m_size.x + touch::kTouchCancelPadPx, m_size.y + touch::kTouchCancelPadPx };
                if (!tol.contains(ev.local_position)) {
                    m_holding = false;
                    mark_needs_paint();
                }
                return;
            }
            if (ev.type == PlatformEventType::PointerMove &&
                ev.device_kind != PointerDeviceKind::Touch &&
                ThemeContext::current().resolve_pointer_reveal(m_enable_pointer_reveal)) {
                m_reveal_pos = ev.local_position;
                m_reveal_active = true;
                mark_needs_paint();
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_hovered = false;
            m_reveal_active = false;
            m_holding = false;
            mark_needs_paint();
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_reveal_active = false;
            if (m_holding) m_holding = false;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            m_holding = true;
            m_fired_this_hold = false;
            m_confirm_flash = 0.0f;
            m_last_tick = std::chrono::steady_clock::now();
            if (ev.device_kind == PointerDeviceKind::Touch) {
                ev.gesture_capture_target = this;
            }
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
    std::optional<bool> m_enable_pointer_reveal{std::nullopt};
    bool m_reveal_active{false};
    PointF m_reveal_pos{0.0f, 0.0f};
    float m_progress{0.0f};
    float m_confirm_flash{0.0f};
    std::chrono::steady_clock::time_point m_last_tick{};
};

struct HoldConfirmButton : BindableMixin<HoldConfirmButton, RenderHoldConfirmButton> {
    std::string text{FrameworkStrings::current().hold_confirm_default};
    std::string holding_text{FrameworkStrings::current().hold_confirm_holding};
    std::string confirmed_text{FrameworkStrings::current().hold_confirm_done};
    float hold_duration_sec{1.5f};
    bool danger{true};
    HoldConfirmShape shape{HoldConfirmShape::PillBar};
    float diameter{64.0f};
    std::optional<bool> enable_pointer_reveal{std::nullopt};
    Fn<void()> on_confirmed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class HoldConfirmButtonWidget : public Widget {
public:
    explicit HoldConfirmButtonWidget(const HoldConfirmButton& desc) : m_desc(desc) {}
    explicit HoldConfirmButtonWidget(HoldConfirmButton&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderHoldConfirmButton>(
            m_desc.text, m_desc.holding_text, m_desc.confirmed_text,
            m_desc.hold_duration_sec, m_desc.danger, m_desc.shape, m_desc.diameter, m_desc.on_confirmed);
        node->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
        return node;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderHoldConfirmButton>(node)) {
            n->set_on_confirmed(m_desc.on_confirmed);
            n->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
        }
    }
private:
    HoldConfirmButton m_desc;
};

inline HoldConfirmButton::operator WidgetPtr() const& { return make_widget<HoldConfirmButtonWidget>(*this); }
inline HoldConfirmButton::operator WidgetPtr() && { return make_widget<HoldConfirmButtonWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
