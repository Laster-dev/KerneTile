#pragma once

#include "data_grid_data.hpp"
#include "data_grid_style.hpp"
#include "../common/fluent_icons.hpp"
#include "../scroll/scrollbar.hpp"
#include "../scroll/smooth_scroll_physics.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <numeric>

namespace ktui {

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
                        Fn<void(size_t)> on_row_selected,
                        DataGridCellOverflow cell_overflow = DataGridCellOverflow::Ellipsis) {
        m_columns = std::move(columns);
        m_rows = std::move(rows);
        m_row_height = std::max(26.0f, row_height);
        m_width = width;
        m_height = height;
        m_on_row_selected = std::move(on_row_selected);
        m_cell_overflow = cell_overflow;
        m_sort_col = -1;
        m_sort_dir = DataGridSortDirection::None;
        m_selected_orig_row = -1;
        m_selected_mask.assign(m_rows.size(), false);
        m_user_resized = false;
        m_sel_anim_init = false;
        m_order.resize(m_rows.size());
        std::iota(m_order.begin(), m_order.end(), size_t{0});
        mark_needs_layout();
    }

    void set_cell_overflow(DataGridCellOverflow mode) noexcept {
        if (m_cell_overflow != mode) {
            m_cell_overflow = mode;
            mark_needs_paint();
        }
    }

    [[nodiscard]] DataGridCellOverflow cell_overflow() const noexcept { return m_cell_overflow; }

    [[nodiscard]] size_t row_count() const noexcept { return m_rows.size(); }
    [[nodiscard]] int sorted_column() const noexcept { return m_sort_col; }
    [[nodiscard]] DataGridSortDirection sort_direction() const noexcept { return m_sort_dir; }
    [[nodiscard]] int selected_row() const noexcept { return m_selected_orig_row; }
    [[nodiscard]] size_t selected_count() const noexcept {
        size_t c = 0;
        for (bool b : m_selected_mask) if (b) ++c;
        return c;
    }
    [[nodiscard]] size_t visible_row_count() const noexcept { return m_last_visible_count; }
    [[nodiscard]] float scroll_offset() const noexcept { return m_physics.offset(); }
    [[nodiscard]] float max_scroll() const noexcept { return m_physics.max_scroll(); }

    void set_rubber_band_enabled(bool enabled) noexcept {
        if (m_physics.is_rubber_band_enabled() != enabled) {
            m_physics.set_rubber_band_enabled(enabled);
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool is_rubber_band_enabled() const noexcept { return m_physics.is_rubber_band_enabled(); }
    [[nodiscard]] bool rubber_band_enabled() const noexcept { return m_physics.rubber_band_enabled(); }

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
        if (!m_user_resized) compute_column_widths();
        float body_h = std::max(0.0f, size().y - kHeaderH);
        float total_h = static_cast<float>(m_rows.size()) * m_row_height;
        m_physics.set_viewport_extent(body_h);
        m_physics.set_max_scroll(std::max(0.0f, total_h - body_h));
    }

    void paint(PaintContext& ctx) override {
        bool anim_scroll = m_physics.update();
        bool anim_bar = m_scrollbar.update_animations(m_viewport_hovered);

        // 推进单选高亮指示条平滑滑动动画
        bool anim_sel = false;
        int primary_vis = find_visible_index(m_selected_orig_row);
        if (primary_vis >= 0) {
            float target_y = static_cast<float>(primary_vis) * m_row_height;
            if (!m_sel_anim_init) {
                m_sel_anim_y = target_y;
                m_sel_anim_init = true;
            } else if (std::abs(m_sel_anim_y - target_y) > 0.2f) {
                auto now = std::chrono::steady_clock::now();
                float dt = m_has_sel_time
                    ? std::clamp(std::chrono::duration<float>(now - m_last_sel_time).count(), 0.001f, 0.05f)
                    : (1.0f / 60.0f);
                m_last_sel_time = now;
                m_has_sel_time = true;
                m_sel_anim_y += (target_y - m_sel_anim_y) * (1.0f - std::exp(-dt * 24.0f));
                anim_sel = true;
            } else {
                m_sel_anim_y = target_y;
                m_has_sel_time = false;
            }
        }
        if (anim_scroll || anim_bar || anim_sel) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        const auto resolved_style = DataGridStyleResolver::resolve(theme);
        const float w = size().x;
        const float h = size().y;

        Rect panel = Rect::from_xywh(0.0f, 0.0f, w, h);
        ctx.draw_rect(panel, theme.colors.card_surface, 6.0f, 0.0f);
        ctx.draw_rect(panel, theme.colors.card_stroke, 6.0f, 1.0f);

        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.94f} : Color{0.0f, 0.0f, 0.0f, 0.90f};
        Color head_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.78f} : Color{0.0f, 0.0f, 0.0f, 0.72f};
        Color stripe = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.022f} : Color{0.0f, 0.0f, 0.0f, 0.020f};
        Color head_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.045f} : Color{0.0f, 0.0f, 0.0f, 0.035f};

        // 1. 固定表头 (支持列宽分隔线悬停高亮与拖拽调宽)
        ctx.push_clip(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, w - 2.0f), kHeaderH), 5.0f);
        ctx.draw_rect(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, w - 2.0f), kHeaderH), head_bg, 0.0f, 0.0f);

        float cx = 1.0f;
        for (size_t c = 0; c < m_columns.size(); ++c) {
            float cw = (c < m_col_widths.size()) ? m_col_widths[c] : 120.0f;
            Rect hr = Rect::from_xywh(cx, 1.0f, cw, kHeaderH - 1.0f);
            if (static_cast<int>(c) == m_hovered_col && m_hover_divider < 0 && m_resizing_col < 0 && m_columns[c].sortable) {
                ctx.draw_rect(hr, theme.colors.subtle_fill_secondary, 0.0f, 0.0f);
            }
            bool has_sort_icon = (m_sort_col == static_cast<int>(c) && m_sort_dir != DataGridSortDirection::None);
            float title_max_w = std::max(0.0f, hr.width() - (has_sort_icon ? 34.0f : 20.0f));
            ctx.push_clip(Rect::from_xywh(hr.min_x + 8.0f, hr.min_y, std::max(0.0f, hr.width() - 16.0f), hr.height()));
            std::string htext = (m_cell_overflow == DataGridCellOverflow::Ellipsis)
                ? truncate_with_ellipsis(m_columns[c].title, title_max_w, 12.5f)
                : m_columns[c].title;
            Vec2 ts = TextMeasure::measure(htext, 12.5f);
            ctx.draw_text(htext,
                          PointF{hr.min_x + 12.0f, std::round(hr.min_y + (hr.height() - ts.y) * 0.5f)},
                          12.5f, (m_sort_col == static_cast<int>(c)) ? resolved_style.accent : head_c);
            ctx.pop_clip();

            if (has_sort_icon) {
                Rect sr = Rect::from_xywh(hr.max_x - 22.0f, hr.min_y, 18.0f, hr.height());
                const char* icon = (m_sort_dir == DataGridSortDirection::Ascending)
                    ? fluent_icons::kChevronUp : fluent_icons::kChevronDown;
                fluent_icons::draw_centered(ctx, icon, sr, 10.0f, resolved_style.accent);
            }

            if (c + 1 < m_columns.size()) {
                bool div_hot = (m_resizing_col == static_cast<int>(c) || m_hover_divider == static_cast<int>(c));
                Color dc = div_hot ? resolved_style.accent : theme.colors.divider_stroke;
                ctx.draw_rect(Rect::from_xywh(hr.max_x - 1.0f, hr.min_y + (div_hot ? 2.0f : 8.0f),
                                              div_hot ? 2.0f : 1.0f, hr.height() - (div_hot ? 4.0f : 16.0f)),
                              dc, 0.0f, 0.0f);
            }
            cx += cw;
        }
        ctx.pop_clip();
        ctx.draw_rect(Rect::from_xywh(1.0f, kHeaderH, std::max(0.0f, w - 2.0f), 1.0f),
                      theme.colors.divider_stroke, 0.0f, 0.0f);

        // 2. O(1) 视口虚拟化数据行绘制 (支持默认列表边缘渐隐羽化与表头接触分隔线)
        Rect body_vp = Rect::from_xywh(1.0f, kHeaderH + 1.0f, std::max(0.0f, w - 2.0f), std::max(0.0f, h - kHeaderH - 2.0f));
        float scroll_y = std::round(m_physics.offset());
        float top_f = 0.0f;
        float bot_f = 0.0f;
        if (m_edge_feather && m_feather_size > 0.5f && body_vp.height() > 8.0f) {
            const float cap = std::min(m_feather_size, body_vp.height() * 0.35f);
            top_f = std::clamp(scroll_y, 0.0f, cap);
            const float rem = std::max(0.0f, m_physics.max_scroll() - scroll_y);
            bot_f = std::clamp(rem, 0.0f, cap);
        }
        ctx.push_feathered_clip(body_vp, 4.0f, top_f, bot_f);

        size_t first = static_cast<size_t>(std::max(0.0f, std::floor(scroll_y / m_row_height)));
        size_t vis = static_cast<size_t>(std::ceil(body_vp.height() / m_row_height)) + 2;
        size_t end = std::min(m_order.size(), first + vis);
        m_last_visible_count = (end > first) ? (end - first) : 0;

        Color multi_sel_bg = resolved_style.accent;
        multi_sel_bg.a = theme.is_dark ? 0.18f : 0.13f;

        for (size_t vi = first; vi < end; ++vi) {
            size_t orig = m_order[vi];
            float ry = body_vp.min_y + static_cast<float>(vi) * m_row_height - scroll_y;
            Rect rr = Rect::from_xywh(body_vp.min_x, ry, body_vp.width(), m_row_height);

            bool is_sel = (orig < m_selected_mask.size() && m_selected_mask[orig]);
            bool is_hov = (!m_marquee_active && static_cast<int>(vi) == m_hovered_vis_row);
            if (is_sel) {
                ctx.draw_rect(rr, (selected_count() > 1) ? multi_sel_bg : theme.colors.subtle_fill_secondary, 0.0f, 0.0f);
                if (selected_count() > 1) {
                    ctx.draw_rect(Rect::from_xywh(rr.min_x + 2.0f, rr.min_y + 6.0f, 3.0f, rr.height() - 12.0f),
                                  resolved_style.accent, 1.5f, 0.0f);
                }
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
                    float avail_w = std::max(0.0f, cw - 20.0f);
                    Rect cell_clip = Rect::from_xywh(cell_x + 8.0f, ry, std::max(0.0f, cw - 16.0f), m_row_height);
                    ctx.push_clip(cell_clip);
                    std::string ctext = (m_cell_overflow == DataGridCellOverflow::Ellipsis)
                        ? truncate_with_ellipsis(row_data[c], avail_w, 13.0f)
                        : row_data[c];
                    Vec2 ts = TextMeasure::measure(ctext, 13.0f, FontWeight::Regular, false, "", m_tabular_nums);
                    ctx.draw_text(ctext,
                                  PointF{cell_x + 12.0f, std::round(ry + (m_row_height - ts.y) * 0.5f)},
                                  13.0f, text_c, FontWeight::Regular, false, "", m_tabular_nums);
                    ctx.pop_clip();
                }
                cell_x += cw;
            }
            ctx.draw_rect(Rect::from_xywh(rr.min_x, rr.max_y - 1.0f, rr.width(), 1.0f),
                          Color{theme.colors.divider_stroke.r, theme.colors.divider_stroke.g, theme.colors.divider_stroke.b,
                                theme.colors.divider_stroke.a * 0.5f},
                          0.0f, 0.0f);
        }

        // 单选态平滑滑动的 Fluent 2 左侧主题色指示条
        if (m_sel_anim_init && primary_vis >= 0 && selected_count() <= 1) {
            float ay = body_vp.min_y + m_sel_anim_y - scroll_y;
            if (ay + m_row_height >= body_vp.min_y && ay <= body_vp.max_y) {
                ctx.draw_rect(Rect::from_xywh(body_vp.min_x + 2.0f, ay + 6.0f, 3.0f, m_row_height - 12.0f),
                              resolved_style.accent, 1.5f, 0.0f);
            }
        }

        // 3. Win32 ListView 风格拉框多选橡皮筋矩形 (Rubber-band Marquee Selection)
        if (m_marquee_active) {
            float x0 = std::min(m_marquee_start_x, m_marquee_cur_x);
            float x1 = std::max(m_marquee_start_x, m_marquee_cur_x);
            float y0 = body_vp.min_y + std::min(m_marquee_start_cy, m_marquee_cur_cy) - scroll_y;
            float y1 = body_vp.min_y + std::max(m_marquee_start_cy, m_marquee_cur_cy) - scroll_y;
            Rect mr = Rect::from_xywh(x0, y0, std::max(1.0f, x1 - x0), std::max(1.0f, y1 - y0));
            Color mfill = resolved_style.accent;
            mfill.a = theme.is_dark ? 0.22f : 0.16f;
            Color mstroke = resolved_style.accent;
            mstroke.a = 0.85f;
            ctx.draw_rect(mr, mfill, 2.0f, 0.0f);
            ctx.draw_rect(mr, mstroke, 2.0f, 1.0f);
        }

        ctx.pop_clip();

        if (m_scroll_hairline && scroll_y > 0.25f) {
            const float t = std::clamp(scroll_y / std::max(1.0f, m_hairline_fade_range), 0.0f, 1.0f);
            const Color hl = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, 0.14f * t}
                : Color{0.0f, 0.0f, 0.0f, 0.12f * t};
            ctx.draw_rect(Rect::from_xywh(body_vp.min_x, body_vp.min_y, body_vp.width(), 1.0f), hl, 0.0f, 0.0f);
        }

        ctx.push_clip(body_vp, 4.0f);
        float total_h = static_cast<float>(m_rows.size()) * m_row_height;
        m_scrollbar.paint(ctx, body_vp, total_h, scroll_y, theme);
        ctx.pop_clip();
    }

    void set_edge_feather(bool enabled, float feather_px = 20.0f) noexcept {
        const float f = std::max(0.0f, feather_px);
        if (m_edge_feather != enabled || std::abs(m_feather_size - f) > 0.01f) {
            m_edge_feather = enabled;
            m_feather_size = f;
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool edge_feather_enabled() const noexcept { return m_edge_feather; }

    void set_scroll_hairline(bool enabled, float fade_range = 18.0f) noexcept {
        const float fr = std::max(1.0f, fade_range);
        if (m_scroll_hairline != enabled || std::abs(m_hairline_fade_range - fr) > 0.01f) {
            m_scroll_hairline = enabled;
            m_hairline_fade_range = fr;
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool scroll_hairline_enabled() const noexcept { return m_scroll_hairline; }

    void set_tabular_nums(bool enabled) noexcept {
        if (m_tabular_nums != enabled) {
            m_tabular_nums = enabled;
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool tabular_nums_enabled() const noexcept { return m_tabular_nums; }

private:
    static constexpr float kHeaderH = 36.0f;
    inline static const std::string kEmpty{};

    // UTF-8 安全省略号截断 (列宽不足时将超出部分替换为 "...")
    static std::string truncate_with_ellipsis(const std::string& text, float max_w, float font_size) {
        if (text.empty() || max_w <= 4.0f) return {};
        if (TextMeasure::measure_width(text, font_size) <= max_w) return text;
        static constexpr const char* kEllipsis = "...";
        float ell_w = TextMeasure::measure_width(kEllipsis, font_size);
        if (max_w <= ell_w) return ".";
        float target_w = max_w - ell_w;

        // 收集 UTF-8 字符边界偏移量并二分查找最大可容纳前缀
        std::vector<size_t> bounds;
        bounds.reserve(text.size() + 1);
        for (size_t i = 0; i < text.size(); ) {
            bounds.push_back(i);
            unsigned char c = static_cast<unsigned char>(text[i]);
            size_t step = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
            i = std::min(text.size(), i + step);
        }
        bounds.push_back(text.size());

        size_t lo = 0, hi = bounds.size() - 1;
        while (lo < hi) {
            size_t mid = (lo + hi + 1) / 2;
            std::string_view sv(text.data(), bounds[mid]);
            if (TextMeasure::measure_width(sv, font_size) <= target_w) lo = mid;
            else hi = mid - 1;
        }
        std::string out(text.data(), bounds[lo]);
        out += kEllipsis;
        return out;
    }

    int find_visible_index(int orig_row) const noexcept {
        if (orig_row < 0) return -1;
        for (size_t i = 0; i < m_order.size(); ++i) {
            if (static_cast<int>(m_order[i]) == orig_row) return static_cast<int>(i);
        }
        return -1;
    }

    int hit_header_divider(Vec2 local) const noexcept {
        if (local.y < 1.0f || local.y > kHeaderH + 2.0f) return -1;
        float x = 1.0f;
        for (size_t i = 0; i + 1 < m_col_widths.size(); ++i) {
            x += m_col_widths[i];
            if (std::abs(local.x - x) <= 4.5f) return static_cast<int>(i);
        }
        return -1;
    }

    void update_marquee_selection() {
        float y0 = std::min(m_marquee_start_cy, m_marquee_cur_cy);
        float y1 = std::max(m_marquee_start_cy, m_marquee_cur_cy);
        std::fill(m_selected_mask.begin(), m_selected_mask.end(), false);
        int last_orig = -1;
        for (size_t vi = 0; vi < m_order.size(); ++vi) {
            float ry0 = static_cast<float>(vi) * m_row_height;
            float ry1 = ry0 + m_row_height;
            if (ry1 >= y0 && ry0 <= y1) {
                size_t orig = m_order[vi];
                if (orig < m_selected_mask.size()) {
                    m_selected_mask[orig] = true;
                    last_orig = static_cast<int>(orig);
                }
            }
        }
        if (last_orig >= 0) m_selected_orig_row = last_orig;
    }

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
                if (m_physics.max_scroll() > 0.0f || m_physics.is_rubber_band_enabled()) {
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

        on_pointer_exit = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_hovered_col = -1;
            m_hover_divider = -1;
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
                if (m_physics.max_scroll() > 0.0f || m_physics.is_rubber_band_enabled()) {
                    m_physics.add_wheel_delta(e.delta.y);
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.stop_propagation();
                }
                return;
            }
            if (m_resizing_col >= 0 && m_resizing_col < static_cast<int>(m_col_widths.size())) {
                float dx = e.local_position.x - m_resize_start_x;
                m_col_widths[static_cast<size_t>(m_resizing_col)] = std::max(56.0f, m_resize_start_w + dx);
                m_user_resized = true;
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (m_marquee_tracking) {
                Rect vp = body_rect();
                float cur_x = std::clamp(e.local_position.x, vp.min_x, vp.max_x);
                float cur_cy = std::max(0.0f, e.local_position.y - vp.min_y + m_physics.offset());
                if (!m_marquee_active) {
                    float dist = std::hypot(cur_x - m_marquee_start_x, cur_cy - m_marquee_start_cy);
                    if (dist >= 5.0f) m_marquee_active = true;
                }
                if (m_marquee_active) {
                    m_marquee_cur_x = cur_x;
                    m_marquee_cur_cy = cur_cy;
                    update_marquee_selection();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
            }
            Rect vp = body_rect();
            float total_h = static_cast<float>(m_rows.size()) * m_row_height;
            bool consumed = false;
            if (m_scrollbar.handle_pointer_move(e.local_position, vp, total_h, m_physics, consumed)) {
                mark_needs_paint();
            }
            if (consumed) { e.stop_propagation(); return; }

            int hdiv = hit_header_divider(e.local_position);
            int hc = (hdiv >= 0) ? -1 : hit_header_col(e.local_position);
            int hr = (hdiv >= 0) ? -1 : hit_body_row(e.local_position);
            cursor_shape = (hdiv >= 0) ? CursorShape::ResizeEW : CursorShape::Arrow;
            if (hdiv != m_hover_divider || hc != m_hovered_col || hr != m_hovered_vis_row) {
                m_hover_divider = hdiv;
                m_hovered_col = hc;
                m_hovered_vis_row = hr;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int hdiv = hit_header_divider(e.local_position);
            if (hdiv >= 0 && hdiv < static_cast<int>(m_col_widths.size())) {
                m_resizing_col = hdiv;
                m_resize_start_x = e.local_position.x;
                m_resize_start_w = m_col_widths[static_cast<size_t>(hdiv)];
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
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
            if (e.local_position.y > kHeaderH && e.device_kind != PointerDeviceKind::Touch) {
                m_marquee_tracking = true;
                m_marquee_active = false;
                m_marquee_start_x = m_marquee_cur_x = std::clamp(e.local_position.x, vp.min_x, vp.max_x);
                m_marquee_start_cy = m_marquee_cur_cy = std::max(0.0f, e.local_position.y - vp.min_y + m_physics.offset());
            }
            int hr = hit_body_row(e.local_position);
            if (hr >= 0 && hr < static_cast<int>(m_order.size())) {
                size_t orig = m_order[static_cast<size_t>(hr)];
                if (e.modifiers.ctrl) {
                    if (orig < m_selected_mask.size()) m_selected_mask[orig] = !m_selected_mask[orig];
                    m_selected_orig_row = static_cast<int>(orig);
                } else if (e.modifiers.shift && m_anchor_vis_row >= 0) {
                    int lo = std::min(m_anchor_vis_row, hr);
                    int hi = std::max(m_anchor_vis_row, hr);
                    std::fill(m_selected_mask.begin(), m_selected_mask.end(), false);
                    for (int vi = lo; vi <= hi && vi < static_cast<int>(m_order.size()); ++vi) {
                        size_t o = m_order[static_cast<size_t>(vi)];
                        if (o < m_selected_mask.size()) m_selected_mask[o] = true;
                    }
                    m_selected_orig_row = static_cast<int>(orig);
                } else {
                    std::fill(m_selected_mask.begin(), m_selected_mask.end(), false);
                    if (orig < m_selected_mask.size()) m_selected_mask[orig] = true;
                    m_selected_orig_row = static_cast<int>(orig);
                    m_anchor_vis_row = hr;
                }
                m_last_sel_time = std::chrono::steady_clock::now();
                m_has_sel_time = true;
                mark_needs_paint();
                if (m_on_row_selected) m_on_row_selected(static_cast<size_t>(m_selected_orig_row));
                e.stop_propagation();
            }
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (m_resizing_col >= 0) { m_resizing_col = -1; mark_needs_paint(); return; }
            if (m_marquee_tracking || m_marquee_active) {
                bool was_marquee = m_marquee_active;
                m_marquee_tracking = false;
                m_marquee_active = false;
                if (was_marquee && m_selected_orig_row >= 0 && m_on_row_selected) {
                    m_on_row_selected(static_cast<size_t>(m_selected_orig_row));
                }
                mark_needs_paint();
                return;
            }
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
    std::vector<bool> m_selected_mask;
    float m_row_height{34.0f};
    float m_width{0.0f};
    float m_height{320.0f};
    int m_sort_col{-1};
    DataGridSortDirection m_sort_dir{DataGridSortDirection::None};
    int m_hovered_col{-1};
    int m_hover_divider{-1};
    int m_resizing_col{-1};
    float m_resize_start_x{0.0f};
    float m_resize_start_w{0.0f};
    bool m_user_resized{false};
    int m_hovered_vis_row{-1};
    int m_selected_orig_row{-1};
    int m_anchor_vis_row{-1};
    float m_sel_anim_y{0.0f};
    bool m_sel_anim_init{false};
    bool m_has_sel_time{false};
    std::chrono::steady_clock::time_point m_last_sel_time{};
    bool m_marquee_tracking{false};
    bool m_marquee_active{false};
    float m_marquee_start_x{0.0f};
    float m_marquee_start_cy{0.0f};
    float m_marquee_cur_x{0.0f};
    float m_marquee_cur_cy{0.0f};
    size_t m_last_visible_count{0};
    bool m_viewport_hovered{false};
    bool m_touch_tracking{false};
    bool m_touch_dragging{false};
    float m_touch_start_y{0.0f};
    float m_touch_last_y{0.0f};
    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;
    DataGridCellOverflow m_cell_overflow{DataGridCellOverflow::Ellipsis};
    Fn<void(size_t)> m_on_row_selected;
    bool m_edge_feather{true};
    float m_feather_size{20.0f};
    bool m_scroll_hairline{false};
    float m_hairline_fade_range{18.0f};
    bool m_tabular_nums{false};
};

// C++20 指定初始化器声明式 WinUI 3 DataGrid
struct DataGrid : BindableMixin<DataGrid, RenderDataGrid> {
    std::vector<DataGridColumn> columns;
    std::vector<std::vector<std::string>> rows;
    float row_height{34.0f};
    float width{0.0f};
    float height{320.0f};
    Fn<void(size_t)> on_row_selected{nullptr};
    DataGridCellOverflow cell_overflow{DataGridCellOverflow::Ellipsis};
    bool rubber_band_overscroll{true};
    bool edge_feather{true};
    float feather_size{20.0f};
    bool scroll_hairline{false};
    float hairline_fade_range{18.0f};
    bool tabular_nums{false};

    DataGrid& rubber_band(bool enabled) & noexcept {
        rubber_band_overscroll = enabled;
        return *this;
    }
    DataGrid&& rubber_band(bool enabled) && noexcept {
        rubber_band_overscroll = enabled;
        return std::move(*this);
    }

    DataGrid& feather(bool enabled, float size = 20.0f) & noexcept {
        edge_feather = enabled;
        feather_size = size;
        return *this;
    }
    DataGrid&& feather(bool enabled, float size = 20.0f) && noexcept {
        edge_feather = enabled;
        feather_size = size;
        return std::move(*this);
    }

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
        node->set_properties(m_desc.columns, m_desc.rows, m_desc.row_height, m_desc.width, m_desc.height, m_desc.on_row_selected, m_desc.cell_overflow);
        node->set_rubber_band_enabled(m_desc.rubber_band_overscroll);
        node->set_edge_feather(m_desc.edge_feather, m_desc.feather_size);
        node->set_scroll_hairline(m_desc.scroll_hairline, m_desc.hairline_fade_range);
        node->set_tabular_nums(m_desc.tabular_nums);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderDataGrid>(node)) {
            n->set_properties(m_desc.columns, m_desc.rows, m_desc.row_height, m_desc.width, m_desc.height, m_desc.on_row_selected, m_desc.cell_overflow);
            n->set_rubber_band_enabled(m_desc.rubber_band_overscroll);
            n->set_edge_feather(m_desc.edge_feather, m_desc.feather_size);
            n->set_scroll_hairline(m_desc.scroll_hairline, m_desc.hairline_fade_range);
            n->set_tabular_nums(m_desc.tabular_nums);
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
 // namespace ktui
