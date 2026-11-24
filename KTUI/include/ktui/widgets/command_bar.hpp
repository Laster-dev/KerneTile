#pragma once

#include "common/fluent_icons.hpp"
#include "menu_flyout.hpp"
#include "widget.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// WinUI 3 CommandBarDefaultLabelPosition
enum class CommandBarDefaultLabelPosition : uint8_t {
    Right = 0,     // 现代紧凑工具栏 (40px 高，图标在左、文字在右)
    Bottom = 1,    // 经典 WinUI 3 工具栏 (56px 高，图标在上、文字在下)
    Collapsed = 2  // 仅图标模式 (40px 高，隐藏文字标签)
};

// CommandBar 命令项描述 (支持普通按钮、ToggleButton 与垂直分隔线)
struct CommandBarItem {
    std::string label;
    std::string svg_path;               // Fluent 24x24 SVG 路径 (d="...")
    std::string shortcut;               // 溢出/二级菜单快捷键提示
    bool is_separator{false};
    bool is_toggle{false};
    bool checked{false};
    bool disabled{false};
    std::function<void()> on_click{nullptr};
    std::function<void(bool)> on_toggle{nullptr};

    static CommandBarItem separator() {
        CommandBarItem it;
        it.is_separator = true;
        return it;
    }
};

// WinUI 3 CommandBar 渲染盒 (支持主命令栏、Toggle 高亮、宽度自适应溢出与 "..." 二级 MenuFlyout)
class RenderCommandBar : public RenderBox {
public:
    RenderCommandBar() noexcept {
        cursor_shape = CursorShape::Arrow;
        setup_behavior();
    }

    ~RenderCommandBar() override = default;

    void set_properties(std::string content_text,
                        std::vector<CommandBarItem> primary_commands,
                        std::vector<CommandBarItem> secondary_commands,
                        CommandBarDefaultLabelPosition label_position,
                        float width) {
        m_content_text = std::move(content_text);
        m_primary = std::move(primary_commands);
        m_secondary = std::move(secondary_commands);
        m_label_position = label_position;
        m_width = width;
        mark_needs_layout();
    }

    [[nodiscard]] size_t visible_primary_count() const noexcept { return m_visible_primary_count; }
    [[nodiscard]] size_t overflowed_primary_count() const noexcept {
        return (m_primary.size() > m_visible_primary_count) ? (m_primary.size() - m_visible_primary_count) : 0;
    }

    void perform_layout() override {
        float bar_h = (m_label_position == CommandBarDefaultLabelPosition::Bottom) ? 56.0f : 40.0f;
        float desired_w = (m_width > 0.0f)
            ? m_width
            : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 480.0f);
        set_size(m_constraints.constrain(Vec2{desired_w, bar_h}));
        rebuild_layout();
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        // 1. WinUI 3 CommandBar 浅层容器底板与 1px 柔和描边
        Color bar_bg = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.032f}
            : Color{1.0f, 1.0f, 1.0f, 0.55f};
        Color bar_bd = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.07f}
            : Color{0.0f, 0.0f, 0.0f, 0.07f};
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, h), bar_bg, 6.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, h), bar_bd, 6.0f, 1.0f);

        Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.94f} : Color{0.0f, 0.0f, 0.0f, 0.90f};
        Color muted_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.62f} : Color{0.0f, 0.0f, 0.0f, 0.58f};
        Color dis_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.34f} : Color{0.0f, 0.0f, 0.0f, 0.34f};
        Color sep_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.10f} : Color{0.0f, 0.0f, 0.0f, 0.10f};
        Color hov_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.065f} : Color{0.0f, 0.0f, 0.0f, 0.042f};
        Color prs_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.04f} : Color{0.0f, 0.0f, 0.0f, 0.028f};
        Color accent = theme.is_dark
            ? Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}
            : Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
        Color accent_fg = theme.is_dark ? Color{0.05f, 0.08f, 0.12f, 1.0f} : Color{1.0f, 1.0f, 1.0f, 1.0f};

        // 2. 左侧可选内容标签 (如 "已选择 3 项")
        if (!m_content_text.empty() && m_content_max_x > 16.0f) {
            ctx.push_clip(Rect::from_xywh(12.0f, 0.0f, std::max(0.0f, m_content_max_x - 16.0f), h), 0.0f);
            Vec2 cs = TextMeasure::measure(m_content_text, 13.0f);
            ctx.draw_text(m_content_text, PointF{12.0f, std::round((h - cs.y) * 0.5f)}, 13.0f, muted_col);
            ctx.pop_clip();
        }

        // 3. 绘制可见主命令与分隔线
        for (const auto& slot : m_slots) {
            if (slot.is_more) {
                bool hov = (m_hovered_slot == slot.slot_idx);
                bool prs = (m_pressed_slot == slot.slot_idx || m_overflow_open);
                if (prs) ctx.draw_rect(slot.rect, prs_bg, 4.0f, 0.0f);
                else if (hov) ctx.draw_rect(slot.rect, hov_bg, 4.0f, 0.0f);

                fluent_icons::draw_centered(ctx, fluent_icons::kMoreHorizontal, slot.rect, 16.0f, text_col);
                continue;
            }

            const auto& item = m_primary[slot.primary_idx];
            if (item.is_separator) {
                float sx = std::round(slot.rect.min_x + slot.rect.width() * 0.5f);
                ctx.draw_rect(Rect::from_xywh(sx, 8.0f, 1.0f, std::max(8.0f, h - 16.0f)), sep_col, 0.0f, 0.0f);
                continue;
            }

            bool hov = (!item.disabled && m_hovered_slot == slot.slot_idx);
            bool prs = (!item.disabled && m_pressed_slot == slot.slot_idx);
            bool checked = (item.is_toggle && item.checked);

            if (checked) {
                Color cbg = accent;
                if (prs) cbg.a *= 0.80f;
                else if (hov) cbg.a *= 0.90f;
                ctx.draw_rect(slot.rect, cbg, 4.0f, 0.0f);
            } else if (prs) {
                ctx.draw_rect(slot.rect, prs_bg, 4.0f, 0.0f);
            } else if (hov) {
                ctx.draw_rect(slot.rect, hov_bg, 4.0f, 0.0f);
            }

            Color fg = item.disabled ? dis_col : (checked ? accent_fg : text_col);

            if (m_label_position == CommandBarDefaultLabelPosition::Bottom) {
                // 上图标 + 下文字 (56px)
                if (!item.svg_path.empty()) {
                    float isz = 16.0f;
                    Rect ir = Rect::from_xywh(
                        std::round(slot.rect.min_x + (slot.rect.width() - isz) * 0.5f),
                        std::round(slot.rect.min_y + 8.0f), isz, isz);
                    SvgEngine::draw_svg_path(ctx, item.svg_path, 24.0f, 24.0f, ir, fg);
                }
                if (!item.label.empty()) {
                    Vec2 ls = TextMeasure::measure(item.label, 11.5f);
                    ctx.draw_text(item.label,
                                  PointF{std::round(slot.rect.min_x + (slot.rect.width() - ls.x) * 0.5f),
                                         std::round(slot.rect.max_y - ls.y - 6.0f)},
                                  11.5f, fg);
                }
            } else if (m_label_position == CommandBarDefaultLabelPosition::Collapsed) {
                // 仅图标
                if (!item.svg_path.empty()) {
                    float isz = 16.0f;
                    Rect ir = Rect::from_xywh(
                        std::round(slot.rect.min_x + (slot.rect.width() - isz) * 0.5f),
                        std::round(slot.rect.min_y + (slot.rect.height() - isz) * 0.5f), isz, isz);
                    SvgEngine::draw_svg_path(ctx, item.svg_path, 24.0f, 24.0f, ir, fg);
                } else if (!item.label.empty()) {
                    Vec2 ls = TextMeasure::measure(item.label, 12.5f);
                    ctx.draw_text(item.label,
                                  PointF{std::round(slot.rect.min_x + (slot.rect.width() - ls.x) * 0.5f),
                                         std::round(slot.rect.min_y + (slot.rect.height() - ls.y) * 0.5f)},
                                  12.5f, fg);
                }
            } else {
                // Right 模式：左图标 + 右文字
                float x = slot.rect.min_x + 10.0f;
                if (!item.svg_path.empty()) {
                    float isz = 15.0f;
                    Rect ir = Rect::from_xywh(std::round(x),
                                              std::round(slot.rect.min_y + (slot.rect.height() - isz) * 0.5f),
                                              isz, isz);
                    SvgEngine::draw_svg_path(ctx, item.svg_path, 24.0f, 24.0f, ir, fg);
                    x += isz + 7.0f;
                }
                if (!item.label.empty()) {
                    Vec2 ls = TextMeasure::measure(item.label, 13.0f);
                    ctx.draw_text(item.label,
                                  PointF{std::round(x), std::round(slot.rect.min_y + (slot.rect.height() - ls.y) * 0.5f)},
                                  13.0f, fg);
                }
            }
        }
    }

private:
    struct LayoutSlot {
        int slot_idx{0};
        size_t primary_idx{0};
        bool is_more{false};
        Rect rect;
    };

    [[nodiscard]] float measure_primary_item(const CommandBarItem& item) const {
        if (item.is_separator) return 11.0f;
        if (m_label_position == CommandBarDefaultLabelPosition::Collapsed) {
            return 36.0f;
        }
        if (m_label_position == CommandBarDefaultLabelPosition::Bottom) {
            float lw = item.label.empty() ? 0.0f : std::ceil(TextMeasure::measure(item.label, 11.5f).x);
            return std::max(54.0f, lw + 18.0f);
        }
        // Right
        float w = 20.0f;
        if (!item.svg_path.empty()) w += 15.0f;
        if (!item.svg_path.empty() && !item.label.empty()) w += 7.0f;
        if (!item.label.empty()) w += std::ceil(TextMeasure::measure(item.label, 13.0f).x);
        return std::max(36.0f, w);
    }

    void rebuild_layout() {
        m_slots.clear();
        const float w = size().x;
        const float h = size().y;
        const float pad_y = 4.0f;
        const float btn_h = std::max(24.0f, h - pad_y * 2.0f);
        const float gap = 2.0f;
        const float more_w = 34.0f;

        float left_min = m_content_text.empty()
            ? 6.0f
            : std::min(w * 0.45f, std::ceil(TextMeasure::measure(m_content_text, 13.0f).x) + 28.0f);

        // 先测算全部主命令总宽，判断是否需要溢出到 "..." 菜单
        std::vector<float> widths(m_primary.size(), 0.0f);
        float total_primary_w = 0.0f;
        for (size_t i = 0; i < m_primary.size(); ++i) {
            widths[i] = measure_primary_item(m_primary[i]);
            total_primary_w += widths[i] + (i > 0 ? gap : 0.0f);
        }

        bool has_secondary = !m_secondary.empty();
        float avail_no_more = std::max(0.0f, w - left_min - 6.0f);
        bool need_more = has_secondary || (total_primary_w > avail_no_more);
        float avail = std::max(0.0f, avail_no_more - (need_more ? (more_w + gap) : 0.0f));

        size_t fit_count = 0;
        float used_w = 0.0f;
        for (size_t i = 0; i < m_primary.size(); ++i) {
            float add = widths[i] + (fit_count > 0 ? gap : 0.0f);
            if (used_w + add <= avail) {
                used_w += add;
                ++fit_count;
            } else {
                break;
            }
        }
        // 不让分隔线孤零零停在末尾
        while (fit_count > 0 && m_primary[fit_count - 1].is_separator) {
            used_w -= widths[fit_count - 1] + (fit_count > 1 ? gap : 0.0f);
            --fit_count;
        }
        m_visible_primary_count = fit_count;

        float right_block_w = used_w + (need_more ? ((fit_count > 0 ? gap : 0.0f) + more_w) : 0.0f);
        float cur_x = std::max(left_min, w - 6.0f - right_block_w);
        m_content_max_x = cur_x;

        int slot_idx = 0;
        for (size_t i = 0; i < fit_count; ++i) {
            m_slots.push_back(LayoutSlot{slot_idx++, i, false, Rect::from_xywh(cur_x, pad_y, widths[i], btn_h)});
            cur_x += widths[i] + gap;
        }
        if (need_more) {
            m_slots.push_back(LayoutSlot{slot_idx++, 0, true, Rect::from_xywh(cur_x, pad_y, more_w, btn_h)});
        }
    }

    void open_overflow_menu(const Rect& more_rect) {
        Window* win = Window::active_window();
        if (!win) return;

        std::vector<MenuFlyoutItem> items;
        // 1. 溢出的主命令
        for (size_t i = m_visible_primary_count; i < m_primary.size(); ++i) {
            const auto& p = m_primary[i];
            if (p.is_separator) {
                items.push_back(MenuFlyoutItem::separator());
            } else {
                MenuFlyoutItem mi;
                mi.label = p.label;
                mi.shortcut = p.shortcut;
                mi.svg_path = p.svg_path;
                mi.disabled = p.disabled;
                mi.is_checkable = p.is_toggle;
                mi.checked = p.checked;
                size_t idx = i;
                mi.on_click = [this, idx]() {
                    if (idx < m_primary.size()) trigger_primary_item(idx);
                };
                items.push_back(std::move(mi));
            }
        }
        if (!items.empty() && !m_secondary.empty()) {
            items.push_back(MenuFlyoutItem::separator());
        }
        // 2. 二级命令
        for (const auto& s : m_secondary) {
            if (s.is_separator) {
                items.push_back(MenuFlyoutItem::separator());
            } else {
                MenuFlyoutItem mi;
                mi.label = s.label;
                mi.shortcut = s.shortcut;
                mi.svg_path = s.svg_path;
                mi.disabled = s.disabled;
                mi.is_checkable = s.is_toggle;
                mi.checked = s.checked;
                mi.on_click = s.on_click;
                items.push_back(std::move(mi));
            }
        }
        if (items.empty()) return;

        Vec2 g = global_offset();
        g.y += win->titlebar_height();
        Vec2 anchor{g.x + more_rect.max_x - 190.0f, g.y + size().y + 4.0f};
        m_overflow_open = true;
        mark_needs_paint();
        MenuFlyout::show_at(items, anchor, [this]() {
            m_overflow_open = false;
            mark_needs_paint();
        });
    }

    void trigger_primary_item(size_t idx) {
        if (idx >= m_primary.size()) return;
        auto& item = m_primary[idx];
        if (item.disabled || item.is_separator) return;
        if (item.is_toggle) {
            item.checked = !item.checked;
            mark_needs_paint();
            if (item.on_toggle) item.on_toggle(item.checked);
        }
        if (item.on_click) item.on_click();
    }

    int hit_test_slot(PointF p) const noexcept {
        for (const auto& s : m_slots) {
            if (!s.is_more && m_primary[s.primary_idx].is_separator) continue;
            if (s.rect.contains(p)) return s.slot_idx;
        }
        return -1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int hs = hit_test_slot(e.local_position);
            if (hs != m_hovered_slot) {
                m_hovered_slot = hs;
                cursor_shape = (hs >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_slot != -1 || m_pressed_slot != -1) {
                m_hovered_slot = -1;
                m_pressed_slot = -1;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int hs = hit_test_slot(e.local_position);
            if (hs >= 0) {
                m_pressed_slot = hs;
                mark_needs_paint();
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            int ps = m_pressed_slot;
            m_pressed_slot = -1;
            if (ps >= 0 && hit_test_slot(e.local_position) == ps && ps < static_cast<int>(m_slots.size())) {
                const auto& slot = m_slots[static_cast<size_t>(ps)];
                if (slot.is_more) {
                    open_overflow_menu(slot.rect);
                } else {
                    trigger_primary_item(slot.primary_idx);
                }
                e.stop_propagation();
            }
            mark_needs_paint();
        };
    }

    std::string m_content_text;
    std::vector<CommandBarItem> m_primary;
    std::vector<CommandBarItem> m_secondary;
    CommandBarDefaultLabelPosition m_label_position{CommandBarDefaultLabelPosition::Right};
    float m_width{0.0f};

    std::vector<LayoutSlot> m_slots;
    size_t m_visible_primary_count{0};
    float m_content_max_x{0.0f};
    int m_hovered_slot{-1};
    int m_pressed_slot{-1};
    bool m_overflow_open{false};
};

// C++20 指定初始化器声明式 WinUI 3 CommandBar
struct CommandBar {
    std::string content{""};
    std::vector<CommandBarItem> primary_commands;
    std::vector<CommandBarItem> secondary_commands;
    CommandBarDefaultLabelPosition default_label_position{CommandBarDefaultLabelPosition::Right};
    float width{0.0f};

    operator WidgetPtr() const;
};

class CommandBarWidget : public Widget {
public:
    explicit CommandBarWidget(const CommandBar& desc) : m_desc(desc) {}
    ~CommandBarWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderCommandBar>();
        node->set_properties(m_desc.content, m_desc.primary_commands, m_desc.secondary_commands,
                             m_desc.default_label_position, m_desc.width);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto cb = std::dynamic_pointer_cast<RenderCommandBar>(node)) {
            cb->set_properties(m_desc.content, m_desc.primary_commands, m_desc.secondary_commands,
                               m_desc.default_label_position, m_desc.width);
        }
    }

private:
    CommandBar m_desc;
};

inline CommandBar::operator WidgetPtr() const {
    return std::make_shared<CommandBarWidget>(*this);
}

} // namespace ktui
