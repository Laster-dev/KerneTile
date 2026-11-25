#pragma once

#include "widget.hpp"
#include "menu_flyout.hpp"
#include <ktui/app/window.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// 分段式状态栏槽位描述 (Slot)
struct StatusBarSlot {
    std::string id;
    std::string text;
    bool align_right{false};
    Color indicator_color{0.0f, 0.0f, 0.0f, 0.0f}; // a>0 时左侧绘制 6px 状态灯圆点
    std::vector<std::string> options;              // 非空时点击弹出 MenuFlyout 切换选项
    Fn<void(const std::string&)> on_selected{nullptr};
    Fn<void()> on_click{nullptr};
};

// 结构化“分段式状态栏” (Segmented Status Bar，经典 24px 极客工程底部信息槽)
// 左侧常驻统计/偏移信息，右侧放编码/换行符/刷新率等可点击切换槽位（直接复用 MenuFlyout）
class RenderStatusBar : public RenderBox {
public:
    RenderStatusBar(std::vector<StatusBarSlot> slots = {})
        : m_slots(std::move(slots)) {
        setup_behavior();
    }

    void set_slots(std::vector<StatusBarSlot> slots) {
        m_slots = std::move(slots);
        mark_needs_layout();
        mark_needs_paint();
    }

    void set_slot_text(const std::string& id, const std::string& text) {
        for (auto& s : m_slots) {
            if (s.id == id && s.text != text) {
                s.text = text;
                mark_needs_layout();
                mark_needs_paint();
                return;
            }
        }
    }

    [[nodiscard]] std::string slot_text(const std::string& id) const {
        for (const auto& s : m_slots) {
            if (s.id == id) return s.text;
        }
        return "";
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(m_constraints.min_width, 480.0f);
        float h = m_constraints.has_bounded_height() ? std::min(m_constraints.max_height, 26.0f) : 26.0f;
        m_size = m_constraints.constrain(Vec2{w, std::max(22.0f, h)});

        m_hits.clear();
        float lx = 4.0f;
        float rx = m_size.x - 4.0f;

        for (size_t i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].align_right) continue;
            float sw = measure_slot_w(m_slots[i]);
            m_hits.push_back(SlotHit{i, Rect::from_xywh(lx, 1.0f, sw, m_size.y - 2.0f)});
            lx += sw + 1.0f;
        }
        for (size_t i = m_slots.size(); i > 0; --i) {
            size_t idx = i - 1;
            if (!m_slots[idx].align_right) continue;
            float sw = measure_slot_w(m_slots[idx]);
            rx -= sw;
            m_hits.push_back(SlotHit{idx, Rect::from_xywh(rx, 1.0f, sw, m_size.y - 2.0f)});
            rx -= 1.0f;
        }
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        bool clickable = false;
        for (const auto& h : m_hits) {
            if (h.rect.contains(position)) {
                const auto& s = m_slots[h.index];
                clickable = (!s.options.empty() || s.on_click != nullptr);
                break;
            }
        }
        cursor_shape = clickable ? CursorShape::PointingHand : CursorShape::Arrow;
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const Color bg = is_dark ? Color{1, 1, 1, 0.04f} : Color{0, 0, 0, 0.035f};
        const Color bd = is_dark ? Color{1, 1, 1, 0.09f} : Color{0, 0, 0, 0.10f};
        const Color sep = is_dark ? Color{1, 1, 1, 0.10f} : Color{0, 0, 0, 0.10f};
        const Color hov = is_dark ? Color{1, 1, 1, 0.09f} : Color{0, 0, 0, 0.07f};

        context.draw_rect(bounds(), bg, 4.0f, 0.0f);
        context.push_clip(bounds(), 4.0f);

        for (size_t hi = 0; hi < m_hits.size(); ++hi) {
            const auto& h = m_hits[hi];
            const auto& s = m_slots[h.index];
            const bool clickable = (!s.options.empty() || s.on_click != nullptr);
            if (clickable && m_hovered_slot == static_cast<int>(h.index)) {
                context.draw_rect(h.rect, hov, 3.0f, 0.0f);
            }

            float tx = h.rect.min_x + 8.0f;
            if (s.indicator_color.a > 0.01f) {
                context.draw_rect(Rect::from_xywh(tx, std::round((m_size.y - 6.0f) * 0.5f), 6.0f, 6.0f),
                                  s.indicator_color, 3.0f, 0.0f);
                tx += 10.0f;
            }
            Vec2 ts = TextMeasure::measure(s.text, 11.0f, clickable ? FontWeight::Medium : FontWeight::Regular);
            context.draw_text(s.text, PointF{std::round(tx), std::round((m_size.y - ts.y) * 0.5f)},
                              11.0f, clickable ? t.colors.text_primary : t.colors.text_secondary,
                              clickable ? FontWeight::Medium : FontWeight::Regular);

            // 槽位竖向极细分隔线
            if (!s.align_right) {
                context.draw_rect(Rect::from_xywh(h.rect.max_x, 5.0f, 1.0f, m_size.y - 10.0f), sep, 0.0f, 0.0f);
            } else {
                context.draw_rect(Rect::from_xywh(h.rect.min_x - 1.0f, 5.0f, 1.0f, m_size.y - 10.0f), sep, 0.0f, 0.0f);
            }
        }

        context.pop_clip();
        context.draw_rect(bounds(), bd, 4.0f, 1.0f);
    }

private:
    struct SlotHit { size_t index; Rect rect; };

    [[nodiscard]] static float measure_slot_w(const StatusBarSlot& s) {
        Vec2 ts = TextMeasure::measure(s.text, 11.0f, (!s.options.empty() || s.on_click) ? FontWeight::Medium : FontWeight::Regular);
        return ts.x + 16.0f + (s.indicator_color.a > 0.01f ? 10.0f : 0.0f);
    }

    void setup_behavior() {
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_slot != -1) { m_hovered_slot = -1; mark_needs_paint(); }
        };
        on_pointer_move = [this](PointerEvent& ev) {
            int hs = -1;
            for (const auto& h : m_hits) {
                if (h.rect.contains(ev.local_position)) { hs = static_cast<int>(h.index); break; }
            }
            if (hs != m_hovered_slot) { m_hovered_slot = hs; mark_needs_paint(); }
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            for (const auto& h : m_hits) {
                if (!h.rect.contains(ev.local_position)) continue;
                auto& s = m_slots[h.index];
                if (!s.options.empty()) {
                    Window* win = Window::active_window();
                    if (!win) return;
                    std::vector<MenuFlyoutItem> items;
                    size_t idx = h.index;
                    for (const auto& opt : s.options) {
                        std::string chosen = opt;
                        items.push_back(MenuFlyoutItem{
                            .label = chosen,
                            .is_checkable = true,
                            .checked = (chosen == s.text),
                            .on_click = [this, idx, chosen]() {
                                if (idx < m_slots.size()) {
                                    m_slots[idx].text = chosen;
                                    mark_needs_layout();
                                    mark_needs_paint();
                                    if (m_slots[idx].on_selected) m_slots[idx].on_selected(chosen);
                                }
                            }
                        });
                    }
                    Vec2 g = global_offset();
                    MenuFlyout::show_at(std::move(items), Vec2{g.x + h.rect.min_x, g.y + win->titlebar_height() - 8.0f});
                    ev.stop_propagation();
                    return;
                }
                if (s.on_click) {
                    s.on_click();
                    ev.stop_propagation();
                    return;
                }
            }
        };
    }

    std::vector<StatusBarSlot> m_slots;
    std::vector<SlotHit> m_hits;
    int m_hovered_slot{-1};
};

struct StatusBar : BindableMixin<StatusBar, RenderStatusBar> {
    std::vector<StatusBarSlot> slots{};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class StatusBarWidget : public Widget {
public:
    explicit StatusBarWidget(const StatusBar& desc) : m_desc(desc) {}
    explicit StatusBarWidget(StatusBar&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderStatusBar>(m_desc.slots);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderStatusBar>(node)) {
            n->set_slots(m_desc.slots);
        }
    }
private:
    StatusBar m_desc;
};

inline StatusBar::operator WidgetPtr() const& { return make_widget<StatusBarWidget>(*this); }
inline StatusBar::operator WidgetPtr() && { return make_widget<StatusBarWidget>(std::move(*this)); }

} // namespace ktui
