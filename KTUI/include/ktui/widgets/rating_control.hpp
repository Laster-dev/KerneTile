#pragma once

#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>

namespace ktui {

// WinUI 3 RatingControl 星级评分渲染盒
// 支持整星/半星精度、Hover 实时预览、二次点击清零、只读展示与自定义说明文案
class RenderRatingControl : public RenderBox {
public:
    RenderRatingControl() noexcept {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderRatingControl() override = default;

    void set_properties(float value,
                        int max_rating,
                        bool is_read_only,
                        bool is_clear_enabled,
                        bool allow_half_stars,
                        float star_size,
                        const std::string& caption,
                        std::function<void(float)> on_value_changed) {
        m_max_rating = std::clamp(max_rating, 1, 10);
        float clamped = std::clamp(value, 0.0f, static_cast<float>(m_max_rating));
        if (std::abs(m_value - clamped) > 0.001f) { m_value = clamped; mark_needs_paint(); }
        if (m_read_only != is_read_only) {
            m_read_only = is_read_only;
            cursor_shape = is_read_only ? CursorShape::Arrow : CursorShape::PointingHand;
            if (is_read_only) m_hover_value = -1.0f;
            mark_needs_paint();
        }
        m_clear_enabled = is_clear_enabled;
        m_allow_half = allow_half_stars;
        if (std::abs(m_star_size - star_size) > 0.001f) {
            m_star_size = std::clamp(star_size, 14.0f, 44.0f);
            mark_needs_layout();
        }
        if (m_caption != caption) {
            m_caption = caption;
            mark_needs_layout();
        }
        m_on_value_changed = std::move(on_value_changed);
    }

    void set_value(float v) {
        float clamped = std::clamp(v, 0.0f, static_cast<float>(m_max_rating));
        if (std::abs(m_value - clamped) > 0.001f) {
            m_value = clamped;
            mark_needs_paint();
        }
    }

    [[nodiscard]] float value() const noexcept { return m_value; }
    [[nodiscard]] int max_rating() const noexcept { return m_max_rating; }

    void apply_rating_click(float picked) {
        if (m_read_only) return;
        float next = std::clamp(picked, 0.0f, static_cast<float>(m_max_rating));
        if (m_clear_enabled && std::abs(next - m_value) < 0.001f) {
            next = 0.0f;
        }
        if (std::abs(m_value - next) > 0.001f) {
            m_value = next;
            mark_needs_paint();
            if (m_on_value_changed) m_on_value_changed(m_value);
        }
    }

    void perform_layout() override {
        float stars_w = static_cast<float>(m_max_rating) * m_star_size +
                        static_cast<float>(std::max(0, m_max_rating - 1)) * kStarGap;
        float cap_w = m_caption.empty() ? 0.0f : (10.0f + TextMeasure::measure_width(m_caption, 13.0f));
        float h = std::max(m_star_size + 4.0f, 26.0f);
        set_size(m_constraints.constrain(Vec2{std::ceil(stars_w + cap_w), h}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float h = size().y;
        const float y = std::round((h - m_star_size) * 0.5f);
        const bool hovering = (!m_read_only && m_hover_value > 0.0f);
        const float display_val = hovering ? m_hover_value : m_value;

        static constexpr const char* kStarOutlineSvg =
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
            "<path fill='none' stroke='#FFFFFF' stroke-width='1.65' stroke-linejoin='round' "
            "d='M12 2.8l2.75 5.57 6.15.89-4.45 4.34 1.05 6.12L12 16.83l-5.5 2.89 1.05-6.12L3.1 9.26l6.15-.89L12 2.8z'/>"
            "</svg>";
        static constexpr const char* kStarFilledSvg =
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
            "<path fill='#FFFFFF' d='M12 2.5l2.84 5.75 6.35.92-4.6 4.48 1.09 6.32L12 16.98l-5.68 2.99 1.09-6.32-4.6-4.48 6.35-.92L12 2.5z'/>"
            "</svg>";

        // WinUI 3 RatingControl 标准配色：已评分使用主题强调色，Hover 预览使用高亮强调色，未评分使用柔和空心描边
        Color outline_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.48f} : Color{0.0f, 0.0f, 0.0f, 0.42f};
        Color fill_col = hovering
            ? (theme.is_dark ? Color{118.0f / 255.0f, 214.0f / 255.0f, 255.0f / 255.0f, 1.0f} : theme.colors.primary_hover)
            : (theme.is_dark ? Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f} : theme.colors.primary);
        Color cap_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.72f} : Color{0.0f, 0.0f, 0.0f, 0.66f};

        for (int i = 0; i < m_max_rating; ++i) {
            float x = std::round(static_cast<float>(i) * (m_star_size + kStarGap));
            Rect sr = Rect::from_xywh(x, y, m_star_size, m_star_size);
            SvgEngine::draw_svg_xml(ctx, kStarOutlineSvg, sr, outline_col);

            float star_fill = std::clamp(display_val - static_cast<float>(i), 0.0f, 1.0f);
            if (star_fill >= 0.99f) {
                SvgEngine::draw_svg_xml(ctx, kStarFilledSvg, sr, fill_col);
            } else if (star_fill > 0.01f) {
                ctx.push_clip(Rect::from_xywh(sr.min_x, sr.min_y, sr.width() * star_fill, sr.height()));
                SvgEngine::draw_svg_xml(ctx, kStarFilledSvg, sr, fill_col);
                ctx.pop_clip();
            }
        }

        if (!m_caption.empty()) {
            float stars_w = static_cast<float>(m_max_rating) * m_star_size +
                            static_cast<float>(std::max(0, m_max_rating - 1)) * kStarGap;
            Vec2 ts = TextMeasure::measure(m_caption, 13.0f);
            ctx.draw_text(m_caption,
                          PointF{std::round(stars_w + 10.0f), std::round((h - ts.y) * 0.5f)},
                          13.0f, cap_col);
        }
    }

private:
    static constexpr float kStarGap = 5.0f;

    float rating_at_x(float lx) const noexcept {
        float stride = m_star_size + kStarGap;
        float stars_w = static_cast<float>(m_max_rating) * m_star_size +
                        static_cast<float>(std::max(0, m_max_rating - 1)) * kStarGap;
        if (lx < 0.0f || lx > stars_w) return -1.0f;
        int idx = std::clamp(static_cast<int>(lx / stride), 0, m_max_rating - 1);
        float in_star = lx - static_cast<float>(idx) * stride;
        if (m_allow_half && in_star < m_star_size * 0.5f) {
            return static_cast<float>(idx) + 0.5f;
        }
        return static_cast<float>(idx + 1);
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            if (m_read_only) return;
            float hv = rating_at_x(e.local_position.x);
            cursor_shape = (hv > 0.0f) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (std::abs(hv - m_hover_value) > 0.001f) {
                m_hover_value = hv;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hover_value >= 0.0f) {
                m_hover_value = -1.0f;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (m_read_only || e.button != PointerButton::Primary) return;
            float picked = rating_at_x(e.local_position.x);
            if (picked > 0.0f) {
                apply_rating_click(picked);
                e.stop_propagation();
            }
        };
    }

    float m_value{0.0f};
    int m_max_rating{5};
    bool m_read_only{false};
    bool m_clear_enabled{true};
    bool m_allow_half{false};
    float m_star_size{22.0f};
    std::string m_caption;
    float m_hover_value{-1.0f};
    std::function<void(float)> m_on_value_changed;
};

// C++20 指定初始化器声明式 WinUI 3 RatingControl
struct RatingControl {
    Bindable<float> value{0.0f};
    int max_rating{5};
    bool is_read_only{false};
    bool is_clear_enabled{true};
    bool allow_half_stars{false};
    float star_size{22.0f};
    Bindable<std::string> caption{std::string{}};
    std::function<void(float)> on_value_changed{nullptr};

    operator WidgetPtr() const;
};

class RatingControlWidget : public Widget {
public:
    explicit RatingControlWidget(const RatingControl& desc) : m_desc(desc) {}
    ~RatingControlWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderRatingControl>();
        apply(node);
        bind(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::dynamic_pointer_cast<RenderRatingControl>(node)) {
            apply(n);
            bind(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderRatingControl>& node) {
        auto val_bind = m_desc.value;
        auto user_cb = m_desc.on_value_changed;
        node->set_properties(
            m_desc.value.get(), m_desc.max_rating, m_desc.is_read_only,
            m_desc.is_clear_enabled, m_desc.allow_half_stars, m_desc.star_size,
            m_desc.caption.get(),
            [val_bind, user_cb](float v) mutable {
                val_bind.set_bound(v);
                if (user_cb) user_cb(v);
            });
    }

    void bind(const std::shared_ptr<RenderRatingControl>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.value, node, [](RenderRatingControl& n, float v) { n.set_value(v); });
        m_bindings.attach(m_desc.caption, node, [this](RenderRatingControl& n, const std::string& cap) {
            n.set_properties(n.value(), m_desc.max_rating, m_desc.is_read_only,
                             m_desc.is_clear_enabled, m_desc.allow_half_stars, m_desc.star_size,
                             cap, m_desc.on_value_changed);
        });
    }

    RatingControl m_desc;
    BindingGroup m_bindings;
};

inline RatingControl::operator WidgetPtr() const {
    return std::make_shared<RatingControlWidget>(*this);
}

} // namespace ktui
