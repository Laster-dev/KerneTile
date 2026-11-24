#pragma once

#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace ktui {

// NanoSVG 矢量渲染盒 (RenderSvg)
class RenderSvg : public RenderBox {
public:
    RenderSvg() noexcept = default;
    ~RenderSvg() override = default;

    void set_source(std::string src_file, std::string xml_data, std::string path_data,
                    float viewbox_size, float stroke_width, bool fill) {
        m_src = std::move(src_file);
        m_xml = std::move(xml_data);
        m_path = std::move(path_data);
        m_viewbox = viewbox_size > 0.0f ? viewbox_size : 24.0f;
        m_stroke_width = stroke_width;
        m_fill = fill;
        mark_needs_paint();
    }

    void set_dimensions(float w, float h) {
        if (std::abs(m_width - w) > 0.001f || std::abs(m_height - h) > 0.001f) {
            m_width = w > 0.0f ? w : 20.0f;
            m_height = h > 0.0f ? h : 20.0f;
            mark_needs_layout();
        }
    }

    void set_tint(Color c) {
        if (m_tint != c) {
            m_tint = c;
            mark_needs_paint();
        }
    }

    void set_rotation(float rad) {
        if (std::abs(m_rotation - rad) > 0.0005f) {
            m_rotation = rad;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        m_size = m_constraints.constrain(Vec2{m_width, m_height});
    }

    void paint(PaintContext& ctx) override {
        const Color draw_col = (m_tint.a < 0.0f) ? ThemeContext::current().colors.text_primary : m_tint;
        if (draw_col.a <= 0.001f) return;

        Rect r = bounds();
        if (!m_xml.empty()) {
            SvgEngine::draw_svg_xml(ctx, m_xml, r, draw_col, m_rotation);
        } else if (!m_path.empty()) {
            SvgEngine::draw_svg_path(ctx, m_path, m_viewbox, m_viewbox, r, draw_col, m_stroke_width, m_fill, m_rotation);
        } else if (!m_src.empty()) {
            SvgEngine::draw_svg_file(ctx, m_src, r, draw_col, m_rotation);
        }
    }

private:
    std::string m_src;
    std::string m_xml;
    std::string m_path;
    float m_viewbox{24.0f};
    float m_stroke_width{0.0f};
    bool m_fill{true};
    float m_width{20.0f};
    float m_height{20.0f};
    Color m_tint{0.0f, 0.0f, 0.0f, -1.0f}; // 默认自动跟随当前主题文字色
    float m_rotation{0.0f};
};

// C++20 指定初始化器声明式 SVG 组件 (支持 .src 文件 / .xml 内存字符串 / .path 路径数据)
struct Svg : BindableMixin<Svg, RenderSvg> {
    std::string src{""};
    std::string xml{""};
    std::string path{""};
    float viewbox_size{24.0f};
    float stroke_width{0.0f};
    bool fill{true};
    float width{20.0f};
    float height{20.0f};
    Color tint{Color{0.0f, 0.0f, 0.0f, -1.0f}};
    float rotation{0.0f};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class SvgWidget : public Widget {
public:
    explicit SvgWidget(const Svg& desc) : m_desc(desc) {}
    explicit SvgWidget(Svg&& desc) noexcept : m_desc(std::move(desc)) {}
    ~SvgWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderSvg>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto s = std::static_pointer_cast<RenderSvg>(node)) {
            apply(s);
        }
    }

private:
    void apply(const std::shared_ptr<RenderSvg>& node) {
        node->set_source(m_desc.src, m_desc.xml, m_desc.path, m_desc.viewbox_size, m_desc.stroke_width, m_desc.fill);
        node->set_dimensions(m_desc.width, m_desc.height);
        node->set_tint(m_desc.tint);
        node->set_rotation(m_desc.rotation);
    }

    Svg m_desc;
};

inline Svg::operator WidgetPtr() const& {
    return make_widget<SvgWidget>(*this);
}
inline Svg::operator WidgetPtr() && {
    return make_widget<SvgWidget>(std::move(*this));
}

} // namespace ktui
