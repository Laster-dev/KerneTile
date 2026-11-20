#pragma once

#include "checkbox_data.hpp"
#include "checkbox_style.hpp"
#include "../widget.hpp"
#include <ktui/animation/motion.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>

#include <chrono>
#include <cmath>
#include <memory>
#include <algorithm>

namespace ktui {

// 桌面高精复选框渲染盒 (RenderCheckbox)
// 参照 D:\CUI\CUI.Core\ui 规范：支持两态/三态切换、对勾笔触生长动效与半选横杠动效
class RenderCheckbox : public RenderBox {
public:
    RenderCheckbox() noexcept {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
        m_last_tick_time = std::chrono::steady_clock::now();
    }

    ~RenderCheckbox() override = default;

    // 向后兼容历史接口 (Legacy Setter)
    void set_properties(bool is_chk, std::string lbl, float box_sz, float radius,
                        Color bg_norm, Color bg_chk, Color border_c,
                        Color mark_c, Color txt_c, float font_sz,
                        std::function<void(bool)> on_ch) {
        m_data.label = std::move(lbl);
        m_data.box_size = (box_sz > 0.0f) ? box_sz : 18.0f;
        m_data.corner_radius = (radius >= 0.0f) ? radius : 3.0f;
        m_data.font_size = (font_sz > 0.0f) ? font_sz : 13.0f;
        m_data.on_change = std::move(on_ch);

        m_custom_bg_normal = (bg_norm.r >= 0.0f) ? std::optional<Color>(bg_norm) : std::nullopt;
        m_custom_bg_checked = (bg_chk.r >= 0.0f) ? std::optional<Color>(bg_chk) : std::nullopt;
        m_custom_border = (border_c.r >= 0.0f) ? std::optional<Color>(border_c) : std::nullopt;
        m_custom_check_mark = (mark_c.r >= 0.0f) ? std::optional<Color>(mark_c) : std::nullopt;
        m_custom_text = (txt_c.r >= 0.0f) ? std::optional<Color>(txt_c) : std::nullopt;

        set_checked_internal(is_chk, false);
        mark_needs_layout();
    }

    // 状态访问与设置
    [[nodiscard]] bool checked() const noexcept {
        return m_data.state == CheckState::Checked;
    }

    void set_checked(bool is_chk) {
        set_checked_internal(is_chk, true);
    }

    [[nodiscard]] CheckState state() const noexcept {
        return m_data.state;
    }

    void set_state(CheckState st) {
        set_state_internal(st, true);
    }

    [[nodiscard]] bool is_three_state() const noexcept {
        return m_data.is_three_state;
    }

    void set_three_state(bool three) noexcept {
        m_data.is_three_state = three;
    }

    [[nodiscard]] const std::string& label() const noexcept {
        return m_data.label;
    }

    void set_label(std::string lbl) {
        if (m_data.label != lbl) {
            m_data.label = std::move(lbl);
            mark_needs_layout();
        }
    }

    [[nodiscard]] bool is_disabled() const noexcept {
        return m_state.is_disabled;
    }

    void set_disabled(bool dis) noexcept {
        if (m_state.is_disabled != dis) {
            m_state.is_disabled = dis;
            m_data.disabled = dis;
            cursor_shape = dis ? CursorShape::Arrow : CursorShape::PointingHand;
            mark_needs_paint();
        }
    }

    void set_change_handler(std::function<void(bool)> cb) {
        m_data.on_change = std::move(cb);
    }

    void set_state_change_handler(std::function<void(CheckState)> cb) {
        m_data.on_state_change = std::move(cb);
    }

    // 交互状态翻转
    void toggle() {
        cycle_state();
    }

    void cycle_state() {
        if (m_state.is_disabled) return;

        CheckState next;
        if (m_data.is_three_state) {
            if (m_data.state == CheckState::Unchecked) next = CheckState::Checked;
            else if (m_data.state == CheckState::Checked) next = CheckState::Indeterminate;
            else next = CheckState::Unchecked;
        } else {
            next = (m_data.state == CheckState::Checked) ? CheckState::Unchecked : CheckState::Checked;
        }

        set_state_internal(next, true);

        if (m_data.on_change) {
            m_data.on_change(m_data.state == CheckState::Checked);
        }
        if (m_data.on_state_change) {
            m_data.on_state_change(m_data.state);
        }
    }

    // 样式解析辅助
    [[nodiscard]] ResolvedCheckboxStyle resolve_current_style() const {
        const Theme& theme = ThemeContext::current();
        return CheckboxStyleResolver::resolve(
            m_data, m_state, theme,
            m_custom_bg_normal, m_custom_bg_checked, m_custom_border,
            m_custom_check_mark, m_custom_text,
            m_custom_box_size, m_custom_radius, m_custom_font_size
        );
    }

    // 布局度量
    void perform_layout() override {
        auto style = resolve_current_style();
        Vec2 label_sz = m_data.label.empty() ? Vec2{0.0f, 0.0f} : TextMeasure::measure(m_data.label, style.font_size);
        float label_w = m_data.label.empty() ? 0.0f : (label_sz.x + style.gap);
        float desired_w = style.box_size + label_w;
        float desired_h = std::max(style.box_size, label_sz.y > 0.0f ? label_sz.y : style.font_size * 1.35f);

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
    }

    // 绘制管道
    void paint(PaintContext& context) override {
        // 1. 步进由动效管理器托管的补间驱动器 (无控件私有硬编码算法)
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - m_last_tick_time).count();
        m_last_tick_time = now;
        if (dt > 0.0f && dt < 0.2f) {
            bool animating = false;
            if (m_fill_tween.is_active()) { m_fill_tween.tick(dt); animating = true; }
            if (m_check_tween.is_active()) { m_check_tween.tick(dt); animating = true; }
            if (m_indeterminate_tween.is_active()) { m_indeterminate_tween.tick(dt); animating = true; }
            if (animating) {
                mark_needs_paint();
            }
        }

        auto style = resolve_current_style();

        // 2. 绘制方框
        float box_y = (m_size.y - style.box_size) * 0.5f;
        Rect box_rect = Rect::from_xywh(0.0f, box_y, style.box_size, style.box_size);

        if (style.box_background.a > 0.001f) {
            context.draw_rect(box_rect, style.box_background, style.corner_radius, 0.0f);
        }
        if (style.box_border.a > 0.001f && style.border_width > 0.0f) {
            context.draw_rect(box_rect, style.box_border, style.corner_radius, style.border_width);
        }

        // 3. 绘制对勾生长动效 (Checkmark Path Tracing 严格遵循 CUI 几何比例与 EaseOutCubic 曲线)
        float check_factor = m_check_tween.current_value();
        if (check_factor > 0.001f) {
            float scale = style.box_size / 18.0f;
            PointF p1{box_rect.min_x + 4.6f * scale, box_rect.min_y + 9.4f * scale};
            PointF p2{box_rect.min_x + 7.6f * scale, box_rect.min_y + 12.3f * scale};
            PointF p3{box_rect.min_x + 13.4f * scale, box_rect.min_y + 6.0f * scale};

            float len1 = std::hypot(p2.x - p1.x, p2.y - p1.y);
            float len2 = std::hypot(p3.x - p2.x, p3.y - p2.y);
            float total = len1 + len2;
            float draw_len = total * check_factor;

            if (draw_len <= len1) {
                float seg_t = len1 > 0.0f ? draw_len / len1 : 1.0f;
                PointF cur_pt{p1.x + (p2.x - p1.x) * seg_t, p1.y + (p2.y - p1.y) * seg_t};
                context.draw_line(p1, cur_pt, style.check_mark, style.stroke_width);
            } else {
                context.draw_line(p1, p2, style.check_mark, style.stroke_width);
                float remain = draw_len - len1;
                float seg_t = len2 > 0.0f ? remain / len2 : 1.0f;
                PointF cur_pt{p2.x + (p3.x - p2.x) * seg_t, p2.y + (p3.y - p2.y) * seg_t};
                context.draw_line(p2, cur_pt, style.check_mark, style.stroke_width);
            }
        }

        // 4. 绘制半选横杠动效 (Indeterminate Bar: 从中心向两侧平滑展开)
        float ind_factor = m_indeterminate_tween.current_value();
        if (ind_factor > 0.001f) {
            float scale = style.box_size / 18.0f;
            float half_w = 4.5f * scale * ind_factor;
            float cx = box_rect.min_x + box_rect.width() * 0.5f;
            float cy = box_rect.min_y + box_rect.height() * 0.5f;
            float bar_h = std::max(2.0f, 3.0f * scale);
            context.draw_rect(Rect::from_xywh(cx - half_w, cy - bar_h * 0.5f, half_w * 2.0f, bar_h),
                              style.check_mark, 1.0f, 0.0f);
        }

        // 5. 绘制标签文字 (施加硬件裁剪，严防文本溢出父容器边界)
        if (!m_data.label.empty()) {
            float text_x = style.box_size + style.gap;
            float text_y = std::max(0.0f, (m_size.y - style.font_size * 1.35f) * 0.5f);
            context.push_clip(bounds(), 0.0f);
            context.draw_text(m_data.label, PointF{text_x, text_y}, style.font_size, style.text);
            context.pop_clip();
        }
    }

    void apply_custom_overrides(std::optional<Color> bg_normal,
                                std::optional<Color> bg_checked,
                                std::optional<Color> border,
                                std::optional<Color> check_mark,
                                std::optional<Color> txt,
                                std::optional<float> box_sz,
                                std::optional<float> radius,
                                std::optional<float> font_sz) {
        m_custom_bg_normal = bg_normal;
        m_custom_bg_checked = bg_checked;
        m_custom_border = border;
        m_custom_check_mark = check_mark;
        m_custom_text = txt;
        m_custom_box_size = box_sz;
        m_custom_radius = radius;
        m_custom_font_size = font_sz;
    }

private:
    void set_checked_internal(bool is_chk, bool animate) {
        set_state_internal(is_chk ? CheckState::Checked : CheckState::Unchecked, animate);
    }

    void set_state_internal(CheckState st, bool animate) {
        if (m_data.state == st) return;
        m_data.state = st;

        if (animate) {
            float fill_target = (st == CheckState::Unchecked) ? 0.0f : 1.0f;
            float check_target = (st == CheckState::Checked) ? 1.0f : 0.0f;
            float ind_target = (st == CheckState::Indeterminate) ? 1.0f : 0.0f;

            m_fill_tween.start(m_fill_tween.current_value(), fill_target, 0.16f, Easing::EaseOutQuad);
            m_check_tween.start(m_check_tween.current_value(), check_target, 0.22f, Easing::EaseOutCubic);
            m_indeterminate_tween.start(m_indeterminate_tween.current_value(), ind_target, 0.18f, Easing::EaseOutQuad);
        } else {
            float fill_val = (st == CheckState::Unchecked) ? 0.0f : 1.0f;
            float check_val = (st == CheckState::Checked) ? 1.0f : 0.0f;
            float ind_val = (st == CheckState::Indeterminate) ? 1.0f : 0.0f;
            m_fill_tween.start(fill_val, fill_val, 0.0001f); m_fill_tween.stop();
            m_check_tween.start(check_val, check_val, 0.0001f); m_check_tween.stop();
            m_indeterminate_tween.start(ind_val, ind_val, 0.0001f); m_indeterminate_tween.stop();
        }

        m_last_tick_time = std::chrono::steady_clock::now();
        mark_needs_paint();
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_state.is_disabled) {
                m_state.is_hovered = true;
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_state.is_hovered = false;
            m_state.is_pressed = false;
            mark_needs_paint();
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary && !m_state.is_disabled) {
                m_state.is_pressed = true;
                cycle_state();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary && !m_state.is_disabled) {
                m_state.is_pressed = false;
                mark_needs_paint();
            }
        };

        on_focus = [this](FocusEvent& ev) {
            m_state.is_focused = ev.gained_focus;
            mark_needs_paint();
        };

        on_key = [this](KeyEvent& ev) {
            if (!m_state.is_disabled && ev.type == PlatformEventType::KeyDown) {
                // 空格键 (32) 或 回车键 (13) 切换
                if (ev.key_code == 32 || ev.key_code == 13) {
                    cycle_state();
                }
            }
        };
    }

    CheckboxData m_data;
    CheckboxInteractiveState m_state;

    // 中央动效管理器托管的补间动画驱动器
    MotionTween<float> m_fill_tween;
    MotionTween<float> m_check_tween;
    MotionTween<float> m_indeterminate_tween;
    std::chrono::steady_clock::time_point m_last_tick_time;

    // 自定义覆盖样式
    std::optional<Color> m_custom_bg_normal;
    std::optional<Color> m_custom_bg_checked;
    std::optional<Color> m_custom_border;
    std::optional<Color> m_custom_check_mark;
    std::optional<Color> m_custom_text;
    std::optional<float> m_custom_box_size;
    std::optional<float> m_custom_radius;
    std::optional<float> m_custom_font_size;
};

class CheckboxWidget;

// C++20 指定初始化器声明式复选框 (Checkbox DSL Wrapper)
struct Checkbox {
    bool checked{false};
    CheckState state{CheckState::Unchecked};
    bool is_three_state{false};
    BindRef<bool> bind_checked{nullptr};
    BindRef<CheckState> bind_state{nullptr};
    BindRef<bool> bind_disabled{nullptr};
    std::string label{""};

    float size{0.0f};           // 0 表示按主题解析默认值 18px (兼容外部传递 14.0f 等)
    float corner_radius{-1.0f}; // 负值代表采用主题解析默认值 3px
    Color background_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color checked_background_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color check_mark_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color text_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    float font_size{0.0f};
    bool disabled{false};

    std::function<void(bool)> on_change{nullptr};
    std::function<void(CheckState)> on_state_change{nullptr};

    operator WidgetPtr() const;
};

// CheckboxWidget 声明式包装组件
class CheckboxWidget : public Widget {
public:
    explicit CheckboxWidget(const Checkbox& desc) : m_desc(desc) {}

    ~CheckboxWidget() override {
        unsubscribe();
    }

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderCheckbox>();
        apply_desc_to_node(node);
        setup_bindings(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto box = std::dynamic_pointer_cast<RenderCheckbox>(node)) {
            apply_desc_to_node(box);
            setup_bindings(box);
        }
    }

    [[nodiscard]] const Checkbox& desc() const noexcept { return m_desc; }

private:
    void apply_desc_to_node(std::shared_ptr<RenderCheckbox> node) {
        node->set_three_state(m_desc.is_three_state);
        node->set_label(m_desc.label);

        CheckState init_state = m_desc.state;
        if (m_desc.bind_state) {
            init_state = m_desc.bind_state->get();
        } else if (m_desc.bind_checked) {
            init_state = m_desc.bind_checked->get() ? CheckState::Checked : CheckState::Unchecked;
        } else if (m_desc.checked) {
            init_state = CheckState::Checked;
        }
        node->set_state(init_state);

        bool is_dis = m_desc.bind_disabled ? m_desc.bind_disabled->get() : m_desc.disabled;
        node->set_disabled(is_dis);

        // 应用自定义覆盖参数
        std::optional<Color> bg_norm = (m_desc.background_color.r >= 0.0f) ? std::optional<Color>(m_desc.background_color) : std::nullopt;
        std::optional<Color> bg_chk = (m_desc.checked_background_color.r >= 0.0f) ? std::optional<Color>(m_desc.checked_background_color) : std::nullopt;
        std::optional<Color> border = (m_desc.border_color.r >= 0.0f) ? std::optional<Color>(m_desc.border_color) : std::nullopt;
        std::optional<Color> mark = (m_desc.check_mark_color.r >= 0.0f) ? std::optional<Color>(m_desc.check_mark_color) : std::nullopt;
        std::optional<Color> txt = (m_desc.text_color.r >= 0.0f) ? std::optional<Color>(m_desc.text_color) : std::nullopt;
        std::optional<float> sz = (m_desc.size > 0.0f) ? std::optional<float>(m_desc.size) : std::nullopt;
        std::optional<float> radius = (m_desc.corner_radius >= 0.0f) ? std::optional<float>(m_desc.corner_radius) : std::nullopt;
        std::optional<float> font_sz = (m_desc.font_size > 0.0f) ? std::optional<float>(m_desc.font_size) : std::nullopt;

        node->apply_custom_overrides(bg_norm, bg_chk, border, mark, txt, sz, radius, font_sz);

        // 挂接双向通知代理
        auto on_ch = [this, weak_node = std::weak_ptr<RenderCheckbox>(node)](bool is_chk) {
            m_is_internal_update = true;
            if (m_desc.bind_checked) {
                m_desc.bind_checked->set(is_chk);
            }
            if (m_desc.bind_state) {
                m_desc.bind_state->set(is_chk ? CheckState::Checked : CheckState::Unchecked);
            }
            if (m_desc.on_change) {
                m_desc.on_change(is_chk);
            }
            m_is_internal_update = false;
        };

        auto on_st_ch = [this, weak_node = std::weak_ptr<RenderCheckbox>(node)](CheckState st) {
            m_is_internal_update = true;
            if (m_desc.bind_state) {
                m_desc.bind_state->set(st);
            }
            if (m_desc.bind_checked) {
                m_desc.bind_checked->set(st == CheckState::Checked);
            }
            if (m_desc.on_state_change) {
                m_desc.on_state_change(st);
            }
            m_is_internal_update = false;
        };

        node->set_change_handler(on_ch);
        node->set_state_change_handler(on_st_ch);
    }

    void setup_bindings(std::shared_ptr<RenderCheckbox> node) {
        unsubscribe();

        if (m_desc.bind_checked) {
            m_checked_token = m_desc.bind_checked->subscribe([weak_node = std::weak_ptr<RenderCheckbox>(node), this](bool val) {
                if (m_is_internal_update) return;
                if (auto n = weak_node.lock()) {
                    n->set_checked(val);
                }
            });
        }

        if (m_desc.bind_state) {
            m_state_token = m_desc.bind_state->subscribe([weak_node = std::weak_ptr<RenderCheckbox>(node), this](CheckState st) {
                if (m_is_internal_update) return;
                if (auto n = weak_node.lock()) {
                    n->set_state(st);
                }
            });
        }

        if (m_desc.bind_disabled) {
            m_disabled_token = m_desc.bind_disabled->subscribe([weak_node = std::weak_ptr<RenderCheckbox>(node)](bool dis) {
                if (auto n = weak_node.lock()) {
                    n->set_disabled(dis);
                }
            });
        }
    }

    void unsubscribe() {
        if (m_checked_token != 0 && m_desc.bind_checked) {
            m_desc.bind_checked->unsubscribe(m_checked_token);
            m_checked_token = 0;
        }
        if (m_state_token != 0 && m_desc.bind_state) {
            m_desc.bind_state->unsubscribe(m_state_token);
            m_state_token = 0;
        }
        if (m_disabled_token != 0 && m_desc.bind_disabled) {
            m_desc.bind_disabled->unsubscribe(m_disabled_token);
            m_disabled_token = 0;
        }
    }

    Checkbox m_desc;
    bool m_is_internal_update{false};
    uint64_t m_checked_token{0};
    uint64_t m_state_token{0};
    uint64_t m_disabled_token{0};
};

inline Checkbox::operator WidgetPtr() const {
    return std::make_shared<CheckboxWidget>(*this);
}

} // namespace ktui
