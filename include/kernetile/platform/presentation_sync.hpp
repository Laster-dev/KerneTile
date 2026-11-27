#pragma once

#include <cstdint>
#include <chrono>
#include <cstring>
#include <type_traits>
#include <utility>

namespace kernetile::platform {

// ============================================================================
// 平台呈现与刷新同步管理器
// 拦截 Windows WM_ENTERSIZEMOVE 模态拖拽循环，在系统回调中维持 60-120 Hz 刷新
// ============================================================================
class PresentationSync {
public:
    // 零堆分配轻量回调（替代 std::function，无异常/无 RTTI/极小二进制体积）
    class RenderFrameCallback {
    public:
        RenderFrameCallback() noexcept = default;
        RenderFrameCallback(std::nullptr_t) noexcept {}

        template <typename F,
                  typename Decay = std::decay_t<F>,
                  typename = std::enable_if_t<!std::is_same_v<Decay, RenderFrameCallback>>>
        RenderFrameCallback(F&& f) noexcept {
            static_assert(sizeof(Decay) <= sizeof(m_storage) && alignof(Decay) <= alignof(void*),
                          "RenderFrameCallback capture exceeds 32-byte SBO");
            static_assert(std::is_trivially_copyable_v<Decay>,
                          "RenderFrameCallback requires trivially copyable callable");
            std::memcpy(m_storage, &f, sizeof(Decay));
            m_invoke = [](const void* raw, uint32_t w, uint32_t h) {
                (*static_cast<const Decay*>(raw))(w, h);
            };
        }

        explicit operator bool() const noexcept { return m_invoke != nullptr; }

        void operator()(uint32_t width, uint32_t height) const {
            if (m_invoke) {
                m_invoke(m_storage, width, height);
            }
        }

    private:
        alignas(void*) unsigned char m_storage[32]{};
        void (*m_invoke)(const void*, uint32_t, uint32_t){nullptr};
    };

    PresentationSync() = default;

    // 设置渲染回调
    void set_render_callback(RenderFrameCallback cb) noexcept {
        m_render_callback = cb;
    }

    // 处理 WM_ENTERSIZEMOVE: 用户按住窗口边框开始调整大小
    void on_enter_size_move();

    // 处理 WM_SIZING: 拖拽过程中尺寸实时改变
    void on_sizing(uint32_t new_width, uint32_t new_height);

    // 处理 WM_PAINT: 模态循环中系统要求重绘
    void on_paint(uint32_t current_width, uint32_t current_height);

    // 处理 WM_EXITSIZEMOVE: 拖拽释放
    void on_exit_size_move();

    // 查询当前是否处于系统模态拉伸周期
    bool is_in_modal_size_move() const { return m_in_modal_size_move; }

    // 性能指标查询
    uint32_t get_modal_frame_count() const { return m_modal_frame_count; }
    double   get_total_modal_time_ms() const { return m_total_modal_time_ms; }
    double   get_average_modal_fps() const;

    // 重置统计指标
    void reset_metrics();

private:
    bool                m_in_modal_size_move{false};
    RenderFrameCallback m_render_callback{};

    uint32_t m_modal_frame_count{0};
    double   m_total_modal_time_ms{0.0};

    std::chrono::high_resolution_clock::time_point m_start_time{};
    std::chrono::high_resolution_clock::time_point m_last_frame_time{};
};

} // namespace kernetile::platform
