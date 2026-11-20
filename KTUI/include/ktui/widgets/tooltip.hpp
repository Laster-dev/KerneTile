#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/app/window.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/tree/render_box.hpp>
#include <string>
#include <memory>
#include <algorithm>

namespace ktui {

// 桌面微型悬浮提示框 (RenderTooltip)
class RenderTooltip : public RenderBox {
public:
    explicit RenderTooltip(std::string text, float font_size = 12.0f)
        : m_text(std::move(text)), m_font_size(font_size) {}

    void perform_layout() override {
        float text_w = TextMeasure::measure_width(m_text, m_font_size);
        float w = text_w + m_padding.horizontal();
        float h = m_font_size * 1.30f + m_padding.vertical();
        m_size = Vec2{w, h};
    }

    void paint(PaintContext& context) override {
        // 1. 半透明微暗背景与细边框
        context.draw_rect(bounds(), Color{0.10f, 0.10f, 0.12f, 0.96f}, 2.0f, 0.0f);
        context.draw_rect(bounds(), Color{0.28f, 0.28f, 0.34f, 1.0f}, 2.0f, 1.0f);

        // 2. 文本绘制
        float text_y = m_padding.top + (m_size.y - m_padding.vertical() - m_font_size * 1.30f) * 0.5f;
        context.draw_text(m_text, PointF{m_padding.left, text_y}, m_font_size, Color{0.90f, 0.90f, 0.94f, 1.0f});
    }

private:
    std::string m_text;
    float m_font_size{12.0f};
    EdgeInsets m_padding{EdgeInsets(3.0f, 6.0f)};
};

// 弹出气泡提示辅助函数
inline uint64_t show_tooltip(Window* window, Vec2 anchor_pos, const std::string& text, float font_size = 12.0f) {
    if (!window || text.empty()) return 0;

    float text_w = TextMeasure::measure_width(text, font_size);
    float bubble_w = text_w + 12.0f;
    float bubble_h = font_size * 1.30f + 6.0f;

    Vec2 win_sz = window->size();
    Vec2 pos = Vec2{anchor_pos.x + 12.0f, anchor_pos.y + 16.0f};
    if (pos.x + bubble_w > win_sz.x && win_sz.x > bubble_w) {
        pos.x = std::max(0.0f, anchor_pos.x - bubble_w - 4.0f);
    }
    if (pos.y + bubble_h > win_sz.y && win_sz.y > bubble_h) {
        pos.y = std::max(0.0f, anchor_pos.y - bubble_h - 4.0f);
    }

    auto node = std::make_shared<RenderTooltip>(text, font_size);
    OverlayEntry entry;
    entry.content = node;
    entry.global_pos = pos;
    entry.size = Vec2{bubble_w, bubble_h};
    entry.dismiss_on_outside_click = true;
    entry.is_modal = false;

    return window->show_overlay(std::move(entry));
}

} // namespace ktui
