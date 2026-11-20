#include "test_common.hpp"
#include "kernetile/input/pointer_events.hpp"
#include "kernetile/input/pointer_pipeline.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace kernetile::input;

void test_pointer_spherical_conversion() {
    std::cout << "[Test 1] Testing W3C Pointer L3 Tilt to Spherical Conversion..." << std::endl;

    // 1. 垂直笔身: tilt_x = 0, tilt_y = 0
    {
        float alt_rad = 0.0f;
        float az_rad = 0.0f;
        PointerEvent::tilt_to_spherical(0.0f, 0.0f, alt_rad, az_rad);

        // 垂直时仰角为 pi/2 (约 1.5707963 rad)
        assert(std::abs(alt_rad - 1.5707963f) < 0.001f);

        float out_tilt_x = 0.0f;
        float out_tilt_y = 0.0f;
        PointerEvent::spherical_to_tilt(alt_rad, az_rad, out_tilt_x, out_tilt_y);
        assert(std::abs(out_tilt_x) < 0.01f);
        assert(std::abs(out_tilt_y) < 0.01f);
    }

    // 2. 倾斜 45 度测试: tilt_x = 30.0, tilt_y = 45.0
    {
        float in_tx = 30.0f;
        float in_ty = 45.0f;
        float alt_rad = 0.0f;
        float az_rad = 0.0f;
        PointerEvent::tilt_to_spherical(in_tx, in_ty, alt_rad, az_rad);

        // 仰角应该小于 pi/2
        assert(alt_rad < 1.5707963f && alt_rad > 0.0f);

        float out_tx = 0.0f;
        float out_ty = 0.0f;
        PointerEvent::spherical_to_tilt(alt_rad, az_rad, out_tx, out_ty);

        std::cout << "  Roundtrip tilt_x: " << out_tx << " deg (Target: " << in_tx << ")" << std::endl;
        std::cout << "  Roundtrip tilt_y: " << out_ty << " deg (Target: " << in_ty << ")" << std::endl;

        assert(std::abs(out_tx - in_tx) < 0.05f);
        assert(std::abs(out_ty - in_ty) < 0.05f);
    }

    std::cout << "  -> Tilt to spherical roundtrip PASSED." << std::endl;
}

void test_coalesced_events() {
    std::cout << "[Test 2] Testing 1000 Hz High-Frequency Coalesced Events..." << std::endl;

    PointerPipeline pipeline;
    const uint32_t pointer_id = 1;

    // 模拟在一帧 (16 ms) 内输入 16 个高频采样点 (1000 Hz)
    for (int i = 0; i < 16; ++i) {
        PointerEvent ev{};
        ev.pointer_id = pointer_id;
        ev.pointer_type = PointerType::Pen;
        ev.type = PointerEventType::Move;
        ev.client_x = 100.0 + i * 2.0;
        ev.client_y = 200.0 + i * 1.5;
        ev.pressure = 0.5f + static_cast<float>(i) * 0.02f;
        ev.timestamp_us = 1000000 + i * 1000; // 每 1000 微秒一个点
        pipeline.push_raw_event(ev);
    }

    // 帧渲染时拉取合并采样
    auto coalesced = pipeline.flush_coalesced_events(pointer_id);
    assert(coalesced.size() == 16);
    assert(coalesced[0].client_x == 100.0);
    assert(coalesced[15].client_x == 130.0);

    // 验证第二次拉取已清空
    auto empty_check = pipeline.flush_coalesced_events(pointer_id);
    assert(empty_check.empty());

    std::cout << "  -> Coalesced events (16/16 samples collected) PASSED." << std::endl;
}

void test_motion_prediction() {
    std::cout << "[Test 3] Testing Pen Motion Prediction (Latency Elimination)..." << std::endl;

    PointerPipeline pipeline;
    const uint32_t pointer_id = 2;

    // 模拟笔尖以 2.0 像素/毫秒匀速向右运动
    for (int i = 0; i < 10; ++i) {
        PointerEvent ev{};
        ev.pointer_id = pointer_id;
        ev.pointer_type = PointerType::Pen;
        ev.type = PointerEventType::Move;
        ev.client_x = 100.0 + i * 20.0; // 每 10ms 走 20px -> 2.0 px/ms
        ev.client_y = 300.0;
        ev.timestamp_us = 1000000 + i * 10000;
        pipeline.push_raw_event(ev);
    }

    const auto* track = pipeline.get_track(pointer_id);
    assert(track != nullptr);
    std::cout << "  Estimated tracking velocity_x: " << track->velocity_x << " px/ms (Expected ~2.0)" << std::endl;
    assert(std::abs(track->velocity_x - 2.0) < 0.1);

    // 向前预测 8.33 毫秒 (120 Hz 一帧时间)
    double lookahead = 8.33;
    PointerEvent pred = pipeline.predict_next_event(pointer_id, lookahead);

    double expected_x = 100.0 + 9 * 20.0 + 2.0 * lookahead;
    std::cout << "  Predicted X: " << pred.client_x << " (Expected ~" << expected_x << ")" << std::endl;
    assert(std::abs(pred.client_x - expected_x) < 1.0);

    std::cout << "  -> Pen motion prediction PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 8 Pointer Pipeline Unit Tests ===" << std::endl;

    test_pointer_spherical_conversion();
    test_coalesced_events();
    test_motion_prediction();

    std::cout << "=== All Pointer Pipeline Tests PASSED (100%) ===" << std::endl;
    return 0;
}
