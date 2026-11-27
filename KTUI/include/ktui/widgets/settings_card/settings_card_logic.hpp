#pragma once

#include "settings_card_data.hpp"
#include "settings_card_style.hpp"
#include "../common/fluent_icons.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {


// ==============================================================================
// WinUI 3 / Windows 11 设置卡片 (RenderSettingsCard & SettingsCard)
// 完全对标 CommunityToolkit.WinUI.Controls.SettingsCard:
//   - 左侧可选 20x20 NanoSVG 图标（或任意自定义前置 Widget）
//   - 中部主标题 (14px) + 副标题描述 (12px)
//   - 右侧任意真实 KTUI 控件 (ToggleSwitch / ComboBox / Button / Slider / NumberBox / SegmentedControl 等)
//   - 支持可点击导航卡片模式 (is_click_enabled = true)：悬停高亮、按下反馈、右侧跳转箭头与 on_click 回调
//   - 支持作为 SettingsExpander 子条目无边框内嵌 (is_nested = true)
// ==============================================================================
class RenderSettingsCard : public RenderBox {
public:
    static constexpr float kPadX = 16.0f;
    static constexpr float kPadY = 12.0f;
    static constexpr float kIconSize = 20.0f;
    static constexpr float kIconGap = 16.0f;

    RenderSettingsCard() noexcept {
        setup_behavior();
    }

    ~RenderSettingsCard() override {
        if (m_content_box && m_content_box->parent() == this) {
            remove_child(m_content_box.get());
        }
    }

    void set_properties(std::string header,
                        std::string description,
                        std::string header_icon_svg,
                        std::string action_icon_svg,
                        bool is_click_enabled,
                        bool disabled,
                        bool is_nested,
                        Fn<void()> on_click) {
        m_header = std::move(header);
        m_description = std::move(description);
        m_header_icon_svg = std::move(header_icon_svg);
        m_action_icon_svg = std::move(action_icon_svg);
        m_is_click_enabled = is_click_enabled;
        m_disabled = disabled;
        m_is_nested = is_nested;
        m_on_click = std::move(on_click);
        if (m_disabled) {
            m_reveal_active = false;
        }
        cursor_shape = (m_is_click_enabled && !m_disabled) ? CursorShape::PointingHand : CursorShape::Arrow;
        mark_needs_layout();
    }

    [[nodiscard]] bool pointer_reveal_enabled() const noexcept { return m_enable_pointer_reveal; }
    void set_pointer_reveal_enabled(bool enabled) {
        if (m_enable_pointer_reveal != enabled) {
            m_enable_pointer_reveal = enabled;
            if (!enabled) {
                m_reveal_active = false;
            }
            mark_needs_paint();
        }
    }

    void set_content_box(std::shared_ptr<RenderBox> content) {
        if (m_content_box == content) return;
        if (m_content_box && m_content_box->parent() == this) {
            remove_child(m_content_box.get());
        }
        m_content_box = std::move(content);
        if (m_content_box) {
            append_child(m_content_box.get());
        }
        mark_needs_layout();
    }

    [[nodiscard]] const std::string& header() const noexcept { return m_header; }
    [[nodiscard]] const std::string& description() const noexcept { return m_description; }
    [[nodiscard]] bool is_click_enabled() const noexcept { return m_is_click_enabled; }
    [[nodiscard]] bool is_nested() const noexcept { return m_is_nested; }

    void set_nested(bool nested) {
        if (m_is_nested != nested) {
            m_is_nested = nested;
            mark_needs_layout();
        }
    }

    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 520.0f;
        w = m_constraints.constrain_width(w);

        const float right_arrow_w = m_is_click_enabled ? 24.0f : 0.0f;
        float content_w = 0.0f;
        float content_h = 0.0f;
        if (m_content_box) {
            float max_cw = std::max(40.0f, w * 0.55f);
            m_content_box->layout(BoxConstraints::loose(Vec2{max_cw, 200.0f}), true);
            content_w = m_content_box->size().x;
            content_h = m_content_box->size().y;
        }

        Vec2 hsz = TextMeasure::measure(m_header, 14.0f, FontWeight::Medium);
        Vec2 dsz = m_description.empty() ? Vec2{0.0f, 0.0f} : TextMeasure::measure(m_description, 12.0f);
        float text_h = hsz.y + (m_description.empty() ? 0.0f : (3.0f + dsz.y));
        float min_card_h = m_is_nested ? (m_description.empty() ? 48.0f : 56.0f)
                                       : (m_description.empty() ? 56.0f : 66.0f);
        float total_h = std::max({min_card_h, text_h + kPadY * 2.0f, content_h + kPadY * 2.0f});

        set_size(m_constraints.constrain(Vec2{w, std::round(total_h)}));

        if (m_content_box) {
            float cx = size().x - kPadX - right_arrow_w - content_w;
            float cy = (size().y - content_h) * 0.5f;
            m_content_box->set_offset(Vec2{std::round(cx), std::round(cy)});
        }
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (!bounds().contains(local_position)) return false;
        if (m_content_box) {
            PointF cp = local_position - m_content_box->offset();
            if (m_content_box->hit_test(result, cp)) {
                result.add(this, local_position);
                return true;
            }
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        Color bg, bd, title_col, desc_col, icon_col;
        if (t.is_dark) {
            if (m_is_nested) {
                bg = (m_is_click_enabled && !m_disabled)
                    ? (m_pressed ? Color{1, 1, 1, 0.03f} : (m_hovered ? Color{1, 1, 1, 0.045f} : Color::transparent()))
                    : Color::transparent();
                bd = Color::transparent();
            } else {
                bg = (m_is_click_enabled && !m_disabled)
                    ? (m_pressed ? Color{1, 1, 1, 0.034f} : (m_hovered ? Color{1, 1, 1, 0.072f} : Color{1, 1, 1, 0.048f}))
                    : Color{1, 1, 1, 0.048f};
                bd = Color{1, 1, 1, 0.075f};
            }
            title_col = Color{1, 1, 1, m_disabled ? 0.36f : 0.96f};
            desc_col  = Color{1, 1, 1, m_disabled ? 0.28f : 0.62f};
            icon_col  = Color{1, 1, 1, m_disabled ? 0.32f : 0.88f};
        } else {
            if (m_is_nested) {
                bg = (m_is_click_enabled && !m_disabled)
                    ? (m_pressed ? Color{0, 0, 0, 0.04f} : (m_hovered ? Color{0, 0, 0, 0.028f} : Color::transparent()))
                    : Color::transparent();
                bd = Color::transparent();
            } else {
                bg = (m_is_click_enabled && !m_disabled)
                    ? (m_pressed ? Color{0.96f, 0.96f, 0.96f, 0.72f} : (m_hovered ? Color{252.0f / 255.0f, 252.0f / 255.0f, 252.0f / 255.0f, 0.90f} : Color{1, 1, 1, 0.72f}))
                    : Color{1, 1, 1, 0.72f};
                bd = Color{0, 0, 0, 0.075f};
            }
            title_col = Color{0, 0, 0, m_disabled ? 0.36f : 0.90f};
            desc_col  = Color{0, 0, 0, m_disabled ? 0.28f : 0.56f};
            icon_col  = Color{0, 0, 0, m_disabled ? 0.32f : 0.78f};
        }

        if (bg.a > 0.001f) {
            ctx.draw_rect(bounds(), bg, m_is_nested ? 0.0f : 4.0f, 0.0f);
        }
        if (bd.a > 0.001f) {
            ctx.draw_rect(bounds(), bd, 4.0f, 1.0f);
        }
        if (m_directional_rim && !m_is_nested) {
            const Color top_hi = t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.24f} : Color{1.0f, 1.0f, 1.0f, 0.72f};
            const Color bot_sh = t.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.32f} : Color{0.0f, 0.0f, 0.0f, 0.14f};
            ctx.draw_directional_rim(bounds(), top_hi, bot_sh, 4.0f, 1.0f, Vec2{-0.18f, -0.98f}, 0.25f);
        }

        if (m_enable_pointer_reveal && m_reveal_active && !m_disabled) {
            const Color surface = t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f}
                                            : Color{0.0f, 0.0f, 0.0f, 0.05f};
            const Color edge = t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.28f}
                                         : Color{0.0f, 0.0f, 0.0f, 0.20f};
            ctx.draw_pointer_reveal(bounds(), m_reveal_pos, 80.0f, surface, edge,
                                    m_is_nested ? 0.0f : 4.0f, m_is_nested ? 0.0f : 1.0f);
        }

        // 左侧图标与标题/描述文本
        float tx = m_is_nested ? (kPadX + kIconSize + kIconGap) : kPadX;
        if (!m_header_icon_svg.empty()) {
            float ix = m_is_nested ? (kPadX + 2.0f) : kPadX;
            Rect ir = Rect::from_xywh(ix, std::round((h - kIconSize) * 0.5f), kIconSize, kIconSize);
            SvgEngine::draw_svg_path(ctx, m_header_icon_svg, 24.0f, 24.0f, ir, icon_col);
            if (!m_is_nested) tx += kIconSize + kIconGap;
        }

        Vec2 hsz = TextMeasure::measure(m_header, 14.0f, FontWeight::Medium);
        Vec2 dsz = m_description.empty() ? Vec2{0.0f, 0.0f} : TextMeasure::measure(m_description, 12.0f);
        float block_h = hsz.y + (m_description.empty() ? 0.0f : (3.0f + dsz.y));
        float ty = std::round((h - block_h) * 0.5f);

        float right_limit = m_content_box ? (m_content_box->offset().x - 12.0f) : (w - kPadX - (m_is_click_enabled ? 24.0f : 0.0f));
        float text_clip_w = std::max(20.0f, right_limit - tx);
        ctx.push_clip(Rect::from_xywh(tx, 0.0f, text_clip_w, h), 0.0f);
        ctx.draw_text(m_header, PointF{tx, ty}, 14.0f, title_col, FontWeight::Medium);
        if (!m_description.empty()) {
            ctx.draw_text(m_description, PointF{tx, ty + hsz.y + 3.0f}, 12.0f, desc_col);
        }
        ctx.pop_clip();

        // 右侧真实子控件
        if (m_content_box) {
            PaintContext::OffsetScope scope(ctx, m_content_box->offset());
            m_content_box->paint(ctx);
        }

        // 可点击卡片右侧跳转箭头
        if (m_is_click_enabled) {
            Rect ar = Rect::from_xywh(w - kPadX - 16.0f, 0.0f, 16.0f, h);
            if (!m_action_icon_svg.empty()) {
                Rect ir = Rect::from_xywh(w - kPadX - 16.0f, std::round((h - 14.0f) * 0.5f), 14.0f, 14.0f);
                SvgEngine::draw_svg_path(ctx, m_action_icon_svg, 24.0f, 24.0f, ir, desc_col);
            } else {
                fluent_icons::draw_centered(ctx, fluent_icons::kChevronRight, ar, 11.0f, desc_col);
            }
        }
    }

private:
    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            if (m_is_click_enabled && !m_disabled) {
                m_hovered = true;
                mark_needs_paint();
            }
        };
        on_pointer_move = [this](PointerEvent& e) {
            if (e.type == PlatformEventType::PointerMove &&
                e.device_kind != PointerDeviceKind::Touch &&
                m_enable_pointer_reveal && !m_disabled) {
                if (!m_reveal_active ||
                    std::abs(m_reveal_pos.x - e.local_position.x) > 0.01f ||
                    std::abs(m_reveal_pos.y - e.local_position.y) > 0.01f) {
                    m_reveal_active = true;
                    m_reveal_pos = e.local_position;
                    mark_needs_paint();
                }
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            bool need_paint = m_reveal_active;
            m_reveal_active = false;
            if (m_hovered || m_pressed) {
                m_hovered = false;
                m_pressed = false;
                need_paint = true;
            }
            if (need_paint) {
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (!m_is_click_enabled || m_disabled || e.button != PointerButton::Primary) return;
            m_pressed = true;
            mark_needs_paint();
            e.stop_propagation();
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (!m_pressed) return;
            m_pressed = false;
            mark_needs_paint();
            if (m_is_click_enabled && !m_disabled && bounds().contains(e.local_position) && m_on_click) {
                m_on_click();
            }
            e.stop_propagation();
        };
    }

    std::string m_header;
    std::string m_description;
    std::string m_header_icon_svg;
    std::string m_action_icon_svg;
    bool m_is_click_enabled{false};
    bool m_disabled{false};
    bool m_is_nested{false};
    bool m_hovered{false};
    bool m_pressed{false};
    bool m_enable_pointer_reveal{false};
    bool m_directional_rim{false};
    bool m_reveal_active{false};
    PointF m_reveal_pos{0.0f, 0.0f};
    Fn<void()> m_on_click;
    std::shared_ptr<RenderBox> m_content_box;

public:
    [[nodiscard]] float surface_corner_radius() const noexcept override {
        return m_is_nested ? 0.0f : 4.0f;
    }
    void set_directional_rim(bool enabled) noexcept {
        if (m_directional_rim != enabled) { m_directional_rim = enabled; mark_needs_paint(); }
    }
    [[nodiscard]] bool directional_rim_enabled() const noexcept { return m_directional_rim; }
};

struct SettingsCard : BindableMixin<SettingsCard, RenderSettingsCard> {
    std::string header;
    std::string description;
    std::string header_icon_svg;
    std::string action_icon_svg;
    bool is_click_enabled{false};
    bool disabled{false};
    bool is_nested{false};
    WidgetPtr content{nullptr};
    Fn<void()> on_click{nullptr};
    bool enable_pointer_reveal{false};
    bool directional_rim{false};

    SettingsCard& pointer_reveal(bool enabled) & noexcept { enable_pointer_reveal = enabled; return *this; }
    SettingsCard&& pointer_reveal(bool enabled) && noexcept { enable_pointer_reveal = enabled; return std::move(*this); }

    operator WidgetPtr() const;
};

class SettingsCardWidget : public Widget {
public:
    explicit SettingsCardWidget(SettingsCard desc) : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderSettingsCard>();
        node->set_properties(m_desc.header, m_desc.description, m_desc.header_icon_svg,
                             m_desc.action_icon_svg, m_desc.is_click_enabled, m_desc.disabled,
                             m_desc.is_nested, m_desc.on_click);
        node->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
        node->set_directional_rim(m_desc.directional_rim);
        if (m_desc.content) {
            node->set_content_box(m_desc.content->create_render_node());
        }
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto c = std::static_pointer_cast<RenderSettingsCard>(node)) {
            c->set_properties(m_desc.header, m_desc.description, m_desc.header_icon_svg,
                              m_desc.action_icon_svg, m_desc.is_click_enabled, m_desc.disabled,
                              m_desc.is_nested, m_desc.on_click);
            c->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
            c->set_directional_rim(m_desc.directional_rim);
        }
    }

private:
    SettingsCard m_desc;
};

inline SettingsCard::operator WidgetPtr() const {
    return make_widget<SettingsCardWidget>(*this);
}

// ==============================================================================
// WinUI 3 / Windows 11 可展开设置卡片组 (RenderSettingsExpander & SettingsExpander)
// 完全对标 CommunityToolkit.WinUI.Controls.SettingsExpander:
//   - 头部样式与 SettingsCard 像素级一致（图标 + 标题 + 副标题 + 右侧可选真实控件 + 展开旋转箭头）
//   - 平滑展开/收起动效，内部依次排列子 SettingsCard / 任意子控件，并以 1px 细分割线连接
// ==============================================================================
class RenderSettingsExpander : public RenderBox {
public:
    static constexpr float kPadX = 16.0f;
    static constexpr float kPadY = 12.0f;
    static constexpr float kIconSize = 20.0f;
    static constexpr float kIconGap = 16.0f;

    RenderSettingsExpander() noexcept {
        setup_behavior();
    }

    ~RenderSettingsExpander() override {
        if (m_header_content_box && m_header_content_box->parent() == this) {
            remove_child(m_header_content_box.get());
        }
        for (auto& it : m_item_boxes) {
            if (it && it->parent() == this) remove_child(it.get());
        }
    }

    void set_properties(std::string header,
                        std::string description,
                        std::string header_icon_svg,
                        bool expanded,
                        bool disabled,
                        Fn<void(bool)> on_expanded_changed) {
        m_header = std::move(header);
        m_description = std::move(description);
        m_header_icon_svg = std::move(header_icon_svg);
        m_disabled = disabled;
        if (m_disabled) {
            m_reveal_active = false;
        }
        m_on_expanded_changed = std::move(on_expanded_changed);
        if (!m_initialized) {
            m_initialized = true;
            m_expanded = expanded;
            m_expand_anim = expanded ? 1.0f : 0.0f;
        } else if (m_expanded != expanded) {
            m_expanded = expanded;
            m_anim_time = std::chrono::steady_clock::now();
        }
        mark_needs_layout();
    }

    void set_header_content_box(std::shared_ptr<RenderBox> box) {
        if (m_header_content_box && m_header_content_box->parent() == this) {
            remove_child(m_header_content_box.get());
        }
        m_header_content_box = std::move(box);
        if (m_header_content_box) append_child(m_header_content_box.get());
        mark_needs_layout();
    }

    void set_item_boxes(std::vector<std::shared_ptr<RenderBox>> boxes) {
        for (auto& it : m_item_boxes) {
            if (it && it->parent() == this) remove_child(it.get());
        }
        m_item_boxes = std::move(boxes);
        for (auto& it : m_item_boxes) {
            if (it) append_child(it.get());
        }
        mark_needs_layout();
    }

    [[nodiscard]] bool is_expanded() const noexcept { return m_expanded; }

    [[nodiscard]] bool pointer_reveal_enabled() const noexcept { return m_enable_pointer_reveal; }
    void set_pointer_reveal_enabled(bool enabled) {
        if (m_enable_pointer_reveal != enabled) {
            m_enable_pointer_reveal = enabled;
            if (!enabled) {
                m_reveal_active = false;
            }
            mark_needs_paint();
        }
    }

    void toggle() {
        if (m_disabled) return;
        m_expanded = !m_expanded;
        m_anim_time = std::chrono::steady_clock::now();
        mark_needs_layout();
        if (m_on_expanded_changed) m_on_expanded_changed(m_expanded);
    }

    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 520.0f;
        w = m_constraints.constrain_width(w);

        float hc_w = 0.0f, hc_h = 0.0f;
        if (m_header_content_box) {
            m_header_content_box->layout(BoxConstraints::loose(Vec2{std::max(40.0f, w * 0.45f), 120.0f}), true);
            hc_w = m_header_content_box->size().x;
            hc_h = m_header_content_box->size().y;
        }

        Vec2 hsz = TextMeasure::measure(m_header, 14.0f, FontWeight::Medium);
        Vec2 dsz = m_description.empty() ? Vec2{0.0f, 0.0f} : TextMeasure::measure(m_description, 12.0f);
        float text_h = hsz.y + (m_description.empty() ? 0.0f : (3.0f + dsz.y));
        m_header_h = std::round(std::max({m_description.empty() ? 56.0f : 66.0f, text_h + kPadY * 2.0f, hc_h + kPadY * 2.0f}));

        if (m_header_content_box) {
            float hx = w - kPadX - 32.0f - hc_w;
            float hy = (m_header_h - hc_h) * 0.5f;
            m_header_content_box->set_offset(Vec2{std::round(hx), std::round(hy)});
        }

        float y = m_header_h;
        for (size_t i = 0; i < m_item_boxes.size(); ++i) {
            if (!m_item_boxes[i]) continue;
            y += 1.0f; // 1px 顶部分割线
            BoxConstraints ic{w, w, 0.0f, std::numeric_limits<float>::infinity()};
            m_item_boxes[i]->layout(ic, true);
            m_item_boxes[i]->set_offset(Vec2{0.0f, y});
            y += m_item_boxes[i]->size().y;
        }
        m_full_body_h = std::max(0.0f, y - m_header_h);
        float total_h = m_header_h + std::round(m_full_body_h * m_expand_anim);
        set_size(m_constraints.constrain(Vec2{w, total_h}));
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (!bounds().contains(local_position)) return false;
        if (m_header_content_box && local_position.y < m_header_h) {
            PointF cp = local_position - m_header_content_box->offset();
            if (m_header_content_box->hit_test(result, cp)) {
                return true; // 头部右侧真实子控件独立消费点击，不触发展开/收起
            }
        }
        if (m_expand_anim > 0.5f && local_position.y >= m_header_h) {
            for (auto& it : m_item_boxes) {
                if (!it) continue;
                PointF cp = local_position - it->offset();
                if (it->hit_test(result, cp)) {
                    result.add(this, local_position);
                    return true;
                }
            }
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        update_animation();

        const Theme& t = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;
        const bool show_body = (m_expand_anim > 0.005f && m_full_body_h > 0.5f);

        Color hdr_bg, body_bg, bd, div_col, title_col, desc_col, icon_col;
        if (t.is_dark) {
            hdr_bg = m_pressed ? Color{1, 1, 1, 0.035f}
                   : (m_header_hovered ? Color{1, 1, 1, 0.072f} : Color{1, 1, 1, 0.048f});
            body_bg = Color{1, 1, 1, 0.026f};
            bd = Color{1, 1, 1, 0.075f};
            div_col = Color{1, 1, 1, 0.065f};
            title_col = Color{1, 1, 1, m_disabled ? 0.36f : 0.96f};
            desc_col  = Color{1, 1, 1, m_disabled ? 0.28f : 0.62f};
            icon_col  = Color{1, 1, 1, m_disabled ? 0.32f : 0.88f};
        } else {
            hdr_bg = m_pressed ? Color{0.96f, 0.96f, 0.96f, 0.72f}
                   : (m_header_hovered ? Color{252.0f / 255.0f, 252.0f / 255.0f, 252.0f / 255.0f, 0.90f} : Color{1, 1, 1, 0.72f});
            body_bg = Color{1, 1, 1, 0.44f};
            bd = Color{0, 0, 0, 0.075f};
            div_col = Color{0, 0, 0, 0.065f};
            title_col = Color{0, 0, 0, m_disabled ? 0.36f : 0.90f};
            desc_col  = Color{0, 0, 0, m_disabled ? 0.28f : 0.56f};
            icon_col  = Color{0, 0, 0, m_disabled ? 0.32f : 0.78f};
        }

        // 1. 头部底板 + 展开体底板
        ctx.push_clip(bounds(), 4.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, m_header_h), hdr_bg, 0.0f, 0.0f);
        if (show_body) {
            ctx.draw_rect(Rect::from_xywh(0.0f, m_header_h, w, std::max(0.0f, h - m_header_h)), body_bg, 0.0f, 0.0f);
        }

        if (m_enable_pointer_reveal && m_reveal_active && !m_disabled) {
            const Color surface = t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f}
                                            : Color{0.0f, 0.0f, 0.0f, 0.05f};
            const Color edge = t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.28f}
                                         : Color{0.0f, 0.0f, 0.0f, 0.20f};
            ctx.draw_pointer_reveal(Rect::from_xywh(0.0f, 0.0f, w, m_header_h), m_reveal_pos,
                                    80.0f, surface, edge, 4.0f, 1.0f);
        }

        // 2. 头部图标与标题/副标题
        float tx = kPadX;
        if (!m_header_icon_svg.empty()) {
            Rect ir = Rect::from_xywh(kPadX, std::round((m_header_h - kIconSize) * 0.5f), kIconSize, kIconSize);
            SvgEngine::draw_svg_path(ctx, m_header_icon_svg, 24.0f, 24.0f, ir, icon_col);
            tx += kIconSize + kIconGap;
        }
        Vec2 hsz = TextMeasure::measure(m_header, 14.0f, FontWeight::Medium);
        Vec2 dsz = m_description.empty() ? Vec2{0.0f, 0.0f} : TextMeasure::measure(m_description, 12.0f);
        float block_h = hsz.y + (m_description.empty() ? 0.0f : (3.0f + dsz.y));
        float ty = std::round((m_header_h - block_h) * 0.5f);
        float right_lim = m_header_content_box ? (m_header_content_box->offset().x - 12.0f) : (w - kPadX - 32.0f);
        ctx.push_clip(Rect::from_xywh(tx, 0.0f, std::max(20.0f, right_lim - tx), m_header_h), 0.0f);
        ctx.draw_text(m_header, PointF{tx, ty}, 14.0f, title_col, FontWeight::Medium);
        if (!m_description.empty()) {
            ctx.draw_text(m_description, PointF{tx, ty + hsz.y + 3.0f}, 12.0f, desc_col);
        }
        ctx.pop_clip();

        // 3. 头部右侧可选真实控件
        if (m_header_content_box) {
            PaintContext::OffsetScope scope(ctx, m_header_content_box->offset());
            m_header_content_box->paint(ctx);
        }

        // 4. 右侧展开/收起箭头
        Rect chev_r = Rect::from_xywh(w - kPadX - 20.0f, 0.0f, 20.0f, m_header_h);
        fluent_icons::draw_centered(ctx, m_expanded ? fluent_icons::kChevronUp : fluent_icons::kChevronDown,
                                    chev_r, 11.0f, desc_col);

        // 5. 展开体内的所有子设置项（带 1px 精致分割线）
        if (show_body) {
            ctx.push_clip(Rect::from_xywh(0.0f, m_header_h, w, std::max(0.0f, h - m_header_h)), 0.0f);
            for (auto& it : m_item_boxes) {
                if (!it) continue;
                ctx.draw_rect(Rect::from_xywh(0.0f, it->offset().y - 1.0f, w, 1.0f), div_col, 0.0f, 0.0f);
                PaintContext::OffsetScope scope(ctx, it->offset());
                it->paint(ctx);
            }
            ctx.pop_clip();
        }
        ctx.pop_clip();

        // 6. 统一外框 1px 描边
        ctx.draw_rect(bounds(), bd, 4.0f, 1.0f);
    }

private:
    void update_animation() {
        float target = m_expanded ? 1.0f : 0.0f;
        if (std::abs(m_expand_anim - target) < 0.004f) {
            m_expand_anim = target;
            return;
        }
        auto now = std::chrono::steady_clock::now();
        float dt = std::clamp(std::chrono::duration<float>(now - m_anim_time).count(), 0.0f, 0.05f);
        m_anim_time = now;
        m_expand_anim += (target - m_expand_anim) * std::min(1.0f, dt * 16.0f);
        mark_needs_layout();
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            bool over_hdr = (!m_disabled && e.local_position.y < m_header_h);
            cursor_shape = over_hdr ? CursorShape::PointingHand : CursorShape::Arrow;
            bool need_paint = (over_hdr != m_header_hovered);
            m_header_hovered = over_hdr;
            if (m_enable_pointer_reveal && e.type == PlatformEventType::PointerMove &&
                e.device_kind != PointerDeviceKind::Touch) {
                if (over_hdr) {
                    if (!m_reveal_active ||
                        std::abs(m_reveal_pos.x - e.local_position.x) > 0.01f ||
                        std::abs(m_reveal_pos.y - e.local_position.y) > 0.01f) {
                        m_reveal_active = true;
                        m_reveal_pos = e.local_position;
                        need_paint = true;
                    }
                } else if (m_reveal_active) {
                    m_reveal_active = false;
                    need_paint = true;
                }
            }
            if (need_paint) {
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            bool need_paint = m_reveal_active;
            m_reveal_active = false;
            if (m_header_hovered || m_pressed) {
                m_header_hovered = false;
                m_pressed = false;
                need_paint = true;
            }
            if (need_paint) {
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (!m_disabled && e.button == PointerButton::Primary && e.local_position.y < m_header_h) {
                m_pressed = true;
                mark_needs_paint();
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (!m_pressed) return;
            m_pressed = false;
            if (!m_disabled && e.local_position.y < m_header_h && bounds().contains(e.local_position)) {
                toggle();
            } else {
                mark_needs_paint();
            }
            e.stop_propagation();
        };
    }

    std::string m_header;
    std::string m_description;
    std::string m_header_icon_svg;
    bool m_initialized{false};
    bool m_expanded{false};
    bool m_disabled{false};
    bool m_header_hovered{false};
    bool m_pressed{false};
    bool m_enable_pointer_reveal{false};
    bool m_reveal_active{false};
    PointF m_reveal_pos{0.0f, 0.0f};
    float m_header_h{66.0f};
    float m_full_body_h{0.0f};
    float m_expand_anim{0.0f};
    std::chrono::steady_clock::time_point m_anim_time{std::chrono::steady_clock::now()};
    Fn<void(bool)> m_on_expanded_changed;
    std::shared_ptr<RenderBox> m_header_content_box;
    std::vector<std::shared_ptr<RenderBox>> m_item_boxes;
};

struct SettingsExpander : BindableMixin<SettingsExpander, RenderSettingsExpander> {
    std::string header;
    std::string description;
    std::string header_icon_svg;
    bool expanded{false};
    bool disabled{false};
    WidgetPtr header_content{nullptr};
    std::vector<WidgetPtr> items;
    Fn<void(bool)> on_expanded_changed{nullptr};
    bool enable_pointer_reveal{false};

    SettingsExpander& pointer_reveal(bool enabled) & noexcept { enable_pointer_reveal = enabled; return *this; }
    SettingsExpander&& pointer_reveal(bool enabled) && noexcept { enable_pointer_reveal = enabled; return std::move(*this); }

    operator WidgetPtr() const;
};

class SettingsExpanderWidget : public Widget {
public:
    explicit SettingsExpanderWidget(SettingsExpander desc) : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderSettingsExpander>();
        node->set_properties(m_desc.header, m_desc.description, m_desc.header_icon_svg,
                             m_desc.expanded, m_desc.disabled, m_desc.on_expanded_changed);
        node->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
        if (m_desc.header_content) {
            node->set_header_content_box(m_desc.header_content->create_render_node());
        }
        std::vector<std::shared_ptr<RenderBox>> boxes;
        boxes.reserve(m_desc.items.size());
        for (const auto& w : m_desc.items) {
            if (w) boxes.push_back(w->create_render_node());
        }
        node->set_item_boxes(std::move(boxes));
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto ex = std::static_pointer_cast<RenderSettingsExpander>(node)) {
            ex->set_properties(m_desc.header, m_desc.description, m_desc.header_icon_svg,
                               m_desc.expanded, m_desc.disabled, m_desc.on_expanded_changed);
            ex->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);
        }
    }

private:
    SettingsExpander m_desc;
};

inline SettingsExpander::operator WidgetPtr() const {
    return make_widget<SettingsExpanderWidget>(*this);
}
} // namespace ktui
 // namespace ktui
