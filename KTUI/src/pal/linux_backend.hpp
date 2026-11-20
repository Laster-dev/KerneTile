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
#include <clocale>
#include <unordered_map>

#if defined(KTUI_HAS_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#endif

namespace ktui {

#if defined(KTUI_HAS_X11)
// X11 KeySym 到通用虚拟键码 (Virtual Key Code) 规范映射
static inline uint32_t x11_keysym_to_virtual_key(KeySym sym) {
    switch (sym) {
        case XK_BackSpace: return 8;   // VK_BACK
        case XK_Tab:       return 9;   // VK_TAB
        case XK_Clear:     return 12;  // VK_CLEAR
        case XK_Return:    return 13;  // VK_RETURN
        case XK_Pause:     return 19;  // VK_PAUSE
        case XK_Escape:    return 27;  // VK_ESCAPE
        case XK_space:     return 32;  // VK_SPACE
        case XK_Page_Up:   return 33;  // VK_PRIOR
        case XK_Page_Down: return 34;  // VK_NEXT
        case XK_End:       return 35;  // VK_END
        case XK_Home:      return 36;  // VK_HOME
        case XK_Left:      return 37;  // VK_LEFT
        case XK_Up:        return 38;  // VK_UP
        case XK_Right:     return 39;  // VK_RIGHT
        case XK_Down:      return 40;  // VK_DOWN
        case XK_Select:    return 41;  // VK_SELECT
        case XK_Print:     return 42;  // VK_PRINT
        case XK_Execute:   return 43;  // VK_EXECUTE
        case XK_Insert:    return 45;  // VK_INSERT
        case XK_Delete:    return 46;  // VK_DELETE
        case XK_Help:      return 47;  // VK_HELP

        case XK_0: case XK_1: case XK_2: case XK_3: case XK_4:
        case XK_5: case XK_6: case XK_7: case XK_8: case XK_9:
            return static_cast<uint32_t>(sym);

        case XK_a: case XK_b: case XK_c: case XK_d: case XK_e:
        case XK_f: case XK_g: case XK_h: case XK_i: case XK_j:
        case XK_k: case XK_l: case XK_m: case XK_n: case XK_o:
        case XK_p: case XK_q: case XK_r: case XK_s: case XK_t:
        case XK_u: case XK_v: case XK_w: case XK_x: case XK_y:
        case XK_z:
            return static_cast<uint32_t>(sym - XK_a + 'A');

        case XK_A: case XK_B: case XK_C: case XK_D: case XK_E:
        case XK_F: case XK_G: case XK_H: case XK_I: case XK_J:
        case XK_K: case XK_L: case XK_M: case XK_N: case XK_O:
        case XK_P: case XK_Q: case XK_R: case XK_S: case XK_T:
        case XK_U: case XK_V: case XK_W: case XK_X: case XK_Y:
        case XK_Z:
            return static_cast<uint32_t>(sym);

        case XK_F1:  return 112; // VK_F1
        case XK_F2:  return 113;
        case XK_F3:  return 114;
        case XK_F4:  return 115;
        case XK_F5:  return 116;
        case XK_F6:  return 117;
        case XK_F7:  return 118;
        case XK_F8:  return 119;
        case XK_F9:  return 120;
        case XK_F10: return 121;
        case XK_F11: return 122;
        case XK_F12: return 123; // VK_F12

        default:
            return static_cast<uint32_t>(sym);
    }
}

// X11 修饰键状态解析
static inline KeyModifiers x11_state_to_modifiers(unsigned int state) {
    KeyModifiers mods{};
    mods.shift = (state & ShiftMask) != 0;
    mods.ctrl  = (state & ControlMask) != 0;
    mods.alt   = (state & Mod1Mask) != 0;
    return mods;
}
#endif

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

// Linux 字体解析器
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
                return "sans-serif";
        }
    }

    std::string resolve_font_by_name(const std::string& family_name) override {
        return family_name.empty() ? "sans-serif" : family_name;
    }
};

// Linux 平台抽象物理/虚拟窗口
class LinuxWindow : public IPlatformWindow {
public:
#if defined(KTUI_HAS_X11)
    explicit LinuxWindow(const WindowDesc& desc, Display* dpy = nullptr, XIM xim = nullptr)
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

            if (xim) {
                m_xic = XCreateIC(xim,
                                  XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                  XNClientWindow, m_xwindow,
                                  XNFocusWindow, m_xwindow,
                                  nullptr);
                if (m_xic) {
                    XSetICFocus(m_xic);
                }
            }

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
            if (m_xic) XSetICFocus(m_xic);
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

#if defined(KTUI_HAS_X11)
    Window get_xwindow() const noexcept { return m_xwindow; }
    XIC get_xic() const noexcept { return m_xic; }
#endif

    void set_destroy_handler(std::function<void()> handler) {
        m_destroy_handler = std::move(handler);
    }

    // 将 CPU 光栅化帧缓冲呈现到真实 X11 物理窗口
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

    // 主动请求关闭窗口 (触发应用层 on_close 回调保护)
    bool request_close() {
        if (m_closed) return true;
        if (m_close_handler) {
            if (!m_close_handler()) {
                return false;
            }
        }
        close();
        return true;
    }

    void close() override {
        if (!m_closed) {
            m_closed = true;
            m_visible = false;
#if defined(KTUI_HAS_X11)
            if (m_xic) {
                XDestroyIC(m_xic);
                m_xic = nullptr;
            }
            if (m_display && m_gc) {
                XFreeGC(m_display, m_gc);
                m_gc = nullptr;
            }
            if (m_display && m_xwindow != 0) {
                Window win = m_xwindow;
                m_xwindow = 0;
                XDestroyWindow(m_display, win);
                XFlush(m_display);
            }
#endif
            if (m_destroy_handler) {
                m_destroy_handler();
            }
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
    XIC m_xic{nullptr};
#endif

    std::function<void()> m_destroy_handler{nullptr};
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
        std::setlocale(LC_ALL, "");
        XSetLocaleModifiers("");
        const char* disp_env = std::getenv("DISPLAY");
        if (disp_env && disp_env[0] != '\0') {
            m_display = XOpenDisplay(disp_env);
            if (m_display) {
                std::cout << "[LinuxBackend] Successfully connected to X11 Display Server: " << disp_env << std::endl;
                m_xim = XOpenIM(m_display, nullptr, nullptr, nullptr);
                if (m_xim) {
                    std::cout << "[LinuxBackend] Successfully initialized XIM Input Method." << std::endl;
                }
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

                // 优先交由系统输入法过滤器处理 (fcitx / ibus / deepin-ime)
                if (XFilterEvent(&ev, None)) {
                    continue;
                }

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
                    case FocusIn: {
                        auto it = m_windows_map.find(ev.xfocus.window);
                        if (it != m_windows_map.end() && it->second && it->second->get_xic()) {
                            XSetICFocus(it->second->get_xic());
                        }
                        break;
                    }
                    case FocusOut: {
                        auto it = m_windows_map.find(ev.xfocus.window);
                        if (it != m_windows_map.end() && it->second && it->second->get_xic()) {
                            XUnsetICFocus(it->second->get_xic());
                        }
                        break;
                    }
                    case KeyPress: {
                        if (m_event_callback) {
                            char buf[128];
                            KeySym sym = NoSymbol;
                            Status status = 0;
                            int count = 0;

                            XIC xic = nullptr;
                            auto it = m_windows_map.find(ev.xkey.window);
                            if (it != m_windows_map.end() && it->second) {
                                xic = it->second->get_xic();
                            }

                            if (xic) {
                                count = Xutf8LookupString(xic, &ev.xkey, buf, sizeof(buf) - 1, &sym, &status);
                            } else {
                                count = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &sym, nullptr);
                            }
                            if (count < 0) count = 0;
                            buf[count] = '\0';

                            uint32_t vk = x11_keysym_to_virtual_key(sym);
                            KeyModifiers mods = x11_state_to_modifiers(ev.xkey.state);

                            // 1. 发送 KeyDown 事件 (驱动退格、删除、箭头导航、回车提交及 Ctrl 快捷键)
                            PlatformEvent pe_down;
                            pe_down.type = PlatformEventType::KeyDown;
                            pe_down.window_handle = reinterpret_cast<void*>(ev.xkey.window);
                            pe_down.key_code = vk;
                            pe_down.modifiers = mods;
                            if (count > 0) pe_down.text = std::string(buf, count);
                            m_event_callback(pe_down);

                            // 2. 发送 ImeCommit 事件 (直接输入中英文文本到聚焦文本框)
                            if (count > 0 && !mods.ctrl && !mods.alt) {
                                bool is_printable = false;
                                if (count == 1) {
                                    uint8_t ch = static_cast<uint8_t>(buf[0]);
                                    is_printable = (ch >= 32 && ch != 127);
                                } else if (count > 1) {
                                    is_printable = true; // 多字节 UTF-8 (汉字等)
                                }

                                if (is_printable) {
                                    PlatformEvent pe_commit;
                                    pe_commit.type = PlatformEventType::ImeCommit;
                                    pe_commit.window_handle = reinterpret_cast<void*>(ev.xkey.window);
                                    pe_commit.key_code = vk;
                                    pe_commit.text = std::string(buf, count);
                                    pe_commit.modifiers = mods;
                                    m_event_callback(pe_commit);
                                }
                            }
                        }
                        break;
                    }
                    case KeyRelease: {
                        if (m_event_callback) {
                            KeySym sym = XLookupKeysym(&ev.xkey, 0);
                            PlatformEvent pe;
                            pe.type = PlatformEventType::KeyUp;
                            pe.window_handle = reinterpret_cast<void*>(ev.xkey.window);
                            pe.key_code = x11_keysym_to_virtual_key(sym);
                            pe.modifiers = x11_state_to_modifiers(ev.xkey.state);
                            m_event_callback(pe);
                        }
                        break;
                    }
                    case ClientMessage: {
                        Atom wm_protocols = XInternAtom(m_display, "WM_PROTOCOLS", False);
                        Atom wm_delete = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
                        if (ev.xclient.message_type == wm_protocols &&
                            static_cast<Atom>(ev.xclient.data.l[0]) == wm_delete) {
                            // 优先交由应用层 Window::handle_platform_event 派发并执行 close()
                            if (m_event_callback) {
                                PlatformEvent pe;
                                pe.type = PlatformEventType::WindowCloseRequested;
                                pe.window_handle = reinterpret_cast<void*>(ev.xclient.window);
                                m_event_callback(pe);
                            }
                            // 兜底保障：若窗口未被关闭则直接调用物理窗口关闭
                            auto it = m_windows_map.find(ev.xclient.window);
                            if (it != m_windows_map.end() && it->second) {
                                it->second->request_close();
                            }
                        }
                        break;
                    }
                    case DestroyNotify: {
                        m_windows_map.erase(ev.xdestroywindow.window);
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
        auto win = std::make_unique<LinuxWindow>(desc, m_display, m_xim);
        if (win->get_xwindow() != 0) {
            Window xw = win->get_xwindow();
            m_windows_map[xw] = win.get();
            win->set_destroy_handler([this, xw]() {
                m_windows_map.erase(xw);
            });
        }
        return win;
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
        if (m_xim) {
            XCloseIM(m_xim);
            m_xim = nullptr;
        }
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
    XIM m_xim{nullptr};
    std::unordered_map<Window, LinuxWindow*> m_windows_map;
#endif

    mutable std::mutex m_clip_mutex;
    std::string m_clipboard;
    std::function<void(const PlatformEvent&)> m_event_callback{nullptr};
};

} // namespace ktui
