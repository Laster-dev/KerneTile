#pragma once

#include "text_area_data.hpp"
#include "text_area_style.hpp"
#include <ktui/animation/spring_caret.hpp>
#include <ktui/core/strings.hpp>
#include "../common/fluent_chrome.hpp"
#include "../common/text_context_menu.hpp"
#include "../common/text_edit_core.hpp"
#include "../common/touch_selection_handles.hpp"
#include "../scroll/scrollbar.hpp"
#include "../scroll/smooth_scroll_physics.hpp"
#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

class RenderTextArea : public RenderBox, public ITextInputClient {
public:
    [[nodiscard]] ITextInputClient* as_text_input_client() noexcept override { return this; }
    RenderTextArea() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::IBeam;
        setup_behavior();
    }

    ~RenderTextArea() override {
        m_touch_handles.hide();
    }

    void set_properties(const std::string& text,
                        const std::string& placeholder,
                        const std::string& header,
                        float width,
                        float height,
                        bool disabled,
                        Fn<void(const std::string&)> on_change) {
        if (m_text != text) {
            m_text = text;
            m_caret = std::min(m_caret, m_text.size());
            m_sel_anchor = m_caret;
            rebuild_lines();
            if (m_show_touch_handles) sync_touch_handles();
            mark_needs_paint();
        }
        if (m_placeholder != placeholder) { m_placeholder = placeholder; mark_needs_paint(); }
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        if (std::abs(m_width - width) > 0.001f || std::abs(m_height - height) > 0.001f) {
            m_width = width;
            m_height = height;
            mark_needs_layout();
        }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            is_focusable = !disabled;
            cursor_shape = disabled ? CursorShape::Arrow : CursorShape::IBeam;
            if (disabled) {
                m_focused = false;
                m_dragging = false;
                m_drag_threshold_passed = false;
                m_pending_touch_collapse = false;
                m_click_count = 0;
                m_last_click_time = {};
                m_show_touch_handles = false;
                m_touch_handles.hide();
            }
            mark_needs_paint();
        }
        m_on_change = std::move(on_change);
    }

    void set_text_value(const std::string& v) {
        if (m_text != v) {
            m_text = v;
            m_caret = std::min(m_caret, m_text.size());
            m_sel_anchor = m_caret;
            rebuild_lines();
            if (m_show_touch_handles) sync_touch_handles();
            mark_needs_paint();
        }
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] size_t line_count() const noexcept { return std::max<size_t>(1, m_lines.size()); }
    [[nodiscard]] size_t caret() const noexcept { return m_caret; }

    // ==================== 选区与撤销/重做公共接口 ====================
    [[nodiscard]] bool has_selection() const noexcept { return m_sel_anchor != m_caret; }
    [[nodiscard]] size_t sel_start() const noexcept { return std::min(m_sel_anchor, m_caret); }
    [[nodiscard]] size_t sel_end() const noexcept { return std::max(m_sel_anchor, m_caret); }
    [[nodiscard]] std::string selected_text() const {
        if (!has_selection()) return {};
        size_t s = sel_start();
        size_t e = std::min(sel_end(), m_text.size());
        return (s < e) ? m_text.substr(s, e - s) : std::string{};
    }

    void set_selection(size_t anchor, size_t caret_pos) {
        m_sel_anchor = std::min(anchor, m_text.size());
        m_caret = std::min(caret_pos, m_text.size());
        reset_blink();
        mark_needs_paint();
    }

    void select_all() {
        m_sel_anchor = 0;
        m_caret = m_text.size();
        reset_blink();
        mark_needs_paint();
    }

    void clear_selection() noexcept { m_sel_anchor = m_caret; }

    [[nodiscard]] bool can_undo() const noexcept { return !m_undo_stack.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !m_redo_stack.empty(); }

    void undo() {
        if (m_undo_stack.empty() || m_disabled) return;
        m_redo_stack.push_back(UndoSnapshot{m_text, m_caret, m_sel_anchor});
        if (m_redo_stack.size() > 100) m_redo_stack.erase(m_redo_stack.begin());
        auto snap = std::move(m_undo_stack.back());
        m_undo_stack.pop_back();
        m_text = std::move(snap.text);
        m_caret = std::min(snap.caret, m_text.size());
        m_sel_anchor = std::min(snap.anchor, m_text.size());
        notify_edited();
    }

    void redo() {
        if (m_redo_stack.empty() || m_disabled) return;
        m_undo_stack.push_back(UndoSnapshot{m_text, m_caret, m_sel_anchor});
        if (m_undo_stack.size() > 100) m_undo_stack.erase(m_undo_stack.begin());
        auto snap = std::move(m_redo_stack.back());
        m_redo_stack.pop_back();
        m_text = std::move(snap.text);
        m_caret = std::min(snap.caret, m_text.size());
        m_sel_anchor = std::min(snap.anchor, m_text.size());
        notify_edited();
    }

    void delete_selection() {
        if (!has_selection() || m_disabled) return;
        push_undo_snapshot();
        delete_selection_raw();
        notify_edited();
    }

    // ==================== ITextInputClient ====================
    [[nodiscard]] bool accepts_text_input() const noexcept override { return !m_disabled; }

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        Vec2 g = global_offset();
        Rect b = box_rect();
        PointF cp = caret_local_pos();
        return Rect::from_xywh(g.x + cp.x, g.y + std::clamp(cp.y, b.min_y + 4.0f, b.max_y - 20.0f), 2.0f, 18.0f);
    }

    void on_ime_preedit(const std::string& text, size_t cursor_byte_pos) override {
        if (m_disabled) return;
        m_ime_preedit = text;
        m_ime_preedit_cursor = std::min(cursor_byte_pos, text.size());
        if (!m_ime_preedit.empty() && m_show_touch_handles) {
            m_show_touch_handles = false;
            m_touch_handles.hide();
        }
        rebuild_lines();
        ensure_caret_visible();
        mark_needs_paint();
    }

    void on_ime_commit(const std::string& utf8_text) override {
        if (m_disabled) return;
        m_ime_preedit.clear();
        m_ime_preedit_cursor = 0;
        if (!utf8_text.empty()) {
            insert_text(utf8_text);
        } else {
            rebuild_lines();
            mark_needs_paint();
        }
    }

    bool tick_cursor_blink() override {
        if (!m_focused || m_disabled) return false;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_blink_time).count() >= 530) {
            m_caret_visible = !m_caret_visible;
            m_blink_time = now;
            mark_needs_paint();
            return true;
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept override {
        if (!m_focused || m_disabled) return 1000;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_blink_time).count();
        return (ms >= 530) ? 16 : static_cast<uint32_t>(530 - ms);
    }

    void perform_layout() override {
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        float w = (m_width > 0.0f) ? m_width : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 360.0f);
        float h = (m_height > 0.0f) ? m_height : 120.0f;
        set_size(m_constraints.constrain(Vec2{w, header_h + h}));
        rebuild_lines();
    }

    void paint(PaintContext& ctx) override;

private:
    static constexpr float kFontSize = 13.5f;
    static constexpr float kLineH = 20.0f;

    struct VisualLine {
        size_t start{0};
        size_t end{0};
        std::string text;
    };

    struct UndoSnapshot {
        std::string text;
        size_t caret{0};
        size_t anchor{0};
    };

    Rect box_rect() const noexcept {
        float top = m_header.empty() ? 0.0f : 22.0f;
        return Rect::from_xywh(0.0f, top, size().x, std::max(48.0f, size().y - top));
    }

    void reset_blink() noexcept {
        m_caret_visible = true;
        m_blink_time = std::chrono::steady_clock::now();
    }

    void push_undo_snapshot() {
        m_undo_stack.push_back(UndoSnapshot{m_text, m_caret, m_sel_anchor});
        m_redo_stack.clear();
        if (m_undo_stack.size() > 100) {
            m_undo_stack.erase(m_undo_stack.begin());
        }
    }

    void delete_selection_raw() {
        if (!has_selection()) return;
        size_t s = sel_start();
        size_t e = std::min(sel_end(), m_text.size());
        if (s < e) m_text.erase(s, e - s);
        m_caret = s;
        m_sel_anchor = s;
    }

    static std::string normalize_newlines(const std::string& raw) {
        std::string out;
        out.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '\r') {
                if (i + 1 < raw.size() && raw[i + 1] == '\n') continue;
                out.push_back('\n');
            } else {
                out.push_back(raw[i]);
            }
        }
        return out;
    }

    static bool is_word_char(unsigned char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c >= 0x80;
    }

    std::string composed_text() const {
        if (m_ime_preedit.empty()) return m_text;
        std::string out = m_text;
        size_t pos = std::min(m_caret, out.size());
        out.insert(pos, m_ime_preedit);
        return out;
    }

    void rebuild_lines() {
        m_lines.clear();
        Rect b = box_rect();
        float max_w = std::max(40.0f, b.width() - 28.0f);
        std::string src = composed_text();
        size_t i = 0;
        size_t line_start = 0;
        std::string cur;

        while (i < src.size()) {
            if (src[i] == '\n') {
                m_lines.push_back(VisualLine{line_start, i, cur});
                cur.clear();
                ++i;
                line_start = i;
                continue;
            }
            size_t next = SingleLineTextEditCore::next_utf8(src, i);
            std::string ch = src.substr(i, next - i);
            if (!cur.empty() && TextMeasure::measure_width(cur + ch, kFontSize) > max_w) {
                m_lines.push_back(VisualLine{line_start, i, cur});
                cur = ch;
                line_start = i;
            } else {
                cur += ch;
            }
            i = next;
        }
        m_lines.push_back(VisualLine{line_start, src.size(), cur});

        float content_h = static_cast<float>(m_lines.size()) * kLineH + 16.0f;
        m_physics.set_max_scroll(std::max(0.0f, content_h - b.height()));
    }

    size_t caret_line_index() const noexcept {
        size_t eff_caret = m_caret + m_ime_preedit_cursor;
        for (size_t i = 0; i < m_lines.size(); ++i) {
            bool is_last = (i + 1 == m_lines.size());
            if (eff_caret >= m_lines[i].start && (eff_caret <= m_lines[i].end || is_last)) {
                if (!is_last && eff_caret == m_lines[i + 1].start && m_lines[i].end == eff_caret) {
                    return i + 1;
                }
                return i;
            }
        }
        return m_lines.empty() ? 0 : (m_lines.size() - 1);
    }

    PointF caret_local_pos() const {
        Rect b = box_rect();
        if (m_lines.empty()) return PointF{b.min_x + 11.0f, b.min_y + 8.0f};
        size_t eff_caret = m_caret + m_ime_preedit_cursor;
        size_t li = caret_line_index();
        const auto& line = m_lines[li];
        size_t rel = (eff_caret > line.start) ? std::min(eff_caret - line.start, line.text.size()) : 0;
        float cx = b.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel), kFontSize);
        float cy = b.min_y + 8.0f + static_cast<float>(li) * kLineH - m_physics.offset();
        return PointF{cx, cy};
    }

    void ensure_caret_visible() {
        Rect b = box_rect();
        size_t li = caret_line_index();
        float top = static_cast<float>(li) * kLineH;
        float bot = top + kLineH + 12.0f;
        float cur = m_physics.offset();
        if (top < cur) m_physics.scroll_to(top);
        else if (bot > cur + b.height()) m_physics.scroll_to(bot - b.height());
    }

    size_t byte_index_at_point(Vec2 local) const {
        Rect b = box_rect();
        if (m_lines.empty()) return 0;
        int li = static_cast<int>(std::floor((local.y - (b.min_y + 8.0f) + m_physics.offset()) / kLineH));
        li = std::clamp(li, 0, static_cast<int>(m_lines.size()) - 1);
        const auto& line = m_lines[static_cast<size_t>(li)];
        float target_x = std::max(0.0f, local.x - (b.min_x + 11.0f));
        size_t best = 0;
        float prev_w = 0.0f;
        size_t pos = 0;
        while (pos < line.text.size()) {
            size_t next = SingleLineTextEditCore::next_utf8(line.text, pos);
            float w = TextMeasure::measure_width(line.text.substr(0, next), kFontSize);
            if (target_x < (prev_w + w) * 0.5f) return std::min(line.start + pos, m_text.size());
            prev_w = w;
            best = next;
            pos = next;
        }
        return std::min(line.start + best, m_text.size());
    }

    void select_word_at(size_t idx) {
        if (m_text.empty()) return;
        idx = std::min(idx, m_text.size());
        if (idx == m_text.size() && idx > 0) idx = SingleLineTextEditCore::prev_utf8(m_text, idx);
        size_t s = idx;
        size_t e = idx;
        if (m_text[idx] == '\n') {
            m_sel_anchor = idx;
            m_caret = idx;
            return;
        }
        if (is_word_char(static_cast<unsigned char>(m_text[idx]))) {
            while (s > 0 && is_word_char(static_cast<unsigned char>(m_text[s - 1]))) {
                s = SingleLineTextEditCore::prev_utf8(m_text, s);
            }
            while (e < m_text.size() && is_word_char(static_cast<unsigned char>(m_text[e]))) {
                e = SingleLineTextEditCore::next_utf8(m_text, e);
            }
        } else {
            e = SingleLineTextEditCore::next_utf8(m_text, idx);
        }
        m_sel_anchor = s;
        m_caret = e;
    }

    void select_line_at(size_t idx) {
        if (m_lines.empty()) return;
        for (const auto& line : m_lines) {
            if (idx >= line.start && idx <= line.end) {
                m_sel_anchor = std::min(line.start, m_text.size());
                m_caret = std::min(line.end, m_text.size());
                return;
            }
        }
    }

    void notify_edited() {
        rebuild_lines();
        ensure_caret_visible();
        reset_blink();
        if (m_show_touch_handles) sync_touch_handles();
        mark_needs_paint();
        if (m_on_change) m_on_change(m_text);
    }

    void insert_text(const std::string& incoming) {
        std::string norm = normalize_newlines(incoming);
        if (norm.empty()) return;
        push_undo_snapshot();
        delete_selection_raw();
        m_text.insert(m_caret, norm);
        m_caret += norm.size();
        m_sel_anchor = m_caret;
        if (m_show_touch_handles) {
            m_show_touch_handles = false;
            m_touch_handles.hide();
        }
        notify_edited();
    }

    [[nodiscard]] size_t line_index_for_byte(size_t byte_idx, bool prefer_prev_on_wrap) const noexcept {
        for (size_t i = 0; i < m_lines.size(); ++i) {
            const bool is_last = (i + 1 == m_lines.size());
            if (byte_idx >= m_lines[i].start && (byte_idx <= m_lines[i].end || is_last)) {
                if (!prefer_prev_on_wrap && !is_last &&
                    byte_idx == m_lines[i + 1].start && m_lines[i].end == byte_idx) {
                    return i + 1;
                }
                return i;
            }
        }
        return m_lines.empty() ? 0 : (m_lines.size() - 1);
    }

    struct ByteLocalSpan {
        float x{0.0f};
        float top_y{0.0f};
        float bot_y{0.0f};
    };

    [[nodiscard]] ByteLocalSpan local_span_for_byte(size_t byte_idx, bool prefer_prev_on_wrap) const {
        Rect b = box_rect();
        if (m_lines.empty()) {
            const float ty = b.min_y + 8.0f - m_physics.offset();
            return ByteLocalSpan{b.min_x + 11.0f, ty, ty + kLineH};
        }
        const size_t li = line_index_for_byte(byte_idx, prefer_prev_on_wrap);
        const auto& line = m_lines[li];
        const size_t rel = (byte_idx > line.start) ? std::min(byte_idx - line.start, line.text.size()) : 0;
        const float x = b.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel), kFontSize);
        const float top_y = b.min_y + 8.0f + static_cast<float>(li) * kLineH - m_physics.offset();
        return ByteLocalSpan{x, top_y, top_y + kLineH};
    }

    [[nodiscard]] TouchSelectionHandlesState query_touch_handles_state() const {
        TouchSelectionHandlesState st;
        if (!m_show_touch_handles || !m_focused || m_disabled ||
            !m_ime_preedit.empty() || m_text.empty()) {
            return st;
        }
        Window* win = Window::active_window();
        if (!win) return st;

        const Rect b = box_rect();
        const Vec2 g = global_offset();
        const float tb = win->titlebar_height();
        auto to_win = [&](PointF lp) -> PointF {
            return PointF{g.x + lp.x, g.y + lp.y + tb};
        };
        auto in_view = [&](const ByteLocalSpan& sp) -> bool {
            return sp.x >= b.min_x + 2.0f && sp.x <= b.max_x - 2.0f &&
                   sp.bot_y >= b.min_y + 2.0f && sp.top_y <= b.max_y - 2.0f &&
                   is_local_point_visible_in_ancestors(this, PointF{sp.x, std::clamp(sp.bot_y, b.min_y, b.max_y)});
        };

        st.active = true;
        st.has_selection = has_selection();
        if (st.has_selection) {
            const ByteLocalSpan s_sp = local_span_for_byte(sel_start(), false);
            const ByteLocalSpan e_sp = local_span_for_byte(sel_end(), true);
            st.start_top = to_win(PointF{s_sp.x, s_sp.top_y});
            st.start_bottom = to_win(PointF{s_sp.x, s_sp.bot_y});
            st.end_top = to_win(PointF{e_sp.x, e_sp.top_y});
            st.end_bottom = to_win(PointF{e_sp.x, e_sp.bot_y});
            st.show_start = in_view(s_sp);
            st.show_end = in_view(e_sp);
        } else {
            const ByteLocalSpan c_sp = local_span_for_byte(m_caret, false);
            st.caret_top = to_win(PointF{c_sp.x, c_sp.top_y});
            st.caret_bottom = to_win(PointF{c_sp.x, c_sp.bot_y});
            st.show_caret = in_view(c_sp);
        }
        return st;
    }

    void sync_touch_handles() {
        const bool should_show = m_show_touch_handles && m_focused && !m_disabled &&
                                 m_ime_preedit.empty() && !m_text.empty();
        if (!should_show) {
            m_touch_handles.hide();
            return;
        }
        m_touch_handles.ensure_shown(
            [this]() { return query_touch_handles_state(); },
            [this](TouchHandleKind kind, PointF) {
                m_pending_touch_collapse = false;
                m_dragging = false;
                m_drag_threshold_passed = false;
                m_click_count = 0;
                if (kind == TouchHandleKind::Start) {
                    m_handle_fixed_index = sel_end();
                } else if (kind == TouchHandleKind::End) {
                    m_handle_fixed_index = sel_start();
                } else {
                    m_handle_fixed_index = m_caret;
                }
            },
            [this](TouchHandleKind kind, PointF win_pos) {
                if (m_disabled) return;
                Window* win = Window::active_window();
                const float tb = win ? win->titlebar_height() : 0.0f;
                const Vec2 g = global_offset();
                const Vec2 local_pt{win_pos.x - g.x, win_pos.y - g.y - tb};
                const size_t idx = byte_index_at_point(local_pt);
                if (kind == TouchHandleKind::Caret) {
                    m_caret = idx;
                    m_sel_anchor = idx;
                } else if (idx != m_handle_fixed_index) {
                    m_sel_anchor = m_handle_fixed_index;
                    m_caret = idx;
                }
                ensure_caret_visible();
                reset_blink();
                mark_needs_paint();
            },
            [this](TouchHandleKind) {
                mark_needs_paint();
            });
    }

    void move_vertical(int delta, bool keep_selection) {
        if (m_lines.empty()) return;
        size_t li = caret_line_index();
        int target = std::clamp(static_cast<int>(li) + delta, 0, static_cast<int>(m_lines.size()) - 1);
        if (static_cast<size_t>(target) != li) {
            size_t rel = (m_caret > m_lines[li].start) ? (m_caret - m_lines[li].start) : 0;
            float cur_x = TextMeasure::measure_width(
                m_lines[li].text.substr(0, std::min(rel, m_lines[li].text.size())), kFontSize);
            const auto& dst = m_lines[static_cast<size_t>(target)];
            size_t best = 0;
            float prev_w = 0.0f;
            size_t pos = 0;
            while (pos < dst.text.size()) {
                size_t next = SingleLineTextEditCore::next_utf8(dst.text, pos);
                float w = TextMeasure::measure_width(dst.text.substr(0, next), kFontSize);
                if (cur_x < (prev_w + w) * 0.5f) { best = pos; break; }
                prev_w = w;
                best = next;
                pos = next;
            }
            m_caret = std::min(dst.start + best, m_text.size());
        } else if (delta < 0) {
            m_caret = 0;
        } else {
            m_caret = m_text.size();
        }
        if (!keep_selection) m_sel_anchor = m_caret;
        ensure_caret_visible();
        reset_blink();
        mark_needs_paint();
    }

    void open_context_menu(Vec2 client_pos, bool is_touch = false) {
        m_click_count = 0;
        m_last_click_time = {};
        if (is_touch && has_selection()) {
            m_show_touch_handles = true;
            sync_touch_handles();
        }
        TextContextMenuActions actions;
        actions.can_undo = can_undo();
        actions.can_redo = can_redo();
        actions.can_cut = has_selection();
        actions.can_copy = has_selection();
        actions.can_paste = !Clipboard::get_text().empty();
        actions.can_delete = has_selection();
        actions.can_select_all = !m_text.empty();

        actions.on_undo = [this]() { undo(); };
        actions.on_redo = [this]() { redo(); };
        actions.on_cut = [this]() {
            if (has_selection()) {
                Clipboard::set_text(selected_text());
                delete_selection();
            }
        };
        actions.on_copy = [this]() {
            if (has_selection()) Clipboard::set_text(selected_text());
        };
        actions.on_paste = [this]() {
            std::string clip = Clipboard::get_text();
            if (!clip.empty()) insert_text(clip);
        };
        actions.on_delete = [this]() { delete_selection(); };
        actions.on_select_all = [this, client_pos, is_touch]() {
            select_all();
            if (is_touch && has_selection()) {
                m_show_touch_handles = true;
                sync_touch_handles();
                open_context_menu(client_pos, true);
            }
        };

        Vec2 menu_pos = client_pos;
        if (is_touch) {
            const Vec2 g = global_offset();
            const ByteLocalSpan anchor_sp = has_selection()
                ? local_span_for_byte(sel_end(), true)
                : local_span_for_byte(m_caret, false);
            const float handles_clear_y = g.y + anchor_sp.bot_y + 26.0f;
            menu_pos.y = std::max(menu_pos.y, handles_clear_y);
        }
        show_winui_text_context_menu(menu_pos, actions);
    }

    void setup_behavior();

    std::string m_text;
    std::string m_placeholder{FrameworkStrings::current().multiline_placeholder};
    std::string m_header;
    std::string m_ime_preedit;
    size_t m_ime_preedit_cursor{0};
    float m_width{0.0f};
    float m_height{120.0f};
    bool m_disabled{false};
    bool m_hovered{false};
    bool m_focused{false};
    bool m_dragging{false};
    bool m_drag_threshold_passed{false};
    PointF m_drag_start_local{0.0f, 0.0f};
    bool m_pending_touch_collapse{false};
    size_t m_pending_touch_collapse_idx{0};
    bool m_show_touch_handles{false};
    size_t m_handle_fixed_index{0};
    TouchSelectionHandleController m_touch_handles;
    bool m_caret_visible{true};
    size_t m_caret{0};
    size_t m_sel_anchor{0};
    int m_click_count{0};
    PointF m_last_click_pos{0.0f, 0.0f};
    float m_focus_anim{0.0f};
    std::chrono::steady_clock::time_point m_blink_time{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point m_last_click_time{};
    std::vector<VisualLine> m_lines;
    std::vector<UndoSnapshot> m_undo_stack;
    std::vector<UndoSnapshot> m_redo_stack;
    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;
    bool m_smooth_caret{true};
    SpringCaretSolver m_spring_caret;
    Fn<void(const std::string&)> m_on_change;

public:
    void set_smooth_caret(bool enabled) noexcept {
        if (m_smooth_caret != enabled) {
            m_smooth_caret = enabled;
            m_spring_caret.set_enabled(enabled);
            mark_needs_paint();
        }
    }
    void wrap_on_change(Fn<void(const std::string&)> extra) {
        auto prev = std::move(m_on_change);
        m_on_change = [prev = std::move(prev), extra = std::move(extra)](const std::string& v) {
            if (extra) extra(v);
            if (prev) prev(v);
        };
    }
};

} // namespace ktui

#include "text_area_behavior.hpp"
#include "text_area_widget.hpp"
