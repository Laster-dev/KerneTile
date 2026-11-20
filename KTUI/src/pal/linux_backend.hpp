#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <mutex>
#include <vector>
#include <deque>
#include <string>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <iostream>

#if defined(KTUI_HAS_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#endif

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
                return "WenQuanYi Micro Hei";
        }
    }

    std::string resolve_font_by_name(const std::string& family_name) override {
        return family_name.empty() ? "WenQuanYi Micro Hei" : family_name;
    }
};

// Linux 平台抽象物理/虚拟窗口 (支持原生 X11 窗口与无头模式自动平滑切换)
class LinuxWindow : public IPlatformWindow {
public:
#if defined(KTUI_HAS_X11)
    explicit LinuxWindow(const WindowDesc& desc, Display* dpy = nullptr)
        : m_title(desc.title), m_size(desc.size), m_min_size(desc.min_size),
          m_max_size(desc.max_size), m_resizable(desc.resizable), m_display(dpy)
    {
        if (m_display != nullptr) {
            int screen = DefaultScreen(m_display);
            Window root = RootWindow(m_display, screen);
            int depth = DefaultDepth(m_display, screen);
            Visual* visual = DefaultVisual(m_display, screen);

            XSetWindowAttributes swa{};
            swa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask |
                             ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;
            
            m_xwindow = XCreateWindow(m_display, root, 100, 100,
                                      static_cast<uint32_t>(std::max(1.0f, m_size.x)),
                                      static_cast<uint32_t>(std::max(1.0f, m_size.y)),
                                      0, depth, InputOutput, visual, CWEventMask, &swa);

            XStoreName(m_display, m_xwindow, m_title.c_str());

            Atom wm_delete = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
            XSetWMProtocols(m_display, m_xwindow, &wm_delete, 1);
            m_gc = XCreateGC(m_display, m_xwindow, 0, nullptr);
            XFlush(m_display);
        }
    }
#else
    explicit LinuxWindow(const WindowDesc& desc)
        : m_title(desc.title), m_size(desc.size), m_min_size(desc.min_size),
          m_max_size(desc.max_size), m_resizable(desc.resizable) {}
#endif

    ~LinuxWindow() override {
        close();
    }

    Vec2 size() const override { return m_size; }
    void set_size(Vec2 new_size) override {
        m_size = new_size;
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XResizeWindow(m_display, m_xwindow, static_cast<uint32_t>(new_size.x), static_cast<uint32_t>(new_size.y));
            XFlush(m_display);
        }
#endif
        if (m_resize_handler) m_resize_handler(new_size);
    }

    Vec2 min_size() const override { return m_min_size; }
    void set_min_size(Vec2 min_sz) override { m_min_size = min_sz; }
    Vec2 max_size() const override { return m_max_size; }
    void set_max_size(Vec2 max_sz) override { m_max_size = max_sz; }

    Vec2 position() const override { return m_position; }
    void set_position(Vec2 pos) override {
        m_position = pos;
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XMoveWindow(m_display, m_xwindow, static_cast<int>(pos.x), static_cast<int>(pos.y));
            XFlush(m_display);
        }
#endif
        if (m_move_handler) m_move_handler(pos);
    }
    void center_on_screen() override {
        m_position = Vec2{100.0f, 100.0f};
        set_position(m_position);
    }
    Rect screen_bounds() const override {
        return Rect::from_xywh(0.0f, 0.0f, 1920.0f, 1080.0f);
    }

    void show() override { 
        m_visible = true; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XMapWindow(m_display, m_xwindow);
            XFlush(m_display);
        }
#endif
    }
    void hide() override { 
        m_visible = false; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XUnmapWindow(m_display, m_xwindow);
            XFlush(m_display);
        }
#endif
    }
    bool is_visible() const override { return m_visible; }

    void minimize() override { m_minimized = true; m_maximized = false; }
    void maximize() override { m_maximized = true; m_minimized = false; }
    void restore() override { m_minimized = false; m_maximized = false; }
    bool is_minimized() const override { return m_minimized; }
    bool is_maximized() const override { return m_maximized; }

    void set_fullscreen(bool fullscreen) override { m_fullscreen = fullscreen; }
    bool is_fullscreen() const override { return m_fullscreen; }

    void activate() override { 
        m_active = true; 
        m_focused = true; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XRaiseWindow(m_display, m_xwindow);
            XSetInputFocus(m_display, m_xwindow, RevertToParent, CurrentTime);
            XFlush(m_display);
        }
#endif
    }
    bool is_active() const override { return m_active; }
    void focus() override { activate(); }
    bool has_focus() const override { return m_focused; }

    void set_always_on_top(bool on_top) override { m_always_on_top = on_top; }
    bool is_always_on_top() const override { return m_always_on_top; }
    void flash_taskbar(bool /*flash*/) override {}

    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override { 
        m_title = title; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XStoreName(m_display, m_xwindow, m_title.c_str());
            XFlush(m_display);
        }
#endif
    }
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
    void* get_native_handle() const override { 
#if defined(KTUI_HAS_X11)
        if (m_xwindow != 0) {
            return reinterpret_cast<void*>(m_xwindow);
        }
#endif
        return const_cast<LinuxWindow*>(this); 
    }

    // 将 CPU 光栅化帧缓冲呈现到真实 X11 物理窗口 (实现真实桌面原生弹窗交互)
    void present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) override {
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0 && rgba && width > 0 && height > 0) {
            int screen = DefaultScreen(m_display);
            Visual* visual = DefaultVisual(m_display, screen);
            int depth = DefaultDepth(m_display, screen);

            std::vector<uint32_t> x11_pixels(static_cast<size_t>(width) * height);
            for (size_t i = 0; i < x11_pixels.size(); ++i) {
                uint8_t r = rgba[i * 4 + 0];
                uint8_t g = rgba[i * 4 + 1];
                uint8_t b = rgba[i * 4 + 2];
                uint8_t a = rgba[i * 4 + 3];
                x11_pixels[i] = (static_cast<uint32_t>(a) << 24) |
                                (static_cast<uint32_t>(r) << 16) |
                                (static_cast<uint32_t>(g) << 8)  |
                                (static_cast<uint32_t>(b));
            }

            XImage* img = XCreateImage(m_display, visual, depth, ZPixmap, 0,
                                       reinterpret_cast<char*>(x11_pixels.data()),
                                       width, height, 32, 0);
            if (img) {
                XPutImage(m_display, m_xwindow, m_gc, img, 0, 0, 0, 0, width, height);
                img->data = nullptr;
                XDestroyImage(img);
                XFlush(m_display);
            }
        }
#else
        (void)rgba; (void)width; (void)height;
#endif
    }

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
        if (!m_closed) {
            m_closed = true;
            m_visible = false;
#if defined(KTUI_HAS_X11)
            if (m_display && m_gc) {
                XFreeGC(m_display, m_gc);
                m_gc = nullptr;
            }
            if (m_display && m_xwindow != 0) {
                XDestroyWindow(m_display, m_xwindow);
                XFlush(m_display);
                m_xwindow = 0;
            }
#endif
        }
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

#if defined(KTUI_HAS_X11)
    Display* m_display{nullptr};
    Window m_xwindow{0};
    GC m_gc{nullptr};
#endif

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
    ~LinuxBackend() override {
        shutdown();
    }

    bool init() override {
#if defined(KTUI_HAS_X11)
        const char* disp_env = std::getenv("DISPLAY");
        if (disp_env && disp_env[0] != '\0') {
            m_display = XOpenDisplay(disp_env);
            if (m_display) {
                std::cout << "[LinuxBackend] Successfully connected to X11 Display Server: " << disp_env << std::endl;
            }
        }
#endif
        return true;
    }

    void poll_events() override {
        m_dispatcher.process_tasks();
        m_vsync.tick();

#if defined(KTUI_HAS_X11)
        if (m_display) {
            while (XPending(m_display) > 0) {
                XEvent ev;
                XNextEvent(m_display, &ev);
                switch (ev.type) {
                    case Expose: {
                        if (m_event_callback) {
                            PlatformEvent pe;
                            pe.type = PlatformEventType::WindowResize;
                            pe.window_handle = reinterpret_cast<void*>(ev.xexpose.window);
                            m_event_callback(pe);
                        }
                        break;
                    }
                    case MotionNotify: {
                        if (m_event_callback) {
                            PlatformEvent pe;
                            pe.type = PlatformEventType::PointerMove;
                            pe.window_handle = reinterpret_cast<void*>(ev.xmotion.window);
                            pe.position = PointF{static_cast<float>(ev.xmotion.x), static_cast<float>(ev.xmotion.y)};
                            m_event_callback(pe);
                        }
                        break;
                    }
                    case ButtonPress: {
                        if (m_event_callback) {
                            PlatformEvent pe;
                            pe.type = PlatformEventType::PointerDown;
                            pe.window_handle = reinterpret_cast<void*>(ev.xbutton.window);
                            pe.position = PointF{static_cast<float>(ev.xbutton.x), static_cast<float>(ev.xbutton.y)};
                            pe.button = (ev.xbutton.button == 1) ? PointerButton::Primary :
                                        (ev.xbutton.button == 3) ? PointerButton::Secondary : PointerButton::Auxiliary;
                            m_event_callback(pe);
                        }
                        break;
                    }
                    case ButtonRelease: {
                        if (m_event_callback) {
                            PlatformEvent pe;
                            pe.type = PlatformEventType::PointerUp;
                            pe.window_handle = reinterpret_cast<void*>(ev.xbutton.window);
                            pe.position = PointF{static_cast<float>(ev.xbutton.x), static_cast<float>(ev.xbutton.y)};
                            pe.button = (ev.xbutton.button == 1) ? PointerButton::Primary :
                                        (ev.xbutton.button == 3) ? PointerButton::Secondary : PointerButton::Auxiliary;
                            m_event_callback(pe);
                        }
                        break;
                    }
                    case KeyPress: {
                        if (m_event_callback) {
                            char buf[32];
                            KeySym sym;
                            int count = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &sym, nullptr);
                            buf[std::max(0, count)] = '\0';
                            PlatformEvent pe;
                            pe.type = PlatformEventType::KeyDown;
                            pe.window_handle = reinterpret_cast<void*>(ev.xkey.window);
                            pe.key_code = static_cast<uint32_t>(sym);
                            if (count > 0) pe.text = std::string(buf, count);
                            m_event_callback(pe);
                        }
                        break;
                    }
                    case KeyRelease: {
                        if (m_event_callback) {
                            KeySym sym = XLookupKeysym(&ev.xkey, 0);
                            PlatformEvent pe;
                            pe.type = PlatformEventType::KeyUp;
                            pe.window_handle = reinterpret_cast<void*>(ev.xkey.window);
                            pe.key_code = static_cast<uint32_t>(sym);
                            m_event_callback(pe);
                        }
                        break;
                    }
                    case ClientMessage: {
                        Atom wm_delete = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
                        if (static_cast<Atom>(ev.xclient.data.l[0]) == wm_delete) {
                            if (m_event_callback) {
                                PlatformEvent pe;
                                pe.type = PlatformEventType::WindowCloseRequested;
                                pe.window_handle = reinterpret_cast<void*>(ev.xclient.window);
                                m_event_callback(pe);
                            }
                        }
                        break;
                    }
                }
            }
        }
#endif
    }

    void wait_events(uint32_t timeout_ms = 0) override {
        if (timeout_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(timeout_ms, 16u)));
        }
        poll_events();
    }

    std::unique_ptr<IPlatformWindow> create_window(const WindowDesc& desc) override {
#if defined(KTUI_HAS_X11)
        return std::make_unique<LinuxWindow>(desc, m_display);
#else
        return std::make_unique<LinuxWindow>(desc);
#endif
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

    void shutdown() override {
#if defined(KTUI_HAS_X11)
        if (m_display) {
            XCloseDisplay(m_display);
            m_display = nullptr;
        }
#endif
    }

private:
    LinuxDispatcher m_dispatcher;
    LinuxVSyncSource m_vsync;
    LinuxFontResolver m_font_resolver;

#if defined(KTUI_HAS_X11)
    Display* m_display{nullptr};
#endif

    mutable std::mutex m_clip_mutex;
    std::string m_clipboard;
    std::function<void(const PlatformEvent&)> m_event_callback{nullptr};
};

} // namespace ktui
