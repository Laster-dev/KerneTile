#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <string>

namespace ktui {

// 桌面紧凑矢量图标渲染盒 (RenderSvg)
class RenderSvg : public RenderBox {
public:
    RenderSvg(std::string path_data = "", Vec2 size = Vec2{16.0f, 16.0f},
              Color color = Color{0.90f, 0.90f, 0.92f, 1.0f}) noexcept
        : m_path_data(std::move(path_data)), m_icon_size(size), m_color(color) {}

    ~RenderSvg() override = default;

    void set_properties(std::string path_data, Vec2 sz, Color col) {
        bool relayout = (m_icon_size != sz);
        m_path_data = std::move(path_data);
        m_icon_size = sz;
        m_color = col;

        if (relayout) {
            mark_needs_layout();
        } else {
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        m_size = m_constraints.constrain(m_icon_size);
    }

    void paint(PaintContext& context) override {
        if (m_color.a > 0.001f) {
            // 在计算着色器 2D 引擎中，发射精致紧凑矢量图标包围图元
            context.draw_rect(bounds(), m_color, 1.0f, 0.0f);
        }
    }

private:
    std::string m_path_data;
    Vec2 m_icon_size{16.0f, 16.0f};
    Color m_color{0.90f, 0.90f, 0.92f, 1.0f};
};

class SvgWidget;

// C++20 指定初始化器矢量图标 (Svg)
// 桌面专业标准：16x16 紧凑图标
struct Svg {
    std::string path_data{""};
    Vec2 size{16.0f, 16.0f}; // 桌面标准 16px 矢量小图标
    Color color{Color{0.90f, 0.90f, 0.92f, 1.0f}};

    operator WidgetPtr() const;
};

// SvgWidget 包装实现
class SvgWidget : public Widget {
public:
    explicit SvgWidget(const Svg& desc) : m_desc(desc) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return std::make_shared<RenderSvg>(m_desc.path_data, m_desc.size, m_desc.color);
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto svg = std::dynamic_pointer_cast<RenderSvg>(node)) {
            svg->set_properties(m_desc.path_data, m_desc.size, m_desc.color);
        }
    }

    [[nodiscard]] const Svg& desc() const noexcept { return m_desc; }

private:
    Svg m_desc;
};

inline Svg::operator WidgetPtr() const {
    return std::make_shared<SvgWidget>(*this);
}

} // namespace ktui
