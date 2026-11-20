#pragma once

#include "spring_curve.hpp"
#include <optional>
#include <utility>

namespace ktui {

// 动画事务上下文管理
class AnimationScope {
public:
    [[nodiscard]] static bool is_active() noexcept {
        return s_depth > 0;
    }

    [[nodiscard]] static const SpringConfig& current_config() noexcept {
        return s_current_config;
    }

    explicit AnimationScope(const SpringConfig& config) noexcept
        : m_prev_config(s_current_config) {
        s_current_config = config;
        ++s_depth;
    }

    ~AnimationScope() noexcept {
        --s_depth;
        s_current_config = m_prev_config;
    }

    AnimationScope(const AnimationScope&) = delete;
    AnimationScope& operator=(const AnimationScope&) = delete;
    AnimationScope(AnimationScope&&) = delete;
    AnimationScope& operator=(AnimationScope&&) = delete;

private:
    SpringConfig m_prev_config;
    inline static thread_local int s_depth = 0;
    inline static thread_local SpringConfig s_current_config{};
};

// 事务级隐式动画 API
// 在闭包 action 内部赋值的所有支持动画的属性，均会自动继承指定的弹簧配置并启动物理过渡
template <typename Fn>
void with_animation(const SpringConfig& config, Fn&& fn) {
    AnimationScope scope(config);
    std::forward<Fn>(fn)();
}

} // namespace ktui
