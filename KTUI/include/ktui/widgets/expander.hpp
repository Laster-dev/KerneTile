#pragma once

#include "common/fluent_icons.hpp"
#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <string>

namespace ktui {

// WinUI 3 Expander / SettingsExpander 渲染盒
// 支持标题栏、副标题、前置 NanoSVG 图标、平滑旋转箭头与流畅高度展开/收起动效
class RenderExpander : public RenderBox {
public:
    RenderExpander() noexcept {
        setup_behavior();
    }

    ~RenderExpander() override = default;

    void set_properties(const std::string& header,
                        const std::string& subtitle,
                        const std::string& svg_path,
                        bool expanded,
                        bool disabled,
                        EdgeInsets content_padding,
                        Fn<void(bool)> on_expanded_changed) {
        bool relayout = (m_header != header || m_subtitle != subtitle || !(m_content_padding == content_padding));
        m_header = header;
        m_subtitle = subtitle;
        m_svg_path = svg_path;
        m_disabled = disabled;
        m_content_padding = content_padding;
        m_on_expanded_changed = std::move(on_expanded_changed);

        if (!m_initialized) {
            m_initialized = true;
            m_expanded = expanded;
            m_expand_anim = expanded ? 1.0f : 0.0f;
            relayout = true;
        } else if (m_expanded != expanded) {
            m_expanded = expanded;
            m_anim_time = std::chrono::steady_clock::now();
            relayout = true;
        }

        if (relayout) mark_needs_layout();
        else mark_needs_paint();
    }

    void set_expanded_value(bool v) {
        if (m_expanded != v) {
            m_expanded = v;
            m_anim_time = std::chrono::steady_clock::now();
            mark_needs_layout();
        }
    }

    void set_content_box(std::shared_ptr<RenderBox> content) {
        if (m_content_box == content) return;
        if (m_content_box) remove_child(m_content_box.get());
        m_content_box = std::move(content);
        if (m_content_box) append_child(m_content_box.get());
        mark_needs_layout();
    }

    [[nodiscard]] const std::shared_ptr<RenderBox>& content_box() const noexcept { return m_content_box; }
    [[nodiscard]] bool is_expanded() const noexcept { return m_expanded; }

    void toggle() {
        if (m_disabled) return;
        m_expanded = !m_expanded;
        m_anim_time = std::chrono::steady_clock::now();
        mark_needs_layout();
        if (m_on_expanded_changed) m_on_expanded_changed(m_expanded);
    }

    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 420.0f;
        w = m_constraints.constrain_width(w);
        m_header_h = m_subtitle.empty() ? 48.0f : 58.0f;

        float content_h = 0.0f;
        if (m_content_box) {
            float inner_w = std::max(0.0f, w - m_content_padding.horizontal());
            BoxConstraints cc;
            cc.min_width = inner_w;
            cc.max_width = inner_w;
            cc.min_height = 0.0f;
            cc.max_height = std::numeric_limits<float>::infinity();
            m_content_box->layout(cc, true);
            m_content_box->set_offset(Vec2{m_content_padding.left, m_header_h + m_content_padding.top});
            content_h = m_content_box->size().y + m_content_padding.vertical();
        }
        m_full_content_h = content_h;

        float visible_content_h = std::round(m_full_content_h * m_expand_anim);
        float total_h = m_header_h + visible_content_h;
        set_size(m_constraints.constrain(Vec2{w, total_h}));
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (local_position.x < 0.0f || local_position.y < 0.0f ||
            local_position.x > size().x || local_position.y > size().y) {
            return false;
        }
        // 仅当展开态且命中内容区域时才向子控件分发命中测试
        if (m_content_box && m_expand_anim > 0.5f && local_position.y >= m_header_h) {
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
        update_animation();

        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;
        const bool show_body = (m_expand_anim > 0.005f && m_full_content_h > 0.5f);

        Color hdr_bg, body_bg, border, divider, title_col, sub_col, chev_col;
        if (theme.is_dark) {
            hdr_bg = m_pressed ? Color{1.0f, 1.0f, 1.0f, 0.035f}
                   : (m_header_hovered ? Color{1.0f, 1.0f, 1.0f, 0.068f} : Color{1.0f, 1.0f, 1.0f, 0.048f});
            body_bg = Color{1.0f, 1.0f, 1.0f, 0.028f};
            border = Color{1.0f, 1.0f, 1.0f, 0.075f};
            divider = Color{1.0f, 1.0f, 1.0f, 0.07f};
            title_col = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f};
            sub_col = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.28f : 0.64f};
            chev_col = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.30f : 0.80f};
        } else {
            hdr_bg = m_pressed ? Color{0.96f, 0.96f, 0.96f, 0.72f}
                   : (m_header_hovered ? Color{252.0f / 255.0f, 252.0f / 255.0f, 252.0f / 255.0f, 0.88f}
                                       : Color{1.0f, 1.0f, 1.0f, 0.72f});
            body_bg = Color{1.0f, 1.0f, 1.0f, 0.45f};
            border = Color{0.0f, 0.0f, 0.0f, 0.075f};
            divider = Color{0.0f, 0.0f, 0.0f, 0.07f};
            title_col = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
            sub_col = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.28f : 0.58f};
            chev_col = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.30f : 0.72f};
        }

        // 1. 顶部标题栏背景 + 展开区背景 + 外边框
        if (show_body) {
            ctx.draw_rounded_rect(Rect::from_xywh(0.0f, 0.0f, w, m_header_h), hdr_bg, 6.0f, 6.0f, 0.0f, 0.0f);
            ctx.draw_rounded_rect(Rect::from_xywh(0.0f, m_header_h, w, std::max(0.0f, h - m_header_h)),
                                  body_bg, 0.0f, 0.0f, 6.0f, 6.0f);
            ctx.draw_rect(Rect::from_xywh(1.0f, m_header_h, std::max(0.0f, w - 2.0f), 1.0f), divider, 0.0f, 0.0f);
            ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, h), border, 6.0f, 1.0f);
        } else {
            ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, m_header_h), hdr_bg, 6.0f, 0.0f);
            ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, m_header_h), border, 6.0f, 1.0f);
        }

        // 2. 标题栏图标、主标题、副标题
        float cur_x = 16.0f;
        if (!m_svg_path.empty()) {
            float isz = 18.0f;
            Rect ir = Rect::from_xywh(cur_x, std::round((m_header_h - isz) * 0.5f), isz, isz);
            SvgEngine::draw_svg_path(ctx, m_svg_path, 24.0f, 24.0f, ir, title_col);
            cur_x += isz + 12.0f;
        }
        float text_w = std::max(20.0f, w - cur_x - 48.0f);
        ctx.push_clip(Rect::from_xywh(cur_x, 0.0f, text_w, m_header_h), 0.0f);
        if (m_subtitle.empty()) {
            Vec2 ts = TextMeasure::measure(m_header, 13.5f);
            ctx.draw_text(m_header, PointF{cur_x, std::round((m_header_h - ts.y) * 0.5f)}, 13.5f, title_col);
        } else {
            ctx.draw_text(m_header, PointF{cur_x, 11.0f}, 13.5f, title_col);
            ctx.draw_text(m_subtitle, PointF{cur_x, 31.0f}, 12.0f, sub_col);
        }
        ctx.pop_clip();

        // 3. 右侧旋转箭头按钮 (0 -> π rad 顺滑旋转)
        Rect chev_btn = Rect::from_xywh(w - 38.0f, std::round((m_header_h - 28.0f) * 0.5f), 28.0f, 28.0f);
        if (m_header_hovered && !m_disabled) {
            Color cbg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.06f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
            ctx.draw_rect(chev_btn, cbg, 4.0f, 0.0f);
        }
        float csz = 13.0f;
        Rect cr = Rect::from_xywh(std::round(chev_btn.min_x + (chev_btn.width() - csz) * 0.5f),
                                  std::round(chev_btn.min_y + (chev_btn.height() - csz) * 0.5f), csz, csz);
        SvgEngine::draw_svg_xml(ctx, fluent_icons::kChevronDown, cr, chev_col, m_expand_anim * 3.1415926f);

        // 4. 展开内容区 (硬件裁剪过渡高度)
        if (show_body && m_content_box) {
            Rect body_clip = Rect::from_xywh(1.0f, m_header_h + 1.0f,
                                             std::max(0.0f, w - 2.0f),
                                             std::max(0.0f, h - m_header_h - 2.0f));
            ctx.push_clip(body_clip, 0.0f);
            {
                PaintContext::OffsetScope scope(ctx, m_content_box->offset());
                m_content_box->paint(ctx);
            }
            ctx.pop_clip();
        }
    }

private:
    void update_animation() {
        float target = m_expanded ? 1.0f : 0.0f;
        if (std::abs(m_expand_anim - target) <= 0.004f) {
            if (m_expand_anim != target) {
                m_expand_anim = target;
                mark_needs_layout();
            }
            return;
        }
        auto now = std::chrono::steady_clock::now();
        if (m_anim_time.time_since_epoch().count() == 0) m_anim_time = now;
        float dt = std::clamp(std::chrono::duration<float>(now - m_anim_time).count(), 0.0f, 0.05f);
        m_anim_time = now;
        m_expand_anim += (target - m_expand_anim) * std::min(1.0f, dt * 18.0f);
        if (std::abs(m_expand_anim - target) <= 0.005f) m_expand_anim = target;
        mark_needs_layout();
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            if (m_disabled) return;
            bool hh = (e.local_position.y >= 0.0f && e.local_position.y <= m_header_h);
            if (hh != m_header_hovered) {
                m_header_hovered = hh;
                cursor_shape = hh ? CursorShape::PointingHand : CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_header_hovered || m_pressed) {
                m_header_hovered = false;
                m_pressed = false;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            if (e.local_position.y >= 0.0f && e.local_position.y <= m_header_h) {
                m_pressed = true;
                mark_needs_paint();
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (m_pressed) {
                m_pressed = false;
                if (e.local_position.y >= 0.0f && e.local_position.y <= m_header_h) {
                    toggle();
                } else {
                    mark_needs_paint();
                }
                e.stop_propagation();
            }
        };
    }

    bool m_initialized{false};
    std::string m_header;
    std::string m_subtitle;
    std::string m_svg_path;
    bool m_expanded{false};
    bool m_disabled{false};
    EdgeInsets m_content_padding{14.0f, 16.0f};
    Fn<void(bool)> m_on_expanded_changed;

    std::shared_ptr<RenderBox> m_content_box;
    float m_header_h{48.0f};
    float m_full_content_h{0.0f};
    float m_expand_anim{0.0f};
    bool m_header_hovered{false};
    bool m_pressed{false};
    std::chrono::steady_clock::time_point m_anim_time{};

public:
    void wrap_on_expanded_changed(Fn<void(bool)> extra) {
        auto prev = std::move(m_on_expanded_changed);
        m_on_expanded_changed = [prev = std::move(prev), extra = std::move(extra)](bool v) {
            if (extra) extra(v);
            if (prev) prev(v);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 Expander
struct Expander : BindableMixin<Expander, RenderExpander> {
    std::string header{""};
    std::string subtitle{""};
    std::string svg_path{""};
    bool is_expanded{false};
    bool disabled{false};
    EdgeInsets content_padding{14.0f, 16.0f};
    WidgetPtr content{nullptr};
    Fn<void(bool)> on_expanded_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderExpander>& node, Property<bool>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_expanded_value(prop.get());
    std::weak_ptr<RenderExpander> weak = node;
    bg.subscribe(prop, [weak, guard](bool v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_expanded_value(v);
        }
    });
    Property<bool>* p = &prop;
    node->wrap_on_expanded_changed([p, guard](bool v) {
        *guard = true;
        p->set(v);
        *guard = false;
    });
}

class ExpanderWidget : public Widget {
public:
    explicit ExpanderWidget(const Expander& desc) : m_desc(desc) {}
    explicit ExpanderWidget(Expander&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ExpanderWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderExpander>();
        if (m_desc.content) {
            m_content_box = m_desc.content->create_render_node();
            node->set_content_box(m_content_box);
        }
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto ex = std::static_pointer_cast<RenderExpander>(node)) {
            if (m_desc.content) {
                if (m_content_box) m_desc.content->update_render_node(m_content_box);
                else {
                    m_content_box = m_desc.content->create_render_node();
                    ex->set_content_box(m_content_box);
                }
            }
            apply(ex);
        }
    }

private:
    void apply(const std::shared_ptr<RenderExpander>& ex) const {
        ex->set_properties(
            m_desc.header, m_desc.subtitle, m_desc.svg_path,
            m_desc.is_expanded, m_desc.disabled, m_desc.content_padding,
            m_desc.on_expanded_changed);
    }

    Expander m_desc;
    std::shared_ptr<RenderBox> m_content_box;
};

inline Expander::operator WidgetPtr() const& {
    return make_widget<ExpanderWidget>(*this);
}
inline Expander::operator WidgetPtr() && {
    return make_widget<ExpanderWidget>(std::move(*this));
}

} // namespace ktui
