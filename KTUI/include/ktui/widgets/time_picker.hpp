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
#include <cmath>
#include <cstdio>
#include <ctime>
#include <functional>
#include <memory>
#include <string>

namespace ktui {

// 轻量级时间值类型 (24 小时制存储: hour 0..23, minute 0..59)
struct TimeOfDay {
    int hour{9};
    int minute{0};

    bool operator==(const TimeOfDay&) const = default;

    [[nodiscard]] bool is_valid() const noexcept {
        return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
    }

    [[nodiscard]] std::string format(bool use_24h = true) const {
        char buf[32];
        if (use_24h) {
            std::snprintf(buf, sizeof(buf), "%02d:%02d", std::clamp(hour, 0, 23), std::clamp(minute, 0, 59));
        } else {
            int h12 = hour % 12;
            if (h12 == 0) h12 = 12;
            std::snprintf(buf, sizeof(buf), "%02d:%02d %s", h12, std::clamp(minute, 0, 59), (hour >= 12) ? "PM" : "AM");
        }
        return std::string(buf);
    }

    [[nodiscard]] static TimeOfDay now() {
        std::time_t t = std::time(nullptr);
        std::tm tm_val{};
#ifdef _WIN32
        localtime_s(&tm_val, &t);
#else
        localtime_r(&t, &tm_val);
#endif
        return TimeOfDay{tm_val.tm_hour, tm_val.tm_min};
    }
};

// WinUI 3 TimePicker 固定尺寸滚轮选时浮层
class RenderTimePickerPopup : public RenderBox {
public:
    RenderTimePickerPopup(TimeOfDay initial,
                          bool use_24h,
                          int minute_increment,
                          Fn<void(const TimeOfDay&)> on_confirm,
                          Fn<void()> on_cancel)
        : m_time(initial.is_valid() ? initial : TimeOfDay{9, 0}),
          m_use_24h(use_24h),
          m_minute_inc(std::clamp(minute_increment, 1, 30)),
          m_on_confirm(std::move(on_confirm)),
          m_on_cancel(std::move(on_cancel)) {
        cursor_shape = CursorShape::Arrow;
        setup_behavior();
    }

    ~RenderTimePickerPopup() override = default;

    [[nodiscard]] Vec2 compute_ideal_size() const noexcept {
        return Vec2{m_use_24h ? 184.0f : 236.0f, kPanelH};
    }

    void perform_layout() override {
        set_size(compute_ideal_size());
    }

    void paint(PaintContext& ctx) override {
        auto st = m_surface.tick();
        if (st.animating) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        const float alpha = st.alpha;
        const float w = size().x;
        const float h = size().y;

        FluentPopupSurface::paint_panel(ctx, theme, Rect::from_xywh(0.0f, 0.0f, w, h), alpha, 8.0f);

        // 中央选中高亮带
        float center_y = kPadTop + 2.0f * kRowH;
        Color hi_bg = theme.colors.primary;
        hi_bg.a *= alpha;
        ctx.draw_rect(Rect::from_xywh(6.0f, center_y, w - 12.0f, kRowH), hi_bg, 5.0f, 0.0f);

        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.86f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.82f * alpha};
        Color fade_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.45f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.45f * alpha};
        Color on_acc = theme.colors.on_primary;
        on_acc.a *= alpha;

        int col_count = m_use_24h ? 2 : 3;
        float col_w = (w - 12.0f) / static_cast<float>(col_count);

        // 绘制列分隔线
        for (int c = 1; c < col_count; ++c) {
            float sx = std::round(6.0f + static_cast<float>(c) * col_w);
            ctx.draw_rect(Rect::from_xywh(sx, kPadTop + 4.0f, 1.0f, kRowH * 5.0f - 8.0f),
                          theme.colors.divider_stroke, 0.0f, 0.0f);
        }

        // 绘制 5 行滚轮数字 (-2, -1, 0, +1, +2)
        for (int off = -2; off <= 2; ++off) {
            float ry = kPadTop + static_cast<float>(off + 2) * kRowH;
            Color c_draw = (off == 0) ? on_acc : ((std::abs(off) == 1) ? text_c : fade_c);

            // 1) 小时列
            int disp_h = wrap_hour(m_time.hour + off);
            char hbuf[8];
            if (m_use_24h) {
                std::snprintf(hbuf, sizeof(hbuf), "%02d", disp_h);
            } else {
                int h12 = disp_h % 12;
                if (h12 == 0) h12 = 12;
                std::snprintf(hbuf, sizeof(hbuf), "%02d", h12);
            }
            draw_cell(ctx, Rect::from_xywh(6.0f, ry, col_w, kRowH), hbuf, c_draw, off == 0,
                      m_hover_col == 0 && m_hover_off == off, theme, alpha);

            // 2) 分钟列
            int disp_m = wrap_minute(m_time.minute + off * m_minute_inc);
            char mbuf[8];
            std::snprintf(mbuf, sizeof(mbuf), "%02d", disp_m);
            draw_cell(ctx, Rect::from_xywh(6.0f + col_w, ry, col_w, kRowH), mbuf, c_draw, off == 0,
                      m_hover_col == 1 && m_hover_off == off, theme, alpha);

            // 3) AM/PM 列 (12h 模式)
            if (!m_use_24h) {
                bool is_pm = (m_time.hour >= 12);
                const char* ap = nullptr;
                if (off == 0) ap = is_pm ? "PM" : "AM";
                else if (off == 1 && !is_pm) ap = "PM";
                else if (off == -1 && is_pm) ap = "AM";
                if (ap) {
                    draw_cell(ctx, Rect::from_xywh(6.0f + col_w * 2.0f, ry, col_w, kRowH), ap, c_draw, off == 0,
                              m_hover_col == 2 && m_hover_off == off, theme, alpha);
                }
            }
        }

        // 底部确认 / 取消按钮栏
        float bar_y = h - kFooterH;
        ctx.draw_rect(Rect::from_xywh(1.0f, bar_y, w - 2.0f, 1.0f), theme.colors.divider_stroke, 0.0f, 0.0f);

        Rect ok_r = ok_btn_rect();
        Rect ca_r = cancel_btn_rect();
        if (m_hover_ok) ctx.draw_rect(ok_r, theme.colors.subtle_fill_secondary, 4.0f, 0.0f);
        if (m_hover_cancel) ctx.draw_rect(ca_r, theme.colors.subtle_fill_secondary, 4.0f, 0.0f);

        static constexpr const char* kCheckSvg =
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
            "<path fill='#FFFFFF' d='M9.55 18.2a1 1 0 0 1-.71-.29l-4.55-4.55a1 1 0 1 1 1.42-1.42l3.84 3.85 8.74-8.75a1 1 0 1 1 1.42 1.42l-9.45 9.45a1 1 0 0 1-.71.29z'/>"
            "</svg>";
        fluent_icons::draw_centered(ctx, kCheckSvg, ok_r, 15.0f, text_c);
        fluent_icons::draw_centered(ctx, fluent_icons::kClose, ca_r, 13.0f, text_c);
    }

private:
    static constexpr float kPadTop = 8.0f;
    static constexpr float kRowH = 36.0f;
    static constexpr float kFooterH = 42.0f;
    static constexpr float kPanelH = kPadTop + kRowH * 5.0f + 8.0f + kFooterH; // 238px

    static int wrap_hour(int h) noexcept {
        int r = h % 24;
        return (r < 0) ? (r + 24) : r;
    }

    static int wrap_minute(int m) noexcept {
        int r = m % 60;
        return (r < 0) ? (r + 60) : r;
    }

    static void draw_cell(PaintContext& ctx, Rect r, const char* label, Color col,
                          bool is_center, bool hovered, const Theme& theme, float alpha) {
        if (!is_center && hovered) {
            Color hb = theme.colors.subtle_fill_secondary;
            hb.a *= alpha;
            ctx.draw_rect(Rect::from_xywh(r.min_x + 3.0f, r.min_y + 2.0f, r.width() - 6.0f, r.height() - 4.0f), hb, 4.0f, 0.0f);
        }
        Vec2 ts = TextMeasure::measure(label, 14.0f);
        ctx.draw_text(label,
                      PointF{std::round(r.min_x + (r.width() - ts.x) * 0.5f),
                             std::round(r.min_y + (r.height() - ts.y) * 0.5f)},
                      14.0f, col);
    }

    Rect ok_btn_rect() const noexcept {
        float half = (size().x - 16.0f) * 0.5f;
        return Rect::from_xywh(6.0f, size().y - kFooterH + 5.0f, half, kFooterH - 10.0f);
    }

    Rect cancel_btn_rect() const noexcept {
        float half = (size().x - 16.0f) * 0.5f;
        return Rect::from_xywh(10.0f + half, size().y - kFooterH + 5.0f, half, kFooterH - 10.0f);
    }

    bool hit_drum(Vec2 local, int& out_col, int& out_off) const noexcept {
        if (local.y < kPadTop || local.y >= kPadTop + kRowH * 5.0f) return false;
        if (local.x < 6.0f || local.x > size().x - 6.0f) return false;
        int cols = m_use_24h ? 2 : 3;
        float cw = (size().x - 12.0f) / static_cast<float>(cols);
        out_col = std::clamp(static_cast<int>((local.x - 6.0f) / cw), 0, cols - 1);
        out_off = static_cast<int>(std::floor((local.y - kPadTop) / kRowH)) - 2;
        return true;
    }

    void step_col(int col, int delta) {
        if (col == 0) {
            m_time.hour = wrap_hour(m_time.hour + delta);
        } else if (col == 1) {
            m_time.minute = wrap_minute(m_time.minute + delta * m_minute_inc);
        } else if (col == 2 && !m_use_24h) {
            if (delta != 0) m_time.hour = (m_time.hour + 12) % 24;
        }
        mark_needs_paint();
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            if (e.type == PlatformEventType::PointerScroll) {
                int col = 0, off = 0;
                if (hit_drum(e.local_position, col, off)) {
                    int step = (e.delta.y > 0.0f) ? -1 : 1;
                    step_col(col, step);
                    e.stop_propagation();
                }
                return;
            }
            bool hok = ok_btn_rect().contains(e.local_position);
            bool hca = cancel_btn_rect().contains(e.local_position);
            int hc = -1, ho = 0;
            bool in_drum = hit_drum(e.local_position, hc, ho);
            cursor_shape = (hok || hca || in_drum) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (hok != m_hover_ok || hca != m_hover_cancel || hc != m_hover_col || ho != m_hover_off) {
                m_hover_ok = hok;
                m_hover_cancel = hca;
                m_hover_col = in_drum ? hc : -1;
                m_hover_off = ho;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hover_ok = m_hover_cancel = false;
            m_hover_col = -1;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            if (ok_btn_rect().contains(e.local_position)) {
                if (m_on_confirm) m_on_confirm(m_time);
                e.stop_propagation();
                return;
            }
            if (cancel_btn_rect().contains(e.local_position)) {
                if (m_on_cancel) m_on_cancel();
                e.stop_propagation();
                return;
            }
            int col = 0, off = 0;
            if (hit_drum(e.local_position, col, off) && off != 0) {
                step_col(col, off);
                e.stop_propagation();
            }
        };
    }

    TimeOfDay m_time;
    bool m_use_24h{true};
    int m_minute_inc{1};
    FluentPopupSurface m_surface;
    int m_hover_col{-1};
    int m_hover_off{0};
    bool m_hover_ok{false};
    bool m_hover_cancel{false};
    Fn<void(const TimeOfDay&)> m_on_confirm;
    Fn<void()> m_on_cancel;
};

// WinUI 3 TimePicker 紧凑触发器渲染盒
class RenderTimePicker : public RenderBox {
public:
    RenderTimePicker() noexcept {
        cursor_shape = CursorShape::PointingHand;
        setup_behavior();
    }

    ~RenderTimePicker() override { m_popup_handle.close(); }

    void set_properties(TimeOfDay time,
                        const std::string& header,
                        bool use_24h,
                        int minute_increment,
                        float width,
                        bool disabled,
                        Fn<void(const TimeOfDay&)> on_time_changed) {
        if (m_time != time) { m_time = time; mark_needs_paint(); }
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        if (m_use_24h != use_24h) { m_use_24h = use_24h; mark_needs_paint(); }
        m_minute_inc = std::clamp(minute_increment, 1, 30);
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            cursor_shape = disabled ? CursorShape::Arrow : CursorShape::PointingHand;
            if (disabled) m_popup_handle.close();
            mark_needs_paint();
        }
        m_on_time_changed = std::move(on_time_changed);
    }

    void set_time_value(const TimeOfDay& t) {
        if (m_time != t) { m_time = t; mark_needs_paint(); }
    }

    [[nodiscard]] const TimeOfDay& time() const noexcept { return m_time; }

    void perform_layout() override {
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        float w = (m_width > 0.0f) ? m_width : 190.0f;
        set_size(m_constraints.constrain(Vec2{w, header_h + 32.0f}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.94f}
                                     : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
        if (!m_header.empty()) {
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, text_c);
        }

        Rect box = box_rect();
        Color bg = m_disabled ? theme.colors.control_fill_disabled
                 : ((m_pressed || m_popup_handle.is_open()) ? theme.colors.control_fill_tertiary
                    : (m_hovered ? theme.colors.control_fill_secondary : theme.colors.control_fill_default));
        ctx.draw_rect(box, bg, 4.0f, 0.0f);
        ctx.draw_rect(box, theme.colors.control_stroke_default, 4.0f, 1.0f);

        Color bottom_line = m_popup_handle.is_open() ? theme.colors.primary : theme.colors.control_stroke_bottom;
        ctx.draw_rect(Rect::from_xywh(box.min_x + 2.0f, box.max_y - (m_popup_handle.is_open() ? 2.0f : 1.0f),
                                      std::max(0.0f, box.width() - 4.0f), m_popup_handle.is_open() ? 2.0f : 1.0f),
                      bottom_line, 0.0f, 0.0f);

        std::string disp = m_time.format(m_use_24h);
        Vec2 ts = TextMeasure::measure(disp, 13.5f);
        ctx.draw_text(disp,
                      PointF{box.min_x + 11.0f, std::round(box.min_y + (box.height() - ts.y) * 0.5f)},
                      13.5f, text_c);

        static constexpr const char* kClockSvg =
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
            "<path fill='#FFFFFF' d='M12 2.5a9.5 9.5 0 1 0 0 19 9.5 9.5 0 0 0 0-19zm0 1.5a8 8 0 1 1 0 16 8 8 0 0 1 0-16zm-.75 3.25a.75.75 0 0 1 1.5 0v4.44l3.03 1.82a.75.75 0 1 1-.76 1.29l-3.4-2.04a.75.75 0 0 1-.37-.65V7.25z'/>"
            "</svg>";
        Color ic = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.30f : 0.76f}
                                 : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.30f : 0.72f};
        Rect cr = Rect::from_xywh(box.max_x - 28.0f, box.min_y, 24.0f, box.height());
        fluent_icons::draw_centered(ctx, kClockSvg, cr, 14.0f, ic);
    }

private:
    Rect box_rect() const noexcept {
        float top = m_header.empty() ? 0.0f : 22.0f;
        return Rect::from_xywh(0.0f, top, size().x, std::max(32.0f, size().y - top));
    }

    void toggle_popup() {
        if (m_popup_handle.is_open()) {
            m_popup_handle.close();
            mark_needs_paint();
            return;
        }
        Window* win = Window::active_window();
        if (!win || m_disabled) return;

        auto token = std::make_shared<bool>(true);
        auto popup = make_render_node<RenderTimePickerPopup>(
            m_time, m_use_24h, m_minute_inc,
            [this, token](const TimeOfDay& picked) {
                if (!*token) return;
                m_time = picked;
                m_popup_handle.close();
                mark_needs_paint();
                if (m_on_time_changed) m_on_time_changed(picked);
            },
            [this, token]() {
                if (!*token) return;
                m_popup_handle.close();
                mark_needs_paint();
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

    TimeOfDay m_time{9, 30};
    std::string m_header;
    bool m_use_24h{true};
    int m_minute_inc{1};
    float m_width{190.0f};
    bool m_disabled{false};
    bool m_hovered{false};
    bool m_pressed{false};
    PopupHandle m_popup_handle;
    Fn<void(const TimeOfDay&)> m_on_time_changed;

public:
    void wrap_on_time_changed(Fn<void(const TimeOfDay&)> extra) {
        auto prev = std::move(m_on_time_changed);
        m_on_time_changed = [prev = std::move(prev), extra = std::move(extra)](const TimeOfDay& t) {
            if (extra) extra(t);
            if (prev) prev(t);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 TimePicker
struct TimePicker : BindableMixin<TimePicker, RenderTimePicker> {
    TimeOfDay time{TimeOfDay{9, 30}};
    std::string header;
    bool use_24h{true};
    int minute_increment{1};
    float width{190.0f};
    bool disabled{false};
    Fn<void(const TimeOfDay&)> on_time_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderTimePicker>& node, Property<TimeOfDay>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_time_value(prop.get());
    std::weak_ptr<RenderTimePicker> weak = node;
    bg.subscribe(prop, [weak, guard](const TimeOfDay& t) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_time_value(t);
        }
    });
    Property<TimeOfDay>* p = &prop;
    node->wrap_on_time_changed([p, guard](const TimeOfDay& t) {
        *guard = true;
        p->set(t);
        *guard = false;
    });
}

class TimePickerWidget : public Widget {
public:
    explicit TimePickerWidget(const TimePicker& desc) : m_desc(desc) {}
    explicit TimePickerWidget(TimePicker&& desc) noexcept : m_desc(std::move(desc)) {}
    ~TimePickerWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTimePicker>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderTimePicker>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderTimePicker>& node) {
        node->set_properties(
            m_desc.time, m_desc.header, m_desc.use_24h, m_desc.minute_increment,
            m_desc.width, m_desc.disabled, m_desc.on_time_changed);
    }

    TimePicker m_desc;
};

inline TimePicker::operator WidgetPtr() const& {
    return make_widget<TimePickerWidget>(*this);
}
inline TimePicker::operator WidgetPtr() && {
    return make_widget<TimePickerWidget>(std::move(*this));
}

} // namespace ktui
