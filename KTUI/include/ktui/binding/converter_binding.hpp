#pragma once

#include "property.hpp"
#include <functional>
#include <cstdint>

namespace ktui {

// 类型转换与表达式绑定 (Converter Binding: T -> R)
template <typename T, typename R>
class ConverterBinding {
public:
    ConverterBinding(const Property<T>& property, std::function<R(const T&)> converter)
        : m_property(&const_cast<Property<T>&>(property)),
          m_converter(std::move(converter)) {
        m_sub_id = m_property->subscribe([this](const T& val, uint64_t /*token*/) {
            if (m_view_updater && m_converter) {
                m_view_updater(m_converter(val));
            }
        });
    }

    ~ConverterBinding() {
        if (m_property && m_sub_id != 0) {
            m_property->unsubscribe(m_sub_id);
        }
    }

    ConverterBinding(const ConverterBinding&) = delete;
    ConverterBinding& operator=(const ConverterBinding&) = delete;

    ConverterBinding(ConverterBinding&& other) noexcept
        : m_property(other.m_property),
          m_converter(std::move(other.m_converter)),
          m_sub_id(other.m_sub_id),
          m_view_updater(std::move(other.m_view_updater)) {
        other.m_sub_id = 0;
    }

    ConverterBinding& operator=(ConverterBinding&& other) noexcept {
        if (this != &other) {
            if (m_property && m_sub_id != 0) {
                m_property->unsubscribe(m_sub_id);
            }
            m_property = other.m_property;
            m_converter = std::move(other.m_converter);
            m_sub_id = other.m_sub_id;
            m_view_updater = std::move(other.m_view_updater);
            other.m_sub_id = 0;
        }
        return *this;
    }

    R get() const {
        return m_converter ? m_converter(m_property->get()) : R{};
    }

    void on_view_update(std::function<void(const R&)> updater) {
        m_view_updater = std::move(updater);
    }

private:
    Property<T>* m_property = nullptr;
    std::function<R(const T&)> m_converter;
    uint64_t m_sub_id = 0;
    std::function<void(const R&)> m_view_updater;
};

} // namespace ktui
