#pragma once

#include "common/fluent_chrome.hpp"
#include "common/text_context_menu.hpp"
#include "common/text_edit_core.hpp"
#include "scroll/scrollbar.hpp"
#include "scroll/smooth_scroll_physics.hpp"
#include "widget.hpp"
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

// WinUI 3 多行文本编辑器渲染盒 (RenderTextArea)
// 完整支持鼠标拖拽选区、双击选词、三击选行、Shift+方向/Home/End 选区、
// Ctrl+Z/Y 撤销重做栈、局部剪贴板复制/剪切/粘贴、中文 IME 预编辑下划线、
// WinUI 3 右键上下文菜单与内置垂直平滑滚动条
class RenderTextArea : public RenderBox, public ITextInputClient {
public:
    [[nodiscard]] ITextInputClient* as_text_input_client() noexcept override { return this; }
    RenderTextArea() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::IBeam;
        setup_behavior();
    }

    ~RenderTextArea() override = default;

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

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        bool anim_f = FluentInputChrome::step_focus_anim(m_focus_anim, m_focused && !m_disabled);
        bool anim_s = m_physics.update();
        bool anim_b = m_scrollbar.update_animations(m_hovered);
        if (anim_f || anim_s || anim_b) mark_needs_paint();

        Rect box = box_rect();
        // WinUI 3 TextBox / RichEditBox 原生风格底板
        Color text_c = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f}
                                     : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
        Color ph_c   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.54f} : Color{0.0f, 0.0f, 0.0f, 0.54f};
        Color winui_accent = theme.is_dark
            ? Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}  // WinUI 3 Dark #60CDFF
            : Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};            // WinUI 3 Light #005FB8

        if (!m_header.empty()) {
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, text_c);
        }

        Color bg, border, bottom_line;
        if (theme.is_dark) {
            bg = m_disabled ? Color{1.0f, 1.0f, 1.0f, 0.03f}
               : (m_focused ? Color{30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 0.96f}
                            : (m_hovered ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{1.0f, 1.0f, 1.0f, 0.055f}));
            border      = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.05f : 0.08f};
            bottom_line = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.06f : 0.54f};
        } else {
            bg = m_disabled ? Color{0.96f, 0.96f, 0.96f, 0.55f}
               : (m_focused ? Color{1.0f, 1.0f, 1.0f, 1.0f}
                            : (m_hovered ? Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.65f}
                                         : Color{1.0f, 1.0f, 1.0f, 0.72f}));
            border      = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.05f : 0.08f};
            bottom_line = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.06f : 0.45f};
        }

        ctx.draw_rect(box, bg, 4.0f, 0.0f);
        ctx.draw_rect(box, border, 4.0f, 1.0f);
        ctx.push_clip(box, 4.0f);
        ctx.draw_rect(Rect::from_xywh(box.min_x, box.max_y - 1.0f, box.width(), 1.0f), bottom_line, 0.0f, 0.0f);
        if (m_focus_anim > 0.01f && !m_disabled) {
            float half_w = box.width() * 0.5f * m_focus_anim;
            float cx = (box.min_x + box.max_x) * 0.5f;
            ctx.draw_rect(Rect::from_xywh(cx - half_w, box.max_y - 2.0f, half_w * 2.0f, 2.0f), winui_accent, 0.0f, 0.0f);
        }
        ctx.pop_clip();

        Rect clip = Rect::from_xywh(box.min_x + 2.0f, box.min_y + 2.0f,
                                    std::max(0.0f, box.width() - 4.0f),
                                    std::max(0.0f, box.height() - 4.0f));
        ctx.push_clip(clip, 3.0f);

        float scroll_y = m_physics.offset();
        const bool show_sel = has_selection() && m_ime_preedit.empty();
        const size_t s_start = sel_start();
        const size_t s_end = sel_end();
        Color sel_col = FluentInputChrome::selection_bg(theme, m_focused);
        Color sel_txt_col = FluentInputChrome::selection_text(theme);

        if (m_text.empty() && m_ime_preedit.empty()) {
            if (!m_placeholder.empty()) {
                ctx.draw_text(m_placeholder, PointF{box.min_x + 11.0f, box.min_y + 8.0f}, kFontSize, ph_c);
            }
        } else {
            const size_t pre_start = m_caret;
            const size_t pre_end = m_caret + m_ime_preedit.size();
            for (size_t i = 0; i < m_lines.size(); ++i) {
                float ly = box.min_y + 8.0f + static_cast<float>(i) * kLineH - scroll_y;
                if (ly + kLineH < box.min_y || ly > box.max_y) continue;
                const auto& line = m_lines[i];

                // 先绘制常规行文本
                if (!line.text.empty()) {
                    ctx.draw_text(line.text, PointF{box.min_x + 11.0f, ly}, kFontSize, text_c);
                }

                // WinUI 3 原生逐行选区：聚焦时实心强调色底块 + 裁剪反白前景文字 (#FFFFFF)
                if (show_sel) {
                    size_t ov_s = std::max(s_start, line.start);
                    size_t ov_e = std::min(s_end, line.end);
                    bool spans_newline = (s_start <= line.end && s_end > line.end);
                    if (ov_s < ov_e || spans_newline) {
                        size_t rel_s = (ov_s > line.start) ? std::min(ov_s - line.start, line.text.size()) : 0;
                        size_t rel_e = (ov_e > line.start) ? std::min(ov_e - line.start, line.text.size()) : 0;
                        float x0 = box.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel_s), kFontSize);
                        float x1 = box.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel_e), kFontSize);
                        if (spans_newline) x1 += 6.0f;
                        if (x1 > x0) {
                            Rect sel_r = Rect::from_xywh(x0, ly - 1.0f, x1 - x0, kLineH);
                            ctx.draw_rect(sel_r, sel_col, 0.0f, 0.0f);
                            if (m_focused && !line.text.empty()) {
                                ctx.push_clip(sel_r, 0.0f);
                                ctx.draw_text(line.text, PointF{box.min_x + 11.0f, ly}, kFontSize, sel_txt_col);
                                ctx.pop_clip();
                            }
                        }
                    }
                }

                // IME 预编辑下划线
                if (!m_ime_preedit.empty()) {
                    size_t ps = std::max(pre_start, line.start);
                    size_t pe = std::min(pre_end, line.end);
                    if (ps < pe) {
                        size_t rel_s = std::min(ps - line.start, line.text.size());
                        size_t rel_e = std::min(pe - line.start, line.text.size());
                        float ux0 = box.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel_s), kFontSize);
                        float ux1 = box.min_x + 11.0f + TextMeasure::measure_width(line.text.substr(0, rel_e), kFontSize);
                        ctx.draw_rect(Rect::from_xywh(ux0, ly + kLineH - 3.0f, std::max(2.0f, ux1 - ux0), 1.5f),
                                      winui_accent, 0.0f, 0.0f);
                    }
                }
            }
        }

        if (m_focused && !m_disabled && m_caret_visible) {
            PointF cp = caret_local_pos();
            ctx.draw_rect(Rect::from_xywh(std::round(cp.x), std::round(cp.y), 1.5f, kLineH - 3.0f),
                          text_c, 0.5f, 0.0f);
        }

        float content_h = static_cast<float>(std::max<size_t>(1, m_lines.size())) * kLineH + 16.0f;
        m_scrollbar.paint(ctx, box, content_h, scroll_y, theme);
        ctx.pop_clip();
    }

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
        notify_edited();
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

    void open_context_menu(Vec2 client_pos) {
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
        actions.on_select_all = [this]() { select_all(); };

        show_winui_text_context_menu(client_pos, actions);
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_disabled) { m_hovered = true; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };
        on_pointer_move = [this](PointerEvent& e) {
            if (e.type == PlatformEventType::PointerScroll) {
                if (m_physics.max_scroll() > 0.0f) {
                    m_physics.add_wheel_delta(e.delta.y);
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.stop_propagation();
                }
                return;
            }
            Rect b = box_rect();
            float content_h = static_cast<float>(std::max<size_t>(1, m_lines.size())) * kLineH + 16.0f;
            bool consumed = false;
            if (m_scrollbar.handle_pointer_move(e.local_position, b, content_h, m_physics, consumed)) {
                mark_needs_paint();
            }
            if (consumed) {
                e.stop_propagation();
                return;
            }
            if (m_dragging && !m_disabled) {
                m_caret = byte_index_at_point(e.local_position);
                ensure_caret_visible();
                reset_blink();
                mark_needs_paint();
                e.stop_propagation();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (m_disabled) return;
            if (e.button == PointerButton::Secondary) {
                open_context_menu(e.position);
                e.stop_propagation();
                return;
            }
            if (e.button != PointerButton::Primary) return;

            Rect b = box_rect();
            float content_h = static_cast<float>(std::max<size_t>(1, m_lines.size())) * kLineH + 16.0f;
            if (m_scrollbar.handle_pointer_down(e.local_position, b, content_h, m_physics)) {
                mark_needs_paint();
                e.stop_propagation();
                return;
            }

            auto now = std::chrono::steady_clock::now();
            float dt_ms = std::chrono::duration<float, std::milli>(now - m_last_click_time).count();
            m_click_count = (dt_ms < 400.0f) ? (m_click_count + 1) : 1;
            m_last_click_time = now;

            size_t idx = byte_index_at_point(e.local_position);
            if (m_click_count == 2) {
                select_word_at(idx);
                m_dragging = false;
            } else if (m_click_count >= 3) {
                select_line_at(idx);
                m_dragging = false;
            } else {
                m_caret = idx;
                if (!e.modifiers.shift) m_sel_anchor = m_caret;
                m_dragging = true;
            }
            reset_blink();
            mark_needs_paint();
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent&) {
            m_dragging = false;
            if (m_scrollbar.handle_pointer_up()) mark_needs_paint();
        };
        on_focus = [this](FocusEvent& e) {
            if (m_disabled) return;
            m_focused = e.gained_focus;
            if (m_focused) {
                reset_blink();
            } else {
                m_dragging = false;
                m_ime_preedit.clear();
                m_ime_preedit_cursor = 0;
            }
            mark_needs_paint();
        };
        on_key = [this](KeyEvent& e) {
            if (m_disabled) return;
            if (e.type == PlatformEventType::ImeCommit) {
                if (!e.text.empty()) {
                    on_ime_commit(e.text);
                    e.stop_propagation();
                }
                return;
            }
            if (e.type != PlatformEventType::KeyDown) return;

            if (e.modifiers.ctrl) {
                uint32_t kc = e.key_code;
                if (kc == 'A' || kc == 'a') {
                    select_all();
                    e.stop_propagation();
                    return;
                }
                if (kc == 'C' || kc == 'c') {
                    if (has_selection()) Clipboard::set_text(selected_text());
                    e.stop_propagation();
                    return;
                }
                if (kc == 'X' || kc == 'x') {
                    if (has_selection()) {
                        Clipboard::set_text(selected_text());
                        delete_selection();
                    }
                    e.stop_propagation();
                    return;
                }
                if (kc == 'V' || kc == 'v') {
                    std::string clip = Clipboard::get_text();
                    if (!clip.empty()) insert_text(clip);
                    e.stop_propagation();
                    return;
                }
                if (kc == 'Z' || kc == 'z') {
                    if (e.modifiers.shift) redo();
                    else undo();
                    e.stop_propagation();
                    return;
                }
                if (kc == 'Y' || kc == 'y') {
                    redo();
                    e.stop_propagation();
                    return;
                }
                if (kc == 36) { // Ctrl+Home -> 文档首
                    m_caret = 0;
                    if (!e.modifiers.shift) m_sel_anchor = 0;
                    ensure_caret_visible();
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
                if (kc == 35) { // Ctrl+End -> 文档尾
                    m_caret = m_text.size();
                    if (!e.modifiers.shift) m_sel_anchor = m_caret;
                    ensure_caret_visible();
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
            }

            switch (e.key_code) {
                case 13: // Enter -> 换行
                    insert_text("\n");
                    e.stop_propagation();
                    return;
                case 9: // Tab -> 4 空格缩进
                    insert_text("    ");
                    e.stop_propagation();
                    return;
                case 38: // Up
                    move_vertical(-1, e.modifiers.shift);
                    e.stop_propagation();
                    return;
                case 40: // Down
                    move_vertical(1, e.modifiers.shift);
                    e.stop_propagation();
                    return;
                case 33: // PageUp
                    move_vertical(-5, e.modifiers.shift);
                    e.stop_propagation();
                    return;
                case 34: // PageDown
                    move_vertical(5, e.modifiers.shift);
                    e.stop_propagation();
                    return;
                case 8: // Backspace
                    if (has_selection()) {
                        delete_selection();
                    } else if (m_caret > 0) {
                        push_undo_snapshot();
                        size_t p = SingleLineTextEditCore::prev_utf8(m_text, m_caret);
                        m_text.erase(p, m_caret - p);
                        m_caret = p;
                        m_sel_anchor = p;
                        notify_edited();
                    }
                    e.stop_propagation();
                    return;
                case 46: // Delete
                    if (has_selection()) {
                        delete_selection();
                    } else if (m_caret < m_text.size()) {
                        push_undo_snapshot();
                        size_t n = SingleLineTextEditCore::next_utf8(m_text, m_caret);
                        m_text.erase(m_caret, n - m_caret);
                        notify_edited();
                    }
                    e.stop_propagation();
                    return;
                case 37: // Left
                    if (e.modifiers.shift) {
                        m_caret = SingleLineTextEditCore::prev_utf8(m_text, m_caret);
                    } else if (has_selection()) {
                        m_caret = sel_start();
                        m_sel_anchor = m_caret;
                    } else {
                        m_caret = SingleLineTextEditCore::prev_utf8(m_text, m_caret);
                        m_sel_anchor = m_caret;
                    }
                    ensure_caret_visible();
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 39: // Right
                    if (e.modifiers.shift) {
                        m_caret = SingleLineTextEditCore::next_utf8(m_text, m_caret);
                    } else if (has_selection()) {
                        m_caret = sel_end();
                        m_sel_anchor = m_caret;
                    } else {
                        m_caret = SingleLineTextEditCore::next_utf8(m_text, m_caret);
                        m_sel_anchor = m_caret;
                    }
                    ensure_caret_visible();
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 36: { // Home -> 行首
                    size_t li = caret_line_index();
                    if (li < m_lines.size()) m_caret = std::min(m_lines[li].start, m_text.size());
                    if (!e.modifiers.shift) m_sel_anchor = m_caret;
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
                case 35: { // End -> 行尾
                    size_t li = caret_line_index();
                    if (li < m_lines.size()) m_caret = std::min(m_lines[li].end, m_text.size());
                    if (!e.modifiers.shift) m_sel_anchor = m_caret;
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
                default:
                    if (!e.text.empty()) {
                        insert_text(e.text);
                        e.stop_propagation();
                    }
                    break;
            }
        };
    }

    std::string m_text;
    std::string m_placeholder{"输入多行文本..."};
    std::string m_header;
    std::string m_ime_preedit;
    size_t m_ime_preedit_cursor{0};
    float m_width{0.0f};
    float m_height{120.0f};
    bool m_disabled{false};
    bool m_hovered{false};
    bool m_focused{false};
    bool m_dragging{false};
    bool m_caret_visible{true};
    size_t m_caret{0};
    size_t m_sel_anchor{0};
    int m_click_count{0};
    float m_focus_anim{0.0f};
    std::chrono::steady_clock::time_point m_blink_time{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point m_last_click_time{};
    std::vector<VisualLine> m_lines;
    std::vector<UndoSnapshot> m_undo_stack;
    std::vector<UndoSnapshot> m_redo_stack;
    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;
    Fn<void(const std::string&)> m_on_change;

public:
    void wrap_on_change(Fn<void(const std::string&)> extra) {
        auto prev = std::move(m_on_change);
        m_on_change = [prev = std::move(prev), extra = std::move(extra)](const std::string& v) {
            if (extra) extra(v);
            if (prev) prev(v);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 TextArea
struct TextArea : BindableMixin<TextArea, RenderTextArea> {
    std::string text{std::string{}};
    std::string placeholder{"输入多行文本..."};
    std::string header;
    float width{0.0f};
    float height{120.0f};
    bool disabled{false};
    Fn<void(const std::string&)> on_change{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderTextArea>& node, Property<std::string>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_text_value(prop.get());
    std::weak_ptr<RenderTextArea> weak = node;
    bg.subscribe(prop, [weak, guard](const std::string& v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_text_value(v);
        }
    });
    Property<std::string>* p = &prop;
    node->wrap_on_change([p, guard](const std::string& v) {
        *guard = true;
        p->set(v);
        *guard = false;
    });
}

class TextAreaWidget : public Widget {
public:
    explicit TextAreaWidget(const TextArea& desc) : m_desc(desc) {}
    explicit TextAreaWidget(TextArea&& desc) noexcept : m_desc(std::move(desc)) {}
    ~TextAreaWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTextArea>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderTextArea>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderTextArea>& node) {
        node->set_properties(
            m_desc.text, m_desc.placeholder, m_desc.header,
            m_desc.width, m_desc.height, m_desc.disabled, m_desc.on_change);
    }

    TextArea m_desc;
};

inline TextArea::operator WidgetPtr() const& {
    return make_widget<TextAreaWidget>(*this);
}
inline TextArea::operator WidgetPtr() && {
    return make_widget<TextAreaWidget>(std::move(*this));
}

} // namespace ktui
