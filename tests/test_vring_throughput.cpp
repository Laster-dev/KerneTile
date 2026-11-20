#include "kernetile/ring/submission_channel.hpp"
#include "test_common.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <vector>
#include <atomic>
#include <numeric>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_vring_throughput..." << std::endl;

    using namespace kernetile::ring;
    using namespace kernetile::core;

    constexpr uint32_t TOTAL_FRAMES = 200;
    constexpr uint32_t PRIMITIVES_PER_FRAME = 5000;
    constexpr uint64_t EXPECTED_TOTAL_PRIMITIVES = static_cast<uint64_t>(TOTAL_FRAMES) * PRIMITIVES_PER_FRAME;

    SubmissionChannel<1024, 128, 64 * 1024> channel;

    std::atomic<bool> producer_done{false};
    std::atomic<uint64_t> total_primitives_consumed{0};
    std::atomic<uint64_t> checksum_consumed{0};

    // 预构造测试图元模板
    std::vector<UiPrimitive> frame_primitives(PRIMITIVES_PER_FRAME);
    for (uint32_t i = 0; i < PRIMITIVES_PER_FRAME; ++i) {
        frame_primitives[i].min_x = static_cast<float>(i);
        frame_primitives[i].min_y = static_cast<float>(i * 2);
        frame_primitives[i].max_x = static_cast<float>(i + 10);
        frame_primitives[i].max_y = static_cast<float>(i * 2 + 10);
        frame_primitives[i].color_r = 0.5f;
        frame_primitives[i].color_g = 0.75f;
        frame_primitives[i].color_b = 1.0f;
        frame_primitives[i].color_a = 1.0f;
        frame_primitives[i].flags = static_cast<uint16_t>(i & 0xFFFF);
    }

    uint64_t expected_single_frame_checksum = 0;
    for (const auto& p : frame_primitives) {
        expected_single_frame_checksum += static_cast<uint64_t>(p.min_x) + p.flags;
    }
    uint64_t expected_total_checksum = expected_single_frame_checksum * TOTAL_FRAMES;

    // 1. 启动渲染消费线程 (Consumer)
    std::thread consumer_thread([&]() {
        uint64_t local_consumed = 0;
        uint64_t local_checksum = 0;

        while (local_consumed < EXPECTED_TOTAL_PRIMITIVES) {
            // 等待新任务
            channel.wait_for_work(100);

            // 批量消费
            channel.consume_pending_batches([&](std::span<const UiPrimitive> batch) {
                local_consumed += batch.size();
                for (const auto& p : batch) {
                    local_checksum += static_cast<uint64_t>(p.min_x) + p.flags;
                }
            });
        }

        total_primitives_consumed.store(local_consumed, std::memory_order_release);
        checksum_consumed.store(local_checksum, std::memory_order_release);
    });

    // 2. UI 主线程并发写入 (Producer)
    std::vector<double> submit_latencies_us;
    submit_latencies_us.reserve(TOTAL_FRAMES);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (uint32_t f = 0; f < TOTAL_FRAMES; ++f) {
        auto t0 = std::chrono::high_resolution_clock::now();

        // 提交本帧 5,000 个图元
        while (!channel.submit_primitives(frame_primitives)) {
            // 若遇到瞬态背压，短暂让出时间片等待消费者处理
            std::this_thread::yield();
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        submit_latencies_us.push_back(us);

        // 模拟 120 Hz 帧间微观延迟 (仅轻微睡眠或让出以模拟真实 UI 负载)
        channel.recycle_completed();
    }

    producer_done.store(true, std::memory_order_release);
    auto end_time = std::chrono::high_resolution_clock::now();

    // 等待消费者完成全量校验
    consumer_thread.join();

    double total_wall_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    double avg_submit_us = std::accumulate(submit_latencies_us.begin(), submit_latencies_us.end(), 0.0) / submit_latencies_us.size();

    std::cout << "  -> Total Frames: " << TOTAL_FRAMES << std::endl;
    std::cout << "  -> Total Primitives Processed: " << total_primitives_consumed.load() << " / " << EXPECTED_TOTAL_PRIMITIVES << std::endl;
    std::cout << "  -> Total Wall Time: " << total_wall_time_ms << " ms" << std::endl;
    std::cout << "  -> Average Submit Latency per Frame: " << avg_submit_us << " us (Target: < 50 us)" << std::endl;
    std::cout << "  -> Throughput: " << (static_cast<double>(total_primitives_consumed.load()) / (total_wall_time_ms / 1000.0) / 1'000'000.0) << " M primitives/sec" << std::endl;

    // 3. 校验数据完整性与零撕裂
    assert(total_primitives_consumed.load() == EXPECTED_TOTAL_PRIMITIVES && "Consumed count mismatch!");
    assert(checksum_consumed.load() == expected_total_checksum && "Data corruption / tear detected!");
    
    // 最终回收并验证描述符池与页池完全无泄漏
    channel.recycle_completed();
    std::cout << "  -> Free Descriptors: " << channel.get_ring().get_free_desc_count() << " / 1024" << std::endl;
    assert(channel.get_ring().get_free_desc_count() == 1024 && "Descriptor leak detected!");

    std::cout << "[TEST] test_vring_throughput PASSED successfully!" << std::endl;
    return 0;
}
