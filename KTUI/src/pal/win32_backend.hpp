#pragma once

#if defined(_WIN32)

#include <ktui/pal/platform_backend.hpp>
#include "headless_backend.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vector>
#include <unordered_map>

namespace ktui {

class Win32Window : public IPlatformWindow {
public:
    Win32Window(const WindowDesc& desc, HINSTANCE hinstance);
    ~Win32Window() override;

    Vec2 size() const override { return m_size; }
    void set_size(Vec2 new_size) override;
    Vec2 min_size() const override { return m_min_size; }
    void set_min_size(Vec2 min_sz) override { m_min_size = min_sz; }
    Vec2 max_size() const override { return m_max_size; }
    void set_max_size(Vec2 max_sz) override { m_max_size = max_sz; }

    Vec2 position() const override { return m_position; }
    void set_position(Vec2 pos) override;
    void center_on_screen() override;
    Rect screen_bounds() const override;

    void show() override;
    void hide() override;
    bool is_visible() const override;

    void minimize() override;
    void maximize() override;
    void restore() override;
    bool is_minimized() const override;
    bool is_maximized() const override;

    void set_fullscreen(bool fullscreen) override;
    bool is_fullscreen() const override { return m_fullscreen; }

    void activate() override;
    bool is_active() const override;
    void focus() override;
    bool has_focus() const override;

    void set_always_on_top(bool always_on_top) override;
    bool is_always_on_top() const override { return m_always_on_top; }
    void flash_taskbar(bool flash) override;

    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override;
    void set_icon(const std::string& icon_path) override;

    void set_resizable(bool resizable) override;
    bool is_resizable() const override { return m_resizable; }

    void set_opacity(float opacity) override;
    float opacity() const override { return m_opacity; }

    void set_decorated(bool decorated) override;
    bool is_decorated() const override { return m_decorated; }
    void set_titlebar_hit_test_callback(std::function<TitlebarHitTest(PointF local_pos)> cb) override {
        m_titlebar_hit_test = std::move(cb);
    }

    float dpi_scale() const override { return m_dpi_scale; }
    uint32_t refresh_rate() const override { return m_refresh_rate; }

    void* get_native_handle() const override { return static_cast<void*>(m_hwnd); }

    void present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) override;
    void invalidate() override;
    void on_paint_requested(std::function<void()> handler) override { m_on_paint = std::move(handler); }

    void set_ime_enabled(bool enabled) override;
    void set_ime_cursor_rect(Rect cursor_rect) override;
    void set_cursor(CursorShape cursor) override;

    void on_close_requested(std::function<bool()> handler) override { m_on_close = std::move(handler); }
    void on_resized(std::function<void(Vec2 new_size)> handler) override { m_on_resized = std::move(handler); }
    void on_moved(std::function<void(Vec2 new_pos)> handler) override { m_on_moved = std::move(handler); }
    void on_dpi_changed(std::function<void(float new_dpi)> handler) override { m_on_dpi_changed = std::move(handler); }
    void on_focus_changed(std::function<void(bool focused)> handler) override { m_on_focus_changed = std::move(handler); }
    void close() override;

    HWND hwnd() const { return m_hwnd; }
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

private:
    void update_window_style();

    HWND m_hwnd = nullptr;
    HINSTANCE m_hinstance = nullptr;
    std::string m_title;
    Vec2 m_size;
    Vec2 m_min_size;
    Vec2 m_max_size;
    Vec2 m_position{100.0f, 100.0f};
    bool m_fullscreen = false;
    bool m_always_on_top = false;
    bool m_resizable = true;
    bool m_decorated = true;
    float m_opacity = 1.0f;
    float m_dpi_scale = 1.0f;
    uint32_t m_refresh_rate = 60;
    RECT m_normal_rect{}; // 退出全屏时的窗口恢复坐标

    std::function<TitlebarHitTest(PointF)> m_titlebar_hit_test;
    std::function<void()> m_on_paint;
    std::function<bool()> m_on_close;
    std::function<void(Vec2)> m_on_resized;
    std::function<void(Vec2)> m_on_moved;
    std::function<void(float)> m_on_dpi_changed;
    std::function<void(bool)> m_on_focus_changed;
    std::vector<uint8_t> m_bgra_scratch;
    Rect m_ime_cursor_rect{};
    bool m_ime_enabled = false;
    CursorShape m_cursor_shape = CursorShape::Arrow;
};

// Windows 原生 PAL 平台后端
class Win32Backend : public IPlatformBackend {
public:
    Win32Backend();
    ~Win32Backend() override;

    bool init() override;
    void poll_events() override;
    void wait_events(uint32_t timeout_ms = 0) override;

    std::unique_ptr<IPlatformWindow> create_window(const WindowDesc& desc) override;

    IPlatformDispatcher& get_dispatcher() override { return m_dispatcher; }
    IVSyncSource& get_vsync_source() override { return m_vsync; }
    IFontResolver& get_font_resolver() override { return m_font_resolver; }

    std::string get_clipboard_text() const override;
    void set_clipboard_text(const std::string& utf8_text) override;

    void set_event_callback(std::function<void(const PlatformEvent&)> callback) override {
        m_event_callback = std::move(callback);
    }

    void shutdown() override;

    static Win32Backend* instance() { return s_instance; }
    void dispatch_platform_event(const PlatformEvent& event);

private:
    static LRESULT CALLBACK static_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    static Win32Backend* s_instance;
    HINSTANCE m_hinstance = nullptr;
    HeadlessDispatcher m_dispatcher;
    HeadlessVSyncSource m_vsync;
    DefaultFontResolver m_font_resolver;
    std::function<void(const PlatformEvent&)> m_event_callback;
    std::unordered_map<HWND, Win32Window*> m_window_map;
    bool m_class_registered = false;
};

} // namespace ktui

#endif // defined(_WIN32)
