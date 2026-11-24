#pragma once

#include <ktui/core/fn.hpp>

namespace ktui {

// UI 主线程任务调度器接口 (Thread-Safe Task Dispatcher)
class IPlatformDispatcher {
public:
    virtual ~IPlatformDispatcher() = default;

    // 从任意线程向 UI 主线程安全投递任务
    virtual void post_task(Fn<void()> task) = 0;

    // 由 UI 主循环调用，消费并执行当前所有积压任务
    virtual size_t process_tasks() = 0;

    // 检查是否有挂起待执行的任务
    virtual bool has_pending_tasks() const = 0;
};

} // namespace ktui
