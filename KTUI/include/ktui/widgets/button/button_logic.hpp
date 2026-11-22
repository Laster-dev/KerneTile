#pragma once

#include "button_data.hpp"
#include "button_style.hpp"
#include <ktui/animation/animation.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/theme/theme.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
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
                        std::function<void()> click_cb, bool is_disabled) {
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
        auto resolved = ButtonStyleResolver::resolve(
            m_data, m_state, theme,
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
        if (!m_data.icon.empty()) {
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

    // ==================== 绘制管线 (Painting Pipeline) ====================

    void paint(PaintContext& context) override {
        // 1. 如果水波纹正处于运行中，推进动效管理器步进
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

        auto style = resolve_current_style();

        // 2. 绘制微圆角实心背景 (绝非大圆角，桌面 3px 硬朗)
        if (style.background.a > 0.001f) {
            context.draw_rect(bounds(), style.background, style.corner_radius, 0.0f);
        }

        // 3. 绘制 Fluent 水波纹点击反馈微动效 (局限于微圆角边界内部裁剪)
        if (m_ripple_active && m_ripple_opacity > 0.001f) {
            context.push_clip(bounds(), style.corner_radius);
            Color ripple_col = style.ripple;
            ripple_col.a = m_ripple_opacity; // 纯半透明动态衰减，杜绝白色硬块
            Rect ripple_rect = Rect::from_xywh(
                m_ripple_center.x - m_ripple_radius,
                m_ripple_center.y - m_ripple_radius,
                m_ripple_radius * 2.0f,
                m_ripple_radius * 2.0f
            );
            context.draw_rect(ripple_rect, ripple_col, m_ripple_radius, 0.0f);
            context.pop_clip();
        }

        // 4. 绘制 1px 精致微光描边
        if (style.border_width > 0.0f && style.border.a > 0.001f) {
            context.draw_rect(bounds(), style.border, style.corner_radius, style.border_width);
        }

        // 5. 居中绘制图标与文本 (硬件视口裁剪，防止极端溢出)
        if (!m_data.text.empty() || !m_data.icon.empty()) {
            context.push_clip(bounds(), style.corner_radius);

            float icon_size = m_data.icon.empty() ? 0.0f : (style.font_size + 2.0f);
            float text_w = m_data.text.empty() ? 0.0f : TextMeasure::measure_width(m_data.text, style.font_size);
            float gap = (!m_data.icon.empty() && !m_data.text.empty()) ? style.icon_gap : 0.0f;
            float total_w = icon_size + gap + text_w;

            float start_x = std::max(style.padding.left, (m_size.x - total_w) * 0.5f);

            if (!m_data.icon.empty()) {
                float icon_y = std::max(0.0f, (m_size.y - icon_size) * 0.5f);
                context.draw_text(m_data.icon, PointF{start_x, icon_y}, style.font_size, style.text);
                start_x += icon_size + gap;
            }

            if (!m_data.text.empty()) {
                Vec2 measured = TextMeasure::measure(m_data.text, style.font_size);
                float text_y = std::max(0.0f, (m_size.y - measured.y) * 0.5f);
                context.draw_text(m_data.text, PointF{start_x, text_y}, style.font_size, style.text);
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

    void set_click_handler(std::function<void()> cb) {
        m_data.on_click = std::move(cb);
    }

private:
    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_state.is_disabled && !m_state.is_loading) {
                m_state.is_hovered = true;
                is_hovered = true;
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            if (!m_state.is_disabled && !m_state.is_loading) {
                m_state.is_hovered = false;
                m_state.is_pressed = false;
                is_hovered = false;
                is_active = false;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && !m_state.is_loading && ev.button == PointerButton::Primary) {
                m_state.is_pressed = true;
                is_active = true;
                if (m_data.enable_ripple) {
                    begin_ripple(ev.local_position);
                }
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (!m_state.is_disabled && !m_state.is_loading && ev.button == PointerButton::Primary) {
                if (m_state.is_pressed && m_state.is_hovered) {
                    if (m_data.on_click) {
                        m_data.on_click();
                    }
                }
                m_state.is_pressed = false;
                is_active = false;
                mark_needs_paint();
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
    float m_border_width{1.0f};
    bool m_border_width_explicit{false};

    // 动效管理器托管的补间驱动器 (零控件内硬编码算法)
    MotionTween<float> m_ripple_radius_tween;
    MotionTween<float> m_ripple_opacity_tween;
    Vec2 m_ripple_center{0.0f, 0.0f};
    float m_ripple_radius{0.0f};
    float m_ripple_opacity{0.0f};
    bool m_ripple_active{false};
    std::chrono::steady_clock::time_point m_last_tick_time;
};

class ButtonWidget;

// C++20 指定初始化器专业桌面按钮 (Button DSL Wrapper)
// 桌面专业标准：默认 28px 高度，3px 硬朗微圆角，支持 Accent/Standard/Subtle/Danger 变体，内置 Fluent 水波纹，全属性支持 Bind(...)
struct Button {
    Bindable<std::string> text{""};
    Bindable<std::string> icon{""};
    Bindable<ButtonVariant> variant{ButtonVariant::Standard};
    Bindable<ButtonSize> size{ButtonSize::Normal};

    // 显式覆盖样式参数 (若指定则优先于变体默认值，实现 100% 自由定制与向下兼容)
    Bindable<float> height{0.0f};
    Bindable<float> corner_radius{0.0f};
    Bindable<EdgeInsets> padding{EdgeInsets(-1.0f, -1.0f)}; // 负值代表采用主题/尺寸解析默认值
    Bindable<Color> background_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}}; // r < 0 代表未显式指定
    Bindable<Color> hover_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<Color> active_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<Color> text_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<Color> border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Bindable<float> border_width{-1.0f};                    // 负值代表采用主题解析默认值
    Bindable<float> font_size{0.0f};

    std::function<void()> on_click{nullptr};
    Bindable<bool> disabled{false};
    Bindable<bool> loading{false};
    Bindable<bool> focusable{true};
    Bindable<bool> enable_ripple{true};

    operator WidgetPtr() const;
};

// ButtonWidget 声明式包装组件
class ButtonWidget : public Widget {
public:
    explicit ButtonWidget(const Button& desc) : m_desc(desc) {}
    ~ButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderButton>();
        apply_desc_to_node(node);
        setup_bindings(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto btn = std::dynamic_pointer_cast<RenderButton>(node)) {
            apply_desc_to_node(btn);
            setup_bindings(btn);
        }
    }

    [[nodiscard]] const Button& desc() const noexcept { return m_desc; }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderButton>& node) {
        node->set_text(m_desc.text.get());
        node->set_icon(m_desc.icon.get());
        node->set_variant(m_desc.variant.get());
        node->set_size_mode(m_desc.size.get());
        node->set_click_handler(m_desc.on_click);
        node->set_loading(m_desc.loading.get());
        node->set_disabled(m_desc.disabled.get());
        node->set_focusable(m_desc.focusable.get());
        node->set_enable_ripple(m_desc.enable_ripple.get());

        Color bg_c = m_desc.background_color.get();
        Color hov_c = m_desc.hover_color.get();
        Color act_c = m_desc.active_color.get();
        Color txt_c = m_desc.text_color.get();
        Color brd_c = m_desc.border_color.get();
        float h_v = m_desc.height.get();
        float rad_v = m_desc.corner_radius.get();
        EdgeInsets pad_v = m_desc.padding.get();
        float fs_v = m_desc.font_size.get();
        float bw_v = m_desc.border_width.get();

        std::optional<Color> bg = (bg_c.r >= 0.0f) ? std::optional<Color>(bg_c) : std::nullopt;
        std::optional<Color> hover = (hov_c.r >= 0.0f) ? std::optional<Color>(hov_c) : std::nullopt;
        std::optional<Color> active = (act_c.r >= 0.0f) ? std::optional<Color>(act_c) : std::nullopt;
        std::optional<Color> txt = (txt_c.r >= 0.0f) ? std::optional<Color>(txt_c) : std::nullopt;
        std::optional<Color> border = (brd_c.r >= 0.0f) ? std::optional<Color>(brd_c) : std::nullopt;
        std::optional<float> h = (h_v > 0.0f) ? std::optional<float>(h_v) : std::nullopt;
        std::optional<float> radius = (rad_v > 0.0f) ? std::optional<float>(rad_v) : std::nullopt;
        std::optional<EdgeInsets> pad = (pad_v.left >= 0.0f) ? std::optional<EdgeInsets>(pad_v) : std::nullopt;
        std::optional<float> font_sz = (fs_v > 0.0f) ? std::optional<float>(fs_v) : std::nullopt;
        bool border_w_explicit = (bw_v >= 0.0f);

        node->apply_custom_overrides(bg, hover, active, txt, border, h, radius, pad, font_sz, bw_v, border_w_explicit);
        node->mark_needs_layout();
    }

    void setup_bindings(const std::shared_ptr<RenderButton>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.text, node, [](RenderButton& n, const std::string& v) { n.set_text(v); });
        m_bindings.attach(m_desc.icon, node, [](RenderButton& n, const std::string& v) { n.set_icon(v); });
        m_bindings.attach(m_desc.variant, node, [](RenderButton& n, ButtonVariant v) { n.set_variant(v); });
        m_bindings.attach(m_desc.size, node, [](RenderButton& n, ButtonSize v) { n.set_size_mode(v); });
        m_bindings.attach(m_desc.height, node, [](RenderButton& n, float v) { n.set_custom_height(v); });
        m_bindings.attach(m_desc.corner_radius, node, [](RenderButton& n, float v) { n.set_custom_corner_radius(v); });
        m_bindings.attach(m_desc.padding, node, [](RenderButton& n, EdgeInsets v) { n.set_custom_padding(v); });
        m_bindings.attach(m_desc.background_color, node, [](RenderButton& n, Color v) { n.set_custom_background(v); });
        m_bindings.attach(m_desc.hover_color, node, [](RenderButton& n, Color v) { n.set_custom_hover_color(v); });
        m_bindings.attach(m_desc.active_color, node, [](RenderButton& n, Color v) { n.set_custom_active_color(v); });
        m_bindings.attach(m_desc.text_color, node, [](RenderButton& n, Color v) { n.set_custom_text_color(v); });
        m_bindings.attach(m_desc.border_color, node, [](RenderButton& n, Color v) { n.set_custom_border_color(v); });
        m_bindings.attach(m_desc.border_width, node, [](RenderButton& n, float v) { n.set_custom_border_width(v); });
        m_bindings.attach(m_desc.font_size, node, [](RenderButton& n, float v) { n.set_custom_font_size(v); });
        m_bindings.attach(m_desc.disabled, node, [](RenderButton& n, bool v) { n.set_disabled(v); });
        m_bindings.attach(m_desc.loading, node, [](RenderButton& n, bool v) { n.set_loading(v); });
        m_bindings.attach(m_desc.focusable, node, [](RenderButton& n, bool v) { n.set_focusable(v); });
        m_bindings.attach(m_desc.enable_ripple, node, [](RenderButton& n, bool v) { n.set_enable_ripple(v); });
    }

    Button m_desc;
    BindingGroup m_bindings;
};

inline Button::operator WidgetPtr() const {
    return std::make_shared<ButtonWidget>(*this);
}

} // namespace ktui
