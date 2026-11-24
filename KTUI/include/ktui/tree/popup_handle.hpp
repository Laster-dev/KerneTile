#pragma once

#include <ktui/app/window.hpp>
#include <ktui/tree/overlay.hpp>

#include <cstdint>
#include <functional>
#include <memory>

namespace ktui {

// RAII 浮层弹窗句柄 (PopupHandle)
// 统一接管 OverlayEntry 生命周期：
// 1. 宿主控件析构时自动安全关闭浮层，无需手写析构清理与 m_alive 哨兵
// 2. 浮层被外部点击/Esc/窗口缩放关闭时自动归零句柄并安全回调宿主
class PopupHandle {
public:
    PopupHandle() noexcept = default;
    ~PopupHandle() { close(); }

    PopupHandle(const PopupHandle&) = delete;
    PopupHandle& operator=(const PopupHandle&) = delete;

    PopupHandle(PopupHandle&& other) noexcept
        : m_id(other.m_id), m_alive(std::move(other.m_alive)) {
        other.m_id = 0;
    }

    PopupHandle& operator=(PopupHandle&& other) noexcept {
        if (this != &other) {
            close();
            m_id = other.m_id;
            m_alive = std::move(other.m_alive);
            other.m_id = 0;
        }
        return *this;
    }

    [[nodiscard]] bool is_open() const noexcept { return m_id != 0; }
    [[nodiscard]] uint64_t id() const noexcept { return m_id; }

    // 获取生命周期令牌，供浮层内部回调安全判断宿主是否存活
    [[nodiscard]] std::shared_ptr<bool> lifetime_token() const noexcept { return m_alive; }

    bool open(Window& win, OverlayEntry entry, std::function<void()> on_closed = nullptr) {
        close();
        m_alive = std::make_shared<bool>(true);
        auto alive = m_alive;
        auto prev_dismiss = std::move(entry.on_dismiss);
        entry.on_dismiss = [this, alive, prev = std::move(prev_dismiss), on_closed = std::move(on_closed)]() mutable {
            if (!alive || !*alive) return;
            m_id = 0;
            if (prev) prev();
            if (on_closed) on_closed();
        };
        m_id = win.show_overlay(std::move(entry));
        return m_id != 0;
    }

    void close() {
        if (m_alive) *m_alive = false;
        if (m_id != 0) {
            uint64_t id = m_id;
            m_id = 0;
            if (Window* win = Window::active_window()) {
                win->hide_overlay(id);
            }
        }
    }

private:
    uint64_t m_id{0};
    std::shared_ptr<bool> m_alive;
};

} // namespace ktui
