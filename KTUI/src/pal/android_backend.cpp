#if defined(__ANDROID__)

#include "android_backend.hpp"
#include <android/keycodes.h>
#include <android/log.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>

namespace ktui {

namespace {

std::atomic<JavaVM*> g_pal_jvm{nullptr};
jclass g_pal_bridge_class{nullptr};
jmethodID g_set_ime_method{nullptr};
jmethodID g_get_clip_method{nullptr};
jmethodID g_set_clip_method{nullptr};

JNIEnv* get_pal_env(bool& did_attach) {
    did_attach = false;
    JavaVM* vm = g_pal_jvm.load(std::memory_order_acquire);
    if (!vm) return nullptr;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
        return env;
    }
    if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
        did_attach = true;
        return env;
    }
    return nullptr;
}

void release_pal_env(bool did_attach) {
    if (did_attach) {
        if (JavaVM* vm = g_pal_jvm.load(std::memory_order_acquire)) {
            vm->DetachCurrentThread();
        }
    }
}

uint64_t now_ms() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

uint32_t map_android_keycode(int keycode) {
    switch (keycode) {
        case AKEYCODE_DEL:         return 8;   // Backspace
        case AKEYCODE_TAB:         return 9;   // Tab
        case AKEYCODE_ENTER:
        case AKEYCODE_NUMPAD_ENTER:return 13;  // Enter
        case AKEYCODE_ESCAPE:      return 27;  // Escape
        case AKEYCODE_MOVE_END:    return 35;  // End
        case AKEYCODE_MOVE_HOME:   return 36;  // Home
        case AKEYCODE_DPAD_LEFT:   return 37;  // Left
        case AKEYCODE_DPAD_UP:     return 38;  // Up
        case AKEYCODE_DPAD_RIGHT:  return 39;  // Right
        case AKEYCODE_DPAD_DOWN:   return 40;  // Down
        case AKEYCODE_FORWARD_DEL: return 46;  // Delete
        default:
            if (keycode >= AKEYCODE_A && keycode <= AKEYCODE_Z) {
                return static_cast<uint32_t>('A' + (keycode - AKEYCODE_A));
            }
            if (keycode >= AKEYCODE_0 && keycode <= AKEYCODE_9) {
                return static_cast<uint32_t>('0' + (keycode - AKEYCODE_0));
            }
            return static_cast<uint32_t>(keycode);
    }
}

} // namespace

AndroidBackend* AndroidBackend::s_instance = nullptr;

// ==================== AndroidWindow ====================

AndroidWindow::AndroidWindow(const WindowDesc& desc, AndroidBackend* backend)
    : m_backend(backend),
      m_title(desc.title),
      m_size(desc.size),
      m_min_size(desc.min_size),
      m_max_size(desc.max_size),
      m_position(Vec2{0.0f, 0.0f}),
      m_always_on_top(desc.always_on_top),
      m_resizable(false),
      m_decorated(desc.decorated) {
    if (m_backend && m_backend->current_surface()) {
        bind_surface(m_backend->current_surface(),
                     m_backend->surface_width(),
                     m_backend->surface_height(),
                     m_backend->display_density());
    } else if (m_backend) {
        m_dpi_scale = m_backend->display_density();
    }
}

AndroidWindow::~AndroidWindow() {
    unbind_surface();
    if (m_backend) {
        m_backend->unregister_window(this);
    }
}

void AndroidWindow::set_size(Vec2 new_size) {
    m_size = new_size;
    if (m_on_resized) m_on_resized(m_size);
}

void AndroidWindow::bind_surface(ANativeWindow* window, uint32_t phys_w, uint32_t phys_h, float dpi_scale) {
    if (m_native_window != window) {
        if (m_native_window) {
            ANativeWindow_release(m_native_window);
        }
        m_native_window = window;
        if (m_native_window) {
            ANativeWindow_acquire(m_native_window);
        }
    }

    const float clamped_dpi = std::clamp(dpi_scale, 0.5f, 6.0f);
    const bool dpi_changed = (clamped_dpi != m_dpi_scale);
    m_dpi_scale = clamped_dpi;

    if (phys_w > 0 && phys_h > 0) {
        const Vec2 new_logical{static_cast<float>(phys_w) / m_dpi_scale,
                               static_cast<float>(phys_h) / m_dpi_scale};
        const bool size_changed = (new_logical.x != m_size.x || new_logical.y != m_size.y);
        m_size = new_logical;
        if (size_changed && m_on_resized) {
            m_on_resized(m_size);
        }
    }
    if (dpi_changed && m_on_dpi_changed) {
        m_on_dpi_changed(m_dpi_scale);
    }
}

void AndroidWindow::unbind_surface() {
    if (m_native_window) {
        ANativeWindow_release(m_native_window);
        m_native_window = nullptr;
    }
}

void AndroidWindow::present_rgba8(const uint8_t* rgba, uint32_t width, uint32_t height) {
    present_rgba8_region(rgba, width, height, 0, 0, width, height);
}

void AndroidWindow::present_rgba8_region(const uint8_t* rgba, uint32_t width, uint32_t height,
                                         uint32_t, uint32_t, uint32_t, uint32_t) {
    if (!m_native_window || !rgba || width == 0 || height == 0) return;

    ANativeWindow_setBuffersGeometry(m_native_window,
                                     static_cast<int32_t>(width),
                                     static_cast<int32_t>(height),
                                     WINDOW_FORMAT_RGBA_8888);

    ANativeWindow_Buffer buffer{};
    if (ANativeWindow_lock(m_native_window, &buffer, nullptr) != 0) {
        return;
    }

    const uint32_t copy_w = std::min(width, static_cast<uint32_t>(buffer.width));
    const uint32_t copy_h = std::min(height, static_cast<uint32_t>(buffer.height));
    const size_t row_bytes = static_cast<size_t>(copy_w) * 4;
    auto* dst = static_cast<uint8_t*>(buffer.bits);
    const size_t dst_stride_bytes = static_cast<size_t>(buffer.stride) * 4;
    const size_t src_stride_bytes = static_cast<size_t>(width) * 4;

    for (uint32_t y = 0; y < copy_h; ++y) {
        std::memcpy(dst + y * dst_stride_bytes, rgba + y * src_stride_bytes, row_bytes);
    }

    ANativeWindow_unlockAndPost(m_native_window);
}

void AndroidWindow::set_ime_enabled(bool enabled) {
    if (m_backend) {
        m_backend->set_ime_enabled_jni(enabled);
    }
}

void AndroidWindow::close() {
    if (m_closed) return;
    if (m_on_close && !m_on_close()) return;
    m_closed = true;
    m_visible = false;
}

// ==================== AndroidBackend ====================

AndroidBackend::AndroidBackend() {
    s_instance = this;
}

AndroidBackend::~AndroidBackend() {
    shutdown();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void AndroidBackend::set_jni_bridge(JNIEnv* env, jclass bridge_class) {
    if (!env || !bridge_class) return;
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) == JNI_OK) {
        g_pal_jvm.store(vm, std::memory_order_release);
    }
    if (g_pal_bridge_class) {
        env->DeleteGlobalRef(g_pal_bridge_class);
        g_pal_bridge_class = nullptr;
    }
    g_pal_bridge_class = static_cast<jclass>(env->NewGlobalRef(bridge_class));
    g_set_ime_method = env->GetStaticMethodID(g_pal_bridge_class, "setImeEnabled", "(Z)V");
    g_get_clip_method = env->GetStaticMethodID(g_pal_bridge_class, "getClipboardText", "()Ljava/lang/String;");
    g_set_clip_method = env->GetStaticMethodID(g_pal_bridge_class, "setClipboardText", "(Ljava/lang/String;)V");
}

std::unique_ptr<IPlatformWindow> AndroidBackend::create_window(const WindowDesc& desc) {
    auto win = std::make_unique<AndroidWindow>(desc, this);
    m_windows.push_back(win.get());
    return win;
}

void AndroidBackend::unregister_window(AndroidWindow* win) {
    std::erase(m_windows, win);
}

void AndroidBackend::shutdown() {
    if (m_surface) {
        ANativeWindow_release(m_surface);
        m_surface = nullptr;
    }
    m_windows.clear();
}

void AndroidBackend::on_surface_changed(ANativeWindow* window, int phys_w, int phys_h, float density) {
    if (m_surface != window) {
        if (m_surface) ANativeWindow_release(m_surface);
        m_surface = window;
        if (m_surface) ANativeWindow_acquire(m_surface);
    }
    m_surface_w = static_cast<uint32_t>(std::max(0, phys_w));
    m_surface_h = static_cast<uint32_t>(std::max(0, phys_h));
    m_density = std::clamp(density, 0.5f, 6.0f);

    for (auto* w : m_windows) {
        if (w) w->bind_surface(m_surface, m_surface_w, m_surface_h, m_density);
    }
}

void AndroidBackend::on_surface_destroyed() {
    for (auto* w : m_windows) {
        if (w) w->unbind_surface();
    }
    if (m_surface) {
        ANativeWindow_release(m_surface);
        m_surface = nullptr;
    }
    m_surface_w = 0;
    m_surface_h = 0;
}

void AndroidBackend::on_touch(int action, float phys_x, float phys_y) {
    if (m_windows.empty()) return;
    AndroidWindow* target = m_windows.back();
    const float inv_dpi = (m_density > 0.0f) ? (1.0f / m_density) : 1.0f;
    PlatformEvent pe{};
    pe.window_handle = target->get_native_handle();
    pe.position = Vec2{phys_x * inv_dpi, phys_y * inv_dpi};
    pe.button = PointerButton::Primary;
    pe.timestamp_ms = now_ms();

    // MotionEvent: ACTION_DOWN = 0, ACTION_UP = 1, ACTION_MOVE = 2, ACTION_CANCEL = 3
    if (action == 0) {
        pe.type = PlatformEventType::PointerDown;
    } else if (action == 2) {
        pe.type = PlatformEventType::PointerMove;
        pe.button = PointerButton::None;
    } else if (action == 1 || action == 3) {
        pe.type = PlatformEventType::PointerUp;
    } else {
        return;
    }
    dispatch_event(pe);
}

void AndroidBackend::on_key(int action, int android_key_code, bool shift, bool ctrl, bool alt) {
    if (m_windows.empty()) return;
    AndroidWindow* target = m_windows.back();
    PlatformEvent pe{};
    pe.window_handle = target->get_native_handle();
    pe.type = (action == 0) ? PlatformEventType::KeyDown : PlatformEventType::KeyUp;
    pe.key_code = map_android_keycode(android_key_code);
    pe.modifiers.shift = shift;
    pe.modifiers.ctrl = ctrl;
    pe.modifiers.alt = alt;
    pe.timestamp_ms = now_ms();
    dispatch_event(pe);
}

void AndroidBackend::on_ime_composition(const std::string& text, size_t cursor_byte) {
    if (m_windows.empty()) return;
    AndroidWindow* target = m_windows.back();
    PlatformEvent pe{};
    pe.type = PlatformEventType::ImeComposition;
    pe.window_handle = target->get_native_handle();
    pe.text = text;
    pe.ime_cursor_byte = cursor_byte;
    pe.timestamp_ms = now_ms();
    dispatch_event(pe);
}

void AndroidBackend::on_ime_commit(const std::string& text) {
    if (m_windows.empty() || text.empty()) return;
    AndroidWindow* target = m_windows.back();
    PlatformEvent pe{};
    pe.type = PlatformEventType::ImeCommit;
    pe.window_handle = target->get_native_handle();
    pe.text = text;
    pe.timestamp_ms = now_ms();
    dispatch_event(pe);
}

void AndroidBackend::set_ime_enabled_jni(bool enabled) {
    bool attached = false;
    JNIEnv* env = get_pal_env(attached);
    if (env && g_pal_bridge_class && g_set_ime_method) {
        env->CallStaticVoidMethod(g_pal_bridge_class, g_set_ime_method, enabled ? JNI_TRUE : JNI_FALSE);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    release_pal_env(attached);
}

std::string AndroidBackend::get_clipboard_text() const {
    std::string out;
    bool attached = false;
    JNIEnv* env = get_pal_env(attached);
    if (env && g_pal_bridge_class && g_get_clip_method) {
        auto jstr = static_cast<jstring>(env->CallStaticObjectMethod(g_pal_bridge_class, g_get_clip_method));
        if (jstr && !env->ExceptionCheck()) {
            const char* chars = env->GetStringUTFChars(jstr, nullptr);
            if (chars) {
                out = chars;
                env->ReleaseStringUTFChars(jstr, chars);
            }
            env->DeleteLocalRef(jstr);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
    release_pal_env(attached);
    return out;
}

void AndroidBackend::set_clipboard_text(const std::string& utf8_text) {
    bool attached = false;
    JNIEnv* env = get_pal_env(attached);
    if (env && g_pal_bridge_class && g_set_clip_method) {
        jstring jstr = env->NewStringUTF(utf8_text.c_str());
        if (jstr) {
            env->CallStaticVoidMethod(g_pal_bridge_class, g_set_clip_method, jstr);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(jstr);
        }
    }
    release_pal_env(attached);
}

void AndroidBackend::dispatch_event(const PlatformEvent& pe) {
    if (m_event_callback) {
        m_event_callback(pe);
    }
}

} // namespace ktui

#endif // defined(__ANDROID__)
