#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <ktui/app/window_titlebar.hpp>
#include <ktui/app/ui_inspector.hpp>
#include <ktui/widgets/widget.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/event/event_dispatcher.hpp>
#include <ktui/event/focus_tree.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/presentation/mica_backdrop.hpp>
#include <ktui/theme/theme.hpp>
#include <kernetile/renderer.hpp>
#include <ktui/core/fn.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

class Application;
class PaintContext;

// 逻辑应用窗口抽象 (Logical Application Window)
// 声明与实现分离至 KTUI/src/app/window.cpp，按子模块划分输入路由、标题栏、浮层与帧渲染职责
class Window {
public:
    Window(uint32_t id, std::unique_ptr<IPlatformWindow> platform_window, bool show_titlebar = true);
    virtual ~Window();

    [[nodiscard]] uint32_t id() const noexcept { return m_id; }
    [[nodiscard]] IPlatformWindow* platform_window() const noexcept { return m_platform_window.get(); }

    // ==================== 几何与尺寸 ====================
    [[nodiscard]] Vec2 size() const { return m_platform_window ? m_platform_window->size() : Vec2{0.0f, 0.0f}; }
    void set_size(Vec2 sz) { if (m_platform_window) m_platform_window->set_size(sz); }
    [[nodiscard]] Vec2 position() const { return m_platform_window ? m_platform_window->position() : Vec2{0.0f, 0.0f}; }
    void set_position(Vec2 pos) { if (m_platform_window) m_platform_window->set_position(pos); }
    void center_on_screen() { if (m_platform_window) m_platform_window->center_on_screen(); }
    [[nodiscard]] Rect screen_bounds() const { return m_platform_window ? m_platform_window->screen_bounds() : Rect{}; }

    // ==================== 呈现状态 ====================
    void show() { if (m_platform_window) m_platform_window->show(); }
    void hide() { if (m_platform_window) m_platform_window->hide(); }
    [[nodiscard]] bool is_visible() const { return m_platform_window ? m_platform_window->is_visible() : false; }
    void minimize() { if (m_platform_window) m_platform_window->minimize(); }
    void maximize() { if (m_platform_window) m_platform_window->maximize(); }
    void restore() { if (m_platform_window) m_platform_window->restore(); }
    [[nodiscard]] bool is_minimized() const { return m_platform_window ? m_platform_window->is_minimized() : false; }
    [[nodiscard]] bool is_maximized() const { return m_platform_window ? m_platform_window->is_maximized() : false; }
    void set_fullscreen(bool fullscreen) { if (m_platform_window) m_platform_window->set_fullscreen(fullscreen); }
    [[nodiscard]] bool is_fullscreen() const { return m_platform_window ? m_platform_window->is_fullscreen() : false; }
    void activate() { if (m_platform_window) m_platform_window->activate(); }
    void focus() { if (m_platform_window) m_platform_window->focus(); }
    [[nodiscard]] bool has_focus() const { return m_platform_window ? m_platform_window->has_focus() : false; }
    void set_always_on_top(bool on_top) {
        if (m_platform_window) m_platform_window->set_always_on_top(on_top);
        request_redraw();
    }
    [[nodiscard]] bool is_always_on_top() const { return m_platform_window ? m_platform_window->is_always_on_top() : false; }

    // ==================== 外观与标题栏 ====================
    [[nodiscard]] WindowTitlebar& titlebar() noexcept { return m_titlebar; }
    [[nodiscard]] const WindowTitlebar& titlebar() const noexcept { return m_titlebar; }
    [[nodiscard]] std::string title() const { return m_platform_window ? m_platform_window->title() : ""; }
    void set_title(const std::string& t) {
        if (m_platform_window) m_platform_window->set_title(t);
        request_redraw();
    }
    void set_decorated(bool decorated) {
        if (m_platform_window) m_platform_window->set_decorated(decorated);
        request_redraw();
    }
    [[nodiscard]] bool is_decorated() const { return m_platform_window ? m_platform_window->is_decorated() : false; }
    void set_show_titlebar(bool show) {
        if (m_show_titlebar != show) {
            m_show_titlebar = show;
            request_redraw();
        }
    }
    [[nodiscard]] bool show_titlebar() const noexcept { return m_show_titlebar; }
    [[nodiscard]] bool has_custom_titlebar() const { return m_show_titlebar && !is_decorated(); }
    [[nodiscard]] float titlebar_height() const { return has_custom_titlebar() ? m_titlebar.height() : 0.0f; }
    // 允许自定义无边框浮窗（如 DockPanel 浮动工具窗）注册自定义非客户区命中测试（例如返回 MaximizeBtn 以对接 Win11 分屏布局 Snap Layouts）
    void set_custom_hit_test(Fn<TitlebarHitTest(PointF)> cb) { m_custom_hit_test = std::move(cb); }
    void set_resizable(bool resizable) {
        if (m_platform_window) m_platform_window->set_resizable(resizable);
        request_redraw();
    }
    [[nodiscard]] bool is_resizable() const { return m_platform_window ? m_platform_window->is_resizable() : true; }
    void set_opacity(float opacity) { if (m_platform_window) m_platform_window->set_opacity(opacity); }
    [[nodiscard]] float opacity() const { return m_platform_window ? m_platform_window->opacity() : 1.0f; }

    // ==================== 触控长按等价右键 ====================
    static constexpr uint32_t kTouchLongPressMs = 480;
    static constexpr float kTouchLongPressMoveSlop = 10.0f;

    [[nodiscard]] bool has_pending_touch_long_press() const noexcept {
        return m_touch_lp.active && !m_touch_lp.fired;
    }
    [[nodiscard]] uint32_t time_until_touch_long_press_ms() const noexcept;
    bool tick_touch_long_press();
    bool trigger_touch_long_press_for_test();

    // ==================== 自算云母背景材质与主题切换动画 ====================
    void set_backdrop(WindowBackdrop backdrop) {
        if (m_backdrop != backdrop) {
            m_backdrop = backdrop;
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
        }
    }
    [[nodiscard]] WindowBackdrop backdrop() const noexcept { return m_backdrop; }
    void start_theme_ripple(const Theme& old_theme, const Theme& new_theme, Vec2 origin);
    void start_theme_crossfade(const Theme& old_theme, const Theme& new_theme, float duration_sec = 0.32f);

    [[nodiscard]] Vec2 last_pointer_pos() const noexcept {
        if (m_has_last_pointer) return m_last_pointer_pos;
        Vec2 sz = size();
        return Vec2{sz.x * 0.5f, sz.y * 0.5f};
    }

    // ==================== DPI 与显示参数 ====================
    [[nodiscard]] float dpi_scale() const { return m_platform_window ? m_platform_window->dpi_scale() : 1.0f; }
    [[nodiscard]] uint32_t refresh_rate() const { return m_platform_window ? m_platform_window->refresh_rate() : 60; }

    // ==================== 渲染与画质选项 (默认开启，支持手动关闭) ====================
    void set_dithering_enabled(bool enabled);
    [[nodiscard]] bool dithering_enabled() const noexcept { return m_dithering_enabled; }

    void set_pixel_snapping_enabled(bool enabled);
    [[nodiscard]] bool pixel_snapping_enabled() const noexcept { return m_pixel_snapping_enabled; }

    void set_font_gamma_enabled(bool enabled);
    [[nodiscard]] bool font_gamma_enabled() const noexcept { return m_font_gamma_enabled; }

    void set_acrylic_grain_enabled(bool enabled);
    [[nodiscard]] bool acrylic_grain_enabled() const noexcept { return m_acrylic_grain_enabled; }

    // ==================== 视图内容与管理器访问 ====================
    void set_content(WidgetPtr content);
    [[nodiscard]] RenderBox* root_render_box() const noexcept { return m_root_render_box.get(); }
    [[nodiscard]] FocusManager& focus_manager() noexcept { return m_focus_manager; }
    [[nodiscard]] EventDispatcher& event_dispatcher() noexcept { return m_event_dispatcher; }
    [[nodiscard]] OverlayManager& overlay_manager() noexcept { return m_overlay_manager; }
    [[nodiscard]] const OverlayManager& overlay_manager() const noexcept { return m_overlay_manager; }

    uint64_t show_overlay(OverlayEntry entry) {
        uint64_t id = m_overlay_manager.show(std::move(entry));
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
        return id;
    }
    void hide_overlay(uint64_t id) {
        m_overlay_manager.hide(id);
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }
    void dismiss_all_overlays() {
        m_overlay_manager.dismiss_all();
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }
    [[nodiscard]] bool has_active_overlay() const noexcept { return m_overlay_manager.has_active_overlay(); }

    [[nodiscard]] bool has_blinking_cursor() const noexcept {
        if (auto* client = (m_focus_manager.focused_node() ? m_focus_manager.focused_node()->as_text_input_client() : nullptr)) {
            return client->accepts_text_input();
        }
        return false;
    }
    bool tick_blinking_cursor() {
        if (auto* client = (m_focus_manager.focused_node() ? m_focus_manager.focused_node()->as_text_input_client() : nullptr)) {
            if (client->accepts_text_input()) return client->tick_cursor_blink();
        }
        return false;
    }
    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept {
        if (auto* client = (m_focus_manager.focused_node() ? m_focus_manager.focused_node()->as_text_input_client() : nullptr)) {
            if (client->accepts_text_input()) return client->time_until_next_blink_ms();
        }
        return 0;
    }

    [[nodiscard]] static Window* active_window() noexcept { return s_active_window; }

    // ==================== 帧渲染与平台事件路由 ====================
    void render_frame(bool force_full_present = false);
    void handle_platform_event(const PlatformEvent& pe);

    // ==================== 内置轻量 F12 UI Inspector (仅 Debug 模式生效，Release 零体积) ====================
    void set_inspector_enabled(bool enabled) {
#if KTUI_INSPECTOR_ACTIVE
        if (m_inspector.enabled != enabled) {
            m_inspector.enabled = enabled;
            if (!enabled) m_inspector.tile_heats.clear();
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
        }
#else
        (void)enabled;
#endif
    }
    [[nodiscard]] bool inspector_enabled() const noexcept {
#if KTUI_INSPECTOR_ACTIVE
        return m_inspector.enabled;
#else
        return false;
#endif
    }
    void set_inspector_dirty_tiles(bool show) {
#if KTUI_INSPECTOR_ACTIVE
        if (m_inspector.show_dirty_tiles != show) {
            m_inspector.show_dirty_tiles = show;
            if (!show) m_inspector.tile_heats.clear();
            request_redraw();
            if (m_platform_window) m_platform_window->invalidate();
        }
#else
        (void)show;
#endif
    }
#if KTUI_INSPECTOR_ACTIVE
    [[nodiscard]] UiInspectorState& inspector_state() noexcept { return m_inspector; }
    [[nodiscard]] const UiInspectorState& inspector_state() const noexcept { return m_inspector; }
#endif

    // ==================== 重绘调度与生命周期 ====================
    void request_redraw() noexcept { m_needs_redraw = true; }
    [[nodiscard]] bool needs_redraw() const noexcept {
        return m_needs_redraw || m_overlay_manager.needs_redraw() ||
#if KTUI_INSPECTOR_ACTIVE
               UiInspector::has_active_animations(m_inspector) ||
#endif
               (m_renderer && (m_renderer->compute_ready() != m_compute_was_ready ||
                (m_platform_window && m_platform_window->render_scale() != m_render_scale)));
    }
    void clear_redraw_flag() noexcept { m_needs_redraw = false; }
    void close();
    [[nodiscard]] bool is_closed() const noexcept { return m_is_closed; }

    // ==================== 回调监听挂接 ====================
    void on_resize(Fn<void(Vec2)> handler) { m_resize_callbacks.push_back(std::move(handler)); }
    void on_close(Fn<bool()> handler) { m_close_callbacks.push_back(std::move(handler)); }
    void on_dpi_changed(Fn<void(float)> handler) { m_dpi_callbacks.push_back(std::move(handler)); }
    void on_focus_changed(Fn<void(bool)> handler) { m_focus_callbacks.push_back(std::move(handler)); }
    void on_files_dropped(Fn<void(const std::vector<std::string>&, PointF)> handler) {
        m_file_drop_callbacks.push_back(std::move(handler));
    }

private:
    struct TouchLongPressState {
        bool active{false};
        bool fired{false};
        Vec2 pos{0.0f, 0.0f};
        uint32_t pointer_id{0};
        std::chrono::steady_clock::time_point down_time{};
    };

    struct ThemeRippleState {
        bool active{false};
        Theme old_theme{Theme::dark()};
        Theme new_theme{Theme::dark()};
        Vec2 origin{0.0f, 0.0f};
        float max_radius{0.0f};
        std::chrono::steady_clock::time_point start{};
    };

    struct ThemeCrossfadeState {
        bool active{false};
        Theme old_theme{Theme::dark()};
        Theme new_theme{Theme::dark()};
        float duration_sec{0.32f};
        std::chrono::steady_clock::time_point start{};
    };

    struct MicaAnimationState {
        float focus_progress{1.0f};
        std::chrono::steady_clock::time_point last_time{};
    };

    void ensure_renderer();
    [[nodiscard]] Rect offset_ime_rect(Rect r) const;
    void sync_ime_state();
    void init_internal_hooks();
    void advance_mica_animation();
    void paint_scene(PaintContext& ctx, Vec2 sz, Vec2 client_sz, float tb_h, bool mica_flag);
    bool route_overlay_event(const PlatformEvent& pe);
    bool route_titlebar_event(const PlatformEvent& pe, float tb_h);
    bool fire_touch_long_press();

    static void on_render_box_destroyed(RenderBox* box) noexcept;

    uint32_t m_id{0};
    std::unique_ptr<IPlatformWindow> m_platform_window;
    bool m_is_closed{false};
    bool m_needs_redraw{true};
    bool m_compute_was_ready{false};
    bool m_show_titlebar{true};
    bool m_window_focused{true};
    bool m_dithering_enabled{true};
    bool m_pixel_snapping_enabled{true};
    bool m_font_gamma_enabled{true};
    bool m_acrylic_grain_enabled{false};
    WindowBackdrop m_backdrop{WindowBackdrop::Mica};

    MicaAnimationState m_mica_anim;
    TouchLongPressState m_touch_lp;
    ThemeRippleState m_theme_ripple;
    ThemeCrossfadeState m_theme_crossfade;

    Vec2 m_last_pointer_pos{0.0f, 0.0f};
    bool m_has_last_pointer{false};
    float m_render_scale{1.0f};
    void* m_last_native_handle{nullptr};
    WindowTitlebar m_titlebar;

    WidgetPtr m_content_widget;
    std::shared_ptr<RenderBox> m_root_render_box;
    std::unique_ptr<kernetile::KerneTileRenderer> m_renderer;
    uint32_t m_renderer_w{0};
    uint32_t m_renderer_h{0};
    std::vector<uint8_t> m_framebuffer;
    EventDispatcher m_event_dispatcher;
    FocusManager m_focus_manager;
    OverlayManager m_overlay_manager;
#if KTUI_INSPECTOR_ACTIVE
    UiInspectorState m_inspector;
#endif

    static Window* s_active_window;
    static std::vector<Window*> s_live_windows;

    std::vector<Fn<void(Vec2)>> m_resize_callbacks;
    std::vector<Fn<bool()>> m_close_callbacks;
    std::vector<Fn<void(float)>> m_dpi_callbacks;
    std::vector<Fn<void(bool)>> m_focus_callbacks;
    std::vector<Fn<void(const std::vector<std::string>&, PointF)>> m_file_drop_callbacks;
    Fn<TitlebarHitTest(PointF)> m_custom_hit_test;
};

} // namespace ktui
