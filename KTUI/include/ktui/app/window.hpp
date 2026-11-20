#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <string>
#include <memory>
#include <functional>
#include <atomic>

namespace ktui {

class Application;

// 逻辑应用窗口抽象 (Logical Application Window)
// 统一管理底层物理平台窗口生命周期、DPI 自适应缩放、视图树挂接与重绘调度
class Window {
public:
    Window(uint32_t id, std::unique_ptr<IPlatformWindow> platform_window)
        : m_id(id), m_platform_window(std::move(platform_window)) {
        init_internal_hooks();
    }

    virtual ~Window() {
        close();
    }

    [[nodiscard]] uint32_t id() const noexcept { return m_id; }
    [[nodiscard]] IPlatformWindow* platform_window() const noexcept { return m_platform_window.get(); }

    // ==================== 几何与尺寸 ====================
    [[nodiscard]] Vec2 size() const {
        return m_platform_window ? m_platform_window->size() : Vec2{0.0f, 0.0f};
    }

    void set_size(Vec2 sz) {
        if (m_platform_window) m_platform_window->set_size(sz);
    }

    [[nodiscard]] Vec2 position() const {
        return m_platform_window ? m_platform_window->position() : Vec2{0.0f, 0.0f};
    }

    void set_position(Vec2 pos) {
        if (m_platform_window) m_platform_window->set_position(pos);
    }

    void center_on_screen() {
        if (m_platform_window) m_platform_window->center_on_screen();
    }

    [[nodiscard]] Rect screen_bounds() const {
        return m_platform_window ? m_platform_window->screen_bounds() : Rect{};
    }

    // ==================== 呈现状态 ====================
    void show() {
        if (m_platform_window) m_platform_window->show();
    }

    void hide() {
        if (m_platform_window) m_platform_window->hide();
    }

    [[nodiscard]] bool is_visible() const {
        return m_platform_window ? m_platform_window->is_visible() : false;
    }

    void minimize() {
        if (m_platform_window) m_platform_window->minimize();
    }

    void maximize() {
        if (m_platform_window) m_platform_window->maximize();
    }

    void restore() {
        if (m_platform_window) m_platform_window->restore();
    }

    [[nodiscard]] bool is_minimized() const {
        return m_platform_window ? m_platform_window->is_minimized() : false;
    }

    [[nodiscard]] bool is_maximized() const {
        return m_platform_window ? m_platform_window->is_maximized() : false;
    }

    void set_fullscreen(bool fullscreen) {
        if (m_platform_window) m_platform_window->set_fullscreen(fullscreen);
    }

    [[nodiscard]] bool is_fullscreen() const {
        return m_platform_window ? m_platform_window->is_fullscreen() : false;
    }

    void activate() {
        if (m_platform_window) m_platform_window->activate();
    }

    void focus() {
        if (m_platform_window) m_platform_window->focus();
    }

    [[nodiscard]] bool has_focus() const {
        return m_platform_window ? m_platform_window->has_focus() : false;
    }

    void set_always_on_top(bool on_top) {
        if (m_platform_window) m_platform_window->set_always_on_top(on_top);
    }

    // ==================== 外观与标题栏 ====================
    [[nodiscard]] std::string title() const {
        return m_platform_window ? m_platform_window->title() : "";
    }

    void set_title(const std::string& t) {
        if (m_platform_window) m_platform_window->set_title(t);
    }

    void set_opacity(float opacity) {
        if (m_platform_window) m_platform_window->set_opacity(opacity);
    }

    [[nodiscard]] float opacity() const {
        return m_platform_window ? m_platform_window->opacity() : 1.0f;
    }

    // ==================== DPI 与显示参数 ====================
    [[nodiscard]] float dpi_scale() const {
        return m_platform_window ? m_platform_window->dpi_scale() : 1.0f;
    }

    [[nodiscard]] uint32_t refresh_rate() const {
        return m_platform_window ? m_platform_window->refresh_rate() : 60;
    }

    // ==================== 重绘调度与脏标记 ====================
    void request_redraw() noexcept {
        m_needs_redraw = true;
    }

    [[nodiscard]] bool needs_redraw() const noexcept {
        return m_needs_redraw;
    }

    void clear_redraw_flag() noexcept {
        m_needs_redraw = false;
    }

    // ==================== 关闭与生命周期 ====================
    void close() {
        if (!m_is_closed) {
            m_is_closed = true;
            if (m_platform_window) {
                m_platform_window->close();
            }
        }
    }

    [[nodiscard]] bool is_closed() const noexcept {
        return m_is_closed;
    }

    // ==================== 回调监听挂接 ====================
    void on_resize(std::function<void(Vec2)> handler) {
        m_resize_callbacks.push_back(std::move(handler));
    }

    void on_close(std::function<bool()> handler) {
        m_close_callbacks.push_back(std::move(handler));
    }

    void on_dpi_changed(std::function<void(float)> handler) {
        m_dpi_callbacks.push_back(std::move(handler));
    }

    void on_focus_changed(std::function<void(bool)> handler) {
        m_focus_callbacks.push_back(std::move(handler));
    }

private:
    void init_internal_hooks() {
        if (!m_platform_window) return;

        m_platform_window->on_resized([this](Vec2 sz) {
            request_redraw();
            for (auto& cb : m_resize_callbacks) {
                if (cb) cb(sz);
            }
        });

        m_platform_window->on_close_requested([this]() -> bool {
            bool allow_close = true;
            for (auto& cb : m_close_callbacks) {
                if (cb && !cb()) {
                    allow_close = false;
                }
            }
            if (allow_close) {
                m_is_closed = true;
            }
            return allow_close;
        });

        m_platform_window->on_dpi_changed([this](float dpi) {
            request_redraw();
            for (auto& cb : m_dpi_callbacks) {
                if (cb) cb(dpi);
            }
        });

        m_platform_window->on_focus_changed([this](bool focused) {
            for (auto& cb : m_focus_callbacks) {
                if (cb) cb(focused);
            }
        });
    }

    uint32_t m_id{0};
    std::unique_ptr<IPlatformWindow> m_platform_window;
    bool m_is_closed{false};
    bool m_needs_redraw{true};

    std::vector<std::function<void(Vec2)>> m_resize_callbacks;
    std::vector<std::function<bool()>> m_close_callbacks;
    std::vector<std::function<void(float)>> m_dpi_callbacks;
    std::vector<std::function<void(bool)>> m_focus_callbacks;
};

} // namespace ktui
