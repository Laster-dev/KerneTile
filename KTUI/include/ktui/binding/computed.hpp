#pragma once

#include <functional>
#include <type_traits>
#include <utility>

namespace ktui {

// 派生计算属性 (Computed Property)
template <typename T>
class Computed {
public:
    explicit Computed(Fn<T()> compute_fn)
        : m_compute_fn(std::move(compute_fn)) {}

    T get() const {
        return m_compute_fn ? m_compute_fn() : T{};
    }

    operator T() const {
        return get();
    }

private:
    Fn<T()> m_compute_fn;
};

template <typename Func>
auto computed(Func&& fn) {
    using ReturnType = std::invoke_result_t<Func>;
    return Computed<ReturnType>(std::forward<Func>(fn));
}

} // namespace ktui
