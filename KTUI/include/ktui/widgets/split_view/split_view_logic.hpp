#pragma once

#include "split_view_data.hpp"
#include "split_view_style.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {


// 可拖拽分隔面板渲染盒 (RenderSplitView)
// 支持水平/垂直双栏分割、悬停/拖拽高亮分隔条、ResizeEW/ResizeNS 光标、最小尺寸保护与双击重置
class RenderSplitView : public RenderBox {
public:
    RenderSplitView(bool vertical = false,
                    float ratio = 0.35f,
                    float min_first = 80.0f,
                    float min_second = 80.0f,
                    float splitter_size = 8.0f,
                    Fn<void(float)> on_ratio_changed = nullptr) noexcept
        : m_vertical(vertical),
          m_ratio(std::clamp(ratio, 0.05f, 0.95f)),
          m_default_ratio(std::clamp(ratio, 0.05f, 0.95f)),
          m_min_first(std::max(0.0f, min_first)),
          m_min_second(std::max(0.0f, min_second)),
          m_splitter_size(std::max(4.0f, splitter_size)),
          m_on_ratio_changed(std::move(on_ratio_changed)) {
        setup_behavior();
    }

    void set_vertical(bool v) noexcept {
        if (m_vertical != v) { m_vertical = v; mark_needs_layout(); }
    }
    void set_ratio(float r) noexcept {
        float clamped = std::clamp(r, 0.05f, 0.95f);
        if (std::abs(m_ratio - clamped) > 0.0005f) {
            m_ratio = clamped;
            mark_needs_layout();
        }
    }
    [[nodiscard]] float ratio() const noexcept { return m_ratio; }

    void set_limits(float min_first, float min_second, float splitter_size) noexcept {
        m_min_first = std::max(0.0f, min_first);
        m_min_second = std::max(0.0f, min_second);
        m_splitter_size = std::max(4.0f, splitter_size);
        mark_needs_layout();
    }

    void set_on_ratio_changed(Fn<void(float)> cb) {
        m_on_ratio_changed = std::move(cb);
    }

    void wrap_on_ratio_changed(Fn<void(float)> extra) {
        auto prev = std::move(m_on_ratio_changed);
        m_on_ratio_changed = [prev = std::move(prev), extra = std::move(extra)](float r) {
            if (prev) prev(r);
            if (extra) extra(r);
        };
    }

    void set_panes(std::shared_ptr<RenderBox> first, std::shared_ptr<RenderBox> second) {
        while (first_child()) remove_child(first_child());
        m_first = std::move(first);
        m_second = std::move(second);
        if (m_first) append_child(m_first.get());
        if (m_second) append_child(m_second.get());
        mark_needs_layout();
    }

    [[nodiscard]] float first_extent() const noexcept { return m_first_extent; }
    [[nodiscard]] float second_extent() const noexcept { return m_second_extent; }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(m_constraints.min_width, 400.0f);
        float h = m_constraints.has_bounded_height() ? m_constraints.max_height : std::max(m_constraints.min_height, 200.0f);
        m_size = m_constraints.constrain(Vec2{w, h});

        const float total_main = m_vertical ? m_size.y : m_size.x;
        const float cross = m_vertical ? m_size.x : m_size.y;
        const float usable = std::max(0.0f, total_main - m_splitter_size);

        float first_len = usable * m_ratio;
        if (usable >= m_min_first + m_min_second) {
            first_len = std::clamp(first_len, m_min_first, usable - m_min_second);
        } else if (m_min_first + m_min_second > 0.0f) {
            first_len = usable * (m_min_first / (m_min_first + m_min_second));
        }
        float second_len = std::max(0.0f, usable - first_len);

        m_first_extent = first_len;
        m_second_extent = second_len;

        if (m_vertical) {
            m_splitter_rect = Rect::from_xywh(0.0f, first_len, cross, m_splitter_size);
            if (m_first) {
                m_first->layout(BoxConstraints::tight(Vec2{cross, first_len}), true);
                m_first->set_offset(Vec2{0.0f, 0.0f});
            }
            if (m_second) {
                m_second->layout(BoxConstraints::tight(Vec2{cross, second_len}), true);
                m_second->set_offset(Vec2{0.0f, first_len + m_splitter_size});
            }
        } else {
            m_splitter_rect = Rect::from_xywh(first_len, 0.0f, m_splitter_size, cross);
            if (m_first) {
                m_first->layout(BoxConstraints::tight(Vec2{first_len, cross}), true);
                m_first->set_offset(Vec2{0.0f, 0.0f});
            }
            if (m_second) {
                m_second->layout(BoxConstraints::tight(Vec2{second_len, cross}), true);
                m_second->set_offset(Vec2{first_len + m_splitter_size, 0.0f});
            }
        }
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        // 分隔条优先命中并动态切换光标
        if (m_dragging || m_splitter_rect.contains(position)) {
            cursor_shape = m_vertical ? CursorShape::ResizeNS : CursorShape::ResizeEW;
            result.add(this, position);
            return true;
        }
        cursor_shape = CursorShape::Arrow;
        return RenderBox::hit_test(result, position);
    }

    // 公共可复用 Fluent 2 分隔条绘制（供 SplitView 与 DockPanel 统一复用）
    static void paint_splitter(PaintContext& context,
                               Rect splitter_rect,
                               bool vertical,
                               bool hovered,
                               bool dragging,
                               bool draw_resting_hairline = false) {
        const Theme& t = ThemeContext::current();
        const auto resolved_style = SplitViewStyleResolver::resolve(t);
        const bool active = hovered || dragging;

        if (draw_resting_hairline && !active) {
            Color hairline = t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
            if (vertical) {
                context.draw_rect(Rect::from_xywh(splitter_rect.min_x, std::round(splitter_rect.center().y),
                                                  splitter_rect.width(), 1.0f), hairline, 0.0f, 0.0f);
            } else {
                context.draw_rect(Rect::from_xywh(std::round(splitter_rect.center().x), splitter_rect.min_y,
                                                  1.0f, splitter_rect.height()), hairline, 0.0f, 0.0f);
            }
        }

        Color track_c = active
            ? Color{resolved_style.accent.r, resolved_style.accent.g, resolved_style.accent.b, dragging ? 0.22f : 0.12f}
            : Color::transparent();
        if (track_c.a > 0.001f) {
            context.draw_rect(splitter_rect, track_c, 3.0f, 0.0f);
        }

        Color pill_c = active
            ? resolved_style.accent
            : (t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.20f} : Color{0.0f, 0.0f, 0.0f, 0.20f});

        if (vertical) {
            float pill_w = std::min(36.0f, std::max(12.0f, splitter_rect.width() * 0.2f));
            float pill_h = active ? 3.0f : 2.0f;
            Rect pill = Rect::from_xywh(
                splitter_rect.min_x + (splitter_rect.width() - pill_w) * 0.5f,
                splitter_rect.min_y + (splitter_rect.height() - pill_h) * 0.5f,
                pill_w, pill_h);
            context.draw_rect(pill, pill_c, pill_h * 0.5f, 0.0f);
        } else {
            float pill_w = active ? 3.0f : 2.0f;
            float pill_h = std::min(36.0f, std::max(12.0f, splitter_rect.height() * 0.2f));
            Rect pill = Rect::from_xywh(
                splitter_rect.min_x + (splitter_rect.width() - pill_w) * 0.5f,
                splitter_rect.min_y + (splitter_rect.height() - pill_h) * 0.5f,
                pill_w, pill_h);
            context.draw_rect(pill, pill_c, pill_w * 0.5f, 0.0f);
        }
    }

    void paint(PaintContext& context) override {
        // 1. 绘制左右/上下两个子面板 (分别施加裁剪严防拖拽收缩时内容溢出)
        if (m_first) {
            context.push_clip(Rect::from_xywh(m_first->offset().x, m_first->offset().y, m_first->size().x, m_first->size().y), 0.0f);
            {
                PaintContext::OffsetScope sc(context, m_first->offset());
                m_first->paint(context);
            }
            context.pop_clip();
        }
        if (m_second) {
            context.push_clip(Rect::from_xywh(m_second->offset().x, m_second->offset().y, m_second->size().x, m_second->size().y), 0.0f);
            {
                PaintContext::OffsetScope sc(context, m_second->offset());
                m_second->paint(context);
            }
            context.pop_clip();
        }

        // 2. 绘制分隔条底座与拖拽胶囊把手
        paint_splitter(context, m_splitter_rect, m_vertical, m_hovered, m_dragging, false);
    }

private:
    void apply_drag_pos(PointF local_pos) {
        const float total_main = m_vertical ? m_size.y : m_size.x;
        const float usable = std::max(1.0f, total_main - m_splitter_size);
        float coord = (m_vertical ? local_pos.y : local_pos.x) - m_splitter_size * 0.5f;
        if (usable >= m_min_first + m_min_second) {
            coord = std::clamp(coord, m_min_first, usable - m_min_second);
        } else {
            coord = std::clamp(coord, 0.0f, usable);
        }
        float next_ratio = std::clamp(coord / usable, 0.05f, 0.95f);
        if (std::abs(next_ratio - m_ratio) > 0.0005f) {
            m_ratio = next_ratio;
            mark_needs_layout();
            if (m_on_ratio_changed) m_on_ratio_changed(m_ratio);
        }
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_hovered) { m_hovered = true; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered) { m_hovered = false; mark_needs_paint(); }
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            if (!m_splitter_rect.contains(ev.local_position)) return;
            auto now = std::chrono::steady_clock::now();
            auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
            m_last_click_time = now;
            if (dt_ms > 0 && dt_ms < 320) {
                // 双击分隔条重置为默认比例
                set_ratio(m_default_ratio);
                if (m_on_ratio_changed) m_on_ratio_changed(m_ratio);
                ev.stop_propagation();
                return;
            }
            m_dragging = true;
            mark_needs_paint();
            ev.stop_propagation();
        };
        on_pointer_move = [this](PointerEvent& ev) {
            if (m_dragging) {
                apply_drag_pos(ev.local_position);
                ev.stop_propagation();
            } else {
                bool on_handle = m_splitter_rect.contains(ev.local_position);
                if (m_hovered != on_handle) {
                    m_hovered = on_handle;
                    mark_needs_paint();
                }
            }
        };
        on_pointer_up = [this](PointerEvent& ev) {
            if (m_dragging) {
                m_dragging = false;
                mark_needs_paint();
                ev.stop_propagation();
            }
        };
    }

    bool m_vertical{false};
    float m_ratio{0.35f};
    float m_default_ratio{0.35f};
    float m_min_first{80.0f};
    float m_min_second{80.0f};
    float m_splitter_size{8.0f};
    float m_first_extent{0.0f};
    float m_second_extent{0.0f};
    Rect m_splitter_rect{};
    bool m_hovered{false};
    bool m_dragging{false};
    std::chrono::steady_clock::time_point m_last_click_time{};
    Fn<void(float)> m_on_ratio_changed{nullptr};
    std::shared_ptr<RenderBox> m_first;
    std::shared_ptr<RenderBox> m_second;
};

struct SplitView : BindableMixin<SplitView, RenderSplitView> {
    bool vertical{false};
    float ratio{0.35f};
    float min_first{80.0f};
    float min_second{80.0f};
    float splitter_size{8.0f};
    WidgetPtr first{nullptr};
    WidgetPtr second{nullptr};
    Fn<void(float)> on_ratio_changed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderSplitView>& node, Property<float>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_ratio(prop.get());
    std::weak_ptr<RenderSplitView> weak = node;
    bg.subscribe(prop, [weak, guard](float r) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_ratio(r);
        }
    });
    Property<float>* p = &prop;
    node->wrap_on_ratio_changed([p, guard](float r) {
        *guard = true;
        p->set(r);
        *guard = false;
    });
}

class SplitViewWidget : public Widget {
public:
    explicit SplitViewWidget(const SplitView& desc) : m_desc(desc) {}
    explicit SplitViewWidget(SplitView&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderSplitView>(
            m_desc.vertical, m_desc.ratio, m_desc.min_first, m_desc.min_second,
            m_desc.splitter_size, m_desc.on_ratio_changed);
        m_first_box = m_desc.first ? m_desc.first->create_render_node() : nullptr;
        m_second_box = m_desc.second ? m_desc.second->create_render_node() : nullptr;
        if (m_desc.first && m_first_box) m_desc.first->set_render_node(m_first_box);
        if (m_desc.second && m_second_box) m_desc.second->set_render_node(m_second_box);
        node->set_panes(m_first_box, m_second_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sv = std::static_pointer_cast<RenderSplitView>(node)) {
            sv->set_vertical(m_desc.vertical);
            sv->set_ratio(m_desc.ratio);
            sv->set_limits(m_desc.min_first, m_desc.min_second, m_desc.splitter_size);
            sv->set_on_ratio_changed(m_desc.on_ratio_changed);
            auto sync = [](WidgetPtr& w, std::shared_ptr<RenderBox>& box) {
                if (w) {
                    if (!box) { box = w->create_render_node(); w->set_render_node(box); }
                    else { w->update_render_node(box); }
                } else {
                    box.reset();
                }
            };
            sync(m_desc.first, m_first_box);
            sync(m_desc.second, m_second_box);
            sv->set_panes(m_first_box, m_second_box);
        }
    }

private:
    SplitView m_desc;
    std::shared_ptr<RenderBox> m_first_box;
    std::shared_ptr<RenderBox> m_second_box;
};

inline SplitView::operator WidgetPtr() const& { return make_widget<SplitViewWidget>(*this); }
inline SplitView::operator WidgetPtr() && { return make_widget<SplitViewWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
