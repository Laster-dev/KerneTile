#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <ktui/widgets/widget.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/event/event_dispatcher.hpp>
#include <ktui/event/focus_tree.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <kernetile/renderer.hpp>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <cstdio>

namespace ktui {

class Application;

// 逻辑应用窗口抽象 (Logical Application Window)
// 统一管理底层物理平台窗口生命周期、DPI 自适应缩放、视图树挂接与重绘调度
class Window {
public:
    Window(uint32_t id, std::unique_ptr<IPlatformWindow> platform_window)
        : m_id(id), m_platform_window(std::move(platform_window)) {
        init_internal_hooks();
    }

    virtual ~Window() {
        close();
    }

    [[nodiscard]] uint32_t id() const noexcept { return m_id; }
    [[nodiscard]] IPlatformWindow* platform_window() const noexcept { return m_platform_window.get(); }

    // ==================== 几何与尺寸 ====================
    [[nodiscard]] Vec2 size() const {
        return m_platform_window ? m_platform_window->size() : Vec2{0.0f, 0.0f};
    }

    void set_size(Vec2 sz) {
        if (m_platform_window) m_platform_window->set_size(sz);
    }

    [[nodiscard]] Vec2 position() const {
        return m_platform_window ? m_platform_window->position() : Vec2{0.0f, 0.0f};
    }

    void set_position(Vec2 pos) {
        if (m_platform_window) m_platform_window->set_position(pos);
    }

    void center_on_screen() {
        if (m_platform_window) m_platform_window->center_on_screen();
    }

    [[nodiscard]] Rect screen_bounds() const {
        return m_platform_window ? m_platform_window->screen_bounds() : Rect{};
    }

    // ==================== 呈现状态 ====================
    void show() {
        if (m_platform_window) m_platform_window->show();
    }

    void hide() {
        if (m_platform_window) m_platform_window->hide();
    }

    [[nodiscard]] bool is_visible() const {
        return m_platform_window ? m_platform_window->is_visible() : false;
    }

    void minimize() {
        if (m_platform_window) m_platform_window->minimize();
    }

    void maximize() {
        if (m_platform_window) m_platform_window->maximize();
    }

    void restore() {
        if (m_platform_window) m_platform_window->restore();
    }

    [[nodiscard]] bool is_minimized() const {
        return m_platform_window ? m_platform_window->is_minimized() : false;
    }

    [[nodiscard]] bool is_maximized() const {
        return m_platform_window ? m_platform_window->is_maximized() : false;
    }

    void set_fullscreen(bool fullscreen) {
        if (m_platform_window) m_platform_window->set_fullscreen(fullscreen);
    }

    [[nodiscard]] bool is_fullscreen() const {
        return m_platform_window ? m_platform_window->is_fullscreen() : false;
    }

    void activate() {
        if (m_platform_window) m_platform_window->activate();
    }

    void focus() {
        if (m_platform_window) m_platform_window->focus();
    }

    [[nodiscard]] bool has_focus() const {
        return m_platform_window ? m_platform_window->has_focus() : false;
    }

    void set_always_on_top(bool on_top) {
        if (m_platform_window) m_platform_window->set_always_on_top(on_top);
    }

    // ==================== 外观与标题栏 ====================
    [[nodiscard]] std::string title() const {
        return m_platform_window ? m_platform_window->title() : "";
    }

    void set_title(const std::string& t) {
        if (m_platform_window) m_platform_window->set_title(t);
    }

    void set_opacity(float opacity) {
        if (m_platform_window) m_platform_window->set_opacity(opacity);
    }

    [[nodiscard]] float opacity() const {
        return m_platform_window ? m_platform_window->opacity() : 1.0f;
    }

    // ==================== DPI 与显示参数 ====================
    [[nodiscard]] float dpi_scale() const {
        return m_platform_window ? m_platform_window->dpi_scale() : 1.0f;
    }

    [[nodiscard]] uint32_t refresh_rate() const {
        return m_platform_window ? m_platform_window->refresh_rate() : 60;
    }

    // ==================== 视图内容挂接与全托管渲染 ====================
    void set_content(WidgetPtr content) {
        m_content_widget = std::move(content);
        m_root_render_box = m_content_widget ? m_content_widget->create_render_node() : nullptr;
        m_event_dispatcher.clear();
        m_focus_manager.clear_focus();
        ensure_renderer();
        request_redraw();
        if (m_platform_window) {
            m_platform_window->invalidate();
        }
    }

    [[nodiscard]] RenderBox* root_render_box() const noexcept {
        return m_root_render_box.get();
    }

    [[nodiscard]] FocusManager& focus_manager() noexcept { return m_focus_manager; }
    [[nodiscard]] EventDispatcher& event_dispatcher() noexcept { return m_event_dispatcher; }

    // ==================== 浮层与弹出层管理器 (Overlay Manager) ====================
    [[nodiscard]] OverlayManager& overlay_manager() noexcept { return m_overlay_manager; }
    [[nodiscard]] const OverlayManager& overlay_manager() const noexcept { return m_overlay_manager; }

    uint64_t show_overlay(OverlayEntry entry) {
        uint64_t id = m_overlay_manager.show(std::move(entry));
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
        return id;
    }

    void hide_overlay(uint64_t id) {
        m_overlay_manager.hide(id);
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }

    void dismiss_all_overlays() {
        m_overlay_manager.dismiss_all();
        request_redraw();
        if (m_platform_window) m_platform_window->invalidate();
    }

    [[nodiscard]] bool has_active_overlay() const noexcept {
        return m_overlay_manager.has_active_overlay();
    }

    [[nodiscard]] bool has_blinking_cursor() const noexcept {
        if (auto* client = dynamic_cast<ITextInputClient*>(m_focus_manager.focused_node())) {
            return client->accepts_text_input();
        }
        return false;
    }

    bool tick_blinking_cursor() {
        if (auto* client = dynamic_cast<ITextInputClient*>(m_focus_manager.focused_node())) {
            if (client->accepts_text_input()) {
                return client->tick_cursor_blink();
            }
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept {
        if (auto* client = dynamic_cast<ITextInputClient*>(m_focus_manager.focused_node())) {
            if (client->accepts_text_input()) {
                return client->time_until_next_blink_ms();
            }
        }
        return 0;
    }

    [[nodiscard]] static Window* active_window() noexcept {
        return s_active_window;
    }

    // 渲染当前帧并自动呈现到物理窗口
    // force_full_present = true 用于响应系统 WM_PAINT / X11 Expose 等外部重绘请求，
    // 即使脏瓦片为 0，也必须将现有帧缓冲完整提交到物理窗口表面，彻底避免首帧/遮挡恢复后白屏。
    void render_frame(bool force_full_present = false) {
        if (!m_root_render_box || !m_platform_window) return;
        Vec2 sz = m_platform_window->size();
        if (sz.x <= 0 || sz.y <= 0) return;
        const float scale = m_platform_window->render_scale();
        const auto w = static_cast<uint32_t>(std::ceil(sz.x * scale));
        const auto h = static_cast<uint32_t>(std::ceil(sz.y * scale));

        ensure_renderer();
        if (!m_renderer) return;
        if (m_compute_was_ready != m_renderer->compute_ready()) force_full_present = true;
        m_compute_was_ready = m_renderer->compute_ready();

        // 极速曝光恢复路径：系统要求重绘表面但 UI 状态零变动，直接将已有帧缓冲提交给窗口，免除重复排版与光栅化
        if (force_full_present && !m_compute_was_ready &&
            m_renderer_w == w && m_renderer_h == h && m_render_scale == scale &&
            m_framebuffer.size() == static_cast<size_t>(w) * h * 4 &&
            !needs_redraw() &&
            !m_root_render_box->needs_paint() &&
            !m_root_render_box->needs_layout())
        {
            m_platform_window->present_rgba8(m_framebuffer.data(), w, h);
            return;
        }

        if (m_renderer_w != w || m_renderer_h != h || m_render_scale != scale) {
            m_render_scale = scale;
            m_renderer->resize(w, h);
            m_renderer_w = w;
            m_renderer_h = h;
            force_full_present = true;
        }

        // 1. 布局
        BoxConstraints constraints = BoxConstraints::tight(sz);
        m_root_render_box->layout(constraints);

        // 2. 清理上一帧的历史脏标记 (必须在 paint() 之前清空)
        // 这样任何正在播放动效的子控件（Checkbox 对勾生长、TextBox 底部焦点线滑动、Button 水波纹等）
        // 在 paint() 内部步进动效时调用的 mark_needs_paint() 将被正确保留，
        // 从而驱动主循环继续推进下一帧，彻底解决鼠标静止时动效中途卡住“留尾巴”的问题！
        if (m_root_render_box) {
            m_root_render_box->clear_dirty_recursive();
        }
        clear_redraw_flag();

        // 3. 绘制图元 (运行中的动效将在 paint 中重新 mark_needs_paint())
        m_renderer->begin_frame();
        PaintContext ctx(m_renderer.get(), scale);
        m_root_render_box->paint(ctx);
        // 3.1 绘制顶层独立浮层 (脱离所有父级 clip 硬件裁剪)
        m_overlay_manager.layout_and_paint(ctx, sz);
        m_renderer->end_frame();

        // The compute backend owns its surface and presents the composed texture.
        if (m_renderer->present_computed_frame(force_full_present)) {
            m_framebuffer.clear();
            return;
        }
        if (m_framebuffer.empty()) force_full_present = true;
        m_framebuffer.resize(static_cast<size_t>(w) * h * 4);
        m_renderer->render_to_rgba8(m_framebuffer.data());

        // 5. 呈现到平台窗口 (外部曝光强制全量提交；动效/交互仅对发生实际像素变动的局部脏区提交物理呈现)
        const auto& stats = m_renderer->get_last_frame_stats();
        if (force_full_present) {
            m_platform_window->present_rgba8(m_framebuffer.data(), w, h);
        } else if (stats.dirty_tiles > 0) {
            uint32_t dirty_x = 0, dirty_y = 0, dirty_w = w, dirty_h = h;
            if (m_renderer->get_last_dirty_bounds(dirty_x, dirty_y, dirty_w, dirty_h)) {
                m_platform_window->present_rgba8_region(m_framebuffer.data(), w, h, dirty_x, dirty_y, dirty_w, dirty_h);
            } else {
                m_platform_window->present_rgba8(m_framebuffer.data(), w, h);
            }
        }
    }

    // 框架级平台事件统一路由（指针、焦点、键盘、IME 全部自动托管）
    void handle_platform_event(const PlatformEvent& pe) {
        s_active_window = this;
        if (!m_root_render_box) return;

        // 优先让浮层系统拦截处理输入事件
        if (m_overlay_manager.has_active_overlay()) {
            if (pe.type == PlatformEventType::PointerDown) {
                bool handled = m_overlay_manager.handle_pointer_down(pe.position, pe.button);
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
                if (handled) return;
            } else if (pe.type == PlatformEventType::PointerMove) {
                bool handled = m_overlay_manager.handle_pointer_move(pe.position);
                if (handled) {
                    request_redraw();
                    if (m_platform_window) m_platform_window->invalidate();
                    return;
                }
            } else if (pe.type == PlatformEventType::PointerUp) {
                bool handled = m_overlay_manager.handle_pointer_up(pe.position, pe.button);
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
                if (handled) return;
            } else if (pe.type == PlatformEventType::KeyDown && pe.key_code == 27) {
                // Esc 键优先关闭顶层浮层
                m_overlay_manager.dismiss_top();
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
                return;
            }
            if (m_overlay_manager.has_modal_overlay()) {
                // 模态弹窗激活时阻断向底层派发其他事件
                return;
            }
        }

        switch (pe.type) {
            case PlatformEventType::PointerMove: {
                PointerEvent ev(PlatformEventType::PointerMove, pe.position, pe.button);
                m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), ev);
                // 自动同步鼠标光标形状 (Hover 按钮变手型、输入框变 I-Beam)
                if (m_platform_window) {
                    CursorShape cursor = CursorShape::Arrow;
                    HitTestResult hit_res;
                    if (m_root_render_box->hit_test(hit_res, pe.position)) {
                        for (const auto& entry : hit_res.path()) {
                            if (entry.target && entry.target->cursor_shape != CursorShape::Arrow) {
                                cursor = entry.target->cursor_shape;
                                break;
                            }
                        }
                    }
                    m_platform_window->set_cursor(cursor);
                }
                break;
            }
            case PlatformEventType::PointerScroll: {
                PointerEvent ev(PlatformEventType::PointerScroll, pe.position, PointerButton::None);
                ev.delta = pe.delta;
                m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), ev);
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
                break;
            }
            case PlatformEventType::PointerDown: {
                // 焦点命中测试
                HitTestResult hit_res;
                RenderBox* new_focus = nullptr;
                if (m_root_render_box->hit_test(hit_res, pe.position)) {
                    for (const auto& entry : hit_res.path()) {
                        if (entry.target && entry.target->is_focusable) {
                            new_focus = entry.target;
                            break;
                        }
                    }
                }
                if (new_focus) {
                    m_focus_manager.request_focus(new_focus);
                } else {
                    m_focus_manager.clear_focus();
                }

                PointerEvent ev(PlatformEventType::PointerDown, pe.position, pe.button);
                m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), ev);
                sync_ime_state();
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
                break;
            }
            case PlatformEventType::PointerUp: {
                PointerEvent ev(PlatformEventType::PointerUp, pe.position, pe.button);
                m_event_dispatcher.dispatch_pointer_event(m_root_render_box.get(), ev);
                sync_ime_state();
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
                break;
            }
            case PlatformEventType::KeyDown: {
                constexpr uint32_t kKeyTab = 9;
                constexpr uint32_t kKeyEscape = 27;
                if (pe.key_code == kKeyEscape) {
                    m_focus_manager.clear_focus();
                    sync_ime_state();
                } else if (pe.key_code == kKeyTab) {
                    m_focus_manager.handle_tab_navigation(m_root_render_box.get(), pe.modifiers.shift);
                    sync_ime_state();
                } else {
                    KeyEvent ev(PlatformEventType::KeyDown, pe.key_code, pe.text, pe.modifiers);
                    m_event_dispatcher.dispatch_key_event(m_focus_manager.focused_node(), m_root_render_box.get(), ev);
                    sync_ime_state();
                }
                request_redraw();
                if (m_platform_window) m_platform_window->invalidate();
                break;
            }
            case PlatformEventType::ImeComposition: {
                if (auto* client = dynamic_cast<ITextInputClient*>(m_focus_manager.focused_node())) {
                    if (client->accepts_text_input()) {
                        client->on_ime_preedit(pe.text, pe.text.size());
                        sync_ime_state();
                        request_redraw();
                        if (m_platform_window) m_platform_window->invalidate();
                    }
                }
                break;
            }
            case PlatformEventType::ImeCommit: {
                RenderBox* focused = m_focus_manager.focused_node();
                if (auto* client = dynamic_cast<ITextInputClient*>(focused)) {
                    if (client->accepts_text_input()) {
                        client->on_ime_commit(pe.text);
                        sync_ime_state();
                        request_redraw();
                        if (m_platform_window) m_platform_window->invalidate();
                        break;
                    }
                }
                if (focused) {
                    KeyEvent ev(PlatformEventType::ImeCommit, pe.key_code, pe.text, pe.modifiers);
                    m_event_dispatcher.dispatch_key_event(focused, m_root_render_box.get(), ev);
                    request_redraw();
                    if (m_platform_window) m_platform_window->invalidate();
                }
                break;
            }
            case PlatformEventType::WindowCloseRequested: {
                bool allow_close = true;
                for (auto& cb : m_close_callbacks) {
                    if (cb && !cb()) {
                        allow_close = false;
                    }
                }
                if (allow_close) {
                    close();
                }
                break;
            }
            default:
                break;
        }
    }

    // ==================== 重绘调度与脏标记 ====================
    void request_redraw() noexcept {
        m_needs_redraw = true;
    }

    [[nodiscard]] bool needs_redraw() const noexcept {
        return m_needs_redraw || (m_renderer && (m_renderer->compute_ready() != m_compute_was_ready ||
            m_platform_window->render_scale() != m_render_scale));
    }

    void clear_redraw_flag() noexcept {
        m_needs_redraw = false;
    }

    // ==================== 关闭与生命周期 ====================
    void close() {
        if (!m_is_closed) {
            m_is_closed = true;
            m_renderer.reset();
            if (m_platform_window) {
                m_platform_window->close();
            }
        }
    }

    [[nodiscard]] bool is_closed() const noexcept {
        return m_is_closed;
    }

    // ==================== 回调监听挂接 ====================
    void on_resize(std::function<void(Vec2)> handler) {
        m_resize_callbacks.push_back(std::move(handler));
    }

    void on_close(std::function<bool()> handler) {
        m_close_callbacks.push_back(std::move(handler));
    }

    void on_dpi_changed(std::function<void(float)> handler) {
        m_dpi_callbacks.push_back(std::move(handler));
    }

    void on_focus_changed(std::function<void(bool)> handler) {
        m_focus_callbacks.push_back(std::move(handler));
    }

private:
    void ensure_renderer() {
        if (m_renderer || !m_platform_window) return;
        Vec2 sz = m_platform_window->size();
        m_render_scale = m_platform_window->render_scale();
        const auto w = static_cast<uint32_t>(std::max(1.0f, std::ceil(sz.x * m_render_scale)));
        const auto h = static_cast<uint32_t>(std::max(1.0f, std::ceil(sz.y * m_render_scale)));
        kernetile::RendererConfig cfg;
        cfg.width = w;
        cfg.height = h;
        m_renderer = kernetile::KerneTileRenderer::create(cfg);
        m_renderer->set_compute_target(m_platform_window->get_native_handle(),
                                       m_platform_window->get_compute_display());
        m_renderer_w = w;
        m_renderer_h = h;
    }

    void sync_ime_state() {
        if (!m_platform_window) return;
        RenderBox* focused = m_focus_manager.focused_node();
        if (auto* client = dynamic_cast<ITextInputClient*>(focused)) {
            if (client->accepts_text_input()) {
                m_platform_window->set_ime_enabled(true);
                m_platform_window->set_ime_cursor_rect(client->get_ime_cursor_rect());
                return;
            }
        }
        m_platform_window->set_ime_enabled(false);
    }

    void init_internal_hooks() {
        if (!m_platform_window) return;

        m_platform_window->on_paint_requested([this]() {
            render_frame(true);
        });

        m_platform_window->on_resized([this](Vec2 sz) {
            request_redraw();
            for (auto& cb : m_resize_callbacks) {
                if (cb) cb(sz);
            }
        });

        m_platform_window->on_close_requested([this]() -> bool {
            bool allow_close = true;
            for (auto& cb : m_close_callbacks) {
                if (cb && !cb()) {
                    allow_close = false;
                }
            }
            if (allow_close) {
                m_is_closed = true;
            }
            return allow_close;
        });

        m_platform_window->on_dpi_changed([this](float dpi) {
            request_redraw();
            for (auto& cb : m_dpi_callbacks) {
                if (cb) cb(dpi);
            }
        });

        m_platform_window->on_focus_changed([this](bool focused) {
            for (auto& cb : m_focus_callbacks) {
                if (cb) cb(focused);
            }
        });
    }

    uint32_t m_id{0};
    std::unique_ptr<IPlatformWindow> m_platform_window;
    bool m_is_closed{false};
    bool m_needs_redraw{true};
    bool m_compute_was_ready{false};
    float m_render_scale{1.0f};

    WidgetPtr m_content_widget;
    std::shared_ptr<RenderBox> m_root_render_box;
    std::unique_ptr<kernetile::KerneTileRenderer> m_renderer;
    uint32_t m_renderer_w{0};
    uint32_t m_renderer_h{0};
    std::vector<uint8_t> m_framebuffer;
    EventDispatcher m_event_dispatcher;
    FocusManager m_focus_manager;
    OverlayManager m_overlay_manager;

    inline static Window* s_active_window = nullptr;

    std::vector<std::function<void(Vec2)>> m_resize_callbacks;
    std::vector<std::function<bool()>> m_close_callbacks;
    std::vector<std::function<void(float)>> m_dpi_callbacks;
    std::vector<std::function<void(bool)>> m_focus_callbacks;
};

} // namespace ktui
