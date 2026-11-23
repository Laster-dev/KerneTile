#pragma once

#include "progress_bar_data.hpp"
#include "progress_bar_style.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

namespace ktui {

// 线性进度条渲染盒 (RenderProgressBar - Logic Layer)
class RenderProgressBar : public RenderBox {
public:
    RenderProgressBar() noexcept {
        setup_behavior();
    }

    ~RenderProgressBar() override = default;

    [[nodiscard]] float value() const noexcept { return m_data.value; }
    [[nodiscard]] bool indeterminate() const noexcept { return m_data.indeterminate; }
    [[nodiscard]] bool disabled() const noexcept { return m_state.is_disabled; }
    [[nodiscard]] const ProgressBarData& data() const noexcept { return m_data; }

    void set_value(float val, bool animate = true) {
        float clamped = std::clamp(val, 0.0f, 1.0f);
        if (std::abs(m_data.value - clamped) > 0.0001f) {
            m_data.value = clamped;
            if (!animate) {
                m_displayed_val = clamped;
            } else {
                m_last_anim_time = std::chrono::steady_clock::now();
            }
            mark_needs_paint();
        }
    }

    void set_indeterminate(bool ind) {
        if (m_data.indeterminate != ind) {
            m_data.indeterminate = ind;
            m_phase = 0.0f;
            m_last_anim_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        }
    }

    void set_track_height(float h) {
        if (h > 0.0f && std::abs(m_data.track_height - h) > 0.001f) {
            m_data.track_height = h;
            mark_needs_layout();
        }
    }

    void set_bar_width(float w) {
        if (w > 0.0f && std::abs(m_data.width - w) > 0.001f) {
            m_data.width = w;
            mark_needs_layout();
        }
    }

    void set_disabled(bool disabled) {
        if (m_state.is_disabled != disabled) {
            m_state.is_disabled = disabled;
            m_data.disabled = disabled;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float w = m_constraints.constrain_width(m_data.width > 0.0f ? m_data.width : 180.0f);
        float h = m_constraints.constrain_height(std::max(m_data.track_height, 12.0f));
        set_size(Vec2{w, h});
    }

    void paint(PaintContext& ctx) override {
        update_animations();

        const Theme& theme = ThemeContext::current();
        ResolvedProgressBarStyle style = ProgressBarStyleResolver::resolve(m_data, m_state, theme);

        const float track_w = size().x;
        const float track_h = style.track_height;
        const float track_y = (size().y - track_h) * 0.5f;
        Rect track_rect = Rect::from_xywh(0.0f, track_y, track_w, track_h);

        // 1. 绘制底槽
        ctx.draw_rect(track_rect, style.track_bg, style.corner_radius, 0.0f);

        // 2. 进度内容绘制 (通过裁剪栈确保圆角边缘平滑无溢出)
        ctx.push_clip(track_rect, style.corner_radius);

        if (m_data.indeterminate) {
            // Windows 11 Fluent 风格双脉冲交错循环流光动效
            auto smoothstep = [](float edge0, float edge1, float x) {
                float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
                return t * t * (3.0f - 2.0f * t);
            };

            // 脉冲 1 (主波): 周期前段迅速展开横跨
            float p1 = std::fmod(m_phase, 2.0f);
            if (p1 < 1.4f) {
                float t_norm = p1 / 1.4f;
                float ease_center = smoothstep(0.0f, 1.0f, t_norm);
                float center_x = -0.15f * track_w + (track_w * 1.30f) * ease_center;
                float pulse_w = (0.18f + 0.32f * std::sin(t_norm * 3.14159265f)) * track_w;
                float left = center_x - pulse_w * 0.5f;
                ctx.draw_rect(Rect::from_xywh(left, track_y, pulse_w, track_h),
                              style.indicator_color, style.corner_radius, 0.0f);
            }

            // 脉冲 2 (副波): 滞后交错跟随
            float p2 = std::fmod(m_phase + 1.25f, 2.0f);
            if (p2 < 1.3f) {
                float t_norm = p2 / 1.3f;
                float ease_center = smoothstep(0.0f, 1.0f, t_norm);
                float center_x = -0.10f * track_w + (track_w * 1.20f) * ease_center;
                float pulse_w = (0.12f + 0.24f * std::sin(t_norm * 3.14159265f)) * track_w;
                float left = center_x - pulse_w * 0.5f;
                ctx.draw_rect(Rect::from_xywh(left, track_y, pulse_w, track_h),
                              style.indicator_color, style.corner_radius, 0.0f);
            }
        } else {
            // 确定性平滑插值进度
            if (m_displayed_val > 0.001f) {
                float fill_w = track_w * m_displayed_val;
                Rect fill_rect = Rect::from_xywh(0.0f, track_y, fill_w, track_h);
                ctx.draw_rect(fill_rect, style.indicator_color, style.corner_radius, 0.0f);
            }
        }

        ctx.pop_clip();
    }

private:
    void setup_behavior() {
        m_displayed_val = m_data.value;
    }

    void update_animations() {
        auto now = std::chrono::steady_clock::now();
        if (m_last_anim_time.time_since_epoch().count() == 0) {
            m_last_anim_time = now;
            if (m_data.indeterminate || std::abs(m_displayed_val - m_data.value) > 0.001f) {
                mark_needs_paint();
            }
            return;
        }
        float dt = std::chrono::duration<float>(now - m_last_anim_time).count();
        m_last_anim_time = now;
        dt = std::clamp(dt, 0.0f, 0.05f);

        bool animating = false;
        if (m_data.indeterminate) {
            m_phase += dt; // 2.0s 循环周期
            if (m_phase >= 2.0f) {
                m_phase = std::fmod(m_phase, 2.0f);
            }
            animating = true;
        } else {
            float diff = m_data.value - m_displayed_val;
            if (std::abs(diff) > 0.001f) {
                m_displayed_val += diff * std::min(1.0f, dt * 26.0f);
                if (std::abs(m_data.value - m_displayed_val) <= 0.001f) {
                    m_displayed_val = m_data.value;
                }
                animating = true;
            } else {
                m_displayed_val = m_data.value;
            }
        }

        if (animating) {
            mark_needs_paint();
        }
    }

    ProgressBarData m_data;
    ProgressBarInteractiveState m_state;
    float m_displayed_val{0.0f};
    float m_phase{0.0f};
    std::chrono::steady_clock::time_point m_last_anim_time;
};

// C++20 指定初始化器声明式进度条控件 (ProgressBar DSL Wrapper)
struct ProgressBar {
    Bindable<float> value{0.0f};
    Bindable<bool> indeterminate{false};
    Bindable<float> track_height{4.0f};
    Bindable<float> width{180.0f};
    Bindable<float> corner_radius{-1.0f};
    Bindable<bool> disabled{false};

    operator WidgetPtr() const;
};

class ProgressBarWidget : public Widget {
public:
    explicit ProgressBarWidget(const ProgressBar& desc) : m_desc(desc) {}
    ~ProgressBarWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderProgressBar>();
        apply_desc_to_node(node);
        setup_bindings(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto pb = std::dynamic_pointer_cast<RenderProgressBar>(node)) {
            apply_desc_to_node(pb);
            setup_bindings(pb);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderProgressBar>& node) {
        node->set_value(m_desc.value.get(), false);
        node->set_indeterminate(m_desc.indeterminate.get());
        node->set_track_height(m_desc.track_height.get());
        node->set_bar_width(m_desc.width.get());
        node->set_disabled(m_desc.disabled.get());
    }

    void setup_bindings(const std::shared_ptr<RenderProgressBar>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.value, node, [](RenderProgressBar& n, float v) {
            n.set_value(v, true);
        });
        m_bindings.attach(m_desc.indeterminate, node, [](RenderProgressBar& n, bool v) {
            n.set_indeterminate(v);
        });
        m_bindings.attach(m_desc.track_height, node, [](RenderProgressBar& n, float v) {
            n.set_track_height(v);
        });
        m_bindings.attach(m_desc.width, node, [](RenderProgressBar& n, float v) {
            n.set_bar_width(v);
        });
        m_bindings.attach(m_desc.disabled, node, [](RenderProgressBar& n, bool v) {
            n.set_disabled(v);
        });
    }

    ProgressBar m_desc;
    BindingGroup m_bindings;
};

inline ProgressBar::operator WidgetPtr() const {
    return std::make_shared<ProgressBarWidget>(*this);
}

} // namespace ktui
