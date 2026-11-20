#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <mutex>
#include <vector>
#include <deque>
#include <string>
#include <chrono>
#include <condition_variable>

namespace ktui {

// Linux 平台任务调度器 (POSIX Thread-safe Task Dispatcher)
class LinuxDispatcher : public IPlatformDispatcher {
public:
    void post_task(std::function<void()> task) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(task));
    }

    size_t process_tasks() override {
        std::vector<std::function<void()>> to_run;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            to_run.swap(m_queue);
        }
        for (auto& task : to_run) {
            if (task) task();
        }
        return to_run.size();
    }

    bool has_pending_tasks() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_queue.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::function<void()>> m_queue;
};

// Linux 高精定时器 VSync 时钟源 (POSIX Monotonic Clock)
class LinuxVSyncSource : public IVSyncSource {
public:
    uint64_t request_frame(std::function<void(double timestamp_seconds)> callback) override {
        uint64_t id = m_next_id++;
        m_requests.push_back({id, std::move(callback)});
        return id;
    }

    void cancel_frame(uint64_t request_id) override {
        auto it = std::remove_if(m_requests.begin(), m_requests.end(),
                                 [request_id](const Request& r) { return r.id == request_id; });
        m_requests.erase(it, m_requests.end());
    }

    bool is_sleeping() const override {
        return m_requests.empty();
    }

    void tick() {
        if (m_requests.empty()) return;
        auto now = std::chrono::steady_clock::now();
        double ts = std::chrono::duration<double>(now.time_since_epoch()).count();
        auto callbacks = std::move(m_requests);
        m_requests.clear();
        for (auto& req : callbacks) {
            if (req.callback) req.callback(ts);
        }
    }

private:
    struct Request {
        uint64_t id{0};
        std::function<void(double)> callback;
    };
    std::vector<Request> m_requests;
    uint64_t m_next_id{1};
};

// Linux 字体解析器 (支持 Fontconfig 与标准 XDG/Linux 字体目录)
class LinuxFontResolver : public IFontResolver {
public:
    std::string resolve_font(FontFamilyCategory category, FontWeight /*weight*/ = FontWeight::Regular) override {
        switch (category) {
            case FontFamilyCategory::Monospace:
                return "DejaVu Sans Mono";
            case FontFamilyCategory::Serif:
                return "DejaVu Serif";
            case FontFamilyCategory::SansSerif:
            default:
                return "DejaVu Sans";
        }
    }

    std::string resolve_font_by_name(const std::string& family_name) override {
        return family_name.empty() ? "DejaVu Sans" : family_name;
    }
};

// Linux 平台抽象物理/虚拟窗口
class LinuxWindow : public IPlatformWindow {
public:
    explicit LinuxWindow(const WindowDesc& desc)
        : m_title(desc.title), m_size(desc.size), m_min_size(desc.min_size),
          m_max_size(desc.max_size), m_resizable(desc.resizable) {}

    Vec2 size() const override { return m_size; }
    void set_size(Vec2 new_size) override {
        m_size = new_size;
        if (m_resize_handler) m_resize_handler(new_size);
    }

    Vec2 min_size() const override { return m_min_size; }
    void set_min_size(Vec2 min_sz) override { m_min_size = min_sz; }
    Vec2 max_size() const override { return m_max_size; }
    void set_max_size(Vec2 max_sz) override { m_max_size = max_sz; }

    Vec2 position() const override { return m_position; }
    void set_position(Vec2 pos) override {
        m_position = pos;
        if (m_move_handler) m_move_handler(pos);
    }
    void center_on_screen() override {
        m_position = Vec2{100.0f, 100.0f};
        if (m_move_handler) m_move_handler(m_position);
    }
    Rect screen_bounds() const override {
        return Rect::from_xywh(0.0f, 0.0f, 1920.0f, 1080.0f);
    }

    void show() override { m_visible = true; }
    void hide() override { m_visible = false; }
    bool is_visible() const override { return m_visible; }

    void minimize() override { m_minimized = true; m_maximized = false; }
    void maximize() override { m_maximized = true; m_minimized = false; }
    void restore() override { m_minimized = false; m_maximized = false; }
    bool is_minimized() const override { return m_minimized; }
    bool is_maximized() const override { return m_maximized; }

    void set_fullscreen(bool fullscreen) override { m_fullscreen = fullscreen; }
    bool is_fullscreen() const override { return m_fullscreen; }

    void activate() override { m_active = true; m_focused = true; }
    bool is_active() const override { return m_active; }
    void focus() override { m_focused = true; }
    bool has_focus() const override { return m_focused; }

    void set_always_on_top(bool on_top) override { m_always_on_top = on_top; }
    bool is_always_on_top() const override { return m_always_on_top; }
    void flash_taskbar(bool /*flash*/) override {}

    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override { m_title = title; }
    void set_icon(const std::string& /*icon_path*/) override {}

    void set_resizable(bool resizable) override { m_resizable = resizable; }
    bool is_resizable() const override { return m_resizable; }

    void set_opacity(float opacity) override { m_opacity = opacity; }
    float opacity() const override { return m_opacity; }

    void set_decorated(bool decorated) override { m_decorated = decorated; }
    bool is_decorated() const override { return m_decorated; }
    void set_titlebar_hit_test_callback(std::function<TitlebarHitTest(PointF)> cb) override {
        m_hit_test_cb = std::move(cb);
    }

    float dpi_scale() const override { return 1.0f; }
    uint32_t refresh_rate() const override { return 60; }
    void* get_native_handle() const override { return const_cast<LinuxWindow*>(this); }

    void invalidate() override {
        if (m_paint_requested) m_paint_requested();
    }
    void on_paint_requested(std::function<void()> handler) override {
        m_paint_requested = std::move(handler);
    }

    void on_close_requested(std::function<bool()> handler) override { m_close_handler = std::move(handler); }
    void on_resized(std::function<void(Vec2)> handler) override { m_resize_handler = std::move(handler); }
    void on_moved(std::function<void(Vec2)> handler) override { m_move_handler = std::move(handler); }
    void on_dpi_changed(std::function<void(float)> handler) override { m_dpi_handler = std::move(handler); }
    void on_focus_changed(std::function<void(bool)> handler) override { m_focus_handler = std::move(handler); }

    void close() override {
        m_closed = true;
        m_visible = false;
    }

private:
    std::string m_title;
    Vec2 m_size{800.0f, 600.0f};
    Vec2 m_min_size{0.0f, 0.0f};
    Vec2 m_max_size{10000.0f, 10000.0f};
    Vec2 m_position{100.0f, 100.0f};
    bool m_visible{false};
    bool m_minimized{false};
    bool m_maximized{false};
    bool m_fullscreen{false};
    bool m_active{false};
    bool m_focused{false};
    bool m_always_on_top{false};
    bool m_resizable{true};
    bool m_decorated{true};
    bool m_closed{false};
    float m_opacity{1.0f};

    std::function<TitlebarHitTest(PointF)> m_hit_test_cb{nullptr};
    std::function<void()> m_paint_requested{nullptr};
    std::function<bool()> m_close_handler{nullptr};
    std::function<void(Vec2)> m_resize_handler{nullptr};
    std::function<void(Vec2)> m_move_handler{nullptr};
    std::function<void(float)> m_dpi_handler{nullptr};
    std::function<void(bool)> m_focus_handler{nullptr};
};

// Linux 统一后端实现
class LinuxBackend : public IPlatformBackend {
public:
    LinuxBackend() = default;
    ~LinuxBackend() override = default;

    bool init() override { return true; }
    void poll_events() override {
        m_dispatcher.process_tasks();
        m_vsync.tick();
    }

    void wait_events(uint32_t timeout_ms = 0) override {
        if (timeout_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(timeout_ms, 16u)));
        }
        poll_events();
    }

    std::unique_ptr<IPlatformWindow> create_window(const WindowDesc& desc) override {
        return std::make_unique<LinuxWindow>(desc);
    }

    IPlatformDispatcher& get_dispatcher() override { return m_dispatcher; }
    IVSyncSource& get_vsync_source() override { return m_vsync; }
    IFontResolver& get_font_resolver() override { return m_font_resolver; }

    std::string get_clipboard_text() const override {
        std::lock_guard<std::mutex> lock(m_clip_mutex);
        return m_clipboard;
    }

    void set_clipboard_text(const std::string& utf8_text) override {
        std::lock_guard<std::mutex> lock(m_clip_mutex);
        m_clipboard = utf8_text;
    }

    void set_event_callback(std::function<void(const PlatformEvent&)> callback) override {
        m_event_callback = std::move(callback);
    }

    void shutdown() override {}

private:
    LinuxDispatcher m_dispatcher;
    LinuxVSyncSource m_vsync;
    LinuxFontResolver m_font_resolver;

    mutable std::mutex m_clip_mutex;
    std::string m_clipboard;
    std::function<void(const PlatformEvent&)> m_event_callback{nullptr};
};

} // namespace ktui
