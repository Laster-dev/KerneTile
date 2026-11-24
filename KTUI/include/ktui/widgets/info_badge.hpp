#pragma once

#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace ktui {

enum class InfoBadgeSeverity : uint8_t {
    Attention = 0,
    Informational = 1,
    Success = 2,
    Caution = 3,
    Critical = 4
};

// WinUI 3 InfoBadge 状态角标渲染盒 (支持数字 99+、圆点、图标三形态与 5 种语义色)
class RenderInfoBadge : public RenderBox {
public:
    RenderInfoBadge() noexcept = default;
    ~RenderInfoBadge() override = default;

    void set_properties(int value,
                        InfoBadgeSeverity severity,
                        std::string svg_path,
                        int max_value) {
        if (m_value != value || m_severity != severity || m_svg_path != svg_path || m_max_value != max_value) {
            m_value = value;
            m_severity = severity;
            m_svg_path = std::move(svg_path);
            m_max_value = std::max(1, max_value);
            mark_needs_layout();
        }
    }

    void set_value(int v) {
        if (m_value != v) {
            m_value = v;
            mark_needs_layout();
        }
    }

    [[nodiscard]] int value() const noexcept { return m_value; }

    [[nodiscard]] std::string formatted_text() const {
        if (m_value < 0) return {};
        if (m_value > m_max_value) return std::to_string(m_max_value) + "+";
        return std::to_string(m_value);
    }

    void perform_layout() override {
        if (m_value < 0 && m_svg_path.empty()) {
            set_size(m_constraints.constrain(Vec2{10.0f, 10.0f}));
            return;
        }
        if (!m_svg_path.empty() && m_value < 0) {
            set_size(m_constraints.constrain(Vec2{18.0f, 18.0f}));
            return;
        }
        std::string txt = formatted_text();
        float tw = TextMeasure::measure_width(txt, 11.0f);
        float w = std::max(18.0f, std::ceil(tw + 10.0f));
        set_size(m_constraints.constrain(Vec2{w, 18.0f}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        Color bg = resolve_bg(theme);
        Color fg = (m_severity == InfoBadgeSeverity::Caution)
            ? Color{0.08f, 0.08f, 0.08f, 1.0f}
            : Color{1.0f, 1.0f, 1.0f, 1.0f};

        const float w = size().x;
        const float h = size().y;
        float radius = std::min(w, h) * 0.5f;
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, h), bg, radius, 0.0f);

        if (m_value >= 0) {
            std::string txt = formatted_text();
            Vec2 ts = TextMeasure::measure(txt, 11.0f);
            ctx.draw_text(txt,
                          PointF{std::round((w - ts.x) * 0.5f), std::round((h - ts.y) * 0.5f)},
                          11.0f, fg);
        } else if (!m_svg_path.empty()) {
            float isz = 10.0f;
            Rect ir = Rect::from_xywh(std::round((w - isz) * 0.5f), std::round((h - isz) * 0.5f), isz, isz);
            SvgEngine::draw_svg_path(ctx, m_svg_path, 24.0f, 24.0f, ir, fg);
        }
    }

private:
    Color resolve_bg(const Theme& theme) const noexcept {
        switch (m_severity) {
            case InfoBadgeSeverity::Attention:     return theme.colors.primary;
            case InfoBadgeSeverity::Informational: return theme.colors.info;
            case InfoBadgeSeverity::Success:       return theme.colors.success;
            case InfoBadgeSeverity::Caution:       return theme.colors.warning;
            case InfoBadgeSeverity::Critical:      return theme.colors.error;
        }
        return theme.colors.primary;
    }

    int m_value{-1}; // < 0 表示圆点或图标模式
    InfoBadgeSeverity m_severity{InfoBadgeSeverity::Attention};
    std::string m_svg_path;
    int m_max_value{99};
};

// C++20 指定初始化器声明式 WinUI 3 InfoBadge
struct InfoBadge : BindableMixin<InfoBadge, RenderInfoBadge> {
    int value{-1};
    InfoBadgeSeverity severity{InfoBadgeSeverity::Attention};
    std::string svg_path;
    int max_value{99};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class InfoBadgeWidget : public Widget {
public:
    explicit InfoBadgeWidget(const InfoBadge& desc) : m_desc(desc) {}
    explicit InfoBadgeWidget(InfoBadge&& desc) noexcept : m_desc(std::move(desc)) {}
    ~InfoBadgeWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderInfoBadge>();
        node->set_properties(m_desc.value, m_desc.severity, m_desc.svg_path, m_desc.max_value);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderInfoBadge>(node)) {
            n->set_properties(m_desc.value, m_desc.severity, m_desc.svg_path, m_desc.max_value);
        }
    }

private:
    InfoBadge m_desc;
};

inline InfoBadge::operator WidgetPtr() const& {
    return make_widget<InfoBadgeWidget>(*this);
}
inline InfoBadge::operator WidgetPtr() && {
    return make_widget<InfoBadgeWidget>(std::move(*this));
}

} // namespace ktui
