#include "headless_backend.hpp"
#include <algorithm>

namespace ktui {

// ==================== HeadlessDispatcher ====================

void HeadlessDispatcher::post_task(Fn<void()> task) {
    if (!task) return;
    LightLock lock(m_mutex);
    m_queue.push_back(std::move(task));
}

size_t HeadlessDispatcher::process_tasks() {
    std::vector<Fn<void()>> local_tasks;
    {
        LightLock lock(m_mutex);
        local_tasks.swap(m_queue);
    }
    for (auto& task : local_tasks) {
        if (task) task();
    }
    return local_tasks.size();
}

bool HeadlessDispatcher::has_pending_tasks() const {
    LightLock lock(m_mutex);
    return !m_queue.empty();
}

// ==================== HeadlessVSyncSource ====================

uint64_t HeadlessVSyncSource::request_frame(Fn<void(double timestamp_seconds)> callback) {
    if (!callback) return 0;
    uint64_t id = m_next_id++;
    m_requests.push_back({id, std::move(callback)});
    return id;
}

void HeadlessVSyncSource::cancel_frame(uint64_t request_id) {
    auto it = std::remove_if(m_requests.begin(), m_requests.end(),
                             [request_id](const Request& req) { return req.id == request_id; });
    m_requests.erase(it, m_requests.end());
}

bool HeadlessVSyncSource::is_sleeping() const {
    return m_requests.empty();
}

void HeadlessVSyncSource::step(double delta_seconds) {
    m_current_time += delta_seconds;
    std::vector<Request> local_requests;
    local_requests.swap(m_requests);
    for (auto& req : local_requests) {
        if (req.callback) {
            req.callback(m_current_time);
        }
    }
}

// ==================== DefaultFontResolver ====================

std::string DefaultFontResolver::resolve_font(FontFamilyCategory category, FontWeight /*weight*/) {
#if defined(_WIN32)
    switch (category) {
        case FontFamilyCategory::System: return "Segoe UI";
        case FontFamilyCategory::SansSerif: return "Arial";
        case FontFamilyCategory::Serif: return "Times New Roman";
        case FontFamilyCategory::Monospace: return "Consolas";
    }
#elif defined(__APPLE__)
    switch (category) {
        case FontFamilyCategory::System: return ".AppleSystemUIFont";
        case FontFamilyCategory::SansSerif: return "Helvetica Neue";
        case FontFamilyCategory::Serif: return "Georgia";
        case FontFamilyCategory::Monospace: return "Menlo";
    }
#else
    switch (category) {
        case FontFamilyCategory::System: return "Noto Sans";
        case FontFamilyCategory::SansSerif: return "DejaVu Sans";
        case FontFamilyCategory::Serif: return "DejaVu Serif";
        case FontFamilyCategory::Monospace: return "Noto Sans Mono";
    }
#endif
    return "builtin";
}

std::string DefaultFontResolver::resolve_font_by_name(const std::string& family_name) {
    if (family_name.empty()) {
        return resolve_font(FontFamilyCategory::System);
    }
    return family_name;
}

// ==================== HeadlessWindow ====================

HeadlessWindow::HeadlessWindow(const WindowDesc& desc)
    : m_title(desc.title),
      m_size(desc.size),
      m_min_size(desc.min_size),
      m_max_size(desc.max_size),
      m_position(desc.position.x < 0.0f ? Vec2{100.0f, 100.0f} : desc.position),
      m_always_on_top(desc.always_on_top),
      m_resizable(desc.resizable),
      m_decorated(desc.decorated) {
    
    size_t pixel_count = static_cast<size_t>(std::max(1.0f, m_size.x) * std::max(1.0f, m_size.y));
    m_pixels.resize(pixel_count, 0x00000000);
}

void HeadlessWindow::set_size(Vec2 new_size) {
    float clamped_w = std::clamp(new_size.x, m_min_size.x, m_max_size.x);
    float clamped_h = std::clamp(new_size.y, m_min_size.y, m_max_size.y);
    Vec2 clamped{clamped_w, clamped_h};

    if (clamped.x != m_size.x || clamped.y != m_size.y) {
        m_size = clamped;
        size_t pixel_count = static_cast<size_t>(std::max(1.0f, m_size.x) * std::max(1.0f, m_size.y));
        m_pixels.resize(pixel_count, 0x00000000);
        if (m_on_resized) {
            m_on_resized(m_size);
        }
    }
}

void HeadlessWindow::set_position(Vec2 pos) {
    if (pos.x != m_position.x || pos.y != m_position.y) {
        m_position = pos;
        if (m_on_moved) {
            m_on_moved(m_position);
        }
    }
}

void HeadlessWindow::center_on_screen() {
    // 假设 1080p 虚拟显示器
    Vec2 screen_sz{1920.0f, 1080.0f};
    Vec2 centered{(screen_sz.x - m_size.x) * 0.5f, (screen_sz.y - m_size.y) * 0.5f};
    set_position(centered);
}

Rect HeadlessWindow::screen_bounds() const {
    return Rect{m_position.x, m_position.y, m_size.x, m_size.y};
}

void HeadlessWindow::set_dpi_scale(float scale) {
    if (scale != m_dpi_scale && scale > 0.1f) {
        m_dpi_scale = scale;
        if (m_on_dpi_changed) {
            m_on_dpi_changed(m_dpi_scale);
        }
    }
}

void HeadlessWindow::present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                                         uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!rgba || x >= width || y >= height) return;
    m_pixels.resize(static_cast<size_t>(width) * height);
    auto* pixels = reinterpret_cast<uint8_t*>(m_pixels.data());
    const size_t copy_bytes = static_cast<size_t>(std::min(w, width - x)) * 4;
    const auto rows = std::min(h, height - y);
    for (uint32_t row = 0; row < rows; ++row) {
        const size_t offset = (static_cast<size_t>(y + row) * width + x) * 4;
        std::copy_n(rgba + offset, copy_bytes, pixels + offset);
    }
}

void HeadlessWindow::close() {
    if (m_on_close) {
        if (!m_on_close()) {
            return; // 拦截阻止关闭
        }
    }
    m_visible = false;
}

// ==================== HeadlessBackend ====================

void HeadlessBackend::poll_events() {
    while (!m_event_queue.empty()) {
        PlatformEvent event = m_event_queue.front();
        m_event_queue.pop_front();
        if (m_event_callback) {
            m_event_callback(event);
        }
    }
}

void HeadlessBackend::wait_events(uint32_t /*timeout_ms*/) {
    poll_events();
}

std::unique_ptr<IPlatformWindow> HeadlessBackend::create_window(const WindowDesc& desc) {
    return std::make_unique<HeadlessWindow>(desc);
}

void HeadlessBackend::inject_event(const PlatformEvent& event) {
    m_event_queue.push_back(event);
}

} // namespace ktui
