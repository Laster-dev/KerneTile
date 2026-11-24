#pragma once

#include <ktui/event/event_types.hpp>
#include <ktui/pal/clipboard.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>

namespace ktui {

// 单行 UTF-8 文本编辑与光标闪烁复用内核 (供 PasswordBox / NumberBox / AutoSuggestBox 等共享)
class SingleLineTextEditCore {
public:
    struct KeyActionResult {
        bool handled{false};
        bool text_changed{false};
        bool caret_moved{false};
        bool submitted{false};
    };

    [[nodiscard]] static size_t prev_utf8(const std::string& s, size_t pos) noexcept {
        if (pos == 0) return 0;
        size_t i = pos - 1;
        while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
        return i;
    }

    [[nodiscard]] static size_t next_utf8(const std::string& s, size_t pos) noexcept {
        if (pos >= s.size()) return s.size();
        size_t i = pos + 1;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
        return i;
    }

    [[nodiscard]] size_t caret() const noexcept { return m_caret; }
    [[nodiscard]] bool select_all() const noexcept { return m_select_all; }
    [[nodiscard]] bool caret_visible() const noexcept { return m_caret_visible; }

    void clamp_caret(const std::string& text) noexcept {
        m_caret = std::min(m_caret, text.size());
    }

    void set_caret(size_t pos, const std::string& text) noexcept {
        m_caret = std::min(pos, text.size());
        m_select_all = false;
        reset_blink();
    }

    void clear_selection() noexcept { m_select_all = false; }

    void reset_blink() noexcept {
        m_caret_visible = true;
        m_blink_time = std::chrono::steady_clock::now();
    }

    bool tick_blink(bool focused, bool disabled) noexcept {
        if (!focused || disabled) return false;
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_blink_time).count();
        if (ms >= 530) {
            m_caret_visible = !m_caret_visible;
            m_blink_time = now;
            return true;
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms(bool focused, bool disabled) const noexcept {
        if (!focused || disabled) return 1000;
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_blink_time).count();
        return (ms >= 530) ? 16 : static_cast<uint32_t>(530 - ms);
    }

    bool insert_text(std::string& text, const std::string& incoming, size_t max_length = 0) {
        if (incoming.empty()) return false;
        if (m_select_all) {
            text.clear();
            m_caret = 0;
            m_select_all = false;
        }
        if (max_length > 0 && text.size() + incoming.size() > max_length) {
            return false;
        }
        text.insert(m_caret, incoming);
        m_caret += incoming.size();
        reset_blink();
        return true;
    }

    KeyActionResult handle_key(std::string& text,
                               const KeyEvent& e,
                               bool allow_copy_cut = true,
                               size_t max_length = 0) {
        KeyActionResult res;
        if (e.type != PlatformEventType::KeyDown) return res;

        if (e.modifiers.ctrl) {
            if (e.key_code == 'A' || e.key_code == 'a') {
                m_select_all = !text.empty();
                m_caret = text.size();
                res.handled = true;
                res.caret_moved = true;
                return res;
            }
            if (allow_copy_cut && (e.key_code == 'C' || e.key_code == 'c')) {
                if (!text.empty()) Clipboard::set_text(text);
                res.handled = true;
                return res;
            }
            if (allow_copy_cut && (e.key_code == 'X' || e.key_code == 'x')) {
                if (!text.empty()) {
                    Clipboard::set_text(text);
                    text.clear();
                    m_caret = 0;
                    m_select_all = false;
                    reset_blink();
                    res.text_changed = true;
                }
                res.handled = true;
                return res;
            }
            if (!allow_copy_cut && (e.key_code == 'C' || e.key_code == 'c' || e.key_code == 'X' || e.key_code == 'x')) {
                if (e.key_code == 'X' || e.key_code == 'x') {
                    text.clear();
                    m_caret = 0;
                    m_select_all = false;
                    reset_blink();
                    res.text_changed = true;
                }
                res.handled = true;
                return res;
            }
            if (e.key_code == 'V' || e.key_code == 'v') {
                std::string clip = Clipboard::get_text();
                if (!clip.empty() && insert_text(text, clip, max_length)) {
                    res.text_changed = true;
                }
                res.handled = true;
                return res;
            }
        }

        switch (e.key_code) {
            case 8: // Backspace
                if (m_select_all) {
                    text.clear();
                    m_caret = 0;
                    m_select_all = false;
                    reset_blink();
                    res.text_changed = true;
                } else if (m_caret > 0) {
                    size_t p = prev_utf8(text, m_caret);
                    text.erase(p, m_caret - p);
                    m_caret = p;
                    reset_blink();
                    res.text_changed = true;
                }
                res.handled = true;
                return res;
            case 46: // Delete
                if (m_select_all) {
                    text.clear();
                    m_caret = 0;
                    m_select_all = false;
                    reset_blink();
                    res.text_changed = true;
                } else if (m_caret < text.size()) {
                    size_t n = next_utf8(text, m_caret);
                    text.erase(m_caret, n - m_caret);
                    reset_blink();
                    res.text_changed = true;
                }
                res.handled = true;
                return res;
            case 37: // Left
                m_select_all = false;
                m_caret = prev_utf8(text, m_caret);
                reset_blink();
                res.handled = true;
                res.caret_moved = true;
                return res;
            case 39: // Right
                m_select_all = false;
                m_caret = next_utf8(text, m_caret);
                reset_blink();
                res.handled = true;
                res.caret_moved = true;
                return res;
            case 36: // Home
                m_select_all = false;
                m_caret = 0;
                reset_blink();
                res.handled = true;
                res.caret_moved = true;
                return res;
            case 35: // End
                m_select_all = false;
                m_caret = text.size();
                reset_blink();
                res.handled = true;
                res.caret_moved = true;
                return res;
            case 13: // Enter
                res.handled = true;
                res.submitted = true;
                return res;
            default:
                if (!e.text.empty()) {
                    if (insert_text(text, e.text, max_length)) {
                        res.text_changed = true;
                    }
                    res.handled = true;
                }
                return res;
        }
    }

private:
    size_t m_caret{0};
    bool m_select_all{false};
    bool m_caret_visible{true};
    std::chrono::steady_clock::time_point m_blink_time{std::chrono::steady_clock::now()};
};

} // namespace ktui
