#pragma once

#include "dispatcher.hpp"
#include "window.hpp"
#include <ktui/binding/property.hpp>
#include <ktui/theme/theme.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <vector>
#include <memory>
#include <cstdint>
#include <cassert>

namespace ktui {

// 全局应用宿主中枢 (Application Host)
// 管理进程生命周期、主事件循环、多窗口调度、UI 线程 Dispatcher 与全局响应式主题
class Application {
public:
    // 获取全局 Application 单例指针
    [[nodiscard]] static Application* current() noexcept {
        return s_instance;
    }

    // 获取全局 Application 单例引用 (未初始化则抛出断言)
    [[nodiscard]] static Application& instance() {
        assert(s_instance != nullptr && "Application::instance() called before Application was created!");
        return *s_instance;
    }

    // 构造 Application (若 backend 为空则自动通过统一工厂匹配原生/可用后端)
    explicit Application(std::unique_ptr<IPlatformBackend> backend = nullptr);
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // 启动主事件循环直至退出
    int run();

    // 请求退出主循环
    void quit(int exit_code = 0);

    // 驱动单次事件轮询 (支持指定休眠超时，0 表示纯休眠直至平台输入，实现真正的 0% CPU 闲置)
    void pump_events(bool wait = false, uint32_t timeout_ms = 16);

    // 计算下一帧休眠超时毫秒
    [[nodiscard]] uint32_t compute_wait_timeout() const;

    // 虚拟时钟步进 (支持无头驱动单步测试与动画推进)
    void step(float dt);

    [[nodiscard]] bool is_running() const noexcept { return m_running; }
    [[nodiscard]] int exit_code() const noexcept { return m_exit_code; }

    // ==================== 窗口管理 ====================
    // 创建新逻辑应用窗口
    std::shared_ptr<Window> create_window(const WindowDesc& desc = WindowDesc{});

    // 关闭指定窗口
    void close_window(uint32_t window_id);

    // 获取主窗口 (若不存在返回 nullptr)
    [[nodiscard]] std::shared_ptr<Window> main_window() const;

    // 获取所有存活窗口列表
    [[nodiscard]] const std::vector<std::shared_ptr<Window>>& windows() const noexcept {
        return m_windows;
    }

    // ==================== 全局响应式主题 ====================
    // 采用 Property<Theme> 包装，修改会自动通知并触发响应式绑定更新
    Property<Theme> theme;

    [[nodiscard]] const Theme& current_theme() const noexcept {
        return theme.get();
    }

    void set_theme(const Theme& t) {
        Theme old_t = theme.get();
        if (old_t.is_dark != t.is_dark) {
            for (auto& win : m_windows) {
                if (win && !win->is_closed()) {
                    win->start_theme_ripple(old_t, t, win->last_pointer_pos());
                }
            }
        }
        theme.set(t);
    }

    void set_theme_with_ripple(const Theme& t, Vec2 origin) {
        Theme old_t = theme.get();
        if (old_t.is_dark != t.is_dark) {
            for (auto& win : m_windows) {
                if (win && !win->is_closed()) {
                    win->start_theme_ripple(old_t, t, origin);
                }
            }
        }
        theme.set(t);
    }

    // 使用 OkLab 感知均匀色彩空间插值执行平滑主题渐变过渡
    void set_theme_animated(const Theme& t, float duration_sec = 0.32f) {
        Theme old_t = theme.get();
        if (!(old_t == t)) {
            for (auto& win : m_windows) {
                if (win && !win->is_closed()) {
                    win->start_theme_crossfade(old_t, t, duration_sec);
                }
            }
        }
        theme.set(t);
    }

    // ==================== 核心子系统访问 ====================
    [[nodiscard]] Dispatcher& dispatcher() noexcept { return m_dispatcher; }
    [[nodiscard]] IPlatformBackend& backend() noexcept { return *m_backend; }

    // 驱动单帧渲染与事件派发 (供 WebAssembly 主循环与宿主调度使用)
    void step_frame();

private:
    void cleanup_closed_windows();

    inline static Application* s_instance = nullptr;

    std::unique_ptr<IPlatformBackend> m_backend;
    Dispatcher m_dispatcher;
    std::vector<std::shared_ptr<Window>> m_windows;
    uint32_t m_next_window_id{1};

    bool m_running{false};
    int m_exit_code{0};
};

} // namespace ktui
