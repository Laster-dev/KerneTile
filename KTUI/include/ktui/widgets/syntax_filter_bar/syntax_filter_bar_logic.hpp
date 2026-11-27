#pragma once

#include <ktui/core/strings.hpp>

#include "syntax_filter_bar_data.hpp"
#include "syntax_filter_bar_style.hpp"
#include <algorithm>
#include <cctype>

namespace ktui {

// 带“过滤表达式状态灯”的语法感知输入框 (Wireshark Display Filter 风格)
// 直接继承单一 RenderTextBox（零嵌套），实时语法校验变色 + 字段 Tab 自动补全
class RenderSyntaxFilterBar : public RenderTextBox {
public:
    RenderSyntaxFilterBar(std::string text = "",
                          std::vector<std::string> known_fields = {"ip", "port", "proto", "status", "latency", "method"},
                          std::string placeholder = FrameworkStrings::current().syntax_filter_placeholder,
                          Fn<void(const std::string&, SyntaxFilterState, const std::string&)> on_evaluated = nullptr)
        : m_fields(std::move(known_fields)),
          m_on_evaluated(std::move(on_evaluated)) {
        set_preferred_width(460.0f);
        set_custom_height(32.0f);
        set_custom_corner_radius(5.0f);
        set_placeholder(std::move(placeholder));
        set_ghost_badge(FrameworkStrings::current().syntax_filter_tab_badge);

        set_on_change([this](const std::string&) { evaluate(); });
        set_key_intercept([this](KeyEvent& ev) -> bool {
            if (ev.type == PlatformEventType::KeyDown && ev.key_code == 0x09 && !m_cur_ghost.empty()) {
                std::string next = RenderTextBox::text() + m_cur_ghost;
                RenderTextBox::set_text(next);
                set_cursor_index(next.size());
                evaluate();
                ev.stop_propagation();
                return true;
            }
            return false;
        });

        // 触控右滑手势接受字段自动补全
        auto orig_down = on_pointer_down;
        on_pointer_down = [this, orig_down](PointerEvent& ev) {
            if (ev.is_touch() && ev.button == PointerButton::Primary) {
                m_touch_swiping = true;
                m_touch_start_x = ev.local_position.x;
                m_touch_start_y = ev.local_position.y;
                m_touch_accepted = false;
            }
            if (orig_down) orig_down(ev);
        };
        auto orig_move = on_pointer_move;
        on_pointer_move = [this, orig_move](PointerEvent& ev) {
            if (ev.is_touch() && m_touch_swiping && !m_touch_accepted && !m_cur_ghost.empty()) {
                float dx = ev.local_position.x - m_touch_start_x;
                float dy = ev.local_position.y - m_touch_start_y;
                if (dx > 28.0f && touch::is_horizontal_drag(dx, dy)) {
                    std::string next = RenderTextBox::text() + m_cur_ghost;
                    RenderTextBox::set_text(next);
                    set_cursor_index(next.size());
                    evaluate();
                    m_touch_accepted = true;
                    ev.stop_propagation();
                    return;
                }
            }
            if (orig_move) orig_move(ev);
        };
        auto orig_up = on_pointer_up;
        on_pointer_up = [this, orig_up](PointerEvent& ev) {
            m_touch_swiping = false;
            m_touch_accepted = false;
            if (orig_up) orig_up(ev);
        };
        auto orig_cancel = on_pointer_cancel;
        on_pointer_cancel = [this, orig_cancel](PointerEvent& ev) {
            m_touch_swiping = false;
            m_touch_accepted = false;
            if (orig_cancel) orig_cancel(ev);
        };

        RenderTextBox::set_text(std::move(text));
        set_cursor_index(RenderTextBox::text().size());
        evaluate();
    }

    [[nodiscard]] SyntaxFilterState syntax_state() const noexcept { return m_state; }
    [[nodiscard]] const std::string& diagnostic() const noexcept { return m_diag; }

    void set_expression(const std::string& expr) {
        RenderTextBox::set_text(expr);
        set_cursor_index(expr.size());
        evaluate();
    }

    void set_on_evaluated(Fn<void(const std::string&, SyntaxFilterState, const std::string&)> cb) {
        m_on_evaluated = std::move(cb);
    }

private:
    static bool is_ident_char(char c) noexcept {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
    }

    void update_autocomplete(const std::string& s) {
        m_cur_ghost.clear();
        if (s.empty()) { set_ghost_suffix(""); return; }
        size_t end = s.size();
        size_t start = end;
        while (start > 0 && is_ident_char(s[start - 1])) --start;
        std::string prefix = s.substr(start, end - start);
        if (!prefix.empty()) {
            for (const auto& f : m_fields) {
                if (f.size() > prefix.size() && f.compare(0, prefix.size(), prefix) == 0) {
                    m_cur_ghost = f.substr(prefix.size()) + " == ";
                    break;
                }
            }
        }
        set_ghost_suffix(m_cur_ghost);
    }

    void evaluate() {
        const std::string& s = RenderTextBox::text();
        update_autocomplete(s);

        // 零 regex 轻量级词法/语法检查：支持 && / || / () / == != >= <= > < contains
        std::vector<std::string> toks;
        int paren_depth = 0;
        bool bad_paren = false;
        size_t i = 0;
        while (i < s.size()) {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            if (i >= s.size()) break;
            char c = s[i];
            if (c == '(') { ++paren_depth; toks.push_back("("); ++i; continue; }
            if (c == ')') { if (--paren_depth < 0) bad_paren = true; toks.push_back(")"); ++i; continue; }
            if (i + 1 < s.size()) {
                std::string two = s.substr(i, 2);
                if (two == "&&" || two == "||" || two == "==" || two == "!=" || two == ">=" || two == "<=") {
                    toks.push_back(two);
                    i += 2;
                    continue;
                }
            }
            if (c == '>' || c == '<') { toks.push_back(std::string(1, c)); ++i; continue; }
            size_t st = i;
            while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i])) &&
                   s[i] != '(' && s[i] != ')' && s[i] != '&' && s[i] != '|' &&
                   s[i] != '=' && s[i] != '!' && s[i] != '>' && s[i] != '<') {
                ++i;
            }
            if (i == st) { bad_paren = true; break; }
            toks.push_back(s.substr(st, i - st));
        }

        if (toks.empty()) {
            m_state = SyntaxFilterState::Empty;
            m_diag = FrameworkStrings::current().syntax_filter_ready;
        } else if (bad_paren || paren_depth != 0) {
            m_state = SyntaxFilterState::Error;
            m_diag = FrameworkStrings::current().syntax_filter_err_paren;
        } else {
            // 校验子句序列: field op value (&& / || field op value)*
            bool has_error = false;
            bool has_warning = false;
            std::string warn_field;
            size_t k = 0;
            while (k < toks.size()) {
                while (k < toks.size() && toks[k] == "(") ++k;
                if (k >= toks.size()) { has_error = true; break; }
                const std::string& field = toks[k++];
                if (field == "&&" || field == "||" || field == ")") { has_error = true; break; }
                if (!m_fields.empty() && std::find(m_fields.begin(), m_fields.end(), field) == m_fields.end()) {
                    has_warning = true;
                    warn_field = field;
                }
                if (k >= toks.size()) { has_error = true; break; }
                const std::string& op = toks[k++];
                if (op != "==" && op != "!=" && op != ">" && op != "<" && op != ">=" && op != "<=" && op != "contains") {
                    has_error = true;
                    break;
                }
                if (k >= toks.size() || toks[k] == "&&" || toks[k] == "||" || toks[k] == ")") {
                    has_error = true;
                    break;
                }
                ++k; // value
                while (k < toks.size() && toks[k] == ")") ++k;
                if (k < toks.size()) {
                    if (toks[k] != "&&" && toks[k] != "||") { has_error = true; break; }
                    ++k;
                    if (k >= toks.size()) { has_error = true; break; }
                }
            }

            if (has_error) {
                m_state = SyntaxFilterState::Error;
                m_diag = FrameworkStrings::current().syntax_filter_err_expr;
            } else if (has_warning) {
                m_state = SyntaxFilterState::Warning;
                m_diag = std::string(FrameworkStrings::current().syntax_filter_warn_prefix) + warn_field + FrameworkStrings::current().syntax_filter_warn_suffix;
            } else {
                m_state = SyntaxFilterState::Valid;
                m_diag = FrameworkStrings::current().syntax_filter_valid;
            }
        }

        // Wireshark 风格底色 + 状态灯（自动触发 RenderTextBox 内置 200ms 平滑背景色过渡动效！）
        const bool is_dark = ThemeContext::current().is_dark;
        switch (m_state) {
            case SyntaxFilterState::Valid:
                set_custom_background(is_dark ? Color{0.08f, 0.28f, 0.16f, 0.55f} : Color{0.84f, 0.96f, 0.88f, 0.95f});
                set_status_dot(true, Color{0x10 / 255.0f, 0x89 / 255.0f, 0x3E / 255.0f, 1.0f});
                break;
            case SyntaxFilterState::Warning:
                set_custom_background(is_dark ? Color{0.34f, 0.25f, 0.06f, 0.55f} : Color{1.0f, 0.95f, 0.78f, 0.95f});
                set_status_dot(true, Color{0xF7 / 255.0f, 0x93 / 255.0f, 0x0C / 255.0f, 1.0f});
                break;
            case SyntaxFilterState::Error:
                set_custom_background(is_dark ? Color{0.35f, 0.10f, 0.11f, 0.58f} : Color{0.99f, 0.85f, 0.86f, 0.95f});
                set_status_dot(true, Color{0xD1 / 255.0f, 0x34 / 255.0f, 0x38 / 255.0f, 1.0f});
                break;
            case SyntaxFilterState::Empty:
            default:
                set_custom_background(Color{-1.0f, -1.0f, -1.0f, -1.0f});
                set_status_dot(false);
                break;
        }

        if (m_on_evaluated) m_on_evaluated(s, m_state, m_diag);
    }

    std::vector<std::string> m_fields;
    Fn<void(const std::string&, SyntaxFilterState, const std::string&)> m_on_evaluated;
    SyntaxFilterState m_state{SyntaxFilterState::Empty};
    std::string m_diag;
    std::string m_cur_ghost;
    bool m_touch_swiping{false};
    bool m_touch_accepted{false};
    float m_touch_start_x{0.0f};
    float m_touch_start_y{0.0f};
};

struct SyntaxFilterBar : BindableMixin<SyntaxFilterBar, RenderSyntaxFilterBar> {
    std::string text{""};
    std::vector<std::string> known_fields{"ip", "port", "proto", "status", "latency", "method"};
    std::string placeholder{FrameworkStrings::current().syntax_filter_placeholder};
    Fn<void(const std::string&, SyntaxFilterState, const std::string&)> on_evaluated{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class SyntaxFilterBarWidget : public Widget {
public:
    explicit SyntaxFilterBarWidget(const SyntaxFilterBar& desc) : m_desc(desc) {}
    explicit SyntaxFilterBarWidget(SyntaxFilterBar&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderSyntaxFilterBar>(
            m_desc.text, m_desc.known_fields, m_desc.placeholder, m_desc.on_evaluated);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderSyntaxFilterBar>(node)) {
            n->set_on_evaluated(m_desc.on_evaluated);
        }
    }
private:
    SyntaxFilterBar m_desc;
};

inline SyntaxFilterBar::operator WidgetPtr() const& { return make_widget<SyntaxFilterBarWidget>(*this); }
inline SyntaxFilterBar::operator WidgetPtr() && { return make_widget<SyntaxFilterBarWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
