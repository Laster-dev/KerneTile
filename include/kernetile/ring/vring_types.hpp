#pragma once

#include <cstdint>
#include <atomic>
#include <span>

namespace kernetile::ring {

// ============================================================================
// VirtIO 标准描述符标志位定义
// ============================================================================
constexpr uint16_t VRING_DESC_F_NEXT     = 1; // 链式描述符 (当前描述符通过 next 字段链接下一项)
constexpr uint16_t VRING_DESC_F_WRITE    = 2; // 设备可写 (对于渲染引擎表示只读输入或回写缓冲区)
constexpr uint16_t VRING_DESC_F_INDIRECT = 4; // 间接描述符表 (addr 指向另一张描述符数组)

// ============================================================================
// 通知抑制标志位定义 (Notification Suppression)
// ============================================================================
// 消费者置于 used.flags：告知生产者消费者正处于繁忙处理状态，禁止调用系统级唤醒 (futex / SetEvent)
constexpr uint16_t VRING_USED_F_NO_NOTIFY  = 1;

// 生产者置于 avail.flags：告知消费者无需在处理完单批次后回显中断
constexpr uint16_t VRING_AVAIL_F_NO_NOTIFY = 1;

// ============================================================================
// 16 字节 VirtIO 定长描述符 (严格 16 字节对齐，跨 CPU/GPU 内存架构标准布局)
// ============================================================================
struct alignas(16) VringDesc {
    uint64_t addr{0};   // 内存页物理/虚拟内存起始地址
    uint32_t len{0};    // 缓冲数据长度 (字节)
    uint16_t flags{0};  // 控制标志位 (VRING_DESC_F_*)
    uint16_t next{0};   // 下一个描述符链表索引
};

static_assert(sizeof(VringDesc) == 16, "VringDesc must be exactly 16 bytes");
static_assert(alignof(VringDesc) == 16, "VringDesc must be 16-byte aligned");

// ============================================================================
// 可用环结构 (UI 主线程写入发布，单生产者维护)
// ============================================================================
template<uint32_t QueueSize>
struct VringAvail {
    static_assert((QueueSize & (QueueSize - 1)) == 0, "QueueSize must be power of 2");

    std::atomic<uint16_t> flags{0};
    std::atomic<uint16_t> idx{0};
    uint16_t ring[QueueSize]{};
};

// ============================================================================
// 已用环项结构 (已完成提交的描述符 ID 与实际消费数据字节长度)
// ============================================================================
struct VringUsedElem {
    uint32_t id{0};
    uint32_t len{0};
};

// ============================================================================
// 已用环结构 (渲染线程回写归还，单消费者维护)
// ============================================================================
template<uint32_t QueueSize>
struct VringUsed {
    static_assert((QueueSize & (QueueSize - 1)) == 0, "QueueSize must be power of 2");

    std::atomic<uint16_t> flags{0};
    std::atomic<uint16_t> idx{0};
    VringUsedElem ring[QueueSize]{};
};

} // namespace kernetile::ring
