#pragma once

#include "calendar_date_picker_data.hpp"
#include "calendar_date_picker_style.hpp"
#include "../common/fluent_chrome.hpp"
#include "../common/fluent_icons.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/popup_handle.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace ktui {

// WinUI 3 CalendarDatePicker 悬浮月历弹窗 (支持单日 & 日期区间选择 + 滚轮切月与平滑滑入动画)
class RenderCalendarPopup : public RenderBox {
public:
    RenderCalendarPopup(CalendarDate selected,
                        CalendarDate min_date,
                        CalendarDate max_date,
                        Fn<void(const CalendarDate&)> on_select)
        : m_selected(selected),
          m_min_date(min_date),
          m_max_date(max_date),
          m_on_select(std::move(on_select)) {
        cursor_shape = CursorShape::Arrow;
        m_today = CalendarDate::today();
        CalendarDate anchor = m_selected.is_valid() ? m_selected : m_today;
        m_view_year = anchor.year;
        m_view_month = anchor.month;
        setup_behavior();
    }

    RenderCalendarPopup(CalendarDate start_date,
                        CalendarDate end_date,
                        CalendarDate min_date,
                        CalendarDate max_date,
                        Fn<void(const CalendarDate&, const CalendarDate&)> on_range_select)
        : m_range_mode(true),
          m_range_start(start_date),
          m_range_end(end_date),
          m_min_date(min_date),
          m_max_date(max_date),
          m_on_range_select(std::move(on_range_select)) {
        cursor_shape = CursorShape::Arrow;
        m_today = CalendarDate::today();
        CalendarDate anchor = m_range_start.is_valid() ? m_range_start : m_today;
        m_view_year = anchor.year;
        m_view_month = anchor.month;
        setup_behavior();
    }

    ~RenderCalendarPopup() override = default;

    [[nodiscard]] Vec2 compute_ideal_size() const noexcept {
        return Vec2{kPanelW, kPanelH};
    }

    void perform_layout() override {
        set_size(compute_ideal_size());
    }

    void paint(PaintContext& ctx) override {
        auto st = m_surface.tick();
        if (st.animating) mark_needs_paint();

        // 月份切换平滑滑入动画
        if (m_month_anim_active) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::clamp(std::chrono::duration<float>(now - m_month_anim_time).count(), 0.001f, 0.05f);
            m_month_anim_time = now;
            float decay = std::exp(-dt * 18.0f);
            m_month_slide_y *= decay;
            m_month_anim_alpha = 1.0f - (1.0f - m_month_anim_alpha) * decay;
            if (std::abs(m_month_slide_y) < 0.25f && m_month_anim_alpha > 0.99f) {
                m_month_slide_y = 0.0f;
                m_month_anim_alpha = 1.0f;
                m_month_anim_active = false;
            } else {
                mark_needs_paint();
            }
        }

        PaintContext::OffsetScope slide_scope(ctx, Vec2{0.0f, st.slide_y});

        const Theme& theme = ThemeContext::current();
        const auto resolved_style = CalendarDatePickerStyleResolver::resolve(theme);
        float alpha = st.alpha;
        float grid_alpha = alpha * m_month_anim_alpha;

        Color text_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.95f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.90f * alpha};
        Color muted_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.42f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.42f * alpha};
        Color hov_bg = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.08f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.055f * alpha};
        Color sep_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.08f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.08f * alpha};

        FluentPopupSurface::paint_panel(ctx, theme, Rect::from_xywh(0.0f, 0.0f, kPanelW, kPanelH), alpha);

        // 顶部可点击标题按钮（点击切换 日视图 -> 月视图 -> 年视图，支持快速跨年选择）
        const auto& i18n = FrameworkStrings::current();
        char title_buf[32];
        if (m_view_mode == ViewMode::Day) {
            std::snprintf(title_buf, sizeof(title_buf), i18n.calendar_year_month_fmt, m_view_year, m_view_month);
        } else if (m_view_mode == ViewMode::Month) {
            std::snprintf(title_buf, sizeof(title_buf), i18n.calendar_year_fmt, m_view_year);
        } else {
            int y0 = year_page_start();
            std::snprintf(title_buf, sizeof(title_buf), "%d - %d", y0, y0 + 11);
        }
        Rect title_r = title_btn_rect();
        if (m_hover_title) {
            ctx.draw_rect(title_r, hov_bg, 4.0f, 0.0f);
        }
        Vec2 ts = TextMeasure::measure(title_buf, 14.0f);
        ctx.draw_text(title_buf, PointF{title_r.min_x + 8.0f, std::round((kHeaderH - ts.y) * 0.5f)}, 14.0f, text_col);
        if (m_view_mode != ViewMode::Year) {
            Rect chev_r = Rect::from_xywh(title_r.min_x + 12.0f + ts.x, 8.0f, 14.0f, 28.0f);
            fluent_icons::draw_centered(ctx, fluent_icons::kChevronDown, chev_r, 9.0f, muted_col);
        }

        Rect prev_r = prev_btn_rect();
        Rect next_r = next_btn_rect();
        if (m_hover_prev) ctx.draw_rect(prev_r, hov_bg, 4.0f, 0.0f);
        if (m_hover_next) ctx.draw_rect(next_r, hov_bg, 4.0f, 0.0f);

        fluent_icons::draw_centered(ctx, fluent_icons::kChevronLeft, prev_r, 12.0f, text_col);
        fluent_icons::draw_centered(ctx, fluent_icons::kChevronRight, next_r, 12.0f, text_col);

        ctx.draw_rect(Rect::from_xywh(1.0f, kHeaderH, kPanelW - 2.0f, 1.0f), sep_col, 0.0f, 0.0f);

        Color g_text = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.95f * grid_alpha}
                                     : Color{0.0f, 0.0f, 0.0f, 0.90f * grid_alpha};
        Color g_muted = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.42f * grid_alpha}
                                      : Color{0.0f, 0.0f, 0.0f, 0.42f * grid_alpha};
        Color g_dis = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.22f * grid_alpha}
                                    : Color{0.0f, 0.0f, 0.0f, 0.22f * grid_alpha};
        Color g_hov = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f * grid_alpha}
                                    : Color{0.0f, 0.0f, 0.0f, 0.055f * grid_alpha};
        Color on_accent = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.92f * grid_alpha}
                                        : Color{1.0f, 1.0f, 1.0f, grid_alpha};
        Color accent = resolved_style.accent;
        accent.a *= grid_alpha;

        // 月视图 / 年视图 (4x3 紧凑快捷选择网格)
        if (m_view_mode != ViewMode::Day) {
            Rect body_clip = Rect::from_xywh(kGridPadX - 4.0f, kHeaderH + 8.0f,
                                             kPanelW - kGridPadX * 2.0f + 8.0f, kPanelH - kHeaderH - 16.0f);
            ctx.push_clip(body_clip);
            {
                PaintContext::OffsetScope grid_slide(ctx, Vec2{0.0f, m_month_slide_y});
                int y0 = year_page_start();
                for (int i = 0; i < 12; ++i) {
                    Rect cr = ym_cell_rect(i);
                    Rect pill = Rect::from_xywh(cr.min_x + 4.0f, cr.min_y + 8.0f, cr.width() - 8.0f, cr.height() - 16.0f);
                    bool is_cur = (m_view_mode == ViewMode::Month) ? ((i + 1) == m_view_month) : ((y0 + i) == m_view_year);
                    bool is_now = (m_view_mode == ViewMode::Month)
                        ? (m_view_year == m_today.year && (i + 1) == m_today.month)
                        : ((y0 + i) == m_today.year);
                    bool is_hov = (i == m_hover_ym);

                    if (is_cur) {
                        ctx.draw_rect(pill, accent, 6.0f, 0.0f);
                    } else if (is_now) {
                        if (is_hov) ctx.draw_rect(pill, g_hov, 6.0f, 0.0f);
                        ctx.draw_rect(pill, accent, 6.0f, 1.5f);
                    } else if (is_hov) {
                        ctx.draw_rect(pill, g_hov, 6.0f, 0.0f);
                    }

                    char lbuf[16];
                    if (m_view_mode == ViewMode::Month) {
                        std::snprintf(lbuf, sizeof(lbuf), i18n.calendar_month_fmt, i + 1);
                    } else {
                        std::snprintf(lbuf, sizeof(lbuf), "%d", y0 + i);
                    }
                    Vec2 ls = TextMeasure::measure(lbuf, 13.5f);
                    ctx.draw_text(lbuf,
                                  PointF{std::round(pill.min_x + (pill.width() - ls.x) * 0.5f),
                                         std::round(pill.min_y + (pill.height() - ls.y) * 0.5f)},
                                  13.5f, is_cur ? on_accent : g_text);
                }
            }
            ctx.pop_clip();
            return;
        }

        for (int c = 0; c < 7; ++c) {
            float cx = kGridPadX + static_cast<float>(c) * kCellSize;
            const char* wd = i18n.weekdays[static_cast<size_t>(c)];
            Vec2 ws = TextMeasure::measure(wd, 12.0f);
            ctx.draw_text(wd,
                          PointF{std::round(cx + (kCellSize - ws.x) * 0.5f),
                                 std::round(kHeaderH + 8.0f + (kWeekH - ws.y) * 0.5f)},
                          12.0f, muted_col);
        }

        // 计算区间模式下的有效起止日期 (支持悬停实时预览)
        CalendarDate eff_start, eff_end;
        bool has_range = false;
        if (m_range_mode && m_range_start.is_valid()) {
            CalendarDate second = m_range_end;
            if (m_picking_end && m_hover_cell >= 0) {
                CellInfo hinfo = cell_info(m_hover_cell);
                if (is_in_range(hinfo.date)) second = hinfo.date;
            }
            if (second.is_valid()) {
                if (m_range_start.compare(second) <= 0) {
                    eff_start = m_range_start;
                    eff_end = second;
                } else {
                    eff_start = second;
                    eff_end = m_range_start;
                }
                has_range = true;
            } else {
                eff_start = m_range_start;
                eff_end = m_range_start;
            }
        }

        Color band_col = resolved_style.accent;
        band_col.a = (theme.is_dark ? 0.22f : 0.14f) * grid_alpha;

        Rect grid_clip = Rect::from_xywh(kGridPadX - 4.0f, kHeaderH + 8.0f + kWeekH,
                                         kCellSize * 7.0f + 8.0f, kCellSize * 6.0f + 4.0f);
        ctx.push_clip(grid_clip);
        {
            PaintContext::OffsetScope grid_slide(ctx, Vec2{0.0f, m_month_slide_y});

            for (int idx = 0; idx < 42; ++idx) {
                CellInfo info = cell_info(idx);
                Rect cr = cell_rect(idx);
                Rect circle = Rect::from_xywh(cr.min_x + 2.0f, cr.min_y + 2.0f, kCellSize - 4.0f, kCellSize - 4.0f);
                float radius = circle.width() * 0.5f;

                bool is_today = info.date == m_today;
                bool enabled = is_in_range(info.date);
                bool is_hov = (idx == m_hover_cell && enabled);

                bool is_endpoint = false;
                bool is_mid = false;
                if (m_range_mode) {
                    bool is_s = eff_start.is_valid() && info.date == eff_start;
                    bool is_e = has_range && info.date == eff_end;
                    is_endpoint = is_s || is_e;
                    if (has_range && eff_start.compare(eff_end) < 0) {
                        int cmp_s = info.date.compare(eff_start);
                        int cmp_e = info.date.compare(eff_end);
                        is_mid = (cmp_s > 0 && cmp_e < 0);
                        float by = cr.min_y + 3.0f;
                        float bh = kCellSize - 6.0f;
                        if (is_mid) {
                            ctx.draw_rect(Rect::from_xywh(cr.min_x, by, kCellSize, bh), band_col, 0.0f, 0.0f);
                        } else if (is_s) {
                            ctx.draw_rect(Rect::from_xywh(cr.min_x + kCellSize * 0.5f, by, kCellSize * 0.5f, bh),
                                          band_col, 0.0f, 0.0f);
                        } else if (is_e) {
                            ctx.draw_rect(Rect::from_xywh(cr.min_x, by, kCellSize * 0.5f, bh),
                                          band_col, 0.0f, 0.0f);
                        }
                    }
                } else {
                    is_endpoint = m_selected.is_valid() && info.date == m_selected;
                }

                if (is_endpoint) {
                    ctx.draw_rect(circle, accent, radius, 0.0f);
                } else if (is_today) {
                    if (is_hov) ctx.draw_rect(circle, g_hov, radius, 0.0f);
                    ctx.draw_rect(circle, accent, radius, 1.5f);
                } else if (is_hov) {
                    ctx.draw_rect(circle, g_hov, radius, 0.0f);
                }

                char dbuf[8];
                std::snprintf(dbuf, sizeof(dbuf), "%d", info.date.day);
                Vec2 ds = TextMeasure::measure(dbuf, 13.0f);
                Color dc = is_endpoint ? on_accent
                                       : (!enabled ? g_dis : (info.in_current_month || is_mid ? g_text : g_muted));
                ctx.draw_text(dbuf,
                              PointF{std::round(cr.min_x + (kCellSize - ds.x) * 0.5f),
                                     std::round(cr.min_y + (kCellSize - ds.y) * 0.5f)},
                              13.0f, dc);
            }
        }
        ctx.pop_clip();
    }

private:
    enum class ViewMode : uint8_t { Day = 0, Month = 1, Year = 2 };

    static constexpr float kHeaderH = 44.0f;
    static constexpr float kWeekH = 26.0f;
    static constexpr float kCellSize = 36.0f;
    static constexpr float kGridPadX = 12.0f;
    static constexpr float kPanelW = kGridPadX * 2.0f + kCellSize * 7.0f;
    static constexpr float kPanelH = kHeaderH + 8.0f + kWeekH + kCellSize * 6.0f + 12.0f;

    struct CellInfo {
        CalendarDate date;
        bool in_current_month{true};
    };

    int year_page_start() const noexcept {
        return std::clamp((m_view_year / 12) * 12, 1900, 2188);
    }

    Rect title_btn_rect() const noexcept {
        return Rect::from_xywh(8.0f, 8.0f, kPanelW - 88.0f, 28.0f);
    }

    Rect prev_btn_rect() const noexcept {
        return Rect::from_xywh(kPanelW - 72.0f, 8.0f, 28.0f, 28.0f);
    }

    Rect next_btn_rect() const noexcept {
        return Rect::from_xywh(kPanelW - 38.0f, 8.0f, 28.0f, 28.0f);
    }

    Rect ym_cell_rect(int idx) const noexcept {
        int r = idx / 3;
        int c = idx % 3;
        float body_w = kPanelW - kGridPadX * 2.0f;
        float body_h = kPanelH - kHeaderH - 20.0f;
        float cw = body_w / 3.0f;
        float ch = body_h / 4.0f;
        return Rect::from_xywh(kGridPadX + static_cast<float>(c) * cw,
                               kHeaderH + 10.0f + static_cast<float>(r) * ch, cw, ch);
    }

    int hit_ym_cell(Vec2 local) const noexcept {
        float body_w = kPanelW - kGridPadX * 2.0f;
        float body_h = kPanelH - kHeaderH - 20.0f;
        float gx = local.x - kGridPadX;
        float gy = local.y - (kHeaderH + 10.0f);
        if (gx < 0.0f || gy < 0.0f || gx >= body_w || gy >= body_h) return -1;
        int c = std::clamp(static_cast<int>(gx / (body_w / 3.0f)), 0, 2);
        int r = std::clamp(static_cast<int>(gy / (body_h / 4.0f)), 0, 3);
        return r * 3 + c;
    }

    void trigger_view_anim(float slide_y) noexcept {
        m_month_slide_y = slide_y;
        m_month_anim_alpha = 0.25f;
        m_month_anim_time = std::chrono::steady_clock::now();
        m_month_anim_active = true;
        mark_needs_paint();
    }

    void navigate_page(int delta) {
        if (delta == 0) return;
        if (m_view_mode == ViewMode::Day) {
            shift_month(delta);
        } else if (m_view_mode == ViewMode::Month) {
            int ny = std::clamp(m_view_year + delta, 1900, 2200);
            if (ny != m_view_year) {
                m_view_year = ny;
                trigger_view_anim((delta > 0 ? 1.0f : -1.0f) * 28.0f);
            }
        } else {
            int ny = std::clamp(m_view_year + delta * 12, 1900, 2200);
            if (ny != m_view_year) {
                m_view_year = ny;
                trigger_view_anim((delta > 0 ? 1.0f : -1.0f) * 28.0f);
            }
        }
    }

    Rect cell_rect(int idx) const noexcept {
        int r = idx / 7;
        int c = idx % 7;
        return Rect::from_xywh(kGridPadX + static_cast<float>(c) * kCellSize,
                               kHeaderH + 8.0f + kWeekH + static_cast<float>(r) * kCellSize,
                               kCellSize, kCellSize);
    }

    CellInfo cell_info(int idx) const noexcept {
        int first_dow = CalendarDate::day_of_week(m_view_year, m_view_month, 1);
        int days_cur = CalendarDate::days_in_month(m_view_year, m_view_month);
        int offset = idx - first_dow;

        if (offset < 0) {
            int py = (m_view_month == 1) ? (m_view_year - 1) : m_view_year;
            int pm = (m_view_month == 1) ? 12 : (m_view_month - 1);
            int pd = CalendarDate::days_in_month(py, pm) + offset + 1;
            return CellInfo{CalendarDate{py, pm, pd}, false};
        }
        if (offset >= days_cur) {
            int ny = (m_view_month == 12) ? (m_view_year + 1) : m_view_year;
            int nm = (m_view_month == 12) ? 1 : (m_view_month + 1);
            int nd = offset - days_cur + 1;
            return CellInfo{CalendarDate{ny, nm, nd}, false};
        }
        return CellInfo{CalendarDate{m_view_year, m_view_month, offset + 1}, true};
    }

    bool is_in_range(const CalendarDate& d) const noexcept {
        if (m_min_date.is_valid() && d.compare(m_min_date) < 0) return false;
        if (m_max_date.is_valid() && d.compare(m_max_date) > 0) return false;
        return true;
    }

    int hit_cell(Vec2 local) const noexcept {
        float gx = local.x - kGridPadX;
        float gy = local.y - (kHeaderH + 8.0f + kWeekH);
        if (gx < 0.0f || gy < 0.0f || gx >= kCellSize * 7.0f || gy >= kCellSize * 6.0f) return -1;
        int c = static_cast<int>(gx / kCellSize);
        int r = static_cast<int>(gy / kCellSize);
        return r * 7 + c;
    }

    void shift_month(int delta) {
        if (delta == 0) return;
        int total = (m_view_year * 12 + (m_view_month - 1)) + delta;
        int ny = std::clamp(total / 12, 1900, 2200);
        int nm = (total % 12) + 1;
        if (ny == m_view_year && nm == m_view_month) return;
        m_view_year = ny;
        m_view_month = nm;
        m_month_slide_y = (delta > 0 ? 1.0f : -1.0f) * 28.0f;
        m_month_anim_alpha = 0.25f;
        m_month_anim_time = std::chrono::steady_clock::now();
        m_month_anim_active = true;
        mark_needs_paint();
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            if (e.type == PlatformEventType::PointerScroll) {
                int step = (e.delta.y > 0.0f) ? -1 : 1;
                navigate_page(step);
                e.stop_propagation();
                return;
            }
            bool ht = title_btn_rect().contains(e.local_position);
            bool hp = prev_btn_rect().contains(e.local_position);
            bool hn = next_btn_rect().contains(e.local_position);
            int hc = (m_view_mode == ViewMode::Day) ? hit_cell(e.local_position) : -1;
            int hym = (m_view_mode != ViewMode::Day) ? hit_ym_cell(e.local_position) : -1;
            cursor_shape = (ht || hp || hn || hc >= 0 || hym >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (ht != m_hover_title || hp != m_hover_prev || hn != m_hover_next || hc != m_hover_cell || hym != m_hover_ym) {
                m_hover_title = ht;
                m_hover_prev = hp;
                m_hover_next = hn;
                m_hover_cell = hc;
                m_hover_ym = hym;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hover_title = false;
            m_hover_prev = false;
            m_hover_next = false;
            m_hover_cell = -1;
            m_hover_ym = -1;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            if (title_btn_rect().contains(e.local_position)) {
                if (m_view_mode == ViewMode::Day) {
                    m_view_mode = ViewMode::Month;
                    trigger_view_anim(-14.0f);
                } else if (m_view_mode == ViewMode::Month) {
                    m_view_mode = ViewMode::Year;
                    trigger_view_anim(-14.0f);
                } else {
                    m_view_mode = ViewMode::Day;
                    trigger_view_anim(14.0f);
                }
                e.stop_propagation();
                return;
            }
            if (prev_btn_rect().contains(e.local_position)) {
                navigate_page(-1);
                e.stop_propagation();
                return;
            }
            if (next_btn_rect().contains(e.local_position)) {
                navigate_page(1);
                e.stop_propagation();
                return;
            }
            if (m_view_mode == ViewMode::Month) {
                int idx = hit_ym_cell(e.local_position);
                if (idx >= 0 && idx < 12) {
                    m_view_month = idx + 1;
                    m_view_mode = ViewMode::Day;
                    m_hover_ym = -1;
                    trigger_view_anim(14.0f);
                    e.stop_propagation();
                }
                return;
            }
            if (m_view_mode == ViewMode::Year) {
                int idx = hit_ym_cell(e.local_position);
                if (idx >= 0 && idx < 12) {
                    m_view_year = std::clamp(year_page_start() + idx, 1900, 2200);
                    m_view_mode = ViewMode::Month;
                    m_hover_ym = -1;
                    trigger_view_anim(14.0f);
                    e.stop_propagation();
                }
                return;
            }
            int idx = hit_cell(e.local_position);
            if (idx >= 0) {
                CellInfo info = cell_info(idx);
                if (is_in_range(info.date)) {
                    if (m_range_mode) {
                        if (!m_picking_end) {
                            m_range_start = info.date;
                            m_range_end = CalendarDate{};
                            m_picking_end = true;
                            mark_needs_paint();
                        } else {
                            CalendarDate s = m_range_start;
                            CalendarDate en = info.date;
                            if (s.compare(en) > 0) std::swap(s, en);
                            m_range_start = s;
                            m_range_end = en;
                            m_picking_end = false;
                            mark_needs_paint();
                            if (m_on_range_select) m_on_range_select(s, en);
                        }
                    } else {
                        m_selected = info.date;
                        if (m_on_select) m_on_select(info.date);
                    }
                    e.stop_propagation();
                }
            }
        };
    }

    ViewMode m_view_mode{ViewMode::Day};
    bool m_range_mode{false};
    bool m_picking_end{false};
    CalendarDate m_selected;
    CalendarDate m_range_start;
    CalendarDate m_range_end;
    CalendarDate m_min_date;
    CalendarDate m_max_date;
    CalendarDate m_today;
    int m_view_year{2026};
    int m_view_month{1};
    bool m_hover_title{false};
    bool m_hover_prev{false};
    bool m_hover_next{false};
    int m_hover_cell{-1};
    int m_hover_ym{-1};
    float m_month_slide_y{0.0f};
    float m_month_anim_alpha{1.0f};
    bool m_month_anim_active{false};
    std::chrono::steady_clock::time_point m_month_anim_time{};
    FluentPopupSurface m_surface;
    Fn<void(const CalendarDate&)> m_on_select;
    Fn<void(const CalendarDate&, const CalendarDate&)> m_on_range_select;
};

// WinUI 3 CalendarDatePicker 日历日期选择器渲染盒
class RenderCalendarDatePicker : public RenderBox {
public:
    RenderCalendarDatePicker() noexcept {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderCalendarDatePicker() override {
        close_popup();
    }

    void set_properties(CalendarDate date,
                        const std::string& placeholder,
                        const std::string& header,
                        CalendarDate min_date,
                        CalendarDate max_date,
                        float width,
                        bool disabled,
                        Fn<void(const CalendarDate&)> on_date_changed,
                        bool range_mode = false,
                        CalendarDate start_date = CalendarDate{},
                        CalendarDate end_date = CalendarDate{},
                        Fn<void(const CalendarDate&, const CalendarDate&)> on_range_changed = nullptr) {
        if (m_date != date) { m_date = date; mark_needs_paint(); }
        if (m_placeholder != placeholder) { m_placeholder = placeholder; mark_needs_paint(); }
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        m_min_date = min_date;
        m_max_date = max_date;
        m_range_mode = range_mode;
        if (range_mode) {
            if (start_date.is_valid()) m_range_start = start_date;
            if (end_date.is_valid()) m_range_end = end_date;
        }
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            cursor_shape = disabled ? CursorShape::Arrow : CursorShape::PointingHand;
            if (disabled) close_popup();
            mark_needs_paint();
        }
        m_on_date_changed = std::move(on_date_changed);
        m_on_range_changed = std::move(on_range_changed);
    }

    void set_date_value(const CalendarDate& d) {
        if (m_date != d) {
            m_date = d;
            mark_needs_paint();
        }
    }

    [[nodiscard]] const CalendarDate& date() const noexcept { return m_date; }
    [[nodiscard]] const CalendarDate& start_date() const noexcept { return m_range_start; }
    [[nodiscard]] const CalendarDate& end_date() const noexcept { return m_range_end; }

    [[nodiscard]] bool is_date_in_range(const CalendarDate& d) const noexcept {
        if (!d.is_valid()) return false;
        if (m_min_date.is_valid() && d.compare(m_min_date) < 0) return false;
        if (m_max_date.is_valid() && d.compare(m_max_date) > 0) return false;
        return true;
    }

    void perform_layout() override {
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        float def_w = m_range_mode ? 260.0f : 220.0f;
        float w = (m_width > 0.0f)
            ? m_width
            : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : def_w);
        set_size(m_constraints.constrain(Vec2{w, header_h + 32.0f}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const auto resolved_style = CalendarDatePickerStyleResolver::resolve(theme);
        Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.95f}
                                       : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
        Color muted_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.54f}
                                        : Color{0.0f, 0.0f, 0.0f, 0.54f};

        if (!m_header.empty()) {
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, text_col);
        }

        Rect box = box_rect();
        Color bg;
        if (m_disabled) {
            bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.03f} : Color{0.0f, 0.0f, 0.0f, 0.025f};
        } else if (m_pressed || m_popup_handle.is_open()) {
            bg = theme.colors.control_fill_tertiary;
        } else if (m_hovered) {
            bg = theme.colors.control_fill_secondary;
        } else {
            bg = theme.colors.control_fill_default;
        }
        ctx.draw_rect(box, bg, 4.0f, 0.0f);
        ctx.draw_rect(box, theme.colors.control_stroke_default, 4.0f, 1.0f);

        Color bottom_line = m_popup_handle.is_open()
            ? resolved_style.accent
            : theme.colors.control_stroke_bottom;
        ctx.draw_rect(Rect::from_xywh(box.min_x + 2.0f, box.max_y - (m_popup_handle.is_open() ? 2.0f : 1.0f),
                                      std::max(0.0f, box.width() - 4.0f), m_popup_handle.is_open() ? 2.0f : 1.0f),
                      bottom_line, 0.0f, 0.0f);

        std::string disp;
        bool has_val = false;
        if (m_range_mode) {
            if (m_range_start.is_valid() && m_range_end.is_valid()) {
                disp = m_range_start.format_iso() + " ~ " + m_range_end.format_iso();
                has_val = true;
            } else if (m_range_start.is_valid()) {
                disp = m_range_start.format_iso() + " ~ ...";
                has_val = true;
            } else {
                disp = m_placeholder;
            }
        } else {
            has_val = m_date.is_valid();
            disp = has_val ? m_date.format_iso() : m_placeholder;
        }
        Vec2 ts = TextMeasure::measure(disp, 13.5f);
        ctx.draw_text(disp,
                      PointF{box.min_x + 11.0f, std::round(box.min_y + (box.height() - ts.y) * 0.5f)},
                      13.5f, has_val ? text_col : muted_col);

        Color icon_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.30f : 0.76f}
                                       : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.30f : 0.72f};
        Rect ir = Rect::from_xywh(box.max_x - 28.0f, box.min_y, 24.0f, box.height());
        fluent_icons::draw_centered(ctx, fluent_icons::kCalendar, ir, 14.0f, icon_col);
    }

private:
    Rect box_rect() const noexcept {
        float top = m_header.empty() ? 0.0f : 22.0f;
        return Rect::from_xywh(0.0f, top, size().x, std::max(32.0f, size().y - top));
    }

    void toggle_popup() {
        if (m_popup_handle.is_open()) {
            close_popup();
            return;
        }
        Window* win = Window::active_window();
        if (!win || m_disabled) return;

        auto token = std::make_shared<bool>(true);
        std::shared_ptr<RenderCalendarPopup> popup;
        if (m_range_mode) {
            popup = make_render_node<RenderCalendarPopup>(
                m_range_start, m_range_end, m_min_date, m_max_date,
                [this, token](const CalendarDate& s, const CalendarDate& e) {
                    if (!*token) return;
                    m_range_start = s;
                    m_range_end = e;
                    close_popup();
                    mark_needs_paint();
                    if (m_on_range_changed) m_on_range_changed(s, e);
                });
        } else {
            popup = make_render_node<RenderCalendarPopup>(
                m_date, m_min_date, m_max_date,
                [this, token](const CalendarDate& picked) {
                    if (!*token) return;
                    m_date = picked;
                    close_popup();
                    mark_needs_paint();
                    if (m_on_date_changed) m_on_date_changed(picked);
                });
        }

        Vec2 sz = popup->compute_ideal_size();
        OverlayEntry entry;
        entry.content = popup;
        entry.global_pos = FluentPopupSurface::compute_anchored_pos(*win, global_offset(), box_rect(), sz);
        entry.size = sz;
        entry.dismiss_on_outside_click = true;
        entry.is_modal = false;

        m_popup_handle.open(*win, std::move(entry), [this, token]() {
            *token = false;
            mark_needs_paint();
        });
        mark_needs_paint();
    }

    void close_popup() {
        m_popup_handle.close();
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_disabled) { m_hovered = true; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_pressed = false;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            m_pressed = true;
            mark_needs_paint();
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            bool was = m_pressed;
            m_pressed = false;
            mark_needs_paint();
            if (was) toggle_popup();
            e.stop_propagation();
        };
    }

    CalendarDate m_date;
    CalendarDate m_range_start;
    CalendarDate m_range_end;
    CalendarDate m_min_date;
    CalendarDate m_max_date;
    std::string m_placeholder{FrameworkStrings::current().select_date_placeholder};
    std::string m_header;
    float m_width{220.0f};
    bool m_range_mode{false};
    bool m_disabled{false};
    bool m_hovered{false};
    bool m_pressed{false};
    PopupHandle m_popup_handle;
    Fn<void(const CalendarDate&)> m_on_date_changed;
    Fn<void(const CalendarDate&, const CalendarDate&)> m_on_range_changed;

public:
    void wrap_on_date_changed(Fn<void(const CalendarDate&)> extra) {
        auto prev = std::move(m_on_date_changed);
        m_on_date_changed = [prev = std::move(prev), extra = std::move(extra)](const CalendarDate& d) {
            if (extra) extra(d);
            if (prev) prev(d);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 CalendarDatePicker (支持单日与区间选择)
struct CalendarDatePicker : BindableMixin<CalendarDatePicker, RenderCalendarDatePicker> {
    CalendarDate date{CalendarDate{}};
    std::string placeholder{FrameworkStrings::current().select_date_placeholder};
    std::string header;
    CalendarDate min_date;
    CalendarDate max_date;
    float width{220.0f};
    bool disabled{false};
    Fn<void(const CalendarDate&)> on_date_changed{nullptr};

    // 日期区间选择扩展
    bool range_mode{false};
    CalendarDate start_date{};
    CalendarDate end_date{};
    Fn<void(const CalendarDate&, const CalendarDate&)> on_range_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderCalendarDatePicker>& node, Property<CalendarDate>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_date_value(prop.get());
    std::weak_ptr<RenderCalendarDatePicker> weak = node;
    bg.subscribe(prop, [weak, guard](const CalendarDate& d) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_date_value(d);
        }
    });
    Property<CalendarDate>* p = &prop;
    node->wrap_on_date_changed([p, guard](const CalendarDate& d) {
        *guard = true;
        p->set(d);
        *guard = false;
    });
}

class CalendarDatePickerWidget : public Widget {
public:
    explicit CalendarDatePickerWidget(const CalendarDatePicker& desc) : m_desc(desc) {}
    explicit CalendarDatePickerWidget(CalendarDatePicker&& desc) noexcept : m_desc(std::move(desc)) {}
    ~CalendarDatePickerWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderCalendarDatePicker>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderCalendarDatePicker>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderCalendarDatePicker>& node) {
        node->set_properties(
            m_desc.date,
            m_desc.placeholder,
            m_desc.header,
            m_desc.min_date,
            m_desc.max_date,
            m_desc.width,
            m_desc.disabled,
            m_desc.on_date_changed,
            m_desc.range_mode,
            m_desc.start_date,
            m_desc.end_date,
            m_desc.on_range_changed);
    }

    CalendarDatePicker m_desc;
};

inline CalendarDatePicker::operator WidgetPtr() const& {
    return make_widget<CalendarDatePickerWidget>(*this);
}
inline CalendarDatePicker::operator WidgetPtr() && {
    return make_widget<CalendarDatePickerWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
