#pragma once

#include "button_data.hpp"
#include "button_style.hpp"
#include <ktui/animation/animation.hpp>
#include <ktui/animation/tactile_press.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/theme/theme.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace ktui {

// 桌面级现代按钮渲染盒 (RenderButton - Logic Layer)
// 职责：交互状态机驱动、键盘 Enter/Space 托管响应、布局尺寸测量、图元绘制
// 架构契约：严禁控件内自写动效数学算法，所有曲线插值与物理模型 100% 委托给 Motion 动效管理器！
class RenderButton : public RenderBox {
public:
    RenderButton() noexcept {
        cursor_shape = CursorShape::PointingHand;
        is_focusable = true;
        setup_behavior();
    }

    ~RenderButton() override = default;

    // 向后兼容历史接口 (Legacy Setter)
    void set_properties(std::string text, float h, float radius, EdgeInsets pad,
                        Color bg, Color hover, Color active, Color txt_col,
                        Color border_col, float border_w, float font_sz,
                        Fn<void()> click_cb, bool is_disabled) {
        m_data.text = std::move(text);
        m_custom_height = (h > 0.0f) ? std::optional<float>(h) : std::nullopt;
        m_custom_radius = (radius > 0.0f) ? std::optional<float>(radius) : std::nullopt;
        m_custom_padding = (pad.left >= 0.0f) ? std::optional<EdgeInsets>(pad) : std::nullopt;
        m_custom_bg = (bg.r >= 0.0f) ? std::optional<Color>(bg) : std::nullopt;
        m_custom_hover = (hover.r >= 0.0f) ? std::optional<Color>(hover) : std::nullopt;
        m_custom_active = (active.r >= 0.0f) ? std::optional<Color>(active) : std::nullopt;
        m_custom_text = (txt_col.r >= 0.0f) ? std::optional<Color>(txt_col) : std::nullopt;
        m_custom_border = (border_col.r >= 0.0f) ? std::optional<Color>(border_col) : std::nullopt;
        m_border_width = border_w;
        m_border_width_explicit = (border_w >= 0.0f);
        m_custom_font_size = (font_sz > 0.0f) ? std::optional<float>(font_sz) : std::nullopt;
        m_data.on_click = std::move(click_cb);

        set_disabled(is_disabled);
        mark_needs_layout();
    }

    // ==================== 数据与状态读写 ====================

    [[nodiscard]] const ButtonData& data() const noexcept { return m_data; }
    [[nodiscard]] const ButtonInteractiveState& state() const noexcept { return m_state; }

    [[nodiscard]] const std::string& text() const noexcept { return m_data.text; }
    void set_text(std::string text) {
        if (m_data.text != text) {
            m_data.text = std::move(text);
            mark_needs_layout();
        }
    }

    [[nodiscard]] const std::string& icon() const noexcept { return m_data.icon; }
    void set_icon(std::string icon) {
        if (m_data.icon != icon) {
            m_data.icon = std::move(icon);
            mark_needs_layout();
        }
    }

    [[nodiscard]] const std::string& svg_path() const noexcept { return m_svg_path; }
    void set_svg_path(std::string path) {
        if (m_svg_path != path) {
            m_svg_path = std::move(path);
            mark_needs_layout();
        }
    }

    void set_corner_radii(float tl, float tr, float br, float bl) {
        m_custom_radii = {tl, tr, br, bl};
        mark_needs_paint();
    }

    void set_border_width(float bw) {
        m_border_width = bw;
        m_border_width_explicit = true;
        mark_needs_paint();
    }

    void set_forced_pressed(bool forced) {
        if (m_forced_pressed != forced) {
            m_forced_pressed = forced;
            mark_needs_paint();
        }
    }

    [[nodiscard]] ButtonVariant variant() const noexcept { return m_data.variant; }
    void set_variant(ButtonVariant v) {
        if (m_data.variant != v) {
            m_data.variant = v;
            mark_needs_paint();
        }
    }

    [[nodiscard]] ButtonSize size_mode() const noexcept { return m_data.size; }
    void set_size_mode(ButtonSize s) {
        if (m_data.size != s) {
            m_data.size = s;
            mark_needs_layout();
        }
    }

    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    void set_disabled(bool disabled) {
        if (m_state.is_disabled != disabled) {
            m_state.is_disabled = disabled;
            cursor_shape = (m_state.is_disabled || m_state.is_loading) ? CursorShape::Forbidden : CursorShape::PointingHand;
            if (m_state.is_disabled) {
                m_state.is_hovered = false;
                m_state.is_pressed = false;
                is_hovered = false;
                is_active = false;
                m_ripple_active = false;
                m_reveal_active = false;
                m_ripple_radius_tween.stop();
                m_ripple_opacity_tween.stop();
            }
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool loading() const noexcept { return m_state.is_loading; }
    void set_loading(bool loading) {
        if (m_state.is_loading != loading) {
            m_state.is_loading = loading;
            cursor_shape = (m_state.is_disabled || m_state.is_loading) ? CursorShape::Forbidden : CursorShape::PointingHand;
            if (m_state.is_loading) {
                m_reveal_active = false;
            }
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool pointer_reveal_enabled() const noexcept { return m_data.enable_pointer_reveal; }
    void set_pointer_reveal_enabled(bool enabled) {
        if (m_data.enable_pointer_reveal != enabled) {
            m_data.enable_pointer_reveal = enabled;
            if (!enabled) {
                m_reveal_active = false;
            }
            mark_needs_paint();
        }
    }

    [[nodiscard]] float font_size() const noexcept {
        return m_custom_font_size.value_or(resolve_current_style().font_size);
    }

    [[nodiscard]] Color text_color() const noexcept {
        return m_custom_text.value_or(resolve_current_style().text);
    }

    // ==================== Fluent 水波纹 (Ripple) 动效控制 ====================
    // 完全委托给 MotionTween 与 Motion 管理器，无任何私有数学曲线

    void begin_ripple(Vec2 local_pt) {
        if (!m_data.enable_ripple || m_state.is_disabled || m_state.is_loading) {
            m_ripple_active = false;
            return;
        }
        m_ripple_center = local_pt;

        float corner_x = (local_pt.x > m_size.x * 0.5f) ? 0.0f : m_size.x;
        float corner_y = (local_pt.y > m_size.y * 0.5f) ? 0.0f : m_size.y;
        float dx = local_pt.x - corner_x;
        float dy = local_pt.y - corner_y;
        float max_radius = std::sqrt(dx * dx + dy * dy);

        auto style = resolve_current_style();
        float duration = style.ripple_duration > 0.0f ? style.ripple_duration : 0.65f;

        // 委托动效管理器：半径使用 EaseOutCubic 快速膨胀，透明度从 style.ripple.a 平滑淡出至 0.0 (绝对半透明，杜绝纯白)
        m_ripple_radius_tween.start(4.0f, max_radius, duration, Easing::EaseOutCubic);
        m_ripple_opacity_tween.start(style.ripple.a, 0.0f, duration, Easing::Linear);
        m_ripple_radius = 4.0f;
        m_ripple_opacity = style.ripple.a;
        m_ripple_active = true;
        m_last_tick_time = std::chrono::steady_clock::now();
        mark_needs_paint();
    }

    bool tick_ripple(float dt) {
        if (!m_ripple_active) return false;

        bool r_active = m_ripple_radius_tween.tick(dt);
        bool o_active = m_ripple_opacity_tween.tick(dt);

        m_ripple_radius = m_ripple_radius_tween.current_value();
        m_ripple_opacity = m_ripple_opacity_tween.current_value();
        m_ripple_active = r_active || o_active;

        mark_needs_paint();
        return m_ripple_active;
    }

    [[nodiscard]] bool has_active_ripple() const noexcept { return m_ripple_active; }
    [[nodiscard]] float ripple_radius() const noexcept { return m_ripple_radius; }
    [[nodiscard]] float ripple_opacity() const noexcept { return m_ripple_opacity; }

    // ==================== 样式解析辅助 ====================

    [[nodiscard]] ResolvedButtonStyle resolve_current_style() const {
        const Theme& theme = ThemeContext::current();
        ButtonInteractiveState eff_state = m_state;
        if (m_forced_pressed) eff_state.is_pressed = true;
        auto resolved = ButtonStyleResolver::resolve(
            m_data, eff_state, theme,
            m_custom_bg, m_custom_hover, m_custom_active, m_custom_text, m_custom_border,
            m_custom_height, m_custom_radius, m_custom_padding, m_custom_font_size
        );
        if (m_border_width_explicit) {
            resolved.border_width = m_border_width;
        }
        return resolved;
    }

    // ==================== 布局度量 (Layout & Measure) ====================

    void perform_layout() override {
        auto style = resolve_current_style();

        float content_w = 0.0f;
        if (!m_svg_path.empty()) {
            content_w += 15.0f;
        } else if (!m_data.icon.empty()) {
            content_w += style.font_size + 2.0f;
        }
        if (!m_data.text.empty()) {
            float text_w = TextMeasure::measure_width(m_data.text, style.font_size);
            if (content_w > 0.0f) {
                content_w += style.icon_gap;
            }
            content_w += text_w;
        }

        float desired_w = content_w + style.padding.horizontal() + style.border_width * 2.0f;
        float desired_h = style.height;

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
    }

    // ==================== 表面圆角推导 ====================
    [[nodiscard]] float surface_corner_radius() const noexcept override {
        return resolve_current_style().corner_radius;
    }

    // ==================== 绘制管线 (Painting Pipeline) ====================

    void paint(PaintContext& context) override {
        // 1. 如果水波纹或触感按压弹簧正处于运行中，推进动效步进
        if (m_ripple_active) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::clamp(std::chrono::duration<float>(now - m_last_tick_time).count(), 0.0f, 0.1f);
            m_last_tick_time = now;
            if (dt > 0.0f) {
                bool r_active = m_ripple_radius_tween.tick(dt);
                bool o_active = m_ripple_opacity_tween.tick(dt);
                m_ripple_radius = m_ripple_radius_tween.current_value();
                m_ripple_opacity = m_ripple_opacity_tween.current_value();
                m_ripple_active = r_active || o_active;
            }
            if (m_ripple_active) {
                mark_needs_paint();
            }
        }
        if (m_tactile_press.update()) {
            mark_needs_paint();
        }
        const bool tactile_active = m_tactile_press.is_active();
        const Rect base_rect = bounds();
        const Rect paint_rect = tactile_active ? m_tactile_press.transform_rect(base_rect) : base_rect;
        const float tactile_scale = tactile_active ? m_tactile_press.current_scale() : 1.0f;

        auto style = resolve_current_style();
        const float r_tl = (m_custom_radii ? (*m_custom_radii)[0] : style.corner_radius) * tactile_scale;
        const float r_tr = (m_custom_radii ? (*m_custom_radii)[1] : style.corner_radius) * tactile_scale;
        const float r_br = (m_custom_radii ? (*m_custom_radii)[2] : style.corner_radius) * tactile_scale;
        const float r_bl = (m_custom_radii ? (*m_custom_radii)[3] : style.corner_radius) * tactile_scale;
        const float clip_r = std::max({r_tl, r_tr, r_br, r_bl});

        // 2. WinUI 3 按钮背景填充
        if (style.background.a > 0.001f) {
            if (m_custom_radii) {
                context.draw_rounded_rect(paint_rect, style.background, r_tl, r_tr, r_br, r_bl);
            } else {
                context.draw_rect(paint_rect, style.background, style.corner_radius * tactile_scale, 0.0f);
            }
        }

        // 3. 点击涟漪微动效 (裁剪于圆角边界内)
        if (m_ripple_active && m_ripple_opacity > 0.001f) {
            context.push_clip(paint_rect, clip_r);
            Color ripple_col = style.ripple;
            ripple_col.a = m_ripple_opacity;
            Rect ripple_rect = Rect::from_xywh(
                m_ripple_center.x - m_ripple_radius,
                m_ripple_center.y - m_ripple_radius,
                m_ripple_radius * 2.0f,
                m_ripple_radius * 2.0f
            );
            context.draw_rect(ripple_rect, ripple_col, m_ripple_radius, 0.0f);
            context.pop_clip();
        }

        // 4. WinUI 3 1px 轮廓描边 + 立体高光/底边 (ControlElevationBorder)
        if (style.border_width > 0.0f && style.border.a > 0.001f) {
            if (m_custom_radii) {
                context.draw_rounded_rect(paint_rect, style.border, r_tl, r_tr, r_br, r_bl, 0.0f, Vec2{0.0f, 0.0f}, style.border_width);
            } else {
                context.draw_rect(paint_rect, style.border, style.corner_radius * tactile_scale, style.border_width);
            }
        }
        if (style.elevation_edge.a > 0.001f && style.border_width > 0.0f) {
            float ey = style.elevation_on_top ? paint_rect.min_y : (paint_rect.max_y - 1.0f);
            float inset_l = (r_tl > 0.0f || r_bl > 0.0f) ? 3.0f : 0.0f;
            float inset_r = (r_tr > 0.0f || r_br > 0.0f) ? 3.0f : 0.0f;
            float ew = std::max(0.0f, paint_rect.width() - inset_l - inset_r);
            if (ew > 0.0f) {
                context.draw_rect(Rect::from_xywh(paint_rect.min_x + inset_l, ey, ew, 1.0f), style.elevation_edge, 0.0f, 0.0f);
            }
        }
        if (tactile_active && m_tactile_press.enabled() && !m_state.is_disabled) {
            const float bw = (style.border_width > 0.0f) ? style.border_width : 1.0f;
            const float press_w = std::clamp(0.35f + 0.65f * m_tactile_press.press_amount(), 0.25f, 1.0f);
            context.draw_directional_rim(
                paint_rect,
                Color{1.0f, 1.0f, 1.0f, 0.28f * press_w},
                Color{0.0f, 0.0f, 0.0f, 0.18f * press_w},
                clip_r, bw, m_tactile_press.tilted_light_dir(), 0.22f * press_w);
        }

        // 4.5 Opt-in Fluent Pointer Reveal (支持控件级或 Theme 全局开启)
        if ((m_data.enable_pointer_reveal || ThemeContext::current().enable_pointer_reveal) &&
            m_reveal_active && !m_state.is_disabled && !m_state.is_loading) {
            const bool is_dark = ThemeContext::current().is_dark;
            const bool use_light_sheen = is_dark ||
                m_data.variant == ButtonVariant::Accent ||
                m_data.variant == ButtonVariant::Danger;
            const Color surface = use_light_sheen
                ? Color{1.0f, 1.0f, 1.0f, 0.11f}
                : Color{0.0f, 0.0f, 0.0f, 0.06f};
            const Color edge = use_light_sheen
                ? Color{1.0f, 1.0f, 1.0f, 0.34f}
                : Color{0.0f, 0.0f, 0.0f, 0.24f};
            const float reveal_bw = (style.border_width > 0.0f) ? style.border_width : 1.0f;
            context.draw_pointer_reveal(paint_rect, m_reveal_pos, 64.0f, surface, edge, clip_r, reveal_bw);
        }

        // 5. 居中绘制 SVG/文本图标与文字 (在内容区 padding 内居中，支持外部预留右侧箭头区)
        if (!m_data.text.empty() || !m_data.icon.empty() || !m_svg_path.empty()) {
            context.push_clip(paint_rect, clip_r);
            const float rs = std::max(1.0f, context.render_scale());
            const float dx = tactile_active ? ((paint_rect.min_x + paint_rect.max_x) * 0.5f - (base_rect.min_x + base_rect.max_x) * 0.5f) : 0.0f;
            const float dy = tactile_active ? ((paint_rect.min_y + paint_rect.max_y) * 0.5f - (base_rect.min_y + base_rect.max_y) * 0.5f) : 0.0f;
            const Vec2 content_shift{std::round(dx * rs) / rs, std::round(dy * rs) / rs};
            std::optional<PaintContext::OffsetScope> content_offset;
            if (tactile_active && (std::abs(content_shift.x) > 0.001f || std::abs(content_shift.y) > 0.001f)) {
                content_offset.emplace(context, content_shift);
            }

            float icon_size = !m_svg_path.empty() ? 15.0f : (m_data.icon.empty() ? 0.0f : (style.font_size + 2.0f));
            float text_w = m_data.text.empty() ? 0.0f : TextMeasure::measure_width(m_data.text, style.font_size);
            float gap = ((!m_svg_path.empty() || !m_data.icon.empty()) && !m_data.text.empty()) ? style.icon_gap : 0.0f;
            float total_w = icon_size + gap + text_w;

            float avail_left = style.padding.left;
            float avail_w = std::max(0.0f, m_size.x - style.padding.horizontal());
            float start_x = avail_left + std::max(0.0f, (avail_w - total_w) * 0.5f);

            if (!m_svg_path.empty()) {
                Rect ir = Rect::from_xywh(std::round(start_x), std::round((m_size.y - icon_size) * 0.5f), icon_size, icon_size);
                SvgEngine::draw_svg_path(context, m_svg_path, 24.0f, 24.0f, ir, style.text);
                start_x += icon_size + gap;
            } else if (!m_data.icon.empty()) {
                float icon_y = std::max(0.0f, (m_size.y - icon_size) * 0.5f);
                context.draw_text(m_data.icon, PointF{start_x, icon_y}, style.font_size, style.text);
                start_x += icon_size + gap;
            }

            if (!m_data.text.empty()) {
                Vec2 measured = TextMeasure::measure(m_data.text, style.font_size);
                float text_y = std::round((m_size.y - measured.y) * 0.5f);
                context.draw_text(m_data.text, PointF{std::round(start_x), text_y}, style.font_size, style.text);
            }

            context.pop_clip();
        }
    }

    void apply_custom_overrides(std::optional<Color> bg,
                                std::optional<Color> hover,
                                std::optional<Color> active,
                                std::optional<Color> txt,
                                std::optional<Color> border,
                                std::optional<float> h,
                                std::optional<float> radius,
                                std::optional<EdgeInsets> pad,
                                std::optional<float> font_sz,
                                float border_w,
                                bool border_w_explicit) {
        m_custom_bg = bg;
        m_custom_hover = hover;
        m_custom_active = active;
        m_custom_text = txt;
        m_custom_border = border;
        m_custom_height = h;
        m_custom_radius = radius;
        m_custom_padding = pad;
        m_custom_font_size = font_sz;
        m_border_width = border_w;
        m_border_width_explicit = border_w_explicit;
    }

    void set_custom_height(float h) { m_custom_height = (h > 0.0f) ? std::optional<float>(h) : std::nullopt; mark_needs_layout(); }
    void set_custom_corner_radius(float r) { m_custom_radius = (r > 0.0f) ? std::optional<float>(r) : std::nullopt; mark_needs_paint(); }
    void set_custom_padding(EdgeInsets pad) { m_custom_padding = (pad.left >= 0.0f) ? std::optional<EdgeInsets>(pad) : std::nullopt; mark_needs_layout(); }
    void set_custom_background(Color c) { m_custom_bg = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_hover_color(Color c) { m_custom_hover = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_active_color(Color c) { m_custom_active = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_text_color(Color c) { m_custom_text = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_border_color(Color c) { m_custom_border = (c.r >= 0.0f) ? std::optional<Color>(c) : std::nullopt; mark_needs_paint(); }
    void set_custom_border_width(float bw) { m_border_width = bw; m_border_width_explicit = (bw >= 0.0f); mark_needs_layout(); }
    void set_custom_font_size(float fs) { m_custom_font_size = (fs > 0.0f) ? std::optional<float>(fs) : std::nullopt; mark_needs_layout(); }
    void set_focusable(bool f) { m_data.focusable = f; is_focusable = f; }
    void set_enable_ripple(bool e) { m_data.enable_ripple = e; }

    void set_tactile_press(bool enabled, float press_scale = 0.962f, float tilt_shift = 3.0f) {
        m_tactile_press.set_tuning(press_scale, tilt_shift);
        m_tactile_press.set_enabled(enabled);
        mark_needs_paint();
    }
    [[nodiscard]] bool tactile_press_enabled() const noexcept { return m_tactile_press.enabled(); }

    void set_click_handler(Fn<void()> cb) {
        m_data.on_click = std::move(cb);
    }

private:
    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_state.is_disabled && !m_state.is_loading) {
                m_state.is_hovered = true;
                is_hovered = true;
                m_tactile_press.on_enter();
                mark_needs_paint();
            }
        };

        on_pointer_move = [this](PointerEvent& ev) {
            if (ev.type == PlatformEventType::PointerMove &&
                ev.device_kind != PointerDeviceKind::Touch &&
                !m_state.is_disabled && !m_state.is_loading) {
                bool need_paint = false;
                if (m_tactile_press.enabled()) {
                    m_tactile_press.on_move(ev.local_position, m_size);
                    need_paint = true;
                }
                if ((m_data.enable_pointer_reveal || ThemeContext::current().enable_pointer_reveal) &&
                    (!m_reveal_active ||
                     std::abs(m_reveal_pos.x - ev.local_position.x) > 0.01f ||
                     std::abs(m_reveal_pos.y - ev.local_position.y) > 0.01f)) {
                    m_reveal_active = true;
                    m_reveal_pos = ev.local_position;
                    need_paint = true;
                }
                if (need_paint) {
                    mark_needs_paint();
                }
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            bool need_paint = m_reveal_active || m_tactile_press.is_active();
            m_reveal_active = false;
            m_tactile_press.on_exit();
            if (!m_state.is_disabled && !m_state.is_loading) {
                if (m_state.is_hovered || m_state.is_pressed || is_hovered || is_active) {
                    need_paint = true;
                }
                m_state.is_hovered = false;
                m_state.is_pressed = false;
                is_hovered = false;
                is_active = false;
            }
            if (need_paint) {
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && !m_state.is_loading && ev.button == PointerButton::Primary) {
                m_state.is_pressed = true;
                is_active = true;
                m_tactile_press.on_down(ev.local_position, m_size);
                if (m_data.enable_ripple) {
                    begin_ripple(ev.local_position);
                }
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && !m_state.is_loading && ev.button == PointerButton::Primary) {
                bool should_click = (m_state.is_pressed && m_state.is_hovered);
                auto cb = should_click ? m_data.on_click : Fn<void()>{};
                m_state.is_pressed = false;
                is_active = false;
                m_tactile_press.on_up();
                mark_needs_paint();
                if (cb) {
                    ev.stop_propagation();
                    cb();
                }
            }
        };

        on_focus = [this](FocusEvent& ev) {
            m_state.is_focused = ev.gained_focus;
            mark_needs_paint();
        };

        on_key = [this](KeyEvent& ev) {
            if (!m_state.is_disabled && !m_state.is_loading && ev.type == PlatformEventType::KeyDown) {
                constexpr uint32_t kKeySpace = 32;
                constexpr uint32_t kKeyEnter = 13;
                if (ev.key_code == kKeySpace || ev.key_code == kKeyEnter) {
                    if (m_data.enable_ripple) {
                        begin_ripple(Vec2{m_size.x * 0.5f, m_size.y * 0.5f});
                    }
                    if (m_data.on_click) {
                        m_data.on_click();
                    }
                    mark_needs_paint();
                }
            }
        };
    }

    ButtonData m_data;
    ButtonInteractiveState m_state;

    // 自定义显式覆盖属性 (std::nullopt 表示沿用变体及尺寸主题解析)
    std::optional<Color> m_custom_bg;
    std::optional<Color> m_custom_hover;
    std::optional<Color> m_custom_active;
    std::optional<Color> m_custom_text;
    std::optional<Color> m_custom_border;
    std::optional<float> m_custom_height;
    std::optional<float> m_custom_radius;
    std::optional<EdgeInsets> m_custom_padding;
    std::optional<float> m_custom_font_size;
    std::string m_svg_path;
    std::optional<std::array<float, 4>> m_custom_radii;
    bool m_forced_pressed{false};
    float m_border_width{1.0f};
    bool m_border_width_explicit{false};

    // 动效管理器托管的补间驱动器 (零控件内硬编码算法)
    MotionTween<float> m_ripple_radius_tween;
    MotionTween<float> m_ripple_opacity_tween;
    Vec2 m_ripple_center{0.0f, 0.0f};
    float m_ripple_radius{0.0f};
    float m_ripple_opacity{0.0f};
    bool m_ripple_active{false};
    bool m_reveal_active{false};
    PointF m_reveal_pos{0.0f, 0.0f};
    std::chrono::steady_clock::time_point m_last_tick_time;
    TactilePressPhysics m_tactile_press;
};

class ButtonWidget;

// C++20 指定初始化器专业桌面按钮 (Button DSL Wrapper)
// 零开销原生字段 + 链式 .bind(...) 按需绑定任意属性
struct Button : BindableMixin<Button, RenderButton> {
    std::string text{""};
    std::string icon{""};
    ButtonVariant variant{ButtonVariant::Standard};
    ButtonSize size{ButtonSize::Normal};

    // 显式覆盖样式参数 (若指定则优先于变体默认值，实现 100% 自由定制与向下兼容)
    float height{0.0f};
    float corner_radius{0.0f};
    EdgeInsets padding{EdgeInsets(-1.0f, -1.0f)}; // 负值代表采用主题/尺寸解析默认值
    Color background_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}}; // r < 0 代表未显式指定
    Color hover_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color active_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color text_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    float border_width{-1.0f};                    // 负值代表采用主题解析默认值
    float font_size{0.0f};

    Fn<void()> on_click{nullptr};
    bool disabled{false};
    bool loading{false};
    bool focusable{true};
    bool enable_ripple{true};
    bool enable_pointer_reveal{false};
    bool tactile_press{false};
    float tactile_press_scale{0.962f};
    float tactile_tilt_shift{3.0f};

    Button& pointer_reveal(bool enabled) & noexcept { enable_pointer_reveal = enabled; return *this; }
    Button&& pointer_reveal(bool enabled) && noexcept { enable_pointer_reveal = enabled; return std::move(*this); }

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

// ButtonWidget 声明式包装组件
class ButtonWidget : public Widget {
public:
    explicit ButtonWidget(const Button& desc) : m_desc(desc) {}
    explicit ButtonWidget(Button&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderButton>();
        apply_desc_to_node(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto btn = std::static_pointer_cast<RenderButton>(node)) {
            apply_desc_to_node(btn);
        }
    }

    [[nodiscard]] const Button& desc() const noexcept { return m_desc; }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderButton>& node) {
        node->set_text(m_desc.text);
        node->set_icon(m_desc.icon);
        node->set_variant(m_desc.variant);
        node->set_size_mode(m_desc.size);
        node->set_click_handler(m_desc.on_click);
        node->set_loading(m_desc.loading);
        node->set_disabled(m_desc.disabled);
        node->set_focusable(m_desc.focusable);
        node->set_enable_ripple(m_desc.enable_ripple);
        node->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
        node->set_tactile_press(m_desc.tactile_press, m_desc.tactile_press_scale, m_desc.tactile_tilt_shift);

        std::optional<Color> bg = (m_desc.background_color.r >= 0.0f) ? std::optional<Color>(m_desc.background_color) : std::nullopt;
        std::optional<Color> hover = (m_desc.hover_color.r >= 0.0f) ? std::optional<Color>(m_desc.hover_color) : std::nullopt;
        std::optional<Color> active = (m_desc.active_color.r >= 0.0f) ? std::optional<Color>(m_desc.active_color) : std::nullopt;
        std::optional<Color> txt = (m_desc.text_color.r >= 0.0f) ? std::optional<Color>(m_desc.text_color) : std::nullopt;
        std::optional<Color> border = (m_desc.border_color.r >= 0.0f) ? std::optional<Color>(m_desc.border_color) : std::nullopt;
        std::optional<float> h = (m_desc.height > 0.0f) ? std::optional<float>(m_desc.height) : std::nullopt;
        std::optional<float> radius = (m_desc.corner_radius > 0.0f) ? std::optional<float>(m_desc.corner_radius) : std::nullopt;
        std::optional<EdgeInsets> pad = (m_desc.padding.left >= 0.0f) ? std::optional<EdgeInsets>(m_desc.padding) : std::nullopt;
        std::optional<float> font_sz = (m_desc.font_size > 0.0f) ? std::optional<float>(m_desc.font_size) : std::nullopt;
        bool border_w_explicit = (m_desc.border_width >= 0.0f);

        node->apply_custom_overrides(bg, hover, active, txt, border, h, radius, pad, font_sz, m_desc.border_width, border_w_explicit);
        node->mark_needs_layout();
    }

    Button m_desc;
};

KTUI_NOINLINE inline Button::operator WidgetPtr() const& {
    return make_widget<ButtonWidget>(*this);
}
KTUI_NOINLINE inline Button::operator WidgetPtr() && {
    return make_widget<ButtonWidget>(std::move(*this));
}

} // namespace ktui
