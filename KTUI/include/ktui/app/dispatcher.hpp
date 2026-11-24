#pragma once

#include <ktui/pal/platform_dispatcher.hpp>
#include <vector>
#include <ktui/core/fn.hpp>
#include <mutex>
#include <thread>
#include <cassert>
#include <stdexcept>

namespace ktui {

// UI 主线程任务调度器 (Thread-Safe UI Dispatcher)
// 负责跨线程向 UI 主线程安全投递闭包任务，并提供主线程约束断言与队列消费
class Dispatcher : public IPlatformDispatcher {
public:
    Dispatcher() noexcept
        : m_ui_thread_id(std::this_thread::get_id()) {}

    explicit Dispatcher(std::thread::id ui_thread_id) noexcept
        : m_ui_thread_id(ui_thread_id) {}

    ~Dispatcher() override = default;

    // 绑定当前线程为 UI 主线程
    void bind_current_thread_as_ui_thread() noexcept {
        m_ui_thread_id = std::this_thread::get_id();
    }

    // 从任意后台线程投递异步任务到 UI 主线程队列
    void post_task(Fn<void()> task) override {
        if (!task) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending_tasks.push_back(std::move(task));
    }

    // 由 UI 主循环调用，消费并执行当前积压的全部任务
    // 采用局部 swap 技巧，锁粒度仅为微秒级的指针交换，任务执行期间完全无锁，杜绝循环死锁
    size_t process_tasks() override {
        std::vector<Fn<void()>> tasks_to_run;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_pending_tasks.empty()) {
                return 0;
            }
            tasks_to_run.swap(m_pending_tasks);
        }

        size_t count = tasks_to_run.size();
        for (auto& task : tasks_to_run) {
            if (task) {
                task();
            }
        }
        return count;
    }

    // 检查是否有排队中的任务
    [[nodiscard]] bool has_pending_tasks() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_pending_tasks.empty();
    }

    // 当前线程是否为 UI 主线程
    [[nodiscard]] bool is_ui_thread() const noexcept {
        return std::this_thread::get_id() == m_ui_thread_id;
    }

    // 断言当前必须处于 UI 主线程
    void assert_ui_thread() const {
        assert(is_ui_thread() && "Dispatcher::assert_ui_thread failed: called from background thread!");
    }

    // 如果处于 UI 线程则立即同步调用，否则投递至队列延迟执行
    void run_on_ui_thread(Fn<void()> task) {
        if (is_ui_thread()) {
            if (task) task();
        } else {
            post_task(std::move(task));
        }
    }

    [[nodiscard]] std::thread::id ui_thread_id() const noexcept {
        return m_ui_thread_id;
    }

private:
    std::thread::id m_ui_thread_id;
    mutable std::mutex m_mutex;
    std::vector<Fn<void()>> m_pending_tasks;
};

} // namespace ktui
