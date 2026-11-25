#pragma once

#include "textbox.hpp"
#include <ktui/pal/clipboard.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

namespace ktui {

// 单一原生 RenderTextBox 驱动的 IPv4 + 端口输入控件 (无多余边框，原生全选/复制/粘贴/撤销)
// - Ctrl+A 原生全选完整地址，Ctrl+C/X/V/Z 与鼠标拖选、右键菜单 100% 原生生效
// - 智能输入：输满 3 位自动补 '.'（紧跟手敲 '.' 自动去重）、超过 255 自动进位补 '.'、中文 '。'/'：' 自动转换
// - 智能粘贴：Ctrl+V 自动剥离 http(s):// 与路径（如 http://127.0.0.1:9000/api -> 127.0.0.1:9000）
class RenderIpPortInput : public RenderTextBox {
public:
    RenderIpPortInput(std::string ip = "192.168.1.1",
                      uint16_t port = 8080,
                      bool show_port = true,
                      Fn<void(const std::string&, uint16_t)> on_changed = nullptr)
        : m_show_port(show_port),
          m_user_on_changed(std::move(on_changed)) {
        set_preferred_width(show_port ? 196.0f : 148.0f);
        set_placeholder(show_port ? "127.0.0.1:8080" : "127.0.0.1");
        set_address(ip, port, false);

        set_on_change([this](const std::string& raw) {
            if (m_internal_sync) return;
            const bool is_growing = raw.size() > m_last_text.size();
            std::string formatted = sanitize_and_format(raw, is_growing);
            m_last_text = formatted;
            if (formatted != raw) {
                m_internal_sync = true;
                RenderTextBox::set_text(formatted);
                set_cursor_index(formatted.size());
                m_internal_sync = false;
            }
            parse_and_validate(RenderTextBox::text());
            if (m_user_on_changed) m_user_on_changed(ip_string(), m_port);
        });

        set_key_intercept([this](KeyEvent& ev) -> bool {
            if (ev.type == PlatformEventType::KeyDown && ev.modifiers.ctrl &&
                (ev.key_code == 'V' || ev.key_code == 'v')) {
                if (paste_text(Clipboard::get_text())) {
                    ev.stop_propagation();
                    return true;
                }
            }
            return false;
        });
    }

    [[nodiscard]] bool is_valid() const noexcept { return m_valid; }

    [[nodiscard]] std::string ip_string() const {
        return std::to_string(m_octets[0]) + "." + std::to_string(m_octets[1]) + "." +
               std::to_string(m_octets[2]) + "." + std::to_string(m_octets[3]);
    }

    [[nodiscard]] uint16_t port() const noexcept { return m_port; }

    [[nodiscard]] std::string endpoint_string() const {
        return m_show_port ? (ip_string() + ":" + std::to_string(m_port)) : ip_string();
    }

    [[nodiscard]] std::string address_category() const {
        int a = m_octets[0], b = m_octets[1];
        if (a == 127) return "Loopback";
        if (a == 10 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31)) return "LAN";
        if (a == 0) return "Any";
        return "Public";
    }

    void set_address(const std::string& ip, uint16_t p, bool notify = true) {
        std::string full = m_show_port ? (ip + ":" + std::to_string(p)) : ip;
        m_internal_sync = true;
        m_last_text = full;
        RenderTextBox::set_text(full);
        set_cursor_index(full.size());
        m_internal_sync = false;
        parse_and_validate(full);
        if (notify && m_user_on_changed) m_user_on_changed(ip_string(), m_port);
    }

    // 智能粘贴（自动剥离 http(s)://、路径与首尾空白）
    bool paste_text(const std::string& raw) {
        std::string s = raw;
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        if (s.empty()) return false;

        size_t scheme = s.find("://");
        if (scheme != std::string::npos) s = s.substr(scheme + 3);
        size_t slash = s.find('/');
        if (slash != std::string::npos) s = s.substr(0, slash);

        std::string cleaned = sanitize_and_format(s, false);
        if (cleaned.empty()) return false;

        m_internal_sync = true;
        m_last_text = cleaned;
        RenderTextBox::set_text(cleaned);
        set_cursor_index(cleaned.size());
        m_internal_sync = false;
        parse_and_validate(cleaned);
        if (m_user_on_changed) m_user_on_changed(ip_string(), m_port);
        return true;
    }

    void set_on_changed(Fn<void(const std::string&, uint16_t)> cb) { m_user_on_changed = std::move(cb); }

private:
    std::string sanitize_and_format(const std::string& in, bool auto_dot) const {
        std::string out;
        out.reserve(in.size() + 4);
        int dots = 0;
        bool has_colon = false;
        int cur_seg_digits = 0;
        int cur_seg_val = 0;

        for (size_t i = 0; i < in.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(in[i]);
            char ch = 0;
            // 中文句号 '。' / 中文冒号 '：' 自动转英文
            if (i + 2 < in.size() && c == 0xE3 && static_cast<unsigned char>(in[i + 1]) == 0x80 &&
                static_cast<unsigned char>(in[i + 2]) == 0x82) {
                ch = '.';
                i += 2;
            } else if (i + 2 < in.size() && c == 0xEF && static_cast<unsigned char>(in[i + 1]) == 0xBC &&
                       static_cast<unsigned char>(in[i + 2]) == 0x9A) {
                ch = ':';
                i += 2;
            } else if ((c >= '0' && c <= '9') || c == '.' || c == ':') {
                ch = static_cast<char>(c);
            } else {
                continue;
            }

            if (ch == '.') {
                // 吞掉段首/连续重复 '.' 以及超过 3 个 '.' 或端口段后的 '.'
                if (has_colon || dots >= 3 || cur_seg_digits == 0) continue;
                out.push_back('.');
                ++dots;
                cur_seg_digits = 0;
                cur_seg_val = 0;
                continue;
            }

            if (ch == ':') {
                if (!m_show_port || has_colon || out.empty() || out.back() == '.') continue;
                out.push_back(':');
                has_colon = true;
                cur_seg_digits = 0;
                cur_seg_val = 0;
                continue;
            }

            // 数字字符
            int d = ch - '0';
            if (!has_colon) {
                // IPv4 段：若继续拼入将超过 255 或超过 3 位，且还不足 3 个 '.'，自动补 '.' 进下一段
                if (cur_seg_digits > 0 && (cur_seg_digits >= 3 || cur_seg_val * 10 + d > 255)) {
                    if (dots < 3) {
                        out.push_back('.');
                        ++dots;
                        cur_seg_digits = 0;
                        cur_seg_val = 0;
                    } else if (m_show_port && !has_colon) {
                        out.push_back(':');
                        has_colon = true;
                        cur_seg_digits = 0;
                        cur_seg_val = 0;
                    } else {
                        continue;
                    }
                }
                out.push_back(ch);
                cur_seg_val = cur_seg_val * 10 + d;
                ++cur_seg_digits;
                // 正在末尾输入时，输满 3 位且不足 3 个 '.' 自动补 '.'（紧跟手敲 '.' 会被上方 cur_seg_digits==0 自动去重！）
                if (auto_dot && !has_colon && dots < 3 && cur_seg_digits == 3 && i + 1 == in.size()) {
                    out.push_back('.');
                    ++dots;
                    cur_seg_digits = 0;
                    cur_seg_val = 0;
                }
            } else {
                // 端口段最多 5 位
                if (cur_seg_digits >= 5) continue;
                out.push_back(ch);
                cur_seg_val = cur_seg_val * 10 + d;
                ++cur_seg_digits;
            }
        }
        return out;
    }

    void parse_and_validate(const std::string& s) {
        std::string ip_part = s;
        bool ok = true;
        uint16_t parsed_port = m_port;

        size_t colon = s.find(':');
        if (colon != std::string::npos) {
            if (!m_show_port) ok = false;
            ip_part = s.substr(0, colon);
            std::string port_str = s.substr(colon + 1);
            if (port_str.empty() || port_str.find(':') != std::string::npos) {
                ok = false;
            } else {
                long pv = std::strtol(port_str.c_str(), nullptr, 10);
                if (pv < 0 || pv > 65535) ok = false;
                parsed_port = static_cast<uint16_t>(std::clamp<long>(pv, 0, 65535));
            }
        }

        std::array<int, 4> oct{0, 0, 0, 0};
        size_t pos = 0;
        int parts = 0;
        while (pos <= ip_part.size() && parts < 5) {
            size_t dot = ip_part.find('.', pos);
            std::string seg = (dot == std::string::npos) ? ip_part.substr(pos) : ip_part.substr(pos, dot - pos);
            if (seg.empty() || seg.size() > 3) {
                ok = false;
            } else if (parts < 4) {
                int v = std::atoi(seg.c_str());
                if (v < 0 || v > 255) ok = false;
                oct[static_cast<size_t>(parts)] = std::clamp(v, 0, 255);
            }
            ++parts;
            if (dot == std::string::npos) break;
            pos = dot + 1;
        }
        if (parts != 4) ok = false;

        m_octets = oct;
        m_port = parsed_port;
        m_valid = ok;

        Color dot_c = m_valid
            ? Color{0x10 / 255.0f, 0x89 / 255.0f, 0x3E / 255.0f, 1.0f}
            : Color{0xD1 / 255.0f, 0x34 / 255.0f, 0x38 / 255.0f, 1.0f};
        set_status_dot(true, dot_c);
    }

    std::array<int, 4> m_octets{192, 168, 1, 1};
    uint16_t m_port{8080};
    bool m_show_port{true};
    bool m_valid{true};
    bool m_internal_sync{false};
    std::string m_last_text;
    Fn<void(const std::string&, uint16_t)> m_user_on_changed;
};

struct IpPortInput : BindableMixin<IpPortInput, RenderIpPortInput> {
    std::string ip{"192.168.1.1"};
    uint16_t port{8080};
    bool show_port{true};
    Fn<void(const std::string&, uint16_t)> on_changed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class IpPortInputWidget : public Widget {
public:
    explicit IpPortInputWidget(const IpPortInput& desc) : m_desc(desc) {}
    explicit IpPortInputWidget(IpPortInput&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderIpPortInput>(m_desc.ip, m_desc.port, m_desc.show_port, m_desc.on_changed);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderIpPortInput>(node)) {
            n->set_on_changed(m_desc.on_changed);
        }
    }
private:
    IpPortInput m_desc;
};

inline IpPortInput::operator WidgetPtr() const& { return make_widget<IpPortInputWidget>(*this); }
inline IpPortInput::operator WidgetPtr() && { return make_widget<IpPortInputWidget>(std::move(*this)); }

} // namespace ktui
