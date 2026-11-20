#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include <atomic>
#include <algorithm>

namespace ktui {

// 响应式属性基元 (支持值无变更短路与版本追踪)
template <typename T>
class Property {
public:
    using Listener = std::function<void(const T& value, uint64_t transaction_token)>;

    Property() = default;
    explicit Property(T initial_value) : m_value(std::move(initial_value)) {}

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

    void unsubscribe(uint64_t id) {
        auto it = std::remove_if(m_listeners.begin(), m_listeners.end(),
                                 [id](const Entry& e) { return e.id == id; });
        m_listeners.erase(it, m_listeners.end());
    }

    uint64_t version() const { return m_version; }

private:
    void notify(uint64_t token) {
        auto current_listeners = m_listeners;
        for (auto& entry : current_listeners) {
            if (entry.listener) {
                entry.listener(m_value, token);
            }
        }
    }

    struct Entry {
        uint64_t id;
        Listener listener;
    };

    T m_value{};
    uint64_t m_version = 0;
    std::vector<Entry> m_listeners;
    inline static std::atomic<uint64_t> s_next_listener_id{0};
};

} // namespace ktui
