#include "kernetile/ring/submission_channel.hpp"
#include "test_common.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cassert>
#include <vector>
#include <atomic>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_notify_suppression..." << std::endl;

    using namespace kernetile::ring;
    using namespace kernetile::core;

    constexpr uint32_t TOTAL_BURSTS = 10000;
    SubmissionChannel<1024, 64, 64 * 1024> channel;

    // 单个微型输入事件触发的图元（例如鼠标悬停光标微移：1 个图元）
    UiPrimitive p_micro{};
    p_micro.min_x = 10.0f; p_micro.min_y = 20.0f;
    p_micro.max_x = 20.0f; p_micro.max_y = 30.0f;
    p_micro.color_a = 1.0f;

    std::vector<UiPrimitive> burst_data{ p_micro };

    std::atomic<bool> producer_active{true};
    std::atomic<uint32_t> bursts_received{0};

    // 启动繁忙渲染消费者线程
    std::thread consumer_thread([&]() {
        while (bursts_received.load(std::memory_order_relaxed) < TOTAL_BURSTS) {
            channel.wait_for_work(10);

            channel.consume_pending_batches([&](std::span<const UiPrimitive> batch) {
                bursts_received.fetch_add(static_cast<uint32_t>(batch.size()), std::memory_order_relaxed);
                // 模拟后台渲染管线微秒级状态切换与命令录制开销 (通过高精时钟自旋避免 Windows sleep 毫秒量化)
                auto t0 = std::chrono::high_resolution_clock::now();
                while (std::chrono::duration<double, std::micro>(std::chrono::high_resolution_clock::now() - t0).count() < 1.0) {
#if defined(_MSC_VER)
                    _mm_pause();
#endif
                }
            });
        }
    });

    // 生产者连续快速下发 10,000 次微观输入事件
    for (uint32_t b = 0; b < TOTAL_BURSTS; ++b) {
        while (!channel.submit_primitives(burst_data)) {
            std::this_thread::yield();
        }
        if ((b & 31) == 0) {
            channel.recycle_completed();
        }
    }

    producer_active.store(false, std::memory_order_release);
    consumer_thread.join();

    uint64_t notify_count = channel.get_event().get_notify_count();
    double suppression_ratio = 1.0 - (static_cast<double>(notify_count) / static_cast<double>(TOTAL_BURSTS));

    std::cout << "  -> Total Bursts Submitted: " << TOTAL_BURSTS << std::endl;
    std::cout << "  -> Bursts Received by Consumer: " << bursts_received.load() << std::endl;
    std::cout << "  -> Kernel Event Notify Calls: " << notify_count << " / " << TOTAL_BURSTS << std::endl;
    std::cout << "  -> Notification Suppression Ratio: " 
              << std::fixed << std::setprecision(2) << (suppression_ratio * 100.0) << "% (Target: >= 90.00%)" << std::endl;

    assert(bursts_received.load() == TOTAL_BURSTS && "All bursts must be received!");
    assert(suppression_ratio >= 0.90 && "Notification suppression ratio must be >= 90%!");

    channel.recycle_completed();
    assert(channel.get_ring().get_free_desc_count() == 1024);

    std::cout << "[TEST] test_notify_suppression PASSED successfully!" << std::endl;
    return 0;
}
