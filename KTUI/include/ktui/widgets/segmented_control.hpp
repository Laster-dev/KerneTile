#pragma once

#include "widget.hpp"
#include <ktui/animation/frame_clock.hpp>
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
#include <vector>

namespace ktui {

struct SegmentedItem {
    std::string label;
    std::string svg_path;
};

// WinUI 3 / Fluent 2 分段选择器渲染盒 (RenderSegmentedControl)
// 内置 FrameClock 驱动的平滑滑动激活胶囊底板，支持图标+文字分段与双向 selected_index 绑定
class RenderSegmentedControl : public RenderBox {
public:
    RenderSegmentedControl() noexcept {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderSegmentedControl() override = default;

    void set_properties(std::vector<SegmentedItem> items,
                        int selected_index,
                        float width,
                        std::function<void(int)> on_selected_changed) {
        m_items = std::move(items);
        int max_idx = std::max(0, static_cast<int>(m_items.size()) - 1);
        m_selected = std::clamp(selected_index, 0, max_idx);
        m_width = width;
        m_on_selected = std::move(on_selected_changed);
        mark_needs_layout();
    }

    void set_selected_index(int idx) {
        int max_idx = std::max(0, static_cast<int>(m_items.size()) - 1);
        int clamped = std::clamp(idx, 0, max_idx);
        if (m_selected != clamped) {
            m_selected = clamped;
            mark_needs_paint();
        }
    }

    [[nodiscard]] int selected_index() const noexcept { return m_selected; }

    void select(int idx) {
        int max_idx = std::max(0, static_cast<int>(m_items.size()) - 1);
        int clamped = std::clamp(idx, 0, max_idx);
        if (clamped != m_selected) {
            m_selected = clamped;
            mark_needs_paint();
            if (m_on_selected) m_on_selected(m_selected);
        }
    }

    void perform_layout() override {
        float ideal_w = 8.0f;
        for (const auto& it : m_items) {
            float iw = 24.0f + TextMeasure::measure_width(it.label, 13.0f);
            if (!it.svg_path.empty()) iw += 20.0f;
            ideal_w += std::max(64.0f, iw);
        }
        float w = (m_width > 0.0f) ? m_width : std::max(120.0f, std::ceil(ideal_w));
        set_size(m_constraints.constrain(Vec2{w, 34.0f}));
        if (!m_pill_init && !m_items.empty()) {
            Rect sr = segment_rect(m_selected);
            m_pill_x = sr.min_x;
            m_pill_w = sr.width();
            m_pill_init = true;
        }
    }

    void paint(PaintContext& ctx) override {
        if (m_items.empty()) return;
        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        // 槽底板
        Color track_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.045f} : Color{0.0f, 0.0f, 0.0f, 0.045f};
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, h), track_bg, 6.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, h), theme.colors.control_stroke_default, 6.0f, 1.0f);

        // 平滑滑动激活胶囊
        Rect target = segment_rect(m_selected);
        if (!m_pill_init) {
            m_pill_x = target.min_x;
            m_pill_w = target.width();
            m_pill_init = true;
        } else if (std::abs(m_pill_x - target.min_x) > 0.25f || std::abs(m_pill_w - target.width()) > 0.25f) {
            float dt = std::clamp(FrameClock::global().dt(), 1.0f / 240.0f, 0.05f);
            float t = std::min(1.0f, dt * 18.0f);
            m_pill_x += (target.min_x - m_pill_x) * t;
            m_pill_w += (target.width() - m_pill_w) * t;
            mark_needs_paint();
        } else {
            m_pill_x = target.min_x;
            m_pill_w = target.width();
        }

        // Hover 反馈
        if (m_hovered >= 0 && m_hovered != m_selected && m_hovered < static_cast<int>(m_items.size())) {
            ctx.draw_rect(segment_rect(m_hovered), theme.colors.subtle_fill_secondary, 4.0f, 0.0f);
        }

        Rect pill = Rect::from_xywh(std::round(m_pill_x), 3.0f, std::round(m_pill_w), h - 6.0f);
        Color pill_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.11f} : Color{1.0f, 1.0f, 1.0f, 0.96f};
        ctx.draw_rect(pill, pill_bg, 4.0f, 0.0f);
        ctx.draw_rect(pill, theme.colors.control_stroke_default, 4.0f, 1.0f);
        // 底部主题色微指示条
        float bar_w = std::min(22.0f, std::max(10.0f, pill.width() * 0.35f));
        ctx.draw_rect(Rect::from_xywh(std::round(pill.min_x + (pill.width() - bar_w) * 0.5f),
                                      pill.max_y - 2.5f, bar_w, 2.0f),
                      theme.colors.primary, 1.0f, 0.0f);

        Color active_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.96f} : Color{0.0f, 0.0f, 0.0f, 0.92f};
        Color idle_c   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.68f} : Color{0.0f, 0.0f, 0.0f, 0.62f};

        for (size_t i = 0; i < m_items.size(); ++i) {
            Rect sr = segment_rect(static_cast<int>(i));
            const auto& it = m_items[i];
            Color tc = (static_cast<int>(i) == m_selected) ? active_c : idle_c;
            Vec2 ts = TextMeasure::measure(it.label, 13.0f);
            float content_w = ts.x + (it.svg_path.empty() ? 0.0f : 20.0f);
            float sx = std::round(sr.min_x + (sr.width() - content_w) * 0.5f);
            if (!it.svg_path.empty()) {
                Rect ir = Rect::from_xywh(sx, std::round((h - 14.0f) * 0.5f), 14.0f, 14.0f);
                SvgEngine::draw_svg_path(ctx, it.svg_path, 24.0f, 24.0f, ir, tc);
                sx += 20.0f;
            }
            ctx.draw_text(it.label, PointF{sx, std::round((h - ts.y) * 0.5f)}, 13.0f, tc);
        }
    }

private:
    Rect segment_rect(int idx) const noexcept {
        if (m_items.empty()) return Rect{};
        float inner_w = std::max(1.0f, size().x - 6.0f);
        float seg_w = inner_w / static_cast<float>(m_items.size());
        return Rect::from_xywh(3.0f + static_cast<float>(idx) * seg_w, 3.0f, seg_w, size().y - 6.0f);
    }

    int hit_segment(Vec2 local) const noexcept {
        if (m_items.empty() || local.x < 3.0f || local.x > size().x - 3.0f) return -1;
        float seg_w = std::max(1.0f, size().x - 6.0f) / static_cast<float>(m_items.size());
        int idx = static_cast<int>((local.x - 3.0f) / seg_w);
        return (idx >= 0 && idx < static_cast<int>(m_items.size())) ? idx : -1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int h = hit_segment(e.local_position);
            if (h != m_hovered) { m_hovered = h; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered != -1) { m_hovered = -1; mark_needs_paint(); }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int idx = hit_segment(e.local_position);
            if (idx >= 0) {
                select(idx);
                e.stop_propagation();
            }
        };
    }

    std::vector<SegmentedItem> m_items;
    int m_selected{0};
    int m_hovered{-1};
    float m_width{0.0f};
    bool m_pill_init{false};
    float m_pill_x{3.0f};
    float m_pill_w{60.0f};
    std::function<void(int)> m_on_selected;
};

struct SegmentedControl {
    std::vector<SegmentedItem> items;
    Bindable<int> selected_index{0};
    float width{0.0f};
    std::function<void(int)> on_selected_index_changed{nullptr};

    operator WidgetPtr() const;
};

class SegmentedControlWidget : public Widget {
public:
    explicit SegmentedControlWidget(const SegmentedControl& desc) : m_desc(desc) {}
    ~SegmentedControlWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderSegmentedControl>();
        apply(node);
        bind(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::dynamic_pointer_cast<RenderSegmentedControl>(node)) {
            apply(n);
            bind(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderSegmentedControl>& node) {
        auto sel_bind = m_desc.selected_index;
        auto user_cb = m_desc.on_selected_index_changed;
        node->set_properties(
            m_desc.items, m_desc.selected_index.get(), m_desc.width,
            [sel_bind, user_cb](int idx) mutable {
                sel_bind.set_bound(idx);
                if (user_cb) user_cb(idx);
            });
    }

    void bind(const std::shared_ptr<RenderSegmentedControl>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.selected_index, node, [](RenderSegmentedControl& n, int idx) {
            n.set_selected_index(idx);
        });
    }

    SegmentedControl m_desc;
    BindingGroup m_bindings;
};

inline SegmentedControl::operator WidgetPtr() const {
    return std::make_shared<SegmentedControlWidget>(*this);
}

} // namespace ktui
