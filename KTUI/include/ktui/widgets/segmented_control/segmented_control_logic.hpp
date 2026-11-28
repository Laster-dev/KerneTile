#pragma once

#include "segmented_control_data.hpp"
#include "segmented_control_style.hpp"
#include <ktui/animation/frame_clock.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

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

    void set_liquid_indicator(bool enabled, float smooth_k = 14.0f) {
        if (m_liquid_indicator != enabled || m_liquid_smooth_k != smooth_k) {
            m_liquid_indicator = enabled;
            m_liquid_smooth_k = smooth_k;
            mark_needs_paint();
        }
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
        m_item_rects.clear();
        if (m_items.empty()) {
            set_size(m_constraints.constrain(Vec2{120.0f, 32.0f}));
            return;
        }

        std::vector<float> item_widths;
        item_widths.reserve(m_items.size());
        float ideal_w = 4.0f;
        for (const auto& it : m_items) {
            float iw = 28.0f + TextMeasure::measure_width(it.label, 13.0f);
            if (!it.svg_path.empty()) iw += 20.0f;
            float w_item = std::max(64.0f, std::ceil(iw));
            item_widths.push_back(w_item);
            ideal_w += w_item;
        }

        const float max_avail = std::isfinite(m_constraints.max_width)
            ? m_constraints.constrain_width(m_constraints.max_width)
            : std::max(120.0f, std::ceil(ideal_w));
        const float target_w = (m_width > 0.0f)
            ? m_constraints.constrain_width(m_width)
            : max_avail;

        // 单行放得下（或仅有 1 项）时保持标准 WinUI 3 等分单行形态
        if (m_items.size() <= 1 || ideal_w <= target_w + 1.0f) {
            float w = (m_width > 0.0f) ? target_w : std::max(120.0f, std::ceil(ideal_w));
            Vec2 final_sz = m_constraints.constrain(Vec2{w, 32.0f});
            set_size(final_sz);
            float inner_w = std::max(1.0f, final_sz.x - 4.0f);
            float seg_w = inner_w / static_cast<float>(m_items.size());
            for (size_t i = 0; i < m_items.size(); ++i) {
                m_item_rects.push_back(Rect::from_xywh(
                    2.0f + static_cast<float>(i) * seg_w, 2.0f,
                    seg_w, std::max(24.0f, final_sz.y - 4.0f)));
            }
        } else {
            // 横向空间不足时按行自动折行，每行段等分铺满可用宽度
            constexpr float kRowH = 28.0f;
            constexpr float kRowGap = 2.0f;
            float container_w = std::max(88.0f, target_w);
            float inner_w = std::max(60.0f, container_w - 4.0f);

            std::vector<std::pair<size_t, size_t>> rows;
            size_t row_start = 0;
            float cur_w = 0.0f;
            for (size_t i = 0; i < m_items.size(); ++i) {
                float iw = std::min(item_widths[i], inner_w);
                if (i > row_start && cur_w + iw > inner_w + 0.5f) {
                    rows.emplace_back(row_start, i);
                    row_start = i;
                    cur_w = iw;
                } else {
                    cur_w += iw;
                }
            }
            if (row_start < m_items.size()) {
                rows.emplace_back(row_start, m_items.size());
            }

            float total_h = 4.0f + static_cast<float>(rows.size()) * kRowH +
                            static_cast<float>(rows.size() > 1 ? rows.size() - 1 : 0) * kRowGap;
            Vec2 final_sz = m_constraints.constrain(Vec2{container_w, total_h});
            set_size(final_sz);

            float actual_inner_w = std::max(1.0f, final_sz.x - 4.0f);
            float y = 2.0f;
            for (const auto& [rs, re] : rows) {
                size_t count = re - rs;
                float seg_w = actual_inner_w / static_cast<float>(count);
                for (size_t i = rs; i < re; ++i) {
                    m_item_rects.push_back(Rect::from_xywh(
                        2.0f + static_cast<float>(i - rs) * seg_w, y, seg_w, kRowH));
                }
                y += kRowH + kRowGap;
            }
        }

        if (!m_pill_init && !m_item_rects.empty()) {
            Rect sr = segment_rect(m_selected);
            m_pill_x = sr.min_x;
            m_pill_y = sr.min_y;
            m_pill_w = sr.width();
            m_pill_h = sr.height();
            m_trail_x = sr.min_x;
            m_trail_w = sr.width();
            m_pill_init = true;
        }
    }

    void paint(PaintContext& ctx) override {
        if (m_items.empty()) return;
        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        // 1. Fluent 2 轻质凹槽底板 (单层极简凹槽，可选解析高斯内阴影)
        Color track_bg = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.24f} : Color{0.0f, 0.0f, 0.0f, 0.045f};
        Color track_border = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.06f} : Color{0.0f, 0.0f, 0.0f, 0.065f};
        const Rect track_r = Rect::from_xywh(0.0f, 0.0f, w, h);
        ctx.draw_box(track_r, track_bg, track_border, 6.0f, 1.0f);
        if (m_inset_shadow) {
            const Color sh = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.40f} : Color{0.0f, 0.04f, 0.12f, 0.18f};
            ctx.draw_inset_shadow(track_r, sh, 6.0f, 4.0f, 0.3f, Vec2{0.0f, 1.4f});
        }
        if (ThemeContext::current().resolve_pointer_reveal(m_pointer_reveal) && m_reveal_active) {
            const Color surf = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
            const Color edge = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.26f} : Color{0.0f, 0.0f, 0.0f, 0.18f};
            ctx.draw_pointer_reveal(track_r, m_reveal_pos, 72.0f, surf, edge, 6.0f, 1.0f);
        }

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
            m_pill_y = target.min_y;
            m_pill_w = target.width();
            m_pill_h = target.height();
            m_trail_x = target.min_x;
            m_trail_w = target.width();
            m_pill_init = true;
        } else if (std::abs(m_pill_x - target.min_x) > 0.15f || std::abs(m_pill_y - target.min_y) > 0.15f ||
                   std::abs(m_pill_w - target.width()) > 0.15f || std::abs(m_pill_h - target.height()) > 0.15f ||
                   (m_liquid_indicator && (std::abs(m_trail_x - m_pill_x) > 0.25f || std::abs(m_trail_w - m_pill_w) > 0.25f))) {
            float k = 1.0f - std::exp(-dt * 24.0f);
            float kt = 1.0f - std::exp(-dt * 11.5f);
            m_pill_x += (target.min_x - m_pill_x) * k;
            m_pill_y += (target.min_y - m_pill_y) * k;
            m_pill_w += (target.width() - m_pill_w) * k;
            m_pill_h += (target.height() - m_pill_h) * k;
            m_trail_x += (m_pill_x - m_trail_x) * kt;
            m_trail_w += (m_pill_w - m_trail_w) * kt;
            mark_needs_paint();
        } else {
            m_pill_x = target.min_x;
            m_pill_y = target.min_y;
            m_pill_w = target.width();
            m_pill_h = target.height();
            m_trail_x = m_pill_x;
            m_trail_w = m_pill_w;
            m_has_last_frame = false;
        }

        // 3. 非激活相邻分段之间的柔和分隔竖线（仅同一行相邻段）
        Color sep_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
        for (int i = 1; i < static_cast<int>(m_items.size()); ++i) {
            if (i == m_selected || i - 1 == m_selected || i == m_hovered || i - 1 == m_hovered) continue;
            Rect prev_sr = segment_rect(i - 1);
            Rect sr = segment_rect(i);
            if (std::abs(sr.min_y - prev_sr.min_y) > 1.0f) continue;
            ctx.draw_rect(Rect::from_xywh(std::round(sr.min_x), std::round(sr.min_y + (sr.height() - 14.0f) * 0.5f), 1.0f, 14.0f),
                          sep_col, 0.0f, 0.0f);
        }

        // 4. Hover / Press 柔和背景反馈
        if (m_hovered >= 0 && m_hovered != m_selected && m_hovered < static_cast<int>(m_items.size())) {
            Color hbg = (m_pressed == m_hovered) ? theme.colors.subtle_fill_tertiary : theme.colors.subtle_fill_secondary;
            ctx.draw_rect(segment_rect(m_hovered), hbg, 4.0f, 0.0f);
        }

        // 5. Fluent 2 轻质悬浮滑块 (可选启用 SDF Smooth-Min 液态粘连拉伸)
        Rect pill = Rect::from_xywh(m_pill_x, m_pill_y, m_pill_w, std::max(8.0f, m_pill_h));
        Color pill_bg = theme.is_dark
            ? (m_pressed == m_selected ? Color{1.0f, 1.0f, 1.0f, 0.095f} : Color{1.0f, 1.0f, 1.0f, 0.125f})
            : (m_pressed == m_selected ? Color{1.0f, 1.0f, 1.0f, 0.90f}  : Color{1.0f, 1.0f, 1.0f, 0.99f});
        if (m_liquid_indicator && std::abs(m_trail_x - m_pill_x) > 0.35f && std::abs(m_pill_y - target.min_y) < 1.0f) {
            const float pr = pill.height() * 0.5f;
            const Rect trail = Rect::from_xywh(m_trail_x + 4.0f, pill.min_y + 2.0f, std::max(8.0f, m_trail_w - 8.0f), std::max(4.0f, pill.height() - 4.0f));
            ctx.draw_metaball(pill, pr, trail, trail.height() * 0.5f, m_liquid_smooth_k, pill_bg, 0.0f);
        } else {
            if (!theme.is_dark) {
                ctx.draw_rect(Rect::from_xywh(pill.min_x, pill.min_y + 1.0f, pill.width(), pill.height()),
                              Color{0.0f, 0.0f, 0.0f, 0.06f}, 4.5f, 0.0f);
            }
            ctx.draw_rect(pill, pill_bg, 4.5f, 0.0f);
            if (theme.is_dark) {
                ctx.draw_rect(Rect::from_xywh(pill.min_x + 4.0f, pill.min_y + 0.5f,
                                              std::max(0.0f, pill.width() - 8.0f), 1.0f),
                              Color{1.0f, 1.0f, 1.0f, 0.10f}, 0.0f, 0.0f);
            }
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
            float sx = std::round(sr.min_x + std::max(4.0f, (sr.width() - content_w) * 0.5f));
            ctx.push_clip(sr, 4.0f);
            if (!it.svg_path.empty()) {
                Rect ir = Rect::from_xywh(sx, std::round(sr.min_y + (sr.height() - 14.0f) * 0.5f), 14.0f, 14.0f);
                SvgEngine::draw_svg_path(ctx, it.svg_path, 24.0f, 24.0f, ir, tc);
                sx += 20.0f;
            }
            ctx.draw_text(it.label, PointF{sx, std::round(sr.min_y + (sr.height() - ts.y) * 0.5f)}, 13.0f, tc,
                          is_sel ? FontWeight::SemiBold : FontWeight::Regular);
            ctx.pop_clip();
        }
    }

    bool hit_test_touch(HitTestResult& result, PointF position) override {
        if (touch::expand_hit_rect_for_touch(bounds(), touch::kMinTargetSize).contains(position)) {
            result.add(this, position);
            return true;
        }
        return false;
    }

private:
    Rect segment_rect(int idx) const noexcept {
        if (idx >= 0 && idx < static_cast<int>(m_item_rects.size())) {
            return m_item_rects[static_cast<size_t>(idx)];
        }
        if (m_items.empty()) return Rect{};
        float inner_w = std::max(1.0f, size().x - 4.0f);
        float seg_w = inner_w / static_cast<float>(m_items.size());
        return Rect::from_xywh(2.0f + static_cast<float>(idx) * seg_w, 2.0f, seg_w, std::max(24.0f, size().y - 4.0f));
    }

    int hit_segment(Vec2 local) const noexcept {
        for (size_t i = 0; i < m_item_rects.size(); ++i) {
            if (m_item_rects[i].contains(local)) return static_cast<int>(i);
        }
        return -1;
    }

    void setup_behavior() {
        on_pointer_preview = [this](PointerEvent& e) {
            if (e.device_kind != PointerDeviceKind::Touch) return;
            if (e.type == PlatformEventType::PointerDown) {
                m_touch_down = true;
                m_touch_dragging = false;
                m_touch_start_x = e.position.x;
                m_touch_start_y = e.position.y;
            } else if (e.type == PlatformEventType::PointerMove && m_touch_down && !m_touch_dragging) {
                float dx = e.position.x - m_touch_start_x;
                float dy = e.position.y - m_touch_start_y;
                if (touch::is_horizontal_drag(dx, dy, 8.0f)) {
                    m_touch_dragging = true;
                    e.gesture_capture_target = this;
                    e.stop_propagation();
                }
            }
        };
        on_pointer_move = [this](PointerEvent& e) {
            if (m_touch_dragging) {
                int idx = hit_segment(e.local_position);
                if (idx >= 0 && idx != m_selected) {
                    select(idx);
                }
                e.stop_propagation();
                return;
            }
            if (e.device_kind == PointerDeviceKind::Touch) return;
            int h = hit_segment(e.local_position);
            bool dirty = (h != m_hovered);
            m_hovered = h;
            if (ThemeContext::current().resolve_pointer_reveal(m_pointer_reveal)) {
                m_reveal_pos = e.local_position;
                m_reveal_active = true;
                dirty = true;
            }
            if (dirty) mark_needs_paint();
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered != -1 || m_pressed != -1 || m_reveal_active) {
                m_hovered = -1;
                m_pressed = -1;
                m_reveal_active = false;
                mark_needs_paint();
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_touch_down = false;
            m_touch_dragging = false;
            m_hovered = -1;
            m_pressed = -1;
            m_reveal_active = false;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int idx = hit_segment(e.local_position);
            if (idx >= 0) {
                m_pressed = idx;
                if (e.device_kind != PointerDeviceKind::Touch) {
                    select(idx);
                } else {
                    mark_needs_paint();
                }
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            const int prev_pressed = m_pressed;
            const bool was_drag = m_touch_dragging;
            m_touch_down = false;
            m_touch_dragging = false;
            if (m_pressed != -1) { m_pressed = -1; mark_needs_paint(); }
            if (e.device_kind == PointerDeviceKind::Touch && !was_drag && prev_pressed >= 0) {
                select(prev_pressed);
            }
        };
    }

    std::vector<SegmentedItem> m_items;
    std::vector<Rect> m_item_rects;
    int m_selected{0};
    int m_hovered{-1};
    int m_pressed{-1};
    bool m_touch_down{false};
    bool m_touch_dragging{false};
    float m_touch_start_x{0.0f};
    float m_touch_start_y{0.0f};
    float m_width{0.0f};
    bool m_pill_init{false};
    float m_pill_x{2.0f};
    float m_pill_y{2.0f};
    float m_pill_w{60.0f};
    float m_pill_h{28.0f};
    float m_trail_x{2.0f};
    float m_trail_w{60.0f};
    bool m_liquid_indicator{false};
    float m_liquid_smooth_k{14.0f};
    bool m_inset_shadow{false};
    std::optional<bool> m_pointer_reveal{std::nullopt};
    bool m_reveal_active{false};
    PointF m_reveal_pos{0.0f, 0.0f};
    bool m_has_last_frame{false};
    std::chrono::steady_clock::time_point m_last_frame{};
    Fn<void(int)> m_on_selected;

public:
    [[nodiscard]] float surface_corner_radius() const noexcept override { return 6.0f; }
    void set_inset_shadow(bool enabled) noexcept {
        if (m_inset_shadow != enabled) { m_inset_shadow = enabled; mark_needs_paint(); }
    }
    [[nodiscard]] bool pointer_reveal_enabled() const noexcept {
        return ThemeContext::current().resolve_pointer_reveal(m_pointer_reveal);
    }
    void set_pointer_reveal_enabled(std::optional<bool> enabled) noexcept {
        if (m_pointer_reveal != enabled) {
            m_pointer_reveal = enabled;
            if (!pointer_reveal_enabled()) m_reveal_active = false;
            mark_needs_paint();
        }
    }
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
    bool liquid_indicator{false};
    float liquid_smooth_k{14.0f};
    bool inset_shadow{false};
    std::optional<bool> enable_pointer_reveal{std::nullopt};
    Fn<void(int)> on_selected_index_changed{nullptr};

    SegmentedControl& pointer_reveal(bool enabled) & noexcept { enable_pointer_reveal = enabled; return *this; }
    SegmentedControl&& pointer_reveal(bool enabled) && noexcept { enable_pointer_reveal = enabled; return std::move(*this); }

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
        node->set_liquid_indicator(m_desc.liquid_indicator, m_desc.liquid_smooth_k);
        node->set_inset_shadow(m_desc.inset_shadow);
        node->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
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
 // namespace ktui
