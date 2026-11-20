#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <functional>
#include <string>

namespace ktui {

// 桌面级紧凑按钮渲染盒 (RenderButton)
// 集成 ButtonBehavior (事件状态机) 与 ButtonPainter (高信息密度硬朗绘制)
class RenderButton : public RenderBox {
public:
    RenderButton() noexcept {
        setup_behavior();
    }

    ~RenderButton() override = default;

    void set_properties(std::string text, float h, float radius, EdgeInsets pad,
                        Color bg, Color hover, Color active, Color txt_col,
                        Color border_col, float border_w, float font_sz,
                        std::function<void()> click_cb, bool is_disabled) {
        m_text = std::move(text);
        m_height = h;
        m_corner_radius = radius;
        m_padding = pad;
        m_bg_color = bg;
        m_hover_color = hover;
        m_active_color = active;
        m_text_color = txt_col;
        m_border_color = border_col;
        m_border_width = border_w;
        m_font_size = font_sz;
        m_on_click = std::move(click_cb);
        m_disabled = is_disabled;

        mark_needs_layout();
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] float font_size() const noexcept { return m_font_size; }
    [[nodiscard]] Color text_color() const noexcept { return m_text_color; }
    [[nodiscard]] bool disabled() const noexcept { return m_disabled; }

    void set_disabled(bool disabled) {
        if (m_disabled != disabled) {
            m_disabled = disabled;
            if (m_disabled) {
                is_hovered = false;
                is_active = false;
            }
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        // 计算文本所占空间
        float text_w = static_cast<float>(m_text.length()) * m_font_size * 0.50f;
        float desired_w = text_w + m_padding.horizontal();
        float desired_h = m_height;

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
    }

    void paint(PaintContext& context) override {
        // 根据状态决定当前表面底色
        Color current_bg = m_bg_color;
        if (m_disabled) {
            current_bg = Color{0.15f, 0.15f, 0.16f, 0.5f};
        } else if (is_active) {
            current_bg = m_active_color;
        } else if (is_hovered) {
            current_bg = m_hover_color;
        }

        Color current_border = m_disabled ? Color{0.2f, 0.2f, 0.2f, 0.4f} : m_border_color;
        Color current_text = m_disabled ? Color{0.5f, 0.5f, 0.5f, 0.6f} : m_text_color;

        // 1. 绘制硬朗微圆角实心背景与 1px 细边框 (杜绝大圆角)
        if (current_bg.a > 0.001f) {
            context.draw_rect(bounds(), current_bg, m_corner_radius, 0.0f);
        }
        if (m_border_width > 0.0f && current_border.a > 0.001f) {
            context.draw_rect(bounds(), current_border, m_corner_radius, m_border_width);
        }

        // 2. 居中文本排版
        if (!m_text.empty()) {
            float text_w = static_cast<float>(m_text.length()) * m_font_size * 0.50f;
            float text_h = m_font_size * 1.25f;
            float text_x = std::max(0.0f, (m_size.x - text_w) * 0.5f);
            float text_y = std::max(0.0f, (m_size.y - text_h) * 0.5f);

            context.draw_text(m_text, PointF{text_x, text_y}, m_font_size, current_text);
        }
    }

private:
    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (!m_disabled) {
                is_hovered = true;
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            if (!m_disabled) {
                is_hovered = false;
                is_active = false;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (!m_disabled && ev.button == PointerButton::Primary) {
                is_active = true;
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (!m_disabled && ev.button == PointerButton::Primary) {
                if (is_active && is_hovered) {
                    if (m_on_click) {
                        m_on_click();
                    }
                }
                is_active = false;
                mark_needs_paint();
            }
        };
    }

    std::string m_text;
    float m_height{26.0f};          // 桌面高信息密度标准高度 26px
    float m_corner_radius{3.0f};    // 桌面精致硬朗微圆角 3px，绝非大胶囊
    EdgeInsets m_padding{EdgeInsets(4.0f, 10.0f)}; // 紧凑 4px 上下，10px 左右
    Color m_bg_color{Color{0.20f, 0.20f, 0.23f, 1.0f}};
    Color m_hover_color{Color{0.27f, 0.27f, 0.31f, 1.0f}};
    Color m_active_color{Color{0.15f, 0.15f, 0.17f, 1.0f}};
    Color m_text_color{Color{0.92f, 0.92f, 0.94f, 1.0f}};
    Color m_border_color{Color{0.32f, 0.32f, 0.36f, 0.8f}};
    float m_border_width{1.0f};     // 1px 精致细边框
    float m_font_size{13.0f};       // 桌面基准 13px
    std::function<void()> m_on_click{nullptr};
    bool m_disabled{false};
};

class ButtonWidget;

// C++20 指定初始化器专业桌面按钮 (Button)
// 桌面专业标准：高度 26px，硬朗微圆角 3px，紧凑边距，1px 细线边框
struct Button {
    std::string text{""};
    float height{26.0f};          // 桌面高信息密度紧凑高度 26px
    float corner_radius{3.0f};    // 桌面硬朗微圆角 3px (杜绝大圆角)
    EdgeInsets padding{EdgeInsets(4.0f, 10.0f)}; // 紧凑桌面内边距
    Color background_color{Color{0.20f, 0.20f, 0.23f, 1.0f}};
    Color hover_color{Color{0.27f, 0.27f, 0.31f, 1.0f}};
    Color active_color{Color{0.15f, 0.15f, 0.17f, 1.0f}};
    Color text_color{Color{0.92f, 0.92f, 0.94f, 1.0f}};
    Color border_color{Color{0.32f, 0.32f, 0.36f, 0.8f}};
    float border_width{1.0f};     // 1px 精致细边框
    float font_size{13.0f};       // 桌面基准 13px
    std::function<void()> on_click{nullptr};
    bool disabled{false};

    operator WidgetPtr() const;
};

// ButtonWidget 包装实现
class ButtonWidget : public Widget {
public:
    explicit ButtonWidget(const Button& desc) : m_desc(desc) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderButton>();
        node->set_properties(m_desc.text, m_desc.height, m_desc.corner_radius, m_desc.padding,
                             m_desc.background_color, m_desc.hover_color, m_desc.active_color,
                             m_desc.text_color, m_desc.border_color, m_desc.border_width,
                             m_desc.font_size, m_desc.on_click, m_desc.disabled);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto btn = std::dynamic_pointer_cast<RenderButton>(node)) {
            btn->set_properties(m_desc.text, m_desc.height, m_desc.corner_radius, m_desc.padding,
                                m_desc.background_color, m_desc.hover_color, m_desc.active_color,
                                m_desc.text_color, m_desc.border_color, m_desc.border_width,
                                m_desc.font_size, m_desc.on_click, m_desc.disabled);
        }
    }

    [[nodiscard]] const Button& desc() const noexcept { return m_desc; }

private:
    Button m_desc;
};

inline Button::operator WidgetPtr() const {
    return std::make_shared<ButtonWidget>(*this);
}

} // namespace ktui
