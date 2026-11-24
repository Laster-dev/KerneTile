#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/pal/font_resolver.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <optional>
#include <string>

namespace ktui {

// 文本溢出处理策略
enum class TextOverflow : uint8_t {
    Clip,       // 硬裁剪
    Ellipsis,   // 超出宽度自动截断并追加 "..." (UTF-8 安全)
    Wrap        // 超出宽度自动多行折行
};

// 文本渲染盒 (RenderText)
class RenderText : public RenderBox {
public:
    RenderText(std::string text = "", float font_size = 13.0f,
               Color color = Color{0.0f, 0.0f, 0.0f, -1.0f},
               FontWeight weight = FontWeight::Regular,
               TextOverflow overflow = TextOverflow::Ellipsis) noexcept
        : m_text(std::move(text)), m_font_size(font_size), m_color(color),
          m_font_weight(weight), m_overflow(overflow) {}

    ~RenderText() override = default;

    void set_text(std::string text) {
        if (m_text != text) {
            m_text = std::move(text);
            mark_needs_layout();
        }
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] float font_size() const noexcept { return m_font_size; }
    [[nodiscard]] Color color() const noexcept { return m_color; }

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

    void set_font_weight(FontWeight w) {
        if (m_font_weight != w) {
            m_font_weight = w;
            mark_needs_layout();
        }
    }

    void set_overflow(TextOverflow ov) {
        if (m_overflow != ov) {
            m_overflow = ov;
            mark_needs_layout();
        }
    }

    void perform_layout() override {
        m_wrapped_lines.clear();
        m_elided_text.clear();

        Vec2 measured = TextMeasure::measure(m_text, m_font_size);
        float max_w = m_constraints.max_width;
        const auto stops = kernetile::text::grapheme_boundaries(m_text);
        const auto next_stop = [&stops](size_t pos) { return *std::upper_bound(stops.begin(), stops.end(), pos); };

        if (m_overflow == TextOverflow::Wrap && max_w < BoxConstraints::kInfinity && measured.x > max_w && !m_text.empty()) {
            // UTF-8 码点安全自动换行
            size_t line_start = 0;
            size_t pos = 0;
            while (pos < m_text.size()) {
                size_t next = next_stop(pos);
                if (m_text[pos] == '\n' || m_text[pos] == '\r') {
                    m_wrapped_lines.emplace_back(m_text.substr(line_start, pos - line_start));
                    line_start = next;
                    pos = next;
                    continue;
                }
                float w = TextMeasure::measure_width(std::string_view(m_text).substr(line_start, next - line_start), m_font_size);
                if (w > max_w && pos > line_start) {
                    m_wrapped_lines.emplace_back(m_text.substr(line_start, pos - line_start));
                    line_start = pos;
                } else {
                    pos = next;
                }
            }
            if (line_start < m_text.size()) {
                m_wrapped_lines.emplace_back(m_text.substr(line_start));
            }
            float line_h = TextMeasure::measure({}, m_font_size).y;
            m_size = m_constraints.constrain(Vec2{max_w, line_h * static_cast<float>(std::max<size_t>(1, m_wrapped_lines.size()))});
        } else {
            m_size = m_constraints.constrain(measured);
            if (m_overflow == TextOverflow::Ellipsis && measured.x > m_size.x + 0.5f && !m_text.empty()) {
                float ellipsis_w = TextMeasure::measure_width("...", m_font_size);
                float avail_w = std::max(0.0f, m_size.x - ellipsis_w);
                size_t pos = 0;
                size_t best_end = 0;
                while (pos < m_text.size()) {
                    size_t next = next_stop(pos);
                    float w = TextMeasure::measure_width(std::string_view(m_text).substr(0, next), m_font_size);
                    if (w > avail_w) break;
                    best_end = next;
                    pos = next;
                }
                m_elided_text = m_text.substr(0, best_end) + "...";
            }
        }
    }

    void paint(PaintContext& context) override {
        // alpha < 0 表示自动跟随当前深浅色主题的 text_primary，彻底消除浅色模式下白字问题
        const Color draw_col = (m_color.a < 0.0f) ? ThemeContext::current().colors.text_primary : m_color;
        if (m_text.empty() || draw_col.a <= 0.001f) return;

        // 强制硬件视口裁剪，绝不允许文本溢出自身包围盒
        context.push_clip(bounds(), 0.0f);
        if (m_overflow == TextOverflow::Wrap && !m_wrapped_lines.empty()) {
            float line_h = TextMeasure::measure("A", m_font_size).y;
            float y = 0.0f;
            for (const auto& line : m_wrapped_lines) {
                if (!line.empty()) {
                    context.draw_text(line, PointF{0.0f, y}, m_font_size, draw_col);
                }
                y += line_h;
            }
        } else {
            const std::string& to_draw = m_elided_text.empty() ? m_text : m_elided_text;
            context.draw_text(to_draw, PointF{0.0f, 0.0f}, m_font_size, draw_col);
        }
        context.pop_clip();
    }

private:
    std::string m_text;
    std::string m_elided_text;
    std::vector<std::string> m_wrapped_lines;
    float m_font_size{13.0f}; // 桌面基准 13px
    Color m_color{0.0f, 0.0f, 0.0f, -1.0f};
    FontWeight m_font_weight{FontWeight::Regular};
    TextOverflow m_overflow{TextOverflow::Ellipsis};
};

class TextWidget;

// C++20 指定初始化器文本控件 (Text)
// 桌面专业标准：默认 13px 紧凑字阶，颜色默认跟随当前主题，支持 Clip / Ellipsis / Wrap 防溢出与全属性 Bind(...)
struct Text {
    Bindable<std::string> text{""};
    Bindable<float> font_size{13.0f}; // 桌面 IDE 基准字阶 (13px)
    Bindable<Color> color{Color{0.0f, 0.0f, 0.0f, -1.0f}};
    Bindable<FontWeight> font_weight{FontWeight::Regular};
    Bindable<TextOverflow> overflow{TextOverflow::Ellipsis};

    operator WidgetPtr() const;
};

// TextWidget 包装实现
class TextWidget : public Widget {
public:
    explicit TextWidget(const Text& desc) : m_desc(desc) {}
    ~TextWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderText>(
            m_desc.text.get(), m_desc.font_size.get(), m_desc.color.get(),
            m_desc.font_weight.get(), m_desc.overflow.get());
        bind_all(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto text_box = std::dynamic_pointer_cast<RenderText>(node)) {
            text_box->set_text(m_desc.text.get());
            text_box->set_font_size(m_desc.font_size.get());
            text_box->set_color(m_desc.color.get());
            text_box->set_font_weight(m_desc.font_weight.get());
            text_box->set_overflow(m_desc.overflow.get());
            bind_all(text_box);
        }
    }

    [[nodiscard]] const Text& desc() const noexcept { return m_desc; }

private:
    void bind_all(const std::shared_ptr<RenderText>& text_box) {
        m_bindings.clear();
        m_bindings.attach(m_desc.text, text_box, [](RenderText& n, const std::string& v) { n.set_text(v); });
        m_bindings.attach(m_desc.font_size, text_box, [](RenderText& n, float v) { n.set_font_size(v); });
        m_bindings.attach(m_desc.color, text_box, [](RenderText& n, Color v) { n.set_color(v); });
        m_bindings.attach(m_desc.font_weight, text_box, [](RenderText& n, FontWeight v) { n.set_font_weight(v); });
        m_bindings.attach(m_desc.overflow, text_box, [](RenderText& n, TextOverflow v) { n.set_overflow(v); });
    }

    Text m_desc;
    BindingGroup m_bindings;
};

inline Text::operator WidgetPtr() const {
    return std::make_shared<TextWidget>(*this);
}

} // namespace ktui
