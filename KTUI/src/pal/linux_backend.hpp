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
#include <cstring>
#include <algorithm>

#if defined(KTUI_HAS_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <poll.h>

#ifdef CursorShape
#undef CursorShape
#endif
#endif

namespace ktui {

#if defined(KTUI_HAS_X11)
// X11 KeySym 到通用虚拟键码 (Virtual Key Code) 规范映射 (对标 Windows VK_*)
static inline uint32_t x11_keysym_to_virtual_key(KeySym sym) {
    switch (sym) {
        case XK_BackSpace:   return 8;   // VK_BACK
        case XK_Tab:         return 9;   // VK_TAB
        case XK_Clear:       return 12;  // VK_CLEAR
        case XK_Return:      return 13;  // VK_RETURN
        case XK_Shift_L:
        case XK_Shift_R:     return 16;  // VK_SHIFT
        case XK_Control_L:
        case XK_Control_R:   return 17;  // VK_CONTROL
        case XK_Alt_L:
        case XK_Alt_R:       return 18;  // VK_MENU (Alt)
        case XK_Pause:       return 19;  // VK_PAUSE
        case XK_Caps_Lock:   return 20;  // VK_CAPITAL
        case XK_Escape:      return 27;  // VK_ESCAPE
        case XK_space:       return 32;  // VK_SPACE
        case XK_Page_Up:     return 33;  // VK_PRIOR
        case XK_Page_Down:   return 34;  // VK_NEXT
        case XK_End:         return 35;  // VK_END
        case XK_Home:        return 36;  // VK_HOME
        case XK_Left:        return 37;  // VK_LEFT
        case XK_Up:          return 38;  // VK_UP
        case XK_Right:       return 39;  // VK_RIGHT
        case XK_Down:        return 40;  // VK_DOWN
        case XK_Select:      return 41;  // VK_SELECT
        case XK_Print:       return 42;  // VK_PRINT
        case XK_Execute:     return 43;  // VK_EXECUTE
        case XK_Insert:      return 45;  // VK_INSERT
        case XK_Delete:      return 46;  // VK_DELETE
        case XK_Help:        return 47;  // VK_HELP

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

        case XK_Super_L:     return 91;  // VK_LWIN
        case XK_Super_R:     return 92;  // VK_RWIN
        case XK_Menu:        return 93;  // VK_APPS

        case XK_KP_0:        return 96;  // VK_NUMPAD0
        case XK_KP_1:        return 97;
        case XK_KP_2:        return 98;
        case XK_KP_3:        return 99;
        case XK_KP_4:        return 100;
        case XK_KP_5:        return 101;
        case XK_KP_6:        return 102;
        case XK_KP_7:        return 103;
        case XK_KP_8:        return 104;
        case XK_KP_9:        return 105;
        case XK_KP_Multiply: return 106; // VK_MULTIPLY
        case XK_KP_Add:      return 107; // VK_ADD
        case XK_KP_Separator:return 108; // VK_SEPARATOR
        case XK_KP_Subtract: return 109; // VK_SUBTRACT
        case XK_KP_Decimal:  return 110; // VK_DECIMAL
        case XK_KP_Divide:   return 111; // VK_DIVIDE
        case XK_KP_Enter:    return 13;  // VK_RETURN

        case XK_F1:          return 112; // VK_F1
        case XK_F2:          return 113;
        case XK_F3:          return 114;
        case XK_F4:          return 115;
        case XK_F5:          return 116;
        case XK_F6:          return 117;
        case XK_F7:          return 118;
        case XK_F8:          return 119;
        case XK_F9:          return 120;
        case XK_F10:         return 121;
        case XK_F11:         return 122;
        case XK_F12:         return 123; // VK_F12

        case XK_Num_Lock:    return 144; // VK_NUMLOCK
        case XK_Scroll_Lock: return 145; // VK_SCROLL

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
                return "monospace";
            case FontFamilyCategory::Serif:
                return "serif";
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
          m_max_size(desc.max_size), m_position(desc.position),
          m_always_on_top(desc.always_on_top), m_resizable(desc.resizable),
          m_decorated(desc.decorated), m_display(dpy)
    {
        if (m_display != nullptr) {
            int screen = DefaultScreen(m_display);
            Window root = RootWindow(m_display, screen);
            int depth = DefaultDepth(m_display, screen);
            Visual* visual = DefaultVisual(m_display, screen);

            int scr_w = DisplayWidth(m_display, screen);
            int scr_h = DisplayHeight(m_display, screen);
            int win_x = (desc.position.x >= 0.0f) 
                ? static_cast<int>(desc.position.x) 
                : std::max(0, (scr_w - static_cast<int>(m_size.x)) / 2);
            int win_y = (desc.position.y >= 0.0f) 
                ? static_cast<int>(desc.position.y) 
                : std::max(0, (scr_h - static_cast<int>(m_size.y)) / 2);
            m_position = Vec2{static_cast<float>(win_x), static_cast<float>(win_y)};

            XSetWindowAttributes swa{};
            swa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask |
                             ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;
            
            m_xwindow = XCreateWindow(m_display, root, win_x, win_y,
                                      static_cast<uint32_t>(std::max(1.0f, m_size.x)),
                                      static_cast<uint32_t>(std::max(1.0f, m_size.y)),
                                      0, depth, InputOutput, visual, CWEventMask, &swa);

            // 1. 设置窗口标题 (兼容 Latin-1 与 UTF-8 EWMH)
            XStoreName(m_display, m_xwindow, m_title.c_str());
            Atom net_wm_name = XInternAtom(m_display, "_NET_WM_NAME", False);
            Atom utf8_string = XInternAtom(m_display, "UTF8_STRING", False);
            XChangeProperty(m_display, m_xwindow, net_wm_name, utf8_string, 8, PropModeReplace,
                            reinterpret_cast<const unsigned char*>(m_title.c_str()), static_cast<int>(m_title.size()));

            // 2. 窗口边框装饰 (_MOTIF_WM_HINTS)
            if (!m_decorated) {
                struct MotifWmHints {
                    unsigned long flags{2}; // MWM_HINTS_DECORATIONS
                    unsigned long functions{0};
                    unsigned long decorations{0}; // 0 = 无边框
                    long input_mode{0};
                    unsigned long status{0};
                } hints;
                Atom motif_hints = XInternAtom(m_display, "_MOTIF_WM_HINTS", False);
                XChangeProperty(m_display, m_xwindow, motif_hints, motif_hints, 32,
                                PropModeReplace, reinterpret_cast<unsigned char*>(&hints), 5);
            }

            // 3. 尺寸约束与 Resizable 控制 (XSizeHints)
            update_size_hints();

            // 4. 置顶 (_NET_WM_STATE_ABOVE)
            if (m_always_on_top) {
                Atom wm_state = XInternAtom(m_display, "_NET_WM_STATE", False);
                Atom wm_above = XInternAtom(m_display, "_NET_WM_STATE_ABOVE", False);
                XChangeProperty(m_display, m_xwindow, wm_state, XA_ATOM, 32, PropModeReplace,
                                reinterpret_cast<unsigned char*>(&wm_above), 1);
            }

            // 5. 窗口类型 (Popup / ToolWindow)
            if (desc.type == WindowType::Popup) {
                Atom wm_type = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE", False);
                Atom popup_type = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
                XChangeProperty(m_display, m_xwindow, wm_type, XA_ATOM, 32, PropModeReplace,
                                reinterpret_cast<unsigned char*>(&popup_type), 1);
            } else if (desc.type == WindowType::ToolWindow) {
                Atom wm_type = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE", False);
                Atom tool_type = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
                XChangeProperty(m_display, m_xwindow, wm_type, XA_ATOM, 32, PropModeReplace,
                                reinterpret_cast<unsigned char*>(&tool_type), 1);
            }
            if (desc.parent_handle) {
                XSetTransientForHint(m_display, m_xwindow, reinterpret_cast<Window>(desc.parent_handle));
            }

            // 6. 协议绑定与 GC 初始化
            Atom wm_delete = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
            XSetWMProtocols(m_display, m_xwindow, &wm_delete, 1);
            m_gc = XCreateGC(m_display, m_xwindow, 0, nullptr);

            // 7. XIM 输入法上下文初始化
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

            // 8. 默认光标
            set_cursor(CursorShape::Arrow);

            XFlush(m_display);
        }
    }
#else
    explicit LinuxWindow(const WindowDesc& desc)
        : m_title(desc.title), m_size(desc.size), m_min_size(desc.min_size),
          m_max_size(desc.max_size), m_position(desc.position),
          m_always_on_top(desc.always_on_top), m_resizable(desc.resizable),
          m_decorated(desc.decorated) {}
#endif

    ~LinuxWindow() override {
        close();
    }

    Vec2 size() const override { return m_size; }
    void set_size(Vec2 new_size) override {
        m_size = new_size;
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XResizeWindow(m_display, m_xwindow, 
                          static_cast<uint32_t>(std::max(1.0f, new_size.x)), 
                          static_cast<uint32_t>(std::max(1.0f, new_size.y)));
            XFlush(m_display);
        }
#endif
        if (m_resize_handler) m_resize_handler(new_size);
    }

    Vec2 min_size() const override { return m_min_size; }
    void set_min_size(Vec2 min_sz) override { 
        m_min_size = min_sz; 
        update_size_hints();
    }
    Vec2 max_size() const override { return m_max_size; }
    void set_max_size(Vec2 max_sz) override { 
        m_max_size = max_sz; 
        update_size_hints();
    }

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
#if defined(KTUI_HAS_X11)
        if (m_display) {
            int screen = DefaultScreen(m_display);
            int scr_w = DisplayWidth(m_display, screen);
            int scr_h = DisplayHeight(m_display, screen);
            m_position = Vec2{static_cast<float>(std::max(0, (scr_w - static_cast<int>(m_size.x)) / 2)),
                              static_cast<float>(std::max(0, (scr_h - static_cast<int>(m_size.y)) / 2))};
            set_position(m_position);
            return;
        }
#endif
        m_position = Vec2{100.0f, 100.0f};
        set_position(m_position);
    }

    Rect screen_bounds() const override {
#if defined(KTUI_HAS_X11)
        if (m_display) {
            int screen = DefaultScreen(m_display);
            return Rect::from_xywh(0.0f, 0.0f,
                                   static_cast<float>(DisplayWidth(m_display, screen)),
                                   static_cast<float>(DisplayHeight(m_display, screen)));
        }
#endif
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

    void minimize() override { 
        m_minimized = true; 
        m_maximized = false; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XIconifyWindow(m_display, m_xwindow, DefaultScreen(m_display));
            XFlush(m_display);
        }
#endif
    }

    void maximize() override { 
        m_maximized = true; 
        m_minimized = false; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            send_net_wm_state(true, "_NET_WM_STATE_MAXIMIZED_VERT", "_NET_WM_STATE_MAXIMIZED_HORZ");
        }
#endif
    }

    void restore() override { 
        m_minimized = false; 
        m_maximized = false; 
        m_fullscreen = false;
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            send_net_wm_state(false, "_NET_WM_STATE_MAXIMIZED_VERT", "_NET_WM_STATE_MAXIMIZED_HORZ");
            send_net_wm_state(false, "_NET_WM_STATE_FULLSCREEN", nullptr);
            XMapWindow(m_display, m_xwindow);
            XFlush(m_display);
        }
#endif
    }

    bool is_minimized() const override { return m_minimized; }
    bool is_maximized() const override { return m_maximized; }

    void set_fullscreen(bool fullscreen) override { 
        m_fullscreen = fullscreen; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            send_net_wm_state(fullscreen, "_NET_WM_STATE_FULLSCREEN", nullptr);
        }
#endif
    }
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

    void set_always_on_top(bool on_top) override { 
        m_always_on_top = on_top; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            send_net_wm_state(on_top, "_NET_WM_STATE_ABOVE", nullptr);
        }
#endif
    }
    bool is_always_on_top() const override { return m_always_on_top; }

    void flash_taskbar(bool flash) override {
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            send_net_wm_state(flash, "_NET_WM_STATE_DEMANDS_ATTENTION", nullptr);
        }
#else
        (void)flash;
#endif
    }

    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override { 
        m_title = title; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XStoreName(m_display, m_xwindow, m_title.c_str());
            Atom net_wm_name = XInternAtom(m_display, "_NET_WM_NAME", False);
            Atom utf8_string = XInternAtom(m_display, "UTF8_STRING", False);
            XChangeProperty(m_display, m_xwindow, net_wm_name, utf8_string, 8, PropModeReplace,
                            reinterpret_cast<const unsigned char*>(m_title.c_str()), static_cast<int>(m_title.size()));
            XFlush(m_display);
        }
#endif
    }
    void set_icon(const std::string& /*icon_path*/) override {}

    void set_resizable(bool resizable) override { 
        m_resizable = resizable; 
        update_size_hints();
    }
    bool is_resizable() const override { return m_resizable; }

    void set_opacity(float opacity) override { 
        m_opacity = std::clamp(opacity, 0.0f, 1.0f); 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            uint32_t opacity_val = static_cast<uint32_t>(m_opacity * 4294967295.0f);
            Atom net_opacity = XInternAtom(m_display, "_NET_WM_WINDOW_OPACITY", False);
            XChangeProperty(m_display, m_xwindow, net_opacity, XA_CARDINAL, 32, PropModeReplace,
                            reinterpret_cast<unsigned char*>(&opacity_val), 1);
            XFlush(m_display);
        }
#endif
    }
    float opacity() const override { return m_opacity; }

    void set_decorated(bool decorated) override { 
        m_decorated = decorated; 
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            struct MotifWmHints {
                unsigned long flags{2};
                unsigned long functions{0};
                unsigned long decorations{0};
                long input_mode{0};
                unsigned long status{0};
            } hints;
            hints.decorations = decorated ? 1 : 0;
            Atom motif_hints = XInternAtom(m_display, "_MOTIF_WM_HINTS", False);
            XChangeProperty(m_display, m_xwindow, motif_hints, motif_hints, 32,
                            PropModeReplace, reinterpret_cast<unsigned char*>(&hints), 5);
            XFlush(m_display);
        }
#endif
    }
    bool is_decorated() const override { return m_decorated; }
    void set_titlebar_hit_test_callback(std::function<TitlebarHitTest(PointF)> cb) override {
        m_hit_test_cb = std::move(cb);
    }

    float dpi_scale() const override { 
#if defined(KTUI_HAS_X11)
        if (m_display) {
            char* rms = XResourceManagerString(m_display);
            if (rms) {
                char* ptr = std::strstr(rms, "Xft.dpi:");
                if (ptr) {
                    float dpi = static_cast<float>(std::atof(ptr + 8));
                    if (dpi > 20.0f && dpi < 500.0f) {
                        return dpi / 96.0f;
                    }
                }
            }
            int screen = DefaultScreen(m_display);
            int w_px = DisplayWidth(m_display, screen);
            int w_mm = DisplayWidthMM(m_display, screen);
            if (w_mm > 0) {
                float dpi = (static_cast<float>(w_px) / static_cast<float>(w_mm)) * 25.4f;
                if (dpi >= 80.0f && dpi <= 400.0f) {
                    return dpi / 96.0f;
                }
            }
        }
#endif
        return 1.0f; 
    }

    uint32_t refresh_rate() const override { return 60; }

    float render_scale() const override { return 1.0f; }

    void* get_compute_display() const override {
#if defined(KTUI_HAS_X11)
        if (m_xwindow) return m_display;
#endif
        return nullptr;
    }

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

#if defined(KTUI_HAS_X11)
    Cursor get_or_create_cursor(unsigned int shape) {
        if (!m_display) return None;
        auto it = m_cached_cursors.find(shape);
        if (it != m_cached_cursors.end()) {
            return it->second;
        }
        Cursor c = XCreateFontCursor(m_display, shape);
        if (c != None) {
            m_cached_cursors[shape] = c;
        }
        return c;
    }
#endif

    // 鼠标光标形状切换 (对标 Windows SetCursor)
    void set_cursor(CursorShape cursor) override {
        if (m_cursor_shape == cursor) return;
        m_cursor_shape = cursor;
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            unsigned int shape = XC_left_ptr;
            switch (cursor) {
                case CursorShape::IBeam:        shape = XC_xterm; break;
                case CursorShape::PointingHand: shape = XC_hand2; break;
                case CursorShape::ResizeEW:     shape = XC_sb_h_double_arrow; break;
                case CursorShape::ResizeNS:     shape = XC_sb_v_double_arrow; break;
                case CursorShape::ResizeNWSE:   shape = XC_bottom_right_corner; break;
                case CursorShape::ResizeNESW:   shape = XC_bottom_left_corner; break;
                case CursorShape::Crosshair:    shape = XC_crosshair; break;
                case CursorShape::Forbidden:    shape = XC_circle; break;
                case CursorShape::Arrow:
                default:                        shape = XC_left_ptr; break;
            }
            Cursor xcursor = get_or_create_cursor(shape);
            if (xcursor != None) {
                XDefineCursor(m_display, m_xwindow, xcursor);
                XFlush(m_display);
            }
        }
#endif
    }

    // 输入法光标位置同步 (对标 Windows ImmSetCompositionWindow)
    void set_ime_cursor_rect(Rect cursor_rect) override {
        m_ime_cursor_rect = cursor_rect;
#if defined(KTUI_HAS_X11)
        if (m_display && m_xic) {
            XPoint spot;
            spot.x = static_cast<short>(cursor_rect.min_x);
            spot.y = static_cast<short>(cursor_rect.max_y);
            XVaNestedList preedit_attr = XVaCreateNestedList(0, XNSpotLocation, &spot, nullptr);
            if (preedit_attr) {
                XSetICValues(m_xic, XNPreeditAttributes, preedit_attr, nullptr);
                XFree(preedit_attr);
            }
        }
#endif
    }

    void set_ime_enabled(bool enabled) override {
        m_ime_enabled = enabled;
#if defined(KTUI_HAS_X11)
        if (m_display && m_xic) {
            if (enabled) {
                XSetICFocus(m_xic);
                set_ime_cursor_rect(m_ime_cursor_rect);
            } else {
                XUnsetICFocus(m_xic);
            }
        }
#endif
    }

    // 将 CPU 光栅化帧缓冲呈现到真实 X11 物理窗口 (支持全屏及局部脏区极速直传)
    void present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) override {
        present_rgba8_region(rgba, width, height, 0, 0, width, height);
    }

    void present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                              uint32_t dirty_x, uint32_t dirty_y,
                              uint32_t dirty_w, uint32_t dirty_h) override {
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0 && rgba && width > 0 && height > 0 && dirty_w > 0 && dirty_h > 0) {
            int screen = DefaultScreen(m_display);
            Visual* visual = DefaultVisual(m_display, screen);
            int depth = DefaultDepth(m_display, screen);

            size_t pixel_count = static_cast<size_t>(dirty_w) * dirty_h;
            m_scratch_pixels.resize(pixel_count);
            for (uint32_t r = 0; r < dirty_h; ++r) {
                const uint8_t* src_row = rgba + (static_cast<size_t>(dirty_y + r) * width + dirty_x) * 4;
                uint32_t* dst_row = m_scratch_pixels.data() + static_cast<size_t>(r) * dirty_w;
                for (uint32_t c = 0; c < dirty_w; ++c) {
                    uint8_t red   = src_row[c * 4 + 0];
                    uint8_t green = src_row[c * 4 + 1];
                    uint8_t blue  = src_row[c * 4 + 2];
                    uint8_t alpha = src_row[c * 4 + 3];
                    dst_row[c] = (static_cast<uint32_t>(alpha) << 24) |
                                 (static_cast<uint32_t>(red) << 16) |
                                 (static_cast<uint32_t>(green) << 8)  |
                                 (static_cast<uint32_t>(blue));
                }
            }

            XImage* img = XCreateImage(m_display, visual, depth, ZPixmap, 0,
                                       reinterpret_cast<char*>(m_scratch_pixels.data()),
                                       dirty_w, dirty_h, 32, 0);
            if (img) {
                XPutImage(m_display, m_xwindow, m_gc, img, 0, 0, dirty_x, dirty_y, dirty_w, dirty_h);
                img->data = nullptr; // 避免 XDestroyImage 释放 m_scratch_pixels 内存
                XDestroyImage(img);
                XFlush(m_display);
            }
        }
#else
        (void)rgba; (void)width; (void)height; (void)dirty_x; (void)dirty_y; (void)dirty_w; (void)dirty_h;
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

    // X11 ConfigureNotify 事件通知
    void notify_configure(Vec2 new_sz, Vec2 new_pos) {
        bool sz_changed = (new_sz.x != m_size.x || new_sz.y != m_size.y);
        bool pos_changed = (new_pos.x != m_position.x || new_pos.y != m_position.y);
        m_size = new_sz;
        m_position = new_pos;
        if (sz_changed && m_resize_handler) {
            m_resize_handler(new_sz);
        }
        if (pos_changed && m_move_handler) {
            m_move_handler(new_pos);
        }
    }

    // X11 FocusIn/Out 事件通知
    void notify_focus(bool focused) {
        m_focused = focused;
        m_active = focused;
        if (m_focus_handler) {
            m_focus_handler(focused);
        }
    }

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
            for (auto& pair : m_cached_cursors) {
                if (pair.second != None && m_display) {
                    XFreeCursor(m_display, pair.second);
                }
            }
            m_cached_cursors.clear();
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
    void update_size_hints() {
#if defined(KTUI_HAS_X11)
        if (m_display && m_xwindow != 0) {
            XSizeHints* hints = XAllocSizeHints();
            if (hints) {
                hints->flags = PMinSize | PMaxSize;
                if (!m_resizable) {
                    hints->min_width = hints->max_width = static_cast<int>(m_size.x);
                    hints->min_height = hints->max_height = static_cast<int>(m_size.y);
                } else {
                    hints->min_width = static_cast<int>(m_min_size.x);
                    hints->min_height = static_cast<int>(m_min_size.y);
                    hints->max_width = static_cast<int>(m_max_size.x);
                    hints->max_height = static_cast<int>(m_max_size.y);
                }
                XSetWMNormalHints(m_display, m_xwindow, hints);
                XFree(hints);
                XFlush(m_display);
            }
        }
#endif
    }

#if defined(KTUI_HAS_X11)
    void send_net_wm_state(bool add, const char* first, const char* second) {
        if (!m_display || m_xwindow == 0) return;
        Atom wm_state = XInternAtom(m_display, "_NET_WM_STATE", False);
        Atom a1 = first ? XInternAtom(m_display, first, False) : None;
        Atom a2 = second ? XInternAtom(m_display, second, False) : None;

        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = m_xwindow;
        ev.xclient.message_type = wm_state;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = add ? 1 : 0;
        ev.xclient.data.l[1] = static_cast<long>(a1);
        ev.xclient.data.l[2] = static_cast<long>(a2);
        ev.xclient.data.l[3] = 1;
        XSendEvent(m_display, RootWindow(m_display, DefaultScreen(m_display)),
                   False, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
        XFlush(m_display);
    }
#endif

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
    Rect m_ime_cursor_rect{};
    bool m_ime_enabled{false};
    CursorShape m_cursor_shape{CursorShape::Arrow};

#if defined(KTUI_HAS_X11)
    Display* m_display{nullptr};
    Window m_xwindow{0};
    GC m_gc{nullptr};
    XIC m_xic{nullptr};
    std::vector<uint32_t> m_scratch_pixels;
    std::unordered_map<unsigned int, Cursor> m_cached_cursors;
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
        XInitThreads();
        XSetErrorHandler([](Display* dpy, XErrorEvent* ev) -> int {
            char buf[256];
            XGetErrorText(dpy, ev->error_code, buf, sizeof(buf));
            std::cerr << "[LinuxBackend X11 Protocol Warning] Code " << static_cast<int>(ev->error_code)
                      << " (" << buf << "), Request: " << static_cast<int>(ev->request_code)
                      << ", Minor: " << static_cast<int>(ev->minor_code) << std::endl;
            return 0;
        });
        XSetIOErrorHandler([](Display*) -> int {
            std::cerr << "[LinuxBackend X11 IO Warning] Display server disconnected." << std::endl;
            return 0;
        });
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

                // 创建专用隐藏辅助窗口以稳定管理 X11 系统剪贴板
                int screen = DefaultScreen(m_display);
                m_clip_window = XCreateSimpleWindow(m_display, RootWindow(m_display, screen),
                                                    0, 0, 1, 1, 0, 0, 0);
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
                    case ConfigureNotify: {
                        auto it = m_windows_map.find(ev.xconfigure.window);
                        if (it != m_windows_map.end() && it->second) {
                            Vec2 new_sz{static_cast<float>(ev.xconfigure.width), static_cast<float>(ev.xconfigure.height)};
                            Vec2 new_pos{static_cast<float>(ev.xconfigure.x), static_cast<float>(ev.xconfigure.y)};
                            it->second->notify_configure(new_sz, new_pos);
                        }
                        if (m_event_callback) {
                            PlatformEvent pe;
                            pe.type = PlatformEventType::WindowResize;
                            pe.window_handle = reinterpret_cast<void*>(ev.xconfigure.window);
                            pe.position = Vec2{static_cast<float>(ev.xconfigure.width), static_cast<float>(ev.xconfigure.height)};
                            m_event_callback(pe);
                        }
                        break;
                    }
                    case Expose: {
                        // 仅在最后一个 Expose 碎片到达时执行重绘，避免重复渲染
                        if (ev.xexpose.count == 0) {
                            auto it = m_windows_map.find(ev.xexpose.window);
                            if (it != m_windows_map.end() && it->second) {
                                it->second->invalidate();
                            }
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
                            // 滚轮事件映射 (Button 4/5 垂直, Button 6/7 水平)
                            if (ev.xbutton.button == 4 || ev.xbutton.button == 5 ||
                                ev.xbutton.button == 6 || ev.xbutton.button == 7) {
                                PlatformEvent pe;
                                pe.type = PlatformEventType::PointerScroll;
                                pe.window_handle = reinterpret_cast<void*>(ev.xbutton.window);
                                pe.position = PointF{static_cast<float>(ev.xbutton.x), static_cast<float>(ev.xbutton.y)};
                                float delta_y = (ev.xbutton.button == 4) ? -36.0f : (ev.xbutton.button == 5) ? 36.0f : 0.0f;
                                float delta_x = (ev.xbutton.button == 6) ? -36.0f : (ev.xbutton.button == 7) ? 36.0f : 0.0f;
                                pe.delta = Vec2{delta_x, delta_y};
                                m_event_callback(pe);
                                break;
                            }

                            // 鼠标捕获 (对标 Windows SetCapture)
                            if (ev.xbutton.button == 1) {
                                XGrabPointer(m_display, ev.xbutton.window, True,
                                             ButtonReleaseMask | PointerMotionMask,
                                             GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                            }

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
                        // 忽略鼠标滚轮的 Release 事件
                        if (ev.xbutton.button >= 4 && ev.xbutton.button <= 7) {
                            break;
                        }
                        // 释放鼠标捕获 (对标 Windows ReleaseCapture)
                        if (ev.xbutton.button == 1) {
                            XUngrabPointer(m_display, CurrentTime);
                        }

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
                        if (it != m_windows_map.end() && it->second) {
                            it->second->notify_focus(true);
                            if (it->second->get_xic()) {
                                XSetICFocus(it->second->get_xic());
                            }
                        }
                        break;
                    }
                    case FocusOut: {
                        auto it = m_windows_map.find(ev.xfocus.window);
                        if (it != m_windows_map.end() && it->second) {
                            it->second->notify_focus(false);
                            if (it->second->get_xic()) {
                                XUnsetICFocus(it->second->get_xic());
                            }
                        }
                        break;
                    }
                    case KeyPress: {
                        if (m_event_callback) {
                            std::vector<char> buf(128);
                            KeySym sym = NoSymbol;
                            Status status = 0;
                            int count = 0;

                            XIC xic = nullptr;
                            auto it = m_windows_map.find(ev.xkey.window);
                            if (it != m_windows_map.end() && it->second) {
                                xic = it->second->get_xic();
                            }

                            if (xic) {
                                count = Xutf8LookupString(xic, &ev.xkey, buf.data(), buf.size(), &sym, &status);
                                if (status == XBufferOverflow && count > 0) {
                                    buf.resize(count);
                                    count = Xutf8LookupString(xic, &ev.xkey, buf.data(), buf.size(), &sym, &status);
                                }
                                if (status != XLookupChars && status != XLookupBoth) count = 0;
                                if (status == XLookupChars) sym = NoSymbol;
                            } else {
                                count = XLookupString(&ev.xkey, buf.data(), buf.size(), &sym, nullptr);
                            }

                            uint32_t vk = x11_keysym_to_virtual_key(sym);
                            KeyModifiers mods = x11_state_to_modifiers(ev.xkey.state);

                            // 1. 发送 KeyDown 事件 (驱动退格、删除、箭头导航、回车提交及 Ctrl 快捷键)
                            PlatformEvent pe_down;
                            pe_down.type = PlatformEventType::KeyDown;
                            pe_down.window_handle = reinterpret_cast<void*>(ev.xkey.window);
                            pe_down.key_code = vk;
                            pe_down.modifiers = mods;
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
                                    pe_commit.text.assign(buf.data(), count);
                                    pe_commit.modifiers = mods;
                                    m_event_callback(pe_commit);
                                }
                            }
                        }
                        break;
                    }
                    case KeyRelease: {
                        // 过滤 X11 键盘长按产生的假 KeyRelease 事件 (Auto-repeat filter)
                        if (XEventsQueued(m_display, QueuedAfterReading)) {
                            XEvent next_ev;
                            XPeekEvent(m_display, &next_ev);
                            if (next_ev.type == KeyPress &&
                                next_ev.xkey.window == ev.xkey.window &&
                                next_ev.xkey.keycode == ev.xkey.keycode &&
                                next_ev.xkey.time == ev.xkey.time) {
                                break; // 忽略长按自动重发间隔中的虚假 KeyRelease
                            }
                        }

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
                            if (m_event_callback) {
                                PlatformEvent pe;
                                pe.type = PlatformEventType::WindowCloseRequested;
                                pe.window_handle = reinterpret_cast<void*>(ev.xclient.window);
                                m_event_callback(pe);
                            }
                        }
                        break;
                    }
                    case SelectionRequest: {
                        // X11 系统剪贴板服务响应 (提供跨进程复制)
                        XSelectionRequestEvent* req = &ev.xselectionrequest;
                        XEvent reply{};
                        reply.xselection.type = SelectionNotify;
                        reply.xselection.display = req->display;
                        reply.xselection.requestor = req->requestor;
                        reply.xselection.selection = req->selection;
                        reply.xselection.target = req->target;
                        reply.xselection.time = req->time;
                        reply.xselection.property = None;

                        Atom utf8_atom = XInternAtom(m_display, "UTF8_STRING", False);
                        Atom targets_atom = XInternAtom(m_display, "TARGETS", False);

                        if (req->target == targets_atom) {
                            Atom supported[] = { targets_atom, utf8_atom, XA_STRING };
                            XChangeProperty(m_display, req->requestor, req->property, XA_ATOM, 32,
                                            PropModeReplace, reinterpret_cast<const unsigned char*>(supported), 3);
                            reply.xselection.property = req->property;
                        } else if (req->target == utf8_atom || req->target == XA_STRING) {
                            std::lock_guard<std::mutex> lock(m_clip_mutex);
                            XChangeProperty(m_display, req->requestor, req->property, req->target, 8,
                                            PropModeReplace, reinterpret_cast<const unsigned char*>(m_clipboard.data()),
                                            static_cast<int>(m_clipboard.size()));
                            reply.xselection.property = req->property;
                        }
                        XSendEvent(m_display, req->requestor, False, 0, &reply);
                        XFlush(m_display);
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

    // 0% CPU 极低功耗事件等待 (对标 Windows MsgWaitForMultipleObjects)
    void wait_events(uint32_t timeout_ms = 0) override {
        m_dispatcher.process_tasks();
#if defined(KTUI_HAS_X11)
        if (m_display) {
            XFlush(m_display);
            if (XPending(m_display) == 0) {
                int fd = ConnectionNumber(m_display);
                struct pollfd pfd{};
                pfd.fd = fd;
                pfd.events = POLLIN;
                int timeout = (timeout_ms == 0) ? -1 : static_cast<int>(timeout_ms);
                poll(&pfd, 1, timeout);
            }
        } else {
            if (timeout_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
            }
        }
#else
        if (timeout_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        }
#endif
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

    // X11 系统剪贴板读取 (对标 Windows GetClipboardData)
    std::string get_clipboard_text() const override {
#if defined(KTUI_HAS_X11)
        if (m_display && (!m_windows_map.empty() || m_clip_window != 0)) {
            Atom clip_atom = XInternAtom(m_display, "CLIPBOARD", False);
            Window owner = XGetSelectionOwner(m_display, clip_atom);
            if (owner == None) {
                clip_atom = XA_PRIMARY;
                owner = XGetSelectionOwner(m_display, clip_atom);
            }
            if (owner != None) {
                // 如果是自身进程持有的剪贴板，直接快速返回
                if (owner == m_clip_window || m_windows_map.find(owner) != m_windows_map.end()) {
                    std::lock_guard<std::mutex> lock(m_clip_mutex);
                    return m_clipboard;
                }

                // 跨进程请求转换 UTF8_STRING
                Window req_win = (m_clip_window != 0) ? m_clip_window : m_windows_map.begin()->first;
                Atom utf8_atom = XInternAtom(m_display, "UTF8_STRING", False);
                Atom prop_atom = XInternAtom(m_display, "KTUI_CLIP_RECV", False);
                XConvertSelection(m_display, clip_atom, utf8_atom, prop_atom, req_win, CurrentTime);
                XFlush(m_display);

                // 等待 SelectionNotify 回复 (超时 150ms 保护)
                auto t0 = std::chrono::steady_clock::now();
                while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count() < 150) {
                    XEvent ev;
                    if (XCheckTypedWindowEvent(m_display, req_win, SelectionNotify, &ev)) {
                        if (ev.xselection.property != None) {
                            Atom actual_type;
                            int actual_format;
                            unsigned long nitems = 0, bytes_after = 0;
                            unsigned char* prop_data = nullptr;
                            if (XGetWindowProperty(m_display, req_win, prop_atom, 0, 1024 * 1024 / 4,
                                                   True, AnyPropertyType, &actual_type, &actual_format,
                                                   &nitems, &bytes_after, &prop_data) == Success) {
                                if (prop_data) {
                                    std::string res(reinterpret_cast<char*>(prop_data), nitems);
                                    XFree(prop_data);
                                    return res;
                                }
                            }
                        }
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
        }
#endif
        std::lock_guard<std::mutex> lock(m_clip_mutex);
        return m_clipboard;
    }

    // X11 系统剪贴板写入 (对标 Windows SetClipboardData)
    void set_clipboard_text(const std::string& utf8_text) override {
        {
            std::lock_guard<std::mutex> lock(m_clip_mutex);
            m_clipboard = utf8_text;
        }
#if defined(KTUI_HAS_X11)
        if (m_display) {
            Atom clip_atom = XInternAtom(m_display, "CLIPBOARD", False);
            Window owner = (m_clip_window != 0) ? m_clip_window : 
                           (!m_windows_map.empty() ? m_windows_map.begin()->first : None);
            if (owner != None) {
                XSetSelectionOwner(m_display, clip_atom, owner, CurrentTime);
                XSetSelectionOwner(m_display, XA_PRIMARY, owner, CurrentTime);
                XFlush(m_display);
            }
        }
#endif
    }

    void set_event_callback(std::function<void(const PlatformEvent&)> callback) override {
        m_event_callback = std::move(callback);
    }

    void shutdown() override {
#if defined(KTUI_HAS_X11)
        if (m_clip_window != 0 && m_display) {
            XDestroyWindow(m_display, m_clip_window);
            m_clip_window = 0;
        }
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
    Window m_clip_window{0};
    std::unordered_map<Window, LinuxWindow*> m_windows_map;
#endif

    mutable std::mutex m_clip_mutex;
    std::string m_clipboard;
    std::function<void(const PlatformEvent&)> m_event_callback{nullptr};
};

} // namespace ktui
