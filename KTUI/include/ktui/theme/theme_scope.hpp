#pragma once

#include "theme.hpp"
#include <vector>
#include <optional>
#include <functional>

namespace ktui {

class Application;

// 级联主题上下文管理器 (Thread-Local Cascade Theme Context)
class ThemeContext {
public:
    // 获取当前生效的主题 (优先继承局部作用域，否则回退全局应用主题)
    [[nodiscard]] static const Theme& current() noexcept;

    // 压入局部覆盖主题
    static void push(const Theme& theme) {
        scope_stack().push_back(theme);
    }

    // 弹出局部覆盖主题
    static void pop() {
        if (!scope_stack().empty()) {
            scope_stack().pop_back();
        }
    }

    // 检查是否存在局部作用域覆盖
    [[nodiscard]] static bool has_local_scope() noexcept {
        return !scope_stack().empty();
    }

    // 局部栈深度
    [[nodiscard]] static size_t scope_depth() noexcept {
        return scope_stack().size();
    }

    // 设置默认回退静态主题 (当无 Application 单例且无局部栈时生效)
    static void set_fallback_theme(const Theme& theme) noexcept {
        fallback_theme() = theme;
    }

private:
    static std::vector<Theme>& scope_stack() {
        thread_local std::vector<Theme> s_stack;
        return s_stack;
    }

    static Theme& fallback_theme() noexcept {
        static Theme s_default = Theme::dark();
        return s_default;
    }

    friend class Application;
};

// RAII 风格主题作用域守卫
class ThemeScopeGuard {
public:
    explicit ThemeScopeGuard(const Theme& theme) {
        ThemeContext::push(theme);
    }

    ~ThemeScopeGuard() {
        ThemeContext::pop();
    }

    ThemeScopeGuard(const ThemeScopeGuard&) = delete;
    ThemeScopeGuard& operator=(const ThemeScopeGuard&) = delete;
    ThemeScopeGuard(ThemeScopeGuard&&) = delete;
    ThemeScopeGuard& operator=(ThemeScopeGuard&&) = delete;
};

} // namespace ktui
