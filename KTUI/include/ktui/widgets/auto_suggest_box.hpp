#pragma once

#include "common/fluent_chrome.hpp"
#include "common/fluent_icons.hpp"
#include "common/text_edit_core.hpp"
#include "widget.hpp"
#include <ktui/app/window.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/tree/popup_handle.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// WinUI 3 AutoSuggestBox 悬浮候选下拉列表
class RenderAutoSuggestPopup : public RenderBox {
public:
    RenderAutoSuggestPopup(std::vector<std::string> suggestions,
                           float width,
                           std::function<void(const std::string&)> on_pick)
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
        float anim_h = std::max(24.0f, size().y * (0.78f + 0.22f * st.ease));
        float alpha = st.alpha;

        FluentPopupSurface::paint_panel(ctx, theme, Rect::from_xywh(0.0f, 0.0f, size().x, anim_h), alpha);

        Color hov_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.068f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.045f * alpha};
        Color hi_bg  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.095f * alpha} : Color{0.0f, 0.0f, 0.0f, 0.065f * alpha};
        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.95f * alpha}  : Color{0.0f, 0.0f, 0.0f, 0.90f * alpha};
        Color mute_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.48f * alpha}  : Color{0.0f, 0.0f, 0.0f, 0.48f * alpha};

        if (m_suggestions.empty()) {
            Vec2 ts = TextMeasure::measure("无匹配结果", 13.0f);
            ctx.draw_text("无匹配结果", PointF{14.0f, std::round((anim_h - ts.y) * 0.5f)}, 13.0f, mute_c);
            return;
        }

        ctx.push_clip(Rect::from_xywh(1.0f, 4.0f, size().x - 2.0f, std::max(0.0f, anim_h - 8.0f)));
        float y = 4.0f - m_scroll_offset;
        for (size_t i = 0; i < m_suggestions.size(); ++i) {
            Rect row = Rect::from_xywh(4.0f, y, size().x - 8.0f, kItemH);
            if (row.max_y >= 0.0f && row.min_y <= anim_h) {
                if (static_cast<int>(i) == m_highlighted) {
                    ctx.draw_rect(row, hi_bg, 4.0f, 0.0f);
                    Color pill = theme.colors.primary;
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
            int idx = hit_row(e.local_position);
            cursor_shape = (idx >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (idx != m_hovered) { m_hovered = idx; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered != -1) { m_hovered = -1; mark_needs_paint(); }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int idx = hit_row(e.local_position);
            if (idx >= 0 && idx < static_cast<int>(m_suggestions.size())) {
                if (m_on_pick) m_on_pick(m_suggestions[static_cast<size_t>(idx)]);
                e.stop_propagation();
            }
        };
    }

    std::vector<std::string> m_suggestions;
    float m_width{260.0f};
    std::function<void(const std::string&)> m_on_pick;
    int m_hovered{-1};
    int m_highlighted{-1};
    float m_scroll_offset{0.0f};
    FluentPopupSurface m_surface;
};

// WinUI 3 AutoSuggestBox 自动补全搜索框渲染盒
class RenderAutoSuggestBox : public RenderBox, public ITextInputClient {
public:
    RenderAutoSuggestBox() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::IBeam;
        setup_behavior();
    }

    ~RenderAutoSuggestBox() override { close_popup(); }

    void set_properties(const std::string& text,
                        const std::string& placeholder,
                        const std::string& header,
                        std::vector<std::string> items,
                        float width,
                        bool disabled,
                        std::function<void(const std::string&)> on_text_changed,
                        std::function<void(const std::string&)> on_suggestion_chosen,
                        std::function<void(const std::string&)> on_query_submitted) {
        if (m_text != text) {
            m_text = text;
            m_edit.clamp_caret(m_text);
            m_edit.clear_selection();
            rebuild_filtered();
            mark_needs_paint();
        }
        if (m_placeholder != placeholder) { m_placeholder = placeholder; mark_needs_paint(); }
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        if (m_items != items) {
            m_items = std::move(items);
            rebuild_filtered();
            if (m_popup) m_popup->set_suggestions(m_filtered);
        }
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            is_focusable = !disabled;
            cursor_shape = disabled ? CursorShape::Arrow : CursorShape::IBeam;
            if (disabled) { m_focused = false; close_popup(); }
            mark_needs_paint();
        }
        m_on_text_changed = std::move(on_text_changed);
        m_on_suggestion_chosen = std::move(on_suggestion_chosen);
        m_on_query_submitted = std::move(on_query_submitted);
    }

    void set_text_value(const std::string& v) {
        if (m_text != v) {
            m_text = v;
            m_edit.clamp_caret(m_text);
            m_edit.clear_selection();
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

    void select_suggestion(size_t idx) {
        if (idx < m_filtered.size()) apply_chosen_suggestion(m_filtered[idx]);
    }

    // ==================== ITextInputClient ====================
    [[nodiscard]] bool accepts_text_input() const noexcept override { return !m_disabled; }

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        Vec2 g = global_offset();
        Rect box = box_rect();
        float cx = box.min_x + 11.0f + TextMeasure::measure(m_text.substr(0, m_edit.caret()), 13.5f).x;
        return Rect::from_xywh(g.x + cx, g.y + box.min_y + 6.0f, 2.0f, 20.0f);
    }

    void on_ime_preedit(const std::string&, size_t) override {}

    void on_ime_commit(const std::string& utf8_text) override {
        if (!m_disabled && m_edit.insert_text(m_text, utf8_text)) {
            notify_text_edited();
        }
    }

    bool tick_cursor_blink() override {
        if (m_edit.tick_blink(m_focused, m_disabled)) {
            mark_needs_paint();
            return true;
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept override {
        return m_edit.time_until_next_blink_ms(m_focused, m_disabled);
    }

    void perform_layout() override {
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        float w = (m_width > 0.0f) ? m_width : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 280.0f);
        set_size(m_constraints.constrain(Vec2{w, header_h + 32.0f}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        if (FluentInputChrome::step_focus_anim(m_focus_anim, m_focused && !m_disabled)) {
            mark_needs_paint();
        }

        Rect box = box_rect();
        FluentInputChrome::paint_box(ctx, theme, box, m_header, m_disabled, m_hovered, m_focused, m_focus_anim);

        Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f}
                                       : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.92f};
        Color muted_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.54f} : Color{0.0f, 0.0f, 0.0f, 0.54f};

        Rect search_r = search_btn_rect();
        Rect clear_r = clear_btn_rect();
        bool show_clear = (!m_text.empty() && !m_disabled);
        float text_right = show_clear ? (clear_r.min_x - 4.0f) : (search_r.min_x - 4.0f);

        Rect text_clip = Rect::from_xywh(box.min_x + 10.0f, box.min_y + 2.0f,
                                         std::max(10.0f, text_right - (box.min_x + 10.0f)),
                                         box.height() - 4.0f);
        ctx.push_clip(text_clip);
        if (m_text.empty()) {
            Vec2 ps = TextMeasure::measure(m_placeholder, 13.5f);
            ctx.draw_text(m_placeholder,
                          PointF{box.min_x + 11.0f, std::round(box.min_y + (box.height() - ps.y) * 0.5f)},
                          13.5f, muted_col);
        } else {
            Vec2 ts = TextMeasure::measure(m_text, 13.5f);
            float ty = std::round(box.min_y + (box.height() - ts.y) * 0.5f);
            if (m_edit.select_all() && m_focused) {
                Color sel_bg = theme.colors.primary;
                sel_bg.a = 0.35f;
                ctx.draw_rect(Rect::from_xywh(box.min_x + 10.0f, box.min_y + 5.0f, ts.x + 2.0f, box.height() - 10.0f),
                              sel_bg, 2.0f, 0.0f);
            }
            ctx.draw_text(m_text, PointF{box.min_x + 11.0f, ty}, 13.5f, text_col);
        }

        if (m_focused && !m_disabled && m_edit.caret_visible()) {
            float cx = box.min_x + 11.0f + TextMeasure::measure(m_text.substr(0, m_edit.caret()), 13.5f).x;
            ctx.draw_rect(Rect::from_xywh(std::round(cx), box.min_y + 7.0f, 1.5f, box.height() - 14.0f),
                          text_col, 0.5f, 0.0f);
        }
        ctx.pop_clip();

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
        auto popup = std::make_shared<RenderAutoSuggestPopup>(
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
        m_edit.set_caret(m_text.size(), m_text);
        rebuild_filtered();
        close_popup();
        mark_needs_paint();
        if (m_on_text_changed) m_on_text_changed(m_text);
        if (m_on_suggestion_chosen) m_on_suggestion_chosen(m_text);
        if (m_on_query_submitted) m_on_query_submitted(m_text);
    }

    void notify_text_edited() {
        rebuild_filtered();
        if (m_focused) open_or_update_popup();
        mark_needs_paint();
        if (m_on_text_changed) m_on_text_changed(m_text);
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_disabled) { m_hovered = true; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_hover_clear = false;
            m_hover_search = false;
            mark_needs_paint();
        };
        on_pointer_move = [this](PointerEvent& e) {
            if (m_disabled) return;
            bool hc = !m_text.empty() && clear_btn_rect().contains(e.local_position);
            bool hs = search_btn_rect().contains(e.local_position);
            cursor_shape = (hc || hs) ? CursorShape::PointingHand : CursorShape::IBeam;
            if (hc != m_hover_clear || hs != m_hover_search) {
                m_hover_clear = hc;
                m_hover_search = hs;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            if (!m_text.empty() && clear_btn_rect().contains(e.local_position)) {
                m_text.clear();
                m_edit.set_caret(0, m_text);
                notify_text_edited();
                e.stop_propagation();
                return;
            }
            if (search_btn_rect().contains(e.local_position)) {
                close_popup();
                if (m_on_query_submitted) m_on_query_submitted(m_text);
                e.stop_propagation();
                return;
            }
            m_edit.set_caret(m_text.size(), m_text);
            open_or_update_popup();
            mark_needs_paint();
            e.stop_propagation();
        };
        on_focus = [this](FocusEvent& e) {
            if (m_disabled) return;
            m_focused = e.gained_focus;
            if (m_focused) {
                m_edit.reset_blink();
                open_or_update_popup();
            } else {
                m_edit.clear_selection();
            }
            mark_needs_paint();
        };
        on_key = [this](KeyEvent& e) {
            if (m_disabled || e.type != PlatformEventType::KeyDown) return;
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
            auto r = m_edit.handle_key(m_text, e, true, 0);
            if (r.submitted) {
                if (m_popup && m_popup->highlighted() >= 0 &&
                    m_popup->highlighted() < static_cast<int>(m_filtered.size())) {
                    apply_chosen_suggestion(m_filtered[static_cast<size_t>(m_popup->highlighted())]);
                } else {
                    close_popup();
                    if (m_on_query_submitted) m_on_query_submitted(m_text);
                }
                e.stop_propagation();
                return;
            }
            if (r.text_changed) notify_text_edited();
            else if (r.caret_moved) mark_needs_paint();
            if (r.handled) e.stop_propagation();
        };
    }

    std::string m_text;
    std::string m_placeholder{"搜索..."};
    std::string m_header;
    std::vector<std::string> m_items;
    std::vector<std::string> m_filtered;
    float m_width{280.0f};
    bool m_disabled{false};
    bool m_hovered{false};
    bool m_hover_clear{false};
    bool m_hover_search{false};
    bool m_focused{false};
    float m_focus_anim{0.0f};
    SingleLineTextEditCore m_edit;

    PopupHandle m_popup_handle;
    std::shared_ptr<RenderAutoSuggestPopup> m_popup;

    std::function<void(const std::string&)> m_on_text_changed;
    std::function<void(const std::string&)> m_on_suggestion_chosen;
    std::function<void(const std::string&)> m_on_query_submitted;
};

// C++20 指定初始化器声明式 WinUI 3 AutoSuggestBox
struct AutoSuggestBox {
    Bindable<std::string> text{std::string{}};
    std::string placeholder{"搜索..."};
    std::string header;
    Bindable<std::vector<std::string>> items{std::vector<std::string>{}};
    float width{280.0f};
    bool disabled{false};
    std::function<void(const std::string&)> on_text_changed{nullptr};
    std::function<void(const std::string&)> on_suggestion_chosen{nullptr};
    std::function<void(const std::string&)> on_query_submitted{nullptr};

    operator WidgetPtr() const;
};

class AutoSuggestBoxWidget : public Widget {
public:
    explicit AutoSuggestBoxWidget(const AutoSuggestBox& desc) : m_desc(desc) {}
    ~AutoSuggestBoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderAutoSuggestBox>();
        apply(node);
        bind(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::dynamic_pointer_cast<RenderAutoSuggestBox>(node)) {
            apply(n);
            bind(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderAutoSuggestBox>& node) {
        auto text_bind = m_desc.text;
        auto user_change = m_desc.on_text_changed;
        node->set_properties(
            m_desc.text.get(), m_desc.placeholder, m_desc.header, m_desc.items.get(),
            m_desc.width, m_desc.disabled,
            [text_bind, user_change](const std::string& v) mutable {
                text_bind.set_bound(v);
                if (user_change) user_change(v);
            },
            m_desc.on_suggestion_chosen, m_desc.on_query_submitted);
    }

    void bind(const std::shared_ptr<RenderAutoSuggestBox>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.text, node, [](RenderAutoSuggestBox& n, const std::string& v) { n.set_text_value(v); });
        m_bindings.attach(m_desc.items, node, [](RenderAutoSuggestBox& n, const std::vector<std::string>& v) { n.set_items_value(v); });
    }

    AutoSuggestBox m_desc;
    BindingGroup m_bindings;
};

inline AutoSuggestBox::operator WidgetPtr() const {
    return std::make_shared<AutoSuggestBoxWidget>(*this);
}

} // namespace ktui
