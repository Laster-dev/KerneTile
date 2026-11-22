#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <cstring>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>

namespace ktui {

class WebBackend;

// ==================== C / Wasm 互操作缓存与函数导出 ====================
namespace detail {
    inline std::vector<char> s_text_buf;
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    inline char* ktui_web_get_text_buf(size_t size) {
        detail::s_text_buf.resize(size);
        return detail::s_text_buf.data();
    }

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_ime_composition(const char* text, size_t cursor);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_ime_commit(const char* text);
}

// JavaScript 原生画布与 DOM 桥接接口
EM_JS(void, ktui_js_init_ime, (), {
    var canvas = document.getElementById('canvas');
    if (!canvas) return;

    if (document.getElementById('ktui_ime_input')) return;
    var input = document.createElement('input');
    input.id = 'ktui_ime_input';
    input.type = 'text';
    input.tabIndex = -1;
    input.disabled = true;
    input.autocomplete = 'off';
    input.spellcheck = false;
    input.style.cssText = 'position:absolute;opacity:0;pointer-events:none;width:1px;height:1px';
    document.body.appendChild(input);

    var composing = false;
    var pendingCommit = null;
    var send = function(text, cursor) {
        var size = lengthBytesUTF8(text) + 1;
        var buf = _ktui_web_get_text_buf(size);
        stringToUTF8(text, buf, size);
        if (cursor === undefined) _ktui_web_on_ime_commit(buf);
        else _ktui_web_on_ime_composition(buf, lengthBytesUTF8(text.slice(0, cursor)));
    };
    input.ktuiIsComposing = function() { return composing; };
    input.addEventListener('keydown', function(e) {
        if (!composing && !e.isComposing && e.keyCode !== 229) pendingCommit = null;
    });
    input.addEventListener('compositionstart', function() {
        composing = true;
        pendingCommit = null;
    });
    input.addEventListener('compositionupdate', function(e) {
        if (!composing || input.disabled) return;
        var text = e.data || '';
        var cursor = input.value === text ? input.selectionEnd : text.length;
        send(text, cursor);
    });
    input.addEventListener('compositionend', function(e) {
        if (!composing || input.disabled) return;
        composing = false;
        var text = e.data || '';
        pendingCommit = text || null;
        input.value = '';
        if (text) send(text);
        else send('', 0);
    });
    input.addEventListener('input', function(e) {
        if (input.disabled || composing || e.isComposing) return;
        var text = e.data ?? input.value;
        var inputType = e.inputType || '';
        var duplicate = pendingCommit !== null &&
            (inputType === 'insertFromComposition' ||
                ((inputType === 'insertText' || inputType === 'insertCompositionText') &&
                    text === pendingCommit));
        pendingCommit = null;
        input.value = '';
        if (text && !duplicate && !inputType.startsWith('delete')) send(text);
    });
    input.addEventListener('blur', function() {
        if (composing) {
            composing = false;
            send('', 0);
        }
        pendingCommit = null;
        input.value = '';
    });
    input.addEventListener('paste', function(e) {
        if (e.clipboardData) window._ktui_clipboard = e.clipboardData.getData('text/plain');
    });
});

EM_JS(int, ktui_js_key_target, (), {
    var target = document.activeElement;
    if (!target) return 0;
    if (target.id === 'canvas') return 1;
    if (target.id !== 'ktui_ime_input' || target.disabled || target.ktuiIsComposing()) return 0;
    return 2;
});

EM_JS(void, ktui_js_set_canvas_size, (int w, int h), {
    var canvas = document.getElementById('canvas');
    if (canvas) {
        canvas.style.width = w + 'px';
        canvas.style.height = h + 'px';
    }
});

EM_JS(void, ktui_js_present_rgba8_region, (const uint8_t* rgba, int width, int height,
                                           int dirty_x, int dirty_y, int dirty_w, int dirty_h), {
    var canvas = document.getElementById('canvas');
    if (!canvas) return;
    var resized = (canvas.width !== width || canvas.height !== height);
    if (resized) {
        canvas.width = width;
        canvas.height = height;
    }
    var ctx = canvas.getContext('2d');
    if (!ctx) return;

    var recreated = !window._ktui_img_data ||
        window._ktui_img_data.width !== width ||
        window._ktui_img_data.height !== height;
    if (recreated) {
        window._ktui_img_data = ctx.createImageData(width, height);
    }
    var data = window._ktui_img_data.data;

    if (resized || recreated || (dirty_x === 0 && dirty_y === 0 && dirty_w >= width && dirty_h >= height)) {
        var src = HEAPU8.subarray(rgba, rgba + width * height * 4);
        data.set(src);
        ctx.putImageData(window._ktui_img_data, 0, 0);
        return;
    }

    var x0 = Math.max(0, Math.min(dirty_x, width));
    var y0 = Math.max(0, Math.min(dirty_y, height));
    var x1 = Math.max(x0, Math.min(dirty_x + dirty_w, width));
    var y1 = Math.max(y0, Math.min(dirty_y + dirty_h, height));
    var rw = x1 - x0;
    var rh = y1 - y0;
    if (rw <= 0 || rh <= 0) return;

    var rowBytes = width * 4;
    var copyBytes = rw * 4;
    for (var y = y0; y < y1; ++y) {
        var rowOffset = y * rowBytes + x0 * 4;
        data.set(HEAPU8.subarray(rgba + rowOffset, rgba + rowOffset + copyBytes), rowOffset);
    }
    ctx.putImageData(window._ktui_img_data, 0, 0, x0, y0, rw, rh);
});

EM_JS(void, ktui_js_set_cursor, (const char* cursor_style), {
    var canvas = document.getElementById('canvas');
    if (canvas) {
        canvas.style.cursor = UTF8ToString(cursor_style);
    }
});

EM_JS(void, ktui_js_set_title, (const char* title), {
    document.title = UTF8ToString(title);
});

EM_JS(void, ktui_js_set_ime_enabled, (bool enabled), {
    var input = document.getElementById('ktui_ime_input');
    if (!input) return;
    if (enabled) {
        input.disabled = false;
        if (document.activeElement !== input) input.focus({preventScroll: true});
    } else if (!input.disabled) {
        var wasFocused = document.activeElement === input;
        input.disabled = true;
        input.value = '';
        if (wasFocused) document.getElementById('canvas')?.focus({preventScroll: true});
    }
});

EM_JS(void, ktui_js_set_ime_cursor_rect, (float x, float y, float w, float h), {
    var canvas = document.getElementById('canvas');
    var input = document.getElementById('ktui_ime_input');
    if (!canvas || !input) return;
    var rect = canvas.getBoundingClientRect();
    input.style.left = (window.scrollX + rect.left + x) + 'px';
    input.style.top = (window.scrollY + rect.top + y + h) + 'px';
});

EM_JS(void, ktui_js_set_clipboard, (const char* text), {
    var str = UTF8ToString(text);
    window._ktui_clipboard = str;
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(str).catch(function(){});
    }
});

EM_JS(void, ktui_js_get_clipboard, (char* out_buf, int max_len), {
    var str = window._ktui_clipboard || "";
    stringToUTF8(str, out_buf, max_len);
});

EM_JS(float, ktui_js_get_dpi_scale, (), {
    return Math.max(1, window.devicePixelRatio || 1);
});

EM_JS(void, ktui_js_get_window_size, (int* out_w, int* out_h), {
    HEAP32[out_w >> 2] = window.innerWidth;
    HEAP32[out_h >> 2] = window.innerHeight;
});

// ==================== Web 平台任务调度器 ====================
class WebDispatcher : public IPlatformDispatcher {
public:
    void post_task(std::function<void()> task) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(task));
    }

    size_t process_tasks() override {
        std::vector<std::function<void()>> to_run;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            to_run.swap(m_queue);
        }
        for (auto& t : to_run) {
            if (t) t();
        }
        return to_run.size();
    }

    bool has_pending_tasks() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_queue.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::function<void()>> m_queue;
};

// ==================== Web 平台虚拟帧时钟源 ====================
class WebVSyncSource : public IVSyncSource {
public:
    uint64_t request_frame(std::function<void(double timestamp_seconds)> callback) override {
        uint64_t id = m_next_id++;
        m_requests.push_back({id, std::move(callback)});
        return id;
    }

    void cancel_frame(uint64_t request_id) override {
        std::erase_if(m_requests, [request_id](const Request& r) { return r.id == request_id; });
    }

    bool is_sleeping() const override {
        return m_requests.empty();
    }

    void trigger_frame(double timestamp_seconds) {
        auto reqs = std::move(m_requests);
        m_requests.clear();
        for (const auto& r : reqs) {
            if (r.callback) r.callback(timestamp_seconds);
        }
    }

private:
    struct Request {
        uint64_t id = 0;
        std::function<void(double)> callback;
    };
    std::vector<Request> m_requests;
    uint64_t m_next_id = 1;
};

// ==================== Web 字体回退解析器 ====================
class WebFontResolver : public IFontResolver {
public:
    std::string resolve_font(FontFamilyCategory category, FontWeight weight = FontWeight::Regular) override {
        (void)category; (void)weight;
        return "sans-serif";
    }

    std::string resolve_font_by_name(const std::string& family_name) override {
        return family_name.empty() ? "sans-serif" : family_name;
    }
};

// ==================== Web 物理窗口抽象实现 ====================
class WebWindow : public IPlatformWindow {
public:
    explicit WebWindow(const WindowDesc& desc)
        : m_title(desc.title),
          m_size(desc.size),
          m_min_size(desc.min_size),
          m_max_size(desc.max_size),
          m_resizable(desc.resizable),
          m_decorated(desc.decorated)
    {
        ktui_js_set_title(m_title.c_str());
        ktui_js_set_canvas_size(static_cast<int>(m_size.x), static_cast<int>(m_size.y));
    }

    ~WebWindow() override {
        close();
    }

    // ==================== 几何与尺寸 ====================
    Vec2 size() const override { return m_size; }
    void set_size(Vec2 new_size) override {
        m_size.x = std::clamp(new_size.x, m_min_size.x, m_max_size.x);
        m_size.y = std::clamp(new_size.y, m_min_size.y, m_max_size.y);
        ktui_js_set_canvas_size(static_cast<int>(m_size.x), static_cast<int>(m_size.y));
        if (m_resize_handler) m_resize_handler(m_size);
    }

    Vec2 min_size() const override { return m_min_size; }
    void set_min_size(Vec2 min_sz) override { m_min_size = min_sz; }
    Vec2 max_size() const override { return m_max_size; }
    void set_max_size(Vec2 max_sz) override { m_max_size = max_sz; }

    Vec2 position() const override { return Vec2{0.0f, 0.0f}; }
    void set_position(Vec2 pos) override { (void)pos; }
    void center_on_screen() override {}

    Rect screen_bounds() const override {
        int w = 0, h = 0;
        ktui_js_get_window_size(&w, &h);
        return Rect{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)};
    }

    // ==================== 呈现状态 ====================
    void show() override { m_visible = true; }
    void hide() override { m_visible = false; }
    bool is_visible() const override { return m_visible; }

    void minimize() override { m_state = WindowState::Minimized; }
    void maximize() override { m_state = WindowState::Maximized; }
    void restore() override { m_state = WindowState::Normal; }
    bool is_minimized() const override { return m_state == WindowState::Minimized; }
    bool is_maximized() const override { return m_state == WindowState::Maximized; }

    void set_fullscreen(bool fullscreen) override {
        m_state = fullscreen ? WindowState::Fullscreen : WindowState::Normal;
    }
    bool is_fullscreen() const override { return m_state == WindowState::Fullscreen; }

    void activate() override { m_active = true; m_focused = true; }
    bool is_active() const override { return m_active; }
    void focus() override { m_focused = true; }
    bool has_focus() const override { return m_focused; }

    void set_always_on_top(bool always_on_top) override { (void)always_on_top; }
    bool is_always_on_top() const override { return false; }
    void flash_taskbar(bool flash) override { (void)flash; }

    // ==================== 外观与标题栏 ====================
    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override {
        m_title = title;
        ktui_js_set_title(title.c_str());
    }
    void set_icon(const std::string& icon_path) override { (void)icon_path; }

    void set_resizable(bool resizable) override { m_resizable = resizable; }
    bool is_resizable() const override { return m_resizable; }

    void set_opacity(float opacity) override { m_opacity = opacity; }
    float opacity() const override { return m_opacity; }

    void set_decorated(bool decorated) override { m_decorated = decorated; }
    bool is_decorated() const override { return m_decorated; }
    void set_titlebar_hit_test_callback(std::function<TitlebarHitTest(PointF)> cb) override {
        m_hit_test_cb = std::move(cb);
    }

    // ==================== 显示器与 DPI ====================
    float dpi_scale() const override {
        return ktui_js_get_dpi_scale();
    }
    float render_scale() const override { return dpi_scale(); }
    uint32_t refresh_rate() const override { return 60; }

    // ==================== 原生句柄 ====================
    void* get_compute_display() const override { return nullptr; }
    void* get_native_handle() const override {
        return const_cast<void*>(reinterpret_cast<const void*>(this));
    }

    // ==================== 帧呈现与重绘请求 ====================
    void present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) override {
        if (!rgba || width == 0 || height == 0) return;
        ktui_js_present_rgba8_region(rgba, static_cast<int>(width), static_cast<int>(height),
                                     0, 0, static_cast<int>(width), static_cast<int>(height));
    }

    void present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                              uint32_t dirty_x, uint32_t dirty_y,
                              uint32_t dirty_w, uint32_t dirty_h) override {
        if (!rgba || width == 0 || height == 0 || dirty_w == 0 || dirty_h == 0) return;
        ktui_js_present_rgba8_region(rgba, static_cast<int>(width), static_cast<int>(height),
                                     static_cast<int>(dirty_x), static_cast<int>(dirty_y),
                                     static_cast<int>(dirty_w), static_cast<int>(dirty_h));
    }

    void invalidate() override {
        // Web 端由 requestAnimationFrame (Application::step_frame) 统一按 VSync 节拍驱动重绘，
        // 避免在 DOM 输入事件回调中同步调用 render_frame(true) 导致帧外重复提交、微卡顿及 dt==0。
    }
    void on_paint_requested(std::function<void()> handler) override {
        m_paint_handler = std::move(handler);
    }

    // ==================== 框架级 IME 输入法与光标形状控制 ====================
    void set_ime_enabled(bool enabled) override {
        ktui_js_set_ime_enabled(enabled);
    }
    void set_ime_cursor_rect(Rect cursor_rect) override {
        ktui_js_set_ime_cursor_rect(cursor_rect.min_x, cursor_rect.min_y,
                                    cursor_rect.width(), cursor_rect.height());
    }
    void set_cursor(CursorShape cursor) override {
        if (m_cursor == cursor) return;
        m_cursor = cursor;
        const char* css_cursor = "default";
        switch (cursor) {
            case CursorShape::Arrow:        css_cursor = "default"; break;
            case CursorShape::IBeam:        css_cursor = "text"; break;
            case CursorShape::PointingHand: css_cursor = "pointer"; break;
            case CursorShape::ResizeEW:     css_cursor = "ew-resize"; break;
            case CursorShape::ResizeNS:     css_cursor = "ns-resize"; break;
            case CursorShape::ResizeNWSE:   css_cursor = "nwse-resize"; break;
            case CursorShape::ResizeNESW:   css_cursor = "nesw-resize"; break;
            case CursorShape::Crosshair:    css_cursor = "crosshair"; break;
            case CursorShape::Forbidden:    css_cursor = "not-allowed"; break;
            default:                        css_cursor = "default"; break;
        }
        ktui_js_set_cursor(css_cursor);
    }

    // ==================== 回调与销毁 ====================
    void on_close_requested(std::function<bool()> handler) override { m_close_handler = std::move(handler); }
    void on_resized(std::function<void(Vec2)> handler) override { m_resize_handler = std::move(handler); }
    void on_moved(std::function<void(Vec2)> handler) override { m_move_handler = std::move(handler); }
    void on_dpi_changed(std::function<void(float)> handler) override { m_dpi_handler = std::move(handler); }
    void on_focus_changed(std::function<void(bool)> handler) override { m_focus_handler = std::move(handler); }

    void close() override {
        if (m_is_closed) return;
        if (m_close_handler && !m_close_handler()) return;
        m_is_closed = true;
    }

private:
    std::string m_title;
    Vec2 m_size{800.0f, 600.0f};
    Vec2 m_min_size{100.0f, 100.0f};
    Vec2 m_max_size{16384.0f, 16384.0f};
    bool m_visible{false};
    bool m_active{true};
    bool m_focused{true};
    bool m_resizable{true};
    bool m_decorated{true};
    bool m_is_closed{false};
    float m_opacity{1.0f};
    WindowState m_state{WindowState::Normal};
    CursorShape m_cursor{CursorShape::Arrow};

    std::function<TitlebarHitTest(PointF)> m_hit_test_cb;
    std::function<void()> m_paint_handler;
    std::function<bool()> m_close_handler;
    std::function<void(Vec2)> m_resize_handler;
    std::function<void(Vec2)> m_move_handler;
    std::function<void(float)> m_dpi_handler;
    std::function<void(bool)> m_focus_handler;
};

// ==================== Web 平台抽象层统一后端 ====================
class WebBackend : public IPlatformBackend {
public:
    WebBackend() {
        s_instance = this;
    }

    ~WebBackend() override {
        shutdown();
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    static WebBackend* instance() noexcept {
        return s_instance;
    }

    bool init() override {
        if (m_initialized) return true;

        ktui_js_init_ime();

        // 注册 HTML5 鼠标与滚轮事件
        emscripten_set_mousemove_callback("#canvas", this, EM_TRUE, &on_mouse_event);
        emscripten_set_mousedown_callback("#canvas", this, EM_TRUE, &on_mouse_event);
        emscripten_set_mouseup_callback("#canvas", this, EM_TRUE, &on_mouse_event);
        emscripten_set_wheel_callback("#canvas", this, EM_TRUE, &on_wheel_event);

        // 注册 HTML5 触控事件 (移动端 Web 完美手势支持)
        emscripten_set_touchstart_callback("#canvas", this, EM_TRUE, &on_touch_event);
        emscripten_set_touchmove_callback("#canvas", this, EM_TRUE, &on_touch_event);
        emscripten_set_touchend_callback("#canvas", this, EM_TRUE, &on_touch_event);
        emscripten_set_touchcancel_callback("#canvas", this, EM_TRUE, &on_touch_event);

        // 注册 HTML5 键盘事件
        emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &on_key_down_event);
        emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &on_key_up_event);

        // 注册窗口尺寸响应
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &on_window_resize);

        m_initialized = true;
        return true;
    }

    void poll_events() override {
        m_dispatcher.process_tasks();
    }

    void wait_events(uint32_t timeout_ms = 0) override {
        (void)timeout_ms;
        poll_events();
    }

    std::unique_ptr<IPlatformWindow> create_window(const WindowDesc& desc) override {
        auto win = std::make_unique<WebWindow>(desc);
        m_active_window = win.get();
        return win;
    }

    IPlatformDispatcher& get_dispatcher() override { return m_dispatcher; }
    IVSyncSource& get_vsync_source() override { return m_vsync_source; }
    IFontResolver& get_font_resolver() override { return m_font_resolver; }

    std::string get_clipboard_text() const override {
        char buf[1024] = {0};
        ktui_js_get_clipboard(buf, sizeof(buf));
        return std::string(buf);
    }

    void set_clipboard_text(const std::string& utf8_text) override {
        ktui_js_set_clipboard(utf8_text.c_str());
    }

    void set_event_callback(std::function<void(const PlatformEvent&)> callback) override {
        m_event_callback = std::move(callback);
    }

    void shutdown() override {
        m_initialized = false;
        m_active_window = nullptr;
    }

    void dispatch_event(const PlatformEvent& pe) {
        if (m_event_callback) {
            m_event_callback(pe);
        }
    }

    void handle_ime_composition(const char* text, size_t cursor) {
        if (!m_active_window || !text) return;
        PlatformEvent pe{};
        pe.ime_cursor_byte = cursor;
        pe.type = PlatformEventType::ImeComposition;
        pe.window_handle = m_active_window ? m_active_window->get_native_handle() : nullptr;
        pe.text = text;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        dispatch_event(pe);
    }

    void handle_ime_commit(const char* text) {
        if (!m_active_window || !text || text[0] == '\0') return;
        PlatformEvent pe{};
        pe.type = PlatformEventType::ImeCommit;
        pe.window_handle = m_active_window ? m_active_window->get_native_handle() : nullptr;
        pe.text = text;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        dispatch_event(pe);
    }

private:
    static EM_BOOL on_mouse_event(int event_type, const EmscriptenMouseEvent* e, void* user_data) {
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_active_window) return EM_FALSE;

        PlatformEvent pe{};
        pe.window_handle = self->m_active_window->get_native_handle();
        pe.position = Vec2{static_cast<float>(e->targetX), static_cast<float>(e->targetY)};
        pe.modifiers.shift = e->shiftKey != 0;
        pe.modifiers.ctrl  = e->ctrlKey != 0;
        pe.modifiers.alt   = e->altKey != 0;
        pe.modifiers.meta  = e->metaKey != 0;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());

        if (e->button == 0) pe.button = PointerButton::Primary;
        else if (e->button == 2) pe.button = PointerButton::Secondary;
        else if (e->button == 1) pe.button = PointerButton::Auxiliary;

        if (event_type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
            pe.type = PlatformEventType::PointerMove;
        } else if (event_type == EMSCRIPTEN_EVENT_MOUSEDOWN) {
            pe.type = PlatformEventType::PointerDown;
        } else if (event_type == EMSCRIPTEN_EVENT_MOUSEUP) {
            pe.type = PlatformEventType::PointerUp;
        }

        self->dispatch_event(pe);
        return EM_TRUE;
    }

    static EM_BOOL on_wheel_event(int event_type, const EmscriptenWheelEvent* e, void* user_data) {
        (void)event_type;
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_active_window) return EM_FALSE;

        PlatformEvent pe{};
        pe.type = PlatformEventType::PointerScroll;
        pe.window_handle = self->m_active_window->get_native_handle();
        pe.position = Vec2{static_cast<float>(e->mouse.targetX), static_cast<float>(e->mouse.targetY)};
        pe.delta = Vec2{static_cast<float>(e->deltaX), static_cast<float>(e->deltaY)};
        pe.modifiers.shift = e->mouse.shiftKey != 0;
        pe.modifiers.ctrl  = e->mouse.ctrlKey != 0;
        pe.modifiers.alt   = e->mouse.altKey != 0;
        pe.modifiers.meta  = e->mouse.metaKey != 0;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());

        self->dispatch_event(pe);
        return EM_TRUE; // 阻止浏览器网页原生滚动
    }

    static EM_BOOL on_touch_event(int event_type, const EmscriptenTouchEvent* e, void* user_data) {
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_active_window || e->numTouches == 0) return EM_FALSE;

        const auto& t = e->touches[0];
        PlatformEvent pe{};
        pe.window_handle = self->m_active_window->get_native_handle();
        pe.position = Vec2{static_cast<float>(t.targetX), static_cast<float>(t.targetY)};
        pe.button = PointerButton::Primary;
        pe.modifiers.shift = e->shiftKey != 0;
        pe.modifiers.ctrl  = e->ctrlKey != 0;
        pe.modifiers.alt   = e->altKey != 0;
        pe.modifiers.meta  = e->metaKey != 0;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());

        if (event_type == EMSCRIPTEN_EVENT_TOUCHSTART) {
            pe.type = PlatformEventType::PointerDown;
        } else if (event_type == EMSCRIPTEN_EVENT_TOUCHMOVE) {
            pe.type = PlatformEventType::PointerMove;
        } else if (event_type == EMSCRIPTEN_EVENT_TOUCHEND || event_type == EMSCRIPTEN_EVENT_TOUCHCANCEL) {
            pe.type = PlatformEventType::PointerUp;
        }

        self->dispatch_event(pe);
        return EM_TRUE;
    }

    static EM_BOOL on_key_down_event(int event_type, const EmscriptenKeyboardEvent* e, void* user_data) {
        (void)event_type;
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_active_window) return EM_FALSE;

        const int target = ktui_js_key_target();
        if (target == 0 || e->keyCode == 229) return EM_FALSE;

        KeyModifiers mods{};
        mods.shift = e->shiftKey != 0;
        mods.ctrl  = e->ctrlKey != 0 || e->metaKey != 0;
        mods.alt   = e->altKey != 0;
        mods.meta  = e->metaKey != 0;

        uint32_t vk = e->keyCode;
        // The browser inserts pasted text through the focused input's input event.
        if (target == 2 && (mods.ctrl || mods.meta) && !mods.alt && vk == 'V') return EM_FALSE;

        // 1. 发送 KeyDown 事件驱动按键逻辑 (退格、删除、回车、光标移动及快捷键)
        PlatformEvent pe_down{};
        pe_down.type = PlatformEventType::KeyDown;
        pe_down.window_handle = self->m_active_window->get_native_handle();
        pe_down.key_code = vk;
        pe_down.modifiers = mods;
        pe_down.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        self->dispatch_event(pe_down);

        if (target == 2 && (mods.ctrl || mods.meta) && !mods.alt &&
            (vk == 'A' || vk == 'C' || vk == 'X' || vk == 'Y' || vk == 'Z')) return EM_TRUE;
        if (vk == 8 || vk == 9 || vk == 13 || vk == 46 || (vk >= 35 && vk <= 40)) {
            return EM_TRUE;
        }
        return EM_FALSE;
    }

    static EM_BOOL on_key_up_event(int event_type, const EmscriptenKeyboardEvent* e, void* user_data) {
        (void)event_type;
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_active_window) return EM_FALSE;

        if (!ktui_js_key_target() || e->keyCode == 229) return EM_FALSE;

        PlatformEvent pe{};
        pe.type = PlatformEventType::KeyUp;
        pe.window_handle = self->m_active_window->get_native_handle();
        pe.key_code = e->keyCode;
        pe.modifiers.shift = e->shiftKey != 0;
        pe.modifiers.ctrl  = e->ctrlKey != 0;
        pe.modifiers.alt   = e->altKey != 0;
        pe.modifiers.meta  = e->metaKey != 0;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());

        self->dispatch_event(pe);
        return EM_TRUE;
    }

    static EM_BOOL on_window_resize(int event_type, const EmscriptenUiEvent* e, void* user_data) {
        (void)event_type; (void)e;
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_active_window) return EM_FALSE;

        PlatformEvent pe{};
        pe.type = PlatformEventType::WindowResize;
        pe.window_handle = self->m_active_window->get_native_handle();
        pe.position = self->m_active_window->size();
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());

        self->dispatch_event(pe);
        return EM_TRUE;
    }

    inline static WebBackend* s_instance = nullptr;

    bool m_initialized{false};
    WebDispatcher m_dispatcher;
    WebVSyncSource m_vsync_source;
    WebFontResolver m_font_resolver;
    WebWindow* m_active_window{nullptr};
    std::function<void(const PlatformEvent&)> m_event_callback;
};

// C 函数桥接实现
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_ime_composition(const char* text, size_t cursor) {
        if (WebBackend::instance()) {
            WebBackend::instance()->handle_ime_composition(text, cursor);
        }
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_ime_commit(const char* text) {
        if (WebBackend::instance()) {
            WebBackend::instance()->handle_ime_commit(text);
        }
    }
}

} // namespace ktui

#endif // __EMSCRIPTEN__
