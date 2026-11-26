#if defined(KTUI_ENABLE_SYSTEM_TRAY) && KTUI_ENABLE_SYSTEM_TRAY

#include <ktui/pal/system_tray.hpp>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace ktui {

#if defined(_WIN32)

namespace {
constexpr UINT kTrayCallbackMsg = WM_APP + 0x4B54;

std::wstring tray_utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}
} // namespace

struct SystemTray::Impl {
    HWND hwnd{nullptr};
    HICON hicon{nullptr};
    bool owns_icon{false};
    NOTIFYICONDATAW nid{};
    SystemTray* owner{nullptr};

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        auto* impl = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == kTrayCallbackMsg && impl && impl->owner) {
            const UINT event = LOWORD(lparam);
            if (event == WM_LBUTTONUP) {
                impl->owner->simulate_click();
            } else if (event == WM_LBUTTONDBLCLK) {
                impl->owner->simulate_double_click();
            } else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
                impl->show_context_menu();
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    void show_context_menu() {
        if (!owner || owner->menu().empty() || !hwnd) return;
        HMENU hmenu = CreatePopupMenu();
        if (!hmenu) return;
        for (const auto& item : owner->menu()) {
            if (item.separator) {
                AppendMenuW(hmenu, MF_SEPARATOR, 0, nullptr);
            } else {
                UINT flags = MF_STRING;
                if (!item.enabled) flags |= MF_GRAYED;
                if (item.checked) flags |= MF_CHECKED;
                std::wstring wlabel = tray_utf8_to_wide(item.label);
                AppendMenuW(hmenu, flags, static_cast<UINT_PTR>(item.id), wlabel.c_str());
            }
        }
        POINT pt{};
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);
        UINT cmd = TrackPopupMenu(hmenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                  pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(hmenu);
        if (cmd != 0) {
            owner->simulate_menu_select(cmd);
        }
    }
};

SystemTray::SystemTray() : m_impl(std::make_unique<Impl>()) {
    m_impl->owner = this;
}

SystemTray::~SystemTray() {
    destroy();
}

bool SystemTray::create(const TrayIconDesc& desc) {
    destroy();
    m_tooltip = desc.tooltip;
    m_menu = desc.menu;

    static ATOM s_cls = 0;
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    if (!s_cls) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Impl::wnd_proc;
        wc.hInstance = hinst;
        wc.lpszClassName = L"KTUI_SystemTray_HiddenWindow";
        s_cls = RegisterClassExW(&wc);
    }

    // 使用隐藏顶层窗口而非 HWND_MESSAGE，确保能够接收 Explorer 的 TaskbarCreated 广播与托盘消息
    m_impl->hwnd = CreateWindowExW(0, L"KTUI_SystemTray_HiddenWindow", L"", WS_OVERLAPPED,
                                   0, 0, 0, 0, nullptr, nullptr, hinst, nullptr);
    if (!m_impl->hwnd) return false;
    SetWindowLongPtrW(m_impl->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_impl.get()));

    if (!desc.icon_path.empty()) {
        std::wstring wpath = tray_utf8_to_wide(desc.icon_path);
        m_impl->hicon = static_cast<HICON>(LoadImageW(nullptr, wpath.c_str(), IMAGE_ICON, 0, 0,
                                                      LR_LOADFROMFILE | LR_DEFAULTSIZE));
        m_impl->owns_icon = (m_impl->hicon != nullptr);
    }
    if (!m_impl->hicon) {
        m_impl->hicon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512)); // IDI_APPLICATION
        m_impl->owns_icon = false;
    }

    std::memset(&m_impl->nid, 0, sizeof(m_impl->nid));
    m_impl->nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_impl->nid.hWnd = m_impl->hwnd;
    m_impl->nid.uID = 1;
    m_impl->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_impl->nid.uCallbackMessage = kTrayCallbackMsg;
    m_impl->nid.hIcon = m_impl->hicon;
    std::wstring wtip = tray_utf8_to_wide(m_tooltip);
    wcsncpy_s(m_impl->nid.szTip, wtip.c_str(), _TRUNCATE);

    if (!Shell_NotifyIconW(NIM_ADD, &m_impl->nid)) {
        destroy();
        return false;
    }
    m_active = true;
    return true;
}

void SystemTray::destroy() {
    if (m_active && m_impl && m_impl->hwnd) {
        Shell_NotifyIconW(NIM_DELETE, &m_impl->nid);
    }
    if (m_impl) {
        if (m_impl->owns_icon && m_impl->hicon) {
            DestroyIcon(m_impl->hicon);
        }
        m_impl->hicon = nullptr;
        m_impl->owns_icon = false;
        if (m_impl->hwnd) {
            DestroyWindow(m_impl->hwnd);
            m_impl->hwnd = nullptr;
        }
    }
    m_active = false;
}

void SystemTray::set_tooltip(const std::string& tooltip) {
    m_tooltip = tooltip;
    if (m_active && m_impl && m_impl->hwnd) {
        m_impl->nid.uFlags = NIF_TIP;
        std::wstring wtip = tray_utf8_to_wide(m_tooltip);
        wcsncpy_s(m_impl->nid.szTip, wtip.c_str(), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &m_impl->nid);
    }
}

void SystemTray::set_menu(std::vector<TrayMenuItem> items) {
    m_menu = std::move(items);
}

void SystemTray::show_notification(const std::string& title, const std::string& message) {
    if (!m_active || !m_impl || !m_impl->hwnd) return;
    m_impl->nid.uFlags = NIF_INFO;
    m_impl->nid.dwInfoFlags = NIIF_INFO;
    std::wstring wtitle = tray_utf8_to_wide(title);
    std::wstring wmsg = tray_utf8_to_wide(message);
    wcsncpy_s(m_impl->nid.szInfoTitle, wtitle.c_str(), _TRUNCATE);
    wcsncpy_s(m_impl->nid.szInfo, wmsg.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_impl->nid);
}

#else

struct SystemTray::Impl {};
SystemTray::SystemTray() : m_impl(std::make_unique<Impl>()) {}
SystemTray::~SystemTray() { destroy(); }
bool SystemTray::create(const TrayIconDesc& desc) {
    m_tooltip = desc.tooltip;
    m_menu = desc.menu;
    m_active = true;
    return true;
}
void SystemTray::destroy() { m_active = false; }
void SystemTray::set_tooltip(const std::string& tooltip) { m_tooltip = tooltip; }
void SystemTray::set_menu(std::vector<TrayMenuItem> items) { m_menu = std::move(items); }
void SystemTray::show_notification(const std::string&, const std::string&) {}

#endif

} // namespace ktui

#endif // KTUI_ENABLE_SYSTEM_TRAY
