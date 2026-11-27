#pragma once

#include <ktui/core/strings.hpp>

#include "spotlight_tour_data.hpp"
#include "spotlight_tour_style.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/common/fluent_chrome.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {


// 交互式新手引导聚光灯单步配置
struct SpotlightStep {
    std::string title;
    std::string description;
    Fn<Rect()> target_rect_fn;
    float hole_padding{6.0f};
    std::string action_hint{FrameworkStrings::current().spotlight_action_hint};

    static SpotlightStep for_node(std::string title,
                                  std::string desc,
                                  std::weak_ptr<RenderBox> node,
                                  std::string hint = FrameworkStrings::current().spotlight_action_hint) {
        SpotlightStep s;
        s.title = std::move(title);
        s.description = std::move(desc);
        s.action_hint = std::move(hint);
        s.target_rect_fn = [node]() -> Rect {
            auto nb = node.lock();
            if (!nb) return Rect{};
            Vec2 g = nb->global_offset();
            float tb = 0.0f;
            if (Window* w = Window::active_window()) tb = w->titlebar_height();
            return Rect::from_xywh(g.x, g.y + tb, nb->size().x, nb->size().y);
        };
        return s;
    }
};

// Fluent 2 / WinUI 3 TeachingTip 风格全屏交互式聚光灯引导浮层
// - 柔和暗色遮罩 + 双层呼吸光晕圆角镂空框（镂空区点击完全穿透到底层真实控件）
// - TeachingTip 质感卡片：自动多行 UTF-8 折行、PipsPager 步骤圆点、入场缓动与平滑滑移
class RenderSpotlightTourOverlay : public RenderBox {
public:
    RenderSpotlightTourOverlay(std::vector<SpotlightStep> steps,
                               Fn<void(size_t)> on_step_changed,
                               Fn<void(bool)> on_finished)
        : m_steps(std::move(steps)),
          m_on_step_changed(std::move(on_step_changed)),
          m_on_finished(std::move(on_finished)) {
        m_start_time = std::chrono::steady_clock::now();
        m_last_time = m_start_time;
        setup_behavior();
    }

    [[nodiscard]] size_t current_step() const noexcept { return m_step; }
    [[nodiscard]] size_t step_count() const noexcept { return m_steps.size(); }

    void go_to(size_t idx) {
        if (m_steps.empty()) return;
        if (idx >= m_steps.size()) {
            finish(true);
            return;
        }
        m_step = idx;
        mark_needs_paint();
        if (m_on_step_changed) m_on_step_changed(m_step);
    }

    void next() {
        if (m_step + 1 >= m_steps.size()) finish(true);
        else go_to(m_step + 1);
    }

    void prev() {
        if (m_step > 0) go_to(m_step - 1);
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 800.0f;
        float h = m_constraints.has_bounded_height() ? m_constraints.max_height : 600.0f;
        m_size = m_constraints.constrain(Vec2{w, h});
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        // 聚光灯镂空区完全放行事件穿透到底层真实控件
        if (m_hole_init && m_anim_hole.contains(position)) {
            return false;
        }
        if (m_btn_skip.contains(position) || m_btn_prev.contains(position) || m_btn_next.contains(position)) {
            cursor_shape = CursorShape::PointingHand;
        } else {
            cursor_shape = CursorShape::Arrow;
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        if (m_steps.empty() || m_step >= m_steps.size()) return;

        auto st_anim = m_surface.tick();
        auto now = std::chrono::steady_clock::now();
        float dt = std::clamp(std::chrono::duration<float>(now - m_last_time).count(), 0.001f, 0.05f);
        float elapsed = std::chrono::duration<float>(now - m_start_time).count();
        m_last_time = now;

        const auto& st = m_steps[m_step];
        Rect raw = st.target_rect_fn ? st.target_rect_fn() : Rect::from_xywh(m_size.x * 0.5f - 60.0f, m_size.y * 0.5f - 20.0f, 120.0f, 40.0f);
        Rect target_hole = Rect::from_xywh(
            std::max(4.0f, raw.min_x - st.hole_padding),
            std::max(4.0f, raw.min_y - st.hole_padding),
            std::max(16.0f, raw.width() + st.hole_padding * 2.0f),
            std::max(16.0f, raw.height() + st.hole_padding * 2.0f));

        if (!m_hole_init) {
            m_anim_hole = target_hole;
            m_hole_init = true;
        } else {
            float k = 1.0f - std::exp(-dt * 15.0f);
            m_anim_hole.min_x += (target_hole.min_x - m_anim_hole.min_x) * k;
            m_anim_hole.min_y += (target_hole.min_y - m_anim_hole.min_y) * k;
            m_anim_hole.max_x += (target_hole.max_x - m_anim_hole.max_x) * k;
            m_anim_hole.max_y += (target_hole.max_y - m_anim_hole.max_y) * k;
        }

        const Theme& t = ThemeContext::current();
        const auto resolved_style = SpotlightTourStyleResolver::resolve(t);
        const Color accent = resolved_style.accent;
        const float scrim_a = 0.50f * st_anim.alpha;
        const Color dim = Color{0.0f, 0.0f, 0.0f, scrim_a};
        const Rect h = m_anim_hole;

        // 1. 四周柔和半透明暗幕（精准镂空目标区域）
        if (h.min_y > 0.0f) context.draw_rect(Rect::from_xywh(0.0f, 0.0f, m_size.x, h.min_y), dim, 0.0f, 0.0f);
        if (h.max_y < m_size.y) context.draw_rect(Rect::from_xywh(0.0f, h.max_y, m_size.x, m_size.y - h.max_y), dim, 0.0f, 0.0f);
        if (h.min_x > 0.0f) context.draw_rect(Rect::from_xywh(0.0f, h.min_y, h.min_x, h.height()), dim, 0.0f, 0.0f);
        if (h.max_x < m_size.x) context.draw_rect(Rect::from_xywh(h.max_x, h.min_y, m_size.x - h.max_x, h.height()), dim, 0.0f, 0.0f);

        // 2. Fluent 2 双层柔和呼吸光晕环
        float pulse = 0.5f + 0.5f * std::sin(elapsed * 3.6f);
        Rect halo = Rect::from_xywh(h.min_x - 2.0f - pulse * 3.0f, h.min_y - 2.0f - pulse * 3.0f,
                                    h.width() + 4.0f + pulse * 6.0f, h.height() + 4.0f + pulse * 6.0f);
        context.draw_rect(halo, Color{accent.r, accent.g, accent.b, (0.14f + 0.16f * pulse) * st_anim.alpha}, 10.0f, 2.0f);
        context.draw_rect(h, Color{accent.r, accent.g, accent.b, 0.90f * st_anim.alpha}, 7.0f, 1.5f);

        // 3. Fluent 2 TeachingTip 引导浮层（自动 UTF-8 多行折行计算卡片高度）
        const float card_w = 328.0f;
        const float text_w = card_w - 32.0f;
        auto desc_lines = wrap_utf8(st.description, text_w, 12.5f);
        const float line_h = 18.0f;
        const float card_h = 46.0f + static_cast<float>(desc_lines.size()) * line_h + (st.action_hint.empty() ? 0.0f : 28.0f) + 46.0f;

        float target_cx = std::clamp(h.min_x, 14.0f, std::max(14.0f, m_size.x - card_w - 14.0f));
        bool place_below = (h.max_y + 14.0f + card_h <= m_size.y - 14.0f);
        float target_cy = place_below ? (h.max_y + 14.0f) : std::max(14.0f, h.min_y - card_h - 14.0f);
        float enter_dy = (1.0f - st_anim.ease) * (place_below ? 8.0f : -8.0f);
        m_card_rect = Rect::from_xywh(std::round(target_cx), std::round(target_cy + enter_dy), card_w, card_h);

        FluentPopupSurface::paint_panel(context, t, m_card_rect, st_anim.alpha);

        // 顶部极细主题色指示短条（对齐目标中心）
        float caret_x = std::clamp(h.center().x - 16.0f, m_card_rect.min_x + 16.0f, m_card_rect.max_x - 48.0f);
        context.draw_rect(Rect::from_xywh(caret_x, place_below ? m_card_rect.min_y : (m_card_rect.max_y - 2.5f),
                                          32.0f, 2.5f),
                          Color{accent.r, accent.g, accent.b, st_anim.alpha}, 1.25f, 0.0f);

        // 标题 + 右上角关闭按钮 (×)
        float y = m_card_rect.min_y + 16.0f;
        context.draw_text(st.title, PointF{m_card_rect.min_x + 16.0f, y}, 14.5f,
                          Color{resolved_style.text_primary.r, resolved_style.text_primary.g, resolved_style.text_primary.b, st_anim.alpha},
                          FontWeight::SemiBold);

        m_btn_skip = Rect::from_xywh(m_card_rect.max_x - 32.0f, m_card_rect.min_y + 10.0f, 22.0f, 22.0f);
        if (m_hovered_btn == 1) {
            context.draw_rect(m_btn_skip, t.is_dark ? Color{1, 1, 1, 0.09f} : Color{0, 0, 0, 0.06f}, 4.0f, 0.0f);
        }
        Vec2 xs = TextMeasure::measure("×", 13.0f);
        context.draw_text("×",
                          PointF{std::round(m_btn_skip.min_x + (22.0f - xs.x) * 0.5f),
                                 std::round(m_btn_skip.min_y + (22.0f - xs.y) * 0.5f - 0.5f)},
                          13.0f, resolved_style.text_secondary);

        // 多行正文
        y += 26.0f;
        Color body_c{resolved_style.text_secondary.r, resolved_style.text_secondary.g, resolved_style.text_secondary.b, st_anim.alpha};
        for (const auto& line : desc_lines) {
            context.draw_text(line, PointF{m_card_rect.min_x + 16.0f, y}, 12.5f, body_c);
            y += line_h;
        }

        // 交互提示柔和胶囊
        if (!st.action_hint.empty()) {
            y += 4.0f;
            Rect hint_r = Rect::from_xywh(m_card_rect.min_x + 16.0f, y, text_w, 22.0f);
            context.draw_rect(hint_r, Color{accent.r, accent.g, accent.b, (t.is_dark ? 0.14f : 0.09f) * st_anim.alpha}, 4.0f, 0.0f);
            Vec2 hs = TextMeasure::measure(st.action_hint, 11.0f, FontWeight::Medium);
            context.draw_text(st.action_hint,
                              PointF{hint_r.min_x + 8.0f, std::round(hint_r.min_y + (22.0f - hs.y) * 0.5f)},
                              11.0f, Color{accent.r, accent.g, accent.b, st_anim.alpha}, FontWeight::Medium);
        }

        // 底部页脚：左侧 PipsPager 步骤圆点 + 右侧「上一步」「下一步/完成」
        const float fy = m_card_rect.max_y - 38.0f;
        float dot_x = m_card_rect.min_x + 16.0f;
        for (size_t i = 0; i < m_steps.size(); ++i) {
            const bool cur = (i == m_step);
            float dw = cur ? 14.0f : 6.0f;
            Color dc = cur ? accent : (t.is_dark ? Color{1, 1, 1, 0.22f} : Color{0, 0, 0, 0.20f});
            context.draw_rect(Rect::from_xywh(dot_x, fy + 10.0f, dw, 6.0f),
                              Color{dc.r, dc.g, dc.b, dc.a * st_anim.alpha}, 3.0f, 0.0f);
            dot_x += dw + 5.0f;
        }

        m_btn_next = Rect::from_xywh(m_card_rect.max_x - 84.0f, fy, 68.0f, 26.0f);
        m_btn_prev = (m_step > 0) ? Rect::from_xywh(m_btn_next.min_x - 72.0f, fy, 64.0f, 26.0f) : Rect{};

        auto draw_btn = [&](Rect r, const std::string& txt, bool primary, bool hovered) {
            if (r.is_empty()) return;
            Color bbg = primary
                ? Color{accent.r, accent.g, accent.b, (hovered ? 0.90f : 1.0f) * st_anim.alpha}
                : (t.is_dark ? Color{1, 1, 1, (hovered ? 0.11f : 0.055f) * st_anim.alpha}
                             : Color{0, 0, 0, (hovered ? 0.08f : 0.045f) * st_anim.alpha});
            context.draw_rect(r, bbg, 4.0f, 0.0f);
            if (!primary) {
                context.draw_rect(r, t.is_dark ? Color{1, 1, 1, 0.10f * st_anim.alpha} : Color{0, 0, 0, 0.10f * st_anim.alpha}, 4.0f, 1.0f);
            }
            Vec2 ts = TextMeasure::measure(txt, 11.5f, FontWeight::Medium);
            context.draw_text(txt,
                              PointF{std::round(r.min_x + (r.width() - ts.x) * 0.5f),
                                     std::round(r.min_y + (r.height() - ts.y) * 0.5f)},
                              11.5f, primary ? t.colors.on_primary : resolved_style.text_primary, FontWeight::Medium);
        };

        draw_btn(m_btn_prev, FrameworkStrings::current().prev_step, false, m_hovered_btn == 2);
        draw_btn(m_btn_next, (m_step + 1 >= m_steps.size()) ? FrameworkStrings::current().finish : FrameworkStrings::current().next_step, true, m_hovered_btn == 3);

        mark_needs_paint();
    }

private:
    static std::vector<std::string> wrap_utf8(const std::string& text, float max_w, float font_size) {
        std::vector<std::string> lines;
        std::string cur;
        size_t i = 0;
        while (i < text.size()) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            size_t len = 1;
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            if (i + len > text.size()) len = text.size() - i;
            std::string ch = text.substr(i, len);
            i += len;
            if (ch == "\n") {
                lines.push_back(cur);
                cur.clear();
                continue;
            }
            std::string next = cur + ch;
            if (!cur.empty() && TextMeasure::measure_width(next, font_size) > max_w) {
                lines.push_back(cur);
                cur = ch;
            } else {
                cur = std::move(next);
            }
        }
        if (!cur.empty() || lines.empty()) lines.push_back(cur);
        return lines;
    }

    void finish(bool completed) {
        if (m_on_finished) {
            auto cb = m_on_finished;
            m_on_finished = nullptr;
            cb(completed);
        }
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& ev) {
            int h = 0;
            if (m_btn_skip.contains(ev.local_position)) h = 1;
            else if (m_btn_prev.contains(ev.local_position)) h = 2;
            else if (m_btn_next.contains(ev.local_position)) h = 3;
            if (m_hovered_btn != h) {
                m_hovered_btn = h;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            if (m_btn_skip.contains(ev.local_position)) {
                finish(false);
                ev.stop_propagation();
            } else if (m_btn_prev.contains(ev.local_position)) {
                prev();
                ev.stop_propagation();
            } else if (m_btn_next.contains(ev.local_position)) {
                next();
                ev.stop_propagation();
            }
        };
    }

    std::vector<SpotlightStep> m_steps;
    size_t m_step{0};
    Fn<void(size_t)> m_on_step_changed;
    Fn<void(bool)> m_on_finished;

    Rect m_anim_hole{};
    bool m_hole_init{false};
    Rect m_card_rect{};
    Rect m_btn_skip{};
    Rect m_btn_prev{};
    Rect m_btn_next{};
    int m_hovered_btn{0};
    FluentPopupSurface m_surface;
    std::chrono::steady_clock::time_point m_start_time{};
    std::chrono::steady_clock::time_point m_last_time{};
};

class SpotlightTourController {
public:
    ~SpotlightTourController() { stop(); }

    bool start(std::vector<SpotlightStep> steps,
               Fn<void(size_t)> on_step_changed = nullptr,
               Fn<void(bool)> on_finished = nullptr) {
        stop();
        Window* win = Window::active_window();
        if (!win || steps.empty()) return false;

        m_overlay = make_render_node<RenderSpotlightTourOverlay>(
            std::move(steps),
            std::move(on_step_changed),
            [this, on_finished = std::move(on_finished)](bool completed) mutable {
                stop();
                if (on_finished) on_finished(completed);
            });

        OverlayEntry entry;
        entry.content = m_overlay;
        entry.global_pos = Vec2{0.0f, 0.0f};
        entry.size = win->size();
        entry.dismiss_on_outside_click = false;
        entry.is_modal = false;
        entry.allow_hit_passthrough = true;

        m_overlay_id = win->show_overlay(std::move(entry));
        return m_overlay_id != 0;
    }

    void advance() { if (m_overlay) m_overlay->next(); }
    void prev() { if (m_overlay) m_overlay->prev(); }

    void stop() {
        if (m_overlay_id != 0) {
            uint64_t id = m_overlay_id;
            m_overlay_id = 0;
            m_overlay.reset();
            if (Window* w = Window::active_window()) w->hide_overlay(id);
        } else {
            m_overlay.reset();
        }
    }

    [[nodiscard]] bool is_active() const noexcept { return m_overlay_id != 0 && m_overlay != nullptr; }
    [[nodiscard]] size_t current_step() const noexcept { return m_overlay ? m_overlay->current_step() : 0; }

private:
    uint64_t m_overlay_id{0};
    std::shared_ptr<RenderSpotlightTourOverlay> m_overlay;
};
} // namespace ktui
 // namespace ktui
