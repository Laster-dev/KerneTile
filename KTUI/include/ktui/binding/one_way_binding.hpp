#pragma once

#include "property.hpp"
#include <functional>
#include <cstdint>

namespace ktui {

// 单向只读数据绑定 (One-Way Binding)
template <typename T>
class OneWayBinding {
public:
    explicit OneWayBinding(const Property<T>& property)
        : m_property(&const_cast<Property<T>&>(property)) {
        m_sub_id = m_property->subscribe([this](const T& val, uint64_t /*token*/) {
            if (m_view_updater) {
                m_view_updater(val);
            }
        });
    }

    ~OneWayBinding() {
        if (m_property && m_sub_id != 0) {
            m_property->unsubscribe(m_sub_id);
        }
    }

    OneWayBinding(const OneWayBinding&) = delete;
    OneWayBinding& operator=(const OneWayBinding&) = delete;

    OneWayBinding(OneWayBinding&& other) noexcept
        : m_property(other.m_property),
          m_sub_id(other.m_sub_id),
          m_view_updater(std::move(other.m_view_updater)) {
        other.m_sub_id = 0;
    }

    OneWayBinding& operator=(OneWayBinding&& other) noexcept {
        if (this != &other) {
            if (m_property && m_sub_id != 0) {
                m_property->unsubscribe(m_sub_id);
            }
            m_property = other.m_property;
            m_sub_id = other.m_sub_id;
            m_view_updater = std::move(other.m_view_updater);
            other.m_sub_id = 0;
        }
        return *this;
    }

    const T& get() const { return m_property->get(); }

    void on_view_update(Fn<void(const T&)> updater) {
        m_view_updater = std::move(updater);
    }

private:
    Property<T>* m_property = nullptr;
    uint64_t m_sub_id = 0;
    Fn<void(const T&)> m_view_updater;
};

} // namespace ktui
