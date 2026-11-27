#pragma once

namespace ktui {

namespace detail {

// 递归通知所有存活窗口清理该子树在 EventDispatcher / FocusManager 中的悬停、按压与焦点引用，
// 防止跨窗口迁移 RenderBox 或切换标签页时产生悬空指针崩溃
inline void forget_render_subtree(RenderNode* node) noexcept {
    if (!node) return;
    if (auto* box = node->as_render_box()) {
        if (RenderBox::s_destroy_hook) {
            RenderBox::s_destroy_hook(box);
        }
    }
    for (RenderNode* c = node->first_child(); c != nullptr; c = c->next_sibling()) {
        forget_render_subtree(c);
    }
}

// 独立弹出窗口的内置 Visual Studio 风格极简无边框容器：
// - 26px 紧凑标题栏（无系统大标题栏），右侧仅保留极简「停靠 / 最大化 / 关闭」图标按钮
// - 按住标题栏拖拽可平滑移动窗口，拖回原始父工作区时实时触发五向停靠指南针并松手自动吸附停靠
// - 双击标题栏切换最大化/还原；右键标题栏弹出 Fluent 2 停靠上下文菜单
class RenderDetachedPaneShell : public RenderBox {
public:
    static constexpr float kBarHeight = 26.0f;
    static constexpr float kBtnWidth  = 28.0f;

    struct Callbacks {
        Fn<void()> on_redock;
        Fn<void(DockZone)> on_dock_to;
        Fn<void(Vec2)> on_drag_move; // 参数：鼠标屏幕物理像素坐标
        Fn<void(Vec2)> on_drag_end;  // 参数：鼠标屏幕物理像素坐标
    };

    RenderDetachedPaneShell(std::string title,
                            std::shared_ptr<RenderBox> content,
                            std::weak_ptr<Window> host_window,
                            Callbacks callbacks)
        : m_title(std::move(title)),
          m_content(std::move(content)),
          m_host_window(std::move(host_window)),
          m_cb(std::move(callbacks)) {
        if (m_content) {
            if (m_content->parent()) {
                m_content->parent()->remove_child(m_content.get());
            }
            append_child(m_content.get());
        }
        setup_behavior();
    }

    ~RenderDetachedPaneShell() override {
        release_content();
    }

    // 在停靠回主工作区前安全摘除内容节点，确保不被本容器析构时误伤
    std::shared_ptr<RenderBox> release_content() {
        if (m_content) {
            forget_render_subtree(m_content.get());
            if (m_content->parent() == this) {
                remove_child(m_content.get());
            }
        }
        return std::move(m_content);
    }

    // 对接 Windows 11 原生分屏布局 (Snap Layouts)：悬停最大化按钮时返回 MaximizeBtn (HTMAXBUTTON)
    [[nodiscard]] TitlebarHitTest titlebar_hit_test(PointF local_pos) const noexcept {
        if (m_btn_max.contains(local_pos)) {
            return TitlebarHitTest::MaximizeBtn;
        }
        return TitlebarHitTest::Client;
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 420.0f;
        float h = m_constraints.has_bounded_height() ? m_constraints.max_height : 300.0f;
        m_size = m_constraints.constrain(Vec2{w, h});

        // 右上角 3 个极简图标按钮：[↙ 停靠] [□ 最大化] [× 关闭]
        float bx = std::max(0.0f, m_size.x - kBtnWidth * 3.0f - 1.0f);
        m_btn_dock  = Rect::from_xywh(bx, 1.0f, kBtnWidth, kBarHeight - 2.0f);
        m_btn_max   = Rect::from_xywh(bx + kBtnWidth, 1.0f, kBtnWidth, kBarHeight - 2.0f);
        m_btn_close = Rect::from_xywh(bx + kBtnWidth * 2.0f, 1.0f, kBtnWidth, kBarHeight - 2.0f);

        if (m_content) {
            float cw = std::max(0.0f, m_size.x - 2.0f);
            float ch = std::max(0.0f, m_size.y - kBarHeight - 1.0f);
            m_content->layout(BoxConstraints::tight(Vec2{cw, ch}), true);
            m_content->set_offset(Vec2{1.0f, kBarHeight});
        }
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        if (position.y < kBarHeight) {
            if (m_btn_dock.contains(position) || m_btn_max.contains(position) || m_btn_close.contains(position)) {
                cursor_shape = CursorShape::PointingHand;
            } else {
                cursor_shape = CursorShape::Arrow;
            }
            result.add(this, position);
            return true;
        }
        cursor_shape = CursorShape::Arrow;
        if (m_content) {
            PointF cp{position.x - m_content->offset().x, position.y - m_content->offset().y};
            if (m_content->hit_test(result, cp)) {
                result.add(this, position);
                return true;
            }
        }
        result.add(this, position);
        return true;
    }

    bool hit_test_touch(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        if (position.y < kBarHeight) {
            result.add(this, position);
            return true;
        }
        if (m_content) {
            PointF cp{position.x - m_content->offset().x, position.y - m_content->offset().y};
            if (m_content->hit_test_touch(result, cp)) {
                result.add(this, position);
                return true;
            }
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        const Theme& t = ThemeContext::current();
        const auto resolved_style = DockPanelStyleResolver::resolve(t);
        const bool is_dark = t.is_dark;
        const bool is_max = [&]() {
            auto w = m_host_window.lock();
            return w && w->is_maximized();
        }();

        const Color bg       = is_dark ? Color{0x20 / 255.0f, 0x20 / 255.0f, 0x22 / 255.0f, 1.0f}
                                       : Color{0xF5 / 255.0f, 0xF7 / 255.0f, 0xFA / 255.0f, 1.0f};
        const Color bar_bg   = is_dark ? Color{0x28 / 255.0f, 0x28 / 255.0f, 0x2C / 255.0f, 1.0f}
                                       : Color{0xEA / 255.0f, 0xEE / 255.0f, 0xF4 / 255.0f, 1.0f};
        const Color border   = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.14f}  : Color{0.0f, 0.0f, 0.0f, 0.14f};
        const Color hairline = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.07f}  : Color{0.0f, 0.0f, 0.0f, 0.07f};
        const Color hov_bg   = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.09f}  : Color{0.0f, 0.0f, 0.0f, 0.06f};
        const Color close_hv = Color{0xC4 / 255.0f, 0x2B / 255.0f, 0x1C / 255.0f, 1.0f};
        const Color icon_col = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.78f}  : Color{0.0f, 0.0f, 0.0f, 0.72f};

        // 主体底板 + 26px 紧凑标题栏 + 底部分隔线
        context.draw_rect(bounds(), bg, 0.0f, 0.0f);
        context.draw_rect(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, m_size.x - 2.0f), kBarHeight - 1.0f), bar_bg, 0.0f, 0.0f);
        context.draw_rect(Rect::from_xywh(1.0f, kBarHeight - 1.0f, std::max(0.0f, m_size.x - 2.0f), 1.0f), hairline, 0.0f, 0.0f);

        // 左侧紧凑标题（裁切防止与右侧按钮重叠）
        const float title_max_w = std::max(0.0f, m_btn_dock.min_x - 14.0f);
        if (title_max_w > 8.0f) {
            context.push_clip(Rect::from_xywh(10.0f, 0.0f, title_max_w, kBarHeight), 0.0f);
            Vec2 ts = TextMeasure::measure(m_title, 11.5f, FontWeight::Medium);
            context.draw_text(m_title, PointF{10.0f, std::round((kBarHeight - ts.y) * 0.5f)},
                              11.5f, resolved_style.text_primary, FontWeight::Medium);
            context.pop_clip();
        }

        // 右侧极简按钮 1: 停靠回主工作区 (↙)
        if (m_hovered_btn == 1) context.draw_rect(m_btn_dock, hov_bg, 3.0f, 0.0f);
        {
            const std::string g = "↙";
            Vec2 gs = TextMeasure::measure(g, 11.5f, FontWeight::Regular);
            context.draw_text(g, PointF{std::round(m_btn_dock.min_x + (m_btn_dock.width() - gs.x) * 0.5f),
                                        std::round(m_btn_dock.min_y + (m_btn_dock.height() - gs.y) * 0.5f)},
                              11.5f, icon_col, FontWeight::Regular);
        }

        // 右侧极简按钮 2: 最大化 / 还原 (极简矢量方框)
        if (m_hovered_btn == 2) context.draw_rect(m_btn_max, hov_bg, 3.0f, 0.0f);
        {
            float cx = std::round(m_btn_max.min_x + m_btn_max.width() * 0.5f);
            float cy = std::round(m_btn_max.min_y + m_btn_max.height() * 0.5f);
            if (is_max) {
                context.draw_rect(Rect::from_xywh(cx - 3.0f, cy - 5.0f, 7.0f, 7.0f), icon_col, 1.0f, 1.0f);
                context.draw_rect(Rect::from_xywh(cx - 5.0f, cy - 3.0f, 7.0f, 7.0f), bar_bg, 1.0f, 0.0f);
                context.draw_rect(Rect::from_xywh(cx - 5.0f, cy - 3.0f, 7.0f, 7.0f), icon_col, 1.0f, 1.0f);
            } else {
                context.draw_rect(Rect::from_xywh(cx - 4.5f, cy - 4.5f, 9.0f, 9.0f), icon_col, 1.0f, 1.0f);
            }
        }

        // 右侧极简按钮 3: 关闭 (×)
        if (m_hovered_btn == 3) context.draw_rect(m_btn_close, close_hv, 3.0f, 0.0f);
        {
            const std::string g = "×";
            Vec2 gs = TextMeasure::measure(g, 13.0f, FontWeight::Regular);
            Color xc = (m_hovered_btn == 3) ? Color::white() : icon_col;
            context.draw_text(g, PointF{std::round(m_btn_close.min_x + (m_btn_close.width() - gs.x) * 0.5f),
                                        std::round(m_btn_close.min_y + (m_btn_close.height() - gs.y) * 0.5f)},
                              13.0f, xc, FontWeight::Regular);
        }

        // 内容区
        if (m_content) {
            Rect clip = Rect::from_xywh(1.0f, kBarHeight, std::max(0.0f, m_size.x - 2.0f),
                                       std::max(0.0f, m_size.y - kBarHeight - 1.0f));
            context.push_clip(clip, 0.0f);
            {
                PaintContext::OffsetScope sc(context, m_content->offset());
                m_content->paint(context);
            }
            context.pop_clip();
        }

        // 最外层极细 1px 窗口描边（对齐 Visual Studio 浮动窗质感）
        context.draw_rect(bounds(), border, 0.0f, 1.0f);
    }

private:
    [[nodiscard]] int hit_button(PointF p) const noexcept {
        if (m_btn_dock.contains(p))  return 1;
        if (m_btn_max.contains(p))   return 2;
        if (m_btn_close.contains(p)) return 3;
        return 0;
    }

    [[nodiscard]] Vec2 to_screen_phys(PointF local_pos) const {
        if (auto w = m_host_window.lock()) {
            Vec2 wp = w->position();
            float dpi = std::max(0.1f, w->dpi_scale());
            return Vec2{wp.x + local_pos.x * dpi, wp.y + local_pos.y * dpi};
        }
        return Vec2{local_pos.x, local_pos.y};
    }

    void open_titlebar_context_menu(PointF window_pos) {
        std::vector<MenuFlyoutItem> items;
        if (m_cb.on_redock) {
            items.push_back(MenuFlyoutItem{.label = FrameworkStrings::current().dock_back_to_workspace, .on_click = m_cb.on_redock});
            items.push_back(MenuFlyoutItem::separator());
        }
        if (m_cb.on_dock_to) {
            auto dock_cb = m_cb.on_dock_to;
            items.push_back(MenuFlyoutItem{.label = FrameworkStrings::current().dock_left,   .on_click = [dock_cb]() { dock_cb(DockZone::Left); }});
            items.push_back(MenuFlyoutItem{.label = FrameworkStrings::current().dock_right,  .on_click = [dock_cb]() { dock_cb(DockZone::Right); }});
            items.push_back(MenuFlyoutItem{.label = FrameworkStrings::current().dock_top,    .on_click = [dock_cb]() { dock_cb(DockZone::Top); }});
            items.push_back(MenuFlyoutItem{.label = FrameworkStrings::current().dock_bottom, .on_click = [dock_cb]() { dock_cb(DockZone::Bottom); }});
            items.push_back(MenuFlyoutItem{.label = FrameworkStrings::current().dock_center, .on_click = [dock_cb]() { dock_cb(DockZone::Center); }});
            items.push_back(MenuFlyoutItem::separator());
        }
        if (auto w = m_host_window.lock()) {
            const bool is_max = w->is_maximized();
            auto hw = m_host_window;
            items.push_back(MenuFlyoutItem{
                .label = is_max ? FrameworkStrings::current().dock_restore_window : FrameworkStrings::current().dock_maximize_window,
                .on_click = [hw]() {
                    if (auto win = hw.lock()) {
                        if (win->is_maximized()) win->restore();
                        else win->maximize();
                    }
                }
            });
        }
        if (m_cb.on_redock) {
            items.push_back(MenuFlyoutItem{.label = FrameworkStrings::current().dock_close_and_redock, .on_click = m_cb.on_redock});
        }
        MenuFlyout::show_at(std::move(items), window_pos);
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& ev) {
            if (m_dragging_window) {
                if (auto w = m_host_window.lock()) {
                    const float dpi = std::max(0.1f, w->dpi_scale());
                    Vec2 screen_cur = to_screen_phys(ev.local_position);
                    if (w->is_maximized()) {
                        float ratio = std::clamp(ev.local_position.x / std::max(1.0f, m_size.x), 0.1f, 0.9f);
                        w->restore();
                        m_drag_grab_local.x = w->size().x * ratio;
                        m_drag_grab_local.y = std::min(m_drag_grab_local.y, kBarHeight * 0.5f);
                    }
                    Vec2 next_pos{
                        std::round(screen_cur.x - m_drag_grab_local.x * dpi),
                        std::round(screen_cur.y - m_drag_grab_local.y * dpi)
                    };
                    w->set_position(next_pos);
                    if (m_cb.on_drag_move) m_cb.on_drag_move(screen_cur);
                }
                ev.stop_propagation();
                return;
            }
            int hov = (ev.local_position.y < kBarHeight) ? hit_button(ev.local_position) : 0;
            if (m_hovered_btn != hov) {
                m_hovered_btn = hov;
                mark_needs_paint();
            }
        };

        on_pointer_exit = [this](PointerEvent&) {
            if (!m_dragging_window && (m_hovered_btn != 0 || m_pressed_btn != 0)) {
                m_hovered_btn = 0;
                m_pressed_btn = 0;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.local_position.y >= kBarHeight) return;

            if (ev.button == PointerButton::Secondary) {
                open_titlebar_context_menu(ev.position);
                ev.stop_propagation();
                return;
            }
            if (ev.button != PointerButton::Primary) return;

            int btn = hit_button(ev.local_position);
            if (btn != 0) {
                m_pressed_btn = btn;
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }

            // 检测标题栏双击：切换最大化 / 还原
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
            if (elapsed_ms <= 360 && std::hypot(ev.local_position.x - m_last_click_pos.x,
                                                ev.local_position.y - m_last_click_pos.y) <= 6.0f) {
                m_last_click_time = {};
                if (auto w = m_host_window.lock()) {
                    if (w->is_maximized()) w->restore();
                    else w->maximize();
                    mark_needs_paint();
                }
                ev.stop_propagation();
                return;
            }
            m_last_click_time = now;
            m_last_click_pos = ev.local_position;

            // 开始按住标题栏拖拽窗口（可自由移动或直接拖回父工作区停靠）
            m_dragging_window = true;
            m_drag_grab_local = ev.local_position;
            ev.stop_propagation();
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (m_dragging_window) {
                m_dragging_window = false;
                Vec2 screen_cur = to_screen_phys(ev.local_position);
                if (m_cb.on_drag_end) m_cb.on_drag_end(screen_cur);
                ev.stop_propagation();
                return;
            }
            if (m_pressed_btn != 0) {
                int btn = m_pressed_btn;
                m_pressed_btn = 0;
                mark_needs_paint();
                if (hit_button(ev.local_position) == btn) {
                    if (btn == 1 || btn == 3) {
                        if (m_cb.on_redock) {
                            auto cb = m_cb.on_redock;
                            cb();
                        }
                    } else if (btn == 2) {
                        if (auto w = m_host_window.lock()) {
                            if (w->is_maximized()) w->restore();
                            else w->maximize();
                            mark_needs_paint();
                        }
                    }
                }
                ev.stop_propagation();
            }
        };
    }

    std::string m_title;
    std::shared_ptr<RenderBox> m_content;
    std::weak_ptr<Window> m_host_window;
    Callbacks m_cb;

    Rect m_btn_dock{};
    Rect m_btn_max{};
    Rect m_btn_close{};
    int m_hovered_btn{0};
    int m_pressed_btn{0};

    bool m_dragging_window{false};
    PointF m_drag_grab_local{};
    std::chrono::steady_clock::time_point m_last_click_time{};
    PointF m_last_click_pos{};
};

class DetachedPaneHostWidget : public Widget {
public:
    explicit DetachedPaneHostWidget(std::shared_ptr<RenderDetachedPaneShell> shell)
        : m_shell(std::move(shell)) {}

    std::shared_ptr<RenderBox> create_render_node() override { return m_shell; }
    void update_render_node(std::shared_ptr<RenderBox>) override {}

private:
    std::shared_ptr<RenderDetachedPaneShell> m_shell;
};

} // namespace detail

// Visual Studio + Fluent 2 风格可拖拽、全区域磁吸、开箱即用可撕出独立窗口的停靠工作区 (RenderDockPanel)

} // namespace ktui
