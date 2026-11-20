#pragma once

#include <vector>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <algorithm>

namespace ktui {

enum class ListChangeType {
    Added,
    Removed,
    Updated,
    Reset
};

struct ListChangeEvent {
    ListChangeType type = ListChangeType::Reset;
    size_t index = 0;
    size_t count = 0;
};

// 响应式可观察动态列表 (Observable List)
template <typename T>
class ObservableList {
public:
    using Listener = std::function<void(const ListChangeEvent&)>;

    ObservableList() = default;

    void push_back(T item) {
        size_t idx = m_items.size();
        m_items.push_back(std::move(item));
        notify({ListChangeType::Added, idx, 1});
    }

    void insert(size_t index, T item) {
        if (index > m_items.size()) index = m_items.size();
        m_items.insert(m_items.begin() + index, std::move(item));
        notify({ListChangeType::Added, index, 1});
    }

    void erase(size_t index) {
        if (index < m_items.size()) {
            m_items.erase(m_items.begin() + index);
            notify({ListChangeType::Removed, index, 1});
        }
    }

    void clear() {
        m_items.clear();
        notify({ListChangeType::Reset, 0, 0});
    }

    size_t size() const { return m_items.size(); }
    bool empty() const { return m_items.empty(); }

    const T& operator[](size_t index) const { return m_items[index]; }
    T& operator[](size_t index) { return m_items[index]; }

    void notify_item_changed(size_t index) {
        if (index < m_items.size()) {
            notify({ListChangeType::Updated, index, 1});
        }
    }

    uint64_t subscribe(Listener listener) {
        uint64_t id = ++s_next_id;
        m_listeners.push_back({id, std::move(listener)});
        return id;
    }

    void unsubscribe(uint64_t id) {
        auto it = std::remove_if(m_listeners.begin(), m_listeners.end(),
                                 [id](const Entry& e) { return e.id == id; });
        m_listeners.erase(it, m_listeners.end());
    }

    auto begin() { return m_items.begin(); }
    auto end() { return m_items.end(); }
    auto begin() const { return m_items.begin(); }
    auto end() const { return m_items.end(); }

private:
    void notify(const ListChangeEvent& event) {
        auto current_listeners = m_listeners;
        for (auto& entry : current_listeners) {
            if (entry.listener) {
                entry.listener(event);
            }
        }
    }

    struct Entry {
        uint64_t id;
        Listener listener;
    };

    std::vector<T> m_items;
    std::vector<Entry> m_listeners;
    inline static std::atomic<uint64_t> s_next_id{0};
};

} // namespace ktui
