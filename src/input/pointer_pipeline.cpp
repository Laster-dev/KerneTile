#include "kernetile/input/pointer_pipeline.hpp"
#include <algorithm>

namespace kernetile::input {

void PointerPipeline::push_raw_event(const PointerEvent& ev) {
    auto& track = m_tracks[ev.pointer_id];
    track.latest_event = ev;
    track.active = (ev.type != PointerEventType::Up && ev.type != PointerEventType::Cancel);

    // 压入采样记录
    track.coalesced_events.push_back(ev);
    track.history.push_back(ev);
    if (track.history.size() > 32) {
        track.history.pop_front();
    }

    // 计算瞬时速度与平滑速度 (像素/毫秒)
    if (track.history.size() >= 2) {
        const auto& p1 = track.history[track.history.size() - 1];
        const auto& p0 = track.history[track.history.size() - 2];

        double dt_ms = static_cast<double>(p1.timestamp_us - p0.timestamp_us) / 1000.0;
        if (dt_ms > 0.01) {
            double inst_vx = (p1.client_x - p0.client_x) / dt_ms;
            double inst_vy = (p1.client_y - p0.client_y) / dt_ms;

            // 一阶低通平滑滤波 (EMA: 70% 瞬时值 + 30% 历史值)
            track.velocity_x = inst_vx * 0.7 + track.velocity_x * 0.3;
            track.velocity_y = inst_vy * 0.7 + track.velocity_y * 0.3;
        }
    }
}

std::vector<PointerEvent> PointerPipeline::flush_coalesced_events(uint32_t pointer_id) {
    auto it = m_tracks.find(pointer_id);
    if (it == m_tracks.end()) {
        return {};
    }

    std::vector<PointerEvent> result = std::move(it->second.coalesced_events);
    it->second.coalesced_events.clear();
    return result;
}

PointerEvent PointerPipeline::predict_next_event(uint32_t pointer_id, double lookahead_ms) {
    auto it = m_tracks.find(pointer_id);
    if (it == m_tracks.end() || !it->second.active) {
        return {};
    }

    const auto& track = it->second;
    PointerEvent pred = track.latest_event;

    // 速度外推预测下一帧亚像素接触点
    pred.client_x += track.velocity_x * lookahead_ms;
    pred.client_y += track.velocity_y * lookahead_ms;
    pred.timestamp_us += static_cast<uint64_t>(lookahead_ms * 1000.0);

    return pred;
}

const PointerPipeline::PointerTrack* PointerPipeline::get_track(uint32_t pointer_id) const {
    auto it = m_tracks.find(pointer_id);
    if (it != m_tracks.end()) {
        return &it->second;
    }
    return nullptr;
}

void PointerPipeline::clear() {
    m_tracks.clear();
}

} // namespace kernetile::input
