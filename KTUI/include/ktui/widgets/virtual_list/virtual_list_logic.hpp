#pragma once

#include "virtual_list_data.hpp"
#include "virtual_list_style.hpp"
#include "../scroll/scrollbar.hpp"
#include "../scroll/smooth_scroll_physics.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <cmath>

namespace ktui {


// 高性能视口虚拟化列表渲染盒 (RenderVirtualList)
// 仅在可见视口 [scroll_offset - overscan, scroll_offset + viewport_h + overscan] 内按需实例化、排版与绘制子项，
// 100,000+ 行数据依然保持 O(1) 内存与 120 FPS 恒定帧率，完整复用 SmoothScrollPhysics 与 WinUI 3 滚动条。
class RenderVirtualList : public RenderBox {
public:
    RenderVirtualList() noexcept {
        setup_behavior();
    }

    ~RenderVirtualList() override {
        clear_pool();
    }

    void set_properties(size_t item_count,
                        float item_height,
                        float width,
                        float height,
                        size_t overscan,
                        Fn<WidgetPtr(size_t)> item_builder) {
        bool rebuild = (m_item_count != item_count || std::abs(m_item_height - item_height) > 0.001f);
        m_item_count = item_count;
        m_item_height = std::max(16.0f, item_height);
        m_width = width;
        m_height = height;
        m_overscan = overscan;
        m_item_builder = std::move(item_builder);
        if (rebuild) clear_pool();
        mark_needs_layout();
    }

    [[nodiscard]] size_t item_count() const noexcept { return m_item_count; }
    [[nodiscard]] size_t rendered_item_count() const noexcept { return m_pool.size(); }
    [[nodiscard]] size_t first_visible_index() const noexcept { return m_first_visible; }
    [[nodiscard]] size_t last_visible_index() const noexcept { return m_last_visible; }
    [[nodiscard]] float scroll_offset() const noexcept { return m_physics.offset(); }
    [[nodiscard]] float max_scroll() const noexcept { return m_physics.max_scroll(); }

    void set_rubber_band_enabled(bool enabled) noexcept {
        if (m_physics.is_rubber_band_enabled() != enabled) {
            float prev_offset = m_physics.offset();
            m_physics.set_rubber_band_enabled(enabled);
            if (std::abs(m_physics.offset() - prev_offset) > 1e-5f ||
                std::abs(m_physics.offset() - m_last_synced_offset) > 1e-5f) {
                sync_visible_window();
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

    void scroll_to(float offset) {
        m_physics.scroll_to(offset);
        m_scrollbar.notify_activity();
        sync_visible_window();
        mark_needs_paint();
    }

    void scroll_by(float delta_px) {
        m_physics.scroll_by(delta_px);
        m_scrollbar.notify_activity();
        sync_visible_window();
        mark_needs_paint();
    }

    void perform_layout() override {
        float w = (m_width > 0.0f)
            ? m_width
            : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 360.0f);
        float h = (m_height > 0.0f)
            ? m_height
            : (std::isfinite(m_constraints.max_height) ? m_constraints.max_height : 320.0f);
        set_size(m_constraints.constrain(Vec2{w, h}));

        float total_h = static_cast<float>(m_item_count) * m_item_height;
        m_physics.set_viewport_extent(size().y);
        m_physics.set_max_scroll(std::max(0.0f, total_h - size().y));
        sync_visible_window();
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;

        // 优先命中虚拟池中的可见子项
        for (const auto& kv : m_pool) {
            const auto& child = kv.second;
            if (!child) continue;
            PointF local{position.x - child->offset().x, position.y - child->offset().y};
            if (child->hit_test(result, local)) {
                result.add(this, position);
                return true;
            }
        }
        result.add(this, position);
        return true;
    }

    bool hit_test_touch(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
        float total_h = static_cast<float>(m_item_count) * m_item_height;
        auto m = m_scrollbar.compute_metrics(vp, total_h, m_physics.offset());
        if (m_scrollbar.hit_test(position, m, PointerDeviceKind::Touch) != WinUIScrollBar::HitPart::None) {
            result.add(this, position);
            return true;
        }
        for (const auto& kv : m_pool) {
            const auto& child = kv.second;
            if (!child) continue;
            PointF local{position.x - child->offset().x, position.y - child->offset().y};
            if (child->hit_test_touch(result, local)) {
                result.add(this, position);
                return true;
            }
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        float prev_offset = m_physics.offset();
        bool anim_scroll = m_physics.update();
        bool anim_bar = m_scrollbar.update_animations(m_viewport_hovered);
        if (anim_scroll ||
            std::abs(m_physics.offset() - prev_offset) > 1e-5f ||
            std::abs(m_physics.offset() - m_last_synced_offset) > 1e-5f) {
            sync_visible_window();
        }
        if (anim_scroll || anim_bar) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
        ctx.draw_rect(vp, theme.colors.card_surface, 6.0f, 0.0f);
        ctx.draw_rect(vp, theme.colors.card_stroke, 6.0f, 1.0f);

        const Rect inner_vp = Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, size().x - 2.0f), std::max(0.0f, size().y - 2.0f));
        float top_f = 0.0f;
        float bot_f = 0.0f;
        if (m_edge_feather && m_feather_size > 0.5f && inner_vp.height() > 8.0f) {
            const float cap = std::min(m_feather_size, inner_vp.height() * 0.35f);
            top_f = std::clamp(m_physics.offset(), 0.0f, cap);
            const float rem = std::max(0.0f, m_physics.max_scroll() - m_physics.offset());
            bot_f = std::clamp(rem, 0.0f, cap);
        }
        ctx.push_feathered_clip(inner_vp, 5.0f, top_f, bot_f);
        for (size_t idx = m_first_visible; idx < m_last_visible; ++idx) {
            auto* node = find_in_pool(idx);
            if (!node) continue;
            PaintContext::OffsetScope scope(ctx, node->offset());
            node->paint(ctx);
        }
        ctx.pop_clip();

        if (m_scroll_hairline && m_physics.offset() > 0.25f) {
            const float t = std::clamp(m_physics.offset() / std::max(1.0f, m_hairline_fade_range), 0.0f, 1.0f);
            const Color hl = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, 0.14f * t}
                : Color{0.0f, 0.0f, 0.0f, 0.12f * t};
            ctx.draw_rect(Rect::from_xywh(inner_vp.min_x, inner_vp.min_y, inner_vp.width(), 1.0f), hl, 0.0f, 0.0f);
        }

        ctx.push_clip(inner_vp, 5.0f);
        float total_h = static_cast<float>(m_item_count) * m_item_height;
        m_scrollbar.paint(ctx, vp, total_h, m_physics.offset(), theme);
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

private:
    RenderBox* find_in_pool(size_t idx) const noexcept {
        for (const auto& kv : m_pool) {
            if (kv.first == idx) return kv.second.get();
        }
        return nullptr;
    }

    void clear_pool() {
        for (auto& kv : m_pool) {
            if (kv.second && kv.second->parent() == this) {
                remove_child(kv.second.get());
            }
        }
        m_pool.clear();
        m_first_visible = 0;
        m_last_visible = 0;
    }

    void sync_visible_window() {
        m_last_synced_offset = m_physics.offset();
        if (m_item_count == 0 || !m_item_builder || size().y <= 0.0f) {
            clear_pool();
            return;
        }

        float scroll_y = std::round(m_physics.offset());
        size_t first = static_cast<size_t>(std::max(0.0f, std::floor(scroll_y / m_item_height)));
        size_t vis_count = static_cast<size_t>(std::ceil(size().y / m_item_height)) + 1;
        size_t start = (first > m_overscan) ? (first - m_overscan) : 0;
        size_t end = std::min(m_item_count, first + vis_count + m_overscan);

        m_first_visible = start;
        m_last_visible = end;

        // 1. 回收滑出视口的节点
        for (size_t i = 0; i < m_pool.size(); ) {
            if (m_pool[i].first < start || m_pool[i].first >= end) {
                if (m_pool[i].second && m_pool[i].second->parent() == this) {
                    remove_child(m_pool[i].second.get());
                }
                m_pool[i] = std::move(m_pool.back());
                m_pool.pop_back();
            } else {
                ++i;
            }
        }

        // 2. 按需构建并定位当前可见窗口内的节点
        float row_w = std::max(1.0f, size().x - 14.0f);
        BoxConstraints row_c = BoxConstraints::tight(Vec2{row_w, m_item_height});
        for (size_t idx = start; idx < end; ++idx) {
            RenderBox* node = find_in_pool(idx);
            if (!node) {
                WidgetPtr w = m_item_builder(idx);
                if (!w) continue;
                auto created = w->create_render_node();
                if (!created) continue;
                append_child(created.get());
                node = created.get();
                m_pool.emplace_back(idx, std::move(created));
            }
            node->layout(row_c, false);
            node->set_offset(Vec2{2.0f, static_cast<float>(idx) * m_item_height - scroll_y});
        }
    }

    void setup_behavior() {
        on_pointer_preview = [this](PointerEvent& e) {
            if (e.device_kind != PointerDeviceKind::Touch) return;
            Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
            float total_h = static_cast<float>(m_item_count) * m_item_height;
            if (e.type == PlatformEventType::PointerDown) {
                auto m = m_scrollbar.compute_metrics(vp, total_h, m_physics.offset());
                if (m_scrollbar.hit_test(e.local_position, m, e.device_kind) != WinUIScrollBar::HitPart::None) {
                    m_touch_tracking = false;
                    m_touch_dragging = false;
                    m_touch_braked = false;
                    if (m_scrollbar.handle_pointer_down(e.local_position, vp, total_h, m_physics, 32.0f, e.device_kind)) {
                        sync_visible_window();
                        mark_needs_paint();
                        e.gesture_capture_target = this;
                        e.stop_propagation();
                    }
                    return;
                }
                m_scrollbar.handle_pointer_up();
                if (m_physics.max_scroll() > 0.0f || m_physics.is_rubber_band_enabled()) {
                    const bool was_moving = (std::abs(m_physics.velocity()) > 45.0f) ||
                                            m_physics.is_flinging() ||
                                            m_physics.is_bouncing() ||
                                            (std::abs(m_physics.overscroll_amount()) > 0.5f);
                    m_touch_braked = (std::abs(m_physics.velocity()) > 45.0f) ||
                                     m_physics.is_bouncing() ||
                                     (std::abs(m_physics.overscroll_amount()) > 0.5f);
                    m_touch_tracking = true;
                    m_touch_dragging = m_touch_braked;
                    m_touch_start_y = e.position.y;
                    m_touch_last_y = e.position.y;
                    m_physics.begin_touch_drag();
                    if (was_moving) mark_needs_paint();
                    if (m_touch_braked) {
                        e.gesture_capture_target = this;
                        e.stop_propagation();
                    }
                }
            } else if (e.type == PlatformEventType::PointerMove && m_touch_tracking && !m_touch_dragging && !m_scrollbar.is_dragging()) {
                if (std::abs(e.position.y - m_touch_start_y) > touch::kTouchSlopPx) {
                    m_touch_dragging = true;
                    m_touch_braked = false;
                    m_physics.apply_touch_drag(e.position.y - m_touch_last_y);
                    m_touch_last_y = e.position.y;
                    m_scrollbar.notify_activity();
                    sync_visible_window();
                    mark_needs_paint();
                    e.gesture_capture_target = this;
                    e.stop_propagation();
                }
            } else if (e.type == PlatformEventType::PointerUp) {
                if (m_touch_braked) {
                    m_touch_braked = false;
                    m_touch_tracking = false;
                    m_touch_dragging = false;
                    m_physics.end_touch_drag();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
                if (!m_touch_dragging && m_touch_tracking) {
                    m_touch_tracking = false;
                    if (std::abs(m_physics.overscroll_amount()) > 0.05f) {
                        m_physics.end_touch_drag();
                        mark_needs_paint();
                    }
                }
            }
        };

        on_pointer_enter = [this](PointerEvent&) {
            m_viewport_hovered = true;
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (m_touch_dragging && e.device_kind == PointerDeviceKind::Touch) {
                m_physics.apply_touch_drag(e.position.y - m_touch_last_y);
                m_touch_last_y = e.position.y;
                m_scrollbar.notify_activity();
                sync_visible_window();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (e.type == PlatformEventType::PointerScroll) {
                if (m_physics.max_scroll() > 0.0f || m_physics.is_rubber_band_enabled()) {
                    m_physics.add_wheel_delta(e.delta.y);
                    m_scrollbar.notify_activity();
                    sync_visible_window();
                    mark_needs_paint();
                    e.stop_propagation();
                }
                return;
            }
            Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
            float total_h = static_cast<float>(m_item_count) * m_item_height;
            bool consumed = false;
            if (m_scrollbar.handle_pointer_move(e.local_position, vp, total_h, m_physics, consumed)) {
                sync_visible_window();
                mark_needs_paint();
            }
            if (consumed) e.stop_propagation();
        };

        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            Rect vp = Rect::from_xywh(0.0f, 0.0f, size().x, size().y);
            float total_h = static_cast<float>(m_item_count) * m_item_height;
            if (m_scrollbar.handle_pointer_down(e.local_position, vp, total_h, m_physics, 32.0f, e.device_kind)) {
                sync_visible_window();
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_pointer_up = [this](PointerEvent& e) {
            bool sb_released = m_scrollbar.handle_pointer_up();
            if ((m_touch_dragging || m_touch_braked) && e.device_kind == PointerDeviceKind::Touch) {
                m_touch_dragging = false;
                m_touch_tracking = false;
                m_touch_braked = false;
                m_physics.end_touch_drag();
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            m_touch_tracking = false;
            m_touch_braked = false;
            if (sb_released) {
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };

        on_pointer_cancel = [this](PointerEvent&) {
            const bool need_end = m_touch_dragging || (std::abs(m_physics.overscroll_amount()) > 0.05f);
            m_viewport_hovered = false;
            m_touch_dragging = false;
            m_touch_tracking = false;
            m_touch_braked = false;
            if (need_end) {
                m_physics.end_touch_drag();
            }
            m_scrollbar.handle_pointer_up();
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };
    }

    size_t m_item_count{0};
    float m_item_height{36.0f};
    float m_width{0.0f};
    float m_height{300.0f};
    size_t m_overscan{3};
    Fn<WidgetPtr(size_t)> m_item_builder;

    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;
    std::vector<std::pair<size_t, std::shared_ptr<RenderBox>>> m_pool;
    size_t m_first_visible{0};
    size_t m_last_visible{0};

    bool m_viewport_hovered{false};
    bool m_touch_braked{false};

    bool m_touch_tracking{false};
    bool m_touch_dragging{false};
    float m_touch_start_y{0.0f};
    float m_touch_last_y{0.0f};
    float m_last_synced_offset{0.0f};
    bool m_edge_feather{true};
    float m_feather_size{20.0f};
    bool m_scroll_hairline{false};
    float m_hairline_fade_range{18.0f};
};

// C++20 指定初始化器声明式虚拟化列表 VirtualList
struct VirtualList : BindableMixin<VirtualList, RenderVirtualList> {
    size_t item_count{0};
    float item_height{36.0f};
    float width{0.0f};
    float height{300.0f};
    size_t overscan{3};
    Fn<WidgetPtr(size_t)> item_builder{nullptr};
    bool rubber_band_overscroll{true};
    bool edge_feather{true};
    float feather_size{20.0f};
    bool scroll_hairline{false};
    float hairline_fade_range{18.0f};

    VirtualList& rubber_band(bool enabled) & noexcept {
        rubber_band_overscroll = enabled;
        return *this;
    }
    VirtualList&& rubber_band(bool enabled) && noexcept {
        rubber_band_overscroll = enabled;
        return std::move(*this);
    }

    VirtualList& feather(bool enabled, float size = 28.0f) & noexcept {
        edge_feather = enabled;
        feather_size = size;
        return *this;
    }
    VirtualList&& feather(bool enabled, float size = 28.0f) && noexcept {
        edge_feather = enabled;
        feather_size = size;
        return std::move(*this);
    }

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class VirtualListWidget : public Widget {
public:
    explicit VirtualListWidget(const VirtualList& desc) : m_desc(desc) {}
    explicit VirtualListWidget(VirtualList&& desc) noexcept : m_desc(std::move(desc)) {}
    ~VirtualListWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderVirtualList>();
        node->set_properties(m_desc.item_count, m_desc.item_height, m_desc.width, m_desc.height, m_desc.overscan, m_desc.item_builder);
        node->set_rubber_band_enabled(m_desc.rubber_band_overscroll);
        node->set_edge_feather(m_desc.edge_feather, m_desc.feather_size);
        node->set_scroll_hairline(m_desc.scroll_hairline, m_desc.hairline_fade_range);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderVirtualList>(node)) {
            n->set_properties(m_desc.item_count, m_desc.item_height, m_desc.width, m_desc.height, m_desc.overscan, m_desc.item_builder);
            n->set_rubber_band_enabled(m_desc.rubber_band_overscroll);
            n->set_edge_feather(m_desc.edge_feather, m_desc.feather_size);
            n->set_scroll_hairline(m_desc.scroll_hairline, m_desc.hairline_fade_range);
        }
    }

private:
    VirtualList m_desc;
};

inline VirtualList::operator WidgetPtr() const& {
    return make_widget<VirtualListWidget>(*this);
}
inline VirtualList::operator WidgetPtr() && {
    return make_widget<VirtualListWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
