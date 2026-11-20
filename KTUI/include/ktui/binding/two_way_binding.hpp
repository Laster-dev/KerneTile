#pragma once

#include "property.hpp"
#include <functional>
#include <cstdint>
#include <atomic>

namespace ktui {

// 双向数据绑定 (Two-Way Binding)
// 内置 Transaction Token 事务熔断机制，彻底根除 UI 与 Model 之间的死循环递归
template <typename T>
class TwoWayBinding {
public:
    explicit TwoWayBinding(Property<T>& property)
        : m_property(&property) {
        m_sub_id = m_property->subscribe([this](const T& val, uint64_t token) {
            // 事务回声熔断: 若当前收到的变更正是自身 UI 发出的，直接短路阻断
            if (token != 0 && token == m_last_ui_token) {
                return;
            }
            if (m_view_updater) {
                m_view_updater(val);
            }
        });
    }

    ~TwoWayBinding() {
        if (m_property && m_sub_id != 0) {
            m_property->unsubscribe(m_sub_id);
        }
    }

    TwoWayBinding(const TwoWayBinding&) = delete;
    TwoWayBinding& operator=(const TwoWayBinding&) = delete;

    TwoWayBinding(TwoWayBinding&& other) noexcept
        : m_property(other.m_property),
          m_sub_id(other.m_sub_id),
          m_last_ui_token(other.m_last_ui_token),
          m_view_updater(std::move(other.m_view_updater)) {
        other.m_sub_id = 0;
    }

    TwoWayBinding& operator=(TwoWayBinding&& other) noexcept {
        if (this != &other) {
            if (m_property && m_sub_id != 0) {
                m_property->unsubscribe(m_sub_id);
            }
            m_property = other.m_property;
            m_sub_id = other.m_sub_id;
            m_last_ui_token = other.m_last_ui_token;
            m_view_updater = std::move(other.m_view_updater);
            other.m_sub_id = 0;
        }
        return *this;
    }

    const T& get() const { return m_property->get(); }

    // 由 UI 控件在用户输入改变时调用 (生成全局递增事务 Token)
    void set_from_ui(const T& val) {
        uint64_t token = ++s_next_token;
        m_last_ui_token = token;
        m_property->set(val, token);
    }

    // 注册视图更新回调 (当外部业务修改 Model 时通知 UI 刷新)
    void on_view_update(std::function<void(const T&)> updater) {
        m_view_updater = std::move(updater);
    }

private:
    Property<T>* m_property = nullptr;
    uint64_t m_sub_id = 0;
    uint64_t m_last_ui_token = 0;
    std::function<void(const T&)> m_view_updater;
    inline static std::atomic<uint64_t> s_next_token{0};
};

} // namespace ktui
