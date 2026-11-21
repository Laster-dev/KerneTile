#pragma once

#include "property.hpp"
#include "two_way_binding.hpp"
#include "one_way_binding.hpp"
#include "converter_binding.hpp"
#include "computed.hpp"
#include "observable_list.hpp"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ktui {

// 类型擦除单向/转换数据源接口 (支持 Bind(prop, converter) 等派生绑定)
template <typename T>
struct IBindingSource {
    virtual ~IBindingSource() = default;
    [[nodiscard]] virtual T get_value() const = 0;
    [[nodiscard]] virtual std::function<void()> subscribe_managed(std::function<void(const T&)> cb) const = 0;
};

// 轻量级声明式绑定引用 (BindRef)
// 专为 C++20 指定初始化器设计：零开销、可拷贝、支持双向 Property<T> 与只读/转换器派生绑定
template <typename T>
struct BindRef {
    Property<T>* prop{nullptr};
    bool read_only{false};
    std::shared_ptr<IBindingSource<T>> source{nullptr};

    constexpr BindRef() noexcept = default;
    constexpr BindRef(Property<T>& p, bool ro = false) noexcept : prop(&p), read_only(ro) {}
    constexpr BindRef(Property<T>* p, bool ro = false) noexcept : prop(p), read_only(ro) {}
    constexpr BindRef(std::nullptr_t) noexcept {}
    BindRef(std::shared_ptr<IBindingSource<T>> src) noexcept : read_only(true), source(std::move(src)) {}
    BindRef(const TwoWayBinding<T>& b) noexcept : prop(b.property()), read_only(false) {}

    [[nodiscard]] bool has_value() const noexcept { return prop != nullptr || source != nullptr; }
    [[nodiscard]] bool is_two_way() const noexcept { return prop != nullptr && !read_only; }
    [[nodiscard]] constexpr Property<T>* get() const noexcept { return prop; }
    Property<T>* operator->() const noexcept { return prop; }
    Property<T>& operator*() const noexcept { return *prop; }
    explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] T get_value() const {
        if (prop) return prop->get();
        if (source) return source->get_value();
        return T{};
    }

    [[nodiscard]] std::function<void()> subscribe_managed(std::function<void(const T&)> cb) const {
        if (prop) {
            Property<T>* p = prop;
            uint64_t token = p->subscribe([cb = std::move(cb)](const T& v, uint64_t) { cb(v); });
            return [p, token]() { p->unsubscribe(token); };
        }
        if (source) {
            return source->subscribe_managed(std::move(cb));
        }
        return nullptr;
    }

    bool operator==(const BindRef& other) const noexcept {
        return prop == other.prop && read_only == other.read_only && source == other.source;
    }
};

// 通用可绑定属性包装器 (Bindable<T>)
// 支持同一字段既可直接传入字面量/值 (.text = "abc")，也可传入绑定表达式 (.text = Bind(prop))
template <typename T>
struct Bindable {
    T value{};
    BindRef<T> ref{nullptr};

    constexpr Bindable() = default;
    constexpr Bindable(const T& v) : value(v) {}
    constexpr Bindable(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) : value(std::move(v)) {}

    template <typename U = T, std::enable_if_t<std::is_same_v<U, std::string>, int> = 0>
    Bindable(const char* s) : value(s ? s : "") {}

    template <typename U = T, std::enable_if_t<std::is_same_v<U, std::string>, int> = 0>
    Bindable(std::string_view sv) : value(sv) {}

    Bindable(BindRef<T> b) : ref(std::move(b)) {
        if (ref) value = ref.get_value();
    }

    Bindable(const TwoWayBinding<T>& b) : ref(b) {
        if (ref) value = ref.get_value();
    }

    [[nodiscard]] bool is_bound() const noexcept { return static_cast<bool>(ref); }
    [[nodiscard]] bool is_two_way() const noexcept { return ref.is_two_way(); }

    [[nodiscard]] T get() const {
        return ref ? ref.get_value() : value;
    }

    void set_bound(const T& v) const {
        if (ref.is_two_way() && ref.prop) {
            ref.prop->set(v);
        }
    }

    operator T() const { return get(); }
};

// 自动管理所有属性绑定的生命周期组 (Widget 析构或更新时自动退订)
struct BindingGroup {
    std::vector<std::function<void()>> unbinders;

    BindingGroup() = default;
    ~BindingGroup() { clear(); }
    BindingGroup(const BindingGroup&) = delete;
    BindingGroup& operator=(const BindingGroup&) = delete;
    BindingGroup(BindingGroup&&) noexcept = default;
    BindingGroup& operator=(BindingGroup&&) noexcept = default;

    void clear() {
        for (auto& fn : unbinders) {
            if (fn) fn();
        }
        unbinders.clear();
    }

    template <typename T, typename NodePtr, typename Setter>
    void attach(const Bindable<T>& b, const NodePtr& node, Setter&& setter) {
        if (!b.is_bound()) return;
        std::weak_ptr<typename NodePtr::element_type> weak = node;
        auto unsub = b.ref.subscribe_managed([weak, setter = std::forward<Setter>(setter)](const T& v) {
            if (auto n = weak.lock()) {
                setter(*n, v);
            }
        });
        if (unsub) {
            unbinders.push_back(std::move(unsub));
        }
    }
};

// ==================== 声明式 Bind(...) 工厂函数 ====================

// 1. 非常量左值属性 -> 双向数据绑定 (BindRef<T>)
template <typename T>
[[nodiscard]] inline BindRef<T> Bind(Property<T>& prop) {
    return BindRef<T>(prop, false);
}

// 2. 常量属性 -> 单向只读数据绑定 (BindRef<T>)
template <typename T>
[[nodiscard]] inline BindRef<T> Bind(const Property<T>& prop) {
    return BindRef<T>(const_cast<Property<T>&>(prop), true);
}

// 3. 属性 + 转换器函数 -> 类型转换只读绑定 (BindRef<R>)
template <typename T, typename Func>
[[nodiscard]] inline auto Bind(const Property<T>& prop, Func&& converter) {
    using R = std::decay_t<std::invoke_result_t<Func, const T&>>;
    struct ConverterSource : IBindingSource<R> {
        Property<T>* src;
        std::decay_t<Func> fn;
        ConverterSource(Property<T>* p, Func&& f) : src(p), fn(std::forward<Func>(f)) {}
        R get_value() const override { return fn(src->get()); }
        std::function<void()> subscribe_managed(std::function<void(const R&)> cb) const override {
            Property<T>* p = src;
            auto f = fn;
            uint64_t token = p->subscribe([cb = std::move(cb), f = std::move(f)](const T& v, uint64_t) {
                cb(f(v));
            });
            return [p, token]() { p->unsubscribe(token); };
        }
    };
    auto s = std::make_shared<ConverterSource>(&const_cast<Property<T>&>(prop), std::forward<Func>(converter));
    return BindRef<R>(std::move(s));
}

// 保留传统独立绑定辅助函数 (供 TwoWayBinding / OneWayBinding / ConverterBinding 单元测试使用)
template <typename T>
auto bind(Property<T>& prop) {
    return TwoWayBinding<T>(prop);
}

template <typename T>
auto bind(const Property<T>& prop) {
    return OneWayBinding<T>(prop);
}

template <typename T, typename Func>
auto bind(const Property<T>& prop, Func&& converter) {
    using R = std::invoke_result_t<Func, const T&>;
    return ConverterBinding<T, R>(prop, std::forward<Func>(converter));
}

} // namespace ktui
