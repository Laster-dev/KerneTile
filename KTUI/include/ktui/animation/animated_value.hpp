#pragma once

#include "animatable.hpp"
#include "spring_presets.hpp"
#include "animation_transaction.hpp"
#include <vector>
#include <functional>
#include <optional>

namespace ktui {

// 动画步进接口
class IAnimationTickable {
public:
    virtual ~IAnimationTickable() = default;
    virtual void update_animation(float dt) = 0;
    [[nodiscard]] virtual bool is_animation_settled() const = 0;
};

// 属性级隐式动画包装器
template <Animatable T>
class AnimatedValue : public IAnimationTickable {
public:
    AnimatedValue() noexcept
        : m_solver(T{}, Springs::Default), m_config(Springs::Default), m_target(T{}), m_current(T{}) {}

    explicit AnimatedValue(const T& initial_val, const SpringConfig& config = Springs::Default) noexcept
        : m_solver(initial_val, config), m_config(config), m_target(initial_val), m_current(initial_val) {}

    ~AnimatedValue() override = default;

    AnimatedValue(const AnimatedValue& other)
        : m_solver(other.m_solver), m_config(other.m_config), m_target(other.m_target), m_current(other.m_current) {}

    AnimatedValue& operator=(const AnimatedValue& other) {
        if (this != &other) {
            set_target(other.m_target, other.m_config);
        }
        return *this;
    }

    AnimatedValue(AnimatedValue&& other) noexcept = default;
    AnimatedValue& operator=(AnimatedValue&& other) noexcept = default;

    // 赋值新目标值，启动或重定向物理弹簧动画
    void set_target(const T& new_target, std::optional<SpringConfig> override_config = std::nullopt) {
        m_target = new_target;
        SpringConfig cfg = override_config.value_or(
            AnimationScope::is_active() ? AnimationScope::current_config() : m_config
        );
        m_config = cfg;
        m_solver.rebind(m_current, cfg, m_target);

        if (m_solver.is_settled()) {
            notify_settled();
        }
    }

    // 赋值运算符直接映射到 set_target
    AnimatedValue& operator=(const T& new_target) {
        set_target(new_target);
        return *this;
    }

    // 瞬移：立即赋给目标值并静止，绝不启动弹簧过渡
    void snap_to(const T& val) {
        m_target = val;
        m_current = val;
        m_solver.reset(val, m_config);
        notify_change();
        notify_settled();
    }

    // 动画步进 (通常由帧时钟调用)
    void update_animation(float dt) override {
        if (m_solver.is_settled()) return;

        m_solver.update(dt);
        m_current = m_solver.value();
        notify_change();

        if (m_solver.is_settled()) {
            notify_settled();
        }
    }

    // 方便手动推进的方法别名
    void update(float dt) {
        update_animation(dt);
    }

    [[nodiscard]] bool is_animation_settled() const override {
        return m_solver.is_settled();
    }

    [[nodiscard]] bool is_settled() const noexcept {
        return m_solver.is_settled();
    }

    [[nodiscard]] const T& get() const noexcept { return m_current; }
    [[nodiscard]] operator const T&() const noexcept { return m_current; }
    [[nodiscard]] const T& target() const noexcept { return m_target; }
    [[nodiscard]] const SpringConfig& config() const noexcept { return m_config; }

    void set_config(const SpringConfig& cfg) noexcept {
        m_config = cfg;
    }

    // 监听值变化
    void on_change(std::function<void(const T&)> callback) {
        m_listeners.push_back(std::move(callback));
    }

    // 监听动画收敛静止事件
    void on_settled(std::function<void()> callback) {
        m_settled_listeners.push_back(std::move(callback));
    }

    bool operator==(const T& other) const noexcept {
        return m_current == other;
    }

    bool operator==(const AnimatedValue& other) const noexcept {
        return m_current == other.m_current && m_target == other.m_target;
    }

private:
    void notify_change() {
        for (const auto& cb : m_listeners) {
            if (cb) cb(m_current);
        }
    }

    void notify_settled() {
        for (const auto& cb : m_settled_listeners) {
            if (cb) cb();
        }
    }

    MultiSpringSolver<T> m_solver;
    SpringConfig m_config;
    T m_target{};
    T m_current{};
    std::vector<std::function<void(const T&)>> m_listeners;
    std::vector<std::function<void()>> m_settled_listeners;
};

// 隐式属性动画辅助构造函数
template <Animatable T>
[[nodiscard]] inline AnimatedValue<T> animate(const T& target, const SpringConfig& spring = Springs::Default) {
    return AnimatedValue<T>(target, spring);
}

} // namespace ktui
