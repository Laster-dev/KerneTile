#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

#include "kernetile/ring/vring_types.hpp"
#include "kernetile/ring/thread_event.hpp"
#include <cstdint>
#include <atomic>
#include <span>
#include <cassert>
#include <algorithm>

namespace kernetile::ring {

template<uint32_t QueueSize = 1024>
class LockFreeFrameRing {
    static_assert((QueueSize & (QueueSize - 1)) == 0, "QueueSize must be a power of 2");

public:
    static constexpr uint32_t QUEUE_SIZE = QueueSize;
    static constexpr uint32_t QUEUE_MASK = QueueSize - 1;

    LockFreeFrameRing() {
        // 初始化空闲描述符单向链表
        for (uint32_t i = 0; i < QueueSize - 1; ++i) {
            m_descs[i].next = static_cast<uint16_t>(i + 1);
        }
        m_descs[QueueSize - 1].next = 0xFFFF; // 链表终止标记
        m_producer_free_head = 0;
        m_producer_free_count = QueueSize;
    }

    ~LockFreeFrameRing() = default;

    LockFreeFrameRing(const LockFreeFrameRing&) = delete;
    LockFreeFrameRing& operator=(const LockFreeFrameRing&) = delete;
    LockFreeFrameRing(LockFreeFrameRing&&) = delete;
    LockFreeFrameRing& operator=(LockFreeFrameRing&&) = delete;

    // ========================================================================
    // 生产者 API (仅由 UI 线程调用)
    // ========================================================================

    // 申请一个空闲描述符 (若当前空闲用尽，自动尝试回收已消费项)
    [[nodiscard]] bool allocate_desc(uint16_t& out_desc_id) noexcept {
        if (m_producer_free_count == 0) {
            recycle_used();
            if (m_producer_free_count == 0) {
                return false; // 队列完全耗尽 (背压)
            }
        }

        out_desc_id = m_producer_free_head;
        m_producer_free_head = m_descs[out_desc_id].next;
        m_producer_free_count--;
        m_descs[out_desc_id].flags = 0;
        m_descs[out_desc_id].next = 0xFFFF;
        return true;
    }

    // 获取描述符引用以便填充数据页地址与长度
    [[nodiscard]] VringDesc& get_desc(uint16_t desc_id) noexcept {
        assert(desc_id < QueueSize);
        return m_descs[desc_id];
    }

    // 生产者批量提交已就绪的描述符索引
    // 写入可用环 -> 执行写内存屏障 -> 原子发布索引 -> 通知抑制检查与唤醒
    void submit_batch(std::span<const uint16_t> desc_indices, ThreadEvent* event = nullptr) noexcept {
        if (desc_indices.empty()) return;

        uint16_t current_idx = m_avail.idx.load(std::memory_order_relaxed);
        for (uint16_t id : desc_indices) {
            m_avail.ring[current_idx & QUEUE_MASK] = id;
            current_idx++;
        }

        // 关键写内存屏障：确保内存页内所有图元与 VringDesc 在索引发布前先刷入共享缓存
        std::atomic_thread_fence(std::memory_order_release);
        m_avail.idx.store(current_idx, std::memory_order_release);

        // 通知抑制机制 (Notification Suppression)
        // 仅当消费者未处于繁忙忙碌状态 (未置 VRING_USED_F_NO_NOTIFY) 且提供了事件对象时，触发跨线程内核唤醒
        if (event != nullptr) {
            bool need_notify = (m_used.flags.load(std::memory_order_relaxed) & VRING_USED_F_NO_NOTIFY) == 0;
            if (need_notify) {
                event->notify();
            }
        }
    }

    // 回收渲染消费者在已用环 (Used Ring) 中归还的描述符
    uint32_t recycle_used() noexcept {
        uint16_t target_idx = m_used.idx.load(std::memory_order_acquire);
        uint16_t count = target_idx - m_producer_last_used_idx;
        if (count == 0) return 0;

        for (uint16_t i = 0; i < count; ++i) {
            uint16_t desc_id = static_cast<uint16_t>(m_used.ring[(m_producer_last_used_idx + i) & QUEUE_MASK].id);
            assert(desc_id < QueueSize);
            m_descs[desc_id].next = m_producer_free_head;
            m_producer_free_head = desc_id;
            m_producer_free_count++;
        }

        m_producer_last_used_idx = target_idx;
        return count;
    }

    [[nodiscard]] uint32_t get_free_desc_count() const noexcept {
        return m_producer_free_count;
    }

    // ========================================================================
    // 消费者 API (仅由 渲染提交线程调用)
    // ========================================================================

    // 设置/清除通知抑制标志
    void set_no_notify(bool no_notify) noexcept {
        if (no_notify) {
            m_used.flags.fetch_or(VRING_USED_F_NO_NOTIFY, std::memory_order_release);
        } else {
            m_used.flags.fetch_and(static_cast<uint16_t>(~VRING_USED_F_NO_NOTIFY), std::memory_order_release);
        }
    }

    // 检查是否有待处理的未读描述符
    [[nodiscard]] bool has_pending_work() const noexcept {
        return m_avail.idx.load(std::memory_order_acquire) != m_consumer_last_avail_idx;
    }

    // 批量拉取可用环中的待渲染描述符索引
    size_t poll_batch(std::span<uint16_t> out_desc_indices) noexcept {
        uint16_t target_idx = m_avail.idx.load(std::memory_order_acquire);
        uint16_t available_count = target_idx - m_consumer_last_avail_idx;
        if (available_count == 0) return 0;

        size_t count_to_read = std::min(static_cast<size_t>(available_count), out_desc_indices.size());
        for (size_t i = 0; i < count_to_read; ++i) {
            out_desc_indices[i] = m_avail.ring[(m_consumer_last_avail_idx + static_cast<uint16_t>(i)) & QUEUE_MASK];
        }

        m_consumer_last_avail_idx += static_cast<uint16_t>(count_to_read);
        return count_to_read;
    }

    // 消费者读取描述符内容 (只读)
    [[nodiscard]] const VringDesc& get_desc(uint16_t desc_id) const noexcept {
        assert(desc_id < QueueSize);
        return m_descs[desc_id];
    }

    // 消费者完成一批描述符的处理并写入已用环归还
    void complete_batch(std::span<const uint16_t> desc_ids) noexcept {
        if (desc_ids.empty()) return;

        uint16_t current_idx = m_used.idx.load(std::memory_order_relaxed);
        for (uint16_t id : desc_ids) {
            m_used.ring[current_idx & QUEUE_MASK] = VringUsedElem{
                .id = id,
                .len = m_descs[id].len
            };
            current_idx++;
        }

        std::atomic_thread_fence(std::memory_order_release);
        m_used.idx.store(current_idx, std::memory_order_release);
    }

private:
    // 描述符实体表 (16 字节对齐)
    alignas(64) VringDesc m_descs[QueueSize];

    // 可用环 (UI 线程写入发布，对齐独立缓存行)
    alignas(64) VringAvail<QueueSize> m_avail;

    // 已用环 (渲染线程回写归还，对齐独立缓存行)
    alignas(64) VringUsed<QueueSize> m_used;

    // 生产者私有线程状态 (完全隔离，无跨核假共享)
    alignas(64) uint16_t m_producer_free_head{0};
    uint32_t m_producer_free_count{QueueSize};
    uint16_t m_producer_last_used_idx{0};

    // 消费者私有线程状态 (完全隔离，无跨核假共享)
    alignas(64) uint16_t m_consumer_last_avail_idx{0};
};

} // namespace kernetile::ring

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
