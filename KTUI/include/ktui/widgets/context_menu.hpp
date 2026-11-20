#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/app/window.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/widgets/widget.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>

namespace ktui {

// 上下文菜单项 (MenuItem)
struct MenuItem {
    std::string label{""};
    std::string shortcut{""};
    bool enabled{true};
    bool is_separator{false};
    std::function<void()> on_click{nullptr};

    static MenuItem separator() noexcept {
        MenuItem m;
        m.is_separator = true;
        return m;
    }

    static MenuItem item(std::string text, std::function<void()> cb = nullptr, std::string sc = "", bool en = true) {
        MenuItem m;
        m.label = std::move(text);
        m.on_click = std::move(cb);
        m.shortcut = std::move(sc);
        m.enabled = en;
        m.is_separator = false;
        return m;
    }
};

// 桌面高信息密度右键菜单渲染盒 (RenderContextMenu)
class RenderContextMenu : public RenderBox {
public:
    RenderContextMenu(std::vector<MenuItem> items, float min_width = 160.0f,
                      std::function<void()> on_close = nullptr)
        : m_items(std::move(items)), m_min_width(min_width), m_on_close(std::move(on_close)) {
        setup_behavior();
    }

    ~RenderContextMenu() override = default;

    void perform_layout() override {
        float total_h = m_padding.vertical();
        for (const auto& item : m_items) {
            if (item.is_separator) {
                total_h += 6.0f; // 分割线高度 6px (包含上下间距)
            } else {
                total_h += m_item_height;
            }
        }
        float w = std::max(m_min_width, m_constraints.min_width);
        if (m_constraints.has_bounded_width()) {
            w = std::min(w, m_constraints.max_width);
        }
        m_size = Vec2{w, total_h};
    }

    void paint(PaintContext& context) override {
        // 1. 绘制硬朗微圆角背景与 1px 细边框
        context.draw_rect(bounds(), m_bg_color, m_corner_radius, 0.0f);
        context.draw_rect(bounds(), m_border_color, m_corner_radius, 1.0f);

        // 2. 依次绘制菜单项
        float cur_y = m_padding.top;
        for (size_t i = 0; i < m_items.size(); ++i) {
            const auto& item = m_items[i];
            if (item.is_separator) {
                float sep_y = cur_y + 3.0f;
                context.draw_rect(Rect::from_xywh(m_padding.left, sep_y, m_size.x - m_padding.horizontal(), 1.0f),
                                  m_separator_color, 0.0f, 0.0f);
                cur_y += 6.0f;
                continue;
            }

            Rect item_rect = Rect::from_xywh(m_padding.left, cur_y, m_size.x - m_padding.horizontal(), m_item_height);

            // 悬停高亮
            if (i == m_hovered_index && item.enabled) {
                context.draw_rect(item_rect, m_hover_bg_color, 2.0f, 0.0f);
            }

            // 文本绘制
            Color text_col = item.enabled ? (i == m_hovered_index ? Color::white() : m_text_color) : m_disabled_color;
            float text_y = cur_y + (m_item_height - m_font_size * 1.30f) * 0.5f;

            // 左侧主标签
            context.draw_text(item.label, PointF{item_rect.min_x + 8.0f, text_y}, m_font_size, text_col);

            // 右侧快捷键提示
            if (!item.shortcut.empty()) {
                Color sc_col = item.enabled ? (i == m_hovered_index ? Color{0.90f, 0.90f, 0.95f, 1.0f} : m_shortcut_color) : m_disabled_color;
                // 估算快捷键文本右对齐
                float sc_w = item.shortcut.length() * m_font_size * 0.55f;
                float sc_x = std::max(item_rect.min_x + 10.0f, item_rect.max_x - sc_w - 8.0f);
                context.draw_text(item.shortcut, PointF{sc_x, text_y}, m_font_size * 0.92f, sc_col);
            }

            cur_y += m_item_height;
        }
    }

private:
    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& ev) {
            float y = ev.local_position.y - m_padding.top;
            size_t new_hover = static_cast<size_t>(-1);
            float cur_y = 0.0f;
            for (size_t i = 0; i < m_items.size(); ++i) {
                float h = m_items[i].is_separator ? 6.0f : m_item_height;
                if (y >= cur_y && y < cur_y + h) {
                    if (!m_items[i].is_separator && m_items[i].enabled) {
                        new_hover = i;
                    }
                    break;
                }
                cur_y += h;
            }
            if (new_hover != m_hovered_index) {
                m_hovered_index = new_hover;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            float y = ev.local_position.y - m_padding.top;
            float cur_y = 0.0f;
            for (size_t i = 0; i < m_items.size(); ++i) {
                float h = m_items[i].is_separator ? 6.0f : m_item_height;
                if (y >= cur_y && y < cur_y + h) {
                    if (!m_items[i].is_separator && m_items[i].enabled) {
                        auto cb = m_items[i].on_click;
                        if (m_on_close) m_on_close();
                        if (cb) cb();
                    }
                    break;
                }
                cur_y += h;
            }
        };
    }

    std::vector<MenuItem> m_items;
    float m_min_width{160.0f};
    std::function<void()> m_on_close{nullptr};
    size_t m_hovered_index{static_cast<size_t>(-1)};

    float m_item_height{24.0f};       // 桌面紧凑标准 24px
    float m_corner_radius{3.0f};      // 硬朗微圆角 3px
    float m_font_size{12.5f};
    EdgeInsets m_padding{EdgeInsets(3.0f, 3.0f)};

    Color m_bg_color{Color{0.14f, 0.14f, 0.16f, 0.98f}};
    Color m_border_color{Color{0.28f, 0.28f, 0.32f, 1.0f}};
    Color m_separator_color{Color{0.24f, 0.24f, 0.28f, 1.0f}};
    Color m_hover_bg_color{Color{0.20f, 0.46f, 0.88f, 1.0f}}; // 桌面经典蓝条
    Color m_text_color{Color{0.92f, 0.92f, 0.95f, 1.0f}};
    Color m_disabled_color{Color{0.45f, 0.45f, 0.48f, 1.0f}};
    Color m_shortcut_color{Color{0.62f, 0.62f, 0.66f, 1.0f}};
};

// 快捷弹出上下文菜单函数 (支持视口边缘自动翻转 Edge-Flip)
inline uint64_t show_context_menu(Window* window, Vec2 global_pos,
                                  std::vector<MenuItem> items, float min_width = 160.0f) {
    if (!window) return 0;

    // 预估菜单总高度
    float total_h = 6.0f;
    for (const auto& item : items) {
        total_h += item.is_separator ? 6.0f : 24.0f;
    }

    // 视口防溢出翻转计算
    Vec2 win_sz = window->size();
    Vec2 final_pos = global_pos;
    if (final_pos.x + min_width > win_sz.x && win_sz.x > min_width) {
        final_pos.x = std::max(0.0f, final_pos.x - min_width);
    }
    if (final_pos.y + total_h > win_sz.y && win_sz.y > total_h) {
        final_pos.y = std::max(0.0f, final_pos.y - total_h);
    }

    uint64_t entry_id = 0;
    auto menu_node = std::make_shared<RenderContextMenu>(std::move(items), min_width, [window, &entry_id]() {
        if (entry_id != 0) {
            window->hide_overlay(entry_id);
        }
    });

    OverlayEntry entry;
    entry.content = menu_node;
    entry.global_pos = final_pos;
    entry.size = Vec2{min_width, total_h};
    entry.dismiss_on_outside_click = true;
    entry.is_modal = false;

    entry_id = window->show_overlay(std::move(entry));
    return entry_id;
}

// 声明式上下文菜单配置描述符
struct ContextMenu {
    std::vector<MenuItem> items;
    float min_width{160.0f};
};

} // namespace ktui
