#pragma once

#include "widget.hpp"
#include "menu_flyout.hpp"
#include "split_view.hpp"
#include <ktui/app/application.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ktui {

// 五区停靠方位
enum class DockZone : uint8_t {
    Left = 0,
    Right = 1,
    Top = 2,
    Bottom = 3,
    Center = 4
};

// 单个可拖拽停靠面板描述
struct DockPane {
    std::string id;
    std::string title;
    DockZone zone{DockZone::Center};
    WidgetPtr content{nullptr};
    bool closable{false};
};

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

    void paint(PaintContext& context) override {
        const Theme& t = ThemeContext::current();
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
                              11.5f, t.colors.text_primary, FontWeight::Medium);
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
            items.push_back(MenuFlyoutItem{.label = "停靠回主工作区", .on_click = m_cb.on_redock});
            items.push_back(MenuFlyoutItem::separator());
        }
        if (m_cb.on_dock_to) {
            auto dock_cb = m_cb.on_dock_to;
            items.push_back(MenuFlyoutItem{.label = "停靠到左侧 (Left)",   .on_click = [dock_cb]() { dock_cb(DockZone::Left); }});
            items.push_back(MenuFlyoutItem{.label = "停靠到右侧 (Right)",  .on_click = [dock_cb]() { dock_cb(DockZone::Right); }});
            items.push_back(MenuFlyoutItem{.label = "停靠到顶部 (Top)",    .on_click = [dock_cb]() { dock_cb(DockZone::Top); }});
            items.push_back(MenuFlyoutItem{.label = "停靠到底部 (Bottom)", .on_click = [dock_cb]() { dock_cb(DockZone::Bottom); }});
            items.push_back(MenuFlyoutItem{.label = "停靠到中心 (Center)", .on_click = [dock_cb]() { dock_cb(DockZone::Center); }});
            items.push_back(MenuFlyoutItem::separator());
        }
        if (auto w = m_host_window.lock()) {
            const bool is_max = w->is_maximized();
            auto hw = m_host_window;
            items.push_back(MenuFlyoutItem{
                .label = is_max ? "还原窗口" : "最大化窗口",
                .on_click = [hw]() {
                    if (auto win = hw.lock()) {
                        if (win->is_maximized()) win->restore();
                        else win->maximize();
                    }
                }
            });
        }
        if (m_cb.on_redock) {
            items.push_back(MenuFlyoutItem{.label = "关闭并停靠回工作区", .on_click = m_cb.on_redock});
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
class RenderDockPanel : public RenderBox {
public:
    static constexpr float kHeaderHeight = 32.0f;
    static constexpr float kSplitterHit  = 6.0f;
    static constexpr float kMinPaneSize  = 110.0f;

    struct LivePane {
        std::string id;
        std::string title;
        DockZone zone{DockZone::Center};
        DockZone initial_zone{DockZone::Center};
        bool closable{false};
        bool closed{false};
        bool detached{false};
        std::shared_ptr<RenderBox> content_box;
        std::weak_ptr<Window> detached_window;
        std::weak_ptr<detail::RenderDetachedPaneShell> detached_shell;
    };

    using DetachCallback = Fn<bool(const std::string& id, const std::string& title, std::shared_ptr<RenderBox> content_box, Fn<void()> on_redock)>;

    RenderDockPanel(float left_w = 200.0f,
                    float right_w = 200.0f,
                    float top_h = 110.0f,
                    float bottom_h = 120.0f,
                    EdgeInsets padding = EdgeInsets::zero(),
                    bool allow_detach_window = true,
                    Fn<void(const std::string&, DockZone)> on_dock_changed = nullptr,
                    Fn<void(const std::string&, bool)> on_detach_changed = nullptr,
                    DetachCallback on_detach_pane = nullptr) noexcept
        : m_left_w(left_w), m_right_w(right_w), m_top_h(top_h), m_bottom_h(bottom_h),
          m_init_left_w(left_w), m_init_right_w(right_w), m_init_top_h(top_h), m_init_bottom_h(bottom_h),
          m_padding(padding),
          m_allow_detach_window(allow_detach_window),
          m_on_dock_changed(std::move(on_dock_changed)),
          m_on_detach_changed(std::move(on_detach_changed)),
          m_on_detach_pane(std::move(on_detach_pane)) {
        setup_behavior();
    }

    ~RenderDockPanel() override {
        if (m_alive) *m_alive = false;
        // 离开页面或销毁工作区时，延迟安全关闭所有仍打开的独立弹出窗口
        std::vector<std::weak_ptr<Window>> to_close;
        for (auto& p : m_panes) {
            if (!p.detached_window.expired()) {
                to_close.push_back(p.detached_window);
            }
        }
        if (!to_close.empty()) {
            if (auto* app = Application::current()) {
                app->dispatcher().post_task([wins = std::move(to_close)]() {
                    for (const auto& w : wins) {
                        if (auto win = w.lock(); win && !win->is_closed()) {
                            win->close();
                        }
                    }
                });
            }
        }
    }

    void set_panes(std::vector<LivePane> panes) {
        while (first_child()) remove_child(first_child());
        m_panes = std::move(panes);
        for (auto& p : m_panes) {
            if (p.content_box && !p.detached) {
                if (p.content_box->parent()) p.content_box->parent()->remove_child(p.content_box.get());
                append_child(p.content_box.get());
            }
        }
        m_active_tab.fill(0);
        mark_needs_layout();
    }

    void set_allow_detach_window(bool allow) noexcept {
        if (m_allow_detach_window != allow) {
            m_allow_detach_window = allow;
            mark_needs_paint();
        }
    }
    void set_on_dock_changed(Fn<void(const std::string&, DockZone)> cb) { m_on_dock_changed = std::move(cb); }
    void set_on_detach_changed(Fn<void(const std::string&, bool)> cb) { m_on_detach_changed = std::move(cb); }
    void set_on_detach_pane(DetachCallback cb) { m_on_detach_pane = std::move(cb); }
    void set_padding(EdgeInsets p) noexcept {
        if (!(m_padding == p)) { m_padding = p; mark_needs_layout(); }
    }

    // 编程式将指定面板停靠至目标区域 (若已弹出独立窗口则安全收回原 RenderBox)
    bool dock_pane(const std::string& id, DockZone target) {
        for (size_t i = 0; i < m_panes.size(); ++i) {
            auto& p = m_panes[i];
            if (p.id != id) continue;

            const bool was_detached = p.detached;
            if (auto sh = p.detached_shell.lock()) {
                auto reclaimed = sh->release_content();
                if (reclaimed) p.content_box = std::move(reclaimed);
            }
            p.detached_shell.reset();

            auto old_win = p.detached_window.lock();
            p.detached_window.reset();

            if (p.content_box) {
                detail::forget_render_subtree(p.content_box.get());
                if (p.content_box->parent() != this) {
                    if (p.content_box->parent()) p.content_box->parent()->remove_child(p.content_box.get());
                    append_child(p.content_box.get());
                }
            }

            p.zone = target;
            p.closed = false;
            p.detached = false;
            auto list = panes_in_zone(target);
            for (size_t k = 0; k < list.size(); ++k) {
                if (list[k] == i) {
                    const size_t tz = static_cast<size_t>(target);
                    if (m_active_tab[tz] != k) {
                        m_active_tab[tz] = k;
                        m_tab_anim_t[tz] = 0.0f;
                    }
                    break;
                }
            }

            // 延迟关闭原独立窗口，绝不在回调栈内同步销毁 HWND
            if (old_win && !old_win->is_closed()) {
                if (auto* app = Application::current()) {
                    app->dispatcher().post_task([old_win]() {
                        if (!old_win->is_closed()) old_win->close();
                    });
                }
            }

            mark_needs_layout();
            mark_needs_paint();
            if (was_detached && m_on_detach_changed) m_on_detach_changed(id, false);
            if (m_on_dock_changed) m_on_dock_changed(id, target);
            return true;
        }
        return false;
    }

    // 将指定面板安全撕出为独立 OS 窗口 (内置默认实现，无需业务层编写任何样板代码)
    bool detach_pane(const std::string& id) {
        if (!can_detach()) return false;
        for (size_t i = 0; i < m_panes.size(); ++i) {
            auto& p = m_panes[i];
            if (p.id != id || p.closed || p.detached) continue;

            std::string pid = p.id;
            std::string ptitle = p.title;
            DockZone prev_zone = p.zone;
            std::weak_ptr<bool> alive = m_alive;

            // 先安全清理悬停/焦点引用并从当前工作区摘除该面板的 RenderBox
            if (p.content_box) {
                detail::forget_render_subtree(p.content_box.get());
                if (p.content_box->parent() == this) {
                    remove_child(p.content_box.get());
                }
            }
            p.detached = true;
            mark_needs_layout();
            mark_needs_paint();

            auto on_redock = [this, alive, pid, prev_zone]() {
                if (auto g = alive.lock(); g && *g) {
                    dock_pane(pid, prev_zone);
                }
            };

            if (m_on_detach_pane) {
                bool ok = m_on_detach_pane(pid, ptitle, p.content_box, std::move(on_redock));
                if (!ok) {
                    dock_pane(pid, prev_zone);
                    return false;
                }
                if (m_on_detach_changed) m_on_detach_changed(pid, true);
                return true;
            }

            // 内置开箱即用的跨平台独立窗口撕出 (严格通过 post_task 异步创建，杜绝 WndProc 重入崩溃)
            auto* app = Application::current();
            if (!app) {
                dock_pane(pid, prev_zone);
                return false;
            }

            // 捕获当前所属父窗口，使浮动工具窗始终位于父窗口之上并支持按住标题栏拖回吸附
            std::weak_ptr<Window> parent_win_weak;
            Window* active_win = Window::active_window();
            for (const auto& w : app->windows()) {
                if (w && w.get() == active_win) {
                    parent_win_weak = w;
                    break;
                }
            }
            if (parent_win_weak.expired()) {
                parent_win_weak = app->main_window();
            }

            auto content_ref = p.content_box;
            PointF launch_hint = m_drag_pos;
            app->dispatcher().post_task([this, alive, pid, ptitle, prev_zone, content_ref, parent_win_weak, launch_hint, on_redock = std::move(on_redock)]() mutable {
                auto guard = alive.lock();
                if (!guard || !*guard) return;
                auto* cur_app = Application::current();
                if (!cur_app) return;

                // 防止在 post_task 排队期间面板已被重置或重新停靠，避免弹出幽灵窗口
                auto it = std::find_if(m_panes.begin(), m_panes.end(), [&](const LivePane& lp) { return lp.id == pid; });
                if (it == m_panes.end() || !it->detached || it->closed) return;

                WindowDesc wd;
                wd.title = ptitle;
                wd.size = Vec2{420.0f, 300.0f};
                wd.min_size = Vec2{260.0f, 160.0f};
                wd.resizable = true;
                wd.decorated = false;
                wd.custom_titlebar = false; // 关闭系统与默认大标题栏，由 26px Visual Studio 风格极简标题栏接管
                // 使用 Normal + parent_handle 归属窗口（而非 WS_EX_TOOLWINDOW）：既不占任务栏且保持在父窗口之上，
                // 又完整支持系统原生最大化/还原动画与 Windows 11 悬停最大化按钮的分屏布局 (Snap Layouts)
                wd.type = WindowType::Normal;

                if (auto pw = parent_win_weak.lock(); pw && !pw->is_closed()) {
                    if (pw->platform_window()) {
                        wd.parent_handle = pw->platform_window()->get_native_handle();
                    }
                    const Vec2 p_pos = pw->position();
                    const float p_dpi = std::max(0.1f, pw->dpi_scale());
                    const Vec2 dock_org = global_offset();
                    float lx = (launch_hint.x != 0.0f || launch_hint.y != 0.0f) ? launch_hint.x : m_size.x * 0.5f;
                    float ly = (launch_hint.x != 0.0f || launch_hint.y != 0.0f) ? launch_hint.y : m_size.y * 0.5f;
                    wd.position = Vec2{
                        std::max(20.0f, std::round(p_pos.x + (dock_org.x + lx - wd.size.x * 0.4f) * p_dpi)),
                        std::max(20.0f, std::round(p_pos.y + (pw->titlebar_height() + dock_org.y + ly - 14.0f) * p_dpi))
                    };
                }

                auto detached_win = cur_app->create_window(wd);
                if (!detached_win) {
                    dock_pane(pid, prev_zone);
                    return;
                }

                auto redocked = std::make_shared<bool>(false);
                auto schedule_dock_to = [this, alive, redocked, pid](DockZone target) {
                    if (*redocked) return;
                    *redocked = true;
                    if (auto* a = Application::current()) {
                        a->dispatcher().post_task([this, alive, pid, target]() {
                            if (auto g = alive.lock(); g && *g) {
                                m_external_drag_active = false;
                                m_drop_target.reset();
                                dock_pane(pid, target);
                            }
                        });
                    }
                };
                auto schedule_redock = [schedule_dock_to, prev_zone]() {
                    schedule_dock_to(prev_zone);
                };

                detail::RenderDetachedPaneShell::Callbacks cbs;
                cbs.on_redock = schedule_redock;
                cbs.on_dock_to = schedule_dock_to;
                cbs.on_drag_move = [this, alive, parent_win_weak](Vec2 screen_phys) {
                    if (auto g = alive.lock(); !g || !*g) return;
                    if (auto pw = parent_win_weak.lock(); pw && !pw->is_closed()) {
                        Vec2 p_pos = pw->position();
                        float p_dpi = std::max(0.1f, pw->dpi_scale());
                        Vec2 dock_org = global_offset();
                        PointF lp{
                            (screen_phys.x - p_pos.x) / p_dpi - dock_org.x,
                            (screen_phys.y - p_pos.y) / p_dpi - pw->titlebar_height() - dock_org.y
                        };
                        update_external_drag(lp, pw.get());
                    }
                };
                cbs.on_drag_end = [this, alive, parent_win_weak, schedule_dock_to](Vec2 screen_phys) {
                    if (auto g = alive.lock(); !g || !*g) return;
                    std::optional<DockZone> target;
                    if (auto pw = parent_win_weak.lock(); pw && !pw->is_closed()) {
                        Vec2 p_pos = pw->position();
                        float p_dpi = std::max(0.1f, pw->dpi_scale());
                        Vec2 dock_org = global_offset();
                        PointF lp{
                            (screen_phys.x - p_pos.x) / p_dpi - dock_org.x,
                            (screen_phys.y - p_pos.y) / p_dpi - pw->titlebar_height() - dock_org.y
                        };
                        target = finish_external_drag(lp, pw.get());
                    } else {
                        m_external_drag_active = false;
                        m_drop_target.reset();
                    }
                    if (target.has_value()) {
                        schedule_dock_to(*target);
                    }
                };

                auto shell = make_render_node<detail::RenderDetachedPaneShell>(
                    ptitle, content_ref, std::weak_ptr<Window>(detached_win), std::move(cbs));
                for (auto& pane : m_panes) {
                    if (pane.id == pid) {
                        pane.detached_window = detached_win;
                        pane.detached_shell = shell;
                        break;
                    }
                }

                detached_win->set_custom_hit_test([sh_weak = std::weak_ptr<detail::RenderDetachedPaneShell>(shell)](PointF p) -> TitlebarHitTest {
                    if (auto sh = sh_weak.lock()) return sh->titlebar_hit_test(p);
                    return TitlebarHitTest::Client;
                });
                detached_win->on_close([schedule_redock]() -> bool {
                    schedule_redock();
                    return true;
                });
                detached_win->set_content(make_widget<detail::DetachedPaneHostWidget>(shell));
                detached_win->show();
                if (m_on_detach_changed) m_on_detach_changed(pid, true);
            });
            return true;
        }
        return false;
    }

    // 一键恢复初始停靠布局与尺寸 (自动收回所有已弹出窗口)
    void reset_layout() {
        m_left_w = m_init_left_w;
        m_right_w = m_init_right_w;
        m_top_h = m_init_top_h;
        m_bottom_h = m_init_bottom_h;

        std::vector<std::shared_ptr<Window>> to_close;
        for (auto& p : m_panes) {
            if (auto sh = p.detached_shell.lock()) {
                auto reclaimed = sh->release_content();
                if (reclaimed) p.content_box = std::move(reclaimed);
            }
            p.detached_shell.reset();
            if (auto w = p.detached_window.lock(); w && !w->is_closed()) {
                to_close.push_back(std::move(w));
            }
            p.detached_window.reset();
            p.zone = p.initial_zone;
            p.closed = false;
            p.detached = false;
            if (p.content_box) {
                detail::forget_render_subtree(p.content_box.get());
                if (p.content_box->parent() != this) {
                    if (p.content_box->parent()) p.content_box->parent()->remove_child(p.content_box.get());
                    append_child(p.content_box.get());
                }
            }
        }
        m_active_tab.fill(0);
        m_dragging_pane.reset();
        m_dragging_splitter.reset();
        m_drop_target.reset();
        m_drag_started = false;
        m_will_detach_on_drop = false;
        m_external_drag_active = false;

        if (!to_close.empty()) {
            if (auto* app = Application::current()) {
                app->dispatcher().post_task([wins = std::move(to_close)]() {
                    for (const auto& w : wins) {
                        if (w && !w->is_closed()) w->close();
                    }
                });
            }
        }
        mark_needs_layout();
        mark_needs_paint();
    }

    [[nodiscard]] std::optional<DockZone> pane_zone(const std::string& id) const noexcept {
        for (const auto& p : m_panes) {
            if (p.id == id && !p.closed && !p.detached) return p.zone;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool is_pane_detached(const std::string& id) const noexcept {
        for (const auto& p : m_panes) {
            if (p.id == id) return p.detached;
        }
        return false;
    }

    [[nodiscard]] Rect zone_rect(DockZone z) const noexcept {
        return m_zone_rects[static_cast<size_t>(z)];
    }

    void perform_layout() override {
        BoxConstraints inner = m_constraints.deflate(m_padding);
        float w = inner.has_bounded_width() ? inner.max_width : std::max(inner.min_width, 560.0f);
        float h = inner.has_bounded_height() ? inner.max_height : std::max(inner.min_height, 360.0f);
        m_size = m_constraints.constrain(Vec2{w + m_padding.horizontal(), h + m_padding.vertical()});

        const float ox = m_padding.left + 1.0f;
        const float oy = m_padding.top + 1.0f;
        const float iw = std::max(0.0f, m_size.x - m_padding.horizontal() - 2.0f);
        const float ih = std::max(0.0f, m_size.y - m_padding.vertical() - 2.0f);
        constexpr float kHairline = 1.0f;

        m_zone_rects.fill(Rect{});
        m_splitters.clear();
        m_tab_hits.clear();

        const bool has_top    = !panes_in_zone(DockZone::Top).empty();
        const bool has_bottom = !panes_in_zone(DockZone::Bottom).empty();
        const bool has_left   = !panes_in_zone(DockZone::Left).empty();
        const bool has_right  = !panes_in_zone(DockZone::Right).empty();
        const bool has_center = !panes_in_zone(DockZone::Center).empty();
        const bool has_mid    = has_left || has_right || has_center;

        float top_h = 0.0f, bot_h = 0.0f;
        float v_gaps = 0.0f;
        if (has_top && (has_mid || has_bottom)) v_gaps += kHairline;
        if (has_bottom && has_mid) v_gaps += kHairline;
        float avail_h = std::max(0.0f, ih - v_gaps);

        if (has_top) {
            float max_t = (has_mid || has_bottom) ? std::max(kMinPaneSize, avail_h - kMinPaneSize) : avail_h;
            top_h = std::clamp(m_top_h, std::min(kMinPaneSize, max_t), max_t);
            if (!has_mid && !has_bottom) top_h = avail_h;
        }
        if (has_bottom) {
            float rem = std::max(0.0f, avail_h - top_h);
            float max_b = has_mid ? std::max(kMinPaneSize, rem - kMinPaneSize) : rem;
            bot_h = std::clamp(m_bottom_h, std::min(kMinPaneSize, max_b), max_b);
            if (!has_mid) bot_h = rem;
        }
        float mid_y = oy + top_h + (has_top && (has_mid || has_bottom) ? kHairline : 0.0f);
        float mid_h = std::max(0.0f, ih - top_h - bot_h - v_gaps);

        if (has_top) {
            m_zone_rects[static_cast<size_t>(DockZone::Top)] = Rect::from_xywh(ox, oy, iw, top_h);
            if (has_mid || has_bottom) {
                float sy = oy + top_h;
                m_splitters.push_back(Splitter{SplitterKind::Top,
                    Rect::from_xywh(ox, sy - kSplitterHit * 0.5f, iw, kSplitterHit),
                    Rect::from_xywh(ox, sy, iw, kHairline)});
            }
        }
        if (has_bottom) {
            float by = oy + ih - bot_h;
            m_zone_rects[static_cast<size_t>(DockZone::Bottom)] = Rect::from_xywh(ox, by, iw, bot_h);
            if (has_mid) {
                float sy = by - kHairline;
                m_splitters.push_back(Splitter{SplitterKind::Bottom,
                    Rect::from_xywh(ox, sy - kSplitterHit * 0.5f, iw, kSplitterHit),
                    Rect::from_xywh(ox, sy, iw, kHairline)});
            }
        }

        if (has_mid) {
            float h_gaps = 0.0f;
            if (has_left && (has_center || has_right)) h_gaps += kHairline;
            if (has_right && has_center) h_gaps += kHairline;
            float avail_w = std::max(0.0f, iw - h_gaps);

            float left_w = 0.0f, right_w = 0.0f;
            if (has_left) {
                float max_l = (has_center || has_right) ? std::max(kMinPaneSize, avail_w - kMinPaneSize) : avail_w;
                left_w = std::clamp(m_left_w, std::min(kMinPaneSize, max_l), max_l);
                if (!has_center && !has_right) left_w = avail_w;
            }
            if (has_right) {
                float rem = std::max(0.0f, avail_w - left_w);
                float max_r = has_center ? std::max(kMinPaneSize, rem - kMinPaneSize) : rem;
                right_w = std::clamp(m_right_w, std::min(kMinPaneSize, max_r), max_r);
                if (!has_center) right_w = rem;
            }
            float center_x = ox + left_w + (has_left && (has_center || has_right) ? kHairline : 0.0f);
            float center_w = std::max(0.0f, iw - left_w - right_w - h_gaps);

            if (has_left) {
                m_zone_rects[static_cast<size_t>(DockZone::Left)] = Rect::from_xywh(ox, mid_y, left_w, mid_h);
                if (has_center || has_right) {
                    float sx = ox + left_w;
                    m_splitters.push_back(Splitter{SplitterKind::Left,
                        Rect::from_xywh(sx - kSplitterHit * 0.5f, mid_y, kSplitterHit, mid_h),
                        Rect::from_xywh(sx, mid_y, kHairline, mid_h)});
                }
            }
            if (has_center) {
                m_zone_rects[static_cast<size_t>(DockZone::Center)] = Rect::from_xywh(center_x, mid_y, center_w, mid_h);
            }
            if (has_right) {
                float rx = ox + iw - right_w;
                m_zone_rects[static_cast<size_t>(DockZone::Right)] = Rect::from_xywh(rx, mid_y, right_w, mid_h);
                if (has_center) {
                    float sx = rx - kHairline;
                    m_splitters.push_back(Splitter{SplitterKind::Right,
                        Rect::from_xywh(sx - kSplitterHit * 0.5f, mid_y, kSplitterHit, mid_h),
                        Rect::from_xywh(sx, mid_y, kHairline, mid_h)});
                }
            }
        }

        for (auto& pane : m_panes) {
            if (pane.closed && pane.content_box && pane.content_box->parent() == this) {
                pane.content_box->layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}), true);
                pane.content_box->set_offset(Vec2{-10000.0f, -10000.0f});
            }
        }

        for (size_t z = 0; z < 5; ++z) {
            DockZone dz = static_cast<DockZone>(z);
            auto list = panes_in_zone(dz);
            if (list.empty()) continue;
            if (m_active_tab[z] >= list.size()) m_active_tab[z] = 0;

            Rect zr = m_zone_rects[z];
            float content_h = std::max(0.0f, zr.height() - kHeaderHeight);
            float tab_x = zr.min_x + 4.0f;
            float right_reserve = can_detach() ? 28.0f : 4.0f;
            float max_bar_x = zr.max_x - right_reserve;

            for (size_t k = 0; k < list.size(); ++k) {
                auto& pane = m_panes[list[k]];
                float label_w = TextMeasure::measure(pane.title, 12.0f, FontWeight::SemiBold).x;
                float ideal_w = 12.0f + label_w + (pane.closable ? 24.0f : 12.0f);
                float tab_w = std::min(ideal_w, std::max(56.0f, max_bar_x - tab_x));
                if (tab_x + tab_w > max_bar_x) tab_w = std::max(40.0f, max_bar_x - tab_x);

                Rect tr = Rect::from_xywh(tab_x, zr.min_y, tab_w, kHeaderHeight);
                Rect close_r{};
                if (pane.closable && tab_w >= 52.0f) {
                    close_r = Rect::from_xywh(tr.max_x - 20.0f, tr.min_y + (kHeaderHeight - 18.0f) * 0.5f, 18.0f, 18.0f);
                }
                m_tab_hits.push_back(TabHit{dz, k, list[k], tr, close_r});
                tab_x += tab_w + 2.0f;

                if (pane.content_box && pane.content_box->parent() == this) {
                    if (k == m_active_tab[z]) {
                        pane.content_box->layout(BoxConstraints::tight(Vec2{zr.width(), content_h}), true);
                        pane.content_box->set_offset(Vec2{zr.min_x, zr.min_y + kHeaderHeight});
                    } else {
                        pane.content_box->layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}), true);
                        pane.content_box->set_offset(Vec2{-10000.0f, -10000.0f});
                    }
                }
            }
        }
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (m_dragging_pane.has_value() || m_dragging_splitter.has_value()) {
            result.add(this, position);
            return true;
        }
        if (!bounds().contains(position)) return false;

        for (const auto& sp : m_splitters) {
            if (sp.hit_rect.contains(position)) {
                cursor_shape = (sp.kind == SplitterKind::Left || sp.kind == SplitterKind::Right)
                    ? CursorShape::ResizeEW : CursorShape::ResizeNS;
                result.add(this, position);
                return true;
            }
        }

        for (size_t z = 0; z < 5; ++z) {
            Rect zr = m_zone_rects[z];
            if (!zr.is_empty() && Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight).contains(position)) {
                cursor_shape = CursorShape::PointingHand;
                result.add(this, position);
                return true;
            }
        }

        cursor_shape = CursorShape::Arrow;
        for (size_t z = 0; z < 5; ++z) {
            auto list = panes_in_zone(static_cast<DockZone>(z));
            if (list.empty()) continue;
            size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
            auto& pane = m_panes[list[active_idx]];
            if (pane.content_box && pane.content_box->parent() == this) {
                PointF child_pos{position.x - pane.content_box->offset().x, position.y - pane.content_box->offset().y};
                if (pane.content_box->hit_test(result, child_pos)) {
                    result.add(this, position);
                    return true;
                }
            }
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        advance_animations();

        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;

        const Color outer_bg     = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.025f} : Color{0.0f, 0.0f, 0.0f, 0.025f};
        const Color outer_border = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.09f}  : Color{0.0f, 0.0f, 0.0f, 0.10f};
        const Color center_bg    = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.042f} : Color{1.0f, 1.0f, 1.0f, 0.88f};
        const Color tool_bg      = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.018f} : Color{0.96f, 0.97f, 0.98f, 0.80f};
        const Color bar_bg       = is_dark ? Color{0.0f, 0.0f, 0.0f, 0.14f}  : Color{0.0f, 0.0f, 0.0f, 0.03f};
        const Color hairline     = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.075f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
        const Color text_pri     = t.colors.text_primary;
        const Color text_muted   = t.colors.text_secondary;
        const Color accent       = t.colors.primary;

        Rect outer = Rect::from_xywh(m_padding.left, m_padding.top,
                                     std::max(0.0f, m_size.x - m_padding.horizontal()),
                                     std::max(0.0f, m_size.y - m_padding.vertical()));
        context.draw_rect(outer, outer_bg, 8.0f, 0.0f);
        context.push_clip(outer, 8.0f);

        for (size_t z = 0; z < 5; ++z) {
            DockZone dz = static_cast<DockZone>(z);
            auto list = panes_in_zone(dz);
            if (list.empty()) continue;
            Rect target_zr = m_zone_rects[z];
            if (target_zr.is_empty()) continue;
            Rect zr = m_anim_zone_init[z] ? m_anim_zone_rects[z] : target_zr;
            const float dx = zr.min_x - target_zr.min_x;
            const float dy = zr.min_y - target_zr.min_y;

            context.push_clip(zr, 0.0f);

            const Color pane_bg = (dz == DockZone::Center) ? center_bg : tool_bg;
            context.draw_rect(zr, pane_bg, 0.0f, 0.0f);

            Rect header_r = Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight);
            context.draw_rect(header_r, bar_bg, 0.0f, 0.0f);
            context.draw_rect(Rect::from_xywh(zr.min_x, zr.min_y + kHeaderHeight - 1.0f, zr.width(), 1.0f), hairline, 0.0f, 0.0f);

            for (const auto& th : m_tab_hits) {
                if (th.zone != dz) continue;
                const bool is_active = (th.tab_index == m_active_tab[z]);
                const bool is_hovered = (m_hovered_pane_index == th.pane_index);
                const auto& pane = m_panes[th.pane_index];
                Rect tr = Rect::from_xywh(th.rect.min_x + dx, th.rect.min_y + dy, th.rect.width(), th.rect.height());

                if (is_active) {
                    context.draw_rect(Rect::from_xywh(tr.min_x, tr.min_y + 2.0f, tr.width(), tr.height() - 2.0f),
                                      pane_bg, 4.0f, 0.0f);
                } else if (is_hovered) {
                    Color hov = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.045f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
                    context.draw_rect(Rect::from_xywh(tr.min_x, tr.min_y + 3.0f, tr.width(), tr.height() - 6.0f),
                                      hov, 4.0f, 0.0f);
                }

                const FontWeight fw = is_active ? FontWeight::SemiBold : FontWeight::Regular;
                const float text_x = tr.min_x + 10.0f;
                const bool show_close = pane.closable && !th.close_rect.is_empty() && (is_active || is_hovered);
                Rect cr = show_close
                    ? Rect::from_xywh(th.close_rect.min_x + dx, th.close_rect.min_y + dy, th.close_rect.width(), th.close_rect.height())
                    : Rect{};
                const float text_max_w = std::max(0.0f, (show_close ? cr.min_x - 2.0f : tr.max_x - 6.0f) - text_x);
                if (text_max_w > 4.0f) {
                    Vec2 ls = TextMeasure::measure(pane.title, 12.0f, fw);
                    float text_y = std::round(tr.min_y + (tr.height() - ls.y) * 0.5f);
                    context.push_clip(Rect::from_xywh(text_x, tr.min_y, text_max_w, tr.height()), 0.0f);
                    context.draw_text(pane.title, PointF{std::round(text_x), text_y}, 12.0f,
                                      is_active ? text_pri : text_muted, fw);
                    context.pop_clip();
                }

                if (show_close) {
                    Vec2 xs = TextMeasure::measure("×", 13.0f);
                    context.draw_text("×", PointF{std::round(cr.min_x + (cr.width() - xs.x) * 0.5f),
                                                  std::round(cr.min_y + (cr.height() - xs.y) * 0.5f - 0.5f)},
                                      13.0f, text_muted);
                }
            }

            // 平滑滑动的 Fluent 2 激活标签指示条
            if (m_tab_ind_init[z] && m_tab_ind_w[z] > 2.0f) {
                context.draw_rect(Rect::from_xywh(m_tab_ind_x[z], zr.min_y + kHeaderHeight - 2.0f,
                                                  m_tab_ind_w[z], 2.0f),
                                  accent, 1.0f, 0.0f);
            }

            if (can_detach()) {
                Rect dr = detach_btn_rect(zr);
                const bool d_hov = (m_hovered_detach_zone == dz);
                if (d_hov) {
                    Color hov = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.07f};
                    context.draw_rect(dr, hov, 4.0f, 0.0f);
                }
                Vec2 isz = TextMeasure::measure("↗", 12.0f);
                context.draw_text("↗", PointF{std::round(dr.min_x + (dr.width() - isz.x) * 0.5f),
                                              std::round(dr.min_y + (dr.height() - isz.y) * 0.5f)},
                                  12.0f, d_hov ? accent : Color{text_muted.r, text_muted.g, text_muted.b, 0.65f});
            }

            size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
            auto& active_pane = m_panes[list[active_idx]];
            if (active_pane.content_box && active_pane.content_box->parent() == this) {
                Rect content_clip = Rect::from_xywh(zr.min_x, zr.min_y + kHeaderHeight,
                                                    zr.width(), std::max(0.0f, zr.height() - kHeaderHeight));
                const float ease = 1.0f - std::pow(1.0f - std::clamp(m_tab_anim_t[z], 0.0f, 1.0f), 3.0f);
                const float slide_y = (1.0f - ease) * 10.0f;
                context.push_clip(content_clip, 0.0f);
                {
                    Vec2 off = active_pane.content_box->offset();
                    PaintContext::OffsetScope sc(context, Vec2{off.x + dx, off.y + dy + slide_y});
                    active_pane.content_box->paint(context);
                }
                context.pop_clip();
            }

            context.pop_clip();
        }

        // 直接复用 SplitView 的 Fluent 2 分隔条（悬停/拖拽胶囊把手 + 主题色轨道高亮）
        for (const auto& sp : m_splitters) {
            const bool is_vertical = (sp.kind == SplitterKind::Top || sp.kind == SplitterKind::Bottom);
            const bool is_hovered  = (m_hovered_splitter == sp.kind);
            const bool is_dragging = (m_dragging_splitter == sp.kind);
            RenderSplitView::paint_splitter(context, sp.hit_rect, is_vertical, is_hovered, is_dragging, true);
        }

        const bool show_drop_overlay = (m_dragging_pane.has_value() && m_drag_started) || m_external_drag_active;
        if (show_drop_overlay) {
            if (m_drop_target.has_value()) {
                Rect preview = compute_drop_preview(*m_drop_target);
                context.draw_rect(preview, Color{accent.r, accent.g, accent.b, 0.18f}, 6.0f, 0.0f);
                context.draw_rect(preview, Color{accent.r, accent.g, accent.b, 0.85f}, 6.0f, 1.5f);
            }
            if (m_external_drag_active || !m_will_detach_on_drop) {
                paint_docking_compass(context, is_dark, accent, text_pri);
            }
        }

        context.pop_clip();
        context.draw_rect(outer, outer_border, 8.0f, 1.0f);

        if (m_dragging_pane.has_value() && m_drag_started) {
            const auto& dp = m_panes[*m_dragging_pane];
            std::string label = m_will_detach_on_drop
                ? (dp.title + "  ↗ 释放弹出独立窗口")
                : dp.title;
            Vec2 gs = TextMeasure::measure(label, 12.0f, FontWeight::SemiBold);
            float gw = std::max(96.0f, gs.x + 24.0f);
            Rect ghost = Rect::from_xywh(m_drag_pos.x - gw * 0.5f, m_drag_pos.y - 15.0f, gw, 30.0f);
            Color ghost_bg = is_dark ? Color{0.14f, 0.16f, 0.20f, 0.95f} : Color{1.0f, 1.0f, 1.0f, 0.96f};
            context.draw_rect(ghost, ghost_bg, 6.0f, 0.0f);
            context.draw_rect(ghost, accent, 6.0f, 1.5f);
            context.draw_text(label, PointF{std::round(ghost.min_x + (gw - gs.x) * 0.5f),
                                            std::round(ghost.min_y + (30.0f - gs.y) * 0.5f)},
                              12.0f, text_pri, FontWeight::SemiBold);
        }
    }

private:
    enum class SplitterKind : uint8_t { Left, Right, Top, Bottom };
    struct Splitter { SplitterKind kind; Rect hit_rect; Rect visual_rect; };
    struct TabHit {
        DockZone zone;
        size_t tab_index;
        size_t pane_index;
        Rect rect;
        Rect close_rect;
    };
    struct CompassButton { DockZone zone; Rect rect; };

    [[nodiscard]] bool can_detach() const noexcept {
        return m_on_detach_pane != nullptr || (m_allow_detach_window && Application::current() != nullptr);
    }

    [[nodiscard]] static Rect detach_btn_rect(Rect zr) noexcept {
        return Rect::from_xywh(zr.max_x - 24.0f, zr.min_y + (kHeaderHeight - 20.0f) * 0.5f, 20.0f, 20.0f);
    }

    [[nodiscard]] std::vector<size_t> panes_in_zone(DockZone z) const {
        std::vector<size_t> out;
        for (size_t i = 0; i < m_panes.size(); ++i) {
            if (!m_panes[i].closed && !m_panes[i].detached && m_panes[i].zone == z) out.push_back(i);
        }
        return out;
    }

    [[nodiscard]] std::array<CompassButton, 5> compute_compass() const noexcept {
        const float cx = m_size.x * 0.5f;
        const float cy = m_size.y * 0.5f;
        const float s = 30.0f;
        const float g = 5.0f;
        return {{
            {DockZone::Center, Rect::from_xywh(cx - s * 0.5f, cy - s * 0.5f, s, s)},
            {DockZone::Top,    Rect::from_xywh(cx - s * 0.5f, cy - s * 1.5f - g, s, s)},
            {DockZone::Bottom, Rect::from_xywh(cx - s * 0.5f, cy + s * 0.5f + g, s, s)},
            {DockZone::Left,   Rect::from_xywh(cx - s * 1.5f - g, cy - s * 0.5f, s, s)},
            {DockZone::Right,  Rect::from_xywh(cx + s * 0.5f + g, cy - s * 0.5f, s, s)}
        }};
    }

    [[nodiscard]] std::optional<DockZone> hit_test_drop_target(PointF p, bool& out_detach) const noexcept {
        out_detach = false;
        if (can_detach() && !bounds().contains(p)) {
            out_detach = true;
            return std::nullopt;
        }
        if (!bounds().contains(p)) return std::nullopt;

        for (const auto& cb : compute_compass()) {
            if (cb.rect.contains(p)) return cb.zone;
        }

        for (size_t z = 0; z < 5; ++z) {
            Rect zr = m_zone_rects[z];
            if (!zr.is_empty() && Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight).contains(p)) {
                return static_cast<DockZone>(z);
            }
        }

        const float nx = p.x / std::max(1.0f, m_size.x);
        const float ny = p.y / std::max(1.0f, m_size.y);
        const float dl = nx, dr = 1.0f - nx, dt = ny, db = 1.0f - ny;
        const float min_edge = std::min({dl, dr, dt, db});
        if (min_edge <= 0.26f) {
            if (min_edge == dl) return DockZone::Left;
            if (min_edge == dr) return DockZone::Right;
            if (min_edge == dt) return DockZone::Top;
            return DockZone::Bottom;
        }
        return DockZone::Center;
    }

    [[nodiscard]] Rect compute_drop_preview(DockZone z) const noexcept {
        const float ox = m_padding.left + 4.0f;
        const float oy = m_padding.top + 4.0f;
        const float iw = std::max(0.0f, m_size.x - m_padding.horizontal() - 8.0f);
        const float ih = std::max(0.0f, m_size.y - m_padding.vertical() - 8.0f);
        switch (z) {
            case DockZone::Left:   return Rect::from_xywh(ox, oy, std::min(m_left_w, iw * 0.36f), ih);
            case DockZone::Right: {
                float rw = std::min(m_right_w, iw * 0.36f);
                return Rect::from_xywh(ox + iw - rw, oy, rw, ih);
            }
            case DockZone::Top:    return Rect::from_xywh(ox, oy, iw, std::min(m_top_h, ih * 0.34f));
            case DockZone::Bottom: {
                float bh = std::min(m_bottom_h, ih * 0.34f);
                return Rect::from_xywh(ox, oy + ih - bh, iw, bh);
            }
            case DockZone::Center: {
                Rect cr = m_zone_rects[static_cast<size_t>(DockZone::Center)];
                if (!cr.is_empty()) return cr.inset(3.0f);
                return Rect::from_xywh(ox + iw * 0.2f, oy + ih * 0.2f, iw * 0.6f, ih * 0.6f);
            }
        }
        return Rect{};
    }

    void paint_docking_compass(PaintContext& context, bool is_dark, Color accent, Color text_pri) const {
        const float cx = m_size.x * 0.5f;
        const float cy = m_size.y * 0.5f;
        Rect backplate = Rect::from_xywh(cx - 56.0f, cy - 56.0f, 112.0f, 112.0f);
        Color plate_bg = is_dark ? Color{0.10f, 0.11f, 0.14f, 0.82f} : Color{0.98f, 0.99f, 1.0f, 0.88f};
        Color plate_bd = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.12f} : Color{0.0f, 0.0f, 0.0f, 0.10f};
        context.draw_rect(backplate, plate_bg, 10.0f, 0.0f);
        context.draw_rect(backplate, plate_bd, 10.0f, 1.0f);

        for (const auto& cb : compute_compass()) {
            const bool hot = (m_drop_target == cb.zone);
            Color btn_bg = hot
                ? Color{accent.r, accent.g, accent.b, 0.92f}
                : (is_dark ? Color{1.0f, 1.0f, 1.0f, 0.07f} : Color{0.0f, 0.0f, 0.0f, 0.05f});
            context.draw_rect(cb.rect, btn_bg, 5.0f, 0.0f);

            Rect icon_box = Rect::from_xywh(cb.rect.min_x + 6.0f, cb.rect.min_y + 6.0f, 18.0f, 18.0f);
            Color wire = hot ? Color::white() : Color{text_pri.r, text_pri.g, text_pri.b, 0.45f};
            Color fill = hot ? Color::white() : accent;
            context.draw_rect(icon_box, wire, 2.0f, 1.0f);
            switch (cb.zone) {
                case DockZone::Left:   context.draw_rect(Rect::from_xywh(icon_box.min_x + 2.0f, icon_box.min_y + 2.0f, 5.0f, 14.0f), fill, 1.0f, 0.0f); break;
                case DockZone::Right:  context.draw_rect(Rect::from_xywh(icon_box.max_x - 7.0f, icon_box.min_y + 2.0f, 5.0f, 14.0f), fill, 1.0f, 0.0f); break;
                case DockZone::Top:    context.draw_rect(Rect::from_xywh(icon_box.min_x + 2.0f, icon_box.min_y + 2.0f, 14.0f, 5.0f), fill, 1.0f, 0.0f); break;
                case DockZone::Bottom: context.draw_rect(Rect::from_xywh(icon_box.min_x + 2.0f, icon_box.max_y - 7.0f, 14.0f, 5.0f), fill, 1.0f, 0.0f); break;
                case DockZone::Center: context.draw_rect(Rect::from_xywh(icon_box.min_x + 4.0f, icon_box.min_y + 4.0f, 10.0f, 10.0f), fill, 1.5f, 0.0f); break;
            }
        }
    }

    void advance_animations() {
        auto now = std::chrono::steady_clock::now();
        float dt = (m_anim_last_time.time_since_epoch().count() == 0)
            ? 0.016f
            : std::clamp(std::chrono::duration<float>(now - m_anim_last_time).count(), 0.001f, 0.05f);
        m_anim_last_time = now;

        const bool snap = m_dragging_splitter.has_value();
        const float k_zone = 1.0f - std::exp(-dt * 16.0f);
        const float k_ind  = 1.0f - std::exp(-dt * 22.0f);
        bool any_anim = false;

        for (size_t z = 0; z < 5; ++z) {
            const Rect& target = m_zone_rects[z];
            if (target.is_empty()) {
                m_anim_zone_init[z] = false;
                m_tab_ind_init[z] = false;
                continue;
            }
            if (!m_anim_zone_init[z] || snap) {
                m_anim_zone_rects[z] = target;
                m_anim_zone_init[z] = true;
            } else {
                Rect& cur = m_anim_zone_rects[z];
                cur.min_x += (target.min_x - cur.min_x) * k_zone;
                cur.min_y += (target.min_y - cur.min_y) * k_zone;
                cur.max_x += (target.max_x - cur.max_x) * k_zone;
                cur.max_y += (target.max_y - cur.max_y) * k_zone;
                float diff = std::max({std::abs(target.min_x - cur.min_x), std::abs(target.min_y - cur.min_y),
                                       std::abs(target.max_x - cur.max_x), std::abs(target.max_y - cur.max_y)});
                if (diff > 0.35f) any_anim = true;
                else cur = target;
            }

            if (m_tab_anim_t[z] < 1.0f) {
                m_tab_anim_t[z] = std::min(1.0f, m_tab_anim_t[z] + dt / 0.18f);
                if (m_tab_anim_t[z] < 1.0f) any_anim = true;
            }

            // 平滑滑动激活标签指示条
            const float dx = m_anim_zone_rects[z].min_x - target.min_x;
            for (const auto& th : m_tab_hits) {
                if (static_cast<size_t>(th.zone) == z && th.tab_index == m_active_tab[z]) {
                    const float tx = th.rect.min_x + dx + 8.0f;
                    const float tw = std::max(10.0f, th.rect.width() - 16.0f);
                    if (!m_tab_ind_init[z] || snap) {
                        m_tab_ind_x[z] = tx;
                        m_tab_ind_w[z] = tw;
                        m_tab_ind_init[z] = true;
                    } else {
                        m_tab_ind_x[z] += (tx - m_tab_ind_x[z]) * k_ind;
                        m_tab_ind_w[z] += (tw - m_tab_ind_w[z]) * k_ind;
                        if (std::abs(tx - m_tab_ind_x[z]) > 0.35f || std::abs(tw - m_tab_ind_w[z]) > 0.35f) {
                            any_anim = true;
                        } else {
                            m_tab_ind_x[z] = tx;
                            m_tab_ind_w[z] = tw;
                        }
                    }
                    break;
                }
            }
        }

        if (any_anim) {
            mark_needs_paint();
        }
    }

    void update_external_drag(PointF local_pos, Window* parent_win) {
        const bool inside = bounds().contains(local_pos);
        bool next_active = inside;
        std::optional<DockZone> next_target;
        if (inside) {
            bool dummy_detach = false;
            next_target = hit_test_drop_target(local_pos, dummy_detach);
        }
        if (m_external_drag_active != next_active || m_drop_target != next_target) {
            m_external_drag_active = next_active;
            m_drop_target = next_target;
            mark_needs_paint();
            if (parent_win) {
                parent_win->request_redraw();
                if (parent_win->platform_window()) parent_win->platform_window()->invalidate();
            }
        }
    }

    std::optional<DockZone> finish_external_drag(PointF local_pos, Window* parent_win) {
        std::optional<DockZone> target;
        if (bounds().contains(local_pos)) {
            bool dummy_detach = false;
            target = hit_test_drop_target(local_pos, dummy_detach);
        }
        if (m_external_drag_active || m_drop_target.has_value()) {
            m_external_drag_active = false;
            m_drop_target.reset();
            mark_needs_paint();
            if (parent_win) {
                parent_win->request_redraw();
                if (parent_win->platform_window()) parent_win->platform_window()->invalidate();
            }
        }
        return target;
    }

    void open_pane_context_menu(size_t pane_index, PointF event_window_pos) {
        if (pane_index >= m_panes.size()) return;
        const auto& pane = m_panes[pane_index];
        std::string pid = pane.id;
        DockZone cur_zone = pane.zone;
        bool closable = pane.closable;
        std::weak_ptr<bool> alive = m_alive;

        std::vector<MenuFlyoutItem> items;
        if (can_detach()) {
            items.push_back(MenuFlyoutItem{
                .label = "浮动为独立窗口",
                .on_click = [this, alive, pid]() {
                    if (auto g = alive.lock(); g && *g) detach_pane(pid);
                }
            });
            items.push_back(MenuFlyoutItem::separator());
        }

        auto add_zone = [&](const char* label, DockZone z) {
            items.push_back(MenuFlyoutItem{
                .label = label,
                .is_checkable = true,
                .checked = (cur_zone == z),
                .on_click = [this, alive, pid, z]() {
                    if (auto g = alive.lock(); g && *g) dock_pane(pid, z);
                }
            });
        };
        add_zone("停靠到左侧 (Left)",   DockZone::Left);
        add_zone("停靠到右侧 (Right)",  DockZone::Right);
        add_zone("停靠到顶部 (Top)",    DockZone::Top);
        add_zone("停靠到底部 (Bottom)", DockZone::Bottom);
        add_zone("停靠到中心 (Center)", DockZone::Center);

        if (closable) {
            items.push_back(MenuFlyoutItem::separator());
            items.push_back(MenuFlyoutItem{
                .label = "关闭面板",
                .on_click = [this, alive, pid]() {
                    if (auto g = alive.lock(); !g || !*g) return;
                    for (auto& lp : m_panes) {
                        if (lp.id == pid) {
                            if (lp.content_box) detail::forget_render_subtree(lp.content_box.get());
                            lp.closed = true;
                            mark_needs_layout();
                            mark_needs_paint();
                            break;
                        }
                    }
                }
            });
        }

        Vec2 win_pos = event_window_pos;
        if (Window* w = Window::active_window()) win_pos.y += w->titlebar_height();
        MenuFlyout::show_at(std::move(items), win_pos);
    }

    void setup_behavior() {
        on_pointer_exit = [this](PointerEvent&) {
            if (!m_dragging_pane && !m_dragging_splitter) {
                m_hovered_splitter.reset();
                m_hovered_detach_zone.reset();
                m_hovered_pane_index = SIZE_MAX;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            PointF p = ev.local_position;

            // 右键点击停靠面板标签页或标题栏：弹出 Visual Studio 风格停靠上下文菜单
            if (ev.button == PointerButton::Secondary) {
                for (const auto& th : m_tab_hits) {
                    if (th.rect.contains(p)) {
                        const size_t z_idx = static_cast<size_t>(th.zone);
                        if (m_active_tab[z_idx] != th.tab_index) {
                            m_active_tab[z_idx] = th.tab_index;
                            m_tab_anim_t[z_idx] = 0.0f;
                            mark_needs_layout();
                            mark_needs_paint();
                        }
                        open_pane_context_menu(th.pane_index, ev.position);
                        ev.stop_propagation();
                        return;
                    }
                }
                for (size_t z = 0; z < 5; ++z) {
                    Rect zr = m_zone_rects[z];
                    if (zr.is_empty()) continue;
                    if (Rect::from_xywh(zr.min_x, zr.min_y, zr.width(), kHeaderHeight).contains(p)) {
                        auto list = panes_in_zone(static_cast<DockZone>(z));
                        if (!list.empty()) {
                            size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
                            open_pane_context_menu(list[active_idx], ev.position);
                            ev.stop_propagation();
                            return;
                        }
                    }
                }
                return;
            }

            if (ev.button != PointerButton::Primary) return;

            for (const auto& sp : m_splitters) {
                if (sp.hit_rect.contains(p)) {
                    m_dragging_splitter = sp.kind;
                    mark_needs_paint();
                    ev.stop_propagation();
                    return;
                }
            }

            if (can_detach()) {
                for (size_t z = 0; z < 5; ++z) {
                    Rect zr = m_zone_rects[z];
                    if (zr.is_empty()) continue;
                    if (detach_btn_rect(zr).contains(p)) {
                        auto list = panes_in_zone(static_cast<DockZone>(z));
                        if (!list.empty()) {
                            size_t active_idx = std::min(m_active_tab[z], list.size() - 1);
                            detach_pane(m_panes[list[active_idx]].id);
                        }
                        ev.stop_propagation();
                        return;
                    }
                }
            }

            for (const auto& th : m_tab_hits) {
                if (th.rect.contains(p)) {
                    auto& pane = m_panes[th.pane_index];
                    if (pane.closable && !th.close_rect.is_empty() && th.close_rect.contains(p)) {
                        if (pane.content_box) detail::forget_render_subtree(pane.content_box.get());
                        pane.closed = true;
                        mark_needs_layout();
                        mark_needs_paint();
                        ev.stop_propagation();
                        return;
                    }
                    const size_t z_idx = static_cast<size_t>(th.zone);
                    if (m_active_tab[z_idx] != th.tab_index) {
                        auto list = panes_in_zone(th.zone);
                        if (m_active_tab[z_idx] < list.size()) {
                            auto& prev_pane = m_panes[list[m_active_tab[z_idx]]];
                            if (prev_pane.content_box) detail::forget_render_subtree(prev_pane.content_box.get());
                        }
                        m_active_tab[z_idx] = th.tab_index;
                        m_tab_anim_t[z_idx] = 0.0f;
                    }
                    m_dragging_pane = th.pane_index;
                    m_drag_origin = p;
                    m_drag_pos = p;
                    m_drag_started = false;
                    m_will_detach_on_drop = false;
                    m_drop_target.reset();
                    mark_needs_layout();
                    mark_needs_paint();
                    ev.stop_propagation();
                    return;
                }
            }
        };

        on_pointer_move = [this](PointerEvent& ev) {
            PointF p = ev.local_position;

            if (m_dragging_splitter.has_value()) {
                const float iw = std::max(1.0f, m_size.x - m_padding.horizontal());
                const float ih = std::max(1.0f, m_size.y - m_padding.vertical());
                switch (*m_dragging_splitter) {
                    case SplitterKind::Left:
                        m_left_w = std::clamp(p.x - m_padding.left, kMinPaneSize, std::max(kMinPaneSize, iw - kMinPaneSize));
                        break;
                    case SplitterKind::Right:
                        m_right_w = std::clamp(m_size.x - m_padding.right - p.x, kMinPaneSize, std::max(kMinPaneSize, iw - kMinPaneSize));
                        break;
                    case SplitterKind::Top:
                        m_top_h = std::clamp(p.y - m_padding.top, kMinPaneSize, std::max(kMinPaneSize, ih - kMinPaneSize));
                        break;
                    case SplitterKind::Bottom:
                        m_bottom_h = std::clamp(m_size.y - m_padding.bottom - p.y, kMinPaneSize, std::max(kMinPaneSize, ih - kMinPaneSize));
                        break;
                }
                mark_needs_layout();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }

            if (m_dragging_pane.has_value()) {
                m_drag_pos = p;
                if (!m_drag_started && std::hypot(p.x - m_drag_origin.x, p.y - m_drag_origin.y) >= 8.0f) {
                    m_drag_started = true;
                }
                if (m_drag_started) {
                    m_drop_target = hit_test_drop_target(p, m_will_detach_on_drop);
                    mark_needs_paint();
                }
                ev.stop_propagation();
                return;
            }

            std::optional<SplitterKind> next_sp;
            for (const auto& sp : m_splitters) {
                if (sp.hit_rect.contains(p)) { next_sp = sp.kind; break; }
            }
            size_t next_pane = SIZE_MAX;
            for (const auto& th : m_tab_hits) {
                if (th.rect.contains(p)) { next_pane = th.pane_index; break; }
            }
            std::optional<DockZone> next_detach;
            if (can_detach()) {
                for (size_t z = 0; z < 5; ++z) {
                    if (!m_zone_rects[z].is_empty() && detach_btn_rect(m_zone_rects[z]).contains(p)) {
                        next_detach = static_cast<DockZone>(z);
                        break;
                    }
                }
            }
            if (next_sp != m_hovered_splitter || next_pane != m_hovered_pane_index || next_detach != m_hovered_detach_zone) {
                m_hovered_splitter = next_sp;
                m_hovered_pane_index = next_pane;
                m_hovered_detach_zone = next_detach;
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (m_dragging_splitter.has_value()) {
                m_dragging_splitter.reset();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (m_dragging_pane.has_value()) {
                size_t pane_idx = *m_dragging_pane;
                auto target = m_drop_target;
                bool should_detach = m_will_detach_on_drop;
                bool was_dragging = m_drag_started;
                m_dragging_pane.reset();
                m_drop_target.reset();
                m_drag_started = false;
                m_will_detach_on_drop = false;
                if (was_dragging && pane_idx < m_panes.size()) {
                    if (should_detach) {
                        detach_pane(m_panes[pane_idx].id);
                    } else if (target.has_value()) {
                        dock_pane(m_panes[pane_idx].id, *target);
                    } else {
                        mark_needs_paint();
                    }
                } else {
                    mark_needs_paint();
                }
                ev.stop_propagation();
            }
        };
    }

    float m_left_w{200.0f}, m_right_w{200.0f}, m_top_h{110.0f}, m_bottom_h{120.0f};
    float m_init_left_w{200.0f}, m_init_right_w{200.0f}, m_init_top_h{110.0f}, m_init_bottom_h{120.0f};
    EdgeInsets m_padding{EdgeInsets::zero()};
    bool m_allow_detach_window{true};
    Fn<void(const std::string&, DockZone)> m_on_dock_changed{nullptr};
    Fn<void(const std::string&, bool)> m_on_detach_changed{nullptr};
    DetachCallback m_on_detach_pane{nullptr};

    std::vector<LivePane> m_panes;
    std::array<Rect, 5> m_zone_rects{};
    std::array<size_t, 5> m_active_tab{};
    std::vector<Splitter> m_splitters;
    std::vector<TabHit> m_tab_hits;

    std::optional<SplitterKind> m_hovered_splitter;
    std::optional<SplitterKind> m_dragging_splitter;
    std::optional<DockZone> m_hovered_detach_zone;
    size_t m_hovered_pane_index{SIZE_MAX};
    std::optional<size_t> m_dragging_pane;
    bool m_drag_started{false};
    bool m_will_detach_on_drop{false};
    bool m_external_drag_active{false};
    PointF m_drag_origin{};
    PointF m_drag_pos{};
    std::optional<DockZone> m_drop_target;

    std::chrono::steady_clock::time_point m_anim_last_time{};
    std::array<Rect, 5> m_anim_zone_rects{};
    std::array<bool, 5> m_anim_zone_init{};
    std::array<float, 5> m_tab_ind_x{};
    std::array<float, 5> m_tab_ind_w{};
    std::array<bool, 5> m_tab_ind_init{};
    std::array<float, 5> m_tab_anim_t{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
};

struct DockPanel : BindableMixin<DockPanel, RenderDockPanel> {
    std::vector<DockPane> panes{};
    float left_width{200.0f};
    float right_width{200.0f};
    float top_height{110.0f};
    float bottom_height{120.0f};
    EdgeInsets padding{EdgeInsets::zero()};
    bool allow_detach_window{true};
    Fn<void(const std::string&, DockZone)> on_dock_changed{nullptr};
    Fn<void(const std::string&, bool)> on_detach_changed{nullptr};
    RenderDockPanel::DetachCallback on_detach_pane{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class DockPanelWidget : public Widget {
public:
    explicit DockPanelWidget(const DockPanel& desc) : m_desc(desc) {}
    explicit DockPanelWidget(DockPanel&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderDockPanel>(
            m_desc.left_width, m_desc.right_width, m_desc.top_height, m_desc.bottom_height,
            m_desc.padding, m_desc.allow_detach_window,
            m_desc.on_dock_changed, m_desc.on_detach_changed, m_desc.on_detach_pane);
        std::vector<RenderDockPanel::LivePane> live;
        live.reserve(m_desc.panes.size());
        for (auto& p : m_desc.panes) {
            std::shared_ptr<RenderBox> box = p.content ? p.content->create_render_node() : nullptr;
            if (p.content && box) p.content->set_render_node(box);
            live.push_back(RenderDockPanel::LivePane{
                p.id, p.title, p.zone, p.zone, p.closable, false, false, std::move(box), {}, {}
            });
        }
        node->set_panes(std::move(live));
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto dp = std::static_pointer_cast<RenderDockPanel>(node)) {
            dp->set_padding(m_desc.padding);
            dp->set_allow_detach_window(m_desc.allow_detach_window);
            dp->set_on_dock_changed(m_desc.on_dock_changed);
            dp->set_on_detach_changed(m_desc.on_detach_changed);
            dp->set_on_detach_pane(m_desc.on_detach_pane);
        }
    }

private:
    DockPanel m_desc;
};

inline DockPanel::operator WidgetPtr() const& { return make_widget<DockPanelWidget>(*this); }
inline DockPanel::operator WidgetPtr() && { return make_widget<DockPanelWidget>(std::move(*this)); }

} // namespace ktui
