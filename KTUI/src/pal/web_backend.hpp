#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>

namespace ktui {

class WebBackend;
class WebWindow;

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

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_focus_window(int win_id);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_pointer(int win_id, int type, float x, float y, int button, int mods);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_wheel(int win_id, float x, float y, float dx, float dy, int mods);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_file_drop(int win_id, float x, float y, const char* files_joined);

    EMSCRIPTEN_KEEPALIVE
    void ktui_web_on_viewport_resize();
}

// ==================== Web 全屏画板 DOM 桥接 ====================
EM_JS(void, ktui_js_init_stage, (), {
    var legacy = document.getElementById('canvas');
    if (legacy && !legacy.dataset.ktuiWinId) {
        legacy.style.display = 'none';
    }

    if (!document.getElementById('ktui-web-stage-style')) {
        var style = document.createElement('style');
        style.id = 'ktui-web-stage-style';
        style.textContent =
            'html,body,#ktui-stage,#ktui-stage canvas{' +
                'margin:0;padding:0;width:100%;height:100%;overflow:hidden;' +
                '-webkit-tap-highlight-color:transparent;tap-highlight-color:transparent;' +
                '-webkit-touch-callout:none;user-select:none;-webkit-user-select:none;}' +
            '#ktui-stage{position:fixed;inset:0;overflow:hidden;touch-action:none;}' +
            '#ktui-stage canvas{position:absolute;inset:0;display:block;width:100%;height:100%;outline:none;touch-action:none;}';
        document.head.appendChild(style);
    }

    var stage = document.getElementById('ktui-stage');
    if (!stage) {
        stage = document.createElement('div');
        stage.id = 'ktui-stage';
        document.body.appendChild(stage);
    }

    if (!Module['ktuiWindows']) Module['ktuiWindows'] = new Map();
    if (!Module['ktuiCanvases']) Module['ktuiCanvases'] = new Map();

    window._ktuiViewportCbs = window._ktuiViewportCbs || [];
    window._ktuiViewportCbs.push(function() { _ktui_web_on_viewport_resize(); });

    if (!window._ktuiViewportListenersInstalled) {
        window._ktuiViewportListenersInstalled = true;
        window.addEventListener('resize', function() {
            var cbs = window._ktuiViewportCbs || [];
            for (var i = 0; i < cbs.length; ++i) {
                try { cbs[i](); } catch (_) {}
            }
        });

        var watchDpr = function() {
            if (!window.matchMedia) return;
            var dpr = window.devicePixelRatio || 1;
            var mq = window.matchMedia('(resolution: ' + dpr + 'dppx)');
            var onDprChange = function() {
                var cbs = window._ktuiViewportCbs || [];
                for (var i = 0; i < cbs.length; ++i) {
                    try { cbs[i](); } catch (_) {}
                }
                watchDpr();
            };
            if (mq.addEventListener) mq.addEventListener('change', onDprChange, { once: true });
            else if (mq.addListener) mq.addListener(onDprChange);
        };
        watchDpr();
    }

    // 共享隐藏 IME 输入框
    if (!document.getElementById('ktui_ime_input')) {
        var input = document.createElement('input');
        input.id = 'ktui_ime_input';
        input.type = 'text';
        input.tabIndex = -1;
        input.disabled = true;
        input.autocomplete = 'off';
        input.spellcheck = false;
        input.style.cssText = 'position:fixed;opacity:0;pointer-events:none;width:1px;height:1px;z-index:999999;';
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
    }
});

EM_JS(void, ktui_js_create_canvas, (int win_id, const char* title), {
    var stage = document.getElementById('ktui-stage');
    if (!stage) return;

    var canvas = document.createElement('canvas');
    canvas.id = 'ktui-win-' + win_id;
    canvas.dataset.ktuiWinId = String(win_id);
    canvas.tabIndex = 0;
    canvas.style.cssText = 'position:absolute;inset:0;display:block;width:100%;height:100%;outline:none;touch-action:none;';
    stage.appendChild(canvas);

    var titleStr = UTF8ToString(title);
    if (titleStr) document.title = titleStr;

    var entry = {
        canvas: canvas,
        imgData: null,
        visible: false
    };
    Module['ktuiWindows'].set(win_id, entry);
    Module['ktuiCanvases'].set(win_id, canvas);

    var loading = document.getElementById('ktui-loading');
    if (loading) loading.style.display = 'none';

    var getMods = function(e) {
        return (e.shiftKey ? 1 : 0) | (e.ctrlKey ? 2 : 0) | (e.altKey ? 4 : 0) | (e.metaKey ? 8 : 0);
    };
    var getLocal = function(e) {
        var r = canvas.getBoundingClientRect();
        return [e.clientX - r.left, e.clientY - r.top];
    };

    canvas.addEventListener('pointerdown', function(e) {
        _ktui_web_on_focus_window(win_id);
        var pt = getLocal(e);
        var btn = e.button === 0 ? 1 : (e.button === 2 ? 2 : (e.button === 1 ? 3 : 0));
        var touchFlag = (e.pointerType === 'touch') ? 16 : 0;
        try { canvas.setPointerCapture(e.pointerId); } catch (_) {}
        _ktui_web_on_pointer(win_id, 1, pt[0], pt[1], btn, getMods(e) | touchFlag);
    });

    canvas.addEventListener('pointermove', function(e) {
        var pt = getLocal(e);
        var touchFlag = (e.pointerType === 'touch') ? 16 : 0;
        _ktui_web_on_pointer(win_id, 0, pt[0], pt[1], 0, getMods(e) | touchFlag);
    });

    var onPointerEnd = function(e) {
        var pt = getLocal(e);
        var btn = e.button === 0 ? 1 : (e.button === 2 ? 2 : (e.button === 1 ? 3 : 0));
        var touchFlag = (e.pointerType === 'touch') ? 16 : 0;
        _ktui_web_on_pointer(win_id, 2, pt[0], pt[1], btn, getMods(e) | touchFlag);
    };
    canvas.addEventListener('pointerup', onPointerEnd);
    canvas.addEventListener('pointercancel', onPointerEnd);

    canvas.addEventListener('wheel', function(e) {
        e.preventDefault();
        var pt = getLocal(e);
        _ktui_web_on_wheel(win_id, pt[0], pt[1], e.deltaX, e.deltaY, getMods(e));
    }, { passive: false });

    canvas.addEventListener('contextmenu', function(e) { e.preventDefault(); });

    canvas.addEventListener('dragover', function(e) {
        e.preventDefault();
        if (e.dataTransfer) e.dataTransfer.dropEffect = 'copy';
    });
    canvas.addEventListener('drop', function(e) {
        e.preventDefault();
        var files = e.dataTransfer && e.dataTransfer.files;
        if (!files || files.length === 0) return;
        var names = [];
        for (var fi = 0; fi < files.length; ++fi) names.push(files[fi].name);
        var joined = names.join('\n');
        var len = lengthBytesUTF8(joined) + 1;
        var ptr = _malloc(len);
        stringToUTF8(joined, ptr, len);
        var pt = getLocal(e);
        _ktui_web_on_file_drop(win_id, pt[0], pt[1], ptr);
        _free(ptr);
    });
});

EM_JS(void, ktui_js_set_canvas_visible, (int win_id, int visible), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (!entry || !entry.canvas) return;
    var vis = visible !== 0;
    entry.visible = vis;
    entry.canvas.style.display = vis ? 'block' : 'none';
    entry.canvas.style.pointerEvents = vis ? 'auto' : 'none';
});

EM_JS(void, ktui_js_set_window_title, (int win_id, const char* title), {
    var str = UTF8ToString(title);
    if (str) document.title = str;
});

EM_JS(void, ktui_js_focus_window_canvas, (int win_id), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (entry && entry.canvas) {
        var ime = document.getElementById('ktui_ime_input');
        if (!ime || document.activeElement !== ime || ime.disabled) {
            entry.canvas.focus({ preventScroll: true });
        }
    }
});

EM_JS(void, ktui_js_destroy_canvas, (int win_id), {
    if (Module['ktuiWindows']) {
        var entry = Module['ktuiWindows'].get(win_id);
        if (entry && entry.canvas) entry.canvas.remove();
        Module['ktuiWindows'].delete(win_id);
    }
    if (Module['ktuiCanvases']) Module['ktuiCanvases'].delete(win_id);
});

EM_JS(int, ktui_js_key_target, (), {
    var target = document.activeElement;
    if (!target) return 1;
    if (target.dataset && target.dataset.ktuiWinId) return 1;
    if (target.id === 'ktui_ime_input') {
        if (target.disabled || target.ktuiIsComposing()) return 0;
        return 2;
    }
    return 1;
});

EM_JS(void, ktui_js_present_rgba8_region, (int win_id, const uint8_t* rgba, int width, int height,
                                           int dirty_x, int dirty_y, int dirty_w, int dirty_h), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (!entry || !entry.canvas) return;
    var canvas = entry.canvas;
    var dpr = Math.max(1, window.devicePixelRatio || 1);
    var cssW = (width / dpr) + 'px';
    var cssH = (height / dpr) + 'px';
    if (canvas.style.width !== cssW) canvas.style.width = cssW;
    if (canvas.style.height !== cssH) canvas.style.height = cssH;
    var resized = (canvas.width !== width || canvas.height !== height);
    if (resized) {
        canvas.width = width;
        canvas.height = height;
    }
    var ctx = canvas.getContext('2d');
    if (!ctx) return;

    var recreated = !entry.imgData || entry.imgData.width !== width || entry.imgData.height !== height;
    if (recreated) {
        entry.imgData = ctx.createImageData(width, height);
    }
    var data = entry.imgData.data;

    var rgbaPtr = rgba >>> 0;
    if (resized || recreated || (dirty_x === 0 && dirty_y === 0 && dirty_w >= width && dirty_h >= height)) {
        var src = HEAPU8.subarray(rgbaPtr, rgbaPtr + width * height * 4);
        data.set(src);
        ctx.putImageData(entry.imgData, 0, 0);
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
        data.set(HEAPU8.subarray(rgbaPtr + rowOffset, rgbaPtr + rowOffset + copyBytes), rowOffset);
    }
    ctx.putImageData(entry.imgData, 0, 0, x0, y0, rw, rh);
});

EM_JS(void, ktui_js_set_cursor, (int win_id, const char* cursor_style), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    if (entry && entry.canvas) {
        entry.canvas.style.cursor = UTF8ToString(cursor_style);
    }
});

EM_JS(void, ktui_js_set_ime_enabled, (int win_id, bool enabled), {
    var input = document.getElementById('ktui_ime_input');
    if (!input) return;
    if (enabled) {
        input.disabled = false;
        if (document.activeElement !== input) input.focus({preventScroll: true});
    } else if (!input.disabled) {
        var wasFocused = document.activeElement === input;
        input.disabled = true;
        input.value = '';
        if (wasFocused) {
            var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
            if (entry && entry.canvas) entry.canvas.focus({preventScroll: true});
        }
    }
});

EM_JS(void, ktui_js_set_ime_cursor_rect, (int win_id, float x, float y, float w, float h), {
    var entry = Module['ktuiWindows'] && Module['ktuiWindows'].get(win_id);
    var input = document.getElementById('ktui_ime_input');
    if (!entry || !entry.canvas || !input) return;
    var rect = entry.canvas.getBoundingClientRect();
    input.style.left = (rect.left + x) + 'px';
    input.style.top = (rect.top + y + h) + 'px';
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
    HEAP32[out_w >> 2] = Math.max(1, window.innerWidth | 0);
    HEAP32[out_h >> 2] = Math.max(1, window.innerHeight | 0);
});

EM_JS(void, ktui_js_wake_frame, (), {
    if (window._ktuiRequestFrame) window._ktuiRequestFrame();
});

// ==================== Web 平台任务调度器 ====================
class WebDispatcher : public IPlatformDispatcher {
public:
    void post_task(Fn<void()> task) override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(std::move(task));
        }
        ktui_js_wake_frame();
    }

    size_t process_tasks() override {
        std::vector<Fn<void()>> to_run;
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
    std::vector<Fn<void()>> m_queue;
};

// ==================== Web 平台虚拟帧时钟源 ====================
class WebVSyncSource : public IVSyncSource {
public:
    uint64_t request_frame(Fn<void(double timestamp_seconds)> callback) override {
        uint64_t id = m_next_id++;
        m_requests.push_back({id, std::move(callback)});
        ktui_js_wake_frame();
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
        Fn<void(double)> callback;
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

// ==================== Web 全屏单画板窗口实现 ====================
class WebWindow : public IPlatformWindow {
public:
    WebWindow(int win_id, const WindowDesc& desc,
              Fn<void(WebWindow*)> on_destroyed)
        : m_win_id(win_id),
          m_title(desc.title),
          m_on_destroyed(std::move(on_destroyed))
    {
        m_last_dpi_scale = ktui_js_get_dpi_scale();
        Rect sb = screen_bounds();
        m_size = Vec2{std::max(1.0f, sb.width()), std::max(1.0f, sb.height())};

        ktui_js_create_canvas(m_win_id, m_title.c_str());
        sync_visibility();
    }

    ~WebWindow() override {
        close();
        if (m_on_destroyed) m_on_destroyed(this);
        ktui_js_destroy_canvas(m_win_id);
    }

    [[nodiscard]] int win_id() const noexcept { return m_win_id; }

    // ==================== 几何与尺寸 (直接绑定浏览器视口) ====================
    Vec2 size() const override { return m_size; }
    void set_size(Vec2 new_size) override { (void)new_size; }
    Vec2 min_size() const override { return m_size; }
    void set_min_size(Vec2 min_sz) override { (void)min_sz; }
    Vec2 max_size() const override { return m_size; }
    void set_max_size(Vec2 max_sz) override { (void)max_sz; }

    Vec2 position() const override { return Vec2{0.0f, 0.0f}; }
    void set_position(Vec2 pos) override { (void)pos; }
    void center_on_screen() override {}

    Rect screen_bounds() const override {
        int w = 0, h = 0;
        ktui_js_get_window_size(&w, &h);
        return Rect{0.0f, 0.0f, static_cast<float>(std::max(1, w)), static_cast<float>(std::max(1, h))};
    }

    Rect monitor_uv_rect() const override {
        return Rect{0.0f, 0.0f, 1.0f, 1.0f};
    }

    // ==================== 呈现状态 ====================
    void show() override {
        m_visible = true;
        activate();
        sync_visibility();
    }
    void hide() override {
        m_visible = false;
        sync_visibility();
    }
    bool is_visible() const override { return m_visible && !m_is_closed; }

    void minimize() override {}
    void maximize() override {}
    void restore() override {}
    bool is_minimized() const override { return false; }
    bool is_maximized() const override { return false; }
    void set_fullscreen(bool fullscreen) override { (void)fullscreen; }
    bool is_fullscreen() const override { return true; }

    void activate() override {
        set_active_internal(true);
    }
    bool is_active() const override { return m_active; }
    void focus() override { activate(); }
    bool has_focus() const override { return m_focused; }

    void set_always_on_top(bool always_on_top) override { (void)always_on_top; }
    bool is_always_on_top() const override { return false; }
    void flash_taskbar(bool flash) override { (void)flash; }

    // ==================== 外观 ====================
    std::string title() const override { return m_title; }
    void set_title(const std::string& title) override {
        m_title = title;
        ktui_js_set_window_title(m_win_id, m_title.c_str());
    }
    void set_icon(const std::string& icon_path) override { (void)icon_path; }

    void set_resizable(bool resizable) override { (void)resizable; }
    bool is_resizable() const override { return false; }

    void set_opacity(float opacity) override { m_opacity = opacity; }
    float opacity() const override { return m_opacity; }

    void set_decorated(bool decorated) override { (void)decorated; }
    bool is_decorated() const override { return true; }
    void set_titlebar_hit_test_callback(Fn<TitlebarHitTest(PointF)> cb) override { (void)cb; }

    // ==================== 显示器与 DPI ====================
    float dpi_scale() const override { return m_last_dpi_scale; }
    float render_scale() const override { return m_last_dpi_scale; }
    uint32_t refresh_rate() const override { return 60; }

    // ==================== 原生句柄 ====================
    void* get_compute_display() const override { return nullptr; }
    void* get_native_handle() const override {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(m_win_id));
    }

    // ==================== 帧呈现与重绘请求 ====================
    void present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) override {
        if (!rgba || width == 0 || height == 0) return;
        ktui_js_present_rgba8_region(m_win_id, rgba, static_cast<int>(width), static_cast<int>(height),
                                     0, 0, static_cast<int>(width), static_cast<int>(height));
    }

    void present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                              uint32_t dirty_x, uint32_t dirty_y,
                              uint32_t dirty_w, uint32_t dirty_h) override {
        if (!rgba || width == 0 || height == 0 || dirty_w == 0 || dirty_h == 0) return;
        ktui_js_present_rgba8_region(m_win_id, rgba, static_cast<int>(width), static_cast<int>(height),
                                     static_cast<int>(dirty_x), static_cast<int>(dirty_y),
                                     static_cast<int>(dirty_w), static_cast<int>(dirty_h));
    }

    void invalidate() override {
        ktui_js_wake_frame();
    }
    void on_paint_requested(Fn<void()> handler) override {
        m_paint_handler = std::move(handler);
    }

    // ==================== IME 输入法与光标形状 ====================
    void set_ime_enabled(bool enabled) override {
        ktui_js_set_ime_enabled(m_win_id, enabled);
    }
    void set_ime_cursor_rect(Rect cursor_rect) override {
        ktui_js_set_ime_cursor_rect(m_win_id, cursor_rect.min_x, cursor_rect.min_y,
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
        ktui_js_set_cursor(m_win_id, css_cursor);
    }

    // ==================== 回调与销毁 ====================
    void on_close_requested(Fn<bool()> handler) override { m_close_handler = std::move(handler); }
    void on_resized(Fn<void(Vec2)> handler) override { m_resize_handler = std::move(handler); }
    void on_moved(Fn<void(Vec2)> handler) override { m_move_handler = std::move(handler); }
    void on_dpi_changed(Fn<void(float)> handler) override { m_dpi_handler = std::move(handler); }
    void on_focus_changed(Fn<void(bool)> handler) override { m_focus_handler = std::move(handler); }

    void close() override {
        if (m_is_closed) return;
        if (m_close_handler && !m_close_handler()) return;
        m_is_closed = true;
        m_visible = false;
        sync_visibility();
    }

    void set_active_internal(bool active) {
        bool changed = (m_active != active || m_focused != active);
        m_active = active;
        m_focused = active;
        if (active) {
            ktui_js_focus_window_canvas(m_win_id);
        }
        if (changed && m_focus_handler) {
            m_focus_handler(active);
        }
    }

    void handle_viewport_resized() {
        const float cur_dpi = ktui_js_get_dpi_scale();
        if (cur_dpi != m_last_dpi_scale) {
            m_last_dpi_scale = cur_dpi;
            if (m_dpi_handler) m_dpi_handler(cur_dpi);
            ktui_js_wake_frame();
        }
        Rect sb = screen_bounds();
        Vec2 new_sz{std::max(1.0f, sb.width()), std::max(1.0f, sb.height())};
        if (new_sz.x != m_size.x || new_sz.y != m_size.y) {
            m_size = new_sz;
            if (m_resize_handler) m_resize_handler(m_size);
            ktui_js_wake_frame();
        }
    }

private:
    void sync_visibility() {
        int vis = (m_visible && !m_is_closed) ? 1 : 0;
        ktui_js_set_canvas_visible(m_win_id, vis);
        if (vis) ktui_js_wake_frame();
    }

    int m_win_id{0};
    std::string m_title;
    Vec2 m_size{800.0f, 600.0f};
    bool m_visible{false};
    bool m_active{false};
    bool m_focused{false};
    bool m_is_closed{false};
    float m_opacity{1.0f};
    float m_last_dpi_scale{1.0f};
    CursorShape m_cursor{CursorShape::Arrow};

    Fn<void(WebWindow*)> m_on_destroyed;
    Fn<void()> m_paint_handler;
    Fn<bool()> m_close_handler;
    Fn<void(Vec2)> m_resize_handler;
    Fn<void(Vec2)> m_move_handler;
    Fn<void(float)> m_dpi_handler;
    Fn<void(bool)> m_focus_handler;
};

// ==================== Web 平台抽象层统一后端 (全屏单画板) ====================
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

        ktui_js_init_stage();

        emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &on_key_down_event);
        emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, &on_key_up_event);

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
        if (!m_initialized) init();
        // Web 全屏单画板模式：仅创建主画板，忽略额外子窗口请求
        if (!m_windows.empty()) return nullptr;
        int win_id = m_next_win_id++;
        auto win = std::make_unique<WebWindow>(
            win_id, desc,
            [this](WebWindow* w) { unregister_window(w); }
        );
        m_windows.push_back(win.get());
        m_focused_window = win.get();
        win->set_active_internal(true);
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

    void set_event_callback(Fn<void(const PlatformEvent&)> callback) override {
        m_event_callback = std::move(callback);
    }

    void shutdown() override {
        m_initialized = false;
        m_windows.clear();
        m_focused_window = nullptr;
    }

    void dispatch_event(const PlatformEvent& pe) {
        if (m_event_callback) {
            m_event_callback(pe);
        }
        ktui_js_wake_frame();
    }

    WebWindow* find_window(int win_id) const {
        for (auto* w : m_windows) {
            if (w && w->win_id() == win_id) return w;
        }
        return nullptr;
    }

    void handle_viewport_resize() {
        for (auto* w : m_windows) {
            if (w) w->handle_viewport_resized();
        }
    }

    void handle_ime_composition(const char* text, size_t cursor) {
        if (!m_focused_window || !text) return;
        PlatformEvent pe{};
        pe.ime_cursor_byte = cursor;
        pe.type = PlatformEventType::ImeComposition;
        pe.window_handle = m_focused_window->get_native_handle();
        pe.text = text;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        dispatch_event(pe);
    }

    void handle_ime_commit(const char* text) {
        if (!m_focused_window || !text || text[0] == '\0') return;
        PlatformEvent pe{};
        pe.type = PlatformEventType::ImeCommit;
        pe.window_handle = m_focused_window->get_native_handle();
        pe.text = text;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        dispatch_event(pe);
    }

    void handle_pointer(int win_id, int type, float x, float y, int button, int mods) {
        WebWindow* w = find_window(win_id);
        if (!w) return;
        PlatformEvent pe{};
        pe.window_handle = w->get_native_handle();
        pe.position = Vec2{x, y};
        pe.modifiers.shift = (mods & 1) != 0;
        pe.modifiers.ctrl  = (mods & 2) != 0;
        pe.modifiers.alt   = (mods & 4) != 0;
        pe.modifiers.meta  = (mods & 8) != 0;
        pe.device_kind = (mods & 16) ? PointerDeviceKind::Touch : PointerDeviceKind::Mouse;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        if (button == 1) pe.button = PointerButton::Primary;
        else if (button == 2) pe.button = PointerButton::Secondary;
        else if (button == 3) pe.button = PointerButton::Auxiliary;
        if (type == 0) pe.type = PlatformEventType::PointerMove;
        else if (type == 1) pe.type = PlatformEventType::PointerDown;
        else if (type == 2) pe.type = PlatformEventType::PointerUp;
        dispatch_event(pe);
    }

    void handle_wheel(int win_id, float x, float y, float dx, float dy, int mods) {
        WebWindow* w = find_window(win_id);
        if (!w) return;
        PlatformEvent pe{};
        pe.type = PlatformEventType::PointerScroll;
        pe.window_handle = w->get_native_handle();
        pe.position = Vec2{x, y};
        pe.delta = Vec2{dx, dy};
        pe.modifiers.shift = (mods & 1) != 0;
        pe.modifiers.ctrl  = (mods & 2) != 0;
        pe.modifiers.alt   = (mods & 4) != 0;
        pe.modifiers.meta  = (mods & 8) != 0;
        pe.device_kind = (mods & 16) ? PointerDeviceKind::Touch : PointerDeviceKind::Mouse;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        dispatch_event(pe);
    }

    void handle_file_drop(int win_id, float x, float y, const char* files_joined) {
        WebWindow* w = find_window(win_id);
        if (!w || !files_joined || !*files_joined) return;
        PlatformEvent pe{};
        pe.type = PlatformEventType::FileDrop;
        pe.window_handle = w->get_native_handle();
        pe.position = Vec2{x, y};
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());
        std::string s(files_joined);
        size_t pos = 0;
        while (pos < s.size()) {
            size_t nl = s.find('\n', pos);
            if (nl == std::string::npos) {
                pe.dropped_files.push_back(s.substr(pos));
                break;
            }
            pe.dropped_files.push_back(s.substr(pos, nl - pos));
            pos = nl + 1;
        }
        if (!pe.dropped_files.empty()) {
            dispatch_event(pe);
        }
    }

private:
    void unregister_window(WebWindow* w) {
        std::erase(m_windows, w);
        if (m_focused_window == w) {
            m_focused_window = m_windows.empty() ? nullptr : m_windows.front();
        }
    }

    static EM_BOOL on_key_down_event(int event_type, const EmscriptenKeyboardEvent* e, void* user_data) {
        (void)event_type;
        auto* self = static_cast<WebBackend*>(user_data);
        if (!self || !self->m_focused_window) return EM_FALSE;

        const int target = ktui_js_key_target();
        if (target == 0 || e->keyCode == 229) return EM_FALSE;

        KeyModifiers mods{};
        mods.shift = e->shiftKey != 0;
        mods.ctrl  = e->ctrlKey != 0 || e->metaKey != 0;
        mods.alt   = e->altKey != 0;
        mods.meta  = e->metaKey != 0;

        uint32_t vk = e->keyCode;
        if (target == 2 && (mods.ctrl || mods.meta) && !mods.alt && vk == 'V') return EM_FALSE;

        PlatformEvent pe_down{};
        pe_down.type = PlatformEventType::KeyDown;
        pe_down.window_handle = self->m_focused_window->get_native_handle();
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
        if (!self || !self->m_focused_window) return EM_FALSE;

        if (!ktui_js_key_target() || e->keyCode == 229) return EM_FALSE;

        PlatformEvent pe{};
        pe.type = PlatformEventType::KeyUp;
        pe.window_handle = self->m_focused_window->get_native_handle();
        pe.key_code = e->keyCode;
        pe.modifiers.shift = e->shiftKey != 0;
        pe.modifiers.ctrl  = e->ctrlKey != 0;
        pe.modifiers.alt   = e->altKey != 0;
        pe.modifiers.meta  = e->metaKey != 0;
        pe.timestamp_ms = static_cast<uint64_t>(emscripten_get_now());

        self->dispatch_event(pe);
        return EM_TRUE;
    }

    inline static WebBackend* s_instance = nullptr;

    bool m_initialized{false};
    int m_next_win_id{1};
    WebDispatcher m_dispatcher;
    WebVSyncSource m_vsync_source;
    WebFontResolver m_font_resolver;
    std::vector<WebWindow*> m_windows;
    WebWindow* m_focused_window{nullptr};
    Fn<void(const PlatformEvent&)> m_event_callback;
};

// C 函数桥接实现
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_ime_composition(const char* text, size_t cursor) {
        if (WebBackend::instance()) WebBackend::instance()->handle_ime_composition(text, cursor);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_ime_commit(const char* text) {
        if (WebBackend::instance()) WebBackend::instance()->handle_ime_commit(text);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_focus_window(int win_id) {
        if (WebBackend::instance()) {
            if (auto* w = WebBackend::instance()->find_window(win_id)) {
                w->set_active_internal(true);
            }
        }
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_pointer(int win_id, int type, float x, float y, int button, int mods) {
        if (WebBackend::instance()) WebBackend::instance()->handle_pointer(win_id, type, x, y, button, mods);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_wheel(int win_id, float x, float y, float dx, float dy, int mods) {
        if (WebBackend::instance()) WebBackend::instance()->handle_wheel(win_id, x, y, dx, dy, mods);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_file_drop(int win_id, float x, float y, const char* files_joined) {
        if (WebBackend::instance()) WebBackend::instance()->handle_file_drop(win_id, x, y, files_joined);
    }

    EMSCRIPTEN_KEEPALIVE
    inline void ktui_web_on_viewport_resize() {
        if (WebBackend::instance()) WebBackend::instance()->handle_viewport_resize();
    }
}

} // namespace ktui

#endif // __EMSCRIPTEN__
