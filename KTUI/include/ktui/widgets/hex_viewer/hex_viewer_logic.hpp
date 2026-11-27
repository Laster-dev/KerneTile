#pragma once

#include <ktui/core/strings.hpp>

#include "hex_viewer_data.hpp"
#include "hex_viewer_style.hpp"
#include "../scroll/scrollbar.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ktui {


// 十六进制 / 字节流查看与编辑格 (Hex Viewer / Byte Grid)
// - 三栏对齐矩阵：左侧 Offset | 中间动态 8/16/24/32 字节 Hex | 右侧 ASCII 对照，宽高随父容器自适应铺满
// - Hex ↔ ASCII 悬停、单击与按住鼠标左键拖动连续范围选择同步高亮（支持 Shift 扩选）
// - 键盘直接输入 0-9/A-F 就地修改字节，底部实时解码 u8 / i16 / i32 / f32 + LE/BE 端序切换 + 复制选区
class RenderHexViewer : public RenderBox {
public:
    static constexpr float kRowH = 22.0f;
    static constexpr float kHeaderH = 26.0f;
    static constexpr float kFooterH = 28.0f;

    RenderHexViewer(std::vector<uint8_t> bytes = {},
                    bool editable = true,
                    bool big_endian = false,
                    Fn<void(size_t, uint8_t)> on_byte_edited = nullptr)
        : m_bytes(std::move(bytes)),
          m_editable(editable),
          m_big_endian(big_endian),
          m_on_edited(std::move(on_byte_edited)) {
        is_focusable = true;
        if (m_bytes.empty()) {
            m_selected = m_sel_anchor = m_sel_end = -1;
        }
        setup_behavior();
    }

    [[nodiscard]] const std::vector<uint8_t>& bytes() const noexcept { return m_bytes; }
    [[nodiscard]] int selected_offset() const noexcept { return m_selected; }
    [[nodiscard]] size_t columns() const noexcept { return m_cols; }
    [[nodiscard]] std::pair<int, int> selection_range() const noexcept {
        if (m_sel_anchor < 0 || m_sel_end < 0) return {-1, -1};
        return {std::min(m_sel_anchor, m_sel_end), std::max(m_sel_anchor, m_sel_end)};
    }
    [[nodiscard]] bool is_big_endian() const noexcept { return m_big_endian; }

    void set_bytes(std::vector<uint8_t> b) {
        m_bytes = std::move(b);
        if (m_selected >= static_cast<int>(m_bytes.size())) {
            m_selected = m_sel_anchor = m_sel_end = m_bytes.empty() ? -1 : 0;
        }
        clamp_scroll();
        mark_needs_paint();
    }

    void set_big_endian(bool be) noexcept {
        if (m_big_endian != be) { m_big_endian = be; mark_needs_paint(); }
    }

    void set_on_byte_edited(Fn<void(size_t, uint8_t)> cb) { m_on_edited = std::move(cb); }

    void select_offset(int off) {
        if (off >= 0 && off < static_cast<int>(m_bytes.size())) {
            m_selected = m_sel_anchor = m_sel_end = off;
            m_nibble_high = true;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(m_constraints.min_width, 640.0f);
        float h = m_constraints.has_bounded_height() ? m_constraints.max_height : 320.0f;
        m_size = m_constraints.constrain(Vec2{std::max(360.0f, w), std::max(140.0f, h)});
        recompute_columns();
        m_endian_btn = Rect::from_xywh(m_size.x - 118.0f, m_size.y - kFooterH + 4.0f, 44.0f, 20.0f);
        m_copy_btn   = Rect::from_xywh(m_size.x - 68.0f,  m_size.y - kFooterH + 4.0f, 60.0f, 20.0f);
        clamp_scroll();
    }

    void paint(PaintContext& context) override {
        const Theme& t = ThemeContext::current();
        const auto resolved_style = HexViewerStyleResolver::resolve(t);
        const bool is_dark = t.is_dark;
        const Color accent = resolved_style.accent;
        const Color bg = is_dark ? Color{1, 1, 1, 0.025f} : Color{1, 1, 1, 0.85f};
        const Color bar_bg = is_dark ? Color{1, 1, 1, 0.04f} : Color{0, 0, 0, 0.03f};
        const Color bd = is_dark ? Color{1, 1, 1, 0.10f} : Color{0, 0, 0, 0.11f};
        const Color sep = is_dark ? Color{1, 1, 1, 0.07f} : Color{0, 0, 0, 0.07f};

        context.draw_rect(bounds(), bg, 6.0f, 0.0f);
        context.push_clip(bounds(), 6.0f);

        // 1. 顶部列头
        context.draw_rect(Rect::from_xywh(0.0f, 0.0f, m_size.x, kHeaderH), bar_bg, 0.0f, 0.0f);
        context.draw_text("Offset", PointF{kOffX, 6.0f}, 10.5f, resolved_style.text_secondary, FontWeight::SemiBold);
        for (size_t c = 0; c < m_cols; ++c) {
            char hb[4];
            std::snprintf(hb, sizeof(hb), "%02X", static_cast<unsigned>(c));
            context.draw_text(hb, PointF{hex_col_x(c) + 2.0f, 6.0f}, 10.5f, resolved_style.text_secondary, FontWeight::SemiBold);
        }
        context.draw_text("ASCII", PointF{m_ascii_x, 6.0f}, 10.5f, resolved_style.text_secondary, FontWeight::SemiBold);
        context.draw_rect(Rect::from_xywh(0.0f, kHeaderH - 1.0f, m_size.x, 1.0f), sep, 0.0f, 0.0f);

        // 2. 字节矩阵区
        Rect body = Rect::from_xywh(0.0f, kHeaderH, m_size.x, std::max(0.0f, m_size.y - kHeaderH - kFooterH));
        context.push_clip(body, 0.0f);

        auto [sel_lo, sel_hi] = selection_range();
        const size_t total_rows = (m_bytes.size() + m_cols - 1) / m_cols;
        const size_t first_row = static_cast<size_t>(std::max(0.0f, std::floor(m_scroll_y / kRowH)));
        const size_t last_row = std::min(total_rows, first_row + static_cast<size_t>(std::ceil(body.height() / kRowH)) + 2u);

        for (size_t r = first_row; r < last_row; ++r) {
            float ry = std::round(kHeaderH + static_cast<float>(r) * kRowH - m_scroll_y);
            if ((r & 1u) != 0) {
                context.draw_rect(Rect::from_xywh(0.0f, ry, m_size.x, kRowH),
                                  is_dark ? Color{1, 1, 1, 0.016f} : Color{0, 0, 0, 0.016f}, 0.0f, 0.0f);
            }

            char off_buf[16];
            std::snprintf(off_buf, sizeof(off_buf), "0x%04X", static_cast<unsigned>(r * m_cols));
            context.draw_text(off_buf, PointF{kOffX, ry + 4.0f}, 11.5f, resolved_style.text_secondary);

            // 2a. 连续选区合并绘制 (Hex 区与 ASCII 区各自绘制整段无缝高亮带，仅外露角使用圆角)
            const int row_start_idx = static_cast<int>(r * m_cols);
            const int row_end_idx = std::min(static_cast<int>(m_bytes.size()) - 1, row_start_idx + static_cast<int>(m_cols) - 1);
            if (sel_lo >= 0 && sel_hi >= row_start_idx && sel_lo <= row_end_idx) {
                const int lo_in_row = std::max(sel_lo, row_start_idx);
                const int hi_in_row = std::min(sel_hi, row_end_idx);
                const size_t c0 = static_cast<size_t>(lo_in_row - row_start_idx);
                const size_t c1 = static_cast<size_t>(hi_in_row - row_start_idx);

                auto is_selected_idx = [&](int idx) noexcept -> bool {
                    return idx >= sel_lo && idx <= sel_hi && idx >= 0 && idx < static_cast<int>(m_bytes.size());
                };
                const bool above_c0 = is_selected_idx(lo_in_row - static_cast<int>(m_cols));
                const bool above_c1 = is_selected_idx(hi_in_row - static_cast<int>(m_cols));
                const bool below_c0 = is_selected_idx(lo_in_row + static_cast<int>(m_cols));
                const bool below_c1 = is_selected_idx(hi_in_row + static_cast<int>(m_cols));

                const float r_tl = above_c0 ? 0.0f : 4.0f;
                const float r_tr = above_c1 ? 0.0f : 4.0f;
                const float r_br = below_c1 ? 0.0f : 4.0f;
                const float r_bl = below_c0 ? 0.0f : 4.0f;
                const Color band_fill = Color{accent.r, accent.g, accent.b, is_dark ? 0.22f : 0.17f};

                // Hex 连续选区带（平滑跨字节、跨 8 字节组间距、跨行无缝拼接）
                const float hx0 = hex_col_x(c0) - 2.0f;
                const float hx1 = hex_col_x(c1) + m_cell_w - 2.0f;
                context.draw_rounded_rect(Rect::from_xywh(hx0, ry, std::max(0.0f, hx1 - hx0), kRowH),
                                          band_fill, r_tl, r_tr, r_br, r_bl);

                // ASCII 连续选区带
                const float ax0 = m_ascii_x + static_cast<float>(c0) * m_asc_w - 1.5f;
                const float ax1 = m_ascii_x + static_cast<float>(c1 + 1) * m_asc_w + 0.5f;
                context.draw_rounded_rect(Rect::from_xywh(ax0, ry, std::max(0.0f, ax1 - ax0), kRowH),
                                          band_fill, r_tl, r_tr, r_br, r_bl);
            }

            // 2b. 逐字节绘制光标焦点框、悬停态与文字
            for (size_t c = 0; c < m_cols; ++c) {
                size_t idx = r * m_cols + c;
                if (idx >= m_bytes.size()) break;
                uint8_t b = m_bytes[idx];
                const int iidx = static_cast<int>(idx);
                const bool in_sel = (sel_lo >= 0 && iidx >= sel_lo && iidx <= sel_hi);
                const bool is_cur = (iidx == m_selected);
                const bool is_hov = (iidx == m_hovered);

                float cx = hex_col_x(c);
                Rect hr = Rect::from_xywh(cx - 1.0f, ry + 1.0f, m_cell_w - 2.0f, kRowH - 2.0f);
                Rect ar = Rect::from_xywh(m_ascii_x + static_cast<float>(c) * m_asc_w - 1.0f, ry + 1.0f, m_asc_w + 1.0f, kRowH - 2.0f);

                if (is_cur) {
                    Color cur_fill = Color{accent.r, accent.g, accent.b, is_dark ? 0.24f : 0.18f};
                    context.draw_rect(hr, cur_fill, 3.0f, 0.0f);
                    context.draw_rect(hr, accent, 3.0f, 1.0f);
                    context.draw_rect(ar, cur_fill, 2.0f, 0.0f);
                } else if (!in_sel && is_hov) {
                    Color hc = is_dark ? Color{1, 1, 1, 0.10f} : Color{0, 0, 0, 0.08f};
                    context.draw_rect(hr, hc, 3.0f, 0.0f);
                    context.draw_rect(ar, hc, 2.0f, 0.0f);
                }

                char hb[4];
                std::snprintf(hb, sizeof(hb), "%02X", static_cast<unsigned>(b));
                Color hcol = in_sel ? accent : (b == 0 ? resolved_style.text_secondary : resolved_style.text_primary);
                context.draw_text(hb, PointF{cx + 2.0f, ry + 4.0f}, 11.5f, hcol, in_sel ? FontWeight::SemiBold : FontWeight::Regular);

                char ch = (b >= 32 && b <= 126) ? static_cast<char>(b) : '.';
                std::string as(1, ch);
                Color acol = in_sel ? accent : ((b >= 32 && b <= 126) ? resolved_style.text_primary : resolved_style.text_secondary);
                context.draw_text(as, PointF{m_ascii_x + static_cast<float>(c) * m_asc_w, ry + 4.0f}, 11.5f, acol);
            }
        }

        // 统一复用 WinUIScrollBar 控件 (瞬时零动画直连滚动，不自绘滚动条)
        if (m_scrollbar.update_animations(m_viewport_hovered)) mark_needs_paint();
        float content_h = static_cast<float>(total_rows) * kRowH;
        m_scrollbar.paint(context, body, content_h, m_scroll_y, t);

        context.pop_clip();

        // 3. 底部实时类型解码栏 + LE/BE 端序切换 + 复制选区/全部
        float fy = m_size.y - kFooterH;
        context.draw_rect(Rect::from_xywh(0.0f, fy, m_size.x, kFooterH), bar_bg, 0.0f, 0.0f);
        context.draw_rect(Rect::from_xywh(0.0f, fy, m_size.x, 1.0f), sep, 0.0f, 0.0f);

        std::string decode_str = format_decoded_inspector();
        context.push_clip(Rect::from_xywh(10.0f, fy, std::max(0.0f, m_endian_btn.min_x - 16.0f), kFooterH), 0.0f);
        context.draw_text(decode_str, PointF{10.0f, fy + 7.0f}, 11.0f, resolved_style.text_primary, FontWeight::Medium);
        context.pop_clip();

        context.draw_rect(m_endian_btn, Color{accent.r, accent.g, accent.b, is_dark ? 0.20f : 0.14f}, 4.0f, 0.0f);
        std::string elbl = m_big_endian ? "BE" : "LE";
        Vec2 es = TextMeasure::measure(elbl, 10.5f, FontWeight::SemiBold);
        context.draw_text(elbl, PointF{std::round(m_endian_btn.min_x + (m_endian_btn.width() - es.x) * 0.5f),
                                       std::round(m_endian_btn.min_y + (m_endian_btn.height() - es.y) * 0.5f)},
                          10.5f, accent, FontWeight::SemiBold);

        const bool multi_sel = (sel_lo >= 0 && sel_hi > sel_lo);
        const std::string copy_lbl = multi_sel ? FrameworkStrings::current().copy_selection : FrameworkStrings::current().copy;
        context.draw_rect(m_copy_btn, is_dark ? Color{1, 1, 1, 0.08f} : Color{0, 0, 0, 0.06f}, 4.0f, 0.0f);
        Vec2 cs = TextMeasure::measure(copy_lbl, 10.5f, FontWeight::Medium);
        context.draw_text(copy_lbl, PointF{std::round(m_copy_btn.min_x + (m_copy_btn.width() - cs.x) * 0.5f),
                                           std::round(m_copy_btn.min_y + (m_copy_btn.height() - cs.y) * 0.5f)},
                          10.5f, resolved_style.text_secondary, FontWeight::Medium);

        context.pop_clip();
        context.draw_rect(bounds(), bd, 6.0f, 1.0f);
    }

private:
    static constexpr float kOffX = 12.0f;
    static constexpr float kHexX = 76.0f;
    static constexpr float kGroupGap = 10.0f;

    void recompute_columns() noexcept {
        // 按 8 字节步进动态计算每行列数（不设 32 列上限），并均衡填充整行宽度，最大化无留白
        const float avail = std::max(260.0f, m_size.x - kHexX - 22.0f - 18.0f);
        // 每 8 字节最小需求: 8 * (22 + 9) + 10(group_gap) = 258px
        size_t groups_cnt = std::max<size_t>(1, static_cast<size_t>(std::floor((avail + kGroupGap) / (8.0f * (22.0f + 9.0f) + kGroupGap))));
        m_cols = groups_cnt * 8;

        const size_t gap_cnt = (groups_cnt > 0) ? (groups_cnt - 1) : 0;
        const float base_hex = static_cast<float>(m_cols) * 22.0f + static_cast<float>(gap_cnt) * kGroupGap;
        const float base_asc = static_cast<float>(m_cols) * 9.0f;
        const float slack = std::max(0.0f, avail - base_hex - base_asc);

        // 将剩余水平余量按 2:1 比例柔和分配到 Hex 单元与 ASCII 字符宽度，实现 100% 满宽对齐
        m_cell_w = 22.0f + (slack * 0.66f) / static_cast<float>(m_cols);
        m_asc_w  = 9.0f  + (slack * 0.34f) / static_cast<float>(m_cols);

        float hex_w = static_cast<float>(m_cols) * m_cell_w + static_cast<float>(gap_cnt) * kGroupGap;
        m_ascii_x = kHexX + hex_w + 22.0f;
    }

    [[nodiscard]] float hex_col_x(size_t c) const noexcept {
        return kHexX + static_cast<float>(c) * m_cell_w + static_cast<float>(c / 8) * kGroupGap;
    }

    [[nodiscard]] std::string format_decoded_inspector() const {
        auto [sel_lo, sel_hi] = selection_range();
        if (sel_lo < 0 || sel_lo >= static_cast<int>(m_bytes.size())) {
            return FrameworkStrings::current().hex_viewer_hint;
        }
        size_t i = static_cast<size_t>(sel_lo);
        char buf[192];
        int n = 0;
        if (sel_hi > sel_lo) {
            n += std::snprintf(buf, sizeof(buf), "Sel 0x%02X..0x%02X (%dB) | ",
                               static_cast<unsigned>(sel_lo), static_cast<unsigned>(sel_hi), (sel_hi - sel_lo + 1));
        } else {
            n += std::snprintf(buf, sizeof(buf), "0x%04X | ", static_cast<unsigned>(i));
        }
        uint8_t u8 = m_bytes[i];
        n += std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "u8:%u", static_cast<unsigned>(u8));
        if (i + 2 <= m_bytes.size()) {
            uint16_t raw16 = m_big_endian
                ? static_cast<uint16_t>((m_bytes[i] << 8) | m_bytes[i + 1])
                : static_cast<uint16_t>(m_bytes[i] | (m_bytes[i + 1] << 8));
            int16_t i16 = static_cast<int16_t>(raw16);
            n += std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), " | i16:%d", static_cast<int>(i16));
        }
        if (i + 4 <= m_bytes.size()) {
            uint32_t raw32 = m_big_endian
                ? ((uint32_t(m_bytes[i]) << 24) | (uint32_t(m_bytes[i + 1]) << 16) | (uint32_t(m_bytes[i + 2]) << 8) | uint32_t(m_bytes[i + 3]))
                : (uint32_t(m_bytes[i]) | (uint32_t(m_bytes[i + 1]) << 8) | (uint32_t(m_bytes[i + 2]) << 16) | (uint32_t(m_bytes[i + 3]) << 24));
            int32_t i32 = static_cast<int32_t>(raw32);
            float f32 = 0.0f;
            std::memcpy(&f32, &raw32, sizeof(float));
            std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), " | i32:%d | f32:%.2g", i32, static_cast<double>(f32));
        }
        return buf;
    }

    [[nodiscard]] int hit_byte(PointF p, bool clamp_for_drag = false) const noexcept {
        if (m_bytes.empty()) return -1;
        if (!clamp_for_drag && (p.y < kHeaderH || p.y >= m_size.y - kFooterH)) return -1;

        float py = clamp_for_drag ? std::clamp(p.y, kHeaderH, std::max(kHeaderH, m_size.y - kFooterH - 1.0f)) : p.y;
        int row = static_cast<int>(std::floor((py - kHeaderH + m_scroll_y) / kRowH));
        int max_row = static_cast<int>((m_bytes.size() - 1) / m_cols);
        if (clamp_for_drag) row = std::clamp(row, 0, max_row);
        else if (row < 0 || row > max_row) return -1;

        int col = -1;
        if (p.x >= m_ascii_x - 4.0f) {
            col = static_cast<int>(std::floor((p.x - m_ascii_x) / m_asc_w));
            if (clamp_for_drag) col = std::clamp(col, 0, static_cast<int>(m_cols) - 1);
        } else if (p.x >= kHexX || clamp_for_drag) {
            // 逐列精确匹配（兼容多组 8 字节分组间隙）
            for (size_t c = 0; c < m_cols; ++c) {
                float cx = hex_col_x(c);
                if (p.x >= cx - 1.0f && p.x < cx + m_cell_w) { col = static_cast<int>(c); break; }
            }
            if (col < 0 && clamp_for_drag) {
                if (p.x < kHexX) col = 0;
                else {
                    col = static_cast<int>(m_cols) - 1;
                    for (size_t c = 0; c < m_cols; ++c) {
                        if (p.x < hex_col_x(c) + m_cell_w) { col = static_cast<int>(c); break; }
                    }
                }
            }
        }
        if (col < 0 || col >= static_cast<int>(m_cols)) return -1;
        int idx = row * static_cast<int>(m_cols) + col;
        if (clamp_for_drag) return std::clamp(idx, 0, static_cast<int>(m_bytes.size()) - 1);
        return (idx >= 0 && idx < static_cast<int>(m_bytes.size())) ? idx : -1;
    }

    [[nodiscard]] float max_scroll() const noexcept {
        size_t total_rows = (m_bytes.size() + m_cols - 1) / m_cols;
        float view_h = std::max(1.0f, m_size.y - kHeaderH - kFooterH);
        return std::max(0.0f, static_cast<float>(total_rows) * kRowH - view_h);
    }

    void clamp_scroll() noexcept {
        m_scroll_y = std::clamp(m_scroll_y, 0.0f, max_scroll());
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            m_viewport_hovered = true;
            mark_needs_paint();
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_scrollbar.handle_pointer_exit();
            if (m_hovered != -1) m_hovered = -1;
            mark_needs_paint();
        };
        on_pointer_move = [this](PointerEvent& ev) {
            Rect body = Rect::from_xywh(0.0f, kHeaderH, m_size.x, std::max(0.0f, m_size.y - kHeaderH - kFooterH));
            float content_h = static_cast<float>((m_bytes.size() + m_cols - 1) / m_cols) * kRowH;
            if (ev.type == PlatformEventType::PointerScroll) {
                // 零动画瞬时滚动 (高性能字节视图)
                m_scroll_y -= ev.delta.y * 28.0f;
                clamp_scroll();
                m_scrollbar.notify_activity();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            bool sb_consumed = false;
            if (m_scrollbar.handle_pointer_move_direct(ev.local_position, body, content_h, m_scroll_y, max_scroll(), sb_consumed)) {
                mark_needs_paint();
            }
            if (sb_consumed) {
                ev.stop_propagation();
                return;
            }
            if (m_touch_dragging) {
                m_scroll_y -= (ev.local_position.y - m_touch_prev_y);
                m_touch_prev_y = ev.local_position.y;
                clamp_scroll();
                m_scrollbar.notify_activity();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (m_mouse_selecting) {
                int idx = hit_byte(ev.local_position, true);
                if (idx >= 0 && idx != m_sel_end) {
                    m_sel_end = m_selected = idx;
                    m_nibble_high = true;
                    mark_needs_paint();
                }
                ev.stop_propagation();
                return;
            }
            int h = hit_byte(ev.local_position, false);
            if (h != m_hovered) { m_hovered = h; mark_needs_paint(); }
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            Rect body = Rect::from_xywh(0.0f, kHeaderH, m_size.x, std::max(0.0f, m_size.y - kHeaderH - kFooterH));
            float content_h = static_cast<float>((m_bytes.size() + m_cols - 1) / m_cols) * kRowH;
            if (m_scrollbar.handle_pointer_down_direct(ev.local_position, body, content_h, m_scroll_y, max_scroll(), kRowH * 3.0f)) {
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (ev.device_kind == PointerDeviceKind::Touch) {
                m_touch_dragging = true;
                m_touch_prev_y = ev.local_position.y;
            }
            Rect endian_hit = ev.is_touch() ? touch::expand_hit_rect(m_endian_btn, 40.0f) : m_endian_btn;
            Rect copy_hit = ev.is_touch() ? touch::expand_hit_rect(m_copy_btn, 40.0f) : m_copy_btn;
            if (endian_hit.contains(ev.local_position)) {
                m_big_endian = !m_big_endian;
                m_touch_dragging = false;
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (copy_hit.contains(ev.local_position)) {
                m_touch_dragging = false;
                auto [lo, hi] = selection_range();
                size_t start_i = (lo >= 0 && hi > lo) ? static_cast<size_t>(lo) : 0;
                size_t end_i   = (lo >= 0 && hi > lo) ? static_cast<size_t>(hi + 1) : m_bytes.size();
                std::string hex;
                for (size_t i = start_i; i < end_i && i < m_bytes.size(); ++i) {
                    char b[4];
                    std::snprintf(b, sizeof(b), (i > start_i) ? " %02X" : "%02X", static_cast<unsigned>(m_bytes[i]));
                    hex += b;
                }
                Clipboard::set_text(hex);
                ev.stop_propagation();
                return;
            }
            int idx = hit_byte(ev.local_position, false);
            if (idx >= 0) {
                if (ev.modifiers.shift && m_sel_anchor >= 0) {
                    m_sel_end = m_selected = idx;
                } else {
                    m_sel_anchor = m_sel_end = m_selected = idx;
                }
                m_nibble_high = true;
                if (ev.device_kind != PointerDeviceKind::Touch) {
                    m_mouse_selecting = true;
                }
                mark_needs_paint();
                ev.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent&) {
            if (m_scrollbar.handle_pointer_up()) mark_needs_paint();
            m_touch_dragging = false;
            m_mouse_selecting = false;
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_touch_dragging = false;
            m_mouse_selecting = false;
            m_hovered = -1;
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };

        on_key = [this](KeyEvent& ev) {
            if (!m_editable || m_selected < 0 || m_selected >= static_cast<int>(m_bytes.size())) return;
            if (ev.type == PlatformEventType::ImeCommit && !ev.text.empty()) {
                char c = ev.text[0];
                int nib = -1;
                if (c >= '0' && c <= '9') nib = c - '0';
                else if (c >= 'a' && c <= 'f') nib = 10 + (c - 'a');
                else if (c >= 'A' && c <= 'F') nib = 10 + (c - 'A');
                if (nib >= 0) {
                    size_t idx = static_cast<size_t>(m_selected);
                    if (m_nibble_high) {
                        m_bytes[idx] = static_cast<uint8_t>((nib << 4) | (m_bytes[idx] & 0x0F));
                        m_nibble_high = false;
                    } else {
                        m_bytes[idx] = static_cast<uint8_t>((m_bytes[idx] & 0xF0) | nib);
                        m_nibble_high = true;
                        if (m_selected + 1 < static_cast<int>(m_bytes.size())) {
                            ++m_selected;
                            m_sel_anchor = m_sel_end = m_selected;
                        }
                    }
                    if (m_on_edited) m_on_edited(idx, m_bytes[idx]);
                    mark_needs_paint();
                    ev.stop_propagation();
                }
                return;
            }
            if (ev.type != PlatformEventType::KeyDown) return;
            auto move_sel = [&](int next) {
                next = std::clamp(next, 0, static_cast<int>(m_bytes.size()) - 1);
                m_selected = m_sel_end = next;
                if (!ev.modifiers.shift) m_sel_anchor = next;
                m_nibble_high = true;
                mark_needs_paint();
                ev.stop_propagation();
            };
            if (ev.key_code == 0x25 && m_selected > 0) move_sel(m_selected - 1);
            else if (ev.key_code == 0x27 && m_selected + 1 < static_cast<int>(m_bytes.size())) move_sel(m_selected + 1);
            else if (ev.key_code == 0x26 && m_selected >= static_cast<int>(m_cols)) move_sel(m_selected - static_cast<int>(m_cols));
            else if (ev.key_code == 0x28 && m_selected + static_cast<int>(m_cols) < static_cast<int>(m_bytes.size())) move_sel(m_selected + static_cast<int>(m_cols));
        };
    }

    std::vector<uint8_t> m_bytes;
    bool m_editable{true};
    bool m_big_endian{false};
    Fn<void(size_t, uint8_t)> m_on_edited;

    size_t m_cols{16};
    float m_cell_w{22.0f};
    float m_asc_w{9.0f};
    float m_ascii_x{440.0f};

    int m_selected{0};
    int m_sel_anchor{0};
    int m_sel_end{0};
    int m_hovered{-1};
    bool m_nibble_high{true};
    bool m_mouse_selecting{false};
    bool m_viewport_hovered{false};
    WinUIScrollBar m_scrollbar;
    float m_scroll_y{0.0f};
    bool m_touch_dragging{false};
    float m_touch_prev_y{0.0f};
    Rect m_endian_btn{};
    Rect m_copy_btn{};
};

struct HexViewer : BindableMixin<HexViewer, RenderHexViewer> {
    std::vector<uint8_t> bytes{};
    bool editable{true};
    bool big_endian{false};
    Fn<void(size_t, uint8_t)> on_byte_edited{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class HexViewerWidget : public Widget {
public:
    explicit HexViewerWidget(const HexViewer& desc) : m_desc(desc) {}
    explicit HexViewerWidget(HexViewer&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderHexViewer>(m_desc.bytes, m_desc.editable, m_desc.big_endian, m_desc.on_byte_edited);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderHexViewer>(node)) {
            n->set_on_byte_edited(m_desc.on_byte_edited);
        }
    }
private:
    HexViewer m_desc;
};

inline HexViewer::operator WidgetPtr() const& { return make_widget<HexViewerWidget>(*this); }
inline HexViewer::operator WidgetPtr() && { return make_widget<HexViewerWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
