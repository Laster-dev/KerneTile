#pragma once

#include "scroll_view_data.hpp"
#include "scroll_view_style.hpp"
#include "../scroll/smooth_scroll_physics.hpp"
#include "../scroll/scrollbar.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <cmath>

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

    void set_rubber_band_enabled(bool enabled) noexcept {
        if (m_physics.is_rubber_band_enabled() != enabled) {
            m_physics.set_rubber_band_enabled(enabled);
            if (m_child_box) {
                m_child_box->set_offset(Vec2{0.0f, -m_physics.offset()});
            }
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool is_rubber_band_enabled() const noexcept { return m_physics.is_rubber_band_enabled(); }
    [[nodiscard]] bool rubber_band_enabled() const noexcept { return m_physics.rubber_band_enabled(); }

    void set_edge_feather(bool enabled, float feather_px = 28.0f) noexcept {
        const float f = std::max(0.0f, feather_px);
        if (m_edge_feather != enabled || std::abs(m_feather_size - f) > 0.01f) {
            m_edge_feather = enabled;
            m_feather_size = f;
            mark_needs_paint();
        }
    }
    void set_edge_feather_enabled(bool enabled) noexcept {
        set_edge_feather(enabled, m_feather_size);
    }
    void set_feather_size(float feather_px) noexcept {
        set_edge_feather(m_edge_feather, feather_px);
    }
    [[nodiscard]] bool edge_feather_enabled() const noexcept { return m_edge_feather; }
    [[nodiscard]] float feather_size() const noexcept { return m_feather_size; }

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
        if (auto* child = (first_child() ? first_child()->as_render_box() : nullptr)) {
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
        m_physics.set_viewport_extent(final_h);
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

    bool hit_test_touch(HitTestResult& result, PointF local_position) override {
        if (!bounds().contains(local_position)) return false;
        Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
        auto m = m_scrollbar.compute_metrics(vp, m_content_height, m_physics.offset());
        if (m_scrollbar.hit_test(local_position, m, PointerDeviceKind::Touch) != WinUIScrollBar::HitPart::None) {
            result.add(this, local_position);
            return true;
        }
        if (m_child_box) {
            PointF child_pos{local_position.x, local_position.y + m_physics.offset()};
            m_child_box->hit_test_touch(result, child_pos);
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        const float prev_offset = m_physics.offset();
        bool anim_scroll = m_physics.update();
        bool anim_bar = m_scrollbar.update_animations(m_viewport_hovered);
        if (anim_scroll || anim_bar) {
            mark_needs_paint();
        }
        if (m_on_scroll && std::abs(m_physics.offset() - prev_offset) > 0.01f) {
            m_on_scroll(m_physics.offset(), m_physics.velocity());
        }

        Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
        float top_f = 0.0f;
        float bot_f = 0.0f;
        if (m_edge_feather && m_feather_size > 0.5f && vp.height() > 8.0f) {
            const float cap = std::min(m_feather_size, vp.height() * 0.35f);
            top_f = std::clamp(m_physics.offset(), 0.0f, cap);
            const float rem = std::max(0.0f, m_physics.max_scroll() - m_physics.offset());
            bot_f = std::clamp(rem, 0.0f, cap);
        }
        ctx.push_feathered_clip(vp, 0.0f, top_f, bot_f);
        if (m_child_box) {
            // 像素对齐滚动平移 (std::round)，确保内容与文字随滚动整像素同步移动，杜绝子像素抖动
            const float snapped_scroll = std::round(m_physics.offset());
            m_child_box->set_offset(Vec2{0.0f, -snapped_scroll});
            PaintContext::OffsetScope scroll_scope(ctx, m_child_box->offset());
            m_child_box->paint(ctx);
        }
        ctx.pop_clip();

        if (m_scroll_hairline && m_physics.offset() > 0.25f) {
            const float t = std::clamp(m_physics.offset() / std::max(1.0f, m_hairline_fade_range), 0.0f, 1.0f);
            const Theme& theme = ThemeContext::current();
            const Color hl = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, 0.14f * t}
                : Color{0.0f, 0.0f, 0.0f, 0.12f * t};
            const Color sh = theme.is_dark
                ? Color{0.0f, 0.0f, 0.0f, 0.24f * t}
                : Color{0.0f, 0.0f, 0.0f, 0.07f * t};
            ctx.draw_rect(Rect::from_xywh(vp.min_x, vp.min_y, vp.width(), 1.0f), hl, 0.0f, 0.0f);
            ctx.draw_linear_gradient(
                Rect::from_xywh(vp.min_x, vp.min_y + 1.0f, vp.width(), 9.0f),
                sh, Color{0.0f, 0.0f, 0.0f, 0.0f},
                PointF{vp.min_x, vp.min_y + 1.0f}, PointF{vp.min_x, vp.min_y + 10.0f},
                0.0f, 0.0f, false);
        }

        ctx.push_clip(vp, 0.0f);
        m_scrollbar.paint(ctx, vp, m_content_height, m_physics.offset(), ThemeContext::current());
        ctx.pop_clip();
    }

    void set_scroll_hairline(bool enabled, float fade_range = 18.0f) noexcept {
        const float fr = std::max(1.0f, fade_range);
        if (m_scroll_hairline != enabled || std::abs(m_hairline_fade_range - fr) > 0.01f) {
            m_scroll_hairline = enabled;
            m_hairline_fade_range = fr;
            mark_needs_paint();
        }
    }
    void set_scroll_hairline_enabled(bool enabled) noexcept {
        set_scroll_hairline(enabled, m_hairline_fade_range);
    }
    void set_hairline_fade_range(float fade_range) noexcept {
        set_scroll_hairline(m_scroll_hairline, fade_range);
    }
    void set_on_scroll(Fn<void(float, float)> cb) {
        m_on_scroll = std::move(cb);
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

        // 隧道预览：识别 Android / Windows / Linux 触控垂直滚动手势 (8px 容差区分点击/水平滑块与垂直滚动)
        on_pointer_preview = [this](PointerEvent& e) {
            if (e.device_kind != PointerDeviceKind::Touch) return;
            if (e.type == PlatformEventType::PointerDown) {
                // 若触控点落在滚动条区域内，交由滚动条直接处理，不启动内容区触控拖拽
                Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
                auto m = m_scrollbar.compute_metrics(vp, m_content_height, m_physics.offset());
                if (m_scrollbar.hit_test(e.local_position, m, e.device_kind) != WinUIScrollBar::HitPart::None) {
                    m_touch_tracking = false;
                    m_touch_dragging = false;
                    m_touch_braked = false;
                    return;
                }
                // 防御性清理可能残留的滚动条拖拽态
                m_scrollbar.handle_pointer_up();
                if (m_physics.max_scroll() > 0.0f || m_physics.is_rubber_band_enabled()) {
                    m_touch_braked = (std::abs(m_physics.velocity()) > 45.0f);
                    m_touch_tracking = true;
                    m_touch_dragging = false;
                    m_touch_start_pos = e.position;
                    m_touch_last_pos = e.position;
                    m_physics.begin_touch_drag();
                    if (m_touch_braked) {
                        e.gesture_capture_target = this;
                        e.stop_propagation();
                    }
                }
            } else if (e.type == PlatformEventType::PointerMove && m_touch_tracking && !m_touch_dragging && !m_scrollbar.is_dragging()) {
                float dx = e.position.x - m_touch_start_pos.x;
                float dy = e.position.y - m_touch_start_pos.y;
                if (std::abs(dy) > touch::kTouchSlopPx && touch::is_vertical_drag(dx, dy)) {
                    m_touch_dragging = true;
                    m_touch_braked = false;
                    m_physics.apply_touch_drag(e.position.y - m_touch_last_pos.y);
                    m_touch_last_pos = e.position;
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.gesture_capture_target = this;
                    e.stop_propagation();
                }
            } else if (e.type == PlatformEventType::PointerUp) {
                if (m_touch_braked) {
                    m_touch_braked = false;
                    m_touch_tracking = false;
                    e.stop_propagation();
                    return;
                }
                if (!m_touch_dragging && m_touch_tracking) {
                    m_touch_tracking = false;
                    // 若在回弹中途被手指轻触停住后直接抬起，确保继续弹回边界
                    if (std::abs(m_physics.overscroll_amount()) > 0.05f) {
                        m_physics.end_touch_drag();
                        mark_needs_paint();
                    }
                }
            }
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (m_touch_dragging && e.device_kind == PointerDeviceKind::Touch) {
                float dy = e.position.y - m_touch_last_pos.y;
                m_touch_last_pos = e.position;
                m_physics.apply_touch_drag(dy);
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
            if (e.type == PlatformEventType::PointerScroll) {
                if (m_physics.max_scroll() > 0.0f || m_physics.is_rubber_band_enabled()) {
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
            bool sb_released = m_scrollbar.handle_pointer_up();
            if (m_touch_dragging) {
                m_touch_dragging = false;
                m_touch_tracking = false;
                m_physics.end_touch_drag();
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            m_touch_tracking = false;
            if (sb_released) {
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_pointer_cancel = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_touch_dragging = false;
            m_touch_tracking = false;
            m_touch_braked = false;
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };
    }

    float m_width{0.0f};
    bool m_touch_braked{false};
    float m_height{0.0f};
    float m_max_height{0.0f};
    float m_content_height{0.0f};
    bool m_viewport_hovered{false};
    bool m_touch_tracking{false};
    bool m_touch_dragging{false};
    PointF m_touch_start_pos{0.0f, 0.0f};
    PointF m_touch_last_pos{0.0f, 0.0f};
    std::shared_ptr<RenderBox> m_child_box;
    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;
    bool m_edge_feather{true};
    float m_feather_size{20.0f};
    bool m_scroll_hairline{false};
    float m_hairline_fade_range{18.0f};
    Fn<void(float, float)> m_on_scroll;
};

// C++20 指定初始化器声明式滚动容器 (ScrollView DSL Wrapper)
struct ScrollView : BindableMixin<ScrollView, RenderScrollView> {
    float width{0.0f};
    float height{0.0f};
    float max_height{0.0f};
    WidgetPtr child{nullptr};
    bool rubber_band_overscroll{true};
    bool edge_feather{true};
    float feather_size{20.0f};
    bool scroll_hairline{false};
    float hairline_fade_range{18.0f};
    Fn<void(float, float)> on_scroll{nullptr};

    ScrollView& rubber_band(bool enabled) & noexcept {
        rubber_band_overscroll = enabled;
        return *this;
    }
    ScrollView&& rubber_band(bool enabled) && noexcept {
        rubber_band_overscroll = enabled;
        return std::move(*this);
    }

    ScrollView& feather(bool enabled, float size = 28.0f) & noexcept {
        edge_feather = enabled;
        feather_size = size;
        return *this;
    }
    ScrollView&& feather(bool enabled, float size = 28.0f) && noexcept {
        edge_feather = enabled;
        feather_size = size;
        return std::move(*this);
    }

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class ScrollViewWidget : public Widget {
public:
    explicit ScrollViewWidget(const ScrollView& desc) : m_desc(desc) {}
    explicit ScrollViewWidget(ScrollView&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ScrollViewWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderScrollView>();
        node->set_dimensions(m_desc.width, m_desc.height, m_desc.max_height);
        node->set_rubber_band_enabled(m_desc.rubber_band_overscroll);
        node->set_edge_feather(m_desc.edge_feather, m_desc.feather_size);
        node->set_scroll_hairline(m_desc.scroll_hairline, m_desc.hairline_fade_range);
        node->set_on_scroll(m_desc.on_scroll);
        if (m_desc.child) {
            node->set_child_box(m_desc.child->create_render_node());
        }
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sv = std::static_pointer_cast<RenderScrollView>(node)) {
            sv->set_dimensions(m_desc.width, m_desc.height, m_desc.max_height);
            sv->set_rubber_band_enabled(m_desc.rubber_band_overscroll);
            sv->set_edge_feather(m_desc.edge_feather, m_desc.feather_size);
            sv->set_scroll_hairline(m_desc.scroll_hairline, m_desc.hairline_fade_range);
            sv->set_on_scroll(m_desc.on_scroll);
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

inline ScrollView::operator WidgetPtr() const& {
    return make_widget<ScrollViewWidget>(*this);
}
inline ScrollView::operator WidgetPtr() && {
    return make_widget<ScrollViewWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
