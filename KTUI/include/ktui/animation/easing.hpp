#pragma once

#include <cmath>
#include <algorithm>
#include <numbers>

namespace ktui {

// 缓动曲线枚举 (覆盖 CSS Transitions / Flutter Curves / Material / Web Animations 完整工业标准)
enum class Easing {
    Linear,
    // 正弦曲线
    EaseInSine,
    EaseOutSine,
    EaseInOutSine,
    // 二次曲线
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    // 三次曲线
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    // 四次曲线
    EaseInQuart,
    EaseOutQuart,
    EaseInOutQuart,
    // 五次曲线
    EaseInQuint,
    EaseOutQuint,
    EaseInOutQuint,
    // 指数曲线
    EaseInExpo,
    EaseOutExpo,
    EaseInOutExpo,
    // 圆形曲线
    EaseInCirc,
    EaseOutCirc,
    EaseInOutCirc,
    // 回弹越冲曲线
    EaseInBack,
    EaseOutBack,
    EaseInOutBack,
    // 弹簧阻尼弹性曲线
    EaseInElastic,
    EaseOutElastic,
    EaseInOutElastic,
    // 弹跳小球衰减曲线
    EaseInBounce,
    EaseOutBounce,
    EaseInOutBounce
};

// ==================== 按需编译零开销内联函数库 ====================
// 所有曲线公式均为 inline/constexpr 函数与仿函数模板结构体。
// 编译期未引用的算法在编译器优化阶段由链接器完全裁剪 (/OPT:REF / --gc-sections)，确保零死代码开销！

namespace easings {

inline constexpr float kPi = 3.14159265358979323846f;

// 1. 线性
inline constexpr float linear(float t) noexcept {
    return std::clamp(t, 0.0f, 1.0f);
}

// 2. 正弦 Sine
inline float ease_in_sine(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - std::cos((t * kPi) * 0.5f);
}

inline float ease_out_sine(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return std::sin((t * kPi) * 0.5f);
}

inline float ease_in_out_sine(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return -(std::cos(kPi * t) - 1.0f) * 0.5f;
}

// 3. 二次 Quad
inline constexpr float ease_in_quad(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t;
}

inline constexpr float ease_out_quad(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

inline constexpr float ease_in_out_quad(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return (t < 0.5f) ? (2.0f * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f);
}

// 4. 三次 Cubic (Fluent / Web 最常用缓动基元)
inline constexpr float ease_in_cubic(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t;
}

inline constexpr float ease_out_cubic(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

inline constexpr float ease_in_out_cubic(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return (t < 0.5f) ? (4.0f * t * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f);
}

// 5. 四次 Quart
inline constexpr float ease_in_quart(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t * t;
}

inline constexpr float ease_out_quart(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv * inv;
}

inline constexpr float ease_in_out_quart(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return (t < 0.5f) ? (8.0f * t * t * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) * 0.5f);
}

// 6. 五次 Quint
inline constexpr float ease_in_quint(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t * t * t;
}

inline constexpr float ease_out_quint(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv * inv * inv;
}

inline constexpr float ease_in_out_quint(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return (t < 0.5f) ? (16.0f * t * t * t * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) * 0.5f);
}

// 7. 指数 Expo
inline float ease_in_expo(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return (t <= 0.0f) ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
}

inline float ease_out_expo(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return (t >= 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

inline float ease_in_out_expo(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return (t < 0.5f) ? (std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f)
                      : ((2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f);
}

// 8. 圆形 Circ
inline float ease_in_circ(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - std::sqrt(1.0f - t * t);
}

inline float ease_out_circ(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = t - 1.0f;
    return std::sqrt(1.0f - inv * inv);
}

inline float ease_in_out_circ(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return (t < 0.5f) ? ((1.0f - std::sqrt(1.0f - std::pow(2.0f * t, 2.0f))) * 0.5f)
                      : ((std::sqrt(1.0f - std::pow(-2.0f * t + 2.0f, 2.0f)) + 1.0f) * 0.5f);
}

// 9. 回弹越冲 Back
inline constexpr float ease_in_back(float t, float overshoot = 1.70158f) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    float c3 = overshoot + 1.0f;
    return c3 * t * t * t - overshoot * t * t;
}

inline constexpr float ease_out_back(float t, float overshoot = 1.70158f) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    float c3 = overshoot + 1.0f;
    float inv = t - 1.0f;
    return 1.0f + c3 * inv * inv * inv + overshoot * inv * inv;
}

inline constexpr float ease_in_out_back(float t, float overshoot = 1.70158f) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    float c2 = overshoot * 1.525f;
    return (t < 0.5f) ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2) * 0.5f)
                      : ((std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) * 0.5f);
}

// 10. 弹簧阻尼弹性 Elastic
inline float ease_in_elastic(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    constexpr float c4 = (2.0f * kPi) / 3.0f;
    return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
}

inline float ease_out_elastic(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    constexpr float c4 = (2.0f * kPi) / 3.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

inline float ease_in_out_elastic(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    constexpr float c5 = (2.0f * kPi) / 4.5f;
    return (t < 0.5f) ? (-(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) * 0.5f)
                      : ((std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) * 0.5f + 1.0f);
}

// 11. 弹跳小球衰减 Bounce
inline constexpr float ease_out_bounce(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;

    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

inline constexpr float ease_in_bounce(float t) noexcept {
    return 1.0f - ease_out_bounce(1.0f - t);
}

inline constexpr float ease_in_out_bounce(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return (t < 0.5f) ? ((1.0f - ease_out_bounce(1.0f - 2.0f * t)) * 0.5f)
                      : ((1.0f + ease_out_bounce(2.0f * t - 1.0f)) * 0.5f);
}

} // namespace easings

// 编译期曲线策略模板标签 (按需实例化，零虚函数开销)
struct LinearCurve { static constexpr float evaluate(float t) noexcept { return easings::linear(t); } };
struct EaseInSineCurve { static float evaluate(float t) noexcept { return easings::ease_in_sine(t); } };
struct EaseOutSineCurve { static float evaluate(float t) noexcept { return easings::ease_out_sine(t); } };
struct EaseInOutSineCurve { static float evaluate(float t) noexcept { return easings::ease_in_out_sine(t); } };

struct EaseInQuadCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_quad(t); } };
struct EaseOutQuadCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_out_quad(t); } };
struct EaseInOutQuadCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_out_quad(t); } };

struct EaseInCubicCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_cubic(t); } };
struct EaseOutCubicCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_out_cubic(t); } };
struct EaseInOutCubicCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_out_cubic(t); } };

struct EaseInQuartCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_quart(t); } };
struct EaseOutQuartCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_out_quart(t); } };
struct EaseInOutQuartCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_out_quart(t); } };

struct EaseInQuintCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_quint(t); } };
struct EaseOutQuintCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_out_quint(t); } };
struct EaseInOutQuintCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_out_quint(t); } };

struct EaseInExpoCurve { static float evaluate(float t) noexcept { return easings::ease_in_expo(t); } };
struct EaseOutExpoCurve { static float evaluate(float t) noexcept { return easings::ease_out_expo(t); } };
struct EaseInOutExpoCurve { static float evaluate(float t) noexcept { return easings::ease_in_out_expo(t); } };

struct EaseInCircCurve { static float evaluate(float t) noexcept { return easings::ease_in_circ(t); } };
struct EaseOutCircCurve { static float evaluate(float t) noexcept { return easings::ease_out_circ(t); } };
struct EaseInOutCircCurve { static float evaluate(float t) noexcept { return easings::ease_in_out_circ(t); } };

struct EaseInBackCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_back(t); } };
struct EaseOutBackCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_out_back(t); } };
struct EaseInOutBackCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_out_back(t); } };

struct EaseInElasticCurve { static float evaluate(float t) noexcept { return easings::ease_in_elastic(t); } };
struct EaseOutElasticCurve { static float evaluate(float t) noexcept { return easings::ease_out_elastic(t); } };
struct EaseInOutElasticCurve { static float evaluate(float t) noexcept { return easings::ease_in_out_elastic(t); } };

struct EaseInBounceCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_bounce(t); } };
struct EaseOutBounceCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_out_bounce(t); } };
struct EaseInOutBounceCurve { static constexpr float evaluate(float t) noexcept { return easings::ease_in_out_bounce(t); } };

// 统一动态求值入口 (Dynamic Dispatch)
inline float evaluate_easing(Easing easing, float t) noexcept {
    switch (easing) {
        case Easing::Linear: return easings::linear(t);
        case Easing::EaseInSine: return easings::ease_in_sine(t);
        case Easing::EaseOutSine: return easings::ease_out_sine(t);
        case Easing::EaseInOutSine: return easings::ease_in_out_sine(t);
        case Easing::EaseInQuad: return easings::ease_in_quad(t);
        case Easing::EaseOutQuad: return easings::ease_out_quad(t);
        case Easing::EaseInOutQuad: return easings::ease_in_out_quad(t);
        case Easing::EaseInCubic: return easings::ease_in_cubic(t);
        case Easing::EaseOutCubic: return easings::ease_out_cubic(t);
        case Easing::EaseInOutCubic: return easings::ease_in_out_cubic(t);
        case Easing::EaseInQuart: return easings::ease_in_quart(t);
        case Easing::EaseOutQuart: return easings::ease_out_quart(t);
        case Easing::EaseInOutQuart: return easings::ease_in_out_quart(t);
        case Easing::EaseInQuint: return easings::ease_in_quint(t);
        case Easing::EaseOutQuint: return easings::ease_out_quint(t);
        case Easing::EaseInOutQuint: return easings::ease_in_out_quint(t);
        case Easing::EaseInExpo: return easings::ease_in_expo(t);
        case Easing::EaseOutExpo: return easings::ease_out_expo(t);
        case Easing::EaseInOutExpo: return easings::ease_in_out_expo(t);
        case Easing::EaseInCirc: return easings::ease_in_circ(t);
        case Easing::EaseOutCirc: return easings::ease_out_circ(t);
        case Easing::EaseInOutCirc: return easings::ease_in_out_circ(t);
        case Easing::EaseInBack: return easings::ease_in_back(t);
        case Easing::EaseOutBack: return easings::ease_out_back(t);
        case Easing::EaseInOutBack: return easings::ease_in_out_back(t);
        case Easing::EaseInElastic: return easings::ease_in_elastic(t);
        case Easing::EaseOutElastic: return easings::ease_out_elastic(t);
        case Easing::EaseInOutElastic: return easings::ease_in_out_elastic(t);
        case Easing::EaseInBounce: return easings::ease_in_bounce(t);
        case Easing::EaseOutBounce: return easings::ease_out_bounce(t);
        case Easing::EaseInOutBounce: return easings::ease_in_out_bounce(t);
        default: return easings::linear(t);
    }
}

} // namespace ktui
