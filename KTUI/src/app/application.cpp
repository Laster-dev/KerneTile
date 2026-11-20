#include <ktui/app/application.hpp>
#include <ktui/pal/clipboard.hpp>
#include <algorithm>

namespace ktui {

// ==================== ThemeContext 实现 ====================

const Theme& ThemeContext::current() noexcept {
    if (!scope_stack().empty()) {
        return scope_stack().back();
    }
    if (Application* app = Application::current()) {
        return app->current_theme();
    }
    return fallback_theme();
}

// ==================== Application 实现 ====================

Application::Application(std::unique_ptr<IPlatformBackend> backend)
    : theme(Theme::dark()) {
    assert(s_instance == nullptr && "Only one Application instance can exist per process!");
    s_instance = this;

    if (backend) {
        m_backend = std::move(backend);
    } else {
        m_backend = create_platform_backend(false);
    }

    if (m_backend) {
        m_backend->init();
        Clipboard::bind_backend(m_backend.get());
        m_backend->set_event_callback([this](const PlatformEvent& pe) {
            std::shared_ptr<Window> target;
            if (pe.window_handle != nullptr) {
                for (auto& win : m_windows) {
                    if (win && !win->is_closed() && win->platform_window() &&
                        win->platform_window()->get_native_handle() == pe.window_handle) {
                        target = win;
                        break;
                    }
                }
            }
            if (!target && !m_windows.empty()) {
                target = m_windows.front();
            }
            if (target && !target->is_closed()) {
                target->handle_platform_event(pe);
            }
        });
    }

    m_dispatcher.bind_current_thread_as_ui_thread();
}

Application::~Application() {
    // 先销毁所有窗口
    m_windows.clear();

    if (m_backend) {
        Clipboard::bind_backend(nullptr);
        m_backend->shutdown();
    }

    if (s_instance == this) {
        s_instance = nullptr;
    }
}

int Application::run() {
    m_running = true;
    m_exit_code = 0;

    while (m_running) {
        for (auto& win : m_windows) {
            if (win && !win->is_closed() && win->needs_redraw() && win->root_render_box()) {
                win->render_frame();
            }
        }

        pump_events(true);
        cleanup_closed_windows();

        // 当所有窗口均已关闭且未显示指定保持运行，则退出
        if (m_windows.empty()) {
            quit(0);
            break;
        }
    }

    return m_exit_code;
}

void Application::quit(int exit_code) {
    m_running = false;
    m_exit_code = exit_code;
}

void Application::pump_events(bool wait) {
    if (m_backend) {
        if (wait) {
            m_backend->wait_events(16);
        } else {
            m_backend->poll_events();
        }
    }

    // 优先消费处理积压的 UI 线程调度任务
    m_dispatcher.process_tasks();
}

void Application::step(float dt) {
    (void)dt;
    pump_events(false);
    cleanup_closed_windows();
}

std::shared_ptr<Window> Application::create_window(const WindowDesc& desc) {
    if (!m_backend) return nullptr;

    auto plat_win = m_backend->create_window(desc);
    if (!plat_win) return nullptr;

    auto win = std::make_shared<Window>(m_next_window_id++, std::move(plat_win));
    m_windows.push_back(win);
    return win;
}

void Application::close_window(uint32_t window_id) {
    for (auto& win : m_windows) {
        if (win && win->id() == window_id) {
            win->close();
            break;
        }
    }
    cleanup_closed_windows();
}

std::shared_ptr<Window> Application::main_window() const {
    return m_windows.empty() ? nullptr : m_windows.front();
}

void Application::cleanup_closed_windows() {
    std::erase_if(m_windows, [](const std::shared_ptr<Window>& win) {
        return !win || win->is_closed();
    });
}

} // namespace ktui
