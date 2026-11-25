#pragma once

#include "easing.hpp"
#include "cubic_bezier.hpp"
#include "spring_curve.hpp"
#include "spring_presets.hpp"
#include "animated_value.hpp"
#include <ktui/pal/platform_types.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace ktui {

// 统一动效算法与插值计算引擎 (Motion & Animation Core Engine)
// 任何控件/UI元素严禁私自硬编码数学曲线或插值公式，必须统一调用此类
class Motion {
public:
    // ==================== 1. 基础线性插值 (Lerp) ====================

    template <typename T>
    static constexpr T lerp(const T& a, const T& b, float t) noexcept {
        return static_cast<T>(a + (b - a) * t);
    }

    static constexpr Color lerp(const Color& a, const Color& b, float t) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color{
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        };
    }

    static constexpr Rect lerp(const Rect& a, const Rect& b, float t) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        return Rect{
            a.min_x + (b.min_x - a.min_x) * t,
            a.min_y + (b.min_y - a.min_y) * t,
            a.max_x + (b.max_x - a.max_x) * t,
            a.max_y + (b.max_y - a.max_y) * t
        };
    }

    static constexpr Vec2 lerp(const Vec2& a, const Vec2& b, float t) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        return Vec2{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t
        };
    }

    // ==================== 2. 曲线求值 (Curve Evaluation) ====================

    // 动态标准缓动曲线求值
    static float evaluate(Easing easing, float t) noexcept {
        return evaluate_easing(easing, t);
    }

    // 贝塞尔曲线求值 (支持自定义控制点与预设)
    static float evaluate(const CubicBezier& curve, float t) noexcept {
        return curve.evaluate(t);
    }

    // 编译期静态零开销曲线求值 (用于对极端性能有极致要求的场景)
    template <typename CurveTag>
    static constexpr float evaluate(float t) noexcept {
        return CurveTag::evaluate(t);
    }

    // ==================== 3. 缓动带曲线插值 (Curve Lerp) ====================

    template <typename T>
    static T lerp_ease(const T& a, const T& b, float t, Easing easing) noexcept {
        return lerp(a, b, evaluate(easing, t));
    }

    template <typename T>
    static T lerp_bezier(const T& a, const T& b, float t, const CubicBezier& curve) noexcept {
        return lerp(a, b, evaluate(curve, t));
    }

    template <typename CurveTag, typename T>
    static constexpr T lerp_curve(const T& a, const T& b, float t) noexcept {
        return lerp(a, b, evaluate<CurveTag>(t));
    }
};

// 紧致轻量动效补间驱动器 (MotionTween)
// 专为控件内部微动效（如水波纹扩散、淡入淡出、尺寸过渡）设计，零堆分配、按需步进
template <typename T>
class MotionTween {
public:
    MotionTween() noexcept = default;

    MotionTween(T from, T to, float duration_seconds, Easing easing = Easing::EaseOutCubic) noexcept
        : m_from(from), m_to(to), m_duration(std::max(0.0001f, duration_seconds)), m_easing(easing) {}

    // 启动/重启标准缓动补间
    void start(T from, T to, float duration_seconds, Easing easing = Easing::EaseOutCubic) noexcept {
        m_from = from;
        m_to = to;
        m_duration = std::max(0.0001f, duration_seconds);
        m_easing = easing;
        m_use_bezier = false;
        m_elapsed = 0.0f;
        m_active = true;
    }

    // 启动/重启三次贝塞尔补间
    void start_bezier(T from, T to, float duration_seconds, const CubicBezier& curve) noexcept {
        m_from = from;
        m_to = to;
        m_duration = std::max(0.0001f, duration_seconds);
        m_bezier = curve;
        m_use_bezier = true;
        m_elapsed = 0.0f;
        m_active = true;
    }

    // 时间推进步进，返回是否仍处于运行中
    bool tick(float dt) noexcept {
        if (!m_active) return false;
        m_elapsed += dt;
        if (m_elapsed >= m_duration) {
            m_elapsed = m_duration;
            m_active = false;
        }
        return m_active;
    }

    // 计算当前时刻的归一化动效进度 (经过曲线转换后)
    [[nodiscard]] float progress() const noexcept {
        float t = std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
        return m_use_bezier ? Motion::evaluate(m_bezier, t) : Motion::evaluate(m_easing, t);
    }

    // 获取当前时刻的插值结果
    [[nodiscard]] T current_value() const noexcept {
        return Motion::lerp(m_from, m_to, progress());
    }

    [[nodiscard]] bool is_active() const noexcept { return m_active; }
    [[nodiscard]] bool is_completed() const noexcept { return !m_active && m_elapsed >= m_duration; }
    void stop() noexcept { m_active = false; }
    void reset() noexcept { m_elapsed = 0.0f; m_active = false; }
    [[nodiscard]] float duration() const noexcept { return m_duration; }
    [[nodiscard]] float elapsed() const noexcept { return m_elapsed; }

private:
    T m_from{};
    T m_to{};
    float m_duration{0.3f};
    float m_elapsed{0.0f};
    Easing m_easing{Easing::EaseOutCubic};
    CubicBezier m_bezier{0.0f, 0.0f, 1.0f, 1.0f};
    bool m_use_bezier{false};
    bool m_active{false};
};

// 全局/宿主动画管理器 (AnimationManager)
// 集中托管处于活动状态的动画对象，统一由事件循环或 VSync 驱动，确保闲置时真正 0% CPU
class AnimationManager {
public:
    static AnimationManager& instance() noexcept {
        static AnimationManager s_instance;
        return s_instance;
    }

    // 注册动画 Tickable 目标
    void register_tickable(IAnimationTickable* tickable) {
        if (tickable) {
            auto it = std::find(m_tickables.begin(), m_tickables.end(), tickable);
            if (it == m_tickables.end()) {
                m_tickables.push_back(tickable);
            }
        }
    }

    // 注销动画 Tickable 目标 (避免 std::remove 拖入 MSVC 9KB SIMD 查表)
    void unregister_tickable(IAnimationTickable* tickable) {
        if (!tickable) return;
        for (size_t i = 0; i < m_tickables.size(); ++i) {
            if (m_tickables[i] == tickable) {
                m_tickables.erase(m_tickables.begin() + static_cast<ptrdiff_t>(i));
                return;
            }
        }
    }

    // 统一时钟步进，返回是否仍有动画处于激活状态
    bool tick_all(float dt) {
        bool any_active = false;
        // 采用副本遍历，防止回调中动态注销导致迭代器失效
        auto current_list = m_tickables;
        for (auto* item : current_list) {
            if (item) {
                item->update_animation(dt);
                if (!item->is_animation_settled()) {
                    any_active = true;
                }
            }
        }
        return any_active;
    }

    [[nodiscard]] bool has_active_animations() const noexcept {
        for (const auto* item : m_tickables) {
            if (item && !item->is_animation_settled()) {
                return true;
            }
        }
        return false;
    }

private:
    AnimationManager() = default;
    std::vector<IAnimationTickable*> m_tickables;
};

} // namespace ktui
