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

    void set_state(ProgressBarState s) {
        if (m_data.state != s) {
            m_data.state = s;
            mark_needs_paint();
        }
    }

    void set_corner_radius(float r) {
        if (std::abs(m_data.corner_radius - r) > 0.001f) {
            m_data.corner_radius = r;
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

        // 1. WinUI 3 1px 居中极细底线 (仅确定模式显示，不确定模式按 WinUI 3 规范隐藏)
        if (style.show_rail && style.track_bg.a > 0.001f) {
            const float rail_h = style.rail_height;
            const float rail_y = (size().y - rail_h) * 0.5f;
            Rect rail_rect = Rect::from_xywh(0.0f, rail_y, track_w, rail_h);
            ctx.draw_rect(rail_rect, style.track_bg, rail_h * 0.5f, 0.0f);
        }

        // 2. 进度内容绘制 (通过胶囊裁剪栈确保指示条两端无溢出)
        ctx.push_clip(track_rect, style.corner_radius);

        if (m_data.indeterminate) {
            // WinUI 3 官方双段不确定进度动画 (主长条 + 副短条，独立头尾三次贝塞尔变速伸展收缩)
            auto ease_out_fast = [](float t) {
                t = std::clamp(t, 0.0f, 1.0f);
                float inv = 1.0f - t;
                return 1.0f - inv * inv * inv;
            };
            auto ease_in_out_quad = [](float t) {
                t = std::clamp(t, 0.0f, 1.0f);
                return t < 0.5f ? (2.0f * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f);
            };
            auto ease_in_cubic = [](float t) {
                t = std::clamp(t, 0.0f, 1.0f);
                return t * t * t;
            };

            const float u = std::fmod(m_phase, 2.0f) * 0.5f; // 归一化周期 [0, 1)

            // 主波 (Leading Long Bar): u in [0.00, 0.68]
            if (u < 0.68f) {
                float t = u / 0.68f;
                float head = ease_out_fast(std::min(1.0f, t * 1.18f));
                float tail = ease_in_cubic(std::max(0.0f, (t - 0.14f) / 0.86f));
                float left = std::clamp(tail, 0.0f, 1.0f) * track_w;
                float right = std::clamp(head, 0.0f, 1.0f) * track_w;
                float w = right - left;
                if (w > 0.5f) {
                    ctx.draw_rect(Rect::from_xywh(left, track_y, w, track_h),
                                  style.indicator_color, style.corner_radius, 0.0f);
                }
            }

            // 副波 (Trailing Short Bar): u in [0.46, 0.98]
            if (u >= 0.46f && u < 0.98f) {
                float t = (u - 0.46f) / 0.52f;
                float head = ease_in_out_quad(std::min(1.0f, t * 1.25f));
                float tail = ease_in_out_quad(std::max(0.0f, (t - 0.18f) / 0.82f));
                float left = std::clamp(tail, 0.0f, 1.0f) * track_w;
                float right = std::clamp(head, 0.0f, 1.0f) * track_w;
                float w = right - left;
                if (w > 0.5f) {
                    ctx.draw_rect(Rect::from_xywh(left, track_y, w, track_h),
                                  style.indicator_color, style.corner_radius, 0.0f);
                }
            }
        } else {
            // 确定性平滑插值进度 (WinUI 3 3px 实心药丸指示条)
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
    Bindable<ProgressBarState> state{ProgressBarState::Normal};
    Bindable<float> track_height{3.0f};
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
        node->set_state(m_desc.state.get());
        node->set_track_height(m_desc.track_height.get());
        node->set_bar_width(m_desc.width.get());
        node->set_corner_radius(m_desc.corner_radius.get());
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
        m_bindings.attach(m_desc.state, node, [](RenderProgressBar& n, ProgressBarState s) {
            n.set_state(s);
        });
        m_bindings.attach(m_desc.track_height, node, [](RenderProgressBar& n, float v) {
            n.set_track_height(v);
        });
        m_bindings.attach(m_desc.width, node, [](RenderProgressBar& n, float v) {
            n.set_bar_width(v);
        });
        m_bindings.attach(m_desc.corner_radius, node, [](RenderProgressBar& n, float v) {
            n.set_corner_radius(v);
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
