#pragma once

#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace ktui {

// WinUI 3 InfoBar 严重级别
enum class InfoBarSeverity : uint8_t {
    Informational = 0,
    Success       = 1,
    Warning       = 2,
    Error         = 3
};

struct InfoBarData {
    std::string title;
    std::string message;
    InfoBarSeverity severity{InfoBarSeverity::Informational};
    bool is_open{true};
    bool is_closable{true};
    std::string action_text;
    std::function<void()> on_action{nullptr};
    std::function<void()> on_close{nullptr};
};

// WinUI 3 内联状态提示条渲染盒 (RenderInfoBar)
class RenderInfoBar : public RenderBox {
public:
    RenderInfoBar() noexcept {
        setup_behavior();
    }

    ~RenderInfoBar() override = default;

    [[nodiscard]] bool is_open() const noexcept { return m_data.is_open; }
    [[nodiscard]] InfoBarSeverity severity() const noexcept { return m_data.severity; }

    void set_data(const InfoBarData& data) {
        bool layout_changed = (m_data.is_open != data.is_open) ||
                              (m_data.title != data.title) ||
                              (m_data.message != data.message) ||
                              (m_data.action_text != data.action_text) ||
                              (m_data.is_closable != data.is_closable);
        m_data = data;
        if (layout_changed) mark_needs_layout();
        else mark_needs_paint();
    }

    void set_open(bool open) {
        if (m_data.is_open != open) {
            m_data.is_open = open;
            mark_needs_layout();
            mark_needs_paint();
        }
    }

    void set_severity(InfoBarSeverity sev) {
        if (m_data.severity != sev) {
            m_data.severity = sev;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        if (!m_data.is_open) {
            set_size(Vec2{0.0f, 0.0f});
            return;
        }
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 420.0f;
        float h = m_constraints.constrain_height(48.0f);
        set_size(Vec2{w, h});
    }

    void paint(PaintContext& ctx) override {
        if (!m_data.is_open || size().x <= 1.0f || size().y <= 1.0f) return;

        const Theme& theme = ThemeContext::current();
        Color bg, border, icon_col, text_col, msg_col;
        resolve_colors(theme, bg, border, icon_col, text_col, msg_col);

        Rect r = bounds();
        ctx.draw_rect(r, bg, 4.0f, 0.0f);
        ctx.draw_rect(r, border, 4.0f, 1.0f);

        // 1. 左侧 NanoSVG Fluent 语义图标 (18x18)
        const float icon_y = std::round((r.height() - 18.0f) * 0.5f);
        Rect icon_r = Rect::from_xywh(14.0f, icon_y, 18.0f, 18.0f);
        SvgEngine::draw_svg_path(ctx, severity_svg_path(m_data.severity), 24.0f, 24.0f, icon_r, icon_col, 0.0f, true);

        // 2. 右侧关闭按钮 (×) 与可选操作按钮布局
        float right_x = r.max_x - 10.0f;
        m_close_rect = Rect{};
        if (m_data.is_closable) {
            m_close_rect = Rect::from_xywh(right_x - 28.0f, std::round((r.height() - 28.0f) * 0.5f), 28.0f, 28.0f);
            if (m_close_hovered) {
                Color hov = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.05f};
                ctx.draw_rect(m_close_rect, hov, 4.0f, 0.0f);
            }
            float cx = std::round(m_close_rect.min_x + 14.0f);
            float cy = std::round(m_close_rect.min_y + 14.0f);
            Color xc = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.78f} : Color{0.0f, 0.0f, 0.0f, 0.68f};
            ctx.draw_line(PointF{cx - 4.0f, cy - 4.0f}, PointF{cx + 4.0f, cy + 4.0f}, xc, 1.25f);
            ctx.draw_line(PointF{cx + 4.0f, cy - 4.0f}, PointF{cx - 4.0f, cy + 4.0f}, xc, 1.25f);
            right_x -= 34.0f;
        }

        m_action_rect = Rect{};
        if (!m_data.action_text.empty()) {
            Vec2 asz = TextMeasure::measure(m_data.action_text, 12.5f);
            float aw = std::ceil(asz.x + 22.0f);
            float ah = 28.0f;
            m_action_rect = Rect::from_xywh(right_x - aw, std::round((r.height() - ah) * 0.5f), aw, ah);
            Color abg = theme.is_dark
                ? (m_action_hovered ? Color{1.0f, 1.0f, 1.0f, 0.11f} : Color{1.0f, 1.0f, 1.0f, 0.06f})
                : (m_action_hovered ? Color{0.0f, 0.0f, 0.0f, 0.06f} : Color{1.0f, 1.0f, 1.0f, 0.85f});
            Color abd = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.12f} : Color{0.0f, 0.0f, 0.0f, 0.12f};
            ctx.draw_rect(m_action_rect, abg, 4.0f, 0.0f);
            ctx.draw_rect(m_action_rect, abd, 4.0f, 1.0f);
            ctx.draw_text(m_data.action_text,
                          PointF{std::round(m_action_rect.min_x + (aw - asz.x) * 0.5f),
                                 std::round(m_action_rect.min_y + (ah - asz.y) * 0.5f)},
                          12.5f, text_col);
            right_x -= (aw + 8.0f);
        }

        // 3. 标题 + 正文内容 (防溢出裁剪)
        float text_x = 42.0f;
        float text_w = std::max(0.0f, right_x - text_x);
        if (text_w > 4.0f) {
            ctx.push_clip(Rect::from_xywh(text_x, 0.0f, text_w, r.height()), 0.0f);
            float cur_x = text_x;
            if (!m_data.title.empty()) {
                Vec2 tsz = TextMeasure::measure(m_data.title, 13.5f);
                ctx.draw_text(m_data.title, PointF{std::round(cur_x), std::round((r.height() - tsz.y) * 0.5f)}, 13.5f, text_col);
                cur_x += tsz.x + 10.0f;
            }
            if (!m_data.message.empty()) {
                Vec2 msz = TextMeasure::measure(m_data.message, 13.0f);
                ctx.draw_text(m_data.message, PointF{std::round(cur_x), std::round((r.height() - msz.y) * 0.5f)}, 13.0f, msg_col);
            }
            ctx.pop_clip();
        }
    }

private:
    static const char* severity_svg_path(InfoBarSeverity sev) noexcept {
        switch (sev) {
            case InfoBarSeverity::Success:
                return "M12 2c5.523 0 10 4.477 10 10s-4.477 10-10 10S2 17.523 2 12 6.477 2 12 2Zm3.22 6.97-4.47 4.47-1.97-1.97a.75.75 0 0 0-1.06 1.06l2.5 2.5a.75.75 0 0 0 1.06 0l5-5a.75.75 0 1 0-1.06-1.06Z";
            case InfoBarSeverity::Warning:
                return "M10.909 2.782a2.25 2.25 0 0 1 3.918 0l7.145 12.662A2.25 2.25 0 0 1 20.013 19H5.723a2.25 2.25 0 0 1-1.959-3.556l7.145-12.662ZM12.868 8a.75.75 0 0 0-.75.75v3.75a.75.75 0 0 0 1.5 0V8.75a.75.75 0 0 0-.75-.75Zm0 6.5a1 1 0 1 0 0 2 1 1 0 0 0 0-2Z";
            case InfoBarSeverity::Error:
                return "M12 2c5.523 0 10 4.477 10 10s-4.477 10-10 10S2 17.523 2 12 6.477 2 12 2Zm2.47 6.47L12 10.94 9.53 8.47a.75.75 0 0 0-1.06 1.06L10.94 12l-2.47 2.47a.75.75 0 1 0 1.06 1.06L12 13.06l2.47 2.47a.75.75 0 1 0 1.06-1.06L13.06 12l2.47-2.47a.75.75 0 0 0-1.06-1.06Z";
            case InfoBarSeverity::Informational:
            default:
                return "M12 2c5.523 0 10 4.477 10 10s-4.477 10-10 10S2 17.523 2 12 6.477 2 12 2Zm0 8.25a.75.75 0 0 0-.75.75v5.5a.75.75 0 0 0 1.5 0V11a.75.75 0 0 0-.75-.75Zm0-3.25a1 1 0 1 0 0 2 1 1 0 0 0 0-2Z";
        }
    }

    void resolve_colors(const Theme& theme, Color& bg, Color& border,
                        Color& icon_col, Color& text_col, Color& msg_col) const noexcept {
        if (theme.is_dark) {
            text_col = Color{1.0f, 1.0f, 1.0f, 0.96f};
            msg_col  = Color{1.0f, 1.0f, 1.0f, 0.78f};
            border   = Color{1.0f, 1.0f, 1.0f, 0.08f};
            switch (m_data.severity) {
                case InfoBarSeverity::Success:
                    bg       = Color{57.0f / 255.0f, 61.0f / 255.0f, 27.0f / 255.0f, 0.95f}; // #393D1B
                    icon_col = Color{108.0f / 255.0f, 203.0f / 255.0f, 95.0f / 255.0f, 1.0f}; // #6CCB5F
                    break;
                case InfoBarSeverity::Warning:
                    bg       = Color{67.0f / 255.0f, 53.0f / 255.0f, 25.0f / 255.0f, 0.95f}; // #433519
                    icon_col = Color{252.0f / 255.0f, 225.0f / 255.0f, 0.0f, 1.0f};           // #FCE100
                    break;
                case InfoBarSeverity::Error:
                    bg       = Color{68.0f / 255.0f, 39.0f / 255.0f, 38.0f / 255.0f, 0.95f}; // #442726
                    icon_col = Color{255.0f / 255.0f, 153.0f / 255.0f, 164.0f / 255.0f, 1.0f};// #FF99A4
                    break;
                case InfoBarSeverity::Informational:
                default:
                    bg       = Color{1.0f, 1.0f, 1.0f, 0.055f};
                    icon_col = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}; // #60CDFF
                    break;
            }
        } else {
            text_col = Color{0.0f, 0.0f, 0.0f, 0.90f};
            msg_col  = Color{0.0f, 0.0f, 0.0f, 0.72f};
            border   = Color{0.0f, 0.0f, 0.0f, 0.08f};
            switch (m_data.severity) {
                case InfoBarSeverity::Success:
                    bg       = Color{223.0f / 255.0f, 246.0f / 255.0f, 221.0f / 255.0f, 1.0f}; // #DFF6DD
                    icon_col = Color{15.0f / 255.0f, 123.0f / 255.0f, 15.0f / 255.0f, 1.0f};   // #0F7B0F
                    break;
                case InfoBarSeverity::Warning:
                    bg       = Color{255.0f / 255.0f, 244.0f / 255.0f, 206.0f / 255.0f, 1.0f}; // #FFF4CE
                    icon_col = Color{157.0f / 255.0f, 93.0f / 255.0f, 0.0f, 1.0f};             // #9D5D00
                    break;
                case InfoBarSeverity::Error:
                    bg       = Color{253.0f / 255.0f, 231.0f / 255.0f, 233.0f / 255.0f, 1.0f}; // #FDE7E9
                    icon_col = Color{196.0f / 255.0f, 43.0f / 255.0f, 28.0f / 255.0f, 1.0f};   // #C42B1C
                    break;
                case InfoBarSeverity::Informational:
                default:
                    bg       = Color{244.0f / 255.0f, 244.0f / 255.0f, 244.0f / 255.0f, 0.95f};
                    icon_col = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};             // #005FB8
                    break;
            }
        }
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            bool ch = m_data.is_closable && m_close_rect.contains(e.local_position);
            bool ah = !m_data.action_text.empty() && m_action_rect.contains(e.local_position);
            if (ch != m_close_hovered || ah != m_action_hovered) {
                m_close_hovered = ch;
                m_action_hovered = ah;
                cursor_shape = (ch || ah) ? CursorShape::PointingHand : CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_close_hovered || m_action_hovered) {
                m_close_hovered = false;
                m_action_hovered = false;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            if (m_data.is_closable && m_close_rect.contains(e.local_position)) {
                set_open(false);
                if (m_data.on_close) m_data.on_close();
                e.stop_propagation();
            } else if (!m_data.action_text.empty() && m_action_rect.contains(e.local_position)) {
                if (m_data.on_action) m_data.on_action();
                e.stop_propagation();
            }
        };
    }

    InfoBarData m_data;
    Rect m_close_rect;
    Rect m_action_rect;
    bool m_close_hovered{false};
    bool m_action_hovered{false};
};

// C++20 指定初始化器声明式 InfoBar 控件
struct InfoBar {
    Bindable<std::string> title{""};
    Bindable<std::string> message{""};
    Bindable<InfoBarSeverity> severity{InfoBarSeverity::Informational};
    Bindable<bool> is_open{true};
    bool is_closable{true};
    std::string action_text{""};
    std::function<void()> on_action{nullptr};
    std::function<void()> on_close{nullptr};

    operator WidgetPtr() const;
};

class InfoBarWidget : public Widget {
public:
    explicit InfoBarWidget(const InfoBar& desc) : m_desc(desc) {}
    ~InfoBarWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderInfoBar>();
        apply(node);
        bind(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto ib = std::dynamic_pointer_cast<RenderInfoBar>(node)) {
            apply(ib);
            bind(ib);
        }
    }

private:
    void apply(const std::shared_ptr<RenderInfoBar>& node) {
        InfoBarData d;
        d.title = m_desc.title.get();
        d.message = m_desc.message.get();
        d.severity = m_desc.severity.get();
        d.is_open = m_desc.is_open.get();
        d.is_closable = m_desc.is_closable;
        d.action_text = m_desc.action_text;
        d.on_action = m_desc.on_action;
        d.on_close = [this]() {
            m_is_internal_update = true;
            m_desc.is_open.set_bound(false);
            if (m_desc.on_close) m_desc.on_close();
            m_is_internal_update = false;
        };
        node->set_data(d);
    }

    void bind(const std::shared_ptr<RenderInfoBar>& node) {
        m_bindings.clear();
        m_bindings.attach(m_desc.is_open, node, [this](RenderInfoBar& n, bool open) {
            if (!m_is_internal_update) n.set_open(open);
        });
        m_bindings.attach(m_desc.severity, node, [](RenderInfoBar& n, InfoBarSeverity s) {
            n.set_severity(s);
        });
    }

    InfoBar m_desc;
    BindingGroup m_bindings;
    bool m_is_internal_update{false};
};

inline InfoBar::operator WidgetPtr() const {
    return std::make_shared<InfoBarWidget>(*this);
}

} // namespace ktui
