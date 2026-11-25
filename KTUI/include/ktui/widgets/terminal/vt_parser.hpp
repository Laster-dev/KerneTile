#pragma once

#include "terminal_types.hpp"
#include <ktui/core/fn.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace ktui {

// ==============================================================================
// 终端核心状态机与双屏缓冲区 (TerminalBuffer + VT500/ANSI/xterm 解析器)
// 完整对标 Windows Terminal 核心能力：
//   - UTF-8 流式解码 + 东亚宽字符/CJK/Emoji 2格占位
//   - 主屏 + 备用屏 (DECSET ?1049h / ?47h / ?1047h) + 可配置历史回滚缓冲 (默认 9001 行)
//   - 完整 CSI / ESC / OSC / SGR (16色 / 256色 / 24-bit TrueColor / 粗体斜体下划线删除线反色)
//   - 滚动区域 (DECSTBM)、插删行列 (IL/DL/ICH/DCH)、光标样式 (DECSCUSR)、标题 (OSC 0/2)、
//     进度指示 (OSC 9;4)、超链接 (OSC 8)、设备状态回报 (DSR 6n / DA1)、应用光标模式 (DECCKM ?1)、
//     括号粘贴模式 (?2004)、全文检索与选区文本提取
// ==============================================================================
class TerminalBuffer {
public:
    TerminalBuffer(int cols = 80, int rows = 24, size_t max_scrollback = 9001)
        : m_cols(std::max(2, cols)),
          m_rows(std::max(2, rows)),
          m_max_scrollback(max_scrollback) {
        init_grid(m_main_grid, m_cols, m_rows);
        init_grid(m_alt_grid, m_cols, m_rows);
        m_scroll_top = 0;
        m_scroll_bottom = m_rows - 1;
    }

    // ==================== 属性与模式查询 ====================
    [[nodiscard]] int cols() const noexcept { return m_cols; }
    [[nodiscard]] int rows() const noexcept { return m_rows; }
    [[nodiscard]] int cursor_col() const noexcept { return m_cursor_x; }
    [[nodiscard]] int cursor_row() const noexcept { return m_cursor_y; }
    [[nodiscard]] bool cursor_visible() const noexcept { return m_cursor_visible; }
    [[nodiscard]] bool cursor_blink() const noexcept { return m_cursor_blink; }
    [[nodiscard]] bool has_custom_cursor_style() const noexcept { return m_has_vt_cursor_style; }
    [[nodiscard]] TerminalCursorStyle vt_cursor_style() const noexcept { return m_vt_cursor_style; }
    [[nodiscard]] bool is_alt_screen() const noexcept { return m_in_alt_screen; }
    [[nodiscard]] bool app_cursor_keys() const noexcept { return m_app_cursor_keys; }
    [[nodiscard]] bool bracketed_paste() const noexcept { return m_bracketed_paste; }
    [[nodiscard]] bool mouse_sgr_mode() const noexcept { return m_mouse_sgr; }
    [[nodiscard]] bool focus_reporting() const noexcept { return m_focus_reporting; }
    [[nodiscard]] int mouse_tracking_mode() const noexcept {
        if (m_mouse_1003) return 1003;
        if (m_mouse_1002) return 1002;
        if (m_mouse_1000) return 1000;
        return 0;
    }
    [[nodiscard]] const std::string& title() const noexcept { return m_title; }
    [[nodiscard]] const std::string& cwd() const noexcept { return m_cwd; }
    [[nodiscard]] TerminalProgress progress() const noexcept { return m_progress; }

    [[nodiscard]] size_t scrollback_count() const noexcept {
        return m_in_alt_screen ? 0 : m_scrollback.size();
    }

    [[nodiscard]] int total_rows() const noexcept {
        return static_cast<int>(scrollback_count()) + m_rows;
    }

    // 按绝对行号 (0 .. total_rows()-1) 访问行
    [[nodiscard]] const TerminalRow& row_at(int abs_row) const noexcept {
        const auto& grid = active_grid();
        if (m_in_alt_screen) {
            int r = std::clamp(abs_row, 0, m_rows - 1);
            return grid[static_cast<size_t>(r)];
        }
        int sb = static_cast<int>(m_scrollback.size());
        if (abs_row < sb) {
            return m_scrollback[static_cast<size_t>(std::max(0, abs_row))];
        }
        int r = std::clamp(abs_row - sb, 0, m_rows - 1);
        return grid[static_cast<size_t>(r)];
    }

    // 回调钩子 (由 RenderTerminal 绑定)
    Fn<void(const std::string&)> on_title_changed;
    Fn<void(TerminalProgress)> on_progress_changed;
    Fn<void()> on_bell;
    Fn<void(std::string_view)> on_reply; // 回传 DSR / DA1 给后端

    // ==================== 尺寸调整 ====================
    void resize(int new_cols, int new_rows) {
        new_cols = std::max(2, new_cols);
        new_rows = std::max(2, new_rows);
        if (new_cols == m_cols && new_rows == m_rows) return;

        resize_grid(m_main_grid, new_cols, new_rows, !m_in_alt_screen);
        resize_grid(m_alt_grid, new_cols, new_rows, false);

        m_cols = new_cols;
        m_rows = new_rows;
        m_cursor_x = std::clamp(m_cursor_x, 0, m_cols - 1);
        m_cursor_y = std::clamp(m_cursor_y, 0, m_rows - 1);
        m_scroll_top = 0;
        m_scroll_bottom = m_rows - 1;
        m_pending_wrap = false;
    }

    // 清空屏幕与回滚历史 (菜单 "Clear Buffer")
    void clear_all() {
        m_scrollback.clear();
        for (auto& r : active_grid()) {
            for (auto& c : r.cells) c.reset(m_fg, m_bg);
            r.wrapped = false;
        }
        m_cursor_x = 0;
        m_cursor_y = 0;
        m_pending_wrap = false;
    }

    // ==================== 流式输入 VT/UTF-8 字节流 ====================
    void feed(std::string_view bytes) {
        for (size_t i = 0; i < bytes.size(); ++i) {
            process_byte(static_cast<uint8_t>(bytes[i]));
        }
    }

    // ==================== 选区文本提取 & 全文检索 ====================
    [[nodiscard]] std::string extract_text(TerminalCoord a, TerminalCoord b, bool block_mode = false) const {
        if (b < a) std::swap(a, b);
        const int tot = total_rows();
        if (tot <= 0) return {};
        a.row = std::clamp(a.row, 0, tot - 1);
        b.row = std::clamp(b.row, 0, tot - 1);

        std::string out;
        for (int r = a.row; r <= b.row; ++r) {
            const auto& row = row_at(r);
            int c0 = block_mode ? std::min(a.col, b.col) : ((r == a.row) ? a.col : 0);
            int c1 = block_mode ? std::max(a.col, b.col) : ((r == b.row) ? b.col : static_cast<int>(row.cells.size()) - 1);
            c0 = std::clamp(c0, 0, static_cast<int>(row.cells.size()) - 1);
            c1 = std::clamp(c1, 0, static_cast<int>(row.cells.size()) - 1);
            // 宽字符(CJK/Emoji)整字对齐：绝不截断半个汉字
            if (row.cells[static_cast<size_t>(c0)].width == 0 && c0 > 0) --c0;
            if (row.cells[static_cast<size_t>(c1)].width == 2 && c1 + 1 < static_cast<int>(row.cells.size())) ++c1;

            std::string line;
            for (int c = c0; c <= c1; ++c) {
                const auto& cell = row.cells[static_cast<size_t>(c)];
                if (cell.width == 0) continue; // 跳过宽字符右半占位格
                append_utf8(line, cell.codepoint ? cell.codepoint : ' ');
            }
            // 去除行尾空白 (Windows Terminal 行为：保留逻辑换行，不输出尾部填充空格)
            while (!line.empty() && line.back() == ' ') line.pop_back();
            out.append(line);
            if (r < b.row && (!row.wrapped || block_mode)) {
                out.push_back('\n');
            }
        }
        return out;
    }

    [[nodiscard]] std::vector<TerminalSearchMatch> find_all(std::string_view query, bool case_sensitive = false) const {
        std::vector<TerminalSearchMatch> res;
        if (query.empty()) return res;

        std::string needle(query);
        if (!case_sensitive) {
            for (char& ch : needle) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        const int tot = total_rows();
        for (int r = 0; r < tot; ++r) {
            const auto& row = row_at(r);
            std::string line;
            std::vector<int> byte_to_col;
            line.reserve(row.cells.size());
            byte_to_col.reserve(row.cells.size());
            for (int c = 0; c < static_cast<int>(row.cells.size()); ++c) {
                const auto& cell = row.cells[static_cast<size_t>(c)];
                if (cell.width == 0) continue;
                size_t before = line.size();
                append_utf8(line, cell.codepoint ? cell.codepoint : ' ');
                for (size_t k = before; k < line.size(); ++k) {
                    byte_to_col.push_back(c);
                }
            }
            std::string hay = line;
            if (!case_sensitive) {
                for (char& ch : hay) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            size_t pos = 0;
            while ((pos = hay.find(needle, pos)) != std::string::npos) {
                size_t end_byte = pos + needle.size();
                int cs = byte_to_col[pos];
                int ce = (end_byte < byte_to_col.size()) ? byte_to_col[end_byte] : static_cast<int>(row.cells.size());
                if (ce <= cs) ce = cs + 1;
                res.push_back(TerminalSearchMatch{r, cs, ce});
                pos += std::max<size_t>(1, needle.size());
            }
        }
        return res;
    }

    // 工具：将单个 UTF-32 码点编码为 UTF-8
    static void append_utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80u) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp < 0x10000u) {
            out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp <= 0x10FFFFu) {
            out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }

    // 东亚宽字符 / 全角标点 / Emoji 宽度判断 (1 或 2 格)
    [[nodiscard]] static uint8_t char_cell_width(uint32_t cp) noexcept {
        if (cp < 0x1100u) return 1;
        if ((cp >= 0x1100u && cp <= 0x115Fu) ||
            (cp >= 0x2E80u && cp <= 0xA4CFu && cp != 0x303Fu) ||
            (cp >= 0xAC00u && cp <= 0xD7A3u) ||
            (cp >= 0xF900u && cp <= 0xFAFFu) ||
            (cp >= 0xFE10u && cp <= 0xFE19u) ||
            (cp >= 0xFE30u && cp <= 0xFE6Fu) ||
            (cp >= 0xFF00u && cp <= 0xFF60u) ||
            (cp >= 0xFFE0u && cp <= 0xFFE6u) ||
            (cp >= 0x1F300u && cp <= 0x1FAFFu) ||
            (cp >= 0x20000u && cp <= 0x3FFFDu)) {
            return 2;
        }
        return 1;
    }

private:
    enum class ParseState : uint8_t {
        Ground,
        Escape,
        EscapeIntermediate,
        CsiEntry,
        OscString,
        OscEsc,
        DcsIgnore,
        DcsEsc
    };

    struct SavedCursor {
        int x{0};
        int y{0};
        TerminalColor fg{};
        TerminalColor bg{};
        uint16_t attrs{TerminalAttr::None};
        bool auto_wrap{true};
    };

    static void init_grid(std::vector<TerminalRow>& grid, int cols, int rows) {
        grid.assign(static_cast<size_t>(rows), TerminalRow{});
        for (auto& r : grid) {
            r.cells.assign(static_cast<size_t>(cols), TerminalCell{});
            r.wrapped = false;
        }
    }

    void resize_grid(std::vector<TerminalRow>& grid, int new_cols, int new_rows, bool push_scrollback) {
        for (auto& r : grid) {
            r.cells.resize(static_cast<size_t>(new_cols));
        }
        if (new_rows < static_cast<int>(grid.size())) {
            int diff = static_cast<int>(grid.size()) - new_rows;
            // 若光标已接近底部，将顶部多余行滚入回滚历史
            int excess = std::max(0, m_cursor_y - (new_rows - 1));
            int to_push = std::min(diff, excess);
            if (push_scrollback && to_push > 0) {
                for (int i = 0; i < to_push; ++i) {
                    push_to_scrollback(std::move(grid[static_cast<size_t>(i)]));
                }
                grid.erase(grid.begin(), grid.begin() + to_push);
                m_cursor_y -= to_push;
            }
            grid.resize(static_cast<size_t>(new_rows));
        } else if (new_rows > static_cast<int>(grid.size())) {
            size_t old_n = grid.size();
            grid.resize(static_cast<size_t>(new_rows));
            for (size_t i = old_n; i < grid.size(); ++i) {
                grid[i].cells.assign(static_cast<size_t>(new_cols), TerminalCell{});
                grid[i].wrapped = false;
            }
        }
    }

    [[nodiscard]] std::vector<TerminalRow>& active_grid() noexcept {
        return m_in_alt_screen ? m_alt_grid : m_main_grid;
    }
    [[nodiscard]] const std::vector<TerminalRow>& active_grid() const noexcept {
        return m_in_alt_screen ? m_alt_grid : m_main_grid;
    }

    void push_to_scrollback(TerminalRow row) {
        if (m_max_scrollback == 0) return;
        m_scrollback.push_back(std::move(row));
        while (m_scrollback.size() > m_max_scrollback) {
            m_scrollback.pop_front();
        }
    }

    // ==================== 字节状态机 ====================
    void process_byte(uint8_t b) {
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

    void execute_c0(uint8_t b) {
        switch (b) {
            case 0x07: // BEL
                if (on_bell) on_bell();
                break;
            case 0x08: // BS
                m_pending_wrap = false;
                if (m_cursor_x > 0) --m_cursor_x;
                break;
            case 0x09: { // TAB
                m_pending_wrap = false;
                int next_tab = ((m_cursor_x / 8) + 1) * 8;
                m_cursor_x = std::min(m_cols - 1, next_tab);
                break;
            }
            case 0x0A: // LF
            case 0x0B: // VT
            case 0x0C: // FF
                line_feed();
                break;
            case 0x0D: // CR
                m_pending_wrap = false;
                m_cursor_x = 0;
                break;
            default:
                break;
        }
    }

    void put_codepoint(uint32_t cp) {
        uint8_t w = char_cell_width(cp);
        if (m_pending_wrap) {
            if (m_auto_wrap) {
                active_grid()[static_cast<size_t>(m_cursor_y)].wrapped = true;
                m_cursor_x = 0;
                line_feed();
            }
            m_pending_wrap = false;
        }

        // 宽字符落到最后一列时自动换行
        if (w == 2 && m_cursor_x >= m_cols - 1) {
            if (m_auto_wrap) {
                active_grid()[static_cast<size_t>(m_cursor_y)].cells[static_cast<size_t>(m_cursor_x)].reset(m_fg, m_bg);
                active_grid()[static_cast<size_t>(m_cursor_y)].wrapped = true;
                m_cursor_x = 0;
                line_feed();
            } else {
                w = 1;
            }
        }

        auto& row = active_grid()[static_cast<size_t>(m_cursor_y)].cells;
        // 若覆盖了某个宽字符的右半占位格，清理其左半
        if (row[static_cast<size_t>(m_cursor_x)].width == 0 && m_cursor_x > 0) {
            row[static_cast<size_t>(m_cursor_x - 1)].reset(m_fg, m_bg);
        }
        // 若覆盖了某个宽字符的左半，清理其右半
        if (row[static_cast<size_t>(m_cursor_x)].width == 2 && m_cursor_x + 1 < m_cols) {
            row[static_cast<size_t>(m_cursor_x + 1)].reset(m_fg, m_bg);
        }

        auto& cell = row[static_cast<size_t>(m_cursor_x)];
        cell.codepoint = cp;
        cell.fg = m_fg;
        cell.bg = m_bg;
        cell.attrs = m_attrs;
        cell.width = w;
        cell.hyperlink_id = m_hyperlink_id;

        if (w == 2 && m_cursor_x + 1 < m_cols) {
            auto& trail = row[static_cast<size_t>(m_cursor_x + 1)];
            if (trail.width == 2 && m_cursor_x + 2 < m_cols) {
                row[static_cast<size_t>(m_cursor_x + 2)].reset(m_fg, m_bg);
            }
            trail.codepoint = 0;
            trail.fg = m_fg;
            trail.bg = m_bg;
            trail.attrs = m_attrs;
            trail.width = 0;
            trail.hyperlink_id = m_hyperlink_id;
        }

        if (m_cursor_x + w >= m_cols) {
            m_cursor_x = m_cols - 1;
            m_pending_wrap = true;
        } else {
            m_cursor_x += w;
        }
    }

    void line_feed() {
        m_pending_wrap = false;
        if (m_cursor_y == m_scroll_bottom) {
            scroll_up(1);
        } else if (m_cursor_y < m_rows - 1) {
            ++m_cursor_y;
        }
    }

    void reverse_index() {
        m_pending_wrap = false;
        if (m_cursor_y == m_scroll_top) {
            scroll_down(1);
        } else if (m_cursor_y > 0) {
            --m_cursor_y;
        }
    }

    void scroll_up(int n) {
        n = std::clamp(n, 1, m_scroll_bottom - m_scroll_top + 1);
        auto& grid = active_grid();
        bool full_screen = (!m_in_alt_screen && m_scroll_top == 0 && m_scroll_bottom == m_rows - 1);
        for (int k = 0; k < n; ++k) {
            if (full_screen) {
                push_to_scrollback(std::move(grid[0]));
            }
            for (int r = m_scroll_top; r < m_scroll_bottom; ++r) {
                grid[static_cast<size_t>(r)] = std::move(grid[static_cast<size_t>(r + 1)]);
            }
            grid[static_cast<size_t>(m_scroll_bottom)].cells.assign(static_cast<size_t>(m_cols), TerminalCell{});
            for (auto& c : grid[static_cast<size_t>(m_scroll_bottom)].cells) c.reset(m_fg, m_bg);
            grid[static_cast<size_t>(m_scroll_bottom)].wrapped = false;
        }
    }

    void scroll_down(int n) {
        n = std::clamp(n, 1, m_scroll_bottom - m_scroll_top + 1);
        auto& grid = active_grid();
        for (int k = 0; k < n; ++k) {
            for (int r = m_scroll_bottom; r > m_scroll_top; --r) {
                grid[static_cast<size_t>(r)] = std::move(grid[static_cast<size_t>(r - 1)]);
            }
            grid[static_cast<size_t>(m_scroll_top)].cells.assign(static_cast<size_t>(m_cols), TerminalCell{});
            for (auto& c : grid[static_cast<size_t>(m_scroll_top)].cells) c.reset(m_fg, m_bg);
            grid[static_cast<size_t>(m_scroll_top)].wrapped = false;
        }
    }

    void save_cursor() {
        m_saved = SavedCursor{m_cursor_x, m_cursor_y, m_fg, m_bg, m_attrs, m_auto_wrap};
    }

    void restore_cursor() {
        m_cursor_x = std::clamp(m_saved.x, 0, m_cols - 1);
        m_cursor_y = std::clamp(m_saved.y, 0, m_rows - 1);
        m_fg = m_saved.fg;
        m_bg = m_saved.bg;
        m_attrs = m_saved.attrs;
        m_auto_wrap = m_saved.auto_wrap;
        m_pending_wrap = false;
    }

    void dispatch_esc(char ch) {
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

    void dispatch_csi(char final_ch) {
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

    void erase_line_range(int row, int c0, int c1) {
        if (row < 0 || row >= m_rows) return;
        c0 = std::clamp(c0, 0, m_cols);
        c1 = std::clamp(c1, 0, m_cols);
        auto& cells = active_grid()[static_cast<size_t>(row)].cells;
        for (int c = c0; c < c1; ++c) {
            cells[static_cast<size_t>(c)].reset(m_fg, m_bg);
        }
        if (c0 == 0 && c1 == m_cols) {
            active_grid()[static_cast<size_t>(row)].wrapped = false;
        }
    }

    void apply_dec_private_mode(int mode, bool set) {
        switch (mode) {
            case 1: m_app_cursor_keys = set; break;
            case 7: m_auto_wrap = set; break;
            case 12: m_cursor_blink = set; break;
            case 25: m_cursor_visible = set; break;
            case 47:
            case 1047:
            case 1049:
                if (set && !m_in_alt_screen) {
                    if (mode == 1049) save_cursor();
                    m_in_alt_screen = true;
                    init_grid(m_alt_grid, m_cols, m_rows);
                    m_cursor_x = 0;
                    m_cursor_y = 0;
                    m_scroll_top = 0;
                    m_scroll_bottom = m_rows - 1;
                    m_pending_wrap = false;
                } else if (!set && m_in_alt_screen) {
                    m_in_alt_screen = false;
                    if (mode == 1049) restore_cursor();
                    m_scroll_top = 0;
                    m_scroll_bottom = m_rows - 1;
                    m_pending_wrap = false;
                }
                break;
            case 1000: m_mouse_1000 = set; break;
            case 1002: m_mouse_1002 = set; break;
            case 1003: m_mouse_1003 = set; break;
            case 1004: m_focus_reporting = set; break;
            case 1006:
                m_mouse_sgr = set;
                break;
            case 2004:
                m_bracketed_paste = set;
                break;
            default:
                break;
        }
    }

    void apply_sgr(const std::vector<int>& params) {
        if (params.empty()) {
            m_fg = TerminalColor::default_color();
            m_bg = TerminalColor::default_color();
            m_attrs = TerminalAttr::None;
            return;
        }
        for (size_t i = 0; i < params.size(); ++i) {
            int v = params[i];
            if (v == 0) {
                m_fg = TerminalColor::default_color();
                m_bg = TerminalColor::default_color();
                m_attrs = TerminalAttr::None;
            } else if (v == 1) { m_attrs |= TerminalAttr::Bold;
            } else if (v == 2) { m_attrs |= TerminalAttr::Dim;
            } else if (v == 3) { m_attrs |= TerminalAttr::Italic;
            } else if (v == 4) { m_attrs |= TerminalAttr::Underline;
            } else if (v == 5 || v == 6) { m_attrs |= TerminalAttr::Blink;
            } else if (v == 7) { m_attrs |= TerminalAttr::Inverse;
            } else if (v == 8) { m_attrs |= TerminalAttr::Invisible;
            } else if (v == 9) { m_attrs |= TerminalAttr::Strikethrough;
            } else if (v == 21) { m_attrs |= TerminalAttr::DoubleUnderline;
            } else if (v == 22) { m_attrs &= static_cast<uint16_t>(~(TerminalAttr::Bold | TerminalAttr::Dim));
            } else if (v == 23) { m_attrs &= static_cast<uint16_t>(~TerminalAttr::Italic);
            } else if (v == 24) { m_attrs &= static_cast<uint16_t>(~(TerminalAttr::Underline | TerminalAttr::DoubleUnderline));
            } else if (v == 25) { m_attrs &= static_cast<uint16_t>(~TerminalAttr::Blink);
            } else if (v == 27) { m_attrs &= static_cast<uint16_t>(~TerminalAttr::Inverse);
            } else if (v == 28) { m_attrs &= static_cast<uint16_t>(~TerminalAttr::Invisible);
            } else if (v == 29) { m_attrs &= static_cast<uint16_t>(~TerminalAttr::Strikethrough);
            } else if (v == 53) { m_attrs |= TerminalAttr::Overline;
            } else if (v == 55) { m_attrs &= static_cast<uint16_t>(~TerminalAttr::Overline);
            } else if (v >= 30 && v <= 37) { m_fg = TerminalColor::indexed(static_cast<uint8_t>(v - 30));
            } else if (v == 39) { m_fg = TerminalColor::default_color();
            } else if (v >= 40 && v <= 47) { m_bg = TerminalColor::indexed(static_cast<uint8_t>(v - 40));
            } else if (v == 49) { m_bg = TerminalColor::default_color();
            } else if (v >= 90 && v <= 97) { m_fg = TerminalColor::indexed(static_cast<uint8_t>(v - 90 + 8));
            } else if (v >= 100 && v <= 107) { m_bg = TerminalColor::indexed(static_cast<uint8_t>(v - 100 + 8));
            } else if ((v == 38 || v == 48) && i + 1 < params.size()) {
                bool is_fg = (v == 38);
                int kind = params[i + 1];
                if (kind == 5 && i + 2 < params.size()) {
                    TerminalColor c = TerminalColor::indexed(static_cast<uint8_t>(std::clamp(params[i + 2], 0, 255)));
                    if (is_fg) m_fg = c; else m_bg = c;
                    i += 2;
                } else if (kind == 2 && i + 4 < params.size()) {
                    TerminalColor c = TerminalColor::rgb(
                        static_cast<uint8_t>(std::clamp(params[i + 2], 0, 255)),
                        static_cast<uint8_t>(std::clamp(params[i + 3], 0, 255)),
                        static_cast<uint8_t>(std::clamp(params[i + 4], 0, 255)));
                    if (is_fg) m_fg = c; else m_bg = c;
                    i += 4;
                }
            }
        }
    }

    void dispatch_osc() {
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

    int m_cols{80};
    int m_rows{24};
    size_t m_max_scrollback{9001};
    std::vector<TerminalRow> m_main_grid;
    std::vector<TerminalRow> m_alt_grid;
    std::deque<TerminalRow> m_scrollback;
    bool m_in_alt_screen{false};

    int m_cursor_x{0};
    int m_cursor_y{0};
    int m_scroll_top{0};
    int m_scroll_bottom{23};
    bool m_pending_wrap{false};
    bool m_auto_wrap{true};
    bool m_cursor_visible{true};
    bool m_cursor_blink{true};
    bool m_has_vt_cursor_style{false};
    TerminalCursorStyle m_vt_cursor_style{TerminalCursorStyle::Bar};
    bool m_app_cursor_keys{false};
    bool m_bracketed_paste{false};
    bool m_mouse_sgr{false};
    bool m_mouse_1000{false};
    bool m_mouse_1002{false};
    bool m_mouse_1003{false};
    bool m_focus_reporting{false};

    TerminalColor m_fg{TerminalColor::default_color()};
    TerminalColor m_bg{TerminalColor::default_color()};
    uint16_t m_attrs{TerminalAttr::None};
    uint32_t m_hyperlink_id{0};
    uint32_t m_next_hyperlink_id{0};
    SavedCursor m_saved{};

    std::string m_title;
    std::string m_cwd;
    TerminalProgress m_progress{};

    ParseState m_state{ParseState::Ground};
    std::string m_intermediate;
    std::string m_csi_buf;
    std::string m_osc_buf;
    uint32_t m_utf8_cp{0};
    int m_utf8_expected{0};
};

} // namespace ktui
