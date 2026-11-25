#pragma once

#include <ktui/core/fn.hpp>

#include "spring_curve.hpp"
#include "animated_value.hpp"
#include <functional>
#include <vector>
#include <cmath>
#include <algorithm>

namespace ktui {

// 显式动画状态
enum class AnimationStatus {
    Dismissed,  // 处于起点 0.0f
    Forward,    // 正在正向推进
    Reverse,    // 正在反向倒退
    Completed   // 已到达终点 1.0f
};

// 显式时间线动画控制器
// 适用于明确时长的转场、循环旋转、呼吸灯以及时间驱动的时间线动画
class AnimationController : public IAnimationTickable {
public:
    AnimationController(float duration_seconds = 1.0f, float lower_bound = 0.0f, float upper_bound = 1.0f) noexcept
        : m_duration(std::max(0.001f, duration_seconds)),
          m_lower_bound(lower_bound),
          m_upper_bound(upper_bound),
          m_value(lower_bound),
          m_status(AnimationStatus::Dismissed) {}

    ~AnimationController() override = default;

    // 正向播放至 upper_bound
    void forward(float from = -1.0f) noexcept {
        if (from >= 0.0f) {
            m_value = std::clamp(from, m_lower_bound, m_upper_bound);
        }
        m_status = AnimationStatus::Forward;
        m_is_repeating = false;
        notify_status();
    }

    // 反向倒退至 lower_bound
    void reverse(float from = -1.0f) noexcept {
        if (from >= 0.0f) {
            m_value = std::clamp(from, m_lower_bound, m_upper_bound);
        }
        m_status = AnimationStatus::Reverse;
        m_is_repeating = false;
        notify_status();
    }

    // 循环播放 (reverse_cycle 为 true 时进行往返 ping-pong)
    void repeat(bool reverse_cycle = false) noexcept {
        m_is_repeating = true;
        m_reverse_cycle = reverse_cycle;
        if (m_status != AnimationStatus::Reverse) {
            m_status = AnimationStatus::Forward;
        }
        notify_status();
    }

    // 立即停止
    void stop() noexcept {
        m_is_repeating = false;
        if (m_status == AnimationStatus::Forward || m_status == AnimationStatus::Reverse) {
            m_status = (m_value >= m_upper_bound) ? AnimationStatus::Completed : AnimationStatus::Dismissed;
            notify_status();
        }
    }

    // 重置到起点
    void reset() noexcept {
        m_value = m_lower_bound;
        m_status = AnimationStatus::Dismissed;
        m_is_repeating = false;
        notify_value();
        notify_status();
    }

    // 步进更新
    void update_animation(float dt) override {
        if (m_status != AnimationStatus::Forward && m_status != AnimationStatus::Reverse) {
            return;
        }

        float delta = (m_upper_bound - m_lower_bound) * (dt / m_duration);

        if (m_status == AnimationStatus::Forward) {
            m_value += delta;
            if (m_value >= m_upper_bound) {
                if (m_is_repeating) {
                    if (m_reverse_cycle) {
                        m_value = m_upper_bound;
                        m_status = AnimationStatus::Reverse;
                        notify_status();
                    } else {
                        m_value = m_lower_bound;
                    }
                } else {
                    m_value = m_upper_bound;
                    m_status = AnimationStatus::Completed;
                    notify_status();
                }
            }
        } else if (m_status == AnimationStatus::Reverse) {
            m_value -= delta;
            if (m_value <= m_lower_bound) {
                if (m_is_repeating) {
                    if (m_reverse_cycle) {
                        m_value = m_lower_bound;
                        m_status = AnimationStatus::Forward;
                        notify_status();
                    } else {
                        m_value = m_upper_bound;
                    }
                } else {
                    m_value = m_lower_bound;
                    m_status = AnimationStatus::Dismissed;
                    notify_status();
                }
            }
        }

        notify_value();
    }

    void update(float dt) {
        update_animation(dt);
    }

    [[nodiscard]] bool is_animation_settled() const override {
        return m_status == AnimationStatus::Dismissed || m_status == AnimationStatus::Completed;
    }

    [[nodiscard]] bool is_animating() const noexcept {
        return m_status == AnimationStatus::Forward || m_status == AnimationStatus::Reverse;
    }

    [[nodiscard]] float value() const noexcept { return m_value; }
    [[nodiscard]] AnimationStatus status() const noexcept { return m_status; }
    [[nodiscard]] float duration() const noexcept { return m_duration; }
    void set_duration(float d) noexcept { m_duration = std::max(0.001f, d); }

    void add_listener(Fn<void(float)> listener) {
        m_listeners.push_back(std::move(listener));
    }

    void add_status_listener(Fn<void(AnimationStatus)> listener) {
        m_status_listeners.push_back(std::move(listener));
    }

private:
    void notify_value() {
        for (const auto& cb : m_listeners) {
            if (cb) cb(m_value);
        }
    }

    void notify_status() {
        for (const auto& cb : m_status_listeners) {
            if (cb) cb(m_status);
        }
    }

    float m_duration{1.0f};
    float m_lower_bound{0.0f};
    float m_upper_bound{1.0f};
    float m_value{0.0f};
    AnimationStatus m_status{AnimationStatus::Dismissed};
    bool m_is_repeating{false};
    bool m_reverse_cycle{false};

    std::vector<Fn<void(float)>> m_listeners;
    std::vector<Fn<void(AnimationStatus)>> m_status_listeners;
};

} // namespace ktui
