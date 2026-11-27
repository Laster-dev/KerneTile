#pragma once

namespace ktui {

// ==================== TerminalController 内联方法实现 ====================
inline void TerminalController::set_backend(std::shared_ptr<ITerminalBackend> backend) {
    if (auto n = m_node.lock()) n->set_backend(std::move(backend));
}
inline void TerminalController::write_input(std::string_view utf8_bytes) {
    if (auto n = m_node.lock()) n->write_input(utf8_bytes);
}
inline void TerminalController::feed_output(std::string_view vt_bytes) {
    if (auto n = m_node.lock()) n->feed_output(vt_bytes);
}
inline void TerminalController::clear() {
    if (auto n = m_node.lock()) n->clear();
}
inline void TerminalController::set_color_scheme(TerminalColorScheme scheme) {
    if (auto n = m_node.lock()) n->set_color_scheme(std::move(scheme));
}
inline void TerminalController::set_cursor_style(TerminalCursorStyle style) {
    if (auto n = m_node.lock()) n->set_cursor_style(style);
}
inline void TerminalController::set_font_size(float pt) {
    if (auto n = m_node.lock()) n->set_font_size(pt);
}
inline void TerminalController::toggle_find() {
    if (auto n = m_node.lock()) n->toggle_find();
}
inline std::string TerminalController::title() const {
    if (auto n = m_node.lock()) return n->buffer().title();
    return {};
}
inline std::string TerminalController::backend_name() const {
    if (auto n = m_node.lock()) return n->backend_name();
    return {};
}
inline std::pair<int, int> TerminalController::grid_size() const {
    if (auto n = m_node.lock()) return {n->buffer().cols(), n->buffer().rows()};
    return {0, 0};
}

// ==================== 声明式 Terminal Widget ====================
struct Terminal {
    std::shared_ptr<ITerminalBackend> backend{nullptr}; // nullptr 时默认内置 DemoShellBackend；可传 ConPtyTerminalBackend / CallbackTerminalBackend / 任意自定义后端
    std::shared_ptr<TerminalController> controller{nullptr};
    TerminalColorScheme color_scheme{TerminalColorScheme::campbell()};
    TerminalCursorStyle cursor_style{TerminalCursorStyle::Bar};
    float font_size{13.5f};
    size_t max_scrollback{9001};
    bool copy_on_select{false};
    float flex_grow{0.0f};
    Fn<void(const std::string&)> on_title_changed{nullptr};
    Fn<void(int)> on_exit{nullptr};

    operator WidgetPtr() const;
};

class TerminalWidget : public Widget {
public:
    explicit TerminalWidget(Terminal desc) : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTerminal>(
            m_desc.backend,
            m_desc.color_scheme,
            m_desc.cursor_style,
            m_desc.font_size,
            m_desc.max_scrollback,
            m_desc.copy_on_select,
            m_desc.on_title_changed,
            m_desc.on_exit);
        if (m_desc.flex_grow > 0.0f) {
            node->flex_data().flex_grow = m_desc.flex_grow;
        }
        if (m_desc.controller) {
            m_desc.controller->bind(node);
        }
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto t = std::static_pointer_cast<RenderTerminal>(node)) {
            t->set_color_scheme(m_desc.color_scheme);
            t->set_cursor_style(m_desc.cursor_style);
            t->set_font_size(m_desc.font_size);
            t->set_copy_on_select(m_desc.copy_on_select);
            if (m_desc.controller) {
                m_desc.controller->bind(t);
            }
        }
    }

private:
    Terminal m_desc;
};

inline Terminal::operator WidgetPtr() const {
    return make_widget<TerminalWidget>(*this);
}

} // namespace ktui
