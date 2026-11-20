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
