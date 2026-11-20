#pragma once

#include "spring_curve.hpp"
#include <ktui/pal/platform_types.hpp>
#include <array>
#include <cstddef>
#include <type_traits>
#include <concepts>

namespace ktui {

template <typename T>
struct AnimatableTraits;

// float 特化
template <>
struct AnimatableTraits<float> {
    static constexpr size_t Channels = 1;
    static void extract(const float& val, float* out) noexcept { out[0] = val; }
    static float assemble(const float* in) noexcept { return in[0]; }
};

// double 特化
template <>
struct AnimatableTraits<double> {
    static constexpr size_t Channels = 1;
    static void extract(const double& val, float* out) noexcept { out[0] = static_cast<float>(val); }
    static double assemble(const float* in) noexcept { return static_cast<double>(in[0]); }
};

// Vec2 特化
template <>
struct AnimatableTraits<Vec2> {
    static constexpr size_t Channels = 2;
    static void extract(const Vec2& val, float* out) noexcept {
        out[0] = val.x;
        out[1] = val.y;
    }
    static Vec2 assemble(const float* in) noexcept {
        return Vec2{in[0], in[1]};
    }
};

// Rect 特化
template <>
struct AnimatableTraits<Rect> {
    static constexpr size_t Channels = 4;
    static void extract(const Rect& val, float* out) noexcept {
        out[0] = val.min_x;
        out[1] = val.min_y;
        out[2] = val.max_x;
        out[3] = val.max_y;
    }
    static Rect assemble(const float* in) noexcept {
        return Rect{in[0], in[1], in[2], in[3]};
    }
};

// Color 特化
template <>
struct AnimatableTraits<Color> {
    static constexpr size_t Channels = 4;
    static void extract(const Color& val, float* out) noexcept {
        out[0] = val.r;
        out[1] = val.g;
        out[2] = val.b;
        out[3] = val.a;
    }
    static Color assemble(const float* in) noexcept {
        return Color{in[0], in[1], in[2], in[3]};
    }
};

// EdgeInsets 特化
template <>
struct AnimatableTraits<EdgeInsets> {
    static constexpr size_t Channels = 4;
    static void extract(const EdgeInsets& val, float* out) noexcept {
        out[0] = val.top;
        out[1] = val.right;
        out[2] = val.bottom;
        out[3] = val.left;
    }
    static EdgeInsets assemble(const float* in) noexcept {
        return EdgeInsets{in[0], in[1], in[2], in[3]};
    }
};

template <typename T>
concept Animatable = requires {
    typename AnimatableTraits<std::decay_t<T>>;
    { AnimatableTraits<std::decay_t<T>>::Channels } -> std::convertible_to<size_t>;
};

// 支持任意多维 Animatable 类型的多通道物理弹簧解析求解器
template <Animatable T>
class MultiSpringSolver {
public:
    using Traits = AnimatableTraits<std::decay_t<T>>;
    static constexpr size_t Channels = Traits::Channels;

    MultiSpringSolver() noexcept = default;

    explicit MultiSpringSolver(const T& initial_val, const SpringConfig& config = SpringConfig{}) noexcept {
        reset(initial_val, config);
    }

    void reset(const T& val, const SpringConfig& config = SpringConfig{}) noexcept {
        m_config = config;
        float ch[Channels];
        Traits::extract(val, ch);
        for (size_t i = 0; i < Channels; ++i) {
            m_solvers[i] = SpringSolver(ch[i], config);
        }
    }

    void rebind(const T& current_val, const SpringConfig& config, const T& new_target) noexcept {
        m_config = config;
        float cur_ch[Channels];
        float tgt_ch[Channels];
        Traits::extract(current_val, cur_ch);
        Traits::extract(new_target, tgt_ch);
        for (size_t i = 0; i < Channels; ++i) {
            m_solvers[i].set_config(config);
            m_solvers[i].rebind(cur_ch[i], m_solvers[i].velocity(), tgt_ch[i]);
        }
    }

    void update(float dt) noexcept {
        if (is_settled()) return;
        for (size_t i = 0; i < Channels; ++i) {
            m_solvers[i].update(dt);
        }
    }

    [[nodiscard]] bool is_settled() const noexcept {
        for (size_t i = 0; i < Channels; ++i) {
            if (!m_solvers[i].is_settled()) return false;
        }
        return true;
    }

    [[nodiscard]] T value() const noexcept {
        float ch[Channels];
        for (size_t i = 0; i < Channels; ++i) {
            ch[i] = m_solvers[i].value();
        }
        return Traits::assemble(ch);
    }

    [[nodiscard]] T target() const noexcept {
        float ch[Channels];
        for (size_t i = 0; i < Channels; ++i) {
            ch[i] = m_solvers[i].target();
        }
        return Traits::assemble(ch);
    }

    [[nodiscard]] float channel_velocity(size_t index) const noexcept {
        return index < Channels ? m_solvers[index].velocity() : 0.0f;
    }

    [[nodiscard]] const SpringConfig& config() const noexcept { return m_config; }

private:
    SpringConfig m_config;
    std::array<SpringSolver, Channels> m_solvers;
};

} // namespace ktui
