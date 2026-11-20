#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/pal/font_resolver.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <optional>
#include <string>

namespace ktui {

// 文本渲染盒 (RenderText)
class RenderText : public RenderBox {
public:
    RenderText(std::string text = "", float font_size = 13.0f,
               Color color = Color{0.9f, 0.9f, 0.9f, 1.0f},
               FontWeight weight = FontWeight::Regular) noexcept
        : m_text(std::move(text)), m_font_size(font_size), m_color(color), m_font_weight(weight) {}

    ~RenderText() override = default;

    void set_text(std::string text) {
        if (m_text != text) {
            m_text = std::move(text);
            mark_needs_layout();
        }
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

    void set_font_size(float size) {
        if (m_font_size != size) {
            m_font_size = size;
            mark_needs_layout();
        }
    }

    void set_color(Color c) {
        if (m_color != c) {
            m_color = c;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        // 估算文本物理尺寸 (以桌面紧凑英文字符比例约 0.58 测算，中英混合自适应)
        float estimated_w = static_cast<float>(m_text.length()) * m_font_size * 0.58f;
        float estimated_h = m_font_size * 1.35f;

        m_size = m_constraints.constrain(Vec2{estimated_w, estimated_h});
    }

    void paint(PaintContext& context) override {
        if (!m_text.empty() && m_color.a > 0.001f) {
            context.draw_text(m_text, PointF{0.0f, 0.0f}, m_font_size, m_color);
        }
    }

private:
    std::string m_text;
    float m_font_size{13.0f}; // 桌面基准 13px
    Color m_color{0.9f, 0.9f, 0.9f, 1.0f};
    FontWeight m_font_weight{FontWeight::Regular};
};

class TextWidget;

// C++20 指定初始化器文本控件 (Text)
// 桌面专业标准：默认 13px 紧凑字阶
struct Text {
    std::string text{""};
    BindRef<std::string> bind_text{nullptr};
    float font_size{13.0f}; // 桌面 IDE 基准字阶 (13px)
    Color color{Color{0.90f, 0.90f, 0.92f, 1.0f}};
    FontWeight font_weight{FontWeight::Regular};

    operator WidgetPtr() const;
};

// TextWidget 包装实现
class TextWidget : public Widget {
public:
    explicit TextWidget(const Text& desc) : m_desc(desc) {}

    ~TextWidget() override {
        unsubscribe();
    }

    std::shared_ptr<RenderBox> create_render_node() override {
        std::string initial_text = m_desc.bind_text ? m_desc.bind_text->get() : m_desc.text;
        auto node = std::make_shared<RenderText>(initial_text, m_desc.font_size, m_desc.color, m_desc.font_weight);

        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto text_box = std::dynamic_pointer_cast<RenderText>(node)) {
            std::string current_val = m_desc.bind_text ? m_desc.bind_text->get() : m_desc.text;
            text_box->set_text(current_val);
            text_box->set_font_size(m_desc.font_size);
            text_box->set_color(m_desc.color);

            setup_binding(text_box);
        }
    }

    [[nodiscard]] const Text& desc() const noexcept { return m_desc; }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_text) {
            m_desc.bind_text->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderText> text_box) {
        unsubscribe();
        if (m_desc.bind_text) {
            std::weak_ptr<RenderText> weak_box = text_box;
            m_binding_token = m_desc.bind_text->subscribe([weak_box](const std::string& new_val) {
                if (auto box = weak_box.lock()) {
                    box->set_text(new_val);
                }
            });
        }
    }

    Text m_desc;
    uint64_t m_binding_token{0};
};

inline Text::operator WidgetPtr() const {
    return std::make_shared<TextWidget>(*this);
}

} // namespace ktui
