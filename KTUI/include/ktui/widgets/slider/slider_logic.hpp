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
    [[nodiscard]] float step() const noexcept { return m_data.step; }
    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    [[nodiscard]] SliderOrientation orientation() const noexcept { return m_data.orientation; }
    [[nodiscard]] bool range_mode() const noexcept { return m_data.range_mode; }
    [[nodiscard]] float range_start() const noexcept { return m_data.range_start; }
    [[nodiscard]] float range_end() const noexcept { return m_data.range_end; }
    [[nodiscard]] const SliderData& data() const noexcept { return m_data; }
    [[nodiscard]] const SliderInteractiveState& state() const noexcept { return m_state; }

    [[nodiscard]] float normalized_value() const noexcept {
        return normalize_val(m_data.value);
    }

    void set_value(float val, bool notify = true) {
        float clamped = quantize_val(val);
        if (std::abs(m_data.value - clamped) > 0.0001f) {
            m_data.value = clamped;
            if (notify && m_data.on_change) {
                m_data.on_change(m_data.value);
            }
            mark_needs_paint();
        }
    }

    void set_range_values(float start_val, float end_val, bool notify = true) {
        float s = quantize_val(start_val);
        float e = quantize_val(end_val);
        if (s > e) std::swap(s, e);
        if (std::abs(m_data.range_start - s) > 0.0001f || std::abs(m_data.range_end - e) > 0.0001f) {
            m_data.range_start = s;
            m_data.range_end = e;
            if (notify && m_data.on_range_changed) {
                m_data.on_range_changed(m_data.range_start, m_data.range_end);
            }
            mark_needs_paint();
        }
    }

    void set_range(float min_val, float max_val, float step_val = 0.0f) {
        m_data.min_value = min_val;
        m_data.max_value = std::max(min_val, max_val);
        m_data.step = std::max(0.0f, step_val);
        set_value(m_data.value, false);
        set_range_values(m_data.range_start, m_data.range_end, false);
        mark_needs_paint();
    }

    void set_extended_props(SliderOrientation orientation,
                            float height,
                            float tick_frequency,
                            bool show_ticks,
                            bool range_mode,
                            float range_start,
                            float range_end,
                            Fn<void(float, float)> on_range_changed) {
        bool layout_dirty = (m_data.orientation != orientation) ||
                            (std::abs(m_data.height - height) > 0.001f) ||
                            (m_data.show_ticks != show_ticks) ||
                            (std::abs(m_data.tick_frequency - tick_frequency) > 0.001f);
        m_data.orientation = orientation;
        if (height > 0.0f) m_data.height = height;
        m_data.tick_frequency = std::max(0.0f, tick_frequency);
        m_data.show_ticks = show_ticks;
        m_data.range_mode = range_mode;
        m_data.on_range_changed = std::move(on_range_changed);
        set_range_values(range_start, range_end, false);
        if (layout_dirty) mark_needs_layout();
        else mark_needs_paint();
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

    void set_on_change(Fn<void(float)> cb) { m_data.on_change = std::move(cb); }
    void set_on_change_end(Fn<void(float)> cb) { m_data.on_change_end = std::move(cb); }

    void perform_layout() override {
        const bool has_ticks = (m_data.tick_frequency > 0.0f || m_data.show_ticks);
        if (m_data.orientation == SliderOrientation::Vertical) {
            float pref_w = has_ticks ? 40.0f : 32.0f;
            float pref_h = (m_data.height > 40.0f) ? m_data.height : 180.0f;
            set_size(Vec2{m_constraints.constrain_width(pref_w), m_constraints.constrain_height(pref_h)});
        } else {
            float pref_w = m_data.width > 0.0f ? m_data.width : 180.0f;
            float pref_h = has_ticks ? std::max(38.0f, m_data.height) : (m_data.height > 0.0f ? m_data.height : 32.0f);
            set_size(Vec2{m_constraints.constrain_width(pref_w), m_constraints.constrain_height(pref_h)});
        }
    }

    void paint(PaintContext& ctx) override {
        update_animations();

        const Theme& theme = ThemeContext::current();
        const bool vertical = (m_data.orientation == SliderOrientation::Vertical);
        ResolvedSliderStyle style = SliderStyleResolver::resolve(
            m_data, m_state, theme, m_hover_progress, m_press_progress);

        const float thumb_r = style.thumb_radius;
        const float span = m_data.max_value - m_data.min_value;
        const bool smooth_drag = m_state.is_dragging && (m_data.step <= 0.0f || (span > 0.0f && m_data.step <= span * 0.02f));

        auto draw_thumb = [&](float cx, float cy, float hover_p, float press_p) {
            ResolvedSliderStyle ts = SliderStyleResolver::resolve(m_data, m_state, theme, hover_p, press_p);
            float cur_r = thumb_r * ts.thumb_scale;
            Rect thumb_rect = Rect::from_xywh(cx - cur_r, cy - cur_r, cur_r * 2.0f, cur_r * 2.0f);

            if (!m_state.is_disabled && ts.shadow_color.a > 0.001f) {
                Color shadow_outer = ts.shadow_color;
                shadow_outer.a *= 0.45f;
                float sr1 = cur_r + 1.5f;
                ctx.draw_rect(Rect::from_xywh(cx - sr1, cy - sr1 + 1.5f, sr1 * 2.0f, sr1 * 2.0f),
                              shadow_outer, sr1, 0.0f);

                Color shadow_inner = ts.shadow_color;
                shadow_inner.a *= 0.80f;
                float sr2 = cur_r + 0.5f;
                ctx.draw_rect(Rect::from_xywh(cx - sr2, cy - sr2 + 0.8f, sr2 * 2.0f, sr2 * 2.0f),
                              shadow_inner, sr2, 0.0f);
            }

            ctx.draw_rect(thumb_rect, ts.thumb_fill, cur_r, 0.0f);
            if (ts.border_width > 0.0f && ts.thumb_border.a > 0.001f) {
                ctx.draw_rect(thumb_rect, ts.thumb_border, cur_r, ts.border_width);
            }
            if (ts.core_radius > 0.1f) {
                Rect core_rect = Rect::from_xywh(cx - ts.core_radius, cy - ts.core_radius,
                                                 ts.core_radius * 2.0f, ts.core_radius * 2.0f);
                ctx.draw_rect(core_rect, ts.thumb_core, ts.core_radius, 0.0f);
            }
        };

        // 计算刻度步长
        float tick_step = 0.0f;
        if (m_data.tick_frequency > 0.0f) {
            tick_step = m_data.tick_frequency;
        } else if (m_data.show_ticks && m_data.step > 0.0f) {
            tick_step = m_data.step;
        } else if (m_data.step > 0.0f) {
            int sc = static_cast<int>(std::round(span / m_data.step));
            if (sc >= 2 && sc <= 24) tick_step = m_data.step;
        }

        if (!vertical) {
            const float track_x = thumb_r;
            const float track_w = std::max(0.0f, size().x - thumb_r * 2.0f);
            const float thumb_cy = (tick_step > 0.0f) ? std::max(thumb_r + 1.0f, size().y * 0.5f - 3.0f) : (size().y * 0.5f);
            const float track_y = thumb_cy - style.track_height * 0.5f;

            // 1. 未激活底槽 (可选凹槽解析高斯内阴影)
            const Rect track_rect = Rect::from_xywh(track_x, track_y, track_w, style.track_height);
            ctx.draw_rect(track_rect, style.track_inactive, style.track_height * 0.5f, 0.0f);
            if (m_inset_shadow) {
                const Color sh = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.45f} : Color{0.0f, 0.04f, 0.12f, 0.22f};
                ctx.draw_inset_shadow(track_rect, sh, style.track_height * 0.5f, 2.5f, 0.3f, Vec2{0.0f, 1.0f});
            }

            // 2. 激活高亮轨
            if (m_data.range_mode) {
                float ns = (smooth_drag && m_active_thumb == 0) ? m_drag_norm : normalize_val(m_data.range_start);
                float ne = (smooth_drag && m_active_thumb == 1) ? m_drag_norm : normalize_val(m_data.range_end);
                if (ns > ne) std::swap(ns, ne);
                float ax = track_x + track_w * ns;
                float aw = std::max(0.0f, track_w * (ne - ns));
                if (aw > 0.5f) {
                    ctx.draw_rect(Rect::from_xywh(ax, track_y, aw, style.track_height),
                                  style.track_active, style.track_height * 0.5f, 0.0f);
                }

                // 3. 刻度线
                if (tick_step > 0.0f && span > 0.0f) {
                    int tc = static_cast<int>(std::round(span / tick_step));
                    if (tc >= 2 && tc <= 40) {
                        float ty = track_y + style.track_height + 6.0f;
                        for (int i = 0; i <= tc; ++i) {
                            float tx = track_x + track_w * (static_cast<float>(i) / static_cast<float>(tc));
                            ctx.draw_rect(Rect::from_xywh(tx - 0.5f, ty, 1.0f, 5.0f), style.tick_color, 0.5f, 0.0f);
                        }
                    }
                }

                // 4. 双手柄
                float hp0 = (m_hovered_thumb == 0 || (m_state.is_dragging && m_active_thumb == 0)) ? m_hover_progress : 0.0f;
                float pp0 = (m_state.is_dragging && m_active_thumb == 0) ? m_press_progress : 0.0f;
                float hp1 = (m_hovered_thumb == 1 || (m_state.is_dragging && m_active_thumb == 1)) ? m_hover_progress : 0.0f;
                float pp1 = (m_state.is_dragging && m_active_thumb == 1) ? m_press_progress : 0.0f;
                draw_thumb(track_x + track_w * ns, thumb_cy, hp0, pp0);
                draw_thumb(track_x + track_w * ne, thumb_cy, hp1, pp1);
            } else {
                const float norm = smooth_drag ? m_drag_norm : normalized_value();
                if (norm > 0.001f) {
                    ctx.draw_rect(Rect::from_xywh(track_x, track_y, track_w * norm, style.track_height),
                                  style.track_active, style.track_height * 0.5f, 0.0f);
                }

                if (tick_step > 0.0f && span > 0.0f) {
                    int tc = static_cast<int>(std::round(span / tick_step));
                    if (tc >= 2 && tc <= 40) {
                        float ty = track_y + style.track_height + 6.0f;
                        for (int i = 0; i <= tc; ++i) {
                            float tx = track_x + track_w * (static_cast<float>(i) / static_cast<float>(tc));
                            ctx.draw_rect(Rect::from_xywh(tx - 0.5f, ty, 1.0f, 5.0f), style.tick_color, 0.5f, 0.0f);
                        }
                    }
                }

                draw_thumb(track_x + track_w * norm, thumb_cy, m_hover_progress, m_press_progress);
            }
        } else {
            // 竖向滑块 (WinUI 3 Vertical Slider: 底部为 min_value，顶部为 max_value)
            const float track_y = thumb_r;
            const float track_h = std::max(0.0f, size().y - thumb_r * 2.0f);
            const float thumb_cx = (tick_step > 0.0f) ? std::max(thumb_r + 1.0f, size().x * 0.5f - 3.0f) : (size().x * 0.5f);
            const float track_x = thumb_cx - style.track_height * 0.5f;

            const Rect vtrack_rect = Rect::from_xywh(track_x, track_y, style.track_height, track_h);
            ctx.draw_rect(vtrack_rect, style.track_inactive, style.track_height * 0.5f, 0.0f);
            if (m_inset_shadow) {
                const Color sh = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.45f} : Color{0.0f, 0.04f, 0.12f, 0.22f};
                ctx.draw_inset_shadow(vtrack_rect, sh, style.track_height * 0.5f, 2.5f, 0.3f, Vec2{1.0f, 0.0f});
            }

            if (m_data.range_mode) {
                float ns = (smooth_drag && m_active_thumb == 0) ? m_drag_norm : normalize_val(m_data.range_start);
                float ne = (smooth_drag && m_active_thumb == 1) ? m_drag_norm : normalize_val(m_data.range_end);
                if (ns > ne) std::swap(ns, ne);
                float y_top = track_y + track_h * (1.0f - ne);
                float y_bot = track_y + track_h * (1.0f - ns);
                if (y_bot - y_top > 0.5f) {
                    ctx.draw_rect(Rect::from_xywh(track_x, y_top, style.track_height, y_bot - y_top),
                                  style.track_active, style.track_height * 0.5f, 0.0f);
                }

                if (tick_step > 0.0f && span > 0.0f) {
                    int tc = static_cast<int>(std::round(span / tick_step));
                    if (tc >= 2 && tc <= 40) {
                        float tx = track_x + style.track_height + 6.0f;
                        for (int i = 0; i <= tc; ++i) {
                            float ty = track_y + track_h * (static_cast<float>(i) / static_cast<float>(tc));
                            ctx.draw_rect(Rect::from_xywh(tx, ty - 0.5f, 5.0f, 1.0f), style.tick_color, 0.5f, 0.0f);
                        }
                    }
                }

                float hp0 = (m_hovered_thumb == 0 || (m_state.is_dragging && m_active_thumb == 0)) ? m_hover_progress : 0.0f;
                float pp0 = (m_state.is_dragging && m_active_thumb == 0) ? m_press_progress : 0.0f;
                float hp1 = (m_hovered_thumb == 1 || (m_state.is_dragging && m_active_thumb == 1)) ? m_hover_progress : 0.0f;
                float pp1 = (m_state.is_dragging && m_active_thumb == 1) ? m_press_progress : 0.0f;
                draw_thumb(thumb_cx, y_bot, hp0, pp0);
                draw_thumb(thumb_cx, y_top, hp1, pp1);
            } else {
                const float norm = smooth_drag ? m_drag_norm : normalized_value();
                float active_h = track_h * norm;
                float thumb_cy = track_y + track_h * (1.0f - norm);
                if (active_h > 0.5f) {
                    ctx.draw_rect(Rect::from_xywh(track_x, thumb_cy, style.track_height, active_h),
                                  style.track_active, style.track_height * 0.5f, 0.0f);
                }

                if (tick_step > 0.0f && span > 0.0f) {
                    int tc = static_cast<int>(std::round(span / tick_step));
                    if (tc >= 2 && tc <= 40) {
                        float tx = track_x + style.track_height + 6.0f;
                        for (int i = 0; i <= tc; ++i) {
                            float ty = track_y + track_h * (static_cast<float>(i) / static_cast<float>(tc));
                            ctx.draw_rect(Rect::from_xywh(tx, ty - 0.5f, 5.0f, 1.0f), style.tick_color, 0.5f, 0.0f);
                        }
                    }
                }

                draw_thumb(thumb_cx, thumb_cy, m_hover_progress, m_press_progress);
            }
        }
    }

private:
    [[nodiscard]] float normalize_val(float v) const noexcept {
        float span = m_data.max_value - m_data.min_value;
        if (span <= 0.0001f) return 0.0f;
        return std::clamp((v - m_data.min_value) / span, 0.0f, 1.0f);
    }

    [[nodiscard]] float quantize_val(float val) const noexcept {
        float clamped = std::clamp(val, m_data.min_value, m_data.max_value);
        if (m_data.step > 0.0f) {
            float steps = std::round((clamped - m_data.min_value) / m_data.step);
            clamped = std::clamp(m_data.min_value + steps * m_data.step, m_data.min_value, m_data.max_value);
        }
        return clamped;
    }

    [[nodiscard]] float pos_to_norm(Vec2 local_pos) const noexcept {
        float thumb_r = m_data.thumb_radius > 0.0f ? m_data.thumb_radius : 10.0f;
        if (m_data.orientation == SliderOrientation::Vertical) {
            float track_h = std::max(1.0f, size().y - thumb_r * 2.0f);
            return std::clamp(1.0f - (local_pos.y - thumb_r) / track_h, 0.0f, 1.0f);
        }
        float track_w = std::max(1.0f, size().x - thumb_r * 2.0f);
        return std::clamp((local_pos.x - thumb_r) / track_w, 0.0f, 1.0f);
    }

    int pick_closest_thumb(float norm) const noexcept {
        float ns = normalize_val(m_data.range_start);
        float ne = normalize_val(m_data.range_end);
        float ds = std::abs(norm - ns);
        float de = std::abs(norm - ne);
        if (std::abs(ds - de) < 0.001f) return (norm >= ne) ? 1 : 0;
        return (ds < de) ? 0 : 1;
    }

    void setup_behavior() {
        // 移动端触控按下滑块时立即锁定手势捕获，防止拖拽过程中轻微纵向偏移被外层 ScrollView 抢占
        on_pointer_preview = [this](PointerEvent& e) {
            if (m_state.is_disabled || e.device_kind != PointerDeviceKind::Touch) return;
            if (e.type == PlatformEventType::PointerDown) {
                m_state.is_pressed = true;
                m_state.is_dragging = true;
                m_last_anim_time = std::chrono::steady_clock::now();
                if (m_data.range_mode) {
                    m_active_thumb = pick_closest_thumb(pos_to_norm(e.local_position));
                }
                update_value_from_pos(e.local_position);
                e.gesture_capture_target = this;
                e.stop_propagation();
                mark_needs_paint();
            }
        };

        on_pointer_enter = [this](PointerEvent& e) {
            m_state.is_hovered = true;
            if (m_data.range_mode) m_hovered_thumb = pick_closest_thumb(pos_to_norm(e.local_position));
            m_last_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_state.is_hovered = false;
            m_hovered_thumb = -1;
            m_last_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        };

        on_pointer_down = [this](PointerEvent& e) {
            if (m_state.is_disabled) return;
            m_state.is_pressed = true;
            m_state.is_dragging = true;
            m_last_anim_time = std::chrono::steady_clock::now();
            if (m_data.range_mode) {
                m_active_thumb = pick_closest_thumb(pos_to_norm(e.local_position));
                m_hovered_thumb = m_active_thumb;
            }
            update_value_from_pos(e.local_position);
            e.stop_propagation();
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (m_state.is_disabled) return;
            if (!m_state.is_dragging) {
                if (m_data.range_mode) {
                    int ht = pick_closest_thumb(pos_to_norm(e.local_position));
                    if (ht != m_hovered_thumb) {
                        m_hovered_thumb = ht;
                        mark_needs_paint();
                    }
                }
                return;
            }
            update_value_from_pos(e.local_position);
            e.stop_propagation();
            mark_needs_paint();
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (!m_state.is_dragging && !m_state.is_pressed) return;
            m_state.is_pressed = false;
            m_state.is_dragging = false;
            m_last_anim_time = std::chrono::steady_clock::now();
            if (!m_data.range_mode && m_data.on_change_end) {
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

    void update_value_from_pos(Vec2 local_pos) {
        float norm = pos_to_norm(local_pos);
        m_drag_norm = norm;
        float new_val = m_data.min_value + norm * (m_data.max_value - m_data.min_value);
        if (m_data.range_mode) {
            float q = quantize_val(new_val);
            if (m_active_thumb == 0) {
                if (q > m_data.range_end) q = m_data.range_end;
                m_drag_norm = std::min(norm, normalize_val(m_data.range_end));
                set_range_values(q, m_data.range_end, true);
            } else {
                if (q < m_data.range_start) q = m_data.range_start;
                m_drag_norm = std::max(norm, normalize_val(m_data.range_start));
                set_range_values(m_data.range_start, q, true);
            }
        } else {
            set_value(new_val, true);
        }
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
    bool m_inset_shadow{false};

public:
    void set_inset_shadow(bool enabled) noexcept {
        if (m_inset_shadow != enabled) {
            m_inset_shadow = enabled;
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool inset_shadow_enabled() const noexcept { return m_inset_shadow; }

private:
    float m_drag_norm{0.0f};
    int m_active_thumb{0};
    int m_hovered_thumb{-1};
    float m_hover_progress{0.0f};
    float m_press_progress{0.0f};
    std::chrono::steady_clock::time_point m_last_anim_time;
};

// C++20 指定初始化器声明式滑块控件 (Slider DSL Wrapper)
struct Slider : BindableMixin<Slider, RenderSlider> {
    float value{0.0f};
    float min_value{0.0f};
    float max_value{100.0f};
    float step{0.0f};
    float track_height{4.0f};
    float thumb_radius{10.0f};
    float width{180.0f};
    bool disabled{false};

    Fn<void(float)> on_change{nullptr};
    Fn<void(float)> on_change_end{nullptr};

    // WinUI 3 扩展属性
    SliderOrientation orientation{SliderOrientation::Horizontal};
    float height{32.0f};
    float tick_frequency{0.0f};
    bool show_ticks{false};
    bool range_mode{false};
    float range_start{20.0f};
    float range_end{80.0f};
    bool inset_shadow{false};
    Fn<void(float, float)> on_range_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderSlider>& node, Property<float>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_value(prop.get(), false);
    std::weak_ptr<RenderSlider> weak = node;
    bg.subscribe(prop, [weak, guard](float v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_value(v, false);
        }
    });
    auto prev = node->data().on_change;
    Property<float>* p = &prop;
    node->set_on_change([prev = std::move(prev), p, guard](float val) {
        *guard = true;
        p->set(val);
        if (prev) prev(val);
        *guard = false;
    });
}

class SliderWidget : public Widget {
public:
    explicit SliderWidget(const Slider& desc) : m_desc(desc) {}
    explicit SliderWidget(Slider&& desc) noexcept : m_desc(std::move(desc)) {}
    ~SliderWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderSlider>();
        apply_desc_to_node(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sl = std::static_pointer_cast<RenderSlider>(node)) {
            apply_desc_to_node(sl);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderSlider>& node) {
        node->set_range(m_desc.min_value, m_desc.max_value, m_desc.step);
        node->set_value(m_desc.value, false);
        node->set_disabled(m_desc.disabled);
        node->set_track_size(m_desc.track_height, m_desc.thumb_radius, m_desc.width);
        node->set_on_change(m_desc.on_change);
        node->set_on_change_end(m_desc.on_change_end);
        node->set_extended_props(m_desc.orientation, m_desc.height, m_desc.tick_frequency,
                                 m_desc.show_ticks, m_desc.range_mode, m_desc.range_start,
                                 m_desc.range_end, m_desc.on_range_changed);
        node->set_inset_shadow(m_desc.inset_shadow);
    }

    Slider m_desc;
};

inline Slider::operator WidgetPtr() const& {
    return make_widget<SliderWidget>(*this);
}
inline Slider::operator WidgetPtr() && {
    return make_widget<SliderWidget>(std::move(*this));
}

} // namespace ktui
