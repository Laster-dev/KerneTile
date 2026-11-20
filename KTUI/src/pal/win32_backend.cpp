#if defined(_WIN32)

#include "win32_backend.hpp"
#include <algorithm>

namespace ktui {

Win32Backend* Win32Backend::s_instance = nullptr;
static const wchar_t* kWin32ClassName = L"KTUI_Window_Class";

// ==================== Win32Window ====================

Win32Window::Win32Window(const WindowDesc& desc, HINSTANCE hinstance)
    : m_hinstance(hinstance),
      m_title(desc.title),
      m_size(desc.size),
      m_min_size(desc.min_size),
      m_max_size(desc.max_size),
      m_position(desc.position),
      m_always_on_top(desc.always_on_top),
      m_resizable(desc.resizable),
      m_decorated(desc.decorated) {

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!m_resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    if (!m_decorated) {
        style = WS_POPUP | WS_THICKFRAME;
    }

    DWORD ex_style = WS_EX_APPWINDOW;
    if (m_always_on_top) {
        ex_style |= WS_EX_TOPMOST;
    }
    if (desc.type == WindowType::ToolWindow) {
        ex_style = WS_EX_TOOLWINDOW;
    } else if (desc.type == WindowType::Popup) {
        style = WS_POPUP;
        ex_style = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    }

    int x = (m_position.x < 0.0f) ? CW_USEDEFAULT : static_cast<int>(m_position.x);
    int y = (m_position.y < 0.0f) ? CW_USEDEFAULT : static_cast<int>(m_position.y);
    int w = static_cast<int>(m_size.x);
    int h = static_cast<int>(m_size.y);

    std::wstring wtitle(m_title.begin(), m_title.end());

    m_hwnd = CreateWindowExW(
        ex_style,
        kWin32ClassName,
        wtitle.c_str(),
        style,
        x, y, w, h,
        static_cast<HWND>(desc.parent_handle),
        nullptr,
        m_hinstance,
        this
    );

    if (m_hwnd) {
        RECT client_rect;
        GetClientRect(m_hwnd, &client_rect);
        m_size = Vec2{static_cast<float>(client_rect.right - client_rect.left),
                      static_cast<float>(client_rect.bottom - client_rect.top)};
        
        HDC hdc = GetDC(m_hwnd);
        if (hdc) {
            int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            m_dpi_scale = static_cast<float>(dpi) / 96.0f;
            m_refresh_rate = static_cast<uint32_t>(GetDeviceCaps(hdc, VREFRESH));
            if (m_refresh_rate <= 1) m_refresh_rate = 60;
            ReleaseDC(m_hwnd, hdc);
        }
    }
}

Win32Window::~Win32Window() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void Win32Window::set_size(Vec2 new_size) {
    if (!m_hwnd) return;
    float w = std::clamp(new_size.x, m_min_size.x, m_max_size.x);
    float h = std::clamp(new_size.y, m_min_size.y, m_max_size.y);
    SetWindowPos(m_hwnd, nullptr, 0, 0, static_cast<int>(w), static_cast<int>(h),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32Window::set_position(Vec2 pos) {
    if (!m_hwnd) return;
    SetWindowPos(m_hwnd, nullptr, static_cast<int>(pos.x), static_cast<int>(pos.y),
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32Window::center_on_screen() {
    if (!m_hwnd) return;
    HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (GetMonitorInfoW(hmon, &mi)) {
        int mon_w = mi.rcWork.right - mi.rcWork.left;
        int mon_h = mi.rcWork.bottom - mi.rcWork.top;
        int win_w = static_cast<int>(m_size.x);
        int win_h = static_cast<int>(m_size.y);
        int x = mi.rcWork.left + (mon_w - win_w) / 2;
        int y = mi.rcWork.top + (mon_h - win_h) / 2;
        set_position(Vec2{static_cast<float>(x), static_cast<float>(y)});
    }
}

Rect Win32Window::screen_bounds() const {
    if (!m_hwnd) return Rect{0, 0, 0, 0};
    RECT r;
    GetWindowRect(m_hwnd, &r);
    return Rect{static_cast<float>(r.left), static_cast<float>(r.top),
                static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top)};
}

void Win32Window::show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
}

void Win32Window::hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

bool Win32Window::is_visible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void Win32Window::minimize() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_MINIMIZE);
}

void Win32Window::maximize() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_MAXIMIZE);
}

void Win32Window::restore() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_RESTORE);
}

bool Win32Window::is_minimized() const {
    return m_hwnd && IsIconic(m_hwnd);
}

bool Win32Window::is_maximized() const {
    return m_hwnd && IsZoomed(m_hwnd);
}

void Win32Window::set_fullscreen(bool fullscreen) {
    if (!m_hwnd || m_fullscreen == fullscreen) return;
    m_fullscreen = fullscreen;

    DWORD style = GetWindowLongW(m_hwnd, GWL_STYLE);
    if (fullscreen) {
        GetWindowRect(m_hwnd, &m_normal_rect);
        HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(hmon, &mi);
        SetWindowLongW(m_hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(m_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLongW(m_hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPos(m_hwnd, nullptr, m_normal_rect.left, m_normal_rect.top,
                     m_normal_rect.right - m_normal_rect.left,
                     m_normal_rect.bottom - m_normal_rect.top,
                     SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

void Win32Window::activate() {
    if (m_hwnd) {
        SetForegroundWindow(m_hwnd);
    }
}

bool Win32Window::is_active() const {
    return m_hwnd && GetForegroundWindow() == m_hwnd;
}

void Win32Window::focus() {
    if (m_hwnd) SetFocus(m_hwnd);
}

bool Win32Window::has_focus() const {
    return m_hwnd && GetFocus() == m_hwnd;
}

void Win32Window::set_always_on_top(bool always_on_top) {
    if (!m_hwnd) return;
    m_always_on_top = always_on_top;
    SetWindowPos(m_hwnd, always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void Win32Window::flash_taskbar(bool flash) {
    if (!m_hwnd) return;
    FLASHWINFO fi{sizeof(fi)};
    fi.hwnd = m_hwnd;
    fi.dwFlags = flash ? (FLASHW_ALL | FLASHW_TIMER) : FLASHW_STOP;
    fi.uCount = 3;
    FlashWindowEx(&fi);
}

void Win32Window::set_title(const std::string& title) {
    if (!m_hwnd) return;
    m_title = title;
    std::wstring wtitle(title.begin(), title.end());
    SetWindowTextW(m_hwnd, wtitle.c_str());
}

void Win32Window::set_icon(const std::string& /*icon_path*/) {
    // 可选实现图标加载
}

void Win32Window::set_resizable(bool resizable) {
    if (!m_hwnd || m_resizable == resizable) return;
    m_resizable = resizable;
    update_window_style();
}

void Win32Window::set_opacity(float opacity) {
    if (!m_hwnd) return;
    m_opacity = std::clamp(opacity, 0.0f, 1.0f);
    DWORD ex_style = GetWindowLongW(m_hwnd, GWL_EXSTYLE);
    if (m_opacity < 1.0f) {
        SetWindowLongW(m_hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
        BYTE alpha = static_cast<BYTE>(m_opacity * 255.0f);
        SetLayeredWindowAttributes(m_hwnd, 0, alpha, LWA_ALPHA);
    } else {
        SetWindowLongW(m_hwnd, GWL_EXSTYLE, ex_style & ~WS_EX_LAYERED);
    }
}

void Win32Window::set_decorated(bool decorated) {
    if (!m_hwnd || m_decorated == decorated) return;
    m_decorated = decorated;
    update_window_style();
}

void Win32Window::update_window_style() {
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!m_resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    if (!m_decorated) {
        style = WS_POPUP | WS_THICKFRAME;
    }
    SetWindowLongW(m_hwnd, GWL_STYLE, style);
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void Win32Window::close() {
    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    }
}

LRESULT Win32Window::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_SIZE: {
            UINT width = LOWORD(lparam);
            UINT height = HIWORD(lparam);
            m_size = Vec2{static_cast<float>(width), static_cast<float>(height)};
            if (m_on_resized) {
                m_on_resized(m_size);
            }
            return 0;
        }
        case WM_MOVE: {
            int x = static_cast<short>(LOWORD(lparam));
            int y = static_cast<short>(HIWORD(lparam));
            m_position = Vec2{static_cast<float>(x), static_cast<float>(y)};
            if (m_on_moved) {
                m_on_moved(m_position);
            }
            return 0;
        }
        case WM_CLOSE: {
            if (m_on_close) {
                if (!m_on_close()) {
                    return 0; // 拦截阻止关闭
                }
            }
            break;
        }
        case WM_DPICHANGED: {
            m_dpi_scale = static_cast<float>(LOWORD(wparam)) / 96.0f;
            RECT* prc = reinterpret_cast<RECT*>(lparam);
            if (prc) {
                SetWindowPos(m_hwnd, nullptr, prc->left, prc->top,
                             prc->right - prc->left, prc->bottom - prc->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            if (m_on_dpi_changed) {
                m_on_dpi_changed(m_dpi_scale);
            }
            return 0;
        }
        case WM_SETFOCUS: {
            if (m_on_focus_changed) m_on_focus_changed(true);
            return 0;
        }
        case WM_KILLFOCUS: {
            if (m_on_focus_changed) m_on_focus_changed(false);
            return 0;
        }
        case WM_NCHITTEST: {
            if (!m_decorated && m_titlebar_hit_test) {
                POINT pt{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
                ScreenToClient(m_hwnd, &pt);
                TitlebarHitTest ht = m_titlebar_hit_test(PointF{static_cast<float>(pt.x), static_cast<float>(pt.y)});
                switch (ht) {
                    case TitlebarHitTest::Caption: return HTCAPTION;
                    case TitlebarHitTest::CloseBtn: return HTCLOSE;
                    case TitlebarHitTest::MaximizeBtn: return HTMAXBUTTON;
                    case TitlebarHitTest::MinimizeBtn: return HTMINBUTTON;
                    case TitlebarHitTest::BorderLeft: return HTLEFT;
                    case TitlebarHitTest::BorderRight: return HTRIGHT;
                    case TitlebarHitTest::BorderTop: return HTTOP;
                    case TitlebarHitTest::BorderBottom: return HTBOTTOM;
                    case TitlebarHitTest::BorderTopLeft: return HTTOPLEFT;
                    case TitlebarHitTest::BorderTopRight: return HTTOPRIGHT;
                    case TitlebarHitTest::BorderBottomLeft: return HTBOTTOMLEFT;
                    case TitlebarHitTest::BorderBottomRight: return HTBOTTOMRIGHT;
                    default: return HTCLIENT;
                }
            }
            break;
        }
        default:
            break;
    }
    return DefWindowProcW(m_hwnd, msg, wparam, lparam);
}

// ==================== Win32Backend ====================

Win32Backend::Win32Backend() {
    s_instance = this;
    m_hinstance = GetModuleHandleW(nullptr);
}

Win32Backend::~Win32Backend() {
    shutdown();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool Win32Backend::init() {
    if (m_class_registered) return true;

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = static_wnd_proc;
    wc.hInstance = m_hinstance;
    wc.lpszClassName = kWin32ClassName;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = nullptr; // 自绘背景，无闪烁
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        return false;
    }
    m_class_registered = true;
    return true;
}

std::unique_ptr<IPlatformWindow> Win32Backend::create_window(const WindowDesc& desc) {
    if (!m_class_registered) {
        init();
    }
    auto win = std::make_unique<Win32Window>(desc, m_hinstance);
    if (win->hwnd()) {
        m_window_map[win->hwnd()] = win.get();
    }
    return win;
}

void Win32Backend::poll_events() {
    m_dispatcher.process_tasks();

    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Win32Backend::wait_events(uint32_t timeout_ms) {
    m_dispatcher.process_tasks();

    if (timeout_ms == 0) {
        MSG msg;
        if (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    } else {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout_ms, QS_ALLINPUT);
        poll_events();
    }
}

void Win32Backend::dispatch_platform_event(const PlatformEvent& event) {
    if (m_event_callback) {
        m_event_callback(event);
    }
}

void Win32Backend::shutdown() {
    m_window_map.clear();
    if (m_class_registered) {
        UnregisterClassW(kWin32ClassName, m_hinstance);
        m_class_registered = false;
    }
}

LRESULT CALLBACK Win32Backend::static_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (s_instance) {
        auto it = s_instance->m_window_map.find(hwnd);
        if (it != s_instance->m_window_map.end() && it->second) {
            // 处理统一输入事件转发
            PlatformEvent pe{};
            pe.timestamp_ms = GetTickCount64();
            switch (msg) {
                case WM_MOUSEMOVE:
                    pe.type = PlatformEventType::PointerMove;
                    pe.position = Vec2{static_cast<float>(static_cast<short>(LOWORD(lparam))),
                                       static_cast<float>(static_cast<short>(HIWORD(lparam)))};
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_LBUTTONDOWN:
                    pe.type = PlatformEventType::PointerDown;
                    pe.button = PointerButton::Primary;
                    pe.position = Vec2{static_cast<float>(static_cast<short>(LOWORD(lparam))),
                                       static_cast<float>(static_cast<short>(HIWORD(lparam)))};
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_LBUTTONUP:
                    pe.type = PlatformEventType::PointerUp;
                    pe.button = PointerButton::Primary;
                    pe.position = Vec2{static_cast<float>(static_cast<short>(LOWORD(lparam))),
                                       static_cast<float>(static_cast<short>(HIWORD(lparam)))};
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_KEYDOWN:
                    pe.type = PlatformEventType::KeyDown;
                    pe.key_code = static_cast<uint32_t>(wparam);
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_KEYUP:
                    pe.type = PlatformEventType::KeyUp;
                    pe.key_code = static_cast<uint32_t>(wparam);
                    s_instance->dispatch_platform_event(pe);
                    break;
                default:
                    break;
            }
            return it->second->handle_message(msg, wparam, lparam);
        }
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace ktui

#endif // defined(_WIN32)
