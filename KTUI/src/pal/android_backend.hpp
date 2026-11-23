#pragma once

#if defined(__ANDROID__)

#include <ktui/pal/platform_backend.hpp>
#include "headless_backend.hpp"
#include <android/native_window.h>
#include <jni.h>
#include <string>
#include <vector>

namespace ktui {

class AndroidBackend;

class AndroidWindow : public IPlatformWindow {
public:
    AndroidWindow(const WindowDesc& desc, AndroidBackend* backend);
    ~AndroidWindow() override;

    Vec2 size() const override { return m_size; }
    void set_size(Vec2 new_size) override;
    Vec2 min_size() const override { return m_min_size; }
    void set_min_size(Vec2 min_sz) override { m_min_size = min_sz; }
    Vec2 max_size() const override { return m_max_size; }
    void set_max_size(Vec2 max_sz) override { m_max_size = max_sz; }

    Vec2 position() const override { return m_position; }
    void set_position(Vec2 pos) override { m_position = pos; }
    void center_on_screen() override {}
    Rect screen_bounds() const override { return Rect::from_xywh(0.0f, 0.0f, m_size.x, m_size.y); }

    void show() override { m_visible = true; }
    void hide() override { m_visible = false; }
    bool is_visible() const override { return m_visible; }

    void minimize() override {}
    void maximize() override { m_maximized = true; }
    void restore() override { m_maximized = false; }
    bool is_minimized() const override { return false; }
    bool is_maximized() const override { return m_maximized; }

    void set_fullscreen(bool fullscreen) override { m_fullscreen = fullscreen; }
    bool is_fullscreen() const override { return m_fullscreen; }

    void activate() override { m_active = true; m_focused = true; }
    bool is_active() const override { return m_active; }
    void focus() override { activate(); }
    bool has_focus() const override { return m_focused; }

    void set_always_on_top(bool always_on_top) override { m_always_on_top = always_on_top; }
    bool is_always_on_top() const override { return m_always_on_top; }
    void flash_taskbar(bool) override {}

    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override { m_title = title; }
    void set_icon(const std::string&) override {}

    void set_resizable(bool resizable) override { m_resizable = resizable; }
    bool is_resizable() const override { return m_resizable; }

    void set_opacity(float opacity) override { m_opacity = opacity; }
    float opacity() const override { return m_opacity; }

    void set_decorated(bool decorated) override { m_decorated = decorated; }
    bool is_decorated() const override { return m_decorated; }
    void set_titlebar_hit_test_callback(std::function<TitlebarHitTest(PointF)> cb) override {
        m_titlebar_hit_test = std::move(cb);
    }

    float dpi_scale() const override { return m_dpi_scale; }
    float render_scale() const override { return m_dpi_scale; }
    uint32_t refresh_rate() const override { return m_refresh_rate; }

    void* get_compute_display() const override { return nullptr; }
    void* get_native_handle() const override { return static_cast<void*>(m_native_window); }

    void present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) override;
    void present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                              uint32_t dirty_x, uint32_t dirty_y,
                              uint32_t dirty_w, uint32_t dirty_h) override;
    void invalidate() override {}
    void on_paint_requested(std::function<void()> handler) override { m_on_paint = std::move(handler); }

    void set_ime_enabled(bool enabled) override;
    void set_ime_cursor_rect(Rect cursor_rect) override { m_ime_cursor_rect = cursor_rect; }
    void set_cursor(CursorShape) override {}

    void on_close_requested(std::function<bool()> handler) override { m_on_close = std::move(handler); }
    void on_resized(std::function<void(Vec2)> handler) override { m_on_resized = std::move(handler); }
    void on_moved(std::function<void(Vec2)> handler) override { m_on_moved = std::move(handler); }
    void on_dpi_changed(std::function<void(float)> handler) override { m_on_dpi_changed = std::move(handler); }
    void on_focus_changed(std::function<void(bool)> handler) override { m_on_focus_changed = std::move(handler); }
    void close() override;

    // 内部 Surface 生命周期同步
    void bind_surface(ANativeWindow* window, uint32_t phys_w, uint32_t phys_h, float dpi_scale);
    void unbind_surface();

private:
    AndroidBackend* m_backend{nullptr};
    ANativeWindow* m_native_window{nullptr};
    std::string m_title;
    Vec2 m_size{412.0f, 915.0f};
    Vec2 m_min_size{100.0f, 100.0f};
    Vec2 m_max_size{16384.0f, 16384.0f};
    Vec2 m_position{0.0f, 0.0f};
    bool m_visible{true};
    bool m_maximized{true};
    bool m_fullscreen{false};
    bool m_active{true};
    bool m_focused{true};
    bool m_always_on_top{false};
    bool m_resizable{false};
    bool m_decorated{true};
    bool m_closed{false};
    float m_opacity{1.0f};
    float m_dpi_scale{1.0f};
    uint32_t m_refresh_rate{60};
    Rect m_ime_cursor_rect{};

    std::function<TitlebarHitTest(PointF)> m_titlebar_hit_test;
    std::function<void()> m_on_paint;
    std::function<bool()> m_on_close;
    std::function<void(Vec2)> m_on_resized;
    std::function<void(Vec2)> m_on_moved;
    std::function<void(float)> m_on_dpi_changed;
    std::function<void(bool)> m_on_focus_changed;
};

class AndroidBackend : public IPlatformBackend {
public:
    AndroidBackend();
    ~AndroidBackend() override;

    bool init() override { return true; }
    void poll_events() override { m_dispatcher.process_tasks(); }
    void wait_events(uint32_t) override { poll_events(); }

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

    static AndroidBackend* instance() noexcept { return s_instance; }
    static void set_jni_bridge(JNIEnv* env, jclass bridge_class);

    // 由 Android Activity / SurfaceView 回调
    void on_surface_changed(ANativeWindow* window, int phys_w, int phys_h, float density);
    void on_surface_destroyed();
    void on_touch(int action, float phys_x, float phys_y);
    void on_key(int action, int android_key_code, bool shift, bool ctrl, bool alt);
    void on_ime_composition(const std::string& text, size_t cursor_byte);
    void on_ime_commit(const std::string& text);

    void set_ime_enabled_jni(bool enabled);
    void unregister_window(AndroidWindow* win);

    ANativeWindow* current_surface() const noexcept { return m_surface; }
    uint32_t surface_width() const noexcept { return m_surface_w; }
    uint32_t surface_height() const noexcept { return m_surface_h; }
    float display_density() const noexcept { return m_density; }

private:
    void dispatch_event(const PlatformEvent& pe);

    static AndroidBackend* s_instance;
    HeadlessDispatcher m_dispatcher;
    HeadlessVSyncSource m_vsync;
    DefaultFontResolver m_font_resolver;
    std::function<void(const PlatformEvent&)> m_event_callback;
    std::vector<AndroidWindow*> m_windows;

    ANativeWindow* m_surface{nullptr};
    uint32_t m_surface_w{0};
    uint32_t m_surface_h{0};
    float m_density{1.0f};
};

} // namespace ktui

#endif // defined(__ANDROID__)
