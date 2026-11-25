#pragma once

#include <ktui/core/fn.hpp>
#include <vector>
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <memory>

namespace ktui {

// 响应式属性基元 (支持值无变更短路、版本追踪与安全跨生命周期退订)
template <typename T>
class Property {
public:
    using Listener = Fn<void(const T& value, uint64_t transaction_token)>;

    Property() = default;
    explicit Property(T initial_value) : m_value(std::move(initial_value)) {}
    ~Property() {
        if (m_alive) {
            *m_alive = false;
        }
    }
    Property(const Property& other) : m_value(other.m_value), m_version(other.m_version) {}
    Property& operator=(const Property& other) {
        if (this != &other) {
            set(other.m_value);
        }
        return *this;
    }
    Property(Property&& other) noexcept
        : m_value(std::move(other.m_value)), m_version(other.m_version) {}
    Property& operator=(Property&& other) noexcept {
        if (this != &other) {
            set(other.m_value);
        }
        return *this;
    }

    // 惰性存活令牌：仅在 BindingGroup 订阅时按需分配，供析构顺序倒置时安全跳过退订
    std::weak_ptr<bool> alive_token() const {
        if (!m_alive) {
            m_alive = std::make_shared<bool>(true);
        }
        return m_alive;
    }

    const T& get() const { return m_value; }
    const T& value() const { return m_value; }
    operator const T&() const { return m_value; }

    // 设置新值 (携带可选事务 Token)
    void set(const T& new_value, uint64_t transaction_token = 0) {
        if (m_value == new_value) return; // 零变更短路 (No-op Suppression)
        m_value = new_value;
        ++m_version;
        notify(transaction_token);
    }

    Property& operator=(const T& new_value) {
        set(new_value);
        return *this;
    }

    uint64_t subscribe(Listener listener) {
        uint64_t id = ++s_next_listener_id;
        m_listeners.push_back({id, std::move(listener)});
        return id;
    }

    uint64_t subscribe(Fn<void(const T&)> simple_listener) {
        return subscribe([fn = std::move(simple_listener)](const T& val, uint64_t /*token*/) {
            if (fn) fn(val);
        });
    }

    void unsubscribe(uint64_t id) {
        if (m_notify_depth > 0) {
            for (auto& e : m_listeners) {
                if (e.id == id) {
                    e.id = 0;
                    e.listener = nullptr;
                    m_needs_compact = true;
                }
            }
            return;
        }
        auto it = std::remove_if(m_listeners.begin(), m_listeners.end(),
                                 [id](const Entry& e) { return e.id == id; });
        m_listeners.erase(it, m_listeners.end());
    }

    uint64_t version() const { return m_version; }

private:
    void notify(uint64_t token) {
        if (m_listeners.empty()) return;
        const size_t initial_count = m_listeners.size();
        ++m_notify_depth;
        for (size_t i = 0; i < initial_count && i < m_listeners.size(); ++i) {
            if (m_listeners[i].id != 0 && m_listeners[i].listener) {
                // 复制轻量可调用句柄（Fn 内部为 SBO，零堆分配）以防回调内部 push_back 触发 vector 扩容
                auto fn = m_listeners[i].listener;
                fn(m_value, token);
            }
        }
        --m_notify_depth;
        if (m_notify_depth == 0 && m_needs_compact) {
            auto it = std::remove_if(m_listeners.begin(), m_listeners.end(),
                                     [](const Entry& e) { return e.id == 0; });
            m_listeners.erase(it, m_listeners.end());
            m_needs_compact = false;
        }
    }

    struct Entry {
        uint64_t id;
        Listener listener;
    };

    T m_value{};
    uint64_t m_version = 0;
    std::vector<Entry> m_listeners;
    uint32_t m_notify_depth = 0;
    bool m_needs_compact = false;
    mutable std::shared_ptr<bool> m_alive;
    inline static std::atomic<uint64_t> s_next_listener_id{0};
};

} // namespace ktui
