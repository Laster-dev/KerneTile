#pragma once

#include "auto_suggest_box_data.hpp"
#include "auto_suggest_box_style.hpp"
#include "../common/fluent_chrome.hpp"
#include "../common/fluent_icons.hpp"
#include <ktui/app/window.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/popup_handle.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <cmath>

namespace ktui {


// WinUI 3 AutoSuggestBox 悬浮候选下拉列表
class RenderAutoSuggestPopup : public RenderBox {
public:
    RenderAutoSuggestPopup(std::vector<std::string> suggestions,
                           float width,
                           Fn<void(const std::string&)> on_pick)
        : m_suggestions(std::move(suggestions)),
          m_width(width),
          m_on_pick(std::move(on_pick)) {
        cursor_shape = CursorShape::Arrow;
        setup_behavior();
    }

    ~RenderAutoSuggestPopup() override = default;

    void set_suggestions(std::vector<std::string> suggestions) {
        m_suggestions = std::move(suggestions);
        if (m_highlighted >= static_cast<int>(m_suggestions.size())) {
            m_highlighted = m_suggestions.empty() ? -1 : static_cast<int>(m_suggestions.size()) - 1;
        }
        m_hovered = -1;
        m_scroll_offset = 0.0f;
        set_size(compute_ideal_size());
        mark_needs_paint();
    }

    void set_highlighted(int idx) {
        if (m_highlighted != idx) {
            m_highlighted = idx;
            ensure_highlighted_visible();
            mark_needs_paint();
        }
    }

    [[nodiscard]] int highlighted() const noexcept { return m_highlighted; }

    [[nodiscard]] Vec2 compute_ideal_size() const {
        const size_t count = std::max<size_t>(1, m_suggestions.size());
        float content_h = static_cast<float>(count) * kItemH + 8.0f;
        return Vec2{std::max(160.0f, m_width), std::min(content_h, kMaxPopupH)};
    }

    void perform_layout() override {
        set_size(compute_ideal_size());
    }

    void paint(PaintContext& ctx) override {
        auto st = m_surface.tick();
        if (st.animating) mark_needs_paint();

        const Theme& theme = ThemeContext::current();
        const auto resolved_style = AutoSuggestBoxStyleResolver::resolve(theme);
        float anim_h = std::max(24.0f, size().y * (0.78f + 0.22f * st.ease));
        float alpha = st.alpha;

        FluentPopupSurface::paint_panel(ctx, theme, Rect::from_xywh(0.0f, 0.0f, size().x, anim_h), alpha);

        Color hov_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.068f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.045f * alpha};
        Color hi_bg  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.095f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.065f * alpha};
        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.95f * alpha}  : Color{0.0f, 0.0f, 0.0f, 0.90f * alpha};
        Color mute_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.48f * alpha}  : Color{0.0f, 0.0f, 0.0f, 0.48f * alpha};

        if (m_suggestions.empty()) {
            const char* empty_lbl = FrameworkStrings::current().no_matching_results;
            Vec2 ts = TextMeasure::measure(empty_lbl, 13.0f);
            ctx.draw_text(empty_lbl, PointF{14.0f, std::round((anim_h - ts.y) * 0.5f)}, 13.0f, mute_c);
            return;
        }

        ctx.push_clip(Rect::from_xywh(1.0f, 4.0f, size().x - 2.0f, std::max(0.0f, anim_h - 8.0f)));
        float y = 4.0f - m_scroll_offset;
        for (size_t i = 0; i < m_suggestions.size(); ++i) {
            Rect row = Rect::from_xywh(4.0f, y, size().x - 8.0f, kItemH);
            if (row.max_y >= 0.0f && row.min_y <= anim_h) {
                if (static_cast<int>(i) == m_highlighted) {
                    ctx.draw_rect(row, hi_bg, 4.0f, 0.0f);
                    Color pill = resolved_style.accent;
                    pill.a *= alpha;
                    ctx.draw_rect(Rect::from_xywh(row.min_x + 2.0f, row.min_y + 8.0f, 3.0f, kItemH - 16.0f), pill, 1.5f, 0.0f);
                } else if (static_cast<int>(i) == m_hovered) {
                    ctx.draw_rect(row, hov_bg, 4.0f, 0.0f);
                }
                Vec2 ts = TextMeasure::measure(m_suggestions[i], 13.0f);
                ctx.draw_text(m_suggestions[i],
                              PointF{row.min_x + 12.0f, std::round(row.min_y + (kItemH - ts.y) * 0.5f)},
                              13.0f, text_c);
            }
            y += kItemH;
        }
        ctx.pop_clip();
    }

private:
    static constexpr float kItemH = 34.0f;
    static constexpr float kMaxPopupH = 212.0f;

    void ensure_highlighted_visible() {
        if (m_highlighted < 0) return;
        float item_top = static_cast<float>(m_highlighted) * kItemH;
        float item_bot = item_top + kItemH;
        float view_h = std::max(kItemH, size().y - 8.0f);
        if (item_top < m_scroll_offset) m_scroll_offset = item_top;
        else if (item_bot > m_scroll_offset + view_h) m_scroll_offset = item_bot - view_h;
        float max_scroll = std::max(0.0f, static_cast<float>(m_suggestions.size()) * kItemH - view_h);
        m_scroll_offset = std::clamp(m_scroll_offset, 0.0f, max_scroll);
    }

    int hit_row(Vec2 local) const noexcept {
        if (local.x < 0.0f || local.x > size().x || local.y < 4.0f || local.y > size().y - 4.0f) return -1;
        int idx = static_cast<int>(std::floor((local.y - 4.0f + m_scroll_offset) / kItemH));
        return (idx >= 0 && idx < static_cast<int>(m_suggestions.size())) ? idx : -1;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            if (e.type == PlatformEventType::PointerScroll) {
                float view_h = std::max(kItemH, size().y - 8.0f);
                float max_scroll = std::max(0.0f, static_cast<float>(m_suggestions.size()) * kItemH - view_h);
                if (max_scroll > 0.0f) {
                    m_scroll_offset = std::clamp(m_scroll_offset - e.delta.y * 32.0f, 0.0f, max_scroll);
                    mark_needs_paint();
                    e.stop_propagation();
                }
                return;
            }
            if (e.is_touch() && m_touch_down) {
                float dy_total = std::abs(e.local_position.y - m_touch_start_y);
                if (!m_touch_scrolling && dy_total > touch::kTouchSlopPx) {
                    m_touch_scrolling = true;
                }
                if (m_touch_scrolling) {
                    float view_h = std::max(kItemH, size().y - 8.0f);
                    float max_scroll = std::max(0.0f, static_cast<float>(m_suggestions.size()) * kItemH - view_h);
                    float dy = e.local_position.y - m_touch_last_y;
                    m_touch_last_y = e.local_position.y;
                    if (max_scroll > 0.0f) {
                        m_scroll_offset = std::clamp(m_scroll_offset - dy, 0.0f, max_scroll);
                        mark_needs_paint();
                    }
                    e.stop_propagation();
                }
                return;
            }
            int idx = hit_row(e.local_position);
            cursor_shape = (idx >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (idx != m_hovered) { m_hovered = idx; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered != -1) { m_hovered = -1; mark_needs_paint(); }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            if (e.is_touch()) {
                m_touch_down = true;
                m_touch_scrolling = false;
                m_touch_start_y = e.local_position.y;
                m_touch_last_y = e.local_position.y;
                m_touch_down_row = hit_row(e.local_position);
                e.stop_propagation();
                return;
            }
            int idx = hit_row(e.local_position);
            if (idx >= 0 && idx < static_cast<int>(m_suggestions.size())) {
                if (m_on_pick) m_on_pick(m_suggestions[static_cast<size_t>(idx)]);
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (e.is_touch() && m_touch_down) {
                bool was_scroll = m_touch_scrolling;
                int row = hit_row(e.local_position);
                m_touch_down = false;
                m_touch_scrolling = false;
                if (!was_scroll && row >= 0 && row == m_touch_down_row &&
                    row < static_cast<int>(m_suggestions.size())) {
                    if (m_on_pick) m_on_pick(m_suggestions[static_cast<size_t>(row)]);
                }
                e.stop_propagation();
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_touch_down = false;
            m_touch_scrolling = false;
            m_touch_down_row = -1;
            if (m_hovered != -1) { m_hovered = -1; mark_needs_paint(); }
        };
    }

    std::vector<std::string> m_suggestions;
    float m_width{260.0f};
    Fn<void(const std::string&)> m_on_pick;
    int m_hovered{-1};
    int m_highlighted{-1};
    float m_scroll_offset{0.0f};
    bool m_touch_down{false};
    bool m_touch_scrolling{false};
    float m_touch_start_y{0.0f};
    float m_touch_last_y{0.0f};
    int m_touch_down_row{-1};
    FluentPopupSurface m_surface;
};

// WinUI 3 AutoSuggestBox 自动补全搜索框渲染盒
// 直接组合复用真实 RenderTextBox 子节点，完整继承选区、撤销/重做、IME、水平滚动与 WinUI 3 右键菜单
class RenderAutoSuggestBox : public RenderBox {
public:
    RenderAutoSuggestBox() noexcept {
        m_input = make_render_node<RenderTextBox>();
        append_child(m_input.get());
        setup_input_bridge();
        sync_input_properties();
    }

    ~RenderAutoSuggestBox() override {
        close_popup();
        if (m_input && m_input->parent() == this) {
            remove_child(m_input.get());
        }
    }

    void set_properties(const std::string& text,
                        const std::string& placeholder,
                        const std::string& header,
                        std::vector<std::string> items,
                        float width,
                        bool disabled,
                        Fn<void(const std::string&)> on_text_changed,
                        Fn<void(const std::string&)> on_suggestion_chosen,
                        Fn<void(const std::string&)> on_query_submitted) {
        bool text_changed = (m_text != text);
        if (text_changed) {
            m_text = text;
            rebuild_filtered();
        }
        if (m_placeholder != placeholder) m_placeholder = placeholder;
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        if (m_items != items) {
            m_items = std::move(items);
            rebuild_filtered();
            if (m_popup) m_popup->set_suggestions(m_filtered);
        }
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            if (disabled) close_popup();
        }
        m_on_text_changed = std::move(on_text_changed);
        m_on_suggestion_chosen = std::move(on_suggestion_chosen);
        m_on_query_submitted = std::move(on_query_submitted);

        sync_input_properties();
        mark_needs_paint();
    }

    void set_text_value(const std::string& v) {
        if (m_text != v) {
            m_text = v;
            m_input->set_text(m_text);
            update_input_padding();
            rebuild_filtered();
            if (m_popup) m_popup->set_suggestions(m_filtered);
            mark_needs_paint();
        }
    }

    void set_items_value(const std::vector<std::string>& items) {
        if (m_items != items) {
            m_items = items;
            rebuild_filtered();
            if (m_popup) m_popup->set_suggestions(m_filtered);
        }
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] const std::vector<std::string>& filtered_items() const noexcept { return m_filtered; }
    [[nodiscard]] bool is_popup_open() const noexcept { return m_popup_handle.is_open(); }
    [[nodiscard]] const std::shared_ptr<RenderTextBox>& input_box() const noexcept { return m_input; }

    void select_suggestion(size_t idx) {
        if (idx < m_filtered.size()) apply_chosen_suggestion(m_filtered[idx]);
    }

    void perform_layout() override {
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        float w = (m_width > 0.0f) ? m_width : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 280.0f);
        set_size(m_constraints.constrain(Vec2{w, header_h + 32.0f}));

        if (m_input) {
            float box_h = std::max(32.0f, size().y - header_h);
            m_input->layout(BoxConstraints::tight(Vec2{size().x, box_h}), false);
            m_input->set_offset(Vec2{0.0f, header_h});
        }
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();

        if (!m_header.empty()) {
            Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f}
                                           : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.92f};
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, text_col);
        }

        if (m_input) {
            PaintContext::OffsetScope scope(ctx, m_input->offset());
            m_input->paint(ctx);
        }

        // 在复用 TextBox 的右侧预留内边距区叠加 WinUI 3 清除 (×) 与搜索 (🔍) 图标按钮
        Rect search_r = search_btn_rect();
        Rect clear_r = clear_btn_rect();
        bool show_clear = (!m_text.empty() && !m_disabled);

        Color btn_hov = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.055f};
        Color icon_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.30f : 0.78f}
                                       : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.30f : 0.72f};

        if (show_clear) {
            if (m_hover_clear) ctx.draw_rect(clear_r, btn_hov, 4.0f, 0.0f);
            fluent_icons::draw_centered(ctx, fluent_icons::kClose, clear_r, 12.0f, icon_col);
        }
        if (m_hover_search && !m_disabled) ctx.draw_rect(search_r, btn_hov, 4.0f, 0.0f);
        fluent_icons::draw_centered(ctx, fluent_icons::kSearch, search_r, 14.0f, icon_col);
    }

private:
    Rect box_rect() const noexcept {
        float top = m_header.empty() ? 0.0f : 22.0f;
        return Rect::from_xywh(0.0f, top, size().x, std::max(32.0f, size().y - top));
    }

    Rect search_btn_rect() const noexcept {
        Rect b = box_rect();
        return Rect::from_xywh(b.max_x - 30.0f, b.min_y + 3.0f, 26.0f, b.height() - 6.0f);
    }

    Rect clear_btn_rect() const noexcept {
        Rect s = search_btn_rect();
        return Rect::from_xywh(s.min_x - 26.0f, s.min_y, 24.0f, s.height());
    }

    Rect input_local_search_rect() const noexcept {
        Vec2 s = m_input ? m_input->size() : Vec2{size().x, 32.0f};
        return Rect::from_xywh(s.x - 30.0f, 3.0f, 26.0f, std::max(0.0f, s.y - 6.0f));
    }

    Rect input_local_clear_rect() const noexcept {
        Rect sr = input_local_search_rect();
        return Rect::from_xywh(sr.min_x - 26.0f, sr.min_y, 24.0f, sr.height());
    }

    void update_input_padding() {
        if (!m_input) return;
        float right_pad = (!m_text.empty() && !m_disabled) ? 58.0f : 32.0f;
        m_input->apply_custom_overrides(
            std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            32.0f, 4.0f, EdgeInsets{6.0f, right_pad, 6.0f, 10.0f}, 13.5f);
    }

    void sync_input_properties() {
        if (!m_input) return;
        m_input->set_properties(
            m_text, m_placeholder,
            32.0f, 4.0f,
            EdgeInsets{6.0f, (!m_text.empty() && !m_disabled) ? 58.0f : 32.0f, 6.0f, 10.0f},
            Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1},
            Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1}, 13.5f,
            [this](const std::string& updated) {
                m_text = updated;
                update_input_padding();
                notify_text_edited();
            },
            [this](const std::string& submitted) {
                m_text = submitted;
                if (m_popup && m_popup->highlighted() >= 0 &&
                    m_popup->highlighted() < static_cast<int>(m_filtered.size())) {
                    apply_chosen_suggestion(m_filtered[static_cast<size_t>(m_popup->highlighted())]);
                } else {
                    close_popup();
                    if (m_on_query_submitted) m_on_query_submitted(m_text);
                }
            });
        m_input->set_disabled(m_disabled);
    }

    static std::string to_lower_ascii(const std::string& s) {
        std::string out = s;
        for (char& c : out) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return out;
    }

    void rebuild_filtered() {
        m_filtered.clear();
        std::string q = to_lower_ascii(m_text);
        for (const auto& item : m_items) {
            if (q.empty() || to_lower_ascii(item).find(q) != std::string::npos) {
                m_filtered.push_back(item);
            }
        }
    }

    void open_or_update_popup() {
        Window* win = Window::active_window();
        if (!win || m_disabled) return;
        rebuild_filtered();

        if (m_popup_handle.is_open() && m_popup) {
            m_popup->set_suggestions(m_filtered);
            return;
        }

        auto token = std::make_shared<bool>(true);
        auto popup = make_render_node<RenderAutoSuggestPopup>(
            m_filtered, size().x,
            [this, token](const std::string& chosen) {
                if (*token) apply_chosen_suggestion(chosen);
            });

        Vec2 sz = popup->compute_ideal_size();
        OverlayEntry entry;
        entry.content = popup;
        entry.global_pos = FluentPopupSurface::compute_anchored_pos(*win, global_offset(), box_rect(), sz);
        entry.size = sz;
        entry.dismiss_on_outside_click = true;
        entry.is_modal = false;

        m_popup = popup;
        m_popup_handle.open(*win, std::move(entry), [this, token]() {
            *token = false;
            m_popup.reset();
        });
    }

    void close_popup() {
        m_popup_handle.close();
        m_popup.reset();
    }

    void apply_chosen_suggestion(const std::string& chosen) {
        m_text = chosen;
        if (m_input) {
            m_input->set_text(m_text);
            m_input->set_cursor_index(m_text.size());
        }
        update_input_padding();
        rebuild_filtered();
        close_popup();
        mark_needs_paint();
        if (m_on_text_changed) m_on_text_changed(m_text);
        if (m_on_suggestion_chosen) m_on_suggestion_chosen(m_text);
        if (m_on_query_submitted) m_on_query_submitted(m_text);
    }

    void notify_text_edited() {
        rebuild_filtered();
        if (m_input && m_input->state().is_focused) open_or_update_popup();
        mark_needs_paint();
        if (m_on_text_changed) m_on_text_changed(m_text);
    }

    void setup_input_bridge() {
        auto orig_move = m_input->on_pointer_move;
        auto orig_exit = m_input->on_pointer_exit;
        auto orig_down = m_input->on_pointer_down;
        auto orig_cancel = m_input->on_pointer_cancel;
        auto orig_focus = m_input->on_focus;
        auto orig_key = m_input->on_key;

        m_input->on_pointer_move = [this, orig_move](PointerEvent& e) {
            if (!m_disabled && !e.is_touch()) {
                bool hc = !m_text.empty() && input_local_clear_rect().contains(e.local_position);
                bool hs = input_local_search_rect().contains(e.local_position);
                m_input->cursor_shape = (hc || hs) ? CursorShape::PointingHand : CursorShape::IBeam;
                if (hc != m_hover_clear || hs != m_hover_search) {
                    m_hover_clear = hc;
                    m_hover_search = hs;
                    mark_needs_paint();
                }
            }
            if (orig_move) orig_move(e);
        };

        m_input->on_pointer_exit = [this, orig_exit](PointerEvent& e) {
            if (m_hover_clear || m_hover_search) {
                m_hover_clear = false;
                m_hover_search = false;
                mark_needs_paint();
            }
            if (orig_exit) orig_exit(e);
        };

        m_input->on_pointer_cancel = [this, orig_cancel](PointerEvent& e) {
            if (m_hover_clear || m_hover_search) {
                m_hover_clear = false;
                m_hover_search = false;
                mark_needs_paint();
            }
            if (orig_cancel) orig_cancel(e);
        };

        m_input->on_pointer_down = [this, orig_down](PointerEvent& e) {
            if (m_disabled) return;
            if (e.button == PointerButton::Primary) {
                Rect clear_hit = e.is_touch()
                    ? touch::expand_hit_rect(input_local_clear_rect(), 36.0f)
                    : input_local_clear_rect();
                Rect search_hit = e.is_touch()
                    ? touch::expand_hit_rect(input_local_search_rect(), 36.0f)
                    : input_local_search_rect();
                if (!m_text.empty() && clear_hit.contains(e.local_position)) {
                    m_text.clear();
                    m_input->set_text("");
                    m_input->set_cursor_index(0);
                    update_input_padding();
                    notify_text_edited();
                    e.stop_propagation();
                    return;
                }
                if (search_hit.contains(e.local_position)) {
                    close_popup();
                    if (m_on_query_submitted) m_on_query_submitted(m_text);
                    e.stop_propagation();
                    return;
                }
                if (orig_down) orig_down(e);
                open_or_update_popup();
                return;
            }
            // 右键直接透传给内置 RenderTextBox 弹出 WinUI 3 标准文本上下文菜单
            if (orig_down) orig_down(e);
        };

        m_input->on_focus = [this, orig_focus](FocusEvent& e) {
            if (orig_focus) orig_focus(e);
            if (!m_disabled && e.gained_focus) {
                open_or_update_popup();
            }
            mark_needs_paint();
        };

        m_input->on_key = [this, orig_key](KeyEvent& e) {
            if (m_disabled) return;
            if (e.type == PlatformEventType::KeyDown) {
                if (e.key_code == 40) { // Down
                    if (!m_popup) open_or_update_popup();
                    if (m_popup && !m_filtered.empty()) {
                        m_popup->set_highlighted((m_popup->highlighted() + 1) % static_cast<int>(m_filtered.size()));
                    }
                    e.stop_propagation();
                    return;
                }
                if (e.key_code == 38) { // Up
                    if (m_popup && !m_filtered.empty()) {
                        int n = static_cast<int>(m_filtered.size());
                        m_popup->set_highlighted((m_popup->highlighted() <= 0) ? (n - 1) : (m_popup->highlighted() - 1));
                    }
                    e.stop_propagation();
                    return;
                }
                if (e.key_code == 27) { // Escape
                    close_popup();
                    e.stop_propagation();
                    return;
                }
            }
            if (orig_key) orig_key(e);
        };
    }

    std::shared_ptr<RenderTextBox> m_input;
    std::string m_text;
    std::string m_placeholder{FrameworkStrings::current().search_placeholder};
    std::string m_header;
    std::vector<std::string> m_items;
    std::vector<std::string> m_filtered;
    float m_width{280.0f};
    bool m_disabled{false};
    bool m_hover_clear{false};
    bool m_hover_search{false};

    PopupHandle m_popup_handle;
    std::shared_ptr<RenderAutoSuggestPopup> m_popup;

    Fn<void(const std::string&)> m_on_text_changed;
    Fn<void(const std::string&)> m_on_suggestion_chosen;
    Fn<void(const std::string&)> m_on_query_submitted;

public:
    void wrap_on_text_changed(Fn<void(const std::string&)> extra) {
        auto prev = std::move(m_on_text_changed);
        m_on_text_changed = [prev = std::move(prev), extra = std::move(extra)](const std::string& v) {
            if (extra) extra(v);
            if (prev) prev(v);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 AutoSuggestBox
struct AutoSuggestBox : BindableMixin<AutoSuggestBox, RenderAutoSuggestBox> {
    std::string text{std::string{}};
    std::string placeholder{FrameworkStrings::current().search_placeholder};
    std::string header;
    std::vector<std::string> items{std::vector<std::string>{}};
    float width{280.0f};
    bool disabled{false};
    bool smooth_caret{true};
    Fn<void(const std::string&)> on_text_changed{nullptr};
    Fn<void(const std::string&)> on_suggestion_chosen{nullptr};
    Fn<void(const std::string&)> on_query_submitted{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderAutoSuggestBox>& node, Property<std::string>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_text_value(prop.get());
    std::weak_ptr<RenderAutoSuggestBox> weak = node;
    bg.subscribe(prop, [weak, guard](const std::string& v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_text_value(v);
        }
    });
    Property<std::string>* p = &prop;
    node->wrap_on_text_changed([p, guard](const std::string& v) {
        *guard = true;
        p->set(v);
        *guard = false;
    });
}

class AutoSuggestBoxWidget : public Widget {
public:
    explicit AutoSuggestBoxWidget(const AutoSuggestBox& desc) : m_desc(desc) {}
    explicit AutoSuggestBoxWidget(AutoSuggestBox&& desc) noexcept : m_desc(std::move(desc)) {}
    ~AutoSuggestBoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderAutoSuggestBox>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderAutoSuggestBox>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderAutoSuggestBox>& node) {
        node->set_properties(
            m_desc.text, m_desc.placeholder, m_desc.header, m_desc.items,
            m_desc.width, m_desc.disabled,
            m_desc.on_text_changed, m_desc.on_suggestion_chosen, m_desc.on_query_submitted);
        if (node->input_box()) node->input_box()->set_smooth_caret(m_desc.smooth_caret);
    }

    AutoSuggestBox m_desc;
};

inline AutoSuggestBox::operator WidgetPtr() const& {
    return make_widget<AutoSuggestBoxWidget>(*this);
}
inline AutoSuggestBox::operator WidgetPtr() && {
    return make_widget<AutoSuggestBoxWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
