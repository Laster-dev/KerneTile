#pragma once

#include "common/fluent_chrome.hpp"
#include "common/fluent_icons.hpp"
#include "widget.hpp"
#include <ktui/app/window.hpp>
#include <ktui/binding/binding.hpp>
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
#include <functional>
#include <memory>
#include <string>

namespace ktui {

// 轻量级公历日期值类型 (0/0/0 表示未选择)
struct CalendarDate {
    int year{0};
    int month{0};
    int day{0};

    bool operator==(const CalendarDate&) const = default;

    [[nodiscard]] static bool is_leap_year(int y) noexcept {
        return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    }

    [[nodiscard]] static int days_in_month(int y, int m) noexcept {
        static constexpr int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (m < 1 || m > 12) return 30;
        if (m == 2 && is_leap_year(y)) return 29;
        return kDays[m - 1];
    }

    // 返回星期几: 0 = 周日, 1 = 周一, ..., 6 = 周六 (Sakamoto 算法)
    [[nodiscard]] static int day_of_week(int y, int m, int d) noexcept {
        static constexpr int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (m < 3) y -= 1;
        return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    }

    [[nodiscard]] bool is_valid() const noexcept {
        return year >= 1900 && year <= 2200 &&
               month >= 1 && month <= 12 &&
               day >= 1 && day <= days_in_month(year, month);
    }

    [[nodiscard]] int compare(const CalendarDate& o) const noexcept {
        if (year != o.year) return (year < o.year) ? -1 : 1;
        if (month != o.month) return (month < o.month) ? -1 : 1;
        if (day != o.day) return (day < o.day) ? -1 : 1;
        return 0;
    }

    [[nodiscard]] std::string format_iso() const {
        if (!is_valid()) return {};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
        return std::string(buf);
    }

    [[nodiscard]] static CalendarDate today() {
        std::time_t now = std::time(nullptr);
        std::tm tm_val{};
#ifdef _WIN32
        localtime_s(&tm_val, &now);
#else
        localtime_r(&now, &tm_val);
#endif
        return CalendarDate{tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday};
    }
};

// WinUI 3 CalendarDatePicker 悬浮月历弹窗
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

        const Theme& theme = ThemeContext::current();
        float alpha = st.alpha;

        Color text_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.95f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.90f * alpha};
        Color muted_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.42f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.42f * alpha};
        Color dis_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.22f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.22f * alpha};
        Color hov_bg = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.08f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.055f * alpha};
        Color sep_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.08f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.08f * alpha};
        Color on_accent = theme.is_dark
            ? Color{0.0f, 0.0f, 0.0f, 0.92f * alpha}
            : Color{1.0f, 1.0f, 1.0f, alpha};

        FluentPopupSurface::paint_panel(ctx, theme, Rect::from_xywh(0.0f, 0.0f, kPanelW, kPanelH), alpha);

        char title_buf[32];
        std::snprintf(title_buf, sizeof(title_buf), "%d年%d月", m_view_year, m_view_month);
        Vec2 ts = TextMeasure::measure(title_buf, 14.0f);
        ctx.draw_text(title_buf, PointF{16.0f, std::round((kHeaderH - ts.y) * 0.5f)}, 14.0f, text_col);

        Rect prev_r = prev_btn_rect();
        Rect next_r = next_btn_rect();
        if (m_hover_prev) ctx.draw_rect(prev_r, hov_bg, 4.0f, 0.0f);
        if (m_hover_next) ctx.draw_rect(next_r, hov_bg, 4.0f, 0.0f);

        fluent_icons::draw_centered(ctx, fluent_icons::kChevronLeft, prev_r, 12.0f, text_col);
        fluent_icons::draw_centered(ctx, fluent_icons::kChevronRight, next_r, 12.0f, text_col);

        ctx.draw_rect(Rect::from_xywh(1.0f, kHeaderH, kPanelW - 2.0f, 1.0f), sep_col, 0.0f, 0.0f);

        static constexpr const char* kWeekdays[7] = {"日", "一", "二", "三", "四", "五", "六"};
        for (int c = 0; c < 7; ++c) {
            float cx = kGridPadX + static_cast<float>(c) * kCellSize;
            Vec2 ws = TextMeasure::measure(kWeekdays[c], 12.0f);
            ctx.draw_text(kWeekdays[c],
                          PointF{std::round(cx + (kCellSize - ws.x) * 0.5f),
                                 std::round(kHeaderH + 8.0f + (kWeekH - ws.y) * 0.5f)},
                          12.0f, muted_col);
        }

        for (int idx = 0; idx < 42; ++idx) {
            CellInfo info = cell_info(idx);
            Rect cr = cell_rect(idx);
            Rect circle = Rect::from_xywh(cr.min_x + 2.0f, cr.min_y + 2.0f, kCellSize - 4.0f, kCellSize - 4.0f);
            float radius = circle.width() * 0.5f;

            bool is_sel = m_selected.is_valid() && info.date == m_selected;
            bool is_today = info.date == m_today;
            bool enabled = is_in_range(info.date);
            bool is_hov = (idx == m_hover_cell && enabled);

            Color accent = theme.colors.primary;
            accent.a *= alpha;

            if (is_sel) {
                ctx.draw_rect(circle, accent, radius, 0.0f);
            } else if (is_today) {
                if (is_hov) ctx.draw_rect(circle, hov_bg, radius, 0.0f);
                ctx.draw_rect(circle, accent, radius, 1.5f);
            } else if (is_hov) {
                ctx.draw_rect(circle, hov_bg, radius, 0.0f);
            }

            char dbuf[8];
            std::snprintf(dbuf, sizeof(dbuf), "%d", info.date.day);
            Vec2 ds = TextMeasure::measure(dbuf, 13.0f);
            Color dc = is_sel ? on_accent
                              : (!enabled ? dis_col : (info.in_current_month ? text_col : muted_col));
            ctx.draw_text(dbuf,
                          PointF{std::round(cr.min_x + (kCellSize - ds.x) * 0.5f),
                                 std::round(cr.min_y + (kCellSize - ds.y) * 0.5f)},
                          13.0f, dc);
        }
    }

private:
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

    Rect prev_btn_rect() const noexcept {
        return Rect::from_xywh(kPanelW - 72.0f, 8.0f, 28.0f, 28.0f);
    }

    Rect next_btn_rect() const noexcept {
        return Rect::from_xywh(kPanelW - 38.0f, 8.0f, 28.0f, 28.0f);
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
        int total = (m_view_year * 12 + (m_view_month - 1)) + delta;
        m_view_year = std::clamp(total / 12, 1900, 2200);
        m_view_month = (total % 12) + 1;
        mark_needs_paint();
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            bool hp = prev_btn_rect().contains(e.local_position);
            bool hn = next_btn_rect().contains(e.local_position);
            int hc = hit_cell(e.local_position);
            cursor_shape = (hp || hn || hc >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (hp != m_hover_prev || hn != m_hover_next || hc != m_hover_cell) {
                m_hover_prev = hp;
                m_hover_next = hn;
                m_hover_cell = hc;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hover_prev = false;
            m_hover_next = false;
            m_hover_cell = -1;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            if (prev_btn_rect().contains(e.local_position)) {
                shift_month(-1);
                e.stop_propagation();
                return;
            }
            if (next_btn_rect().contains(e.local_position)) {
                shift_month(1);
                e.stop_propagation();
                return;
            }
            int idx = hit_cell(e.local_position);
            if (idx >= 0) {
                CellInfo info = cell_info(idx);
                if (is_in_range(info.date)) {
                    m_selected = info.date;
                    if (m_on_select) m_on_select(info.date);
                    e.stop_propagation();
                }
            }
        };
    }

    CalendarDate m_selected;
    CalendarDate m_min_date;
    CalendarDate m_max_date;
    CalendarDate m_today;
    int m_view_year{2026};
    int m_view_month{1};
    bool m_hover_prev{false};
    bool m_hover_next{false};
    int m_hover_cell{-1};
    FluentPopupSurface m_surface;
    Fn<void(const CalendarDate&)> m_on_select;
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
                        Fn<void(const CalendarDate&)> on_date_changed) {
        if (m_date != date) { m_date = date; mark_needs_paint(); }
        if (m_placeholder != placeholder) { m_placeholder = placeholder; mark_needs_paint(); }
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        m_min_date = min_date;
        m_max_date = max_date;
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            cursor_shape = disabled ? CursorShape::Arrow : CursorShape::PointingHand;
            if (disabled) close_popup();
            mark_needs_paint();
        }
        m_on_date_changed = std::move(on_date_changed);
    }

    void set_date_value(const CalendarDate& d) {
        if (m_date != d) {
            m_date = d;
            mark_needs_paint();
        }
    }

    [[nodiscard]] const CalendarDate& date() const noexcept { return m_date; }

    [[nodiscard]] bool is_date_in_range(const CalendarDate& d) const noexcept {
        if (!d.is_valid()) return false;
        if (m_min_date.is_valid() && d.compare(m_min_date) < 0) return false;
        if (m_max_date.is_valid() && d.compare(m_max_date) > 0) return false;
        return true;
    }

    void perform_layout() override {
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        float w = (m_width > 0.0f)
            ? m_width
            : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 220.0f);
        set_size(m_constraints.constrain(Vec2{w, header_h + 32.0f}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
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
            ? theme.colors.primary
            : theme.colors.control_stroke_bottom;
        ctx.draw_rect(Rect::from_xywh(box.min_x + 2.0f, box.max_y - (m_popup_handle.is_open() ? 2.0f : 1.0f),
                                      std::max(0.0f, box.width() - 4.0f), m_popup_handle.is_open() ? 2.0f : 1.0f),
                      bottom_line, 0.0f, 0.0f);

        std::string disp = m_date.is_valid() ? m_date.format_iso() : m_placeholder;
        Vec2 ts = TextMeasure::measure(disp, 13.5f);
        ctx.draw_text(disp,
                      PointF{box.min_x + 11.0f, std::round(box.min_y + (box.height() - ts.y) * 0.5f)},
                      13.5f, m_date.is_valid() ? text_col : muted_col);

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
        auto popup = make_render_node<RenderCalendarPopup>(
            m_date, m_min_date, m_max_date,
            [this, token](const CalendarDate& picked) {
                if (!*token) return;
                m_date = picked;
                close_popup();
                mark_needs_paint();
                if (m_on_date_changed) m_on_date_changed(picked);
            });

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
    CalendarDate m_min_date;
    CalendarDate m_max_date;
    std::string m_placeholder{"选择日期"};
    std::string m_header;
    float m_width{220.0f};
    bool m_disabled{false};
    bool m_hovered{false};
    bool m_pressed{false};
    PopupHandle m_popup_handle;
    Fn<void(const CalendarDate&)> m_on_date_changed;

public:
    void wrap_on_date_changed(Fn<void(const CalendarDate&)> extra) {
        auto prev = std::move(m_on_date_changed);
        m_on_date_changed = [prev = std::move(prev), extra = std::move(extra)](const CalendarDate& d) {
            if (extra) extra(d);
            if (prev) prev(d);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 CalendarDatePicker
struct CalendarDatePicker : BindableMixin<CalendarDatePicker, RenderCalendarDatePicker> {
    CalendarDate date{CalendarDate{}};
    std::string placeholder{"选择日期"};
    std::string header;
    CalendarDate min_date;
    CalendarDate max_date;
    float width{220.0f};
    bool disabled{false};
    Fn<void(const CalendarDate&)> on_date_changed{nullptr};

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
            m_desc.on_date_changed);
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
