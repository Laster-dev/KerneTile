#pragma once

#include "kernetile/ring/vring_types.hpp"
#include "kernetile/ring/pinned_page_pool.hpp"
#include "kernetile/ring/lockfree_ring.hpp"
#include "kernetile/ring/thread_event.hpp"
#include "kernetile/core/primitives.hpp"
#include <vector>
#include <span>
#include <cstring>
#include <chrono>

namespace kernetile::ring {

template<uint32_t QueueSize = 1024, uint32_t PageCount = 64, size_t PageSize = 64 * 1024>
class SubmissionChannel {
public:
    static constexpr uint32_t MAX_PRIMITIVES_PER_PAGE = PageSize / sizeof(core::UiPrimitive);

    SubmissionChannel() = default;
    ~SubmissionChannel() = default;

    SubmissionChannel(const SubmissionChannel&) = delete;
    SubmissionChannel& operator=(const SubmissionChannel&) = delete;
    SubmissionChannel(SubmissionChannel&&) = delete;
    SubmissionChannel& operator=(SubmissionChannel&&) = delete;

    // ========================================================================
    // 生产者接口 (UI 线程调用)
    // ========================================================================

    // 提交一批图元 (完全零动态内存分配，自动从固定页池获取并挂入无锁环)
    bool submit_primitives(std::span<const core::UiPrimitive> primitives) {
        if (primitives.empty()) return true;

        size_t needed_descs = (primitives.size() + MAX_PRIMITIVES_PER_PAGE - 1) / MAX_PRIMITIVES_PER_PAGE;
        if (m_ring.get_free_desc_count() < needed_descs) {
            m_ring.recycle_used();
            if (m_ring.get_free_desc_count() < needed_descs) {
                return false;
            }
        }
        if (m_pool.get_available_page_count() < needed_descs) {
            m_ring.recycle_used();
            if (m_pool.get_available_page_count() < needed_descs) {
                return false;
            }
        }

        size_t offset = 0;
        uint16_t submitted_descs[QueueSize];
        size_t desc_count = 0;

        while (offset < primitives.size()) {
            size_t batch_size = std::min(primitives.size() - offset, static_cast<size_t>(MAX_PRIMITIVES_PER_PAGE));
            size_t byte_len = batch_size * sizeof(core::UiPrimitive);

            // 1. 获取固定内存页
            PinnedPage page;
            bool ok_page = m_pool.acquire_page(page);
            assert(ok_page && "Page acquire must succeed after availability check");
            (void)ok_page;

            // 2. 拷贝图元至固定内存页
            std::memcpy(page.data, primitives.data() + offset, byte_len);

            // 3. 申请描述符
            uint16_t desc_id = 0;
            bool ok_desc = m_ring.allocate_desc(desc_id);
            assert(ok_desc && "Desc allocate must succeed after availability check");
            (void)ok_desc;

            // 4. 配置 VirtIO 描述符
            auto& desc = m_ring.get_desc(desc_id);
            desc.addr = reinterpret_cast<uint64_t>(page.data);
            desc.len = static_cast<uint32_t>(byte_len);
            desc.flags = 0;
            desc.next = 0xFFFF;

            submitted_descs[desc_count++] = desc_id;
            offset += batch_size;
        }

        // 5. 批量提交至可用环并执行通知抑制检查
        m_ring.submit_batch(std::span<const uint16_t>(submitted_descs, desc_count), &m_event);
        return true;
    }

    // 回收由渲染消费者归还的描述符与相关内存
    uint32_t recycle_completed() {
        return m_ring.recycle_used();
    }

    // ========================================================================
    // 消费者接口 (渲染提交线程调用)
    // ========================================================================

    // 阻塞等待新任务 (带通知抑制状态切换)
    bool wait_for_work(uint32_t timeout_ms = 0xFFFFFFFF) {
        if (m_ring.has_pending_work()) {
            return true;
        }

        // 退出繁忙轮询，允许生产者通过系统调用唤醒
        m_ring.set_no_notify(false);

        // 双重校验屏障：防止在清除 NO_NOTIFY 的瞬态发生丢失唤醒竞争
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (m_ring.has_pending_work()) {
            m_ring.set_no_notify(true);
            return true;
        }

        bool signaled = m_event.wait(timeout_ms);
        // 唤醒后立即重新抑制通知，进入连续忙消费处理
        m_ring.set_no_notify(true);
        return signaled || m_ring.has_pending_work();
    }

    // 消费回调函数模板: void(std::span<const UiPrimitive>)
    template<typename ProcessFn>
    size_t consume_pending_batches(ProcessFn&& process_fn) {
        // 进入忙消费处理，抑制生产者系统调用唤醒
        m_ring.set_no_notify(true);

        size_t total_consumed = 0;
        uint16_t desc_batch[QueueSize];

        while (true) {
            size_t polled_count = m_ring.poll_batch(desc_batch);
            if (polled_count == 0) break;
            total_consumed += polled_count;

            for (size_t i = 0; i < polled_count; ++i) {
                uint16_t desc_id = desc_batch[i];
                const auto& desc = m_ring.get_desc(desc_id);

                // 解析图元切片
                const auto* prim_ptr = reinterpret_cast<const core::UiPrimitive*>(desc.addr);
                size_t prim_count = desc.len / sizeof(core::UiPrimitive);
                process_fn(std::span<const core::UiPrimitive>(prim_ptr, prim_count));

                // 回收内存页
                uint32_t page_id = static_cast<uint32_t>((desc.addr - reinterpret_cast<uint64_t>(m_pool.get_page_address(0))) / PageSize);
                m_pool.release_page(page_id);
            }

            // 批量回写已用环
            m_ring.complete_batch(std::span<const uint16_t>(desc_batch, polled_count));
        }

        return total_consumed;
    }

    ThreadEvent& get_event() noexcept { return m_event; }
    PinnedPagePool<PageCount, PageSize>& get_page_pool() noexcept { return m_pool; }
    LockFreeFrameRing<QueueSize>& get_ring() noexcept { return m_ring; }

private:
    PinnedPagePool<PageCount, PageSize> m_pool;
    LockFreeFrameRing<QueueSize> m_ring;
    ThreadEvent m_event;
};

} // namespace kernetile::ring
