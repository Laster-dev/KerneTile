#pragma once

#include "kernetile/input/pointer_events.hpp"
#include <vector>
#include <unordered_map>
#include <deque>

namespace kernetile::input {

// ============================================================================
// 高频输入采样管道与手写笔低延迟预测器
// 处理 1000 Hz 原生采样合并 (Coalescing) 与 Kalman/一阶速度外推预测
// ============================================================================
class PointerPipeline {
public:
    struct PointerTrack {
        PointerEvent latest_event{};
        std::deque<PointerEvent> history{}; // 近期高频采样点 (保留最多 32 个)
        std::vector<PointerEvent> coalesced_events{}; // 当前渲染帧内合并的所有原始采样
        double velocity_x{0.0}; // 像素 / 毫秒
        double velocity_y{0.0}; // 像素 / 毫秒
        bool active{false};
    };

    PointerPipeline() = default;

    // 接收底层驱动传来的原始高频采样 (例如 1000 Hz pointerrawupdate)
    void push_raw_event(const PointerEvent& ev);

    // 在主渲染帧开始时刷新并获取当前帧的合并指针事件
    std::vector<PointerEvent> flush_coalesced_events(uint32_t pointer_id);

    // 预测下一帧的手写笔位置 (用于消除视觉跟手延迟)
    // lookahead_ms: 向前预测时间 (例如 8.33 ms 对应 120Hz 下一帧)
    PointerEvent predict_next_event(uint32_t pointer_id, double lookahead_ms);

    // 查询指定指针的最新跟踪状态
    const PointerTrack* get_track(uint32_t pointer_id) const;

    // 清空所有跟踪
    void clear();

private:
    std::unordered_map<uint32_t, PointerTrack> m_tracks;
};

} // namespace kernetile::input
