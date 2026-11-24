#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace ktui {

// 全局统一帧时钟 (FrameClock)
// 每帧在 Window::render_frame() 顶部采样一次单调时间，消除控件各自 steady_clock::now() 系统调用，
// 保证多控件联动动画同帧对齐，并支持 Headless 单元测试通过 advance_virtual(dt) 做确定性步进。
class FrameClock {
public:
    static FrameClock& global() noexcept {
        static FrameClock instance;
        return instance;
    }

    // 真实帧开始采样 (在 Window::render_frame 顶部调用一次)
    void begin_frame() noexcept {
        if (m_virtual_mode) return;
        auto now = std::chrono::steady_clock::now();
        if (!m_started) {
            m_origin = now;
            m_last = now;
            m_started = true;
            m_time_sec = 0.0;
            m_dt_sec = 1.0f / 60.0f;
            m_frame_index = 1;
            return;
        }
        double elapsed = std::chrono::duration<double>(now - m_origin).count();
        float dt = static_cast<float>(std::chrono::duration<double>(now - m_last).count());
        m_last = now;
        m_time_sec = elapsed;
        m_dt_sec = std::clamp(dt, 0.0f, 0.1f);
        ++m_frame_index;
    }

    // Headless 确定性测试模式: 显式推进虚拟时间
    void enable_virtual_time(double initial_sec = 0.0) noexcept {
        m_virtual_mode = true;
        m_started = true;
        m_time_sec = initial_sec;
        m_dt_sec = 0.0f;
        m_frame_index = 0;
    }

    void disable_virtual_time() noexcept {
        m_virtual_mode = false;
        m_started = false;
    }

    void advance_virtual(float dt_sec) noexcept {
        m_virtual_mode = true;
        m_started = true;
        m_dt_sec = std::max(0.0f, dt_sec);
        m_time_sec += static_cast<double>(m_dt_sec);
        ++m_frame_index;
    }

    [[nodiscard]] double frame_time_sec() const noexcept { return m_time_sec; }
    [[nodiscard]] float dt() const noexcept { return m_dt_sec; }
    [[nodiscard]] uint64_t frame_index() const noexcept { return m_frame_index; }
    [[nodiscard]] bool is_virtual() const noexcept { return m_virtual_mode; }

private:
    FrameClock() noexcept = default;

    bool m_started{false};
    bool m_virtual_mode{false};
    std::chrono::steady_clock::time_point m_origin{};
    std::chrono::steady_clock::time_point m_last{};
    double m_time_sec{0.0};
    float m_dt_sec{1.0f / 60.0f};
    uint64_t m_frame_index{0};
};

} // namespace ktui
