#pragma once

#include <ktui/core/strings.hpp>

#include "ghost_text_box_data.hpp"
#include "ghost_text_box_style.hpp"
#include <algorithm>
#include <cctype>

namespace ktui {


// 增量式幽灵文本补全输入框 (直接继承复用框架原生 RenderTextBox)
// - 完整继承原生 TextBox 的真实光标、鼠标拖拽选区、中文 IME、Ctrl+A/C/X/V/Z、右键菜单与 Fluent 2 外观
// - 叠加行内半透明灰色补全后缀，每次按 Tab（或末尾按右方向键）逐词补全到下一个空格，按 Esc 忽略
class RenderGhostTextBox : public RenderTextBox {
public:
    RenderGhostTextBox(std::string text = "",
                       std::string placeholder = FrameworkStrings::current().ghost_placeholder,
                       std::vector<std::string> suggestions = {},
                       Fn<std::string(const std::string&)> predictor = nullptr,
                       Fn<void(const std::string&)> on_changed = nullptr,
                       Fn<void(const std::string&)> on_accepted = nullptr)
        : m_suggestions(std::move(suggestions)),
          m_predictor(std::move(predictor)),
          m_user_on_changed(std::move(on_changed)),
          m_on_accepted(std::move(on_accepted)) {
        set_preferred_width(420.0f);
        set_placeholder(std::move(placeholder));
        set_ghost_badge(FrameworkStrings::current().ghost_tab_badge);
        set_text(std::move(text));

        set_on_change([this](const std::string& v) {
            m_dismissed = false;
            recompute_ghost();
            if (m_user_on_changed) m_user_on_changed(v);
        });

        set_key_intercept([this](KeyEvent& ev) -> bool {
            if (ev.type != PlatformEventType::KeyDown) return false;
            if (ev.key_code == 0x09) { // Tab 逐词补全到下一个空格
                if (accept_next_word()) {
                    ev.stop_propagation();
                    return true;
                }
                return false;
            }
            if (ev.key_code == 0x27 && cursor_index() >= RenderTextBox::text().size() && !m_cur_ghost.empty()) {
                // 光标在末尾按右方向键同样逐词补全
                if (accept_next_word()) {
                    ev.stop_propagation();
                    return true;
                }
            }
            if (ev.key_code == 0x1B && !m_cur_ghost.empty()) { // Esc 忽略本次建议
                dismiss_ghost();
                ev.stop_propagation();
                return true;
            }
            return false;
        });

        recompute_ghost();
    }

    [[nodiscard]] const std::string& ghost_suffix() const noexcept { return m_cur_ghost; }

    void set_text(const std::string& s, bool notify = false) {
        RenderTextBox::set_text(s);
        set_cursor_index(s.size());
        m_dismissed = false;
        recompute_ghost();
        if (notify && m_user_on_changed) m_user_on_changed(RenderTextBox::text());
    }

    void set_suggestions(std::vector<std::string> s) {
        m_suggestions = std::move(s);
        recompute_ghost();
    }

    void set_on_changed(Fn<void(const std::string&)> cb) { m_user_on_changed = std::move(cb); }
    void set_on_accepted(Fn<void(const std::string&)> cb) { m_on_accepted = std::move(cb); }

    // 逐词补全：每次 Tab 仅补全到下一个空格边界（方便随时改写后续参数）
    bool accept_next_word() {
        if (m_cur_ghost.empty()) return false;
        size_t pos = 0;
        const size_t n = m_cur_ghost.size();
        while (pos < n && m_cur_ghost[pos] == ' ') ++pos;
        while (pos < n && m_cur_ghost[pos] != ' ') ++pos;
        if (pos < n && m_cur_ghost[pos] == ' ') ++pos;
        std::string next_text = RenderTextBox::text() + m_cur_ghost.substr(0, pos);
        RenderTextBox::set_text(next_text);
        set_cursor_index(next_text.size());
        m_dismissed = false;
        recompute_ghost();
        if (m_user_on_changed) m_user_on_changed(next_text);
        if (m_on_accepted) m_on_accepted(next_text);
        return true;
    }

    bool accept_ghost() {
        if (m_cur_ghost.empty()) return false;
        std::string next_text = RenderTextBox::text() + m_cur_ghost;
        RenderTextBox::set_text(next_text);
        set_cursor_index(next_text.size());
        m_dismissed = false;
        recompute_ghost();
        if (m_user_on_changed) m_user_on_changed(next_text);
        if (m_on_accepted) m_on_accepted(next_text);
        return true;
    }

    void dismiss_ghost() {
        if (!m_cur_ghost.empty()) {
            m_dismissed = true;
            m_cur_ghost.clear();
            set_ghost_suffix("");
        }
    }

private:
    static bool starts_with_icase(const std::string& full, const std::string& prefix) {
        if (prefix.empty() || full.size() <= prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(full[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i]))) {
                return false;
            }
        }
        return true;
    }

    void recompute_ghost() {
        m_cur_ghost.clear();
        const std::string& cur = RenderTextBox::text();
        if (!m_dismissed && !cur.empty()) {
            if (m_predictor) {
                std::string pred = m_predictor(cur);
                if (starts_with_icase(pred, cur)) {
                    m_cur_ghost = pred.substr(cur.size());
                } else if (!pred.empty() && pred != cur) {
                    m_cur_ghost = pred;
                }
            }
            if (m_cur_ghost.empty()) {
                for (const auto& s : m_suggestions) {
                    if (starts_with_icase(s, cur)) {
                        m_cur_ghost = s.substr(cur.size());
                        break;
                    }
                }
            }
        }
        set_ghost_suffix(m_cur_ghost);
    }

    std::vector<std::string> m_suggestions;
    Fn<std::string(const std::string&)> m_predictor;
    Fn<void(const std::string&)> m_user_on_changed;
    Fn<void(const std::string&)> m_on_accepted;
    std::string m_cur_ghost;
    bool m_dismissed{false};
};

struct GhostTextBox : BindableMixin<GhostTextBox, RenderGhostTextBox> {
    std::string text{""};
    std::string placeholder{FrameworkStrings::current().ghost_placeholder};
    std::vector<std::string> suggestions{};
    Fn<std::string(const std::string&)> predictor{nullptr};
    Fn<void(const std::string&)> on_changed{nullptr};
    Fn<void(const std::string&)> on_accepted{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class GhostTextBoxWidget : public Widget {
public:
    explicit GhostTextBoxWidget(const GhostTextBox& desc) : m_desc(desc) {}
    explicit GhostTextBoxWidget(GhostTextBox&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderGhostTextBox>(
            m_desc.text, m_desc.placeholder, m_desc.suggestions,
            m_desc.predictor, m_desc.on_changed, m_desc.on_accepted);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderGhostTextBox>(node)) {
            n->set_suggestions(m_desc.suggestions);
            n->set_on_changed(m_desc.on_changed);
            n->set_on_accepted(m_desc.on_accepted);
        }
    }
private:
    GhostTextBox m_desc;
};

inline GhostTextBox::operator WidgetPtr() const& { return make_widget<GhostTextBoxWidget>(*this); }
inline GhostTextBox::operator WidgetPtr() && { return make_widget<GhostTextBoxWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui
