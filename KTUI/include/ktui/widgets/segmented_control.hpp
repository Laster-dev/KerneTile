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
#include <chrono>
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
                        Fn<void(int)> on_selected_changed) {
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
        float ideal_w = 6.0f;
        for (const auto& it : m_items) {
            float iw = 28.0f + TextMeasure::measure_width(it.label, 13.0f);
            if (!it.svg_path.empty()) iw += 20.0f;
            ideal_w += std::max(68.0f, iw);
        }
        float w = (m_width > 0.0f) ? m_width : std::max(120.0f, std::ceil(ideal_w));
        set_size(m_constraints.constrain(Vec2{w, 32.0f}));
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

        // 1. Fluent 2 轻质凹槽底板 (单层极简凹槽，去除双层硬描边)
        Color track_bg = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.24f} : Color{0.0f, 0.0f, 0.0f, 0.045f};
        Color track_border = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.06f} : Color{0.0f, 0.0f, 0.0f, 0.065f};
        ctx.draw_box(Rect::from_xywh(0.0f, 0.0f, w, h), track_bg, track_border, 6.0f, 1.0f);

        // 2. 独立 steady_clock 指数阻尼平滑滑块 (消除 FrameClock 抖动与取整跳帧)
        auto now = std::chrono::steady_clock::now();
        float dt = 1.0f / 60.0f;
        if (m_has_last_frame) {
            dt = std::clamp(std::chrono::duration<float>(now - m_last_frame).count(), 0.001f, 0.05f);
        }
        m_last_frame = now;
        m_has_last_frame = true;

        Rect target = segment_rect(m_selected);
        if (!m_pill_init) {
            m_pill_x = target.min_x;
            m_pill_w = target.width();
            m_pill_init = true;
        } else if (std::abs(m_pill_x - target.min_x) > 0.15f || std::abs(m_pill_w - target.width()) > 0.15f) {
            float k = 1.0f - std::exp(-dt * 24.0f);
            m_pill_x += (target.min_x - m_pill_x) * k;
            m_pill_w += (target.width() - m_pill_w) * k;
            mark_needs_paint();
        } else {
            m_pill_x = target.min_x;
            m_pill_w = target.width();
            m_has_last_frame = false;
        }

        // 3. 非激活相邻分段之间的柔和分隔竖线
        Color sep_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
        for (int i = 1; i < static_cast<int>(m_items.size()); ++i) {
            if (i == m_selected || i - 1 == m_selected || i == m_hovered || i - 1 == m_hovered) continue;
            Rect sr = segment_rect(i);
            ctx.draw_rect(Rect::from_xywh(std::round(sr.min_x), std::round((h - 14.0f) * 0.5f), 1.0f, 14.0f),
                          sep_col, 0.0f, 0.0f);
        }

        // 4. Hover / Press 柔和背景反馈
        if (m_hovered >= 0 && m_hovered != m_selected && m_hovered < static_cast<int>(m_items.size())) {
            Color hbg = (m_pressed == m_hovered) ? theme.colors.subtle_fill_tertiary : theme.colors.subtle_fill_secondary;
            ctx.draw_rect(segment_rect(m_hovered), hbg, 4.0f, 0.0f);
        }

        // 5. Fluent 2 轻质悬浮滑块 (无冗余内描边/底部小短条，仅以表面层级与微高光区分)
        Rect pill = Rect::from_xywh(m_pill_x, 2.0f, m_pill_w, h - 4.0f);
        if (!theme.is_dark) {
            ctx.draw_rect(Rect::from_xywh(pill.min_x, pill.min_y + 1.0f, pill.width(), pill.height()),
                          Color{0.0f, 0.0f, 0.0f, 0.06f}, 4.5f, 0.0f);
        }
        Color pill_bg = theme.is_dark
            ? (m_pressed == m_selected ? Color{1.0f, 1.0f, 1.0f, 0.095f} : Color{1.0f, 1.0f, 1.0f, 0.125f})
            : (m_pressed == m_selected ? Color{1.0f, 1.0f, 1.0f, 0.90f}  : Color{1.0f, 1.0f, 1.0f, 0.99f});
        ctx.draw_rect(pill, pill_bg, 4.5f, 0.0f);
        if (theme.is_dark) {
            ctx.draw_rect(Rect::from_xywh(pill.min_x + 4.0f, pill.min_y + 0.5f,
                                          std::max(0.0f, pill.width() - 8.0f), 1.0f),
                          Color{1.0f, 1.0f, 1.0f, 0.10f}, 0.0f, 0.0f);
        }

        Color active_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.96f} : Color{0.0f, 0.0f, 0.0f, 0.92f};
        Color hover_c  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.84f} : Color{0.0f, 0.0f, 0.0f, 0.80f};
        Color idle_c   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.64f} : Color{0.0f, 0.0f, 0.0f, 0.60f};

        for (size_t i = 0; i < m_items.size(); ++i) {
            int idx = static_cast<int>(i);
            Rect sr = segment_rect(idx);
            const auto& it = m_items[i];
            bool is_sel = (idx == m_selected);
            Color tc = is_sel ? active_c : (idx == m_hovered ? hover_c : idle_c);
            Vec2 ts = TextMeasure::measure(it.label, 13.0f);
            float content_w = ts.x + (it.svg_path.empty() ? 0.0f : 20.0f);
            float sx = std::round(sr.min_x + (sr.width() - content_w) * 0.5f);
            if (!it.svg_path.empty()) {
                Rect ir = Rect::from_xywh(sx, std::round((h - 14.0f) * 0.5f), 14.0f, 14.0f);
                SvgEngine::draw_svg_path(ctx, it.svg_path, 24.0f, 24.0f, ir, tc);
                sx += 20.0f;
            }
            ctx.draw_text(it.label, PointF{sx, std::round((h - ts.y) * 0.5f)}, 13.0f, tc,
                          is_sel ? FontWeight::SemiBold : FontWeight::Regular);
        }
    }

private:
    Rect segment_rect(int idx) const noexcept {
        if (m_items.empty()) return Rect{};
        float inner_w = std::max(1.0f, size().x - 4.0f);
        float seg_w = inner_w / static_cast<float>(m_items.size());
        return Rect::from_xywh(2.0f + static_cast<float>(idx) * seg_w, 2.0f, seg_w, size().y - 4.0f);
    }

    int hit_segment(Vec2 local) const noexcept {
        if (m_items.empty() || local.x < 2.0f || local.x > size().x - 2.0f) return -1;
        float seg_w = std::max(1.0f, size().x - 4.0f) / static_cast<float>(m_items.size());
        int idx = static_cast<int>((local.x - 2.0f) / seg_w);
        return (idx >= 0 && idx < static_cast<int>(m_items.size())) ? idx : -1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int h = hit_segment(e.local_position);
            if (h != m_hovered) { m_hovered = h; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered != -1 || m_pressed != -1) { m_hovered = -1; m_pressed = -1; mark_needs_paint(); }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int idx = hit_segment(e.local_position);
            if (idx >= 0) {
                m_pressed = idx;
                select(idx);
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent&) {
            if (m_pressed != -1) { m_pressed = -1; mark_needs_paint(); }
        };
    }

    std::vector<SegmentedItem> m_items;
    int m_selected{0};
    int m_hovered{-1};
    int m_pressed{-1};
    float m_width{0.0f};
    bool m_pill_init{false};
    float m_pill_x{2.0f};
    float m_pill_w{60.0f};
    bool m_has_last_frame{false};
    std::chrono::steady_clock::time_point m_last_frame{};
    Fn<void(int)> m_on_selected;

public:
    void wrap_on_selected(Fn<void(int)> extra) {
        auto prev = std::move(m_on_selected);
        m_on_selected = [prev = std::move(prev), extra = std::move(extra)](int idx) {
            if (extra) extra(idx);
            if (prev) prev(idx);
        };
    }
};

struct SegmentedControl : BindableMixin<SegmentedControl, RenderSegmentedControl> {
    std::vector<SegmentedItem> items;
    int selected_index{0};
    float width{0.0f};
    Fn<void(int)> on_selected_index_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderSegmentedControl>& node, Property<int>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_selected_index(prop.get());
    std::weak_ptr<RenderSegmentedControl> weak = node;
    bg.subscribe(prop, [weak, guard](int idx) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_selected_index(idx);
        }
    });
    Property<int>* p = &prop;
    node->wrap_on_selected([p, guard](int idx) {
        *guard = true;
        p->set(idx);
        *guard = false;
    });
}

class SegmentedControlWidget : public Widget {
public:
    explicit SegmentedControlWidget(const SegmentedControl& desc) : m_desc(desc) {}
    explicit SegmentedControlWidget(SegmentedControl&& desc) noexcept : m_desc(std::move(desc)) {}
    ~SegmentedControlWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderSegmentedControl>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderSegmentedControl>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderSegmentedControl>& node) {
        node->set_properties(m_desc.items, m_desc.selected_index, m_desc.width, m_desc.on_selected_index_changed);
    }

    SegmentedControl m_desc;
};

inline SegmentedControl::operator WidgetPtr() const& {
    return make_widget<SegmentedControlWidget>(*this);
}
inline SegmentedControl::operator WidgetPtr() && {
    return make_widget<SegmentedControlWidget>(std::move(*this));
}

} // namespace ktui
