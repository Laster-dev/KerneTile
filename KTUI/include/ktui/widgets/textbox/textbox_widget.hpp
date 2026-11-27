#pragma once

namespace ktui {

class TextBoxWidget;

// C++20 指定初始化器声明式单行文本框 (TextBox DSL Wrapper - 零开销原生字段 + 链式 .bind/.bind_two_way)
struct TextBox : BindableMixin<TextBox, RenderTextBox> {
    std::string text{""};
    std::string placeholder{""};

    float height{0.0f};           // 0 表示按主题解析默认值 28px
    float corner_radius{0.0f};    // 0 表示按主题解析默认值 3px
    EdgeInsets padding{EdgeInsets(-1.0f, -1.0f)}; // 负值代表采用主题解析默认值
    Color background_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color focused_border_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color text_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    Color placeholder_color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};
    float font_size{0.0f};

    bool is_password{false};
    bool is_read_only{false};
    bool disabled{false};
    bool show_reveal_button{false};
    bool smooth_caret{true};   // 默认启用平滑弹簧光标 + 呼吸渐变闪烁，可手动关闭
    bool inset_shadow{false};  // 可选内凹微阴影质感

    Fn<void(const std::string&)> on_change{nullptr};
    Fn<void(const std::string&)> on_submit{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderTextBox>& node, Property<std::string>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_text(prop.get());
    std::weak_ptr<RenderTextBox> weak = node;
    bg.subscribe(prop, [weak, guard](const std::string& v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_text(v);
        }
    });
    auto prev = node->data().on_change;
    Property<std::string>* p = &prop;
    node->set_on_change([prev = std::move(prev), p, guard](const std::string& val) {
        *guard = true;
        p->set(val);
        if (prev) prev(val);
        *guard = false;
    });
}

// TextBoxWidget 声明式包装组件
class TextBoxWidget : public Widget {
public:
    explicit TextBoxWidget(const TextBox& desc) : m_desc(desc) {}
    explicit TextBoxWidget(TextBox&& desc) noexcept : m_desc(std::move(desc)) {}
    ~TextBoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTextBox>();
        apply_desc_to_node(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto box = std::static_pointer_cast<RenderTextBox>(node)) {
            apply_desc_to_node(box);
        }
    }

    [[nodiscard]] const TextBox& desc() const noexcept { return m_desc; }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderTextBox>& node) {
        node->set_text(m_desc.text);
        node->set_placeholder(m_desc.placeholder);
        node->set_is_password(m_desc.is_password);
        node->set_read_only(m_desc.is_read_only);
        node->set_disabled(m_desc.disabled);
        node->set_show_reveal_button(m_desc.show_reveal_button);
        node->set_smooth_caret(m_desc.smooth_caret);
        node->set_inset_shadow(m_desc.inset_shadow);

        std::optional<Color> bg = (m_desc.background_color.r >= 0.0f) ? std::optional<Color>(m_desc.background_color) : std::nullopt;
        std::optional<Color> border = (m_desc.border_color.r >= 0.0f) ? std::optional<Color>(m_desc.border_color) : std::nullopt;
        std::optional<Color> focus_border = (m_desc.focused_border_color.r >= 0.0f) ? std::optional<Color>(m_desc.focused_border_color) : std::nullopt;
        std::optional<Color> txt = (m_desc.text_color.r >= 0.0f) ? std::optional<Color>(m_desc.text_color) : std::nullopt;
        std::optional<Color> ph = (m_desc.placeholder_color.r >= 0.0f) ? std::optional<Color>(m_desc.placeholder_color) : std::nullopt;
        std::optional<float> h = (m_desc.height > 0.0f) ? std::optional<float>(m_desc.height) : std::nullopt;
        std::optional<float> radius = (m_desc.corner_radius > 0.0f) ? std::optional<float>(m_desc.corner_radius) : std::nullopt;
        std::optional<EdgeInsets> pad = (m_desc.padding.left >= 0.0f) ? std::optional<EdgeInsets>(m_desc.padding) : std::nullopt;
        std::optional<float> font_sz = (m_desc.font_size > 0.0f) ? std::optional<float>(m_desc.font_size) : std::nullopt;

        node->apply_custom_overrides(bg, border, focus_border, txt, ph, h, radius, pad, font_sz);

        node->set_properties(m_desc.text, m_desc.placeholder,
                             h.value_or(28.0f), radius.value_or(5.0f),
                             pad.value_or(EdgeInsets(4.0f, 8.0f)),
                             m_desc.background_color, m_desc.border_color, m_desc.focused_border_color,
                             m_desc.text_color, m_desc.placeholder_color,
                             font_sz.value_or(13.0f), m_desc.on_change, m_desc.on_submit);
    }

    TextBox m_desc;
};

inline TextBox::operator WidgetPtr() const& {
    return make_widget<TextBoxWidget>(*this);
}
inline TextBox::operator WidgetPtr() && {
    return make_widget<TextBoxWidget>(std::move(*this));
}

// ==================== 100% 向后兼容别名 (Type Aliases) ====================
using TextInput = TextBox;
using RenderTextInput = RenderTextBox;
using TextInputWidget = TextBoxWidget;

} // namespace ktui
