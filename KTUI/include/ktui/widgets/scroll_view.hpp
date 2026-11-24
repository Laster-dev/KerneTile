#pragma once

#include "scroll/smooth_scroll_physics.hpp"
#include "scroll/scrollbar.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace ktui {

// 通用垂直滚动容器渲染盒 (RenderScrollView)
// 内置 Chromium / Edge Impulse 平滑滚动算法 + WinUI 3 像素级对齐自动展开滚动条
class RenderScrollView : public RenderBox {
public:
    RenderScrollView() noexcept {
        setup_behavior();
    }

    ~RenderScrollView() override = default;

    [[nodiscard]] float scroll_offset() const noexcept { return m_physics.offset(); }
    [[nodiscard]] float max_scroll() const noexcept { return m_physics.max_scroll(); }
    [[nodiscard]] float content_height() const noexcept { return m_content_height; }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child_box == child) return;
        if (m_child_box) {
            remove_child(m_child_box.get());
        }
        m_child_box = std::move(child);
        if (m_child_box) {
            append_child(m_child_box.get());
        }
        mark_needs_layout();
    }

    [[nodiscard]] const std::shared_ptr<RenderBox>& child_box() const noexcept { return m_child_box; }

    void set_dimensions(float w, float h, float max_h) {
        bool changed = false;
        if (std::abs(m_width - w) > 0.001f) { m_width = w; changed = true; }
        if (std::abs(m_height - h) > 0.001f) { m_height = h; changed = true; }
        if (std::abs(m_max_height - max_h) > 0.001f) { m_max_height = max_h; changed = true; }
        if (changed) mark_needs_layout();
    }

    void scroll_to(float offset) {
        m_physics.scroll_to(offset);
        m_scrollbar.notify_activity();
        mark_needs_paint();
    }

    void scroll_by(float delta_px) {
        m_physics.scroll_by(delta_px);
        m_scrollbar.notify_activity();
        mark_needs_paint();
    }

    void perform_layout() override {
        float target_w = m_width > 0.0f ? m_constraints.constrain_width(m_width)
                                        : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 240.0f);

        float child_h = 0.0f;
        if (auto* child = dynamic_cast<RenderBox*>(first_child())) {
            BoxConstraints child_c;
            child_c.min_width = target_w;
            child_c.max_width = target_w;
            child_c.min_height = 0.0f;
            child_c.max_height = std::numeric_limits<float>::infinity();
            child->layout(child_c);
            child->set_offset(Vec2{0.0f, 0.0f});
            child_h = child->size().y;
        }
        m_content_height = child_h;

        float desired_h = m_height > 0.0f ? m_height : child_h;
        if (m_max_height > 0.0f) {
            desired_h = std::min(desired_h, m_max_height);
        }
        float final_h = m_constraints.constrain_height(desired_h);
        set_size(Vec2{target_w, final_h});
        m_physics.set_max_scroll(std::max(0.0f, m_content_height - final_h));
        if (m_child_box) {
            m_child_box->set_offset(Vec2{0.0f, -m_physics.offset()});
        }
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (local_position.x < 0.0f || local_position.y < 0.0f ||
            local_position.x > size().x || local_position.y > size().y) {
            return false;
        }

        // 若命中滚动条区域，直接由 ScrollView 自身捕获，避免误触底层子控件
        Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
        auto m = m_scrollbar.compute_metrics(vp, m_content_height, m_physics.offset());
        if (m_scrollbar.hit_test(local_position, m) != WinUIScrollBar::HitPart::None) {
            result.add(this, local_position);
            return true;
        }

        // 否则转换到滚动后坐标命中测试子节点
        if (m_child_box) {
            PointF child_pos{local_position.x, local_position.y + m_physics.offset()};
            m_child_box->hit_test(result, child_pos);
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        bool anim_scroll = m_physics.update();
        bool anim_bar = m_scrollbar.update_animations(m_viewport_hovered);
        if (anim_scroll || anim_bar) {
            mark_needs_paint();
        }

        Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
        ctx.push_clip(vp, 0.0f);
        if (m_child_box) {
            // 同步更新子节点的 offset，使内部 ComboBox 弹出定位、Slider 拖拽、TextInput IME 坐标全局精确
            m_child_box->set_offset(Vec2{0.0f, -m_physics.offset()});
            PaintContext::OffsetScope scroll_scope(ctx, m_child_box->offset());
            m_child_box->paint(ctx);
        }
        m_scrollbar.paint(ctx, vp, m_content_height, m_physics.offset(), ThemeContext::current());
        ctx.pop_clip();
    }

private:
    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            m_viewport_hovered = true;
            m_scrollbar.notify_activity();
            mark_needs_paint();
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& e) {
            Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
            if (e.type == PlatformEventType::PointerScroll) {
                if (m_physics.max_scroll() > 0.0f) {
                    m_physics.add_wheel_delta(e.delta.y);
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.stop_propagation();
                }
                return;
            }
            bool consumed = false;
            if (m_scrollbar.handle_pointer_move(e.local_position, vp, m_content_height, m_physics, consumed)) {
                mark_needs_paint();
            }
            if (consumed) {
                e.stop_propagation();
            }
        };

        on_pointer_down = [this](PointerEvent& e) {
            Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
            if (m_scrollbar.handle_pointer_down(e.local_position, vp, m_content_height, m_physics)) {
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (m_scrollbar.handle_pointer_up()) {
                mark_needs_paint();
                e.stop_propagation();
            }
        };
    }

    float m_width{0.0f};
    float m_height{0.0f};
    float m_max_height{0.0f};
    float m_content_height{0.0f};
    bool m_viewport_hovered{false};
    std::shared_ptr<RenderBox> m_child_box;
    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;
};

// C++20 指定初始化器声明式滚动容器 (ScrollView DSL Wrapper)
struct ScrollView {
    float width{0.0f};
    float height{0.0f};
    float max_height{0.0f};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const;
};

class ScrollViewWidget : public Widget {
public:
    explicit ScrollViewWidget(const ScrollView& desc) : m_desc(desc) {}
    ~ScrollViewWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderScrollView>();
        node->set_dimensions(m_desc.width, m_desc.height, m_desc.max_height);
        if (m_desc.child) {
            node->set_child_box(m_desc.child->create_render_node());
        }
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sv = std::dynamic_pointer_cast<RenderScrollView>(node)) {
            sv->set_dimensions(m_desc.width, m_desc.height, m_desc.max_height);
            if (m_desc.child) {
                if (sv->child_box()) {
                    m_desc.child->update_render_node(sv->child_box());
                } else {
                    sv->set_child_box(m_desc.child->create_render_node());
                }
            }
        }
    }

private:
    ScrollView m_desc;
};

inline ScrollView::operator WidgetPtr() const {
    return std::make_shared<ScrollViewWidget>(*this);
}

} // namespace ktui
