#pragma once

#include <cstdint>
#include <atomic>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <mutex>
#include <condition_variable>
#include <chrono>
#endif

namespace kernetile::ring {

class ThreadEvent {
public:
    ThreadEvent() {
#if defined(_WIN32)
        m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr); // Auto-reset event
#endif
    }

    ~ThreadEvent() {
#if defined(_WIN32)
        if (m_event != nullptr) {
            CloseHandle(m_event);
            m_event = nullptr;
        }
#endif
    }

    ThreadEvent(const ThreadEvent&) = delete;
    ThreadEvent& operator=(const ThreadEvent&) = delete;
    ThreadEvent(ThreadEvent&&) = delete;
    ThreadEvent& operator=(ThreadEvent&&) = delete;

    // 发送唤醒信号 (触发系统级中断)
    void notify() noexcept {
        m_notify_count.fetch_add(1, std::memory_order_relaxed);
#if defined(_WIN32)
        SetEvent(m_event);
#else
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_signaled = true;
        }
        m_cv.notify_one();
#endif
    }

    // 等待信号唤醒
    // timeout_ms = 0xFFFFFFFF 表示无限等待
    bool wait(uint32_t timeout_ms = 0xFFFFFFFF) noexcept {
#if defined(_WIN32)
        DWORD res = WaitForSingleObject(m_event, timeout_ms == 0xFFFFFFFF ? INFINITE : timeout_ms);
        return res == WAIT_OBJECT_0;
#else
        std::unique_lock<std::mutex> lock(m_mutex);
        if (timeout_ms == 0xFFFFFFFF) {
            m_cv.wait(lock, [this]() { return m_signaled; });
            m_signaled = false;
            return true;
        } else {
            bool ok = m_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return m_signaled; });
            if (ok) {
                m_signaled = false;
            }
            return ok;
        }
#endif
    }

    [[nodiscard]] uint64_t get_notify_count() const noexcept {
        return m_notify_count.load(std::memory_order_relaxed);
    }

    void reset_notify_count() noexcept {
        m_notify_count.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> m_notify_count{0};
#if defined(_WIN32)
    HANDLE m_event{nullptr};
#else
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_signaled{false};
#endif
};

} // namespace kernetile::ring
