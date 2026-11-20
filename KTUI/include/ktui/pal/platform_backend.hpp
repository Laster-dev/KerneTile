#pragma once

#include "platform_types.hpp"
#include "platform_dispatcher.hpp"
#include "vsync_source.hpp"
#include "font_resolver.hpp"
#include <memory>
#include <functional>

namespace ktui {

// 跨平台抽象物理窗口接口
class IPlatformWindow {
public:
    virtual ~IPlatformWindow() = default;

    // ==================== 几何与尺寸 ====================
    virtual Vec2 size() const = 0;
    virtual void set_size(Vec2 new_size) = 0;
    virtual Vec2 min_size() const = 0;
    virtual void set_min_size(Vec2 min_sz) = 0;
    virtual Vec2 max_size() const = 0;
    virtual void set_max_size(Vec2 max_sz) = 0;

    virtual Vec2 position() const = 0;
    virtual void set_position(Vec2 pos) = 0;
    virtual void center_on_screen() = 0;
    virtual Rect screen_bounds() const = 0;

    // ==================== 呈现状态 ====================
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual bool is_visible() const = 0;

    virtual void minimize() = 0;
    virtual void maximize() = 0;
    virtual void restore() = 0;
    virtual bool is_minimized() const = 0;
    virtual bool is_maximized() const = 0;

    virtual void set_fullscreen(bool fullscreen) = 0;
    virtual bool is_fullscreen() const = 0;

    virtual void activate() = 0;
    virtual bool is_active() const = 0;
    virtual void focus() = 0;
    virtual bool has_focus() const = 0;

    virtual void set_always_on_top(bool always_on_top) = 0;
    virtual bool is_always_on_top() const = 0;
    virtual void flash_taskbar(bool flash) = 0;

    // ==================== 外观与标题栏 ====================
    virtual std::string title() const = 0;
    virtual void set_title(const std::string& title) = 0;
    virtual void set_icon(const std::string& icon_path) = 0;

    virtual void set_resizable(bool resizable) = 0;
    virtual bool is_resizable() const = 0;

    virtual void set_opacity(float opacity) = 0;
    virtual float opacity() const = 0;

    virtual void set_decorated(bool decorated) = 0;
    virtual bool is_decorated() const = 0;
    virtual void set_titlebar_hit_test_callback(std::function<TitlebarHitTest(PointF local_pos)> cb) = 0;

    // ==================== 显示器与 DPI ====================
    virtual float dpi_scale() const = 0;
    virtual uint32_t refresh_rate() const = 0;

    // ==================== 原生句柄 ====================
    virtual void* get_native_handle() const = 0;

    // ==================== 帧呈现与重绘请求 ====================
    virtual void present_rgba8(const uint8_t* /*rgba*/, uint32_t /*width*/, uint32_t /*height*/) {}
    virtual void invalidate() {}
    virtual void on_paint_requested(std::function<void()> /*handler*/) {}

    // ==================== 框架级 IME 输入法与光标形状控制 ====================
    virtual void set_ime_enabled(bool /*enabled*/) {}
    virtual void set_ime_cursor_rect(Rect /*cursor_rect*/) {}
    virtual void set_cursor(CursorShape /*cursor*/) {}

    // ==================== 回调与销毁 ====================
    virtual void on_close_requested(std::function<bool()> handler) = 0;
    virtual void on_resized(std::function<void(Vec2 new_size)> handler) = 0;
    virtual void on_moved(std::function<void(Vec2 new_pos)> handler) = 0;
    virtual void on_dpi_changed(std::function<void(float new_dpi)> handler) = 0;
    virtual void on_focus_changed(std::function<void(bool focused)> handler) = 0;
    virtual void close() = 0;
};

// 跨平台平台抽象层统一后端接口
class IPlatformBackend {
public:
    virtual ~IPlatformBackend() = default;

    virtual bool init() = 0;
    virtual void poll_events() = 0;
    virtual void wait_events(uint32_t timeout_ms = 0) = 0;

    // 创建顶层窗口
    virtual std::unique_ptr<IPlatformWindow> create_window(const WindowDesc& desc) = 0;

    // 获取核心子系统抽象引用
    virtual IPlatformDispatcher& get_dispatcher() = 0;
    virtual IVSyncSource& get_vsync_source() = 0;
    virtual IFontResolver& get_font_resolver() = 0;

    // 系统剪贴板 (UTF-8)
    virtual std::string get_clipboard_text() const { return ""; }
    virtual void set_clipboard_text(const std::string& /*utf8_text*/) {}

    // 注册全局事件监听器
    virtual void set_event_callback(std::function<void(const PlatformEvent&)> callback) = 0;

    virtual void shutdown() = 0;
};

// 平台后端统一工厂
std::unique_ptr<IPlatformBackend> create_platform_backend(bool force_headless = false);

} // namespace ktui
