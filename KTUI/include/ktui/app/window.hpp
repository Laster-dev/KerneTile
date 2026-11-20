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

    [[nodiscard]] static Window* active_window() noexcept {
        return s_active_window;
    }

    // 渲染当前帧并自动呈现到物理窗口
    void render_frame() {
        if (!m_root_render_box || !m_platform_window) return;
        Vec2 sz = m_platform_window->size();
        uint32_t w = static_cast<uint32_t>(sz.x);
        uint32_t h = static_cast<uint32_t>(sz.y);
        if (w == 0 || h == 0) return;

        ensure_renderer();
        if (!m_renderer) return;

        if (m_renderer_w != w || m_renderer_h != h) {
            m_renderer->resize(w, h);
            m_renderer_w = w;
            m_renderer_h = h;
        }

        // 1. 布局
        BoxConstraints constraints = BoxConstraints::tight(Vec2{static_cast<float>(w), static_cast<float>(h)});
        m_root_render_box->layout(constraints);

        // 2. 绘制图元
        m_renderer->begin_frame();
        PaintContext ctx(m_renderer.get());
        m_root_render_box->paint(ctx);
        // 2.1 绘制顶层独立浮层 (脱离所有父级 clip 硬件裁剪)
        m_overlay_manager.layout_and_paint(ctx, Vec2{static_cast<float>(w), static_cast<float>(h)});
        m_renderer->end_frame();

        // 3. 光栅化输出 RGBA8
        m_framebuffer.resize(static_cast<size_t>(w) * h * 4);
        m_renderer->render_to_rgba8(m_framebuffer.data());

        // 4. 呈现到平台窗口
        m_platform_window->present_rgba8(m_framebuffer.data(), w, h);
        clear_redraw_flag();
    }

    // 跨平台导出当前帧为 32-bit BMP 位图（零平台 API 依赖）
    bool export_bmp(const std::string& filepath) {
        if (m_framebuffer.empty() || m_renderer_w == 0 || m_renderer_h == 0) {
            render_frame();
        }
        if (m_framebuffer.empty() || m_renderer_w == 0 || m_renderer_h == 0) return false;

#pragma pack(push, 1)
        struct BmpHeader {
            uint16_t bfType{0x4D42};
            uint32_t bfSize{0};
            uint16_t bfReserved1{0};
            uint16_t bfReserved2{0};
            uint32_t bfOffBits{54};
            uint32_t biSize{40};
            int32_t  biWidth{0};
            int32_t  biHeight{0};
            uint16_t biPlanes{1};
            uint16_t biBitCount{32};
            uint32_t biCompression{0};
            uint32_t biSizeImage{0};
            int32_t  biXPelsPerMeter{2835};
            int32_t  biYPelsPerMeter{2835};
            uint32_t biClrUsed{0};
            uint32_t biClrImportant{0};
        };
#pragma pack(pop)

        const uint32_t w = m_renderer_w;
        const uint32_t h = m_renderer_h;
        BmpHeader header;
        header.biWidth = static_cast<int32_t>(w);
        header.biHeight = -static_cast<int32_t>(h);
        header.biSizeImage = w * h * 4;
        header.bfSize = 54 + header.biSizeImage;

        std::vector<uint8_t> bgra(static_cast<size_t>(w) * h * 4);
        const size_t count = static_cast<size_t>(w) * h;
        for (size_t i = 0; i < count; ++i) {
            bgra[i * 4 + 0] = m_framebuffer[i * 4 + 2];
            bgra[i * 4 + 1] = m_framebuffer[i * 4 + 1];
            bgra[i * 4 + 2] = m_framebuffer[i * 4 + 0];
            bgra[i * 4 + 3] = 255;
        }

        FILE* fp = std::fopen(filepath.c_str(), "wb");
        if (!fp) return false;
        std::fwrite(&header, sizeof(header), 1, fp);
        std::fwrite(bgra.data(), 1, bgra.size(), fp);
        std::fclose(fp);
        return true;
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
                    m_platform_window->invalidate();
                }
                request_redraw();
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
                    KeyEvent ev(PlatformEventType::KeyDown, pe.key_code, "", pe.modifiers);
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
            default:
                break;
        }
    }

    // ==================== 重绘调度与脏标记 ====================
    void request_redraw() noexcept {
        m_needs_redraw = true;
    }

    [[nodiscard]] bool needs_redraw() const noexcept {
        return m_needs_redraw;
    }

    void clear_redraw_flag() noexcept {
        m_needs_redraw = false;
    }

    // ==================== 关闭与生命周期 ====================
    void close() {
        if (!m_is_closed) {
            m_is_closed = true;
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
        uint32_t w = static_cast<uint32_t>(std::max(1.0f, sz.x));
        uint32_t h = static_cast<uint32_t>(std::max(1.0f, sz.y));
        kernetile::RendererConfig cfg;
        cfg.width = w;
        cfg.height = h;
        cfg.prefer_warp = true;
        m_renderer = kernetile::KerneTileRenderer::create(cfg);
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
            render_frame();
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
