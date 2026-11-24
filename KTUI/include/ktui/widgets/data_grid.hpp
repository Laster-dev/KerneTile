#pragma once

#include "common/fluent_icons.hpp"
#include "scroll/scrollbar.hpp"
#include "scroll/smooth_scroll_physics.hpp"
#include "widget.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace ktui {

enum class DataGridSortDirection : uint8_t {
    None = 0,
    Ascending = 1,
    Descending = 2
};

struct DataGridColumn {
    std::string title;
    float width{140.0f}; // <= 0 表示自动均分剩余宽度
    bool sortable{true};
    bool numeric{false};
};

// WinUI 3 高性能虚拟化数据表格 (RenderDataGrid)
// 固定表头 + 点击表头升/降序排序 + O(1) 视口虚拟化渲染 (万行 120 FPS) + SmoothScrollPhysics & WinUIScrollBar
class RenderDataGrid : public RenderBox {
public:
    RenderDataGrid() noexcept {
        cursor_shape = CursorShape::Arrow;
        setup_behavior();
    }

    ~RenderDataGrid() override = default;

    void set_properties(std::vector<DataGridColumn> columns,
                        std::vector<std::vector<std::string>> rows,
                        float row_height,
                        float width,
                        float height,
                        Fn<void(size_t)> on_row_selected) {
        m_columns = std::move(columns);
        m_rows = std::move(rows);
        m_row_height = std::max(26.0f, row_height);
        m_width = width;
        m_height = height;
        m_on_row_selected = std::move(on_row_selected);
        m_sort_col = -1;
        m_sort_dir = DataGridSortDirection::None;
        m_selected_orig_row = -1;
        m_order.resize(m_rows.size());
        std::iota(m_order.begin(), m_order.end(), size_t{0});
        mark_needs_layout();
    }

    [[nodiscard]] size_t row_count() const noexcept { return m_rows.size(); }
    [[nodiscard]] int sorted_column() const noexcept { return m_sort_col; }
    [[nodiscard]] DataGridSortDirection sort_direction() const noexcept { return m_sort_dir; }
    [[nodiscard]] int selected_row() const noexcept { return m_selected_orig_row; }
    [[nodiscard]] size_t visible_row_count() const noexcept { return m_last_visible_count; }

    void sort_by_column(size_t col_idx) {
        if (col_idx >= m_columns.size() || !m_columns[col_idx].sortable) return;
        if (m_sort_col == static_cast<int>(col_idx)) {
            if (m_sort_dir == DataGridSortDirection::Ascending) m_sort_dir = DataGridSortDirection::Descending;
            else if (m_sort_dir == DataGridSortDirection::Descending) {
                m_sort_dir = DataGridSortDirection::None;
                m_sort_col = -1;
            } else {
                m_sort_dir = DataGridSortDirection::Ascending;
            }
        } else {
            m_sort_col = static_cast<int>(col_idx);
            m_sort_dir = DataGridSortDirection::Ascending;
        }

        if (m_sort_dir == DataGridSortDirection::None || m_sort_col < 0) {
            std::iota(m_order.begin(), m_order.end(), size_t{0});
        } else {
            bool asc = (m_sort_dir == DataGridSortDirection::Ascending);
            bool num = m_columns[col_idx].numeric;
            std::stable_sort(m_order.begin(), m_order.end(), [&](size_t a, size_t b) {
                const std::string& sa = (col_idx < m_rows[a].size()) ? m_rows[a][col_idx] : kEmpty;
                const std::string& sb = (col_idx < m_rows[b].size()) ? m_rows[b][col_idx] : kEmpty;
                if (num) {
                    double da = std::strtod(sa.c_str(), nullptr);
                    double db = std::strtod(sb.c_str(), nullptr);
                    return asc ? (da < db) : (da > db);
                }
                return asc ? (sa < sb) : (sa > sb);
            });
        }
        m_physics.scroll_to(0.0f);
        mark_needs_paint();
    }

    void perform_layout() override {
        float w = (m_width > 0.0f)
            ? m_width
            : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 640.0f);
        float h = (m_height > 0.0f)
            ? m_height
            : (std::isfinite(m_constraints.max_height) ? m_constraints.max_height : 360.0f);
        set_size(m_constraints.constrain(Vec2{w, h}));
        compute_column_widths();
        float body_h = std::max(0.0f, size().y - kHeaderH);
        float total_h = static_cast<float>(m_rows.size()) * m_row_height;
        m_physics.set_max_scroll(std::max(0.0f, total_h - body_h));
    }

    void paint(PaintContext& ctx) override {
        bool anim_scroll = m_physics.update();
        bool anim_bar = m_scrollbar.update_animations(m_viewport_hovered);
        if (anim_scroll || anim_bar) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        Rect panel = Rect::from_xywh(0.0f, 0.0f, w, h);
        ctx.draw_rect(panel, theme.colors.card_surface, 6.0f, 0.0f);
        ctx.draw_rect(panel, theme.colors.card_stroke, 6.0f, 1.0f);

        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.94f} : Color{0.0f, 0.0f, 0.0f, 0.90f};
        Color head_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.78f} : Color{0.0f, 0.0f, 0.0f, 0.72f};
        Color stripe = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.022f} : Color{0.0f, 0.0f, 0.0f, 0.020f};
        Color head_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.045f} : Color{0.0f, 0.0f, 0.0f, 0.035f};

        // 1. 固定表头
        ctx.push_clip(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, w - 2.0f), kHeaderH), 5.0f);
        ctx.draw_rect(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, w - 2.0f), kHeaderH), head_bg, 0.0f, 0.0f);

        float cx = 1.0f;
        for (size_t c = 0; c < m_columns.size(); ++c) {
            float cw = (c < m_col_widths.size()) ? m_col_widths[c] : 120.0f;
            Rect hr = Rect::from_xywh(cx, 1.0f, cw, kHeaderH - 1.0f);
            if (static_cast<int>(c) == m_hovered_col && m_columns[c].sortable) {
                ctx.draw_rect(hr, theme.colors.subtle_fill_secondary, 0.0f, 0.0f);
            }
            ctx.push_clip(Rect::from_xywh(hr.min_x + 8.0f, hr.min_y, std::max(0.0f, hr.width() - 16.0f), hr.height()));
            Vec2 ts = TextMeasure::measure(m_columns[c].title, 12.5f);
            ctx.draw_text(m_columns[c].title,
                          PointF{hr.min_x + 12.0f, std::round(hr.min_y + (hr.height() - ts.y) * 0.5f)},
                          12.5f, (m_sort_col == static_cast<int>(c)) ? theme.colors.primary : head_c);
            ctx.pop_clip();

            if (m_sort_col == static_cast<int>(c) && m_sort_dir != DataGridSortDirection::None) {
                Rect sr = Rect::from_xywh(hr.max_x - 22.0f, hr.min_y, 18.0f, hr.height());
                const char* icon = (m_sort_dir == DataGridSortDirection::Ascending)
                    ? fluent_icons::kChevronUp : fluent_icons::kChevronDown;
                fluent_icons::draw_centered(ctx, icon, sr, 10.0f, theme.colors.primary);
            }

            if (c + 1 < m_columns.size()) {
                ctx.draw_rect(Rect::from_xywh(hr.max_x - 1.0f, hr.min_y + 8.0f, 1.0f, hr.height() - 16.0f),
                              theme.colors.divider_stroke, 0.0f, 0.0f);
            }
            cx += cw;
        }
        ctx.pop_clip();
        ctx.draw_rect(Rect::from_xywh(1.0f, kHeaderH, std::max(0.0f, w - 2.0f), 1.0f),
                      theme.colors.divider_stroke, 0.0f, 0.0f);

        // 2. O(1) 视口虚拟化数据行绘制
        Rect body_vp = Rect::from_xywh(1.0f, kHeaderH + 1.0f, std::max(0.0f, w - 2.0f), std::max(0.0f, h - kHeaderH - 2.0f));
        ctx.push_clip(body_vp, 4.0f);

        float scroll_y = m_physics.offset();
        size_t first = static_cast<size_t>(std::max(0.0f, std::floor(scroll_y / m_row_height)));
        size_t vis = static_cast<size_t>(std::ceil(body_vp.height() / m_row_height)) + 2;
        size_t end = std::min(m_order.size(), first + vis);
        m_last_visible_count = (end > first) ? (end - first) : 0;

        for (size_t vi = first; vi < end; ++vi) {
            size_t orig = m_order[vi];
            float ry = body_vp.min_y + static_cast<float>(vi) * m_row_height - scroll_y;
            Rect rr = Rect::from_xywh(body_vp.min_x, ry, body_vp.width(), m_row_height);

            bool is_sel = (static_cast<int>(orig) == m_selected_orig_row);
            bool is_hov = (static_cast<int>(vi) == m_hovered_vis_row);
            if (is_sel) {
                ctx.draw_rect(rr, theme.colors.subtle_fill_secondary, 0.0f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(rr.min_x + 2.0f, rr.min_y + 6.0f, 3.0f, rr.height() - 12.0f),
                              theme.colors.primary, 1.5f, 0.0f);
            } else if (is_hov) {
                ctx.draw_rect(rr, theme.colors.subtle_fill_tertiary, 0.0f, 0.0f);
            } else if (vi % 2 == 1) {
                ctx.draw_rect(rr, stripe, 0.0f, 0.0f);
            }

            float cell_x = body_vp.min_x;
            const auto& row_data = m_rows[orig];
            for (size_t c = 0; c < m_columns.size(); ++c) {
                float cw = (c < m_col_widths.size()) ? m_col_widths[c] : 120.0f;
                if (c < row_data.size() && !row_data[c].empty()) {
                    Rect cell_clip = Rect::from_xywh(cell_x + 8.0f, ry, std::max(0.0f, cw - 16.0f), m_row_height);
                    ctx.push_clip(cell_clip);
                    Vec2 ts = TextMeasure::measure(row_data[c], 13.0f);
                    ctx.draw_text(row_data[c],
                                  PointF{cell_x + 12.0f, std::round(ry + (m_row_height - ts.y) * 0.5f)},
                                  13.0f, text_c);
                    ctx.pop_clip();
                }
                cell_x += cw;
            }
            ctx.draw_rect(Rect::from_xywh(rr.min_x, rr.max_y - 1.0f, rr.width(), 1.0f),
                          Color{theme.colors.divider_stroke.r, theme.colors.divider_stroke.g, theme.colors.divider_stroke.b,
                                theme.colors.divider_stroke.a * 0.5f},
                          0.0f, 0.0f);
        }

        float total_h = static_cast<float>(m_rows.size()) * m_row_height;
        m_scrollbar.paint(ctx, body_vp, total_h, scroll_y, theme);
        ctx.pop_clip();
    }

private:
    static constexpr float kHeaderH = 36.0f;
    inline static const std::string kEmpty{};

    void compute_column_widths() {
        m_col_widths.assign(m_columns.size(), 120.0f);
        float avail = std::max(40.0f, size().x - 14.0f);
        float fixed_sum = 0.0f;
        int flex_cnt = 0;
        for (size_t i = 0; i < m_columns.size(); ++i) {
            if (m_columns[i].width > 0.0f) {
                m_col_widths[i] = m_columns[i].width;
                fixed_sum += m_columns[i].width;
            } else {
                ++flex_cnt;
            }
        }
        if (flex_cnt > 0) {
            float flex_w = std::max(80.0f, (avail - fixed_sum) / static_cast<float>(flex_cnt));
            for (size_t i = 0; i < m_columns.size(); ++i) {
                if (m_columns[i].width <= 0.0f) m_col_widths[i] = flex_w;
            }
        }
    }

    int hit_header_col(Vec2 local) const noexcept {
        if (local.y < 1.0f || local.y > kHeaderH) return -1;
        float x = 1.0f;
        for (size_t i = 0; i < m_col_widths.size(); ++i) {
            if (local.x >= x && local.x < x + m_col_widths[i]) return static_cast<int>(i);
            x += m_col_widths[i];
        }
        return -1;
    }

    int hit_body_row(Vec2 local) const noexcept {
        if (local.y <= kHeaderH || local.y >= size().y - 1.0f) return -1;
        int idx = static_cast<int>(std::floor((local.y - kHeaderH - 1.0f + m_physics.offset()) / m_row_height));
        return (idx >= 0 && idx < static_cast<int>(m_order.size())) ? idx : -1;
    }

    Rect body_rect() const noexcept {
        return Rect::from_xywh(1.0f, kHeaderH + 1.0f, std::max(0.0f, size().x - 2.0f), std::max(0.0f, size().y - kHeaderH - 2.0f));
    }

    void setup_behavior() {
        on_pointer_preview = [this](PointerEvent& e) {
            if (e.device_kind != PointerDeviceKind::Touch) return;
            Rect vp = body_rect();
            float total_h = static_cast<float>(m_rows.size()) * m_row_height;
            if (e.type == PlatformEventType::PointerDown && e.local_position.y > kHeaderH) {
                auto m = m_scrollbar.compute_metrics(vp, total_h, m_physics.offset());
                if (m_scrollbar.hit_test(e.local_position, m) != WinUIScrollBar::HitPart::None) return;
                m_scrollbar.handle_pointer_up();
                if (m_physics.max_scroll() > 0.0f) {
                    m_touch_tracking = true;
                    m_touch_dragging = false;
                    m_touch_start_y = e.position.y;
                    m_touch_last_y = e.position.y;
                    m_physics.begin_touch_drag();
                }
            } else if (e.type == PlatformEventType::PointerMove && m_touch_tracking && !m_touch_dragging && !m_scrollbar.is_dragging()) {
                if (std::abs(e.position.y - m_touch_start_y) > 8.0f) {
                    m_touch_dragging = true;
                    m_physics.apply_touch_drag(e.position.y - m_touch_last_y);
                    m_touch_last_y = e.position.y;
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.gesture_capture_target = this;
                    e.stop_propagation();
                }
            } else if (e.type == PlatformEventType::PointerUp) {
                m_touch_tracking = false;
                m_touch_dragging = false;
            }
        };

        on_pointer_enter = [this](PointerEvent&) {
            m_viewport_hovered = true;
            mark_needs_paint();
        };

        on_pointer_exit = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_hovered_col = -1;
            m_hovered_vis_row = -1;
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (m_touch_dragging && e.device_kind == PointerDeviceKind::Touch) {
                m_physics.apply_touch_drag(e.position.y - m_touch_last_y);
                m_touch_last_y = e.position.y;
                m_scrollbar.notify_activity();
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (e.type == PlatformEventType::PointerScroll) {
                if (m_physics.max_scroll() > 0.0f) {
                    m_physics.add_wheel_delta(e.delta.y);
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.stop_propagation();
                }
                return;
            }
            Rect vp = body_rect();
            float total_h = static_cast<float>(m_rows.size()) * m_row_height;
            bool consumed = false;
            if (m_scrollbar.handle_pointer_move(e.local_position, vp, total_h, m_physics, consumed)) {
                mark_needs_paint();
            }
            if (consumed) { e.stop_propagation(); return; }

            int hc = hit_header_col(e.local_position);
            int hr = hit_body_row(e.local_position);
            cursor_shape = ((hc >= 0 && m_columns[static_cast<size_t>(hc)].sortable) || hr >= 0)
                ? CursorShape::PointingHand : CursorShape::Arrow;
            if (hc != m_hovered_col || hr != m_hovered_vis_row) {
                m_hovered_col = hc;
                m_hovered_vis_row = hr;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            Rect vp = body_rect();
            float total_h = static_cast<float>(m_rows.size()) * m_row_height;
            if (e.local_position.y > kHeaderH &&
                m_scrollbar.handle_pointer_down(e.local_position, vp, total_h, m_physics)) {
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            int hc = hit_header_col(e.local_position);
            if (hc >= 0) {
                sort_by_column(static_cast<size_t>(hc));
                e.stop_propagation();
                return;
            }
            int hr = hit_body_row(e.local_position);
            if (hr >= 0 && hr < static_cast<int>(m_order.size())) {
                m_selected_orig_row = static_cast<int>(m_order[static_cast<size_t>(hr)]);
                mark_needs_paint();
                if (m_on_row_selected) m_on_row_selected(static_cast<size_t>(m_selected_orig_row));
                e.stop_propagation();
            }
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (m_touch_dragging && e.device_kind == PointerDeviceKind::Touch) {
                m_touch_dragging = false;
                m_touch_tracking = false;
                m_physics.end_touch_drag();
                mark_needs_paint();
                return;
            }
            m_touch_tracking = false;
            if (m_scrollbar.handle_pointer_up()) mark_needs_paint();
        };
    }

    std::vector<DataGridColumn> m_columns;
    std::vector<std::vector<std::string>> m_rows;
    std::vector<size_t> m_order;
    std::vector<float> m_col_widths;
    float m_row_height{34.0f};
    float m_width{0.0f};
    float m_height{320.0f};
    int m_sort_col{-1};
    DataGridSortDirection m_sort_dir{DataGridSortDirection::None};
    int m_hovered_col{-1};
    int m_hovered_vis_row{-1};
    int m_selected_orig_row{-1};
    size_t m_last_visible_count{0};
    bool m_viewport_hovered{false};
    bool m_touch_tracking{false};
    bool m_touch_dragging{false};
    float m_touch_start_y{0.0f};
    float m_touch_last_y{0.0f};
    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;
    Fn<void(size_t)> m_on_row_selected;
};

// C++20 指定初始化器声明式 WinUI 3 DataGrid
struct DataGrid {
    std::vector<DataGridColumn> columns;
    std::vector<std::vector<std::string>> rows;
    float row_height{34.0f};
    float width{0.0f};
    float height{320.0f};
    Fn<void(size_t)> on_row_selected{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class DataGridWidget : public Widget {
public:
    explicit DataGridWidget(const DataGrid& desc) : m_desc(desc) {}
    explicit DataGridWidget(DataGrid&& desc) noexcept : m_desc(std::move(desc)) {}
    ~DataGridWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderDataGrid>();
        node->set_properties(m_desc.columns, m_desc.rows, m_desc.row_height, m_desc.width, m_desc.height, m_desc.on_row_selected);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderDataGrid>(node)) {
            n->set_properties(m_desc.columns, m_desc.rows, m_desc.row_height, m_desc.width, m_desc.height, m_desc.on_row_selected);
        }
    }

private:
    DataGrid m_desc;
};

inline DataGrid::operator WidgetPtr() const& {
    return make_widget<DataGridWidget>(*this);
}
inline DataGrid::operator WidgetPtr() && {
    return make_widget<DataGridWidget>(std::move(*this));
}

} // namespace ktui
