#pragma once

#include <ktui/core/fn.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// 系统托盘右键菜单项
struct TrayMenuItem {
    uint32_t id{0};
    std::string label;
    bool enabled{true};
    bool checked{false};
    bool separator{false};
};

// 系统托盘图标描述符
struct TrayIconDesc {
    std::string tooltip;
    std::string icon_path; // 可选 .ico 文件路径 (为空则使用应用默认图标)
    std::vector<TrayMenuItem> menu;
};

// 跨平台系统托盘图标 (按需可选编译：通过 CMake -DKTUI_ENABLE_SYSTEM_TRAY=ON 启用原生后端，默认关闭零体积开销)
class SystemTray {
public:
    SystemTray();
    ~SystemTray();

    SystemTray(const SystemTray&) = delete;
    SystemTray& operator=(const SystemTray&) = delete;

    [[nodiscard]] static bool is_compiled_in() noexcept {
#if defined(KTUI_ENABLE_SYSTEM_TRAY) && KTUI_ENABLE_SYSTEM_TRAY
        return true;
#else
        return false;
#endif
    }

    bool create(const TrayIconDesc& desc);
    void destroy();

    void set_tooltip(const std::string& tooltip);
    void set_menu(std::vector<TrayMenuItem> items);
    void show_notification(const std::string& title, const std::string& message);

    void on_click(Fn<void()> cb) { m_on_click = std::move(cb); }
    void on_double_click(Fn<void()> cb) { m_on_double_click = std::move(cb); }
    void on_menu_item(Fn<void(uint32_t)> cb) { m_on_menu_item = std::move(cb); }

    [[nodiscard]] bool is_active() const noexcept { return m_active; }
    [[nodiscard]] const std::string& tooltip() const noexcept { return m_tooltip; }
    [[nodiscard]] const std::vector<TrayMenuItem>& menu() const noexcept { return m_menu; }

    // 供单元测试 / Headless 环境触发模拟事件
    void simulate_click() { if (m_active && m_on_click) m_on_click(); }
    void simulate_double_click() { if (m_active && m_on_double_click) m_on_double_click(); }
    void simulate_menu_select(uint32_t id) { if (m_active && m_on_menu_item) m_on_menu_item(id); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_active{false};
    std::string m_tooltip;
    std::vector<TrayMenuItem> m_menu;
    Fn<void()> m_on_click;
    Fn<void()> m_on_double_click;
    Fn<void(uint32_t)> m_on_menu_item;
};

#if !defined(KTUI_ENABLE_SYSTEM_TRAY) || !KTUI_ENABLE_SYSTEM_TRAY
// 未启用原生系统托盘编译时提供零成本轻量内联桩，避免链接额外系统符号
struct SystemTray::Impl {};
inline SystemTray::SystemTray() = default;
inline SystemTray::~SystemTray() = default;
inline bool SystemTray::create(const TrayIconDesc& desc) {
    m_tooltip = desc.tooltip;
    m_menu = desc.menu;
    m_active = false;
    return false;
}
inline void SystemTray::destroy() { m_active = false; }
inline void SystemTray::set_tooltip(const std::string& tooltip) { m_tooltip = tooltip; }
inline void SystemTray::set_menu(std::vector<TrayMenuItem> items) { m_menu = std::move(items); }
inline void SystemTray::show_notification(const std::string&, const std::string&) {}
#endif

} // namespace ktui
