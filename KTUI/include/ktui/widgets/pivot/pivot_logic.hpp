#pragma once

#include "pivot_data.hpp"
#include "pivot_style.hpp"
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

// WinUI 3 / Fluent 2 Pivot 无边框下划线页签渲染盒 (RenderPivot)
// 适用场景：计算器 [历史记录 | 内存]、[日期差值 | 加减天数]、视图局部标签切换等。
// 特点：零外框/内框描边噪点、自然紧凑文字宽度、平滑主题色滑动下划线指示条、可选关联内容区。
class RenderPivot : public RenderBox {
public:
    RenderPivot() noexcept {
        cursor_shape = CursorShape::PointingHand;
        is_focusable = true;
        setup_behavior();
    }

    ~RenderPivot() override = default;

    void set_properties(std::vector<PivotItem> items,
                        int selected_index,
                        float width,
                        float height,
                        float font_size,
                        float item_spacing,
                        bool equal_width,
                        bool show_divider,
                        Fn<WidgetPtr(int)> content_builder,
                        Fn<void(int)> on_selection_changed) {
        bool items_changed = (m_items.size() != items.size());
        if (!items_changed) {
            for (size_t i = 0; i < items.size(); ++i) {
                if (m_items[i].header != items[i].header ||
                    m_items[i].id != items[i].id ||
                    m_items[i].svg_path != items[i].svg_path ||
                    m_items[i].disabled != items[i].disabled) {
                    items_changed = true;
                    break;
                }
            }
        }
        m_items = std::move(items);
        m_width = width;
        m_height = height;
        m_font_size = std::max(11.0f, font_size);
        m_item_spacing = std::max(0.0f, item_spacing);
        m_equal_width = equal_width;
        m_show_divider = show_divider;
        m_content_builder = std::move(content_builder);
        m_on_selected = std::move(on_selection_changed);

        int max_idx = std::max(0, static_cast<int>(m_items.size()) - 1);
        int clamped = m_items.empty() ? 0 : std::clamp(selected_index, 0, max_idx);
        bool sel_changed = (clamped != m_selected);
        m_selected = clamped;

        if (m_content_builder) {
            if (items_changed || sel_changed || !m_active_content_box) {
                rebuild_active_content();
            }
        } else if (m_active_content_box) {
            clear_active_content();
        }
        mark_needs_layout();
    }

    void set_selected_index(int idx) {
        if (m_items.empty()) return;
        int max_idx = static_cast<int>(m_items.size()) - 1;
        int clamped = std::clamp(idx, 0, max_idx);
        if (m_selected != clamped) {
            trigger_slide_anim(m_selected, clamped);
            m_selected = clamped;
            if (m_content_builder) rebuild_active_content();
            mark_needs_layout();
        }
    }

    [[nodiscard]] int selected_index() const noexcept { return m_selected; }

    void select(int idx) {
        if (m_items.empty()) return;
        int max_idx = static_cast<int>(m_items.size()) - 1;
        int clamped = std::clamp(idx, 0, max_idx);
        if (m_items[static_cast<size_t>(clamped)].disabled) return;
        if (clamped != m_selected) {
            trigger_slide_anim(m_selected, clamped);
            m_selected = clamped;
            if (m_content_builder) rebuild_active_content();
            mark_needs_layout();
            if (m_on_selected) m_on_selected(m_selected);
        }
    }

    void perform_layout() override {
        float strip_h = strip_height();
        float natural_w = compute_natural_strip_width();
        float w = (m_width > 0.0f)
            ? m_width
            : (m_content_builder && std::isfinite(m_constraints.max_width)
                   ? m_constraints.max_width
                   : (m_equal_width && std::isfinite(m_constraints.max_width)
                          ? m_constraints.max_width
                          : std::max(64.0f, natural_w)));
        w = m_constraints.constrain_width(w);
        compute_item_rects(w, strip_h);

        if (!m_ind_init && m_selected >= 0 && m_selected < static_cast<int>(m_ind_spans.size())) {
            const auto& sp = m_ind_spans[static_cast<size_t>(m_selected)];
            m_ind_x = sp.first;
            m_ind_w = sp.second;
            m_ind_init = true;
        }

        if (m_active_content_box) {
            float content_top = strip_h + 6.0f;
            float avail_h = std::isfinite(m_constraints.max_height)
                ? std::max(0.0f, m_constraints.max_height - content_top)
                : std::numeric_limits<float>::infinity();
            if (m_height > 0.0f) {
                avail_h = std::max(0.0f, m_height - content_top);
            }
            BoxConstraints cc{
                0.0f, w,
                0.0f, avail_h
            };
            m_active_content_box->layout(cc, true);
            m_active_content_box->set_offset(Vec2{0.0f, content_top});

            float h = (m_height > 0.0f)
                ? m_height
                : ((std::isfinite(m_constraints.max_height) && m_constraints.min_height >= m_constraints.max_height - 0.5f)
                       ? m_constraints.max_height
                       : (content_top + m_active_content_box->size().y));
            set_size(m_constraints.constrain(Vec2{w, h}));
        } else {
            float h = (m_height > 0.0f) ? m_height : strip_h;
            set_size(m_constraints.constrain(Vec2{w, h}));
        }
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (local_position.x < 0.0f || local_position.y < 0.0f ||
            local_position.x > size().x || local_position.y > size().y) {
            return false;
        }
        if (m_active_content_box && local_position.y >= m_active_content_box->offset().y) {
            PointF cp = local_position - m_active_content_box->offset();
            if (m_active_content_box->hit_test(result, cp)) {
                result.add(this, local_position);
                return true;
            }
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        if (m_items.empty()) return;
        const Theme& theme = ThemeContext::current();
        const auto resolved_style = PivotStyleResolver::resolve(theme);
        const float w = size().x;
        const float strip_h = strip_height();

        // 1. 平滑滑动下划线指示条 (steady_clock 指数阻尼)
        auto now = std::chrono::steady_clock::now();
        float dt = m_has_last_frame
            ? std::clamp(std::chrono::duration<float>(now - m_last_frame).count(), 0.001f, 0.05f)
            : (1.0f / 60.0f);
        m_last_frame = now;
        m_has_last_frame = true;

        bool animating = false;
        if (m_selected >= 0 && m_selected < static_cast<int>(m_ind_spans.size())) {
            const auto& target = m_ind_spans[static_cast<size_t>(m_selected)];
            if (!m_ind_init) {
                m_ind_x = target.first;
                m_ind_w = target.second;
                m_trail_x = target.first;
                m_trail_w = target.second;
                m_ind_init = true;
            } else if (std::abs(m_ind_x - target.first) > 0.15f || std::abs(m_ind_w - target.second) > 0.15f ||
                       (m_liquid_indicator && (std::abs(m_trail_x - m_ind_x) > 0.25f || std::abs(m_trail_w - m_ind_w) > 0.25f))) {
                float k = 1.0f - std::exp(-dt * 24.0f);
                float kt = 1.0f - std::exp(-dt * 11.5f);
                m_ind_x += (target.first - m_ind_x) * k;
                m_ind_w += (target.second - m_ind_w) * k;
                m_trail_x += (m_ind_x - m_trail_x) * kt;
                m_trail_w += (m_ind_w - m_trail_w) * kt;
                animating = true;
            } else {
                m_ind_x = target.first;
                m_ind_w = target.second;
                m_trail_x = m_ind_x;
                m_trail_w = m_ind_w;
            }
        }
        if (std::abs(m_content_slide_x) > 0.2f) {
            m_content_slide_x *= std::exp(-dt * 20.0f);
            animating = true;
        } else {
            m_content_slide_x = 0.0f;
        }
        if (animating) mark_needs_paint();
        else m_has_last_frame = false;

        // 2. 可选底部分割细线 (带内容区或显式开启时)
        if (m_show_divider || m_active_content_box) {
            Color div_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.07f} : Color{0.0f, 0.0f, 0.0f, 0.07f};
            ctx.draw_rect(Rect::from_xywh(0.0f, strip_h - 1.0f, w, 1.0f), div_c, 0.0f, 0.0f);
        }

        // 3. 页签文字与无边框 Hover 柔和衬底
        Color active_c   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.96f} : Color{0.0f, 0.0f, 0.0f, 0.92f};
        Color hover_c    = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.88f} : Color{0.0f, 0.0f, 0.0f, 0.84f};
        Color idle_c     = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.60f} : Color{0.0f, 0.0f, 0.0f, 0.58f};
        Color disabled_c = theme.colors.text_disabled;

        for (size_t i = 0; i < m_items.size() && i < m_item_rects.size(); ++i) {
            int idx = static_cast<int>(i);
            const auto& it = m_items[i];
            const Rect& ir = m_item_rects[i];
            bool is_sel = (idx == m_selected);
            bool is_hov = (!it.disabled && idx == m_hovered);
            bool is_prs = (!it.disabled && idx == m_pressed);

            if (is_hov || is_prs) {
                Color hbg = is_prs ? theme.colors.subtle_fill_tertiary : theme.colors.subtle_fill_secondary;
                ctx.draw_rect(Rect::from_xywh(ir.min_x, ir.min_y + 2.0f, ir.width(), std::max(0.0f, ir.height() - 6.0f)),
                              hbg, 4.0f, 0.0f);
            }

            Color fg = it.disabled ? disabled_c : (is_sel ? active_c : (is_hov ? hover_c : idle_c));
            FontWeight fw = is_sel ? FontWeight::SemiBold : FontWeight::Regular;

            Vec2 ts = TextMeasure::measure(it.header, m_font_size);
            float content_w = ts.x + (it.svg_path.empty() ? 0.0f : 19.0f);
            float sx = std::round(ir.min_x + (ir.width() - content_w) * 0.5f);
            float cy = std::round(ir.min_y + (ir.height() - 3.0f - ts.y) * 0.5f);

            if (!it.svg_path.empty()) {
                Rect icr = Rect::from_xywh(sx, std::round(ir.min_y + (ir.height() - 17.0f) * 0.5f), 14.0f, 14.0f);
                SvgEngine::draw_svg_path(ctx, it.svg_path, 24.0f, 24.0f, icr, fg);
                sx += 19.0f;
            }
            ctx.draw_text(it.header, PointF{sx, cy}, m_font_size, fg, fw);
        }

        // 4. WinUI 3 Pivot 滑动主题色下划线指示条 (可选 SDF Smooth-Min 液态粘连)
        if (m_ind_init && m_ind_w > 0.5f) {
            const Rect lead = Rect::from_xywh(m_ind_x, strip_h - 3.0f, m_ind_w, 2.5f);
            if (m_liquid_indicator && std::abs(m_trail_x - m_ind_x) > 0.35f) {
                const Rect trail = Rect::from_xywh(m_trail_x, strip_h - 2.75f, std::max(6.0f, m_trail_w), 2.0f);
                ctx.draw_metaball(lead, 1.25f, trail, 1.0f, 8.0f, resolved_style.accent, 0.0f);
            } else {
                ctx.draw_rect(lead, resolved_style.accent, 1.25f, 0.0f);
            }
        }

        // 5. 关联内容区绘制
        if (m_active_content_box) {
            float content_top = m_active_content_box->offset().y;
            Rect content_clip = Rect::from_xywh(0.0f, content_top, w, std::max(0.0f, size().y - content_top));
            ctx.push_clip(content_clip, 0.0f);
            {
                Vec2 off = m_active_content_box->offset();
                off.x += m_content_slide_x;
                PaintContext::OffsetScope scope(ctx, off);
                m_active_content_box->paint(ctx);
            }
            ctx.pop_clip();
        }
    }

private:
    [[nodiscard]] float strip_height() const noexcept {
        return std::max(32.0f, std::ceil(m_font_size + 18.0f));
    }

    [[nodiscard]] float item_content_width(const PivotItem& it) const {
        float w = TextMeasure::measure_width(it.header, m_font_size);
        if (!it.svg_path.empty()) w += 19.0f;
        return w;
    }

    [[nodiscard]] float compute_natural_strip_width() const {
        if (m_items.empty()) return 0.0f;
        float total = 0.0f;
        for (size_t i = 0; i < m_items.size(); ++i) {
            total += std::ceil(item_content_width(m_items[i])) + 16.0f;
            if (i + 1 < m_items.size()) total += m_item_spacing;
        }
        return total;
    }

    void compute_item_rects(float w, float strip_h) {
        m_item_rects.clear();
        m_ind_spans.clear();
        if (m_items.empty()) return;

        size_t n = m_items.size();
        if (m_equal_width) {
            float gap_sum = m_item_spacing * static_cast<float>(n > 0 ? n - 1 : 0);
            float iw = std::max(36.0f, (w - gap_sum) / static_cast<float>(n));
            float x = 0.0f;
            for (size_t i = 0; i < n; ++i) {
                Rect r = Rect::from_xywh(x, 0.0f, iw, strip_h);
                m_item_rects.push_back(r);
                float cw = std::max(14.0f, std::min(iw - 8.0f, item_content_width(m_items[i])));
                m_ind_spans.emplace_back(r.min_x + (r.width() - cw) * 0.5f, cw);
                x += iw + m_item_spacing;
            }
            return;
        }

        float x = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            float cw = item_content_width(m_items[i]);
            float iw = std::ceil(cw) + 16.0f;
            if (x + iw > w && i == n - 1) {
                iw = std::max(28.0f, w - x);
            }
            Rect r = Rect::from_xywh(x, 0.0f, iw, strip_h);
            m_item_rects.push_back(r);
            float span_w = std::max(14.0f, std::min(iw - 8.0f, cw));
            m_ind_spans.emplace_back(r.min_x + (r.width() - span_w) * 0.5f, span_w);
            x += iw + m_item_spacing;
        }
    }

    void clear_active_content() {
        if (m_active_content_box) {
            remove_child(m_active_content_box.get());
            m_active_content_box.reset();
            m_active_content_widget.reset();
        }
    }

    void rebuild_active_content() {
        clear_active_content();
        if (m_selected >= 0 && m_selected < static_cast<int>(m_items.size()) && m_content_builder) {
            m_active_content_widget = m_content_builder(m_selected);
            if (m_active_content_widget) {
                m_active_content_box = m_active_content_widget->create_render_node();
                if (m_active_content_box) {
                    append_child(m_active_content_box.get());
                }
            }
        }
    }

    void trigger_slide_anim(int old_idx, int new_idx) noexcept {
        if (old_idx >= 0 && new_idx >= 0 && old_idx != new_idx) {
            if (m_active_content_box || m_content_builder) {
                m_content_slide_x = (new_idx > old_idx ? 1.0f : -1.0f) * 14.0f;
            }
            m_last_frame = std::chrono::steady_clock::now();
            m_has_last_frame = true;
            mark_needs_paint();
        }
    }

    [[nodiscard]] int hit_item(Vec2 local) const noexcept {
        if (local.y < 0.0f || local.y > strip_height()) return -1;
        for (size_t i = 0; i < m_item_rects.size(); ++i) {
            if (m_item_rects[i].contains(local)) return static_cast<int>(i);
        }
        return -1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int h = hit_item(e.local_position);
            if (h != m_hovered) {
                m_hovered = h;
                bool en = (h >= 0 && h < static_cast<int>(m_items.size()) && !m_items[static_cast<size_t>(h)].disabled);
                cursor_shape = en ? CursorShape::PointingHand : CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered != -1 || m_pressed != -1) {
                m_hovered = -1;
                m_pressed = -1;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int idx = hit_item(e.local_position);
            if (idx >= 0 && idx < static_cast<int>(m_items.size()) && !m_items[static_cast<size_t>(idx)].disabled) {
                m_pressed = idx;
                select(idx);
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent&) {
            if (m_pressed != -1) {
                m_pressed = -1;
                mark_needs_paint();
            }
        };
        on_key = [this](KeyEvent& e) {
            if (e.type != PlatformEventType::KeyDown || m_items.empty()) return;
            int dir = (e.key_code == 0x27) ? 1 : (e.key_code == 0x25 ? -1 : 0);
            if (dir == 0) return;
            int n = static_cast<int>(m_items.size());
            for (int step = 1; step <= n; ++step) {
                int cand = (m_selected + dir * step % n + n) % n;
                if (!m_items[static_cast<size_t>(cand)].disabled) {
                    select(cand);
                    e.stop_propagation();
                    break;
                }
            }
        };
    }

    std::vector<PivotItem> m_items;
    int m_selected{0};
    int m_hovered{-1};
    int m_pressed{-1};
    float m_width{0.0f};
    float m_height{0.0f};
    float m_font_size{14.0f};
    float m_item_spacing{6.0f};
    bool m_equal_width{false};
    bool m_show_divider{false};
    Fn<WidgetPtr(int)> m_content_builder;
    Fn<void(int)> m_on_selected;

    std::vector<Rect> m_item_rects;
    std::vector<std::pair<float, float>> m_ind_spans;
    bool m_ind_init{false};
    float m_ind_x{0.0f};
    float m_ind_w{0.0f};
    float m_trail_x{0.0f};
    float m_trail_w{0.0f};
    bool m_liquid_indicator{false};
    float m_content_slide_x{0.0f};
    bool m_has_last_frame{false};
    std::chrono::steady_clock::time_point m_last_frame{};
    WidgetPtr m_active_content_widget;
    std::shared_ptr<RenderBox> m_active_content_box;

public:
    void set_liquid_indicator(bool enabled) noexcept {
        if (m_liquid_indicator != enabled) { m_liquid_indicator = enabled; mark_needs_paint(); }
    }
    [[nodiscard]] bool liquid_indicator_enabled() const noexcept { return m_liquid_indicator; }
    void wrap_on_selected(Fn<void(int)> extra) {
        auto prev = std::move(m_on_selected);
        m_on_selected = [prev = std::move(prev), extra = std::move(extra)](int idx) {
            if (extra) extra(idx);
            if (prev) prev(idx);
        };
    }
};

// C++20 声明式 WinUI 3 Pivot 控件
struct Pivot : BindableMixin<Pivot, RenderPivot> {
    std::vector<PivotItem> items;
    int selected_index{0};
    float width{0.0f};
    float height{0.0f};
    float font_size{14.0f};
    float item_spacing{6.0f};
    bool equal_width{false};
    bool show_divider{false};
    bool liquid_indicator{false};
    Fn<WidgetPtr(int)> content_builder{nullptr};
    Fn<void(int)> on_selection_changed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderPivot>& node, Property<int>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_selected_index(prop.get());
    std::weak_ptr<RenderPivot> weak = node;
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

class PivotWidget : public Widget {
public:
    explicit PivotWidget(const Pivot& desc) : m_desc(desc) {}
    explicit PivotWidget(Pivot&& desc) noexcept : m_desc(std::move(desc)) {}
    ~PivotWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderPivot>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderPivot>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderPivot>& node) const {
        node->set_properties(
            m_desc.items,
            m_desc.selected_index,
            m_desc.width,
            m_desc.height,
            m_desc.font_size,
            m_desc.item_spacing,
            m_desc.equal_width,
            m_desc.show_divider,
            m_desc.content_builder,
            m_desc.on_selection_changed
        );
        node->set_liquid_indicator(m_desc.liquid_indicator);
    }

    Pivot m_desc;
};

inline Pivot::operator WidgetPtr() const& {
    return make_widget<PivotWidget>(*this);
}
inline Pivot::operator WidgetPtr() && {
    return make_widget<PivotWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
