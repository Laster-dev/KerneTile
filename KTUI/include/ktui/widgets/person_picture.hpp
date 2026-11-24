#pragma once

#include "widget.hpp"
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string>

namespace ktui {

enum class PersonPresence : uint8_t {
    None = 0,
    Available = 1,
    Busy = 2,
    Away = 3,
    Offline = 4
};

// WinUI 3 PersonPicture 用户头像渲染盒
// 支持中英文姓名首字母自动提取、确定性 Fluent 配色、多尺寸规范与在线状态指示圆点
class RenderPersonPicture : public RenderBox {
public:
    RenderPersonPicture() noexcept = default;
    ~RenderPersonPicture() override = default;

    static std::string extract_initials(const std::string& display_name) {
        if (display_name.empty()) return "?";
        // 跳过前导空格
        size_t start = 0;
        while (start < display_name.size() && display_name[start] == ' ') ++start;
        if (start >= display_name.size()) return "?";

        unsigned char c0 = static_cast<unsigned char>(display_name[start]);
        // 多字节 UTF-8 (如中文姓名)：取第一个完整字符
        if (c0 >= 0x80) {
            size_t len = 1;
            if ((c0 & 0xE0) == 0xC0) len = 2;
            else if ((c0 & 0xF0) == 0xE0) len = 3;
            else if ((c0 & 0xF8) == 0xF0) len = 4;
            return display_name.substr(start, std::min(len, display_name.size() - start));
        }

        // ASCII 英文名：取首个单词与最后一个单词的首字母大写 (如 "Jane Doe" -> "JD")
        std::string out;
        out.push_back(static_cast<char>(std::toupper(c0)));
        size_t last_space = display_name.rfind(' ');
        if (last_space != std::string::npos && last_space + 1 < display_name.size() && last_space > start) {
            unsigned char c1 = static_cast<unsigned char>(display_name[last_space + 1]);
            if (c1 < 0x80 && c1 != ' ') {
                out.push_back(static_cast<char>(std::toupper(c1)));
            }
        }
        return out;
    }

    void set_properties(std::string display_name,
                        std::string initials,
                        float diameter,
                        PersonPresence presence,
                        Color custom_bg) {
        m_display_name = std::move(display_name);
        m_initials = initials.empty() ? extract_initials(m_display_name) : std::move(initials);
        m_diameter = std::clamp(diameter, 24.0f, 128.0f);
        m_presence = presence;
        m_custom_bg = custom_bg;
        mark_needs_layout();
    }

    [[nodiscard]] const std::string& initials() const noexcept { return m_initials; }

    void perform_layout() override {
        set_size(m_constraints.constrain(Vec2{m_diameter, m_diameter}));
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float d = std::min(size().x, size().y);
        const float r = d * 0.5f;

        Color bg = (m_custom_bg.a > 0.001f) ? m_custom_bg : pick_palette_color(m_display_name);
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, d, d), bg, r, 0.0f);

        float font_sz = std::clamp(d * 0.38f, 11.0f, 34.0f);
        Vec2 ts = TextMeasure::measure(m_initials, font_sz);
        ctx.draw_text(m_initials,
                      PointF{std::round((d - ts.x) * 0.5f), std::round((d - ts.y) * 0.5f)},
                      font_sz, Color{1.0f, 1.0f, 1.0f, 0.96f});

        // 右下角在线状态指示点 (带主题底色外环描边)
        if (m_presence != PersonPresence::None) {
            float dot = std::clamp(d * 0.28f, 9.0f, 18.0f);
            float dx = d - dot;
            float dy = d - dot;
            Color ring = theme.is_dark
                ? Color{32.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f, 1.0f}
                : Color{248.0f / 255.0f, 248.0f / 255.0f, 248.0f / 255.0f, 1.0f};
            ctx.draw_rect(Rect::from_xywh(dx - 1.5f, dy - 1.5f, dot + 3.0f, dot + 3.0f),
                          ring, (dot + 3.0f) * 0.5f, 0.0f);
            ctx.draw_rect(Rect::from_xywh(dx, dy, dot, dot), presence_color(theme), dot * 0.5f, 0.0f);
        }
    }

private:
    static Color pick_palette_color(const std::string& seed) noexcept {
        static constexpr Color kPalette[6] = {
            Color{0.00f, 0.47f, 0.84f, 1.0f}, // Fluent Blue
            Color{0.06f, 0.56f, 0.45f, 1.0f}, // Teal
            Color{0.46f, 0.32f, 0.78f, 1.0f}, // Iris Purple
            Color{0.80f, 0.28f, 0.42f, 1.0f}, // Berry Rose
            Color{0.84f, 0.45f, 0.14f, 1.0f}, // Copper Orange
            Color{0.24f, 0.52f, 0.32f, 1.0f}  // Forest Green
        };
        size_t h = 2166136261u;
        for (unsigned char c : seed) { h ^= c; h *= 16777619u; }
        return kPalette[h % 6];
    }

    Color presence_color(const Theme& theme) const noexcept {
        switch (m_presence) {
            case PersonPresence::Available: return theme.colors.success;
            case PersonPresence::Busy:      return theme.colors.error;
            case PersonPresence::Away:      return theme.colors.warning;
            case PersonPresence::Offline:   return Color{0.55f, 0.55f, 0.58f, 1.0f};
            default:                        return Color::transparent();
        }
    }

    std::string m_display_name;
    std::string m_initials{"?"};
    float m_diameter{48.0f};
    PersonPresence m_presence{PersonPresence::None};
    Color m_custom_bg{Color::transparent()};
};

// C++20 指定初始化器声明式 WinUI 3 PersonPicture
struct PersonPicture {
    std::string display_name;
    std::string initials;
    float diameter{48.0f};
    PersonPresence presence{PersonPresence::None};
    Color background_color{Color::transparent()};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

class PersonPictureWidget : public Widget {
public:
    explicit PersonPictureWidget(const PersonPicture& desc) : m_desc(desc) {}
    explicit PersonPictureWidget(PersonPicture&& desc) noexcept : m_desc(std::move(desc)) {}
    ~PersonPictureWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderPersonPicture>();
        node->set_properties(m_desc.display_name, m_desc.initials, m_desc.diameter, m_desc.presence, m_desc.background_color);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderPersonPicture>(node)) {
            n->set_properties(m_desc.display_name, m_desc.initials, m_desc.diameter, m_desc.presence, m_desc.background_color);
        }
    }

private:
    PersonPicture m_desc;
};

inline PersonPicture::operator WidgetPtr() const& {
    return make_widget<PersonPictureWidget>(*this);
}
inline PersonPicture::operator WidgetPtr() && {
    return make_widget<PersonPictureWidget>(std::move(*this));
}

} // namespace ktui
