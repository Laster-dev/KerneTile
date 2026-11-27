#pragma once

namespace ktui {

inline void RenderTerminal::paint(PaintContext& ctx) {
    drain_rx_queue();

    const Theme& theme = ThemeContext::current();
    const auto resolved_style = TerminalStyleResolver::resolve(theme);
    const Rect b = bounds();
    const Color bg = m_scheme.background;
    const Color bd = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.11f} : Color{0.0f, 0.0f, 0.0f, 0.14f};

    // 1. 终端主背景与圆角裁剪
    ctx.draw_rect(b, bg, 6.0f, 0.0f);
    ctx.push_clip(b, 6.0f);

    const int total_r = m_buffer.total_rows();
    const int first_r = first_visible_row();
    const int vis_rows = m_buffer.rows();
    const int cols = m_buffer.cols();

    TerminalCoord sel_a = m_sel_start;
    TerminalCoord sel_b = m_sel_end;
    if (m_has_selection && sel_b < sel_a) std::swap(sel_a, sel_b);

    // 2. 绘制所有可见行（背景批处理合并 + 选区/搜索高亮 + 文本与装饰线）
    for (int vr = 0; vr < vis_rows; ++vr) {
        int abs_r = first_r + vr;
        if (abs_r < 0 || abs_r >= total_r) continue;
        const auto& row = m_buffer.row_at(abs_r);
        const float y = kPadY + static_cast<float>(vr) * m_cell_h;
        const int row_cols = std::min(cols, static_cast<int>(row.cells.size()));

        // 2a. 合并连续同色背景格，大幅减少 GPU 图元数
        int run_start = -1;
        Color run_col{};
        auto flush_bg = [&](int end_col) {
            if (run_start >= 0 && end_col > run_start) {
                float rx = kPadX + static_cast<float>(run_start) * m_cell_w;
                float rw = static_cast<float>(end_col - run_start) * m_cell_w;
                ctx.draw_rect(Rect::from_xywh(rx, y, rw + 0.5f, m_cell_h + 0.5f), run_col, 0.0f, 0.0f);
            }
            run_start = -1;
        };

        for (int c = 0; c < row_cols; ++c) {
            const auto& cell = row.cells[static_cast<size_t>(c)];
            bool inv = (cell.attrs & TerminalAttr::Inverse) != 0;
            bool has_custom_bg = inv || (cell.bg.kind != TerminalColor::Kind::Default);
            if (!has_custom_bg) {
                flush_bg(c);
                continue;
            }
            Color cbg = inv ? m_scheme.resolve(cell.fg, true, (cell.attrs & TerminalAttr::Bold) != 0)
                            : m_scheme.resolve(cell.bg, false, false);
            if (run_start >= 0 &&
                std::abs(cbg.r - run_col.r) < 0.002f &&
                std::abs(cbg.g - run_col.g) < 0.002f &&
                std::abs(cbg.b - run_col.b) < 0.002f &&
                std::abs(cbg.a - run_col.a) < 0.002f) {
                continue;
            }
            flush_bg(c);
            run_start = c;
            run_col = cbg;
        }
        flush_bg(row_cols);

        // 2b. 搜索命中高亮 (Windows Terminal 风格：普通命中暗黄，当前命中亮橙)
        if (m_find_open && !m_find_matches.empty()) {
            for (size_t mi = 0; mi < m_find_matches.size(); ++mi) {
                const auto& m = m_find_matches[mi];
                if (m.row != abs_r) continue;
                bool is_cur = (static_cast<int>(mi) == m_find_idx);
                Color hc = is_cur ? Color{1.0f, 0.55f, 0.0f, 0.55f} : Color{1.0f, 0.85f, 0.20f, 0.30f};
                float mx = kPadX + static_cast<float>(m.col_start) * m_cell_w;
                float mw = static_cast<float>(std::max(1, m.col_end - m.col_start)) * m_cell_w;
                ctx.draw_rect(Rect::from_xywh(mx, y, mw, m_cell_h), hc, 2.0f, 0.0f);
            }
        }

        // 2c. 选区高亮 (支持线性选区与 Alt 块选区，宽字符自动整字对齐绝不选半个汉字)
        if (m_has_selection && abs_r >= sel_a.row && abs_r <= sel_b.row) {
            int sc0 = m_sel_block ? std::min(sel_a.col, sel_b.col) : ((abs_r == sel_a.row) ? sel_a.col : 0);
            int sc1 = m_sel_block ? std::max(sel_a.col, sel_b.col) : ((abs_r == sel_b.row) ? sel_b.col : row_cols - 1);
            sc0 = std::clamp(sc0, 0, row_cols - 1);
            sc1 = std::clamp(sc1, 0, row_cols - 1);
            if (row.cells[static_cast<size_t>(sc0)].width == 0 && sc0 > 0) --sc0;
            if (row.cells[static_cast<size_t>(sc1)].width == 2 && sc1 + 1 < row_cols) ++sc1;
            if (sc1 >= sc0) {
                float sx = kPadX + static_cast<float>(sc0) * m_cell_w;
                float sw = static_cast<float>(sc1 - sc0 + 1) * m_cell_w;
                ctx.draw_rect(Rect::from_xywh(sx, y, sw, m_cell_h), m_scheme.selection_bg, 0.0f, 0.0f);
            }
        }

        // 2d. 字符与装饰线绘制 (真正等宽字体 Cascadia Mono / Consolas + 连续 ASCII 同样式合批，极速且零字距撕裂)
        int c = 0;
        while (c < row_cols) {
            const auto& cell = row.cells[static_cast<size_t>(c)];
            if (cell.width == 0 || (cell.attrs & TerminalAttr::Invisible) != 0) {
                ++c;
                continue;
            }

            bool inv = (cell.attrs & TerminalAttr::Inverse) != 0;
            bool bold = (cell.attrs & TerminalAttr::Bold) != 0;
            bool dim = (cell.attrs & TerminalAttr::Dim) != 0;
            bool italic = (cell.attrs & TerminalAttr::Italic) != 0;
            Color fg = inv ? m_scheme.resolve(cell.bg, false, false)
                           : m_scheme.resolve(cell.fg, true, bold);
            if (dim) fg.a *= 0.55f;

            // 窄 ASCII 字符与同样式连续合并为一个 draw_text 批次（完美复用等宽字体自带精确步进）
            if (cell.width == 1 && cell.codepoint >= 0x20u && cell.codepoint < 0x7Fu) {
                int start_c = c;
                std::string run;
                while (c < row_cols) {
                    const auto& nc = row.cells[static_cast<size_t>(c)];
                    if (nc.width != 1 || nc.codepoint < 0x20u || nc.codepoint >= 0x7Fu) break;
                    if (nc.fg != cell.fg || nc.bg != cell.bg || nc.attrs != cell.attrs) break;
                    run.push_back(static_cast<char>(nc.codepoint));
                    ++c;
                }
                float rx = kPadX + static_cast<float>(start_c) * m_cell_w;
                float rw = static_cast<float>(c - start_c) * m_cell_w;
                bool has_non_space = false;
                for (char ch : run) {
                    if (ch != ' ') { has_non_space = true; break; }
                }
                if (has_non_space) {
                    ctx.draw_text(run, PointF{std::round(rx), std::round(y + m_text_y_off)},
                                  m_font_size, fg, bold ? FontWeight::Bold : FontWeight::Regular,
                                  italic, m_font_family);
                }
                if ((cell.attrs & (TerminalAttr::Underline | TerminalAttr::Hyperlink)) != 0) {
                    ctx.draw_rect(Rect::from_xywh(rx, y + m_cell_h - 2.0f, rw, 1.0f), fg, 0.0f, 0.0f);
                }
                if ((cell.attrs & TerminalAttr::DoubleUnderline) != 0) {
                    ctx.draw_rect(Rect::from_xywh(rx, y + m_cell_h - 3.5f, rw, 1.0f), fg, 0.0f, 0.0f);
                    ctx.draw_rect(Rect::from_xywh(rx, y + m_cell_h - 1.5f, rw, 1.0f), fg, 0.0f, 0.0f);
                }
                if ((cell.attrs & TerminalAttr::Strikethrough) != 0) {
                    ctx.draw_rect(Rect::from_xywh(rx, y + m_cell_h * 0.52f, rw, 1.0f), fg, 0.0f, 0.0f);
                }
                if ((cell.attrs & TerminalAttr::Overline) != 0) {
                    ctx.draw_rect(Rect::from_xywh(rx, y + 1.0f, rw, 1.0f), fg, 0.0f, 0.0f);
                }
                continue;
            }

            // 宽字符 (CJK/Emoji) 或非 ASCII 符号单格精准定位绘制
            float x = kPadX + static_cast<float>(c) * m_cell_w;
            float cw = static_cast<float>(cell.width) * m_cell_w;
            if (cell.codepoint != ' ' && cell.codepoint != 0) {
                std::string utf8;
                TerminalBuffer::append_utf8(utf8, cell.codepoint);
                ctx.draw_text(utf8, PointF{std::round(x), std::round(y + m_text_y_off)},
                              m_font_size, fg, bold ? FontWeight::Bold : FontWeight::Regular,
                              italic, m_font_family);
            }
            if ((cell.attrs & (TerminalAttr::Underline | TerminalAttr::Hyperlink)) != 0) {
                ctx.draw_rect(Rect::from_xywh(x, y + m_cell_h - 2.0f, cw, 1.0f), fg, 0.0f, 0.0f);
            }
            if ((cell.attrs & TerminalAttr::DoubleUnderline) != 0) {
                ctx.draw_rect(Rect::from_xywh(x, y + m_cell_h - 3.5f, cw, 1.0f), fg, 0.0f, 0.0f);
                ctx.draw_rect(Rect::from_xywh(x, y + m_cell_h - 1.5f, cw, 1.0f), fg, 0.0f, 0.0f);
            }
            if ((cell.attrs & TerminalAttr::Strikethrough) != 0) {
                ctx.draw_rect(Rect::from_xywh(x, y + m_cell_h * 0.52f, cw, 1.0f), fg, 0.0f, 0.0f);
            }
            if ((cell.attrs & TerminalAttr::Overline) != 0) {
                ctx.draw_rect(Rect::from_xywh(x, y + 1.0f, cw, 1.0f), fg, 0.0f, 0.0f);
            }
            ++c;
        }
    }

    // 3. 光标与 IME 组合态预编辑绘制
    const int cursor_abs_r = static_cast<int>(m_buffer.scrollback_count()) + m_buffer.cursor_row();
    if (m_buffer.cursor_visible() && cursor_abs_r >= first_r && cursor_abs_r < first_r + vis_rows) {
        float cx = kPadX + static_cast<float>(m_buffer.cursor_col()) * m_cell_w;
        float cy = kPadY + static_cast<float>(cursor_abs_r - first_r) * m_cell_h;

        if (!m_ime_preedit.empty()) {
            Vec2 psz = TextMeasure::measure(m_ime_preedit, m_font_size, FontWeight::Regular, false, m_font_family);
            Rect pr = Rect::from_xywh(cx, cy, std::max(m_cell_w, psz.x + 6.0f), m_cell_h);
            ctx.draw_rect(pr, m_scheme.background, 2.0f, 0.0f);
            ctx.draw_text(m_ime_preedit, PointF{std::round(cx + 2.0f), std::round(cy + m_text_y_off)},
                          m_font_size, m_scheme.foreground, FontWeight::Regular, false, m_font_family);
            ctx.draw_rect(Rect::from_xywh(cx, cy + m_cell_h - 2.0f, pr.width(), 1.5f), resolved_style.accent, 0.0f, 0.0f);
        } else if (!m_focused) {
            // 失焦时绘制 Windows Terminal 标志性空心光标框
            ctx.draw_rect(Rect::from_xywh(cx, cy + 1.0f, m_cell_w, m_cell_h - 2.0f),
                          Color{m_scheme.cursor_color.r, m_scheme.cursor_color.g, m_scheme.cursor_color.b, 0.55f},
                          0.0f, 1.0f);
        } else if (m_cursor_blink_on || !m_buffer.cursor_blink()) {
            TerminalCursorStyle st = m_buffer.has_custom_cursor_style() ? m_buffer.vt_cursor_style() : m_cursor_style;
            Color cc = m_scheme.cursor_color;
            switch (st) {
                case TerminalCursorStyle::Bar:
                    ctx.draw_rect(Rect::from_xywh(cx, cy + 1.0f, 2.0f, m_cell_h - 2.0f), cc, 0.5f, 0.0f);
                    break;
                case TerminalCursorStyle::Vintage:
                    ctx.draw_rect(Rect::from_xywh(cx, cy + m_cell_h - 3.0f, m_cell_w, 2.5f), cc, 0.0f, 0.0f);
                    break;
                case TerminalCursorStyle::DoubleUnderscore:
                    ctx.draw_rect(Rect::from_xywh(cx, cy + m_cell_h - 4.5f, m_cell_w, 1.5f), cc, 0.0f, 0.0f);
                    ctx.draw_rect(Rect::from_xywh(cx, cy + m_cell_h - 2.0f, m_cell_w, 1.5f), cc, 0.0f, 0.0f);
                    break;
                case TerminalCursorStyle::EmptyBox:
                    ctx.draw_rect(Rect::from_xywh(cx, cy + 1.0f, m_cell_w, m_cell_h - 2.0f), cc, 0.0f, 1.2f);
                    break;
                case TerminalCursorStyle::FilledBox:
                    ctx.draw_rect(Rect::from_xywh(cx, cy + 1.0f, m_cell_w, m_cell_h - 2.0f),
                                  Color{cc.r, cc.g, cc.b, 0.80f}, 0.0f, 0.0f);
                    break;
            }
        }
    }

    // 4. Windows Terminal OSC 9;4 顶部进度条指示器
    const TerminalProgress prog = m_buffer.progress();
    if (prog.state != TerminalProgressState::Hidden) {
        Color pc = resolved_style.accent;
        if (prog.state == TerminalProgressState::Error) pc = Color{0.90f, 0.24f, 0.24f, 1.0f};
        else if (prog.state == TerminalProgressState::Paused) pc = Color{0.96f, 0.72f, 0.14f, 1.0f};
        float frac = (prog.state == TerminalProgressState::Indeterminate) ? 0.35f : (static_cast<float>(prog.value) / 100.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, m_size.x * frac, 2.5f), pc, 0.0f, 0.0f);
    }

    // 5. 复用 WinUIScrollBar 绘制回滚历史滚动条 (严禁自绘滚动条)
    const float content_h = total_content_height();
    if (m_scrollbar.update_animations(m_scrollbar.is_hovered() || m_scrollbar.is_dragging())) {
        mark_needs_paint();
    }
    m_scrollbar.paint(ctx, b, content_h, m_scroll_y, theme);

    // 6. Ctrl+Shift+F 顶部浮动内联搜索栏 (完全复用真实 TextBox / Text / ToggleButton / Button 控件)
    if (m_find_open && m_find_bar_node) {
        Rect fbr = Rect::from_xywh(m_find_bar_node->offset().x, m_find_bar_node->offset().y,
                                   m_find_bar_node->size().x, m_find_bar_node->size().y);
        ctx.draw_elevation_shadow(fbr, 6.0f, theme.is_dark, 16.0f, 0.85f);
        PaintContext::OffsetScope scope(ctx, m_find_bar_node->offset());
        m_find_bar_node->paint(ctx);
    }

    ctx.pop_clip();

    // 7. 常规细边框 (无蓝框焦点边框，与 Windows Terminal 视觉一致)
    ctx.draw_rect(b, bd, 6.0f, 1.0f);
}

} // namespace ktui
