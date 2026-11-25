#pragma once

#include <atomic>

namespace ktui {

// 零 CRT/libcpmt 依赖的轻量跨平台自旋锁 (彻底避免 Windows MSVC std::mutex 拖入 _Sys_errtab 与 EH4 异常运行时)
class LightMutex {
public:
    constexpr LightMutex() noexcept = default;
    LightMutex(const LightMutex&) = delete;
    LightMutex& operator=(const LightMutex&) = delete;

    void lock() noexcept {
        while (m_flag.test_and_set(std::memory_order_acquire)) {
#if defined(__cpp_lib_atomic_flag_test)
            while (m_flag.test(std::memory_order_relaxed)) {}
#endif
        }
    }

    void unlock() noexcept {
        m_flag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
};

class LightLock {
public:
    explicit LightLock(LightMutex& m) noexcept : m_mutex(m) { m_mutex.lock(); }
    ~LightLock() noexcept { m_mutex.unlock(); }
    LightLock(const LightLock&) = delete;
    LightLock& operator=(const LightLock&) = delete;

private:
    LightMutex& m_mutex;
};

} // namespace ktui
