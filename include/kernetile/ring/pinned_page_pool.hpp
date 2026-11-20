#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>
#include <memory>
#include <cassert>
#include <new>

namespace kernetile::ring {

struct PinnedPage {
    uint32_t page_id{0};
    uint8_t* data{nullptr};
    size_t capacity{0};
};

template<uint32_t PageCount = 64, size_t PageSize = 64 * 1024>
class PinnedPagePool {
    static_assert((PageCount & (PageCount - 1)) == 0, "PageCount must be a power of 2");

public:
    static constexpr uint32_t PAGE_COUNT = PageCount;
    static constexpr size_t PAGE_SIZE = PageSize;

    PinnedPagePool() {
        // 分配 64 字节对齐的连续固定内存池
        m_raw_storage = std::make_unique<uint8_t[]>(PageCount * PageSize + 64);
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(m_raw_storage.get());
        uintptr_t aligned_addr = (raw_addr + 63) & ~uintptr_t(63);
        m_aligned_base = reinterpret_cast<uint8_t*>(aligned_addr);

        // 初始化空闲页索引队列
        for (uint32_t i = 0; i < PageCount; ++i) {
            m_free_page_ring[i] = i;
        }
        m_head.store(PageCount, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
    }

    ~PinnedPagePool() = default;

    PinnedPagePool(const PinnedPagePool&) = delete;
    PinnedPagePool& operator=(const PinnedPagePool&) = delete;
    PinnedPagePool(PinnedPagePool&&) = delete;
    PinnedPagePool& operator=(PinnedPagePool&&) = delete;

    // UI 主线程 (单生产者) 申请空闲内存页
    // 返回空表示当前固定页池已耗尽
    [[nodiscard]] bool acquire_page(PinnedPage& out_page) noexcept {
        uint32_t current_tail = m_tail.load(std::memory_order_relaxed);
        uint32_t current_head = m_head.load(std::memory_order_acquire);

        if (current_tail == current_head) {
            // 内存池无空闲页
            return false;
        }

        uint32_t page_id = m_free_page_ring[current_tail & (PageCount - 1)];
        m_tail.store(current_tail + 1, std::memory_order_relaxed);

        out_page.page_id = page_id;
        out_page.data = m_aligned_base + (static_cast<size_t>(page_id) * PageSize);
        out_page.capacity = PageSize;
        return true;
    }

    // 渲染提交线程 (单消费者) 回收已消费完毕的内存页
    void release_page(uint32_t page_id) noexcept {
        assert(page_id < PageCount && "Invalid page_id released");

        uint32_t current_head = m_head.load(std::memory_order_relaxed);
        m_free_page_ring[current_head & (PageCount - 1)] = page_id;
        m_head.store(current_head + 1, std::memory_order_release);
    }

    // 获取当前空闲页数量
    [[nodiscard]] uint32_t get_available_page_count() const noexcept {
        uint32_t head = m_head.load(std::memory_order_relaxed);
        uint32_t tail = m_tail.load(std::memory_order_relaxed);
        return (head >= tail) ? (head - tail) : 0;
    }

    // 获取特定页的内存物理指针
    [[nodiscard]] uint8_t* get_page_address(uint32_t page_id) const noexcept {
        assert(page_id < PageCount);
        return m_aligned_base + (static_cast<size_t>(page_id) * PageSize);
    }

private:
    std::unique_ptr<uint8_t[]> m_raw_storage;
    uint8_t* m_aligned_base{nullptr};

    // SPSC 无锁空闲队列
    alignas(64) std::atomic<uint32_t> m_head{0};
    alignas(64) std::atomic<uint32_t> m_tail{0};
    uint32_t m_free_page_ring[PageCount]{};
};

} // namespace kernetile::ring

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
