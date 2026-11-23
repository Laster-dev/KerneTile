#include <ktui/app/application.hpp>
#include <ktui/pal/clipboard.hpp>
#include <algorithm>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

extern "C" {
EMSCRIPTEN_KEEPALIVE
inline void ktui_web_step_frame() {
    if (auto* app = ktui::Application::current()) {
        app->step_frame();
    }
}
}

EM_JS(void, ktui_js_schedule_next_frame, (int immediate, int delay_ms), {
    if (!window._ktuiRafId) window._ktuiRafId = 0;
    if (!window._ktuiTimerId) window._ktuiTimerId = 0;

    if (!window._ktuiRequestFrame) {
        window._ktuiRequestFrame = function() {
            if (window._ktuiRafId) return;
            if (window._ktuiTimerId) {
                clearTimeout(window._ktuiTimerId);
                window._ktuiTimerId = 0;
            }
            window._ktuiRafId = requestAnimationFrame(function() {
                window._ktuiRafId = 0;
                _ktui_web_step_frame();
            });
        };
    }

    if (immediate) {
        window._ktuiRequestFrame();
    } else if (delay_ms > 0 && !window._ktuiRafId && !window._ktuiTimerId) {
        window._ktuiTimerId = setTimeout(function() {
            window._ktuiTimerId = 0;
            window._ktuiRequestFrame();
        }, delay_ms);
    }
});
#endif

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

    // 主题切换时自动刷新全部窗口
    theme.subscribe([this](const Theme&) {
        for (auto& win : m_windows) {
            if (win && !win->is_closed()) {
                win->request_redraw();
                if (win->platform_window()) {
                    win->platform_window()->invalidate();
                }
            }
        }
    });
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

void Application::step_frame() {
    pump_events(false, 0);

    for (auto& win : m_windows) {
        if (win && !win->is_closed() && win->has_blinking_cursor()) {
            win->tick_blinking_cursor();
        }
    }

    for (auto& win : m_windows) {
        if (win && !win->is_closed() && win->root_render_box()) {
            bool needs_paint = win->needs_redraw() ||
                win->root_render_box()->needs_paint() ||
                win->root_render_box()->needs_layout() || win->root_render_box()->child_needs_layout();
            if (needs_paint) {
                win->render_frame();
            }
        }
    }

    cleanup_closed_windows();

#if defined(__EMSCRIPTEN__)
    if (m_windows.empty()) {
        quit(0);
        return;
    }
    // 按需调度下一帧：有动画/脏状态则立即挂下一帧 rAF；仅有光标闪烁则精确 setTimeout；完全闲置时 0% CPU！
    bool has_dirty = false;
    uint32_t min_blink_wait = 0;
    for (const auto& win : m_windows) {
        if (win && !win->is_closed()) {
            if (win->needs_redraw() ||
                (win->root_render_box() && (win->root_render_box()->needs_paint() ||
                                            win->root_render_box()->needs_layout() ||
                                            win->root_render_box()->child_needs_layout()))) {
                has_dirty = true;
                break;
            }
            if (win->has_blinking_cursor()) {
                uint32_t rem = win->time_until_next_blink_ms();
                if (rem == 0) {
                    has_dirty = true;
                    break;
                }
                min_blink_wait = (min_blink_wait == 0) ? rem : std::min(min_blink_wait, rem);
            }
        }
    }
    if (has_dirty) {
        ktui_js_schedule_next_frame(1, 0);
    } else if (min_blink_wait > 0) {
        ktui_js_schedule_next_frame(0, static_cast<int>(min_blink_wait));
    }
#endif
}

int Application::run() {
    m_running = true;
    m_exit_code = 0;

#if defined(__EMSCRIPTEN__)
    // 事件驱动按需 rAF：静止时彻底休眠 0% CPU，交互/动画/光标闪烁时自动唤醒
    ktui_js_schedule_next_frame(1, 0);
    return m_exit_code;
#elif defined(__ANDROID__)
    // Android 端由 Choreographer 按屏幕刷新率回调 Application::step_frame()
    return m_exit_code;
#else
    auto last_render_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(16);
    constexpr auto kFrameInterval = std::chrono::milliseconds(16); // 60 FPS 工业基准帧率节拍

    while (m_running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_since_render = now - last_render_time;

        bool has_pending_render = false;
        for (const auto& win : m_windows) {
            if (win && !win->is_closed() && win->root_render_box()) {
                if (win->needs_redraw() ||
                    win->root_render_box()->needs_paint() ||
                    win->root_render_box()->needs_layout() || win->root_render_box()->child_needs_layout()) {
                    has_pending_render = true;
                    break;
                }
            }
        }

        if (has_pending_render) {
            if (elapsed_since_render >= kFrameInterval) {
                // 到达或超过 16ms 帧周期，准时推进并呈现新帧
                last_render_time = now;
                for (auto& win : m_windows) {
                    if (win && !win->is_closed() && win->root_render_box()) {
                        bool needs_paint = win->needs_redraw() ||
                            win->root_render_box()->needs_paint() ||
                            win->root_render_box()->needs_layout() || win->root_render_box()->child_needs_layout();
                        if (needs_paint) {
                            win->render_frame();
                        }
                    }
                }
            }
        }

        uint32_t wait_ms = compute_wait_timeout();
        // 若有待渲染帧但未到 16ms 节拍，精准休眠到下一个 16ms 周期
        if (has_pending_render) {
            now = std::chrono::steady_clock::now();
            auto remaining = kFrameInterval - (now - last_render_time);
            if (remaining > std::chrono::milliseconds(0)) {
                uint32_t rem_ms = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count());
                wait_ms = (wait_ms == 0) ? rem_ms : std::min(wait_ms, rem_ms);
                if (wait_ms == 0) wait_ms = 1;
            }
        }

        pump_events(true, wait_ms);

        // 若光标闪烁周期到达，精确触发单帧翻转重绘 (绝无无脑连续 request_redraw() 导致的 60FPS 满载空转)
        for (auto& win : m_windows) {
            if (win && !win->is_closed() && win->has_blinking_cursor()) {
                win->tick_blinking_cursor();
            }
        }

        cleanup_closed_windows();

        // 当所有窗口均已关闭且未显示指定保持运行，则退出
        if (m_windows.empty()) {
            quit(0);
            break;
        }
    }

    return m_exit_code;
#endif
}

uint32_t Application::compute_wait_timeout() const {
    bool has_dirty = false;
    uint32_t min_blink_wait = 0;
    for (const auto& win : m_windows) {
        if (win && !win->is_closed()) {
            if (win->needs_redraw()) {
                has_dirty = true;
                break;
            }
            if (win->root_render_box() && (win->root_render_box()->needs_paint() || win->root_render_box()->needs_layout() || win->root_render_box()->child_needs_layout())) {
                has_dirty = true;
                break;
            }
            if (win->has_blinking_cursor()) {
                uint32_t rem = win->time_until_next_blink_ms();
                if (rem > 0) {
                    min_blink_wait = (min_blink_wait == 0) ? rem : std::min(min_blink_wait, rem);
                } else {
                    has_dirty = true;
                    break;
                }
            }
        }
    }
    if (has_dirty) return 16; // 60 FPS 工业标准帧间隔平滑推进连续动效 (水波纹、对勾展开、焦点滑条等)
    if (min_blink_wait > 0) return min_blink_wait; // 精确休眠直到下次光标闪烁，0% CPU！
    return 0; // 完全闲置，平台消息队列彻底挂起休眠，0% CPU！
}

void Application::quit(int exit_code) {
    m_running = false;
    m_exit_code = exit_code;
}

void Application::pump_events(bool wait, uint32_t timeout_ms) {
    if (m_backend) {
        if (wait) {
            m_backend->wait_events(timeout_ms);
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

#if defined(__ANDROID__)
    bool show_titlebar = false;
#else
    bool show_titlebar = desc.custom_titlebar && (desc.type != WindowType::Popup);
#endif
    auto win = std::make_shared<Window>(m_next_window_id++, std::move(plat_win), show_titlebar);
    win->titlebar().on_theme_toggle = [this]() {
        set_theme(current_theme().is_dark ? Theme::light() : Theme::dark());
    };
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
