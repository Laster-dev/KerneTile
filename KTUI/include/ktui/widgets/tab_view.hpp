#pragma once

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
#include <memory>
#include <string>
#include <vector>

namespace ktui {

struct TabViewItem {
    std::string id;
    std::string header;
    std::string svg_path;
    bool is_closable{true};

    bool operator==(const TabViewItem&) const = default;
};

// WinUI 3 TabView 多标签页渲染盒 (对标 Windows Terminal / Edge 标签栏)
// 支持选中标签无缝融合内容区、独立关闭按钮 (×)、新建标签 (+) 与动态内容构建
class RenderTabView : public RenderBox {
public:
    RenderTabView() noexcept {
        setup_behavior();
    }

    ~RenderTabView() override = default;

    void set_properties(std::vector<TabViewItem> tabs,
                        int selected_index,
                        bool show_add_button,
                        float height,
                        Fn<WidgetPtr(const std::string&)> content_builder,
                        Fn<void(int)> on_selection_changed,
                        Fn<void(int)> on_tab_close,
                        Fn<void()> on_add_tab) {
        bool tabs_changed = (m_tabs != tabs);
        m_tabs = std::move(tabs);
        m_show_add_button = show_add_button;
        m_height = height;
        m_content_builder = std::move(content_builder);
        m_on_selection_changed = std::move(on_selection_changed);
        m_on_tab_close = std::move(on_tab_close);
        m_on_add_tab = std::move(on_add_tab);

        int clamped = m_tabs.empty() ? -1 : std::clamp(selected_index, 0, static_cast<int>(m_tabs.size()) - 1);
        bool sel_changed = (clamped != m_selected_index);
        m_selected_index = clamped;

        if (tabs_changed || sel_changed || !m_active_content_box) {
            rebuild_active_content();
        }
        mark_needs_layout();
    }

    void set_tabs_value(const std::vector<TabViewItem>& tabs) {
        if (m_tabs == tabs) return;
        m_tabs = tabs;
        if (m_tabs.empty()) m_selected_index = -1;
        else m_selected_index = std::clamp(m_selected_index, 0, static_cast<int>(m_tabs.size()) - 1);
        m_hovered_tab = -1;
        m_hovered_close = -1;
        rebuild_active_content();
        mark_needs_layout();
    }

    void set_selected_index_value(int idx) {
        int clamped = m_tabs.empty() ? -1 : std::clamp(idx, 0, static_cast<int>(m_tabs.size()) - 1);
        if (m_selected_index != clamped) {
            trigger_tab_switch_anim(m_selected_index, clamped);
            m_selected_index = clamped;
            rebuild_active_content();
            mark_needs_layout();
        }
    }

    [[nodiscard]] int selected_index() const noexcept { return m_selected_index; }
    [[nodiscard]] size_t tab_count() const noexcept { return m_tabs.size(); }

    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 640.0f;
        w = m_constraints.constrain_width(w);
        float h = (m_height > 0.0f) ? m_height
                                    : (std::isfinite(m_constraints.max_height) ? m_constraints.max_height : 260.0f);
        h = m_constraints.constrain_height(h);
        set_size(Vec2{w, h});

        compute_tab_rects(w);

        if (m_active_content_box) {
            float body_w = std::max(0.0f, w - 2.0f);
            float body_h = std::max(0.0f, h - kStripH - 2.0f);
            m_active_content_box->layout(BoxConstraints::tight(Vec2{body_w, body_h}), true);
            m_active_content_box->set_offset(Vec2{1.0f, kStripH + 1.0f});
        }
    }

    bool hit_test(HitTestResult& result, PointF local_position) override {
        if (local_position.x < 0.0f || local_position.y < 0.0f ||
            local_position.x > size().x || local_position.y > size().y) {
            return false;
        }
        if (local_position.y >= kStripH && m_active_content_box) {
            PointF cp = local_position - m_active_content_box->offset();
            if (m_active_content_box->hit_test(result, cp)) {
                result.add(this, local_position);
                return true;
            }
        }
        result.add(this, local_position);
        return true;
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        // 推进标签滑块与内容方向滑入动效
        auto now = std::chrono::steady_clock::now();
        float dt = m_has_anim_time
            ? std::clamp(std::chrono::duration<float>(now - m_last_anim_time).count(), 0.001f, 0.05f)
            : (1.0f / 60.0f);
        m_last_anim_time = now;
        m_has_anim_time = true;

        bool animating = false;
        if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_tab_rects.size())) {
            const Rect& target = m_tab_rects[static_cast<size_t>(m_selected_index)];
            if (!m_ind_init) {
                m_ind_x = target.min_x;
                m_ind_w = target.width();
                m_ind_init = true;
            } else if (std::abs(m_ind_x - target.min_x) > 0.2f || std::abs(m_ind_w - target.width()) > 0.2f) {
                float k = 1.0f - std::exp(-dt * 24.0f);
                m_ind_x += (target.min_x - m_ind_x) * k;
                m_ind_w += (target.width() - m_ind_w) * k;
                animating = true;
            } else {
                m_ind_x = target.min_x;
                m_ind_w = target.width();
            }
        }
        if (std::abs(m_content_slide_x) > 0.2f) {
            m_content_slide_x *= std::exp(-dt * 20.0f);
            animating = true;
        } else {
            m_content_slide_x = 0.0f;
        }
        if (animating) mark_needs_paint();
        else m_has_anim_time = false;

        Color strip_bg = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.022f}
            : Color{0.0f, 0.0f, 0.0f, 0.032f};
        Color active_bg = theme.is_dark
            ? Color{42.0f / 255.0f, 43.0f / 255.0f, 48.0f / 255.0f, 1.0f}
            : Color{1.0f, 1.0f, 1.0f, 0.92f};
        Color hov_bg = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.06f}
            : Color{0.0f, 0.0f, 0.0f, 0.045f};
        Color border = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.085f}
            : Color{0.0f, 0.0f, 0.0f, 0.09f};
        Color sep_col = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.10f}
            : Color{0.0f, 0.0f, 0.0f, 0.10f};
        Color text_active = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.96f} : Color{0.0f, 0.0f, 0.0f, 0.92f};
        Color text_idle   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.66f} : Color{0.0f, 0.0f, 0.0f, 0.60f};

        // 1. 顶部标签栏条背景 (顶圆角 8px)
        ctx.draw_rounded_rect(Rect::from_xywh(0.0f, 0.0f, w, kStripH + 2.0f), strip_bg, 8.0f, 8.0f, 0.0f, 0.0f);

        // 2. 内容区底板与边框 (底圆角 8px)
        Rect body_rect = Rect::from_xywh(0.0f, kStripH, w, std::max(0.0f, h - kStripH));
        ctx.draw_rounded_rect(body_rect, active_bg, 0.0f, 0.0f, 8.0f, 8.0f);
        ctx.draw_rounded_rect(body_rect, border, 0.0f, 0.0f, 8.0f, 8.0f, 0.0f, Vec2{0.0f, 0.0f}, 1.0f);

        // 3. 平滑滑动的激活标签融底壳体 + 顶部主题色微指示条
        if (m_ind_init && m_selected_index >= 0 && m_selected_index < static_cast<int>(m_tab_rects.size())) {
            Rect glide_r = Rect::from_xywh(m_ind_x, 4.0f, m_ind_w, kStripH - 4.0f);
            Rect sel_fill = Rect::from_xywh(glide_r.min_x, glide_r.min_y, glide_r.width(), glide_r.height() + 2.0f);
            ctx.draw_rounded_rect(sel_fill, active_bg, 6.0f, 6.0f, 0.0f, 0.0f);
            ctx.push_clip(glide_r, 0.0f);
            ctx.draw_rounded_rect(Rect::from_xywh(glide_r.min_x, glide_r.min_y, glide_r.width(), glide_r.height() + 4.0f),
                                  border, 6.0f, 6.0f, 0.0f, 0.0f, 0.0f, Vec2{0.0f, 0.0f}, 1.0f);
            ctx.pop_clip();
            float ind_bar_w = std::max(16.0f, glide_r.width() - 24.0f);
            ctx.draw_rect(Rect::from_xywh(glide_r.min_x + (glide_r.width() - ind_bar_w) * 0.5f,
                                          glide_r.min_y + 1.0f, ind_bar_w, 2.5f),
                          theme.colors.primary, 1.25f, 0.0f);
        }

        // 4. 绘制所有标签项
        for (size_t i = 0; i < m_tabs.size() && i < m_tab_rects.size(); ++i) {
            const auto& tab = m_tabs[i];
            const Rect& tr = m_tab_rects[i];
            bool is_sel = (static_cast<int>(i) == m_selected_index);
            bool is_hov = (static_cast<int>(i) == m_hovered_tab);

            // 非激活标签间的 1px 竖向分隔线 (靠近选中或悬停标签时隐藏)
            if (i > 0 && !is_sel && !is_hov &&
                static_cast<int>(i - 1) != m_selected_index &&
                static_cast<int>(i - 1) != m_hovered_tab) {
                ctx.draw_rect(Rect::from_xywh(std::round(tr.min_x), 11.0f, 1.0f, 16.0f), sep_col, 0.0f, 0.0f);
            }

            if (!is_sel && is_hov) {
                Rect hov_r = Rect::from_xywh(tr.min_x + 2.0f, tr.min_y + 2.0f, tr.width() - 4.0f, tr.height() - 4.0f);
                ctx.draw_rect(hov_r, hov_bg, 5.0f, 0.0f);
            }

            Color fg = (is_sel || is_hov) ? text_active : text_idle;
            float cx = tr.min_x + 11.0f;
            if (!tab.svg_path.empty()) {
                float isz = 14.0f;
                Rect ir = Rect::from_xywh(std::round(cx), std::round(tr.min_y + (tr.height() - isz) * 0.5f), isz, isz);
                SvgEngine::draw_svg_path(ctx, tab.svg_path, 24.0f, 24.0f, ir, fg);
                cx += isz + 7.0f;
            }

            float right_pad = tab.is_closable ? 30.0f : 10.0f;
            float label_w = std::max(12.0f, tr.max_x - right_pad - cx);
            ctx.push_clip(Rect::from_xywh(cx, tr.min_y, label_w, tr.height()), 0.0f);
            Vec2 ls = TextMeasure::measure(tab.header, 12.5f);
            ctx.draw_text(tab.header, PointF{std::round(cx), std::round(tr.min_y + (tr.height() - ls.y) * 0.5f)}, 12.5f, fg);
            ctx.pop_clip();

            // 关闭按钮 (×)
            if (tab.is_closable && (is_sel || is_hov)) {
                Rect cr = close_rect_for(tr);
                if (m_hovered_close == static_cast<int>(i)) {
                    ctx.draw_rect(cr, hov_bg, 4.0f, 0.0f);
                }
                float xcx = cr.min_x + cr.width() * 0.5f;
                float xcy = cr.min_y + cr.height() * 0.5f;
                ctx.draw_line(PointF{xcx - 3.5f, xcy - 3.5f}, PointF{xcx + 3.5f, xcy + 3.5f}, fg, 1.3f);
                ctx.draw_line(PointF{xcx + 3.5f, xcy - 3.5f}, PointF{xcx - 3.5f, xcy + 3.5f}, fg, 1.3f);
            }
        }

        // 5. 尾部新建标签 (+) 按钮
        if (m_show_add_button && m_add_btn_rect.width() > 0.0f) {
            if (m_add_hovered) {
                ctx.draw_rect(m_add_btn_rect, hov_bg, 5.0f, 0.0f);
            }
            float px = m_add_btn_rect.min_x + m_add_btn_rect.width() * 0.5f;
            float py = m_add_btn_rect.min_y + m_add_btn_rect.height() * 0.5f;
            ctx.draw_line(PointF{px - 4.5f, py}, PointF{px + 4.5f, py}, text_active, 1.4f);
            ctx.draw_line(PointF{px, py - 4.5f}, PointF{px, py + 4.5f}, text_active, 1.4f);
        }

        // 6. 绘制当前激活标签的内容视图 (带方向微滑入过渡)
        if (m_active_content_box) {
            Rect content_clip = Rect::from_xywh(1.0f, kStripH + 1.0f,
                                                std::max(0.0f, w - 2.0f),
                                                std::max(0.0f, h - kStripH - 2.0f));
            ctx.push_clip(content_clip, 0.0f);
            {
                Vec2 off = m_active_content_box->offset();
                off.x += m_content_slide_x;
                PaintContext::OffsetScope scope(ctx, off);
                m_active_content_box->paint(ctx);
            }
            ctx.pop_clip();
        }
    }

private:
    static constexpr float kStripH = 38.0f;

    [[nodiscard]] static Rect close_rect_for(const Rect& tab_rect) noexcept {
        return Rect::from_xywh(tab_rect.max_x - 26.0f, tab_rect.min_y + (tab_rect.height() - 20.0f) * 0.5f, 20.0f, 20.0f);
    }

    void compute_tab_rects(float w) {
        m_tab_rects.clear();
        float add_reserve = m_show_add_button ? 38.0f : 8.0f;
        float avail = std::max(60.0f, w - 12.0f - add_reserve);
        size_t n = m_tabs.size();
        float tab_w = (n > 0) ? std::clamp(avail / static_cast<float>(n), 92.0f, 200.0f) : 160.0f;

        float x = 6.0f;
        for (size_t i = 0; i < n; ++i) {
            float tw = std::min(tab_w, std::max(64.0f, avail - (x - 6.0f)));
            m_tab_rects.push_back(Rect::from_xywh(std::round(x), 4.0f, std::round(tw), kStripH - 4.0f));
            x += std::round(tw);
            if (x > w - add_reserve) break;
        }
        if (m_show_add_button) {
            m_add_btn_rect = Rect::from_xywh(std::min(x + 4.0f, w - 34.0f), 6.0f, 28.0f, 26.0f);
        } else {
            m_add_btn_rect = Rect::from_xywh(0, 0, 0, 0);
        }
    }

    void rebuild_active_content() {
        if (m_active_content_box) {
            remove_child(m_active_content_box.get());
            m_active_content_box.reset();
            m_active_content_widget.reset();
        }
        if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_tabs.size()) && m_content_builder) {
            m_active_content_widget = m_content_builder(m_tabs[static_cast<size_t>(m_selected_index)].id);
            if (m_active_content_widget) {
                m_active_content_box = m_active_content_widget->create_render_node();
                if (m_active_content_box) {
                    append_child(m_active_content_box.get());
                }
            }
        }
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            int ht = -1;
            int hc = -1;
            bool ha = m_show_add_button && m_add_btn_rect.contains(e.local_position);
            if (!ha && e.local_position.y <= kStripH) {
                for (size_t i = 0; i < m_tab_rects.size(); ++i) {
                    if (m_tab_rects[i].contains(e.local_position)) {
                        ht = static_cast<int>(i);
                        if (m_tabs[i].is_closable && close_rect_for(m_tab_rects[i]).contains(e.local_position)) {
                            hc = static_cast<int>(i);
                        }
                        break;
                    }
                }
            }
            if (ht != m_hovered_tab || hc != m_hovered_close || ha != m_add_hovered) {
                m_hovered_tab = ht;
                m_hovered_close = hc;
                m_add_hovered = ha;
                cursor_shape = (ht >= 0 || ha) ? CursorShape::PointingHand : CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_tab != -1 || m_hovered_close != -1 || m_add_hovered) {
                m_hovered_tab = -1;
                m_hovered_close = -1;
                m_add_hovered = false;
                cursor_shape = CursorShape::Arrow;
                mark_needs_paint();
            }
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary || e.local_position.y > kStripH) return;
            if (m_show_add_button && m_add_btn_rect.contains(e.local_position)) {
                if (m_on_add_tab) m_on_add_tab();
                e.stop_propagation();
                return;
            }
            for (size_t i = 0; i < m_tab_rects.size(); ++i) {
                if (m_tab_rects[i].contains(e.local_position)) {
                    if (m_tabs[i].is_closable && close_rect_for(m_tab_rects[i]).contains(e.local_position)) {
                        if (m_on_tab_close) m_on_tab_close(static_cast<int>(i));
                    } else if (static_cast<int>(i) != m_selected_index) {
                        trigger_tab_switch_anim(m_selected_index, static_cast<int>(i));
                        m_selected_index = static_cast<int>(i);
                        rebuild_active_content();
                        mark_needs_layout();
                        if (m_on_selection_changed) m_on_selection_changed(m_selected_index);
                    }
                    e.stop_propagation();
                    return;
                }
            }
        };
    }

    void trigger_tab_switch_anim(int old_idx, int new_idx) noexcept {
        if (old_idx >= 0 && new_idx >= 0 && old_idx != new_idx) {
            m_content_slide_x = (new_idx > old_idx ? 1.0f : -1.0f) * 18.0f;
            m_last_anim_time = std::chrono::steady_clock::now();
            m_has_anim_time = true;
            mark_needs_paint();
        }
    }

    std::vector<TabViewItem> m_tabs;
    int m_selected_index{0};
    bool m_show_add_button{true};
    float m_height{260.0f};
    Fn<WidgetPtr(const std::string&)> m_content_builder;
    Fn<void(int)> m_on_selection_changed;
    Fn<void(int)> m_on_tab_close;
    Fn<void()> m_on_add_tab;

    std::vector<Rect> m_tab_rects;
    Rect m_add_btn_rect;
    int m_hovered_tab{-1};
    int m_hovered_close{-1};
    bool m_add_hovered{false};
    float m_ind_x{0.0f};
    float m_ind_w{0.0f};
    bool m_ind_init{false};
    float m_content_slide_x{0.0f};
    bool m_has_anim_time{false};
    std::chrono::steady_clock::time_point m_last_anim_time{};
    WidgetPtr m_active_content_widget;
    std::shared_ptr<RenderBox> m_active_content_box;

public:
    void wrap_on_selection_changed(Fn<void(int)> extra) {
        auto prev = std::move(m_on_selection_changed);
        m_on_selection_changed = [prev = std::move(prev), extra = std::move(extra)](int idx) {
            if (extra) extra(idx);
            if (prev) prev(idx);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 TabView
struct TabView : BindableMixin<TabView, RenderTabView> {
    std::vector<TabViewItem> tabs{};
    int selected_index{0};
    bool is_add_tab_button_visible{true};
    float height{260.0f};
    Fn<WidgetPtr(const std::string&)> content_builder{nullptr};
    Fn<void(int)> on_selection_changed{nullptr};
    Fn<void(int)> on_tab_close{nullptr};
    Fn<void()> on_add_tab{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderTabView>& node, Property<int>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_selected_index_value(prop.get());
    std::weak_ptr<RenderTabView> weak = node;
    bg.subscribe(prop, [weak, guard](int idx) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_selected_index_value(idx);
        }
    });
    Property<int>* p = &prop;
    node->wrap_on_selection_changed([p, guard](int idx) {
        *guard = true;
        p->set(idx);
        *guard = false;
    });
}

class TabViewWidget : public Widget {
public:
    explicit TabViewWidget(const TabView& desc) : m_desc(desc) {}
    explicit TabViewWidget(TabView&& desc) noexcept : m_desc(std::move(desc)) {}
    ~TabViewWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTabView>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto tv = std::static_pointer_cast<RenderTabView>(node)) {
            apply(tv);
        }
    }

private:
    void apply(const std::shared_ptr<RenderTabView>& tv) const {
        tv->set_properties(
            m_desc.tabs, m_desc.selected_index, m_desc.is_add_tab_button_visible,
            m_desc.height, m_desc.content_builder,
            m_desc.on_selection_changed, m_desc.on_tab_close, m_desc.on_add_tab);
    }

    TabView m_desc;
};

inline TabView::operator WidgetPtr() const& {
    return make_widget<TabViewWidget>(*this);
}
inline TabView::operator WidgetPtr() && {
    return make_widget<TabViewWidget>(std::move(*this));
}

} // namespace ktui
