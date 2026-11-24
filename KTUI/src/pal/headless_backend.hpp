#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <mutex>
#include <vector>
#include <deque>
#include <cstdint>

namespace ktui {

// 纯内存虚拟任务调度器
class HeadlessDispatcher : public IPlatformDispatcher {
public:
    void post_task(Fn<void()> task) override;
    size_t process_tasks() override;
    bool has_pending_tasks() const override;

private:
    mutable std::mutex m_mutex;
    std::vector<Fn<void()>> m_queue;
};

// 纯内存虚拟帧时钟源
class HeadlessVSyncSource : public IVSyncSource {
public:
    uint64_t request_frame(Fn<void(double timestamp_seconds)> callback) override;
    void cancel_frame(uint64_t request_id) override;
    bool is_sleeping() const override;

    // 手动推进虚拟时钟并触发帧回调
    void step(double delta_seconds);
    double current_time() const { return m_current_time; }

private:
    struct Request {
        uint64_t id = 0;
        Fn<void(double)> callback;
    };
    std::vector<Request> m_requests;
    uint64_t m_next_id = 1;
    double m_current_time = 0.0;
};

// 跨平台默认系统字体回退解析器
class DefaultFontResolver : public IFontResolver {
public:
    std::string resolve_font(FontFamilyCategory category, FontWeight weight = FontWeight::Regular) override;
    std::string resolve_font_by_name(const std::string& family_name) override;
};

// 纯内存虚拟窗口
class HeadlessWindow : public IPlatformWindow {
public:
    explicit HeadlessWindow(const WindowDesc& desc);

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

    void show() override { m_visible = true; }
    void hide() override { m_visible = false; }
    bool is_visible() const override { return m_visible; }

    void minimize() override { m_state = WindowState::Minimized; }
    void maximize() override { m_state = WindowState::Maximized; }
    void restore() override { m_state = WindowState::Normal; }
    bool is_minimized() const override { return m_state == WindowState::Minimized; }
    bool is_maximized() const override { return m_state == WindowState::Maximized; }

    void set_fullscreen(bool fullscreen) override {
        m_state = fullscreen ? WindowState::Fullscreen : WindowState::Normal;
    }
    bool is_fullscreen() const override { return m_state == WindowState::Fullscreen; }

    void activate() override { m_active = true; m_focused = true; }
    bool is_active() const override { return m_active; }
    void focus() override { m_focused = true; }
    bool has_focus() const override { return m_focused; }

    void set_always_on_top(bool always_on_top) override { m_always_on_top = always_on_top; }
    bool is_always_on_top() const override { return m_always_on_top; }
    void flash_taskbar(bool flash) override { m_flashing = flash; }

    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override { m_title = title; }
    void set_icon(const std::string& icon_path) override { m_icon = icon_path; }

    void set_resizable(bool resizable) override { m_resizable = resizable; }
    bool is_resizable() const override { return m_resizable; }

    void set_opacity(float opacity) override { m_opacity = opacity; }
    float opacity() const override { return m_opacity; }

    void set_decorated(bool decorated) override { m_decorated = decorated; }
    bool is_decorated() const override { return m_decorated; }
    void set_titlebar_hit_test_callback(Fn<TitlebarHitTest(PointF local_pos)> cb) override {
        m_titlebar_hit_test = std::move(cb);
    }

    float dpi_scale() const override { return m_dpi_scale; }
    void set_dpi_scale(float scale);
    uint32_t refresh_rate() const override { return m_refresh_rate; }

    float render_scale() const override { return m_dpi_scale; }
    void* get_compute_display() const override { return nullptr; }
    void* get_native_handle() const override { return nullptr; }

    void present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                              uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
    void invalidate() override { if (m_on_paint) m_on_paint(); }
    void on_paint_requested(Fn<void()> handler) override { m_on_paint = std::move(handler); }
    void set_ime_enabled(bool enabled) override { m_ime_enabled = enabled; }
    void set_ime_cursor_rect(Rect rect) override { m_ime_cursor_rect = rect; }
    void set_cursor(CursorShape shape) override { m_cursor = shape; }

    void on_close_requested(Fn<bool()> handler) override { m_on_close = std::move(handler); }
    void on_resized(Fn<void(Vec2 new_size)> handler) override { m_on_resized = std::move(handler); }
    void on_moved(Fn<void(Vec2 new_pos)> handler) override { m_on_moved = std::move(handler); }
    void on_dpi_changed(Fn<void(float new_dpi)> handler) override { m_on_dpi_changed = std::move(handler); }
    void on_focus_changed(Fn<void(bool focused)> handler) override { m_on_focus_changed = std::move(handler); }
    void close() override;

    // 虚拟离屏像素缓冲区 (RGBA8)
    const std::vector<uint32_t>& virtual_framebuffer() const { return m_pixels; }
    std::vector<uint32_t>& virtual_framebuffer() { return m_pixels; }

private:
    std::string m_title;
    std::string m_icon;
    Vec2 m_size;
    Vec2 m_min_size;
    Vec2 m_max_size;
    Vec2 m_position = {100.0f, 100.0f};
    WindowState m_state = WindowState::Normal;
    bool m_visible = false;
    bool m_active = false;
    bool m_focused = false;
    bool m_always_on_top = false;
    bool m_flashing = false;
    bool m_resizable = true;
    bool m_decorated = true;
    float m_opacity = 1.0f;
    float m_dpi_scale = 1.0f;
    uint32_t m_refresh_rate = 60;

    std::vector<uint32_t> m_pixels;
    Fn<void()> m_on_paint;
    bool m_ime_enabled = false;
    Rect m_ime_cursor_rect{};
    CursorShape m_cursor = CursorShape::Arrow;

    Fn<TitlebarHitTest(PointF)> m_titlebar_hit_test;
    Fn<bool()> m_on_close;
    Fn<void(Vec2)> m_on_resized;
    Fn<void(Vec2)> m_on_moved;
    Fn<void(float)> m_on_dpi_changed;
    Fn<void(bool)> m_on_focus_changed;
};

// 纯内存无头平台驱动 (脱离物理显卡与屏幕，100% CI 自动化测试)
class HeadlessBackend : public IPlatformBackend {
public:
    HeadlessBackend() = default;
    ~HeadlessBackend() override = default;

    bool init() override { return true; }
    void poll_events() override;
    void wait_events(uint32_t timeout_ms = 0) override;

    std::unique_ptr<IPlatformWindow> create_window(const WindowDesc& desc) override;

    IPlatformDispatcher& get_dispatcher() override { return m_dispatcher; }
    IVSyncSource& get_vsync_source() override { return m_vsync; }
    IFontResolver& get_font_resolver() override { return m_font_resolver; }

    std::string get_clipboard_text() const override { return m_clipboard_text; }
    void set_clipboard_text(const std::string& utf8_text) override { m_clipboard_text = utf8_text; }

    void set_event_callback(Fn<void(const PlatformEvent&)> callback) override {
        m_event_callback = std::move(callback);
    }

    void shutdown() override {}

    // ==================== 单元测试模拟注入接口 ====================
    void inject_event(const PlatformEvent& event);
    void step_frame(double delta_seconds) { m_vsync.step(delta_seconds); }
    size_t pending_event_count() const { return m_event_queue.size(); }

private:
    HeadlessDispatcher m_dispatcher;
    HeadlessVSyncSource m_vsync;
    DefaultFontResolver m_font_resolver;
    Fn<void(const PlatformEvent&)> m_event_callback;
    std::deque<PlatformEvent> m_event_queue;
    std::string m_clipboard_text;
};

} // namespace ktui
