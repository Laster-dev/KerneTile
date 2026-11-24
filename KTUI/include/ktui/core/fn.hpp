#pragma once

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace ktui {

// 非模板共享存储基类：所有 Fn<R(Args...)> 签名共用同一份拷贝/移动/析构实现，杜绝按签名模板膨胀
struct FnStorage {
    static constexpr size_t kBufSize = 24;
    using ManageFn = void (*)(void* dst, const void* src);

    FnStorage() noexcept = default;
    FnStorage(const FnStorage& other) { copy_from(other); }
    FnStorage(FnStorage&& other) noexcept { move_from(std::move(other)); }
    ~FnStorage() { reset(); }

    FnStorage& operator=(const FnStorage& other) {
        if (this != &other) {
            reset();
            copy_from(other);
        }
        return *this;
    }

    FnStorage& operator=(FnStorage&& other) noexcept {
        if (this != &other) {
            reset();
            move_from(std::move(other));
        }
        return *this;
    }

    void reset() noexcept {
        if (m_raw_invoke) {
            if (m_manage) m_manage(nullptr, m_buf);
            m_raw_invoke = nullptr;
            m_manage = nullptr;
        }
    }

    void copy_from(const FnStorage& other) {
        if (!other.m_raw_invoke) return;
        m_raw_invoke = other.m_raw_invoke;
        m_manage = other.m_manage;
        if (m_manage) {
            m_manage(m_buf, other.m_buf);
        } else {
            std::memcpy(m_buf, other.m_buf, kBufSize);
        }
    }

    void move_from(FnStorage&& other) noexcept {
        if (!other.m_raw_invoke) return;
        m_raw_invoke = other.m_raw_invoke;
        m_manage = other.m_manage;
        if (m_manage) {
            m_manage(m_buf, other.m_buf);
            other.reset();
        } else {
            std::memcpy(m_buf, other.m_buf, kBufSize);
            other.m_raw_invoke = nullptr;
            other.m_manage = nullptr;
        }
    }

    alignas(std::max_align_t) unsigned char m_buf[kBufSize]{};
    void (*m_raw_invoke)(){nullptr};
    ManageFn m_manage{nullptr};
};

// 极致零膨胀轻量回调容器 (ktui::Fn)
// - 小型平凡可拷贝 lambda (sizeof <= 24)：仅生成 1 个静态 invoke 函数，m_manage == nullptr
// - 非平凡或大于 24B：仅生成 1 个 invoke + 1 个统一的 manage 函数
template <typename Sig>
class Fn;

template <typename R, typename... Args>
class Fn<R(Args...)> : private FnStorage {
    using InvokeFn = R (*)(const void*, Args...);

public:
    Fn() noexcept = default;
    Fn(std::nullptr_t) noexcept {}
    Fn(const Fn&) = default;
    Fn(Fn&&) noexcept = default;
    Fn& operator=(const Fn&) = default;
    Fn& operator=(Fn&&) noexcept = default;
    ~Fn() = default;

    Fn& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    template <typename F,
              typename DF = std::decay_t<F>,
              std::enable_if_t<!std::is_same_v<DF, Fn> && !std::is_same_v<DF, std::nullptr_t> &&
                               std::is_invocable_r_v<R, DF&, Args...>, int> = 0>
    Fn(F&& f) {
        if constexpr (std::is_pointer_v<DF> || std::is_member_pointer_v<DF>) {
            if (!f) return;
        }
        if constexpr (sizeof(DF) <= kBufSize && alignof(DF) <= alignof(std::max_align_t)) {
            ::new (m_buf) DF(std::forward<F>(f));
            InvokeFn inv = [](const void* buf, Args... args) -> R {
                auto* fn = reinterpret_cast<DF*>(const_cast<void*>(buf));
                if constexpr (std::is_void_v<R>) {
                    (*fn)(std::forward<Args>(args)...);
                } else {
                    return (*fn)(std::forward<Args>(args)...);
                }
            };
            m_raw_invoke = reinterpret_cast<void (*)()>(inv);
            if constexpr (!std::is_trivially_copyable_v<DF> || !std::is_trivially_destructible_v<DF>) {
                m_manage = [](void* dst, const void* src) {
                    if (dst) {
                        ::new (dst) DF(*reinterpret_cast<const DF*>(src));
                    } else {
                        reinterpret_cast<const DF*>(src)->~DF();
                    }
                };
            }
        } else {
            DF* heap_fn = new DF(std::forward<F>(f));
            std::memcpy(m_buf, &heap_fn, sizeof(heap_fn));
            InvokeFn inv = [](const void* buf, Args... args) -> R {
                DF* fn = nullptr;
                std::memcpy(&fn, buf, sizeof(fn));
                if constexpr (std::is_void_v<R>) {
                    (*fn)(std::forward<Args>(args)...);
                } else {
                    return (*fn)(std::forward<Args>(args)...);
                }
            };
            m_raw_invoke = reinterpret_cast<void (*)()>(inv);
            m_manage = [](void* dst, const void* src) {
                DF* src_fn = nullptr;
                std::memcpy(&src_fn, src, sizeof(src_fn));
                if (dst) {
                    DF* copy = new DF(*src_fn);
                    std::memcpy(dst, &copy, sizeof(copy));
                } else {
                    delete src_fn;
                }
            };
        }
    }

    using FnStorage::reset;

    explicit operator bool() const noexcept { return m_raw_invoke != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return m_raw_invoke == nullptr; }
    bool operator!=(std::nullptr_t) const noexcept { return m_raw_invoke != nullptr; }

    R operator()(Args... args) const {
        auto inv = reinterpret_cast<InvokeFn>(m_raw_invoke);
        if constexpr (std::is_void_v<R>) {
            if (inv) inv(m_buf, std::forward<Args>(args)...);
        } else {
            return inv(m_buf, std::forward<Args>(args)...);
        }
    }
};

} // namespace ktui
