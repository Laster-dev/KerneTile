#pragma once

namespace ktui {

inline void TerminalBuffer::process_byte(uint8_t b) {
    // UTF-8 多字节续接
    if (m_state == ParseState::Ground && m_utf8_expected > 0) {
        if ((b & 0xC0u) == 0x80u) {
            m_utf8_cp = (m_utf8_cp << 6) | (b & 0x3Fu);
            if (--m_utf8_expected == 0) {
                put_codepoint(m_utf8_cp);
            }
            return;
        } else {
            m_utf8_expected = 0; // 非法续字节，放弃并按正常字节处理
        }
    }

    switch (m_state) {
        case ParseState::Ground:
            if (b == 0x1Bu) {
                m_state = ParseState::Escape;
                m_intermediate.clear();
            } else if (b < 0x20u || b == 0x7Fu) {
                execute_c0(b);
            } else if (b < 0x80u) {
                put_codepoint(b);
            } else if ((b & 0xE0u) == 0xC0u) {
                m_utf8_cp = b & 0x1Fu;
                m_utf8_expected = 1;
            } else if ((b & 0xF0u) == 0xE0u) {
                m_utf8_cp = b & 0x0Fu;
                m_utf8_expected = 2;
            } else if ((b & 0xF8u) == 0xF0u) {
                m_utf8_cp = b & 0x07u;
                m_utf8_expected = 3;
            }
            break;

        case ParseState::Escape:
            if (b == '[') {
                m_state = ParseState::CsiEntry;
                m_csi_buf.clear();
            } else if (b == ']') {
                m_state = ParseState::OscString;
                m_osc_buf.clear();
            } else if (b == 'P' || b == '_' || b == '^') {
                m_state = ParseState::DcsIgnore;
            } else if (b >= 0x20u && b <= 0x2Fu) {
                m_intermediate.push_back(static_cast<char>(b));
                m_state = ParseState::EscapeIntermediate;
            } else {
                dispatch_esc(static_cast<char>(b));
                m_state = ParseState::Ground;
            }
            break;

        case ParseState::EscapeIntermediate:
            if (b >= 0x20u && b <= 0x2Fu) {
                m_intermediate.push_back(static_cast<char>(b));
            } else {
                // 忽略字符集切换 ESC ( B 等
                m_state = ParseState::Ground;
            }
            break;

        case ParseState::CsiEntry:
            if (b == 0x1Bu) {
                m_state = ParseState::Escape;
                m_intermediate.clear();
            } else if (b < 0x20u) {
                execute_c0(b);
            } else if (b >= 0x40u && b <= 0x7Eu) {
                dispatch_csi(static_cast<char>(b));
                m_state = ParseState::Ground;
            } else if (m_csi_buf.size() < 128) {
                m_csi_buf.push_back(static_cast<char>(b));
            }
            break;

        case ParseState::OscString:
            if (b == 0x07u) {
                dispatch_osc();
                m_state = ParseState::Ground;
            } else if (b == 0x1Bu) {
                m_state = ParseState::OscEsc;
            } else if (m_osc_buf.size() < 1024) {
                m_osc_buf.push_back(static_cast<char>(b));
            }
            break;

        case ParseState::OscEsc:
            if (b == '\\') {
                dispatch_osc();
                m_state = ParseState::Ground;
            } else {
                m_state = ParseState::Ground;
            }
            break;

        case ParseState::DcsIgnore:
            if (b == 0x1Bu) m_state = ParseState::DcsEsc;
            break;

        case ParseState::DcsEsc:
            if (b == '\\') m_state = ParseState::Ground;
            else m_state = ParseState::DcsIgnore;
            break;
    }
}

inline void TerminalBuffer::dispatch_esc(char ch) {
    switch (ch) {
        case '7': save_cursor(); break;
        case '8': restore_cursor(); break;
        case 'D': line_feed(); break;
        case 'M': reverse_index(); break;
        case 'E': m_cursor_x = 0; line_feed(); break;
        case 'c': // RIS Full Reset
            m_fg = TerminalColor::default_color();
            m_bg = TerminalColor::default_color();
            m_attrs = TerminalAttr::None;
            m_in_alt_screen = false;
            m_scroll_top = 0;
            m_scroll_bottom = m_rows - 1;
            clear_all();
            break;
        default:
            break;
    }
}

inline void TerminalBuffer::dispatch_csi(char final_ch) {
    std::string_view raw(m_csi_buf);
    bool is_private = false;
    char space_inter = 0;
    if (!raw.empty() && (raw.front() == '?' || raw.front() == '>' || raw.front() == '=')) {
        is_private = (raw.front() == '?');
        if (raw.front() == '>' || raw.front() == '=') {
            // DA2 / XTVERSION 等忽略
            return;
        }
        raw.remove_prefix(1);
    }
    if (!raw.empty() && raw.back() == ' ') {
        space_inter = ' ';
        raw.remove_suffix(1);
    }

    // 解析参数 (兼容 ';' 和 ':' 子参数分隔，支持 38:2:r:g:b TrueColor)
    std::vector<int> params;
    if (!raw.empty()) {
        int val = 0;
        bool has_digit = false;
        for (char c : raw) {
            if (c >= '0' && c <= '9') {
                val = val * 10 + (c - '0');
                has_digit = true;
            } else if (c == ';' || c == ':') {
                params.push_back(has_digit ? val : 0);
                val = 0;
                has_digit = false;
            }
        }
        params.push_back(has_digit ? val : 0);
    }
    auto p = [&](size_t idx, int def) noexcept -> int {
        if (idx >= params.size() || params[idx] == 0) return def;
        return params[idx];
    };
    auto p_raw = [&](size_t idx, int def) noexcept -> int {
        if (idx >= params.size()) return def;
        return params[idx];
    };

    // DECSCUSR: CSI Ps SP q
    if (space_inter == ' ' && final_ch == 'q') {
        int ps = p_raw(0, 0);
        m_has_vt_cursor_style = (ps != 0);
        switch (ps) {
            case 0: m_vt_cursor_style = TerminalCursorStyle::Bar; m_cursor_blink = true; break;
            case 1: m_vt_cursor_style = TerminalCursorStyle::FilledBox; m_cursor_blink = true; break;
            case 2: m_vt_cursor_style = TerminalCursorStyle::FilledBox; m_cursor_blink = false; break;
            case 3: m_vt_cursor_style = TerminalCursorStyle::Vintage; m_cursor_blink = true; break;
            case 4: m_vt_cursor_style = TerminalCursorStyle::Vintage; m_cursor_blink = false; break;
            case 5: m_vt_cursor_style = TerminalCursorStyle::Bar; m_cursor_blink = true; break;
            case 6: m_vt_cursor_style = TerminalCursorStyle::Bar; m_cursor_blink = false; break;
            default: break;
        }
        return;
    }

    if (is_private) {
        bool set = (final_ch == 'h');
        if (final_ch == 'h' || final_ch == 'l') {
            for (int mode : params) {
                apply_dec_private_mode(mode, set);
            }
        }
        return;
    }

    auto& grid = active_grid();
    switch (final_ch) {
        case 'A': // CUU
            m_pending_wrap = false;
            m_cursor_y = std::max(m_scroll_top, m_cursor_y - p(0, 1));
            break;
        case 'B': // CUD
        case 'e': // VPR
            m_pending_wrap = false;
            m_cursor_y = std::min(m_scroll_bottom, m_cursor_y + p(0, 1));
            break;
        case 'C': // CUF
        case 'a': // HPR
            m_pending_wrap = false;
            m_cursor_x = std::min(m_cols - 1, m_cursor_x + p(0, 1));
            break;
        case 'D': // CUB
            m_pending_wrap = false;
            m_cursor_x = std::max(0, m_cursor_x - p(0, 1));
            break;
        case 'E': // CNL
            m_pending_wrap = false;
            m_cursor_x = 0;
            m_cursor_y = std::min(m_scroll_bottom, m_cursor_y + p(0, 1));
            break;
        case 'F': // CPL
            m_pending_wrap = false;
            m_cursor_x = 0;
            m_cursor_y = std::max(m_scroll_top, m_cursor_y - p(0, 1));
            break;
        case 'G': // CHA
        case '`':
            m_pending_wrap = false;
            m_cursor_x = std::clamp(p(0, 1) - 1, 0, m_cols - 1);
            break;
        case 'd': // VPA
            m_pending_wrap = false;
            m_cursor_y = std::clamp(p(0, 1) - 1, 0, m_rows - 1);
            break;
        case 'H': // CUP
        case 'f': // HVP
            m_pending_wrap = false;
            m_cursor_y = std::clamp(p(0, 1) - 1, 0, m_rows - 1);
            m_cursor_x = std::clamp(p(1, 1) - 1, 0, m_cols - 1);
            break;
        case 'J': { // ED Erase in Display
            int mode = p_raw(0, 0);
            if (mode == 0) {
                erase_line_range(m_cursor_y, m_cursor_x, m_cols);
                for (int r = m_cursor_y + 1; r < m_rows; ++r) erase_line_range(r, 0, m_cols);
            } else if (mode == 1) {
                for (int r = 0; r < m_cursor_y; ++r) erase_line_range(r, 0, m_cols);
                erase_line_range(m_cursor_y, 0, m_cursor_x + 1);
            } else if (mode == 2) {
                for (int r = 0; r < m_rows; ++r) erase_line_range(r, 0, m_cols);
            } else if (mode == 3) {
                m_scrollback.clear();
            }
            break;
        }
        case 'K': { // EL Erase in Line
            int mode = p_raw(0, 0);
            if (mode == 0) erase_line_range(m_cursor_y, m_cursor_x, m_cols);
            else if (mode == 1) erase_line_range(m_cursor_y, 0, m_cursor_x + 1);
            else if (mode == 2) erase_line_range(m_cursor_y, 0, m_cols);
            break;
        }
        case 'X': { // ECH Erase Characters
            int n = p(0, 1);
            erase_line_range(m_cursor_y, m_cursor_x, std::min(m_cols, m_cursor_x + n));
            break;
        }
        case '@': { // ICH Insert Blank Characters
            int n = std::min(p(0, 1), m_cols - m_cursor_x);
            auto& cells = grid[static_cast<size_t>(m_cursor_y)].cells;
            for (int c = m_cols - 1; c >= m_cursor_x + n; --c) {
                cells[static_cast<size_t>(c)] = cells[static_cast<size_t>(c - n)];
            }
            for (int c = m_cursor_x; c < m_cursor_x + n; ++c) {
                cells[static_cast<size_t>(c)].reset(m_fg, m_bg);
            }
            break;
        }
        case 'P': { // DCH Delete Characters
            int n = std::min(p(0, 1), m_cols - m_cursor_x);
            auto& cells = grid[static_cast<size_t>(m_cursor_y)].cells;
            for (int c = m_cursor_x; c < m_cols - n; ++c) {
                cells[static_cast<size_t>(c)] = cells[static_cast<size_t>(c + n)];
            }
            for (int c = m_cols - n; c < m_cols; ++c) {
                cells[static_cast<size_t>(c)].reset(m_fg, m_bg);
            }
            break;
        }
        case 'L': { // IL Insert Lines
            if (m_cursor_y >= m_scroll_top && m_cursor_y <= m_scroll_bottom) {
                int saved_top = m_scroll_top;
                m_scroll_top = m_cursor_y;
                scroll_down(p(0, 1));
                m_scroll_top = saved_top;
            }
            break;
        }
        case 'M': { // DL Delete Lines
            if (m_cursor_y >= m_scroll_top && m_cursor_y <= m_scroll_bottom) {
                int saved_top = m_scroll_top;
                m_scroll_top = m_cursor_y;
                scroll_up(p(0, 1));
                m_scroll_top = saved_top;
            }
            break;
        }
        case 'S': // SU Scroll Up
            scroll_up(p(0, 1));
            break;
        case 'T': // SD Scroll Down
            scroll_down(p(0, 1));
            break;
        case 'r': { // DECSTBM Set Top and Bottom Margins
            int top = p(0, 1) - 1;
            int bot = p(1, m_rows) - 1;
            top = std::clamp(top, 0, m_rows - 1);
            bot = std::clamp(bot, 0, m_rows - 1);
            if (top < bot) {
                m_scroll_top = top;
                m_scroll_bottom = bot;
            } else {
                m_scroll_top = 0;
                m_scroll_bottom = m_rows - 1;
            }
            m_cursor_x = 0;
            m_cursor_y = 0;
            m_pending_wrap = false;
            break;
        }
        case 's': save_cursor(); break;
        case 'u': restore_cursor(); break;
        case 'm': apply_sgr(params); break;
        case 'n': { // DSR
            int mode = p_raw(0, 0);
            if (mode == 5 && on_reply) {
                on_reply("\x1b[0n");
            } else if (mode == 6 && on_reply) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "\x1b[%d;%dR", m_cursor_y + 1, m_cursor_x + 1);
                on_reply(buf);
            }
            break;
        }
        case 'c': { // DA1
            if (p_raw(0, 0) == 0 && on_reply) {
                on_reply("\x1b[?62;c");
            }
            break;
        }
        default:
            break;
    }
}

inline void TerminalBuffer::dispatch_osc() {
    size_t semi = m_osc_buf.find(';');
    if (semi == std::string::npos) return;
    int code = std::atoi(m_osc_buf.substr(0, semi).c_str());
    std::string payload = m_osc_buf.substr(semi + 1);

    if (code == 0 || code == 2) {
        m_title = payload;
        if (on_title_changed) on_title_changed(m_title);
    } else if (code == 7) {
        m_cwd = payload;
    } else if (code == 8) {
        size_t s2 = payload.find(';');
        std::string uri = (s2 != std::string::npos) ? payload.substr(s2 + 1) : "";
        if (uri.empty()) {
            m_hyperlink_id = 0;
            m_attrs &= static_cast<uint16_t>(~TerminalAttr::Hyperlink);
        } else {
            m_hyperlink_id = ++m_next_hyperlink_id;
            m_attrs |= TerminalAttr::Hyperlink;
        }
    } else if (code == 9) {
        // Windows Terminal OSC 9;4;st;pr 进度指示条
        if (payload.rfind("4;", 0) == 0) {
            std::string rest = payload.substr(2);
            size_t sp = rest.find(';');
            int st = std::atoi((sp == std::string::npos ? rest : rest.substr(0, sp)).c_str());
            int pr = (sp == std::string::npos) ? 0 : std::atoi(rest.substr(sp + 1).c_str());
            m_progress.state = static_cast<TerminalProgressState>(std::clamp(st, 0, 4));
            m_progress.value = static_cast<uint8_t>(std::clamp(pr, 0, 100));
            if (on_progress_changed) on_progress_changed(m_progress);
        }
    }
}

} // namespace ktui
