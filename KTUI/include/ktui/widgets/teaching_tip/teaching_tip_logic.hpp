#pragma once

#include "teaching_tip_data.hpp"
#include "teaching_tip_style.hpp"
#include "../common/fluent_chrome.hpp"
#include "../common/fluent_icons.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <cmath>

namespace ktui {

// WinUI 3 TeachingTip 引导气泡渲染盒
// 复用 FluentPopupSurface 32dp 真高斯阴影与 130ms 缓出展开动效，带指向目标的矢量三角尾标 (Tail)
class RenderTeachingTip : public RenderBox {
public:
    RenderTeachingTip(TeachingTipDesc desc,
                      bool show_tail,
                      bool tail_on_top,
                      float tail_center_x,
                      Fn<void()> on_dismiss_self)
        : m_desc(std::move(desc)),
          m_show_tail(show_tail),
          m_tail_on_top(tail_on_top),
          m_tail_center_x(tail_center_x),
          m_on_dismiss_self(std::move(on_dismiss_self)) {
        cursor_shape = CursorShape::Arrow;
        rebuild_lines();
        setup_behavior();
    }

    ~RenderTeachingTip() override = default;

    [[nodiscard]] Vec2 compute_ideal_size() const noexcept {
        float w = std::clamp(m_desc.width, 240.0f, 440.0f);
        float body_h = 16.0f + 24.0f; // 顶部内边距 + 标题行
        if (!m_lines.empty()) {
            body_h += 8.0f + static_cast<float>(m_lines.size()) * 19.0f;
        }
        bool has_buttons = !m_desc.action_text.empty() || !m_desc.close_text.empty();
        if (has_buttons) {
            body_h += 14.0f + 32.0f;
        }
        body_h += 16.0f; // 底部内边距
        float total_h = body_h + (m_show_tail ? kTailH : 0.0f);
        return Vec2{w, std::ceil(total_h)};
    }

    void perform_layout() override {
        set_size(compute_ideal_size());
    }

    void paint(PaintContext& ctx) override {
        auto st = m_surface.tick();
        if (st.animating) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        const auto resolved_style = TeachingTipStyleResolver::resolve(theme);
        const float alpha = st.alpha;
        const float w = size().x;
        const float h = size().y;

        float panel_y = (m_show_tail && m_tail_on_top) ? kTailH : 0.0f;
        float panel_h = h - (m_show_tail ? kTailH : 0.0f);
        Rect panel = Rect::from_xywh(0.0f, panel_y, w, panel_h);

        // 1. 复用 FluentPopupSurface 绘制 32dp 真高斯阴影 + 8px 圆角面板 (可选亚克力微颗粒与方向性玻璃高光)
        FluentPopupSurface::paint_panel(ctx, theme, panel, alpha, 8.0f);
        if (m_desc.acrylic_grain) {
            Color tint = theme.colors.flyout_surface; tint.a = 0.02f * alpha;
            ctx.draw_acrylic_panel(panel, tint, 8.0f, 0.024f, 1.0f);
        }
        if (m_desc.directional_rim) {
            const Color top_hi = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.26f * alpha} : Color{1.0f, 1.0f, 1.0f, 0.76f * alpha};
            const Color bot_sh = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.34f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.14f * alpha};
            ctx.draw_directional_rim(panel, top_hi, bot_sh, 8.0f, 1.0f, Vec2{-0.18f, -0.98f}, 0.26f);
        }

        // 2. 绘制指向目标锚点的矢量三角尾标 (Tail)
        if (m_show_tail) {
            float tx = std::clamp(m_tail_center_x, 22.0f, w - 22.0f);
            Color surf = theme.colors.flyout_surface;
            surf.a *= alpha;
            static constexpr const char* kTailUpSvg =
                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 20 10'>"
                "<polygon fill='#FFFFFF' points='10,0 20,10 0,10'/>"
                "</svg>";
            static constexpr const char* kTailDownSvg =
                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 20 10'>"
                "<polygon fill='#FFFFFF' points='0,0 20,0 10,10'/>"
                "</svg>";
            if (m_tail_on_top) {
                Rect tr = Rect::from_xywh(std::round(tx - 9.0f), panel.min_y - kTailH + 1.0f, 18.0f, kTailH);
                SvgEngine::draw_svg_xml(ctx, kTailUpSvg, tr, surf);
            } else {
                Rect tr = Rect::from_xywh(std::round(tx - 9.0f), panel.max_y - 1.0f, 18.0f, kTailH);
                SvgEngine::draw_svg_xml(ctx, kTailDownSvg, tr, surf);
            }
        }

        Color title_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.96f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.92f * alpha};
        Color body_c  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.78f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.72f * alpha};
        Color close_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.65f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.60f * alpha};

        // 3. 标题栏 (可选图标 + 标题 + 右上角关闭按钮)
        float tx = panel.min_x + 16.0f;
        float ty = panel.min_y + 16.0f;
        if (!m_desc.svg_path.empty()) {
            Rect ir = Rect::from_xywh(tx, ty + 2.0f, 18.0f, 18.0f);
            Color ic = resolved_style.accent;
            ic.a *= alpha;
            SvgEngine::draw_svg_path(ctx, m_desc.svg_path, 24.0f, 24.0f, ir, ic);
            tx += 26.0f;
        }
        ctx.draw_text(m_desc.title, PointF{tx, ty}, 15.0f, title_c);

        Rect x_r = close_x_rect();
        if (m_hover_x) {
            Color hb = theme.colors.subtle_fill_secondary;
            hb.a *= alpha;
            ctx.draw_rect(x_r, hb, 4.0f, 0.0f);
        }
        fluent_icons::draw_centered(ctx, fluent_icons::kClose, x_r, 12.0f, close_c);

        // 4. 多行说明文本
        float cur_y = ty + 28.0f;
        for (const auto& line : m_lines) {
            ctx.draw_text(line, PointF{panel.min_x + 16.0f, cur_y}, 13.0f, body_c);
            cur_y += 19.0f;
        }

        // 5. 底部主/次操作按钮
        bool has_action = !m_desc.action_text.empty();
        bool has_close = !m_desc.close_text.empty();
        if (has_action || has_close) {
            if (has_action) {
                Rect ar = action_btn_rect();
                Color abg = m_hover_action ? theme.colors.primary_hover : resolved_style.accent;
                abg.a *= alpha;
                ctx.draw_rect(ar, abg, 4.0f, 0.0f);
                Color afg = theme.colors.on_primary;
                afg.a *= alpha;
                Vec2 as = TextMeasure::measure(m_desc.action_text, 13.0f);
                ctx.draw_text(m_desc.action_text,
                              PointF{std::round(ar.min_x + (ar.width() - as.x) * 0.5f),
                                     std::round(ar.min_y + (ar.height() - as.y) * 0.5f)},
                              13.0f, afg);
            }
            if (has_close) {
                Rect cr = close_btn_rect();
                Color cbg = m_hover_close ? theme.colors.control_fill_secondary : theme.colors.control_fill_default;
                cbg.a *= alpha;
                Color cbor = theme.colors.control_stroke_default;
                cbor.a *= alpha;
                ctx.draw_rect(cr, cbg, 4.0f, 0.0f);
                ctx.draw_rect(cr, cbor, 4.0f, 1.0f);
                Vec2 cs = TextMeasure::measure(m_desc.close_text, 13.0f);
                ctx.draw_text(m_desc.close_text,
                              PointF{std::round(cr.min_x + (cr.width() - cs.x) * 0.5f),
                                     std::round(cr.min_y + (cr.height() - cs.y) * 0.5f)},
                              13.0f, title_c);
            }
        }
    }

private:
    static constexpr float kTailH = 9.0f;

    Rect panel_rect() const noexcept {
        float py = (m_show_tail && m_tail_on_top) ? kTailH : 0.0f;
        return Rect::from_xywh(0.0f, py, size().x, size().y - (m_show_tail ? kTailH : 0.0f));
    }

    Rect close_x_rect() const noexcept {
        Rect p = panel_rect();
        return Rect::from_xywh(p.max_x - 36.0f, p.min_y + 10.0f, 26.0f, 26.0f);
    }

    Rect action_btn_rect() const noexcept {
        Rect p = panel_rect();
        bool has_close = !m_desc.close_text.empty();
        float avail_w = p.width() - 32.0f;
        float bw = has_close ? std::floor((avail_w - 8.0f) * 0.5f) : avail_w;
        return Rect::from_xywh(p.min_x + 16.0f, p.max_y - 48.0f, bw, 32.0f);
    }

    Rect close_btn_rect() const noexcept {
        Rect p = panel_rect();
        bool has_action = !m_desc.action_text.empty();
        float avail_w = p.width() - 32.0f;
        if (!has_action) {
            return Rect::from_xywh(p.min_x + 16.0f, p.max_y - 48.0f, avail_w, 32.0f);
        }
        float bw = std::floor((avail_w - 8.0f) * 0.5f);
        return Rect::from_xywh(p.min_x + 16.0f + bw + 8.0f, p.max_y - 48.0f, avail_w - bw - 8.0f, 32.0f);
    }

    void rebuild_lines() {
        m_lines.clear();
        if (m_desc.subtitle.empty()) return;
        float max_w = std::clamp(m_desc.width, 240.0f, 440.0f) - 32.0f;
        std::string cur;
        size_t i = 0;
        const std::string& s = m_desc.subtitle;
        while (i < s.size()) {
            if (s[i] == '\n') {
                m_lines.push_back(cur);
                cur.clear();
                ++i;
                continue;
            }
            size_t len = 1;
            unsigned char c = static_cast<unsigned char>(s[i]);
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            len = std::min(len, s.size() - i);
            std::string ch = s.substr(i, len);
            if (!cur.empty() && TextMeasure::measure_width(cur + ch, 13.0f) > max_w) {
                m_lines.push_back(cur);
                cur = ch;
            } else {
                cur += ch;
            }
            i += len;
        }
        if (!cur.empty()) m_lines.push_back(cur);
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            bool hx = close_x_rect().contains(e.local_position);
            bool ha = !m_desc.action_text.empty() && action_btn_rect().contains(e.local_position);
            bool hc = !m_desc.close_text.empty() && close_btn_rect().contains(e.local_position);
            cursor_shape = (hx || ha || hc) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (hx != m_hover_x || ha != m_hover_action || hc != m_hover_close) {
                m_hover_x = hx;
                m_hover_action = ha;
                m_hover_close = hc;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hover_x = m_hover_action = m_hover_close = false;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            Rect x_hit = e.is_touch() ? touch::expand_hit_rect(close_x_rect(), 40.0f) : close_x_rect();
            Rect act_hit = e.is_touch() ? touch::expand_hit_rect(action_btn_rect(), 40.0f) : action_btn_rect();
            Rect cls_hit = e.is_touch() ? touch::expand_hit_rect(close_btn_rect(), 40.0f) : close_btn_rect();
            if (x_hit.contains(e.local_position)) {
                auto cb = m_desc.on_close;
                auto dis = m_on_dismiss_self;
                if (dis) dis();
                if (cb) cb();
                e.stop_propagation();
                return;
            }
            if (!m_desc.action_text.empty() && act_hit.contains(e.local_position)) {
                auto cb = m_desc.on_action;
                auto dis = m_on_dismiss_self;
                if (dis) dis();
                if (cb) cb();
                e.stop_propagation();
                return;
            }
            if (!m_desc.close_text.empty() && cls_hit.contains(e.local_position)) {
                auto cb = m_desc.on_close;
                auto dis = m_on_dismiss_self;
                if (dis) dis();
                if (cb) cb();
                e.stop_propagation();
                return;
            }
            if (e.is_touch()) {
                m_touch_swiping = true;
                m_touch_start = e.local_position;
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (m_touch_swiping && e.is_touch()) {
                m_touch_swiping = false;
                float dist = std::hypot(e.local_position.x - m_touch_start.x, e.local_position.y - m_touch_start.y);
                if (dist > 48.0f) {
                    auto cb = m_desc.on_close;
                    auto dis = m_on_dismiss_self;
                    if (dis) dis();
                    if (cb) cb();
                    e.stop_propagation();
                }
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_touch_swiping = false;
            m_hover_x = m_hover_action = m_hover_close = false;
            mark_needs_paint();
        };
    }

    TeachingTipDesc m_desc;
    bool m_touch_swiping{false};
    PointF m_touch_start{};
    bool m_show_tail{true};
    bool m_tail_on_top{true};
    float m_tail_center_x{160.0f};
    Fn<void()> m_on_dismiss_self;
    std::vector<std::string> m_lines;
    FluentPopupSurface m_surface;
    bool m_hover_x{false};
    bool m_hover_action{false};
    bool m_hover_close{false};
};

// 静态便捷引导气泡弹出器 (TeachingTip::show)
struct TeachingTip {
    static uint64_t show(TeachingTipDesc desc, Vec2 anchor_window_pos = Vec2{-1.0f, -1.0f}) {
        Window* win = Window::active_window();
        if (!win) return 0;

        bool has_anchor = (anchor_window_pos.x >= 0.0f && anchor_window_pos.y >= 0.0f);
        Vec2 win_sz = win->size();
        float tb = win->titlebar_height();

        // 预计算尺寸与上下翻转方向
        RenderTeachingTip probe(desc, has_anchor, true, 160.0f, nullptr);
        Vec2 sz = probe.compute_ideal_size();

        bool tail_on_top = (desc.preferred_placement != TeachingTipPlacement::Top);
        float gx = 0.0f;
        float gy = 0.0f;
        float tail_x = sz.x * 0.5f;

        if (has_anchor) {
            gx = std::clamp(anchor_window_pos.x - sz.x * 0.5f, 10.0f, std::max(10.0f, win_sz.x - sz.x - 10.0f));
            tail_x = anchor_window_pos.x - gx;
            if (tail_on_top) {
                gy = anchor_window_pos.y + 6.0f;
                if (gy + sz.y > win_sz.y - 10.0f) {
                    tail_on_top = false;
                    gy = std::max(tb + 8.0f, anchor_window_pos.y - sz.y - 6.0f);
                }
            } else {
                gy = anchor_window_pos.y - sz.y - 6.0f;
                if (gy < tb + 8.0f) {
                    tail_on_top = true;
                    gy = anchor_window_pos.y + 6.0f;
                }
            }
        } else {
            gx = std::max(10.0f, (win_sz.x - sz.x) * 0.5f);
            gy = std::max(tb + 24.0f, (win_sz.y - sz.y) * 0.35f);
        }

        auto overlay_id = std::make_shared<uint64_t>(0);
        auto tip = make_render_node<RenderTeachingTip>(
            std::move(desc),
            has_anchor,
            tail_on_top,
            tail_x,
            [overlay_id]() {
                if (Window* w = Window::active_window(); w && *overlay_id != 0) {
                    uint64_t id = *overlay_id;
                    *overlay_id = 0;
                    w->hide_overlay(id);
                }
            });

        OverlayEntry entry;
        entry.content = tip;
        entry.global_pos = Vec2{std::round(gx), std::round(gy)};
        entry.size = sz;
        entry.dismiss_on_outside_click = true;
        entry.is_modal = false;

        *overlay_id = win->show_overlay(std::move(entry));
        return *overlay_id;
    }
};
} // namespace ktui
 // namespace ktui
