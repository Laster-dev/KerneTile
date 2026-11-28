#pragma once

#include "content_dialog_data.hpp"
#include "content_dialog_style.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

// WinUI 3 模态对话框浮层渲染盒 (RenderContentDialog)
class RenderContentDialog : public RenderBox {
public:
    RenderContentDialog(ContentDialogDesc desc, Fn<void(ContentDialogResult)> on_close)
        : m_desc(std::move(desc)), m_on_close(std::move(on_close)) {
        m_open_start = std::chrono::steady_clock::now();
        setup_behavior();
    }

    ~RenderContentDialog() override = default;

    [[nodiscard]] Vec2 compute_size() const {
        float max_w = 540.0f;
        if (m_constraints.has_bounded_width()) {
            max_w = std::min(max_w, std::max(220.0f, m_constraints.max_width));
        } else if (Window* win = Window::active_window(); win && win->size().x > 1.0f) {
            max_w = std::min(max_w, std::max(220.0f, win->size().x - 24.0f));
        }
        float min_w = std::min(300.0f, max_w);
        float w = std::clamp(m_desc.width > 0.0f ? m_desc.width : 420.0f, min_w, max_w);
        float text_w = std::max(80.0f, w - 48.0f);
        int lines = count_wrapped_lines(m_desc.content, 13.5f, text_w);
        float body_h = 24.0f + (m_desc.title.empty() ? 0.0f : 32.0f) +
                       static_cast<float>(std::max(1, lines)) * 21.0f + 22.0f;
        float footer_h = 72.0f;
        return Vec2{w, std::ceil(body_h + footer_h)};
    }

    void perform_layout() override {
        set_size(compute_size());
    }

    void paint(PaintContext& ctx) override {
        auto now = std::chrono::steady_clock::now();
        float scale_factor = 1.0f;
        float alpha = 1.0f;
        float smoke_alpha = 1.0f;

        if (m_closing) {
            float elapsed = std::chrono::duration<float>(now - m_close_start).count();
            float t = std::clamp(elapsed / 0.12f, 0.0f, 1.0f);
            // WinUI 3 DialogHidden: Accelerate 1.00 -> 1.05, Opacity 1 -> 0 (120ms)
            float accel = t * t;
            scale_factor = 1.0f + 0.05f * accel;
            alpha = 1.0f - t;
            smoke_alpha = 1.0f - t;
            if (t < 1.0f) {
                mark_needs_paint();
            } else if (!m_close_finished) {
                m_close_finished = true;
                if (m_on_close_anim_done) m_on_close_anim_done();
                return;
            }
        } else {
            float elapsed = std::chrono::duration<float>(now - m_open_start).count();
            float t_scale = std::clamp(elapsed / 0.25f, 0.0f, 1.0f);
            float t_alpha = std::clamp(elapsed / 0.15f, 0.0f, 1.0f);
            float t_smoke = std::clamp(elapsed / 0.18f, 0.0f, 1.0f);
            // WinUI 3 DialogShowing: Decelerate (KeySpline 0.1,0.9 0.2,1.0) Scale 1.05 -> 1.00 (250ms)
            float ease_scale = 1.0f - std::pow(1.0f - t_scale, 4.0f);
            scale_factor = 1.05f - 0.05f * ease_scale;
            alpha = 1.0f - std::pow(1.0f - t_alpha, 2.0f);
            smoke_alpha = 1.0f - std::pow(1.0f - t_smoke, 2.0f);
            m_open_progress = t_scale;
            if (t_scale < 1.0f || t_smoke < 1.0f) {
                mark_needs_paint();
            }
        }

        const Theme& theme = ThemeContext::current();

        // 0. 全窗口平滑淡入/淡出 Smoke 遮罩层 (实时跟踪当前窗口尺寸与居中偏移，最大化/缩放无缝铺满)
        Vec2 live_win_sz = m_smoke_window_size;
        if (Window* active_win = Window::active_window()) {
            Vec2 wsz = active_win->size();
            if (wsz.x > 0.0f && wsz.y > 0.0f) {
                live_win_sz = wsz;
            }
        }
        Vec2 live_pos = offset();
        if (live_pos.x == 0.0f && live_pos.y == 0.0f) {
            live_pos = m_dialog_global_pos;
        }
        if (live_win_sz.x > 0.0f && live_win_sz.y > 0.0f && smoke_alpha > 0.001f) {
            float max_smoke = theme.is_dark ? 0.45f : 0.30f;
            ctx.draw_rect(
                Rect::from_xywh(-live_pos.x, -live_pos.y,
                                live_win_sz.x, live_win_sz.y),
                Color{0.0f, 0.0f, 0.0f, max_smoke * smoke_alpha}, 0.0f, 0.0f);
        }

        const float base_w = size().x;
        const float base_h = size().y;
        const float w = base_w * scale_factor;
        const float h = base_h * scale_factor;
        const float dx = (base_w - w) * 0.5f;
        const float dy = (base_h - h) * 0.5f;
        const float footer_h = 72.0f * scale_factor;
        const float body_h = h - footer_h;

        Color body_bg = theme.is_dark
            ? Color{43.0f / 255.0f, 43.0f / 255.0f, 43.0f / 255.0f, alpha}
            : Color{1.0f, 1.0f, 1.0f, alpha};
        Color footer_bg = theme.is_dark
            ? Color{32.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f, alpha}
            : Color{243.0f / 255.0f, 243.0f / 255.0f, 243.0f / 255.0f, alpha};
        Color border = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.10f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.12f * alpha};
        Color divider = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.07f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.08f * alpha};
        Color title_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.96f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.92f * alpha};
        Color msg_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.78f * alpha}
            : Color{0.0f, 0.0f, 0.0f, 0.72f * alpha};

        // 1. WinUI 3 Dialog 64dp 双层真实高斯软阴影
        ctx.draw_elevation_shadow(Rect::from_xywh(dx, dy, w, h), 8.0f, theme.is_dark, 64.0f, alpha);

        // 2. WinUI 3 双段底板 (上部内容区 + 下部操作条；可选亚克力微颗粒与方向性玻璃高光)
        const Rect dlg_rect = Rect::from_xywh(dx, dy, w, h);
        ctx.draw_rounded_rect(Rect::from_xywh(dx, dy, w, body_h), body_bg, 8.0f, 8.0f, 0.0f, 0.0f);
        ctx.draw_rounded_rect(Rect::from_xywh(dx, dy + body_h, w, footer_h), footer_bg, 0.0f, 0.0f, 8.0f, 8.0f);
        if (m_desc.acrylic_grain) {
            Color tint = body_bg; tint.a = 0.02f * alpha;
            ctx.draw_acrylic_panel(dlg_rect, tint, 8.0f, 0.024f, 1.0f);
        }
        ctx.draw_rect(Rect::from_xywh(dx, dy + body_h, w, 1.0f), divider, 0.0f, 0.0f);
        ctx.draw_rect(dlg_rect, border, 8.0f, 1.0f);
        if (m_desc.directional_rim) {
            const Color top_hi = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.28f * alpha} : Color{1.0f, 1.0f, 1.0f, 0.78f * alpha};
            const Color bot_sh = theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.36f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.16f * alpha};
            ctx.draw_directional_rim(dlg_rect, top_hi, bot_sh, 8.0f, 1.0f, Vec2{-0.18f, -0.98f}, 0.28f);
        }

        // 3. 标题与多行折行正文
        float cur_y = dy + 24.0f * scale_factor;
        float text_x = dx + 24.0f * scale_factor;
        if (!m_desc.title.empty()) {
            ctx.draw_text(m_desc.title, PointF{std::round(text_x), std::round(cur_y)}, 18.0f, title_col);
            cur_y += 32.0f * scale_factor;
        }
        if (!m_desc.content.empty()) {
            auto lines = wrap_lines(m_desc.content, 13.5f, base_w - 48.0f);
            for (const auto& line : lines) {
                ctx.draw_text(line, PointF{std::round(text_x), std::round(cur_y)}, 13.5f, msg_col);
                cur_y += 21.0f * scale_factor;
            }
        }

        // 4. 底部等宽按钮组 (Primary Accent / Secondary / Close)
        m_buttons.clear();
        struct BtnSpec {
            std::string label;
            ContentDialogResult res;
            bool accent;
        };
        std::vector<BtnSpec> specs;
        if (!m_desc.primary_button_text.empty()) specs.push_back({m_desc.primary_button_text, ContentDialogResult::Primary, true});
        if (!m_desc.secondary_button_text.empty()) specs.push_back({m_desc.secondary_button_text, ContentDialogResult::Secondary, false});
        if (!m_desc.close_button_text.empty()) specs.push_back({m_desc.close_button_text, ContentDialogResult::Close, false});

        const int n = static_cast<int>(specs.size());
        if (n > 0) {
            const float pad_x = 24.0f * scale_factor;
            const float gap = 8.0f * scale_factor;
            const float btn_h = 32.0f * scale_factor;
            const float btn_y = std::round(dy + body_h + (footer_h - btn_h) * 0.5f);
            const float avail_w = w - pad_x * 2.0f - gap * static_cast<float>(n - 1);
            const float btn_w = (n == 1) ? std::min(160.0f * scale_factor, avail_w) : (avail_w / static_cast<float>(n));
            float start_x = dx + ((n == 1) ? (w - pad_x - btn_w) : pad_x);

            for (int i = 0; i < n; ++i) {
                Rect br = Rect::from_xywh(std::round(start_x + static_cast<float>(i) * (btn_w + gap)), btn_y, std::round(btn_w), btn_h);
                m_buttons.push_back({br, specs[static_cast<size_t>(i)].res});

                bool hov = (m_hovered_btn == i);
                bool prs = (m_pressed_btn == i);
                Color bbg, bbd, btc;
                if (specs[static_cast<size_t>(i)].accent) {
                    if (theme.is_dark) {
                        bbg = prs ? Color{76.0f / 255.0f, 180.0f / 255.0f, 229.0f / 255.0f, alpha}
                                  : (hov ? Color{108.0f / 255.0f, 210.0f / 255.0f, 255.0f / 255.0f, alpha}
                                         : Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, alpha});
                        bbd = Color::transparent();
                        btc = Color{0.0f, 0.0f, 0.0f, 0.90f * alpha};
                    } else {
                        bbg = prs ? Color{49.0f / 255.0f, 126.0f / 255.0f, 200.0f / 255.0f, alpha}
                                  : (hov ? Color{25.0f / 255.0f, 110.0f / 255.0f, 192.0f / 255.0f, alpha}
                                         : Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, alpha});
                        bbd = Color::transparent();
                        btc = Color{1.0f, 1.0f, 1.0f, alpha};
                    }
                } else {
                    if (theme.is_dark) {
                        bbg = prs ? Color{1.0f, 1.0f, 1.0f, 0.04f * alpha}
                                  : (hov ? Color{1.0f, 1.0f, 1.0f, 0.09f * alpha} : Color{1.0f, 1.0f, 1.0f, 0.06f * alpha});
                        bbd = Color{1.0f, 1.0f, 1.0f, 0.09f * alpha};
                        btc = Color{1.0f, 1.0f, 1.0f, 0.96f * alpha};
                    } else {
                        bbg = prs ? Color{0.0f, 0.0f, 0.0f, 0.04f * alpha}
                                  : (hov ? Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.65f * alpha}
                                         : Color{1.0f, 1.0f, 1.0f, 0.90f * alpha});
                        bbd = Color{0.0f, 0.0f, 0.0f, 0.10f * alpha};
                        btc = Color{0.0f, 0.0f, 0.0f, 0.90f * alpha};
                    }
                }
                ctx.draw_rect(br, bbg, 4.0f, 0.0f);
                if (bbd.a > 0.001f) ctx.draw_rect(br, bbd, 4.0f, 1.0f);
                Vec2 lsz = TextMeasure::measure(specs[static_cast<size_t>(i)].label, 13.0f);
                ctx.draw_text(specs[static_cast<size_t>(i)].label,
                              PointF{std::round(br.min_x + (br.width() - lsz.x) * 0.5f),
                                     std::round(br.min_y + (br.height() - lsz.y) * 0.5f)},
                              13.0f, btc);
            }
        }
    }

    void set_smoke_backdrop_bounds(Vec2 dialog_global_pos, Vec2 window_size) {
        m_dialog_global_pos = dialog_global_pos;
        m_smoke_window_size = window_size;
    }

    void begin_close(Fn<void()> on_done) {
        if (m_closing) return;
        m_closing = true;
        m_close_start = std::chrono::steady_clock::now();
        m_on_close_anim_done = std::move(on_done);
        mark_needs_paint();
    }

private:
    struct ButtonHit {
        Rect rect;
        ContentDialogResult result;
    };

    static std::vector<std::string> wrap_lines(const std::string& text, float font_size, float max_w) {
        std::vector<std::string> lines;
        if (text.empty()) return lines;
        const auto stops = kernetile::text::grapheme_boundaries(text);
        size_t start = 0;
        size_t pos = 0;
        while (pos < text.size()) {
            auto it = std::upper_bound(stops.begin(), stops.end(), pos);
            size_t next = (it == stops.end()) ? text.size() : *it;
            if (text[pos] == '\n') {
                lines.push_back(text.substr(start, pos - start));
                start = next;
                pos = next;
                continue;
            }
            float w = TextMeasure::measure_width(std::string_view(text).substr(start, next - start), font_size);
            if (w > max_w && pos > start) {
                lines.push_back(text.substr(start, pos - start));
                start = pos;
            } else {
                pos = next;
            }
        }
        if (start < text.size()) lines.push_back(text.substr(start));
        return lines;
    }

    static int count_wrapped_lines(const std::string& text, float font_size, float max_w) {
        return static_cast<int>(wrap_lines(text, font_size, max_w).size());
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int hov = -1;
            for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
                if (m_buttons[static_cast<size_t>(i)].rect.contains(e.local_position)) {
                    hov = i;
                    break;
                }
            }
            if (hov != m_hovered_btn) {
                m_hovered_btn = hov;
                cursor_shape = (hov >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
                mark_needs_paint();
            }
            e.stop_propagation();
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_btn != -1 || m_pressed_btn != -1) {
                m_hovered_btn = -1;
                m_pressed_btn = -1;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            if (m_hovered_btn != -1 || m_pressed_btn != -1) {
                m_hovered_btn = -1;
                m_pressed_btn = -1;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
                if (m_buttons[static_cast<size_t>(i)].rect.contains(e.local_position)) {
                    m_pressed_btn = i;
                    mark_needs_paint();
                    break;
                }
            }
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& e) {
            int pressed = m_pressed_btn;
            m_pressed_btn = -1;
            if (pressed >= 0 && pressed < static_cast<int>(m_buttons.size()) &&
                m_buttons[static_cast<size_t>(pressed)].rect.contains(e.local_position)) {
                ContentDialogResult res = m_buttons[static_cast<size_t>(pressed)].result;
                if (m_on_close) m_on_close(res);
            }
            mark_needs_paint();
            e.stop_propagation();
        };
    }

    ContentDialogDesc m_desc;
    Fn<void(ContentDialogResult)> m_on_close;
    std::vector<ButtonHit> m_buttons;
    int m_hovered_btn{-1};
    int m_pressed_btn{-1};
    float m_open_progress{0.0f};
    std::chrono::steady_clock::time_point m_open_start;
    bool m_closing{false};
    bool m_close_finished{false};
    std::chrono::steady_clock::time_point m_close_start;
    Fn<void()> m_on_close_anim_done;
    Vec2 m_dialog_global_pos{0.0f, 0.0f};
    Vec2 m_smoke_window_size{0.0f, 0.0f};
};

// 静态便捷弹出器 (ContentDialog::show)
struct ContentDialog {
    static uint64_t show(const ContentDialogDesc& desc) {
        Window* win = Window::active_window();
        if (!win) return 0;

        auto overlay_id = std::make_shared<uint64_t>(0);
        auto on_result = desc.on_result;
        auto result_fired = std::make_shared<bool>(false);
        auto dialog_weak = std::make_shared<std::weak_ptr<RenderContentDialog>>();

        Vec2 win_sz = win->size();
        ContentDialogDesc clamped_desc = desc;
        if (win_sz.x > 1.0f) {
            clamped_desc.width = std::min(clamped_desc.width, std::max(240.0f, win_sz.x - 24.0f));
        }

        auto dialog = make_render_node<RenderContentDialog>(
            clamped_desc,
            [overlay_id, on_result, result_fired, dialog_weak](ContentDialogResult res) {
                if (*result_fired) return;
                *result_fired = true;
                if (on_result) on_result(res);
                if (auto dlg = dialog_weak->lock()) {
                    dlg->begin_close([overlay_id]() {
                        if (Window* w = Window::active_window(); w && *overlay_id != 0) {
                            uint64_t id = *overlay_id;
                            *overlay_id = 0;
                            w->hide_overlay(id);
                        }
                    });
                } else if (Window* w = Window::active_window(); w && *overlay_id != 0) {
                    uint64_t id = *overlay_id;
                    *overlay_id = 0;
                    w->hide_overlay(id);
                }
            });
        *dialog_weak = dialog;

        Vec2 dlg_sz = dialog->compute_size();
        float gx = std::max(12.0f, std::round((win_sz.x - dlg_sz.x) * 0.5f));
        float gy = std::max(win->titlebar_height() + 12.0f, std::round((win_sz.y - dlg_sz.y) * 0.5f));
        dialog->set_smoke_backdrop_bounds(Vec2{gx, gy}, win_sz);

        OverlayEntry entry;
        entry.content = dialog;
        entry.global_pos = Vec2{gx, gy};
        entry.size = dlg_sz;
        entry.dismiss_on_outside_click = false;
        entry.is_modal = true;
        entry.center_in_viewport = true;
        entry.barrier_color = Color::transparent(); // 由 RenderContentDialog 自绘平滑淡入/淡出 Smoke 遮罩
        entry.on_dismiss = [on_result, result_fired]() {
            if (!*result_fired && on_result) {
                *result_fired = true;
                on_result(ContentDialogResult::None);
            }
        };

        *overlay_id = win->show_overlay(std::move(entry));
        return *overlay_id;
    }
};
} // namespace ktui
 // namespace ktui
