#pragma once

#include "label_data.hpp"
#include "label_style.hpp"
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <cmath>

namespace ktui {

// WinUI 3 语义标签渲染盒 (RenderLabel)
// 支持纯文本标签、必填红星 (*)、前置 NanoSVG 图标以及 Fluent 2 语义胶囊标签 (pill = true)
class RenderLabel : public RenderBox {
public:
    RenderLabel() noexcept = default;
    ~RenderLabel() override = default;

    void set_properties(const std::string& text,
                        LabelVariant variant,
                        bool pill,
                        bool required,
                        const std::string& svg_path,
                        float font_size_override,
                        bool disabled,
                        FontWeight font_weight = FontWeight::Regular,
                        bool bold = false,
                        bool italic = false,
                        bool underline = false,
                        bool strikethrough = false) {
        FontWeight eff_weight = bold ? FontWeight::Bold : font_weight;
        if (eff_weight == FontWeight::Regular &&
            (variant == LabelVariant::Title || variant == LabelVariant::Subtitle)) {
            eff_weight = FontWeight::SemiBold;
        }
        bool relayout = (m_text != text || m_variant != variant || m_pill != pill ||
                         m_required != required || m_svg_path != svg_path ||
                         m_font_weight != eff_weight || m_italic != italic ||
                         std::abs(m_font_size_override - font_size_override) > 0.001f);
        m_text = text;
        m_variant = variant;
        m_pill = pill;
        m_required = required;
        m_svg_path = svg_path;
        m_font_size_override = font_size_override;
        m_disabled = disabled;
        m_font_weight = eff_weight;
        m_italic = italic;
        m_underline = underline;
        m_strikethrough = strikethrough;
        if (relayout) mark_needs_layout();
        else mark_needs_paint();
    }

    void set_text_value(const std::string& t) {
        if (m_text != t) {
            m_text = t;
            mark_needs_layout();
        }
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

    void perform_layout() override {
        const float fs = effective_font_size();
        const float pad_x = m_pill ? 10.0f : 0.0f;
        const float pad_y = m_pill ? 4.0f : 2.0f;
        const bool has_icon = !m_svg_path.empty();
        const float icon_sz = std::clamp(fs + 1.0f, 12.0f, 18.0f);

        Vec2 ts = m_text.empty() ? Vec2{0.0f, fs * 1.25f} : TextMeasure::measure(m_text, fs, m_font_weight, m_italic);
        float w = pad_x * 2.0f + ts.x + (m_italic ? 2.0f : 0.0f);
        if (has_icon) w += icon_sz + (m_text.empty() ? 0.0f : 6.0f);
        if (m_required) w += 10.0f;
        float h = std::max(ts.y, has_icon ? icon_sz : 0.0f) + pad_y * 2.0f;
        if (m_pill) h = std::max(h, 24.0f);

        set_size(m_constraints.constrain(Vec2{std::ceil(w), std::ceil(h)}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float fs = effective_font_size();
        const float pad_x = m_pill ? 10.0f : 0.0f;
        const bool has_icon = !m_svg_path.empty();
        const float icon_sz = std::clamp(fs + 1.0f, 12.0f, 18.0f);

        Color fg, bg, bd;
        resolve_colors(theme.is_dark, fg, bg, bd);
        if (m_disabled) {
            fg.a *= 0.42f;
            bg.a *= 0.5f;
            bd.a *= 0.5f;
        }

        if (m_pill) {
            float rad = size().y * 0.5f;
            ctx.draw_rect(bounds(), bg, rad, 0.0f);
            ctx.draw_rect(bounds(), bd, rad, 1.0f);
        }

        float cur_x = pad_x;
        if (has_icon) {
            Rect ir = Rect::from_xywh(
                std::round(cur_x),
                std::round((size().y - icon_sz) * 0.5f),
                icon_sz, icon_sz);
            SvgEngine::draw_svg_path(ctx, m_svg_path, 24.0f, 24.0f, ir, fg);
            cur_x += icon_sz + 6.0f;
        }

        if (!m_text.empty()) {
            Vec2 ts = TextMeasure::measure(m_text, fs, m_font_weight, m_italic);
            float tx = std::round(cur_x);
            float ty = std::round((size().y - ts.y) * 0.5f);
            ctx.draw_text(m_text, PointF{tx, ty}, fs, fg, m_font_weight, m_italic);
            if (m_underline) {
                float uy = std::round(ty + ts.y - 1.5f);
                ctx.draw_rect(Rect::from_xywh(tx, uy, ts.x, 1.0f), fg, 0.0f, 0.0f);
            }
            if (m_strikethrough) {
                float sy = std::round(ty + ts.y * 0.52f);
                ctx.draw_rect(Rect::from_xywh(tx, sy, ts.x, 1.0f), fg, 0.0f, 0.0f);
            }
            cur_x += ts.x;
        }

        if (m_required) {
            Color star_col = theme.is_dark
                ? Color{1.0f, 0.60f, 0.64f, m_disabled ? 0.4f : 1.0f}
                : Color{0.77f, 0.17f, 0.11f, m_disabled ? 0.4f : 1.0f};
            Vec2 ss = TextMeasure::measure("*", fs);
            ctx.draw_text("*", PointF{std::round(cur_x + 3.0f), std::round((size().y - ss.y) * 0.5f)}, fs, star_col);
        }
    }

private:
    [[nodiscard]] float effective_font_size() const noexcept {
        if (m_font_size_override > 0.0f) return m_font_size_override;
        switch (m_variant) {
            case LabelVariant::Caption:  return 12.0f;
            case LabelVariant::Subtitle: return 16.0f;
            case LabelVariant::Title:    return 20.0f;
            default:                     return m_pill ? 12.0f : 13.5f;
        }
    }

    void resolve_colors(bool is_dark, Color& fg, Color& bg, Color& bd) const noexcept {
        if (is_dark) {
            switch (m_variant) {
                case LabelVariant::Caption:
                    fg = Color{1.0f, 1.0f, 1.0f, 0.68f};
                    bg = Color{1.0f, 1.0f, 1.0f, 0.06f};
                    bd = Color{1.0f, 1.0f, 1.0f, 0.10f};
                    break;
                case LabelVariant::Accent:
                    fg = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};
                    bg = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 0.14f};
                    bd = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 0.28f};
                    break;
                case LabelVariant::Success:
                    fg = Color{108.0f / 255.0f, 203.0f / 255.0f, 95.0f / 255.0f, 1.0f};
                    bg = Color{108.0f / 255.0f, 203.0f / 255.0f, 95.0f / 255.0f, 0.14f};
                    bd = Color{108.0f / 255.0f, 203.0f / 255.0f, 95.0f / 255.0f, 0.28f};
                    break;
                case LabelVariant::Warning:
                    fg = Color{252.0f / 255.0f, 225.0f / 255.0f, 0.0f, 1.0f};
                    bg = Color{252.0f / 255.0f, 225.0f / 255.0f, 0.0f, 0.13f};
                    bd = Color{252.0f / 255.0f, 225.0f / 255.0f, 0.0f, 0.26f};
                    break;
                case LabelVariant::Error:
                    fg = Color{255.0f / 255.0f, 153.0f / 255.0f, 164.0f / 255.0f, 1.0f};
                    bg = Color{255.0f / 255.0f, 153.0f / 255.0f, 164.0f / 255.0f, 0.14f};
                    bd = Color{255.0f / 255.0f, 153.0f / 255.0f, 164.0f / 255.0f, 0.28f};
                    break;
                default:
                    fg = Color{1.0f, 1.0f, 1.0f, 0.96f};
                    bg = Color{1.0f, 1.0f, 1.0f, 0.07f};
                    bd = Color{1.0f, 1.0f, 1.0f, 0.11f};
                    break;
            }
        } else {
            switch (m_variant) {
                case LabelVariant::Caption:
                    fg = Color{0.0f, 0.0f, 0.0f, 0.62f};
                    bg = Color{0.0f, 0.0f, 0.0f, 0.045f};
                    bd = Color{0.0f, 0.0f, 0.0f, 0.08f};
                    break;
                case LabelVariant::Accent:
                    fg = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
                    bg = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 0.10f};
                    bd = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 0.22f};
                    break;
                case LabelVariant::Success:
                    fg = Color{15.0f / 255.0f, 123.0f / 255.0f, 15.0f / 255.0f, 1.0f};
                    bg = Color{15.0f / 255.0f, 123.0f / 255.0f, 15.0f / 255.0f, 0.10f};
                    bd = Color{15.0f / 255.0f, 123.0f / 255.0f, 15.0f / 255.0f, 0.22f};
                    break;
                case LabelVariant::Warning:
                    fg = Color{157.0f / 255.0f, 93.0f / 255.0f, 0.0f, 1.0f};
                    bg = Color{157.0f / 255.0f, 93.0f / 255.0f, 0.0f, 0.10f};
                    bd = Color{157.0f / 255.0f, 93.0f / 255.0f, 0.0f, 0.22f};
                    break;
                case LabelVariant::Error:
                    fg = Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 1.0f};
                    bg = Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 0.10f};
                    bd = Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 0.22f};
                    break;
                default:
                    fg = Color{0.0f, 0.0f, 0.0f, 0.90f};
                    bg = Color{0.0f, 0.0f, 0.0f, 0.045f};
                    bd = Color{0.0f, 0.0f, 0.0f, 0.09f};
                    break;
            }
        }
    }

    std::string m_text;
    LabelVariant m_variant{LabelVariant::Body};
    bool m_pill{false};
    bool m_required{false};
    std::string m_svg_path;
    float m_font_size_override{0.0f};
    bool m_disabled{false};
    FontWeight m_font_weight{FontWeight::Regular};
    bool m_italic{false};
    bool m_underline{false};
    bool m_strikethrough{false};
};

// C++20 指定初始化器声明式 WinUI 3 Label
struct Label : BindableMixin<Label, RenderLabel> {
    std::string text{""};
    LabelVariant variant{LabelVariant::Body};
    bool pill{false};
    bool required{false};
    std::string svg_path{""};
    float font_size{0.0f};
    bool disabled{false};
    FontWeight font_weight{FontWeight::Regular};
    bool bold{false};
    bool italic{false};
    bool underline{false};
    bool strikethrough{false};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class LabelWidget : public Widget {
public:
    explicit LabelWidget(const Label& desc) : m_desc(desc) {}
    explicit LabelWidget(Label&& desc) noexcept : m_desc(std::move(desc)) {}
    ~LabelWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderLabel>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto lb = std::static_pointer_cast<RenderLabel>(node)) {
            apply(lb);
        }
    }

private:
    void apply(const std::shared_ptr<RenderLabel>& lb) const {
        lb->set_properties(m_desc.text, m_desc.variant, m_desc.pill,
                           m_desc.required, m_desc.svg_path, m_desc.font_size, m_desc.disabled,
                           m_desc.font_weight, m_desc.bold, m_desc.italic,
                           m_desc.underline, m_desc.strikethrough);
    }

    Label m_desc;
};

KTUI_NOINLINE inline Label::operator WidgetPtr() const& {
    return make_widget<LabelWidget>(*this);
}
KTUI_NOINLINE inline Label::operator WidgetPtr() && {
    return make_widget<LabelWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui
