#pragma once

namespace ktui {

// C++20 指定初始化器声明式 WinUI 3 TextArea
struct TextArea : BindableMixin<TextArea, RenderTextArea> {
    std::string text{std::string{}};
    std::string placeholder{FrameworkStrings::current().multiline_placeholder};
    std::string header;
    float width{0.0f};
    float height{120.0f};
    bool disabled{false};
    bool smooth_caret{true}; // 默认启用平滑弹簧光标 + 呼吸渐变闪烁，可手动关闭
    Fn<void(const std::string&)> on_change{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderTextArea>& node, Property<std::string>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_text_value(prop.get());
    std::weak_ptr<RenderTextArea> weak = node;
    bg.subscribe(prop, [weak, guard](const std::string& v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_text_value(v);
        }
    });
    Property<std::string>* p = &prop;
    node->wrap_on_change([p, guard](const std::string& v) {
        *guard = true;
        p->set(v);
        *guard = false;
    });
}

class TextAreaWidget : public Widget {
public:
    explicit TextAreaWidget(const TextArea& desc) : m_desc(desc) {}
    explicit TextAreaWidget(TextArea&& desc) noexcept : m_desc(std::move(desc)) {}
    ~TextAreaWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTextArea>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderTextArea>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderTextArea>& node) {
        node->set_properties(
            m_desc.text, m_desc.placeholder, m_desc.header,
            m_desc.width, m_desc.height, m_desc.disabled, m_desc.on_change);
        node->set_smooth_caret(m_desc.smooth_caret);
    }

    TextArea m_desc;
};

inline TextArea::operator WidgetPtr() const& {
    return make_widget<TextAreaWidget>(*this);
}
inline TextArea::operator WidgetPtr() && {
    return make_widget<TextAreaWidget>(std::move(*this));
}

} // namespace ktui
