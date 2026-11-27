#pragma once

#include <ktui/core/strings.hpp>

#include "rich_text_block_data.hpp"
#include "rich_text_block_style.hpp"
#include "../common/fluent_chrome.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <cmath>

namespace ktui {

// WinUI 3 RichTextBlock 富文本排版渲染盒
// 支持多段落、多字号基线对齐混排、粗体/斜体/下划线/删除线/荧光高亮/内联代码胶囊、
// 可点击超链接 (Hyperlink) 以及跨行鼠标拖拽选区与 Ctrl+C / Ctrl+A 复制
class RenderRichTextBlock : public RenderBox {
public:
    RenderRichTextBlock() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::Arrow;
        setup_behavior();
    }

    ~RenderRichTextBlock() override = default;

    void set_properties(std::vector<RichTextParagraph> paragraphs,
                        std::vector<RichTextRun> flat_runs,
                        float font_size,
                        float line_height,
                        bool selectable,
                        float max_width) {
        if (!paragraphs.empty()) {
            m_paragraphs = std::move(paragraphs);
        } else if (!flat_runs.empty()) {
            m_paragraphs.clear();
            m_paragraphs.push_back(RichTextParagraph{std::move(flat_runs), 0.0f});
        } else {
            m_paragraphs.clear();
        }
        m_default_font_size = (font_size > 0.0f) ? font_size : 14.0f;
        m_line_height = std::max(1.1f, line_height);
        m_selectable = selectable;
        m_max_width = max_width;
        m_sel_anchor = m_sel_end = 0;
        m_dragging_sel = false;
        m_hovered_link = {-1, -1};
        mark_needs_layout();
    }

    [[nodiscard]] std::string plain_text() const { return m_plain_text; }

    [[nodiscard]] std::string selected_text() const {
        auto [lo, hi] = selection_bounds();
        if (hi <= lo || hi > m_plain_text.size()) return {};
        return m_plain_text.substr(lo, hi - lo);
    }

    void select_all() {
        if (!m_selectable) return;
        m_sel_anchor = 0;
        m_sel_end = m_plain_text.size();
        mark_needs_paint();
    }

    void clear_selection() {
        if (m_sel_anchor != m_sel_end) {
            m_sel_anchor = m_sel_end = 0;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float avail_w = (m_max_width > 0.0f)
            ? m_max_width
            : (m_constraints.has_bounded_width() ? m_constraints.max_width : 520.0f);
        avail_w = std::max(60.0f, m_constraints.constrain_width(avail_w));

        layout_lines(avail_w);

        float used_w = m_constraints.has_bounded_width() ? avail_w : std::min(avail_w, m_measured_w);
        float used_h = std::max(m_default_font_size * m_line_height, m_measured_h);
        set_size(m_constraints.constrain(Vec2{std::ceil(used_w), std::ceil(used_h)}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const Color sel_bg = FluentInputChrome::selection_bg(theme, m_focused);
        const Color sel_fg = FluentInputChrome::selection_text(theme);
        const Color code_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.075f} : Color{0.0f, 0.0f, 0.0f, 0.055f};
        const Color code_bd = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.12f}  : Color{0.0f, 0.0f, 0.0f, 0.10f};
        auto [sel_lo, sel_hi] = selection_bounds();

        for (const auto& line : m_lines) {
            for (const auto& seg : line.segments) {
                const auto& run = m_paragraphs[seg.para_idx].runs[seg.run_idx];
                const bool is_link = run.is_link();
                const bool is_link_hov = is_link && (m_hovered_link.first == static_cast<int>(seg.para_idx) &&
                                                     m_hovered_link.second == static_cast<int>(seg.run_idx));

                Color fg = resolve_run_color(run, theme, is_link_hov);
                FontWeight fw = effective_weight(run);
                float fsz = effective_font_size(run);

                // 基线对齐：大字号与小字号同排时按行底基线自然对齐
                float baseline_offset = line.height - seg.height;
                float draw_y = std::round(line.y + baseline_offset * 0.78f);

                // 1. 内联代码胶囊 / 荧光高亮底板
                if (run.is_code) {
                    Rect pill = Rect::from_xywh(seg.x, draw_y - 1.5f, seg.width, seg.height + 3.0f);
                    ctx.draw_rect(pill, code_bg, 4.0f, 0.0f);
                    ctx.draw_rect(pill, code_bd, 4.0f, 1.0f);
                } else if (run.highlight.r >= 0.0f) {
                    Rect hr = Rect::from_xywh(seg.x - 1.0f, draw_y - 1.0f, seg.width + 2.0f, seg.height + 2.0f);
                    ctx.draw_rect(hr, run.highlight, 3.0f, 0.0f);
                }

                float text_x = seg.x + (run.is_code ? kCodePadX : 0.0f);

                // 2. 选区高亮渲染 (支持跨段/跨 Run 精确子串高亮)
                bool has_sel = (m_selectable && sel_hi > sel_lo &&
                                seg.char_end > sel_lo && seg.char_start < sel_hi);
                if (has_sel) {
                    size_t s0 = std::max(seg.char_start, sel_lo) - seg.char_start;
                    size_t s1 = std::min(seg.char_end, sel_hi) - seg.char_start;
                    float x0 = text_x + TextMeasure::measure_width(std::string_view(seg.text.data(), s0), fsz, fw);
                    float x1 = text_x + TextMeasure::measure_width(std::string_view(seg.text.data(), s1), fsz, fw);
                    ctx.draw_rect(Rect::from_xywh(x0, draw_y - 1.0f, std::max(1.0f, x1 - x0), seg.height + 2.0f),
                                  sel_bg, 2.0f, 0.0f);

                    if (s0 > 0) {
                        ctx.draw_text(std::string_view(seg.text.data(), s0), PointF{text_x, draw_y}, fsz, fg, fw, run.italic);
                    }
                    if (s1 > s0) {
                        ctx.draw_text(std::string_view(seg.text.data() + s0, s1 - s0), PointF{x0, draw_y}, fsz, sel_fg, fw, run.italic);
                    }
                    if (s1 < seg.text.size()) {
                        ctx.draw_text(std::string_view(seg.text.data() + s1, seg.text.size() - s1), PointF{x1, draw_y}, fsz, fg, fw, run.italic);
                    }
                } else {
                    ctx.draw_text(seg.text, PointF{text_x, draw_y}, fsz, fg, fw, run.italic);
                }

                // 3. 下划线 (显式 underline 或 超链接悬停/默认下划线)
                if (run.underline || is_link_hov) {
                    float uy = std::round(draw_y + seg.height - 1.0f);
                    ctx.draw_rect(Rect::from_xywh(text_x, uy, seg.text_width, 1.0f), fg, 0.0f, 0.0f);
                }
                // 4. 删除线
                if (run.strikethrough) {
                    float sy = std::round(draw_y + seg.height * 0.52f);
                    ctx.draw_rect(Rect::from_xywh(text_x, sy, seg.text_width, 1.0f), fg, 0.0f, 0.0f);
                }
            }
        }
    }

private:
    static constexpr float kCodePadX = 5.0f;

    struct LayoutSegment {
        size_t para_idx{0};
        size_t run_idx{0};
        std::string text;
        float x{0.0f};
        float width{0.0f};
        float text_width{0.0f};
        float height{0.0f};
        size_t char_start{0};
        size_t char_end{0};
    };

    struct LayoutLine {
        float y{0.0f};
        float width{0.0f};
        float height{0.0f};
        std::vector<LayoutSegment> segments;
    };

    [[nodiscard]] float effective_font_size(const RichTextRun& r) const noexcept {
        if (r.font_size > 0.0f) return r.font_size;
        return r.is_code ? std::max(11.0f, m_default_font_size - 1.5f) : m_default_font_size;
    }

    [[nodiscard]] static FontWeight effective_weight(const RichTextRun& r) noexcept {
        return r.bold ? FontWeight::Bold : r.font_weight;
    }

    [[nodiscard]] static Color resolve_run_color(const RichTextRun& r, const Theme& theme, bool link_hovered) noexcept {
        if (r.color.r >= 0.0f) return r.color;
        if (r.is_link()) {
            Color c = theme.colors.primary;
            if (link_hovered) {
                c.r = std::min(1.0f, c.r * 1.12f);
                c.g = std::min(1.0f, c.g * 1.12f);
                c.b = std::min(1.0f, c.b * 1.12f);
            }
            return c;
        }
        return theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.94f} : Color{0.0f, 0.0f, 0.0f, 0.90f};
    }

    [[nodiscard]] std::pair<size_t, size_t> selection_bounds() const noexcept {
        return (m_sel_anchor <= m_sel_end)
            ? std::pair<size_t, size_t>{m_sel_anchor, m_sel_end}
            : std::pair<size_t, size_t>{m_sel_end, m_sel_anchor};
    }

    static size_t next_utf8_step(const std::string& s, size_t pos) noexcept {
        if (pos >= s.size()) return 0;
        unsigned char c = static_cast<unsigned char>(s[pos]);
        size_t len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
        return std::min(len, s.size() - pos);
    }

    void layout_lines(float max_w) {
        m_lines.clear();
        m_plain_text.clear();
        m_measured_w = 0.0f;
        m_measured_h = 0.0f;

        float cur_y = 0.0f;
        for (size_t pi = 0; pi < m_paragraphs.size(); ++pi) {
            if (pi > 0) m_plain_text.push_back('\n');
            const auto& para = m_paragraphs[pi];

            LayoutLine cur_line;
            cur_line.y = cur_y;
            float cur_x = 0.0f;

            auto flush_line = [&](bool empty_fallback) {
                if (cur_line.segments.empty() && !empty_fallback) return;
                float lh = cur_line.height > 0.0f ? cur_line.height : m_default_font_size;
                cur_line.height = lh;
                cur_line.width = cur_x;
                m_measured_w = std::max(m_measured_w, cur_x);
                cur_y += std::ceil(lh * m_line_height);
                m_lines.push_back(std::move(cur_line));
                cur_line = LayoutLine{};
                cur_line.y = cur_y;
                cur_x = 0.0f;
            };

            for (size_t ri = 0; ri < para.runs.size(); ++ri) {
                const auto& run = para.runs[ri];
                const float fsz = effective_font_size(run);
                const FontWeight fw = effective_weight(run);
                const float extra_pad = run.is_code ? (kCodePadX * 2.0f) : 0.0f;

                size_t pos = 0;
                while (pos < run.text.size()) {
                    if (run.text[pos] == '\n') {
                        m_plain_text.push_back('\n');
                        flush_line(true);
                        ++pos;
                        continue;
                    }

                    // 贪心打包当前行可容纳的最长 UTF-8 片段 (尽量按空格/CJK 边界换行)
                    size_t seg_start = pos;
                    size_t best_end = pos;
                    size_t last_break = pos;
                    float best_tw = 0.0f;
                    float best_th = fsz;

                    while (pos < run.text.size() && run.text[pos] != '\n') {
                        size_t step = next_utf8_step(run.text, pos);
                        size_t cand_end = pos + step;
                        Vec2 m = TextMeasure::measure(std::string_view(run.text.data() + seg_start, cand_end - seg_start), fsz, fw);
                        float cand_w = m.x + extra_pad;
                        if (cur_x + cand_w > max_w && best_end > seg_start) {
                            break;
                        }
                        if (cur_x + cand_w > max_w && cur_x > 0.0f) {
                            break;
                        }
                        best_end = cand_end;
                        best_tw = m.x;
                        best_th = std::max(m.y, fsz);
                        unsigned char ch = static_cast<unsigned char>(run.text[pos]);
                        if (ch == ' ' || ch == '\t' || ch >= 0x80) {
                            last_break = cand_end;
                        }
                        pos = cand_end;
                    }

                    if (best_end == seg_start) {
                        // 当前行放不下一个字符：先换行，若仍放不下则强制放入单字符防止死循环
                        if (cur_x > 0.0f) {
                            flush_line(false);
                            continue;
                        }
                        size_t step = next_utf8_step(run.text, seg_start);
                        best_end = seg_start + step;
                        Vec2 m = TextMeasure::measure(std::string_view(run.text.data() + seg_start, step), fsz, fw);
                        best_tw = m.x;
                        best_th = std::max(m.y, fsz);
                        pos = best_end;
                    } else if (pos < run.text.size() && run.text[pos] != '\n' && last_break > seg_start && last_break < best_end) {
                        // 在英文词边界回退换行，避免把单词从中间腰斩
                        best_end = last_break;
                        pos = best_end;
                        Vec2 m = TextMeasure::measure(std::string_view(run.text.data() + seg_start, best_end - seg_start), fsz, fw);
                        best_tw = m.x;
                        best_th = std::max(m.y, fsz);
                    }

                    LayoutSegment seg;
                    seg.para_idx = pi;
                    seg.run_idx = ri;
                    seg.text = run.text.substr(seg_start, best_end - seg_start);
                    seg.x = cur_x;
                    seg.text_width = best_tw;
                    seg.width = best_tw + extra_pad;
                    seg.height = best_th;
                    seg.char_start = m_plain_text.size();
                    m_plain_text += seg.text;
                    seg.char_end = m_plain_text.size();

                    cur_line.height = std::max(cur_line.height, seg.height);
                    cur_x += seg.width + (run.is_code ? 2.0f : 0.0f);
                    cur_line.segments.push_back(std::move(seg));

                    if (pos < run.text.size() && run.text[pos] != '\n') {
                        flush_line(false);
                    }
                }
            }
            flush_line(false);
            if (pi + 1 < m_paragraphs.size()) {
                cur_y += para.spacing_after;
            }
        }
        m_measured_h = cur_y;
    }

    std::pair<int, int> hit_link(PointF local) const noexcept {
        for (const auto& line : m_lines) {
            float lh = std::ceil(line.height * m_line_height);
            if (local.y < line.y || local.y > line.y + lh) continue;
            for (const auto& seg : line.segments) {
                if (local.x >= seg.x && local.x <= seg.x + seg.width) {
                    const auto& run = m_paragraphs[seg.para_idx].runs[seg.run_idx];
                    if (run.is_link()) {
                        return {static_cast<int>(seg.para_idx), static_cast<int>(seg.run_idx)};
                    }
                }
            }
        }
        return {-1, -1};
    }

    size_t hit_char_index(PointF local) const noexcept {
        if (m_lines.empty()) return 0;
        const LayoutLine* target_line = &m_lines.front();
        if (local.y >= m_lines.back().y) {
            target_line = &m_lines.back();
        } else {
            for (const auto& line : m_lines) {
                float lh = std::ceil(line.height * m_line_height);
                if (local.y <= line.y + lh) {
                    target_line = &line;
                    break;
                }
            }
        }
        if (target_line->segments.empty()) return 0;
        if (local.x <= target_line->segments.front().x) return target_line->segments.front().char_start;

        for (const auto& seg : target_line->segments) {
            if (local.x <= seg.x + seg.width) {
                const auto& run = m_paragraphs[seg.para_idx].runs[seg.run_idx];
                float fsz = effective_font_size(run);
                FontWeight fw = effective_weight(run);
                float rel_x = std::max(0.0f, local.x - seg.x - (run.is_code ? kCodePadX : 0.0f));
                size_t pos = 0;
                float prev_w = 0.0f;
                while (pos < seg.text.size()) {
                    size_t step = next_utf8_step(seg.text, pos);
                    float w = TextMeasure::measure_width(std::string_view(seg.text.data(), pos + step), fsz, fw);
                    if (rel_x < (prev_w + w) * 0.5f) return seg.char_start + pos;
                    prev_w = w;
                    pos += step;
                }
                return seg.char_end;
            }
        }
        return target_line->segments.back().char_end;
    }

    void setup_behavior() {
        on_focus = [this](FocusEvent& e) {
            m_focused = e.gained_focus;
            mark_needs_paint();
        };
        on_pointer_move = [this](PointerEvent& e) {
            if (m_dragging_sel && m_selectable) {
                size_t idx = hit_char_index(e.local_position);
                if (idx != m_sel_end) {
                    m_sel_end = idx;
                    mark_needs_paint();
                }
                e.stop_propagation();
                return;
            }
            auto lnk = hit_link(e.local_position);
            cursor_shape = (lnk.first >= 0) ? CursorShape::PointingHand
                                            : (m_selectable ? CursorShape::IBeam : CursorShape::Arrow);
            if (lnk != m_hovered_link) {
                m_hovered_link = lnk;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_link.first >= 0) {
                m_hovered_link = {-1, -1};
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button == PointerButton::Secondary && m_selectable) {
                Window* win = Window::active_window();
                Vec2 win_pos = e.position;
                if (win) win_pos.y += win->titlebar_height();
                auto [lo, hi] = selection_bounds();
                bool has_sel = (hi > lo);
                std::vector<MenuFlyoutItem> items = {
                    MenuFlyoutItem{
                        .label = FrameworkStrings::current().copy,
                        .shortcut = "Ctrl+C",
                        .disabled = !has_sel,
                        .on_click = [this]() {
                            std::string s = selected_text();
                            if (!s.empty()) Clipboard::set_text(s);
                        }
                    },
                    MenuFlyoutItem{
                        .label = FrameworkStrings::current().copy_all_text,
                        .disabled = m_plain_text.empty(),
                        .on_click = [this]() {
                            if (!m_plain_text.empty()) Clipboard::set_text(m_plain_text);
                        }
                    },
                    MenuFlyoutItem::separator(),
                    MenuFlyoutItem{
                        .label = FrameworkStrings::current().select_all,
                        .shortcut = "Ctrl+A",
                        .disabled = m_plain_text.empty(),
                        .on_click = [this]() { select_all(); }
                    },
                    MenuFlyoutItem{
                        .label = FrameworkStrings::current().clear_selection,
                        .disabled = !has_sel,
                        .on_click = [this]() { clear_selection(); }
                    }
                };
                MenuFlyout::show_at(std::move(items), win_pos);
                e.stop_propagation();
                return;
            }
            if (e.button != PointerButton::Primary) return;
            auto lnk = hit_link(e.local_position);
            if (lnk.first >= 0) {
                if (e.is_touch()) {
                    m_touch_pending_link = lnk;
                    m_touch_down_pos = e.local_position;
                    e.stop_propagation();
                    return;
                }
                const auto& run = m_paragraphs[static_cast<size_t>(lnk.first)].runs[static_cast<size_t>(lnk.second)];
                if (run.on_click) run.on_click();
                e.stop_propagation();
                return;
            }
            if (m_selectable && !e.is_touch()) {
                size_t idx = hit_char_index(e.local_position);
                m_sel_anchor = m_sel_end = idx;
                m_dragging_sel = true;
                mark_needs_paint();
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            m_dragging_sel = false;
            if (e.is_touch() && m_touch_pending_link.first >= 0) {
                auto pending = m_touch_pending_link;
                m_touch_pending_link = {-1, -1};
                float dx = e.local_position.x - m_touch_down_pos.x;
                float dy = e.local_position.y - m_touch_down_pos.y;
                if (std::hypot(dx, dy) <= touch::kTouchSlopPx && hit_link(e.local_position) == pending) {
                    const auto& run = m_paragraphs[static_cast<size_t>(pending.first)].runs[static_cast<size_t>(pending.second)];
                    if (run.on_click) run.on_click();
                    e.stop_propagation();
                }
            }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_dragging_sel = false;
            m_touch_pending_link = {-1, -1};
            if (m_hovered_link.first >= 0) {
                m_hovered_link = {-1, -1};
                mark_needs_paint();
            }
        };
        on_key = [this](KeyEvent& e) {
            if (!m_selectable || e.type != PlatformEventType::KeyDown || !e.modifiers.ctrl) return;
            if (e.key_code == 'C' || e.key_code == 'c') {
                std::string sel = selected_text();
                if (!sel.empty()) Clipboard::set_text(sel);
                e.stop_propagation();
            } else if (e.key_code == 'A' || e.key_code == 'a') {
                select_all();
                e.stop_propagation();
            }
        };
    }

    std::vector<RichTextParagraph> m_paragraphs;
    float m_default_font_size{14.0f};
    float m_line_height{1.45f};
    bool m_selectable{true};
    float m_max_width{0.0f};

    std::vector<LayoutLine> m_lines;
    std::string m_plain_text;
    float m_measured_w{0.0f};
    float m_measured_h{0.0f};

    bool m_focused{false};
    bool m_dragging_sel{false};
    size_t m_sel_anchor{0};
    size_t m_sel_end{0};
    std::pair<int, int> m_hovered_link{-1, -1};
    std::pair<int, int> m_touch_pending_link{-1, -1};
    PointF m_touch_down_pos{};
};

// C++20 指定初始化器声明式 WinUI 3 RichTextBlock
struct RichTextBlock : BindableMixin<RichTextBlock, RenderRichTextBlock> {
    std::vector<RichTextParagraph> paragraphs{};
    std::vector<RichTextRun> runs{}; // 单段落快捷方式
    float font_size{14.0f};
    float line_height{1.45f};
    bool is_text_selection_enabled{true};
    float max_width{0.0f};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class RichTextBlockWidget : public Widget {
public:
    explicit RichTextBlockWidget(const RichTextBlock& desc) : m_desc(desc) {}
    explicit RichTextBlockWidget(RichTextBlock&& desc) noexcept : m_desc(std::move(desc)) {}
    ~RichTextBlockWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderRichTextBlock>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderRichTextBlock>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderRichTextBlock>& node) {
        node->set_properties(m_desc.paragraphs, m_desc.runs, m_desc.font_size,
                             m_desc.line_height, m_desc.is_text_selection_enabled, m_desc.max_width);
    }

    RichTextBlock m_desc;
};

inline RichTextBlock::operator WidgetPtr() const& {
    return make_widget<RichTextBlockWidget>(*this);
}
inline RichTextBlock::operator WidgetPtr() && {
    return make_widget<RichTextBlockWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
