#pragma once

#include "property.hpp"
#include "two_way_binding.hpp"
#include "one_way_binding.hpp"
#include "converter_binding.hpp"
#include "computed.hpp"
#include "observable_list.hpp"
#include <ktui/widgets/widget.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ktui {

// 自动管理所有属性绑定的生命周期组 (Widget 析构或更新时自动退订)
struct BindingGroup {
    std::vector<Fn<void()>> unbinders;

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

    void add_unbinder(Fn<void()> unbinder) {
        if (unbinder) {
            unbinders.push_back(std::move(unbinder));
        }
    }

    template <typename T, typename Func>
    void subscribe(const Property<T>& prop, Func&& fn) {
        Property<T>* p = &const_cast<Property<T>&>(prop);
        auto alive = p->alive_token();
        uint64_t token = p->subscribe([f = std::forward<Func>(fn)](const T& v, uint64_t) {
            f(v);
        });
        unbinders.push_back([p, token, alive = std::move(alive)]() {
            if (auto guard = alive.lock(); guard && *guard) {
                p->unsubscribe(token);
            }
        });
    }
};

// ==================== 零开销按需链式绑定系统 (Zero-Cost Fluent Binding) ====================
// 1. 控件结构体只存原生 C++ 字段，零 Bindable<T> 包装、零非平凡析构膨胀
// 2. 未调用 .bind(...) / .bind_two_way(...) 的属性和控件完全不实例化绑定模板（二进制 0 字节开销）
// 3. 任意控件的任意 RenderNode 属性（成员 setter / lambda / 转换器 / 双向绑定）均可按需链式绑定

template <typename RenderT>
class BoundWidget final : public Widget {
public:
    using HookFn = Fn<void(const std::shared_ptr<RenderT>&, BindingGroup&)>;

    explicit BoundWidget(WidgetPtr inner) : m_inner(std::move(inner)) {}
    ~BoundWidget() override = default;

    void add_hook(HookFn hook) {
        if (hook) m_hooks.push_back(std::move(hook));
    }

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = m_inner ? m_inner->create_render_node() : nullptr;
        apply_hooks(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (m_inner) m_inner->update_render_node(node);
        apply_hooks(node);
    }

private:
    void apply_hooks(const std::shared_ptr<RenderBox>& node) {
        m_bindings.clear();
        if (auto typed = std::static_pointer_cast<RenderT>(node)) {
            for (const auto& hook : m_hooks) {
                hook(typed, m_bindings);
            }
        }
    }

    WidgetPtr m_inner;
    std::vector<HookFn> m_hooks;
    BindingGroup m_bindings;
};

template <typename RenderT>
class BoundWidgetBuilder {
public:
    explicit BoundWidgetBuilder(WidgetPtr inner)
        : m_widget(std::static_pointer_cast<BoundWidget<RenderT>>(make_widget<BoundWidget<RenderT>>(std::move(inner)))) {}

    // 1. 成员 setter + Property<T> 单向绑定: .bind(&RenderText::set_text, app.title)
    template <typename Setter, typename T,
              std::enable_if_t<std::is_member_function_pointer_v<Setter>, int> = 0>
    BoundWidgetBuilder& bind(Setter setter, const Property<T>& prop) {
        Property<T>* p = &const_cast<Property<T>&>(prop);
        m_widget->add_hook([setter, p](const std::shared_ptr<RenderT>& node, BindingGroup& bg) {
            ((*node).*setter)(p->get());
            std::weak_ptr<RenderT> weak = node;
            bg.subscribe(*p, [weak, setter](const T& v) {
                if (auto n = weak.lock()) ((*n).*setter)(v);
            });
        });
        return *this;
    }

    // 2. 成员 setter + Property<T> + 转换器: .bind(&RenderText::set_text, app.count, [](int v){ ... })
    template <typename Setter, typename T, typename Converter,
              std::enable_if_t<std::is_member_function_pointer_v<Setter>, int> = 0>
    BoundWidgetBuilder& bind(Setter setter, const Property<T>& prop, Converter&& converter) {
        Property<T>* p = &const_cast<Property<T>&>(prop);
        m_widget->add_hook(
            [setter, p, conv = std::forward<Converter>(converter)](const std::shared_ptr<RenderT>& node, BindingGroup& bg) {
                ((*node).*setter)(conv(p->get()));
                std::weak_ptr<RenderT> weak = node;
                bg.subscribe(*p, [weak, setter, conv](const T& v) {
                    if (auto n = weak.lock()) ((*n).*setter)(conv(v));
                });
            });
        return *this;
    }

    // 3. 通用 Lambda 绑定任意属性/多属性: .bind(app.prop, [](RenderT& n, const T& v){ ... })
    template <typename T, typename Applier>
    BoundWidgetBuilder& bind(const Property<T>& prop, Applier&& applier) {
        Property<T>* p = &const_cast<Property<T>&>(prop);
        m_widget->add_hook(
            [p, fn = std::forward<Applier>(applier)](const std::shared_ptr<RenderT>& node, BindingGroup& bg) {
                fn(*node, p->get());
                std::weak_ptr<RenderT> weak = node;
                bg.subscribe(*p, [weak, fn](const T& v) {
                    if (auto n = weak.lock()) fn(*n, v);
                });
            });
        return *this;
    }

    // 4. 输入控件默认主值双向绑定: .bind_two_way(app.checked)
    // 通过 ADL 调用各控件按需定义的 ktui_bind_two_way(node, prop, bg)，未调用则零实例化
    template <typename T>
    BoundWidgetBuilder& bind_two_way(Property<T>& prop) {
        Property<T>* p = &prop;
        m_widget->add_hook([p](const std::shared_ptr<RenderT>& node, BindingGroup& bg) {
            ktui_bind_two_way(node, *p, bg);
        });
        return *this;
    }

    // 5. 通用显式双向绑定: .bind_two_way(setter, attach_change, prop)
    template <typename Setter, typename ChangeAttacher, typename T>
    BoundWidgetBuilder& bind_two_way(Setter&& setter, ChangeAttacher&& attach_change, Property<T>& prop) {
        Property<T>* p = &prop;
        m_widget->add_hook(
            [p, s = std::forward<Setter>(setter), a = std::forward<ChangeAttacher>(attach_change)](
                const std::shared_ptr<RenderT>& node, BindingGroup& bg) {
                s(*node, p->get());
                std::weak_ptr<RenderT> weak = node;
                bg.subscribe(*p, [weak, s](const T& v) {
                    if (auto n = weak.lock()) s(*n, v);
                });
                a(*node, [p](const T& v) { p->set(v); });
            });
        return *this;
    }

    operator WidgetPtr() const { return m_widget; }

private:
    std::shared_ptr<BoundWidget<RenderT>> m_widget;
};

// 空基类 CRTP 混入 (0 字节开销，保持 C++20 聚合指定初始化器完全兼容)
template <typename Derived, typename RenderT>
struct BindableMixin {
    template <typename Setter, typename T,
              std::enable_if_t<std::is_member_function_pointer_v<Setter>, int> = 0>
    BoundWidgetBuilder<RenderT> bind(Setter setter, const Property<T>& prop) && {
        BoundWidgetBuilder<RenderT> builder(static_cast<Derived&&>(*this));
        builder.bind(setter, prop);
        return builder;
    }

    template <typename Setter, typename T,
              std::enable_if_t<std::is_member_function_pointer_v<Setter>, int> = 0>
    BoundWidgetBuilder<RenderT> bind(Setter setter, const Property<T>& prop) const& {
        BoundWidgetBuilder<RenderT> builder(static_cast<const Derived&>(*this));
        builder.bind(setter, prop);
        return builder;
    }

    template <typename Setter, typename T, typename Converter,
              std::enable_if_t<std::is_member_function_pointer_v<Setter>, int> = 0>
    BoundWidgetBuilder<RenderT> bind(Setter setter, const Property<T>& prop, Converter&& converter) && {
        BoundWidgetBuilder<RenderT> builder(static_cast<Derived&&>(*this));
        builder.bind(setter, prop, std::forward<Converter>(converter));
        return builder;
    }

    template <typename Setter, typename T, typename Converter,
              std::enable_if_t<std::is_member_function_pointer_v<Setter>, int> = 0>
    BoundWidgetBuilder<RenderT> bind(Setter setter, const Property<T>& prop, Converter&& converter) const& {
        BoundWidgetBuilder<RenderT> builder(static_cast<const Derived&>(*this));
        builder.bind(setter, prop, std::forward<Converter>(converter));
        return builder;
    }

    template <typename T, typename Applier>
    BoundWidgetBuilder<RenderT> bind(const Property<T>& prop, Applier&& applier) && {
        BoundWidgetBuilder<RenderT> builder(static_cast<Derived&&>(*this));
        builder.bind(prop, std::forward<Applier>(applier));
        return builder;
    }

    template <typename T, typename Applier>
    BoundWidgetBuilder<RenderT> bind(const Property<T>& prop, Applier&& applier) const& {
        BoundWidgetBuilder<RenderT> builder(static_cast<const Derived&>(*this));
        builder.bind(prop, std::forward<Applier>(applier));
        return builder;
    }

    template <typename T>
    BoundWidgetBuilder<RenderT> bind_two_way(Property<T>& prop) && {
        BoundWidgetBuilder<RenderT> builder(static_cast<Derived&&>(*this));
        builder.bind_two_way(prop);
        return builder;
    }

    template <typename T>
    BoundWidgetBuilder<RenderT> bind_two_way(Property<T>& prop) const& {
        BoundWidgetBuilder<RenderT> builder(static_cast<const Derived&>(*this));
        builder.bind_two_way(prop);
        return builder;
    }

    template <typename Setter, typename ChangeAttacher, typename T>
    BoundWidgetBuilder<RenderT> bind_two_way(Setter&& setter, ChangeAttacher&& attach_change, Property<T>& prop) && {
        BoundWidgetBuilder<RenderT> builder(static_cast<Derived&&>(*this));
        builder.bind_two_way(std::forward<Setter>(setter), std::forward<ChangeAttacher>(attach_change), prop);
        return builder;
    }

    template <typename Setter, typename ChangeAttacher, typename T>
    BoundWidgetBuilder<RenderT> bind_two_way(Setter&& setter, ChangeAttacher&& attach_change, Property<T>& prop) const& {
        BoundWidgetBuilder<RenderT> builder(static_cast<const Derived&>(*this));
        builder.bind_two_way(std::forward<Setter>(setter), std::forward<ChangeAttacher>(attach_change), prop);
        return builder;
    }
};

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
