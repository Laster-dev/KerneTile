#if defined(_WIN32)

#include "win32_backend.hpp"
#include <ktui/presentation/mica_backdrop.hpp>
#include <imm.h>
#include <algorithm>
#include <cmath>

namespace ktui {

namespace {
struct DwmMargins { int left, right, top, bottom; };
using DwmExtendFrameFn = HRESULT(WINAPI*)(HWND, const DwmMargins*);
using DwmSetAttrFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
using DwmDefWndProcFn = BOOL(WINAPI*)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

float query_window_dpi_scale(HWND hwnd) {
    if (hwnd) {
        static auto get_dpi_for_window = reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
        if (get_dpi_for_window) {
            UINT dpi = get_dpi_for_window(hwnd);
            if (dpi >= 48 && dpi <= 960) {
                return static_cast<float>(dpi) / 96.0f;
            }
        }
        HDC hdc = GetDC(hwnd);
        if (hdc) {
            int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hwnd, hdc);
            if (dpi >= 48 && dpi <= 960) {
                return static_cast<float>(dpi) / 96.0f;
            }
        }
    }
    return 1.0f;
}

HMODULE get_dwm_module() {
    static HMODULE s_dwm = LoadLibraryW(L"dwmapi.dll");
    return s_dwm;
}

bool call_dwm_def_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* out_result) {
    if (HMODULE dwm = get_dwm_module()) {
        static auto fn = reinterpret_cast<DwmDefWndProcFn>(GetProcAddress(dwm, "DwmDefWindowProc"));
        if (fn) {
            return fn(hwnd, msg, wparam, lparam, out_result) != FALSE;
        }
    }
    return false;
}

void apply_frameless_dwm_chrome(HWND hwnd, bool maximized, bool dark_mode_enabled) {
    if (!hwnd) return;
    HMODULE dwm = get_dwm_module();
    if (!dwm) return;
    if (auto set_attr = reinterpret_cast<DwmSetAttrFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"))) {
        constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;
        constexpr DWORD kDwmwaWindowCornerPreference = 33;
        constexpr DWORD kDwmwaBorderColor = 34;
        constexpr DWORD kDwmwcpDoNotRound = 1;
        constexpr DWORD kDwmwcpRound = 2;
        constexpr COLORREF kDwmwaColorNone = 0xFFFFFFFE;
        BOOL dark_mode = dark_mode_enabled ? TRUE : FALSE;
        set_attr(hwnd, kDwmwaUseImmersiveDarkMode, &dark_mode, sizeof(dark_mode));
        DWORD pref = maximized ? kDwmwcpDoNotRound : kDwmwcpRound;
        set_attr(hwnd, kDwmwaWindowCornerPreference, &pref, sizeof(pref));
        set_attr(hwnd, kDwmwaBorderColor, &kDwmwaColorNone, sizeof(kDwmwaColorNone));
    }
    if (auto extend_fn = reinterpret_cast<DwmExtendFrameFn>(GetProcAddress(dwm, "DwmExtendFrameIntoClientArea"))) {
        // 对齐 CUI.Core Window::UpdateDwmChrome():
        // 非最大化时仅延伸底部 1px {0,0,0,1} 保留 DWM 原生阴影，避免顶部延伸触发 Win10 Aero Caption 裁切
        DwmMargins margins = maximized ? DwmMargins{0, 0, 0, 0} : DwmMargins{0, 0, 0, 1};
        extend_fn(hwnd, &margins);
    }
    SetWindowRgn(hwnd, nullptr, FALSE);
}
} // namespace

Win32Backend* Win32Backend::s_instance = nullptr;
static const wchar_t* kWin32ClassName = L"KTUI_Window_Class";

static std::wstring utf8_to_wstring(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &out[0], len);
    return out;
}

static std::string wstring_to_utf8(std::wstring_view wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), &out[0], len, nullptr, nullptr);
    return out;
}

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

    // 对齐 CUI.Core 窗口原理：保留 WS_OVERLAPPEDWINDOW (含 WS_CAPTION)，让 DWM 识别为标准顶层应用窗口，
    // 从而原生拥有系统最小化/最大化/还原过渡动画与 Win11 Snap Layout；非客户区由 WM_NCCALCSIZE 返回 0 接管。
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!m_resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
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
    if (m_decorated) {
        RECT adj{0, 0, w, h};
        AdjustWindowRectEx(&adj, style, FALSE, ex_style);
        w = adj.right - adj.left;
        h = adj.bottom - adj.top;
    }

    std::wstring wtitle = utf8_to_wstring(m_title);

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
        m_dpi_scale = query_window_dpi_scale(m_hwnd);
        HDC hdc = GetDC(m_hwnd);
        if (hdc) {
            m_refresh_rate = static_cast<uint32_t>(GetDeviceCaps(hdc, VREFRESH));
            if (m_refresh_rate <= 1) m_refresh_rate = 60;
            ReleaseDC(m_hwnd, hdc);
        }

        int phys_w = std::max(1, static_cast<int>(std::lround(desc.size.x * m_dpi_scale)));
        int phys_h = std::max(1, static_cast<int>(std::lround(desc.size.y * m_dpi_scale)));
        if (m_decorated) {
            RECT adj{0, 0, phys_w, phys_h};
            AdjustWindowRectEx(&adj, style, FALSE, ex_style);
            phys_w = adj.right - adj.left;
            phys_h = adj.bottom - adj.top;
        }

        int pos_x = x;
        int pos_y = y;
        if (m_position.x < 0.0f && m_position.y < 0.0f) {
            HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{sizeof(mi)};
            if (GetMonitorInfoW(hmon, &mi)) {
                pos_x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - phys_w) / 2;
                pos_y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - phys_h) / 2;
            }
        }
        const UINT swp_flags = SWP_NOZORDER | SWP_NOACTIVATE |
            ((m_position.x < 0.0f && m_position.y < 0.0f && m_decorated) ? SWP_NOMOVE : 0) |
            (!m_decorated ? SWP_FRAMECHANGED : 0);
        SetWindowPos(m_hwnd, nullptr, pos_x, pos_y, phys_w, phys_h, swp_flags);
        if (!m_decorated) {
            update_dwm_chrome();
        }

        RECT client_rect;
        GetClientRect(m_hwnd, &client_rect);
        m_size = Vec2{static_cast<float>(client_rect.right - client_rect.left) / m_dpi_scale,
                      static_cast<float>(client_rect.bottom - client_rect.top) / m_dpi_scale};
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
    int phys_w = std::max(1, static_cast<int>(std::lround(w * m_dpi_scale)));
    int phys_h = std::max(1, static_cast<int>(std::lround(h * m_dpi_scale)));
    if (m_decorated) {
        RECT adj{0, 0, phys_w, phys_h};
        DWORD style = static_cast<DWORD>(GetWindowLongW(m_hwnd, GWL_STYLE));
        DWORD ex_style = static_cast<DWORD>(GetWindowLongW(m_hwnd, GWL_EXSTYLE));
        AdjustWindowRectEx(&adj, style, FALSE, ex_style);
        phys_w = adj.right - adj.left;
        phys_h = adj.bottom - adj.top;
    }
    SetWindowPos(m_hwnd, nullptr, 0, 0, phys_w, phys_h,
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
        RECT wr{};
        GetWindowRect(m_hwnd, &wr);
        int win_w = wr.right - wr.left;
        int win_h = wr.bottom - wr.top;
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

Rect Win32Window::monitor_uv_rect() const {
    if (!m_hwnd) return Rect::from_xywh(0.0f, 0.0f, 1.0f, 1.0f);
    POINT pt_client{0, 0};
    ClientToScreen(m_hwnd, &pt_client);
    RECT cr{};
    GetClientRect(m_hwnd, &cr);
    const float cw = static_cast<float>(std::max<LONG>(1, cr.right - cr.left));
    const float ch = static_cast<float>(std::max<LONG>(1, cr.bottom - cr.top));

    HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfoW(hmon, &mi)) {
        mi.rcMonitor = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    }
    const float mw = static_cast<float>(std::max<LONG>(1, mi.rcMonitor.right - mi.rcMonitor.left));
    const float mh = static_cast<float>(std::max<LONG>(1, mi.rcMonitor.bottom - mi.rcMonitor.top));
    const float rel_x = static_cast<float>(pt_client.x - mi.rcMonitor.left);
    const float rel_y = static_cast<float>(pt_client.y - mi.rcMonitor.top);
    return Rect::from_xywh(rel_x / mw, rel_y / mh, cw / mw, ch / mh);
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
    if (m_hwnd) SendMessageW(m_hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

void Win32Window::maximize() {
    if (m_hwnd) SendMessageW(m_hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
}

void Win32Window::restore() {
    if (m_hwnd) SendMessageW(m_hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
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
    std::wstring wtitle = utf8_to_wstring(title);
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

void Win32Window::update_dwm_chrome() {
    if (!m_hwnd || m_decorated) return;
    const bool maximized = IsZoomed(m_hwnd) != FALSE;
    m_last_maximized = maximized;
    apply_frameless_dwm_chrome(m_hwnd, maximized, true);
}

void Win32Window::update_window_style() {
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!m_resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    SetWindowLongW(m_hwnd, GWL_STYLE, style);
    if (!m_decorated) {
        update_dwm_chrome();
    }
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void Win32Window::present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) {
    present_rgba8_region(rgba, width, height, 0, 0, width, height);
}

void Win32Window::present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                                       uint32_t dirty_x, uint32_t dirty_y,
                                       uint32_t dirty_w, uint32_t dirty_h) {
    if (!m_hwnd || !rgba || width == 0 || height == 0 || dirty_w == 0 || dirty_h == 0) return;

    HDC hdc = GetDC(m_hwnd);
    if (!hdc) return;

    struct {
        BITMAPINFOHEADER bmiHeader;
        DWORD bmiColors[3];
    } bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(width);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(height); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_BITFIELDS;
    bmi.bmiColors[0] = 0x000000FF; // Red
    bmi.bmiColors[1] = 0x0000FF00; // Green
    bmi.bmiColors[2] = 0x00FF0000; // Blue

    // 针对局部脏区极速拷贝：ySrc 计算严格依据 top-down DIB 规范: height - dirty_y - dirty_h
    int ySrc = static_cast<int>(height) - static_cast<int>(dirty_y) - static_cast<int>(dirty_h);
    SetDIBitsToDevice(hdc, dirty_x, dirty_y, dirty_w, dirty_h,
                      dirty_x, ySrc, 0, height,
                      rgba, reinterpret_cast<const BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
    ReleaseDC(m_hwnd, hdc);
}

void Win32Window::invalidate() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Win32Window::set_ime_enabled(bool enabled) {
    m_ime_enabled = enabled;
    if (!m_hwnd) return;
    ImmAssociateContextEx(m_hwnd, nullptr, enabled ? IACE_DEFAULT : 0);
    if (enabled) {
        set_ime_cursor_rect(m_ime_cursor_rect);
    }
}

void Win32Window::set_ime_cursor_rect(Rect cursor_rect) {
    m_ime_cursor_rect = cursor_rect;
    if (!m_hwnd || !m_ime_enabled) return;

    HIMC himc = ImmGetContext(m_hwnd);
    if (himc) {
        const float s = m_dpi_scale;
        COMPOSITIONFORM cf{};
        cf.dwStyle = CFS_POINT;
        cf.ptCurrentPos.x = static_cast<LONG>(std::lround(cursor_rect.min_x * s));
        cf.ptCurrentPos.y = static_cast<LONG>(std::lround(cursor_rect.min_y * s));
        ImmSetCompositionWindow(himc, &cf);

        CANDIDATEFORM cand{};
        cand.dwIndex = 0;
        cand.dwStyle = CFS_EXCLUDE;
        cand.ptCurrentPos.x = static_cast<LONG>(std::lround(cursor_rect.min_x * s));
        cand.ptCurrentPos.y = static_cast<LONG>(std::lround(cursor_rect.max_y * s));
        cand.rcArea.left = static_cast<LONG>(std::lround(cursor_rect.min_x * s));
        cand.rcArea.top = static_cast<LONG>(std::lround(cursor_rect.min_y * s));
        cand.rcArea.right = static_cast<LONG>(std::lround(cursor_rect.max_x * s));
        cand.rcArea.bottom = static_cast<LONG>(std::lround((cursor_rect.max_y + 4.0f) * s));
        ImmSetCandidateWindow(himc, &cand);

        ImmReleaseContext(m_hwnd, himc);
    }
}

static HCURSOR load_win32_cursor(CursorShape shape) {
    WORD id = 32512; // IDC_ARROW
    switch (shape) {
        case CursorShape::IBeam:        id = 32513; break; // IDC_IBEAM
        case CursorShape::PointingHand: id = 32649; break; // IDC_HAND
        case CursorShape::ResizeEW:     id = 32644; break; // IDC_SIZEWE
        case CursorShape::ResizeNS:     id = 32645; break; // IDC_SIZENS
        case CursorShape::ResizeNWSE:   id = 32642; break; // IDC_SIZENWSE
        case CursorShape::ResizeNESW:   id = 32643; break; // IDC_SIZENESW
        case CursorShape::Crosshair:    id = 32515; break; // IDC_CROSS
        case CursorShape::Forbidden:    id = 32648; break; // IDC_NO
        case CursorShape::Arrow:
        default:                        id = 32512; break; // IDC_ARROW
    }
    return LoadCursorW(nullptr, MAKEINTRESOURCEW(id));
}

void Win32Window::set_cursor(CursorShape cursor) {
    if (m_cursor_shape == cursor) return;
    m_cursor_shape = cursor;
    SetCursor(load_win32_cursor(m_cursor_shape));
}

void Win32Window::close() {
    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    }
}

LRESULT Win32Window::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1; // 禁止 GDI 擦除背景防止闪烁

        case WM_NCACTIVATE:
            if (!m_decorated) {
                return TRUE; // 对齐 CUI.Core：禁止系统绘制原生非客户区激活标题栏
            }
            break;

        case WM_NCPAINT:
            if (!m_decorated) {
                return 0; // 对齐 CUI.Core：禁止系统绘制原生非客户区边框
            }
            break;

        case WM_SYNCPAINT:
            if (!m_decorated) {
                return 0;
            }
            break;

        case WM_SETCURSOR: {
            if (LOWORD(lparam) == HTCLIENT) {
                SetCursor(load_win32_cursor(m_cursor_shape));
                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(m_hwnd, &ps);
            EndPaint(m_hwnd, &ps);
            if (m_on_paint) {
                m_on_paint();
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
            SetCapture(m_hwnd);
            return 0;

        case WM_LBUTTONUP:
            ReleaseCapture();
            return 0;

        case WM_IME_STARTCOMPOSITION: {
            set_ime_cursor_rect(m_ime_cursor_rect);
            return 0; // 由 KTUI 控件自行绘制预编辑下划线，不显示系统默认悬浮小黑框
        }

        case WM_IME_COMPOSITION: {
            HIMC himc = ImmGetContext(m_hwnd);
            if (himc) {
                if (lparam & (GCS_COMPSTR | GCS_CURSORPOS)) {
                    LONG bytes = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
                    std::string utf8_comp;
                    size_t cursor_byte = std::string::npos;
                    if (bytes > 0) {
                        std::wstring wide(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
                        ImmGetCompositionStringW(himc, GCS_COMPSTR, &wide[0], static_cast<DWORD>(bytes));
                        utf8_comp = wstring_to_utf8(wide);
                        const LONG cursor = ImmGetCompositionStringW(himc, GCS_CURSORPOS, nullptr, 0);
                        if (cursor >= 0 && static_cast<size_t>(cursor) <= wide.size()) {
                            cursor_byte = wstring_to_utf8(std::wstring_view(wide).substr(0, cursor)).size();
                        }
                    }
                    if (Win32Backend::instance()) {
                        PlatformEvent pe{};
                        pe.type = PlatformEventType::ImeComposition;
                        pe.window_handle = static_cast<void*>(m_hwnd);
                        pe.text = std::move(utf8_comp);
                        pe.ime_cursor_byte = cursor_byte;
                        pe.timestamp_ms = GetTickCount64();
                        Win32Backend::instance()->dispatch_platform_event(pe);
                    }
                }
                if (lparam & GCS_RESULTSTR) {
                    LONG bytes = ImmGetCompositionStringW(himc, GCS_RESULTSTR, nullptr, 0);
                    if (bytes > 0) {
                        std::wstring wide(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
                        ImmGetCompositionStringW(himc, GCS_RESULTSTR, &wide[0], static_cast<DWORD>(bytes));
                        std::string utf8_res = wstring_to_utf8(wide);
                        if (Win32Backend::instance() && !utf8_res.empty()) {
                            PlatformEvent pe{};
                            pe.type = PlatformEventType::ImeCommit;
                            pe.window_handle = static_cast<void*>(m_hwnd);
                            pe.text = std::move(utf8_res);
                            pe.timestamp_ms = GetTickCount64();
                            Win32Backend::instance()->dispatch_platform_event(pe);
                        }
                    }
                }
                ImmReleaseContext(m_hwnd, himc);
            }
            return 0;
        }

        case WM_IME_ENDCOMPOSITION: {
            if (Win32Backend::instance()) {
                PlatformEvent pe{};
                pe.type = PlatformEventType::ImeComposition;
                pe.window_handle = static_cast<void*>(m_hwnd);
                pe.text = "";
                pe.timestamp_ms = GetTickCount64();
                Win32Backend::instance()->dispatch_platform_event(pe);
            }
            return 0;
        }

        case WM_IME_CHAR:
            // GCS_RESULTSTR already delivered the committed text.
            return 0;

        case WM_CHAR: {
            const auto ch = static_cast<wchar_t>(wparam);
            if (ch >= 0xd800 && ch <= 0xdbff) {
                m_pending_high_surrogate = ch;
                return 0;
            }
            const auto high = std::exchange(m_pending_high_surrogate, wchar_t{});
            if (ch >= 32 && ch != 127) {
                std::wstring ws;
                if (ch >= 0xdc00 && ch <= 0xdfff) {
                    if (!high) return 0;
                    ws.push_back(high);
                }
                ws.push_back(ch);
                std::string utf8 = wstring_to_utf8(ws);
                if (Win32Backend::instance() && !utf8.empty()) {
                    PlatformEvent pe{};
                    pe.type = PlatformEventType::ImeCommit;
                    pe.window_handle = static_cast<void*>(m_hwnd);
                    pe.key_code = static_cast<uint32_t>(ch);
                    pe.text = std::move(utf8);
                    pe.timestamp_ms = GetTickCount64();
                    Win32Backend::instance()->dispatch_platform_event(pe);
                }
            }
            return 0;
        }

        case WM_SIZE: {
            UINT width = LOWORD(lparam);
            UINT height = HIWORD(lparam);
            const float inv_dpi = (m_dpi_scale > 0.0f) ? (1.0f / m_dpi_scale) : 1.0f;
            m_size = Vec2{static_cast<float>(width) * inv_dpi, static_cast<float>(height) * inv_dpi};
            if (!m_decorated) {
                const bool maximized = IsZoomed(m_hwnd) != FALSE;
                if (maximized != m_last_maximized) {
                    update_dwm_chrome();
                }
            }
            if (m_on_resized) {
                m_on_resized(m_size);
            }
            if (wparam != SIZE_MINIMIZED && width > 0 && height > 0 && m_on_paint) {
                m_on_paint();
                ValidateRect(m_hwnd, nullptr);
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
            if (!IsIconic(m_hwnd) && m_on_paint) {
                m_on_paint();
            }
            return 0;
        }
        case WM_SETTINGCHANGE: {
            if (wparam == SPI_SETDESKWALLPAPER) {
                MicaBackdropEngine::invalidate_wallpaper();
                if (!IsIconic(m_hwnd) && m_on_paint) {
                    m_on_paint();
                }
            }
            break;
        }
        case WM_THEMECHANGED: {
            MicaBackdropEngine::invalidate_wallpaper();
            if (!IsIconic(m_hwnd) && m_on_paint) {
                m_on_paint();
            }
            break;
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
            const float next_dpi = static_cast<float>(LOWORD(wparam)) / 96.0f;
            if (next_dpi > 0.1f) {
                m_dpi_scale = next_dpi;
            }
            RECT* prc = reinterpret_cast<RECT*>(lparam);
            if (prc) {
                SetWindowPos(m_hwnd, nullptr, prc->left, prc->top,
                             prc->right - prc->left, prc->bottom - prc->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            RECT client_rect{};
            if (GetClientRect(m_hwnd, &client_rect)) {
                m_size = Vec2{static_cast<float>(client_rect.right - client_rect.left) / m_dpi_scale,
                              static_cast<float>(client_rect.bottom - client_rect.top) / m_dpi_scale};
            }
            if (m_on_dpi_changed) {
                m_on_dpi_changed(m_dpi_scale);
            }
            if (m_on_paint && !IsIconic(m_hwnd)) {
                m_on_paint();
                ValidateRect(m_hwnd, nullptr);
            }
            return 0;
        }
        case WM_SETFOCUS: {
            if (m_on_focus_changed) m_on_focus_changed(true);
            return 0;
        }
        case WM_KILLFOCUS: {
            m_pending_high_surrogate = 0;
            if (m_on_focus_changed) m_on_focus_changed(false);
            return 0;
        }
        case WM_NCCALCSIZE: {
            if (!m_decorated) {
                if (IsIconic(m_hwnd)) {
                    return DefWindowProcW(m_hwnd, msg, wparam, lparam);
                }
                if (wparam == TRUE) {
                    if (IsZoomed(m_hwnd)) {
                        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
                        HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
                        MONITORINFO mi{sizeof(mi)};
                        if (GetMonitorInfoW(hmon, &mi)) {
                            params->rgrc[0] = mi.rcWork;
                        }
                    }
                    return 0;
                }
                return 0;
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
            const float s = (m_dpi_scale > 0.0f) ? m_dpi_scale : 1.0f;
            mmi->ptMinTrackSize.x = static_cast<LONG>(std::lround(m_min_size.x * s));
            mmi->ptMinTrackSize.y = static_cast<LONG>(std::lround(m_min_size.y * s));
            mmi->ptMaxTrackSize.x = static_cast<LONG>(std::min<double>(32767.0, std::lround(m_max_size.x * s)));
            mmi->ptMaxTrackSize.y = static_cast<LONG>(std::min<double>(32767.0, std::lround(m_max_size.y * s)));
            if (!m_decorated) {
                HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{sizeof(mi)};
                if (GetMonitorInfoW(hmon, &mi)) {
                    mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                    mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                    mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                    mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
                }
            }
            return 0;
        }
        case WM_NCHITTEST: {
            if (!m_decorated && m_titlebar_hit_test) {
                POINT pt{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
                ScreenToClient(m_hwnd, &pt);
                const float inv_dpi = (m_dpi_scale > 0.0f) ? (1.0f / m_dpi_scale) : 1.0f;
                TitlebarHitTest ht = m_titlebar_hit_test(PointF{static_cast<float>(pt.x) * inv_dpi,
                                                                static_cast<float>(pt.y) * inv_dpi});
                switch (ht) {
                    case TitlebarHitTest::Caption: return HTCAPTION;
                    // 对齐 CUI.Core：最大化按钮必须返回 HTMAXBUTTON，才能触发 Windows 11 原生分屏布局 (Snap Layouts)
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
        case WM_NCMOUSEMOVE: {
            if (!m_decorated) {
                POINT pt{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
                ScreenToClient(m_hwnd, &pt);
                if (Win32Backend::instance()) {
                    const float inv_dpi = (m_dpi_scale > 0.0f) ? (1.0f / m_dpi_scale) : 1.0f;
                    PlatformEvent pe{};
                    pe.type = PlatformEventType::PointerMove;
                    pe.window_handle = static_cast<void*>(m_hwnd);
                    pe.position = Vec2{static_cast<float>(pt.x) * inv_dpi, static_cast<float>(pt.y) * inv_dpi};
                    pe.timestamp_ms = GetTickCount64();
                    Win32Backend::instance()->dispatch_platform_event(pe);
                }
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE | TME_NONCLIENT, m_hwnd, 0};
                TrackMouseEvent(&tme);

                // 窄范围交由 DwmDefWindowProc 处理，启用 Win11 最大化按钮悬停分屏布局 (Snap Layouts)
                LRESULT dwm_res = 0;
                if (call_dwm_def_window_proc(m_hwnd, msg, wparam, lparam, &dwm_res)) {
                    return dwm_res;
                }
            }
            break;
        }
        case WM_NCMOUSELEAVE: {
            if (!m_decorated && Win32Backend::instance()) {
                PlatformEvent pe{};
                pe.type = PlatformEventType::PointerMove;
                pe.window_handle = static_cast<void*>(m_hwnd);
                pe.position = Vec2{-1.0f, -1.0f};
                pe.timestamp_ms = GetTickCount64();
                Win32Backend::instance()->dispatch_platform_event(pe);
            }
            break;
        }
        case WM_NCLBUTTONDOWN: {
            if (!m_decorated && (wparam == HTCLOSE || wparam == HTMAXBUTTON || wparam == HTMINBUTTON)) {
                return 0; // 吞掉默认绘制，防止系统在自绘标题栏上重绘旧版原生按钮
            }
            break;
        }
        case WM_NCLBUTTONUP: {
            if (!m_decorated) {
                if (wparam == HTCLOSE) {
                    PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
                    return 0;
                }
                if (wparam == HTMAXBUTTON) {
                    SendMessageW(m_hwnd, WM_SYSCOMMAND, IsZoomed(m_hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
                    return 0;
                }
                if (wparam == HTMINBUTTON) {
                    SendMessageW(m_hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                    return 0;
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

    // 自动启用 Windows Per-Monitor DPI 感知 V2，彻底杜绝系统拉伸导致的文字模糊
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

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
            poll_events();
        }
    } else {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout_ms, QS_ALLINPUT);
        poll_events();
    }
}

std::string Win32Backend::get_clipboard_text() const {
    if (!OpenClipboard(nullptr)) return "";
    std::string result;
    HANDLE hdata = GetClipboardData(CF_UNICODETEXT);
    if (hdata) {
        const wchar_t* wstr = static_cast<const wchar_t*>(GlobalLock(hdata));
        if (wstr) {
            result = wstring_to_utf8(std::wstring(wstr));
            GlobalUnlock(hdata);
        }
    }
    CloseClipboard();
    return result;
}

void Win32Backend::set_clipboard_text(const std::string& utf8_text) {
    std::wstring wide = utf8_to_wstring(utf8_text);
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hmem) {
        void* ptr = GlobalLock(hmem);
        if (ptr) {
            std::memcpy(ptr, wide.c_str(), bytes);
            GlobalUnlock(hmem);
            SetClipboardData(CF_UNICODETEXT, hmem);
        } else {
            GlobalFree(hmem);
        }
    }
    CloseClipboard();
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
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        if (cs && cs->lpCreateParams) {
            auto* win = static_cast<Win32Window*>(cs->lpCreateParams);
            win->attach_hwnd(hwnd);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
            if (s_instance) {
                s_instance->m_window_map[hwnd] = win;
            }
        }
    }

    Win32Window* win = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!win && s_instance) {
        auto it = s_instance->m_window_map.find(hwnd);
        if (it != s_instance->m_window_map.end()) {
            win = it->second;
        }
    }

    if (win) {
        if (s_instance) {
            // 处理统一输入事件转发
            PlatformEvent pe{};
            pe.window_handle = static_cast<void*>(hwnd);
            pe.timestamp_ms = GetTickCount64();
            pe.modifiers.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            pe.modifiers.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            pe.modifiers.alt   = (GetKeyState(VK_MENU) & 0x8000) != 0;
            const float inv_dpi = (win->dpi_scale() > 0.0f) ? (1.0f / win->dpi_scale()) : 1.0f;
            switch (msg) {
                case WM_MOUSEMOVE:
                    pe.type = PlatformEventType::PointerMove;
                    pe.position = Vec2{static_cast<float>(static_cast<short>(LOWORD(lparam))) * inv_dpi,
                                       static_cast<float>(static_cast<short>(HIWORD(lparam))) * inv_dpi};
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_LBUTTONDOWN:
                    pe.type = PlatformEventType::PointerDown;
                    pe.button = PointerButton::Primary;
                    pe.position = Vec2{static_cast<float>(static_cast<short>(LOWORD(lparam))) * inv_dpi,
                                       static_cast<float>(static_cast<short>(HIWORD(lparam))) * inv_dpi};
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_LBUTTONUP:
                    pe.type = PlatformEventType::PointerUp;
                    pe.button = PointerButton::Primary;
                    pe.position = Vec2{static_cast<float>(static_cast<short>(LOWORD(lparam))) * inv_dpi,
                                       static_cast<float>(static_cast<short>(HIWORD(lparam))) * inv_dpi};
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_RBUTTONDOWN:
                    pe.type = PlatformEventType::PointerDown;
                    pe.button = PointerButton::Secondary;
                    pe.position = Vec2{static_cast<float>(static_cast<short>(LOWORD(lparam))) * inv_dpi,
                                       static_cast<float>(static_cast<short>(HIWORD(lparam))) * inv_dpi};
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_RBUTTONUP:
                    pe.type = PlatformEventType::PointerUp;
                    pe.button = PointerButton::Secondary;
                    pe.position = Vec2{static_cast<float>(static_cast<short>(LOWORD(lparam))) * inv_dpi,
                                       static_cast<float>(static_cast<short>(HIWORD(lparam))) * inv_dpi};
                    s_instance->dispatch_platform_event(pe);
                    break;
                case WM_MOUSEWHEEL: {
                    POINT pt{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
                    ScreenToClient(hwnd, &pt);
                    short wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
                    pe.type = PlatformEventType::PointerScroll;
                    pe.position = Vec2{static_cast<float>(pt.x) * inv_dpi, static_cast<float>(pt.y) * inv_dpi};
                    pe.delta = Vec2{0.0f, -static_cast<float>(wheel_delta) / 120.0f * 36.0f};
                    s_instance->dispatch_platform_event(pe);
                    break;
                }
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
        }
        return win->handle_message(msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace ktui

#endif // defined(_WIN32)
