#pragma once

#include "flip_view_data.hpp"
#include "flip_view_style.hpp"
#include "../common/fluent_icons.hpp"
#include <ktui/animation/frame_clock.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <cmath>

namespace ktui {


// WinUI 3 FlipView 轮播视图渲染盒
// 支持左右悬浮翻页箭头、触控左右滑动切页与 FrameClock 驱动的平滑位移过渡动画
class RenderFlipView : public RenderBox {
public:
    RenderFlipView() noexcept {
        setup_behavior();
    }

    ~RenderFlipView() override {
        clear_items();
    }

    void set_properties(const std::vector<WidgetPtr>& items,
                        int selected_index,
                        float width,
                        float height,
                        Fn<void(int)> on_selected_changed) {
        clear_items();
        for (const auto& w : items) {
            if (!w) continue;
            auto node = w->create_render_node();
            if (node) {
                append_child(node.get());
                m_item_nodes.push_back(std::move(node));
            }
        }
        int max_idx = std::max(0, static_cast<int>(m_item_nodes.size()) - 1);
        m_selected = std::clamp(selected_index, 0, max_idx);
        m_slide_pos = static_cast<float>(m_selected);
        m_width = width;
        m_height = height;
        m_on_selected = std::move(on_selected_changed);
        mark_needs_layout();
    }

    void set_selected_index(int idx) {
        int max_idx = std::max(0, static_cast<int>(m_item_nodes.size()) - 1);
        int clamped = std::clamp(idx, 0, max_idx);
        if (m_selected != clamped) {
            m_selected = clamped;
            mark_needs_paint();
        }
    }

    [[nodiscard]] int selected_index() const noexcept { return m_selected; }
    [[nodiscard]] size_t item_count() const noexcept { return m_item_nodes.size(); }

    void go_to(int idx) {
        int max_idx = std::max(0, static_cast<int>(m_item_nodes.size()) - 1);
        int clamped = std::clamp(idx, 0, max_idx);
        if (clamped != m_selected) {
            m_selected = clamped;
            mark_needs_paint();
            if (m_on_selected) m_on_selected(m_selected);
        }
    }

    void perform_layout() override {
        float w = (m_width > 0.0f)
            ? m_width
            : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 480.0f);
        float h = (m_height > 0.0f) ? m_height : 200.0f;
        set_size(m_constraints.constrain(Vec2{w, h}));

        BoxConstraints item_c = BoxConstraints::tight(size());
        for (auto& node : m_item_nodes) {
            if (node) node->layout(item_c, false);
        }
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        // 统一 FrameClock 驱动平滑水平滑动
        float target = static_cast<float>(m_selected);
        if (std::abs(m_slide_pos - target) > 0.002f) {
            float dt = std::clamp(FrameClock::global().dt(), 1.0f / 240.0f, 0.05f);
            float t = std::min(1.0f, dt * 14.0f);
            m_slide_pos += (target - m_slide_pos) * t;
            if (std::abs(m_slide_pos - target) <= 0.002f) m_slide_pos = target;
            mark_needs_paint();
        }

        Rect panel = Rect::from_xywh(0.0f, 0.0f, w, h);
        ctx.draw_rect(panel, theme.colors.card_surface, 8.0f, 0.0f);
        ctx.draw_rect(panel, theme.colors.card_stroke, 8.0f, 1.0f);

        ctx.push_clip(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, w - 2.0f), std::max(0.0f, h - 2.0f)), 7.0f);
        for (size_t i = 0; i < m_item_nodes.size(); ++i) {
            float ox = (static_cast<float>(i) - m_slide_pos) * w;
            if (ox + w < -2.0f || ox > w + 2.0f) continue;
            m_item_nodes[i]->set_offset(Vec2{std::round(ox), 0.0f});
            PaintContext::OffsetScope scope(ctx, m_item_nodes[i]->offset());
            m_item_nodes[i]->paint(ctx);
        }

        // 左右轻量翻页箭头 (无大深色底板，仅在悬停箭头本身时呈现柔和微透明高亮)
        if (m_hovered && m_item_nodes.size() > 1) {
            Color btn_hv = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.16f} : Color{0.0f, 0.0f, 0.0f, 0.10f};
            Color ic     = Color{1.0f, 1.0f, 1.0f, 0.88f};
            Color ic_hv  = Color{1.0f, 1.0f, 1.0f, 1.0f};

            if (m_selected > 0) {
                Rect pr = prev_btn_rect();
                if (m_hover_prev) ctx.draw_rect(pr, btn_hv, 4.0f, 0.0f);
                fluent_icons::draw_centered(ctx, fluent_icons::kChevronLeft, pr, 14.0f, m_hover_prev ? ic_hv : ic);
            }
            if (m_selected + 1 < static_cast<int>(m_item_nodes.size())) {
                Rect nr = next_btn_rect();
                if (m_hover_next) ctx.draw_rect(nr, btn_hv, 4.0f, 0.0f);
                fluent_icons::draw_centered(ctx, fluent_icons::kChevronRight, nr, 14.0f, m_hover_next ? ic_hv : ic);
            }
        }
        ctx.pop_clip();
    }

private:
    void clear_items() {
        for (auto& n : m_item_nodes) {
            if (n && n->parent() == this) remove_child(n.get());
        }
        m_item_nodes.clear();
    }

    Rect prev_btn_rect() const noexcept {
        return Rect::from_xywh(8.0f, std::round((size().y - 36.0f) * 0.5f), 24.0f, 36.0f);
    }

    Rect next_btn_rect() const noexcept {
        return Rect::from_xywh(size().x - 32.0f, std::round((size().y - 36.0f) * 0.5f), 24.0f, 36.0f);
    }

    void setup_behavior() {
        on_pointer_preview = [this](PointerEvent& e) {
            if (e.device_kind != PointerDeviceKind::Touch) return;
            if (e.type == PlatformEventType::PointerDown) {
                m_touch_start_x = e.local_position.x;
                m_touch_start_y = e.local_position.y;
                m_touch_active = true;
            } else if (e.type == PlatformEventType::PointerMove && m_touch_active) {
                float dx = e.local_position.x - m_touch_start_x;
                float dy = e.local_position.y - m_touch_start_y;
                if (std::abs(dx) > touch::kTouchSlopPx && touch::is_horizontal_drag(dx, dy)) {
                    e.gesture_capture_target = this;
                    e.stop_propagation();
                }
            }
        };
        on_pointer_enter = [this](PointerEvent&) { m_hovered = true; mark_needs_paint(); };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = m_hover_prev = m_hover_next = false;
            mark_needs_paint();
        };
        on_pointer_move = [this](PointerEvent& e) {
            if (e.is_touch()) return;
            bool hp = (m_selected > 0) && prev_btn_rect().contains(e.local_position);
            bool hn = (m_selected + 1 < static_cast<int>(m_item_nodes.size())) && next_btn_rect().contains(e.local_position);
            cursor_shape = (hp || hn) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (hp != m_hover_prev || hn != m_hover_next) {
                m_hover_prev = hp;
                m_hover_next = hn;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            Rect prev_hit = e.is_touch() ? touch::expand_hit_rect(prev_btn_rect(), 44.0f) : prev_btn_rect();
            Rect next_hit = e.is_touch() ? touch::expand_hit_rect(next_btn_rect(), 44.0f) : next_btn_rect();
            if (m_selected > 0 && prev_hit.contains(e.local_position)) {
                go_to(m_selected - 1);
                m_touch_active = false;
                e.stop_propagation();
                return;
            }
            if (m_selected + 1 < static_cast<int>(m_item_nodes.size()) && next_hit.contains(e.local_position)) {
                go_to(m_selected + 1);
                m_touch_active = false;
                e.stop_propagation();
                return;
            }
            if (e.device_kind == PointerDeviceKind::Touch) {
                m_touch_start_x = e.local_position.x;
                m_touch_start_y = e.local_position.y;
                m_touch_active = true;
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (m_touch_active && e.device_kind == PointerDeviceKind::Touch) {
                m_touch_active = false;
                float dx = e.local_position.x - m_touch_start_x;
                if (dx > 36.0f) go_to(m_selected - 1);
                else if (dx < -36.0f) go_to(m_selected + 1);
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_touch_active = false;
            m_hovered = m_hover_prev = m_hover_next = false;
            mark_needs_paint();
        };
    }

    std::vector<std::shared_ptr<RenderBox>> m_item_nodes;
    int m_selected{0};
    float m_slide_pos{0.0f};
    float m_width{0.0f};
    float m_height{200.0f};
    bool m_hovered{false};
    bool m_hover_prev{false};
    bool m_hover_next{false};
    bool m_touch_active{false};
    float m_touch_start_x{0.0f};
    float m_touch_start_y{0.0f};
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

struct FlipView : BindableMixin<FlipView, RenderFlipView> {
    std::vector<WidgetPtr> items;
    int selected_index{0};
    float width{0.0f};
    float height{200.0f};
    Fn<void(int)> on_selected_index_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderFlipView>& node, Property<int>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_selected_index(prop.get());
    std::weak_ptr<RenderFlipView> weak = node;
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

class FlipViewWidget : public Widget {
public:
    explicit FlipViewWidget(const FlipView& desc) : m_desc(desc) {}
    explicit FlipViewWidget(FlipView&& desc) noexcept : m_desc(std::move(desc)) {}
    ~FlipViewWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderFlipView>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderFlipView>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderFlipView>& node) {
        node->set_properties(m_desc.items, m_desc.selected_index, m_desc.width, m_desc.height, m_desc.on_selected_index_changed);
    }

    FlipView m_desc;
};

inline FlipView::operator WidgetPtr() const& {
    return make_widget<FlipViewWidget>(*this);
}
inline FlipView::operator WidgetPtr() && {
    return make_widget<FlipViewWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
