#pragma once

#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// WinUI 3 ContentDialog 返回结果
enum class ContentDialogResult : uint8_t {
    None      = 0,
    Primary   = 1,
    Secondary = 2,
    Close     = 3
};

// WinUI 3 模态对话框配置描述
struct ContentDialogDesc {
    std::string title;
    std::string content;
    std::string primary_button_text;     // 主强调按钮 (可选)
    std::string secondary_button_text;   // 次操作按钮 (可选)
    std::string close_button_text{"取消"}; // 关闭按钮
    float width{420.0f};
    std::function<void(ContentDialogResult)> on_result{nullptr};
};

// WinUI 3 模态对话框浮层渲染盒 (RenderContentDialog)
class RenderContentDialog : public RenderBox {
public:
    RenderContentDialog(ContentDialogDesc desc, std::function<void(ContentDialogResult)> on_close)
        : m_desc(std::move(desc)), m_on_close(std::move(on_close)) {
        m_open_start = std::chrono::steady_clock::now();
        setup_behavior();
    }

    ~RenderContentDialog() override = default;

    [[nodiscard]] Vec2 compute_size() const {
        float w = std::clamp(m_desc.width > 0.0f ? m_desc.width : 420.0f, 300.0f, 540.0f);
        float text_w = w - 48.0f;
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
        if (m_open_progress < 1.0f) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - m_open_start).count();
            m_open_progress = std::clamp(elapsed / 0.16f, 0.0f, 1.0f);
            if (m_open_progress < 1.0f) mark_needs_paint();
        }

        const Theme& theme = ThemeContext::current();
        float ease = 1.0f - std::pow(1.0f - m_open_progress, 3.0f);
        float alpha = std::clamp(ease * 1.15f, 0.0f, 1.0f);
        float y_off = std::round((1.0f - ease) * 10.0f);

        const float w = size().x;
        const float h = size().y;
        const float footer_h = 72.0f;
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
        ctx.draw_elevation_shadow(Rect::from_xywh(0.0f, y_off, w, h), 8.0f, theme.is_dark, 64.0f, alpha);

        // 2. WinUI 3 双段底板 (上部内容区 + 下部操作条)
        ctx.draw_rounded_rect(Rect::from_xywh(0.0f, y_off, w, body_h), body_bg, 8.0f, 8.0f, 0.0f, 0.0f);
        ctx.draw_rounded_rect(Rect::from_xywh(0.0f, y_off + body_h, w, footer_h), footer_bg, 0.0f, 0.0f, 8.0f, 8.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, y_off + body_h, w, 1.0f), divider, 0.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, y_off, w, h), border, 8.0f, 1.0f);

        // 3. 标题与多行折行正文
        float cur_y = y_off + 24.0f;
        if (!m_desc.title.empty()) {
            ctx.draw_text(m_desc.title, PointF{24.0f, std::round(cur_y)}, 18.0f, title_col);
            cur_y += 32.0f;
        }
        if (!m_desc.content.empty()) {
            auto lines = wrap_lines(m_desc.content, 13.5f, w - 48.0f);
            for (const auto& line : lines) {
                ctx.draw_text(line, PointF{24.0f, std::round(cur_y)}, 13.5f, msg_col);
                cur_y += 21.0f;
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
            const float pad_x = 24.0f;
            const float gap = 8.0f;
            const float btn_h = 32.0f;
            const float btn_y = std::round(y_off + body_h + (footer_h - btn_h) * 0.5f);
            const float avail_w = w - pad_x * 2.0f - gap * static_cast<float>(n - 1);
            const float btn_w = (n == 1) ? std::min(160.0f, avail_w) : (avail_w / static_cast<float>(n));
            float start_x = (n == 1) ? (w - pad_x - btn_w) : pad_x;

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
    std::function<void(ContentDialogResult)> m_on_close;
    std::vector<ButtonHit> m_buttons;
    int m_hovered_btn{-1};
    int m_pressed_btn{-1};
    float m_open_progress{0.0f};
    std::chrono::steady_clock::time_point m_open_start;
};

// 静态便捷弹出器 (ContentDialog::show)
struct ContentDialog {
    static uint64_t show(const ContentDialogDesc& desc) {
        Window* win = Window::active_window();
        if (!win) return 0;

        auto overlay_id = std::make_shared<uint64_t>(0);
        auto on_result = desc.on_result;
        auto result_fired = std::make_shared<bool>(false);

        auto dialog = std::make_shared<RenderContentDialog>(
            desc,
            [overlay_id, on_result, result_fired](ContentDialogResult res) {
                *result_fired = true;
                if (Window* w = Window::active_window(); w && *overlay_id != 0) {
                    uint64_t id = *overlay_id;
                    *overlay_id = 0;
                    w->hide_overlay(id);
                }
                if (on_result) on_result(res);
            });

        Vec2 dlg_sz = dialog->compute_size();
        Vec2 win_sz = win->size();
        float gx = std::round((win_sz.x - dlg_sz.x) * 0.5f);
        float gy = std::round((win_sz.y - dlg_sz.y) * 0.5f);

        OverlayEntry entry;
        entry.content = dialog;
        entry.global_pos = Vec2{std::max(12.0f, gx), std::max(win->titlebar_height() + 12.0f, gy)};
        entry.size = dlg_sz;
        entry.dismiss_on_outside_click = false;
        entry.is_modal = true;
        entry.barrier_color = Color{0.0f, 0.0f, 0.0f, 0.45f};
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
