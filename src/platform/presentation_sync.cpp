#include "kernetile/platform/presentation_sync.hpp"

namespace kernetile::platform {

void PresentationSync::on_enter_size_move() {
    m_in_modal_size_move = true;
    m_modal_frame_count = 0;
    m_total_modal_time_ms = 0.0;
    m_start_time = std::chrono::high_resolution_clock::now();
    m_last_frame_time = m_start_time;
}

void PresentationSync::on_sizing(uint32_t new_width, uint32_t new_height) {
    if (!m_in_modal_size_move) {
        return;
    }

    auto now = std::chrono::high_resolution_clock::now();
    // 驱动内部重绘与呈现
    if (m_render_callback) {
        m_render_callback(new_width, new_height);
    }

    m_modal_frame_count++;
    m_last_frame_time = now;
}

void PresentationSync::on_paint(uint32_t current_width, uint32_t current_height) {
    if (m_render_callback) {
        m_render_callback(current_width, current_height);
    }
    if (m_in_modal_size_move) {
        m_modal_frame_count++;
        m_last_frame_time = std::chrono::high_resolution_clock::now();
    }
}

void PresentationSync::on_exit_size_move() {
    if (!m_in_modal_size_move) {
        return;
    }

    auto now = std::chrono::high_resolution_clock::now();
    m_total_modal_time_ms = std::chrono::duration<double, std::milli>(now - m_start_time).count();
    m_in_modal_size_move = false;
}

double PresentationSync::get_average_modal_fps() const {
    if (m_total_modal_time_ms <= 0.001 || m_modal_frame_count == 0) {
        return 0.0;
    }
    return static_cast<double>(m_modal_frame_count) / (m_total_modal_time_ms / 1000.0);
}

void PresentationSync::reset_metrics() {
    m_modal_frame_count = 0;
    m_total_modal_time_ms = 0.0;
    m_in_modal_size_move = false;
}

} // namespace kernetile::platform
