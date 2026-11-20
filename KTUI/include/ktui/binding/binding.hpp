#pragma once

#include "property.hpp"
#include "two_way_binding.hpp"
#include "one_way_binding.hpp"
#include "converter_binding.hpp"
#include "computed.hpp"
#include "observable_list.hpp"
#include <type_traits>
#include <utility>

namespace ktui {

// 轻量级声明式绑定引用 (BindRef)
// 专为 C++20 指定初始化器设计：零开销、可拷贝、支持隐式转换
template <typename T>
struct BindRef {
    Property<T>* prop{nullptr};

    constexpr BindRef() noexcept = default;
    constexpr BindRef(Property<T>& p) noexcept : prop(&p) {}
    constexpr BindRef(Property<T>* p) noexcept : prop(p) {}
    constexpr BindRef(std::nullptr_t) noexcept : prop(nullptr) {}
    BindRef(const TwoWayBinding<T>& b) noexcept : prop(b.property()) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return prop != nullptr; }
    [[nodiscard]] constexpr Property<T>* get() const noexcept { return prop; }
    Property<T>* operator->() const noexcept { return prop; }
    Property<T>& operator*() const noexcept { return *prop; }
    explicit constexpr operator bool() const noexcept { return prop != nullptr; }

    bool operator==(const BindRef&) const noexcept = default;
};

// 1. 非常量左值属性 -> 双向数据绑定 (TwoWayBinding)
template <typename T>
auto bind(Property<T>& prop) {
    return TwoWayBinding<T>(prop);
}

// 2. 常量引用属性 -> 单向只读数据绑定 (OneWayBinding)
template <typename T>
auto bind(const Property<T>& prop) {
    return OneWayBinding<T>(prop);
}

// 3. 属性 + 转换器函数 -> 类型转换绑定 (ConverterBinding: T -> R)
template <typename T, typename Func>
auto bind(const Property<T>& prop, Func&& converter) {
    using R = std::invoke_result_t<Func, const T&>;
    return ConverterBinding<T, R>(prop, std::forward<Func>(converter));
}

} // namespace ktui
