#include "gallery_app.hpp"
#include <atomic>
#include <clocale>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(__ANDROID__)
#include <pal/android_backend.hpp>
#include <android/native_window_jni.h>
#include <jni.h>

namespace kernetile::text {
void android_init_text_bridge(JNIEnv* env, jclass bridge_class);
}
#endif

using namespace ktui;

#if defined(__ANDROID__)

namespace {

struct AndroidGalleryState {
    std::unique_ptr<Application> app;
    std::shared_ptr<Window> window;
    gallery::GalleryApp gallery;

    void init_ui() {
        app = std::make_unique<Application>(create_platform_backend(false));

        WindowDesc desc;
        desc.title = "KTUI Gallery";
        desc.size = Vec2{412.0f, 915.0f};
        desc.resizable = false;
        desc.custom_titlebar = false;

        window = app->create_window(desc);
        if (!window) return;

        gallery.setup(*app, window);
        window->show();
        app->run();
    }
};

std::unique_ptr<AndroidGalleryState> g_gallery;

std::string jstring_to_utf8(JNIEnv* env, jstring jstr) {
    if (!env || !jstr) return "";
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    if (!chars) return "";
    std::string out(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return out;
}

} // namespace

extern "C" {

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeInit(JNIEnv* env, jclass clazz) {
    kernetile::text::android_init_text_bridge(env, clazz);
    ktui::AndroidBackend::set_jni_bridge(env, clazz);
    if (!g_gallery) {
        g_gallery = std::make_unique<AndroidGalleryState>();
        g_gallery->init_ui();
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeSurfaceChanged(JNIEnv* env, jclass, jobject surface, jint width, jint height, jfloat density) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        ANativeWindow* native_window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
        backend->on_surface_changed(native_window, width, height, density);
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeSurfaceDestroyed(JNIEnv*, jclass) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_surface_destroyed();
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeStepFrame(JNIEnv*, jclass) {
    if (g_gallery && g_gallery->app) {
        g_gallery->app->step_frame();
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnTouch(JNIEnv*, jclass, jint action, jfloat x, jfloat y) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_touch(action, x, y);
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnKey(JNIEnv*, jclass, jint action, jint key_code, jboolean shift, jboolean ctrl, jboolean alt) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_key(action, key_code, shift == JNI_TRUE, ctrl == JNI_TRUE, alt == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnImeComposition(JNIEnv* env, jclass, jstring text, jint cursor_byte) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_ime_composition(jstring_to_utf8(env, text), static_cast<size_t>(cursor_byte));
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnImeCommit(JNIEnv* env, jclass, jstring text) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_ime_commit(jstring_to_utf8(env, text));
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeShutdown(JNIEnv*, jclass) {
    g_gallery.reset();
}

} // extern "C"

#else // !defined(__ANDROID__)

#if defined(__EMSCRIPTEN__)
static std::unique_ptr<Application> g_web_app;
static std::unique_ptr<gallery::GalleryApp> g_web_gallery;
#endif

#if !defined(__EMSCRIPTEN__)
namespace {

struct AutoWidgetInfo {
    float x{0.0f}, y{0.0f}, w{0.0f}, h{0.0f};
    bool focusable{false};
    bool text_input{false};
    bool hand{false};
    bool overlay{false};
};

void collect_interactive_boxes(
    RenderBox* node,
    float ox,
    float oy,
    float win_w,
    float win_h,
    bool in_overlay,
    std::vector<AutoWidgetInfo>& out)
{
    if (!node) return;
    Vec2 sz = node->size();
    float gx = ox + node->offset().x;
    float gy = oy + node->offset().y;

    bool interactive = static_cast<bool>(node->on_pointer_down) ||
                       node->is_focusable ||
                       node->cursor_shape == CursorShape::PointingHand ||
                       node->cursor_shape == CursorShape::IBeam;

    // Only collect reasonably sized visible interactive controls inside the window
    if (interactive && sz.x >= 12.0f && sz.y >= 12.0f &&
        gx + sz.x > 0.0f && gy + sz.y > 0.0f && gx < win_w && gy < win_h)
    {
        // Filter out full-page container boxes (like ScrollView or root containers)
        if (sz.x < win_w * 0.85f || sz.y < win_h * 0.65f) {
            out.push_back(AutoWidgetInfo{
                .x = gx,
                .y = gy,
                .w = sz.x,
                .h = sz.y,
                .focusable = node->is_focusable,
                .text_input = (node->as_text_input_client() != nullptr),
                .hand = (node->cursor_shape == CursorShape::PointingHand),
                .overlay = in_overlay
            });
        }
    }

    for (RenderNode* ch = node->first_child(); ch != nullptr; ch = ch->next_sibling()) {
        if (auto* child_box = ch->as_render_box()) {
            collect_interactive_boxes(child_box, gx, gy, win_w, win_h, in_overlay, out);
        }
    }
}

void start_automation_thread(
    Application& app,
    std::shared_ptr<Window> window,
    gallery::GalleryApp& gallery,
    std::atomic<bool>& stop_flag)
{
    std::thread([&app, window, &gallery, &stop_flag]() {
        auto run_ui = [&](Fn<std::string()> fn) -> std::string {
            std::mutex mtx;
            std::condition_variable cv;
            bool done = false;
            std::string result;

            app.dispatcher().post_task([&]() {
                if (!window->is_closed()) {
                    result = fn();
                } else {
                    result = "ERR window_closed";
                }
                {
                    std::lock_guard<std::mutex> lk(mtx);
                    done = true;
                }
                cv.notify_one();
            });
            if (window && window->platform_window()) {
                window->platform_window()->invalidate();
            }

            std::unique_lock<std::mutex> lk(mtx);
            cv.wait_for(lk, std::chrono::seconds(5), [&]() { return done || stop_flag.load(); });
            return done ? result : "ERR timeout";
        };

        std::string line;
        while (!stop_flag.load() && std::getline(std::cin, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            std::string reply;
            if (cmd == "PING") {
                reply = "OK PONG";
            } else if (cmd == "QUIT") {
                reply = run_ui([&]() -> std::string {
                    app.quit(0);
                    return "OK";
                });
                std::printf("%s\n", reply.c_str());
                std::fflush(stdout);
                break;
            } else if (cmd == "INFO") {
                reply = run_ui([&]() -> std::string {
                    Vec2 sz = window->size();
                    float dpi = window->dpi_scale();
                    float tb = window->titlebar_height();
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                        "OK {\"w\":%.1f,\"h\":%.1f,\"dpi\":%.3f,\"tb\":%.1f,\"page\":\"%s\",\"dark\":%s}",
                        sz.x, sz.y, dpi, tb,
                        gallery.nav_selected.get().c_str(),
                        app.current_theme().is_dark ? "true" : "false");
                    return std::string(buf);
                });
            } else if (cmd == "RESIZE") {
                float w = 1200.0f, h = 760.0f;
                iss >> w >> h;
                reply = run_ui([&]() -> std::string {
                    window->set_size(Vec2{w, h});
                    window->request_redraw();
                    window->render_frame(true);
                    return "OK";
                });
            } else if (cmd == "NAV") {
                std::string page_id;
                iss >> page_id;
                reply = run_ui([&]() -> std::string {
                    window->dismiss_all_overlays();
                    gallery.nav_selected.set(page_id);
                    window->request_redraw();
                    window->render_frame(true);
                    return "OK";
                });
            } else if (cmd == "THEME") {
                std::string mode;
                iss >> mode;
                reply = run_ui([&]() -> std::string {
                    Vec2 sz = window->size();
                    Vec2 origin{sz.x - 140.0f, 16.0f};
                    if (mode == "light") {
                        app.set_theme_with_ripple(Theme::light(), origin);
                    } else if (mode == "dark") {
                        app.set_theme_with_ripple(Theme::dark(), origin);
                    } else {
                        app.set_theme_with_ripple(app.current_theme().is_dark ? Theme::light() : Theme::dark(), origin);
                    }
                    window->request_redraw();
                    return "OK";
                });
            } else if (cmd == "LIST") {
                reply = run_ui([&]() -> std::string {
                    window->render_frame();
                    Vec2 sz = window->size();
                    float tb = window->titlebar_height();
                    std::vector<AutoWidgetInfo> items;
                    if (auto* root = window->root_render_box()) {
                        collect_interactive_boxes(root, 0.0f, tb, sz.x, sz.y, false, items);
                    }
                    for (const auto& ov : window->overlay_manager().entries()) {
                        if (ov.content) {
                            collect_interactive_boxes(ov.content.get(), 0.0f, tb, sz.x, sz.y, true, items);
                        }
                    }
                    std::ostringstream out;
                    out << "OK [";
                    for (size_t i = 0; i < items.size(); ++i) {
                        const auto& it = items[i];
                        if (i > 0) out << ",";
                        char buf[220];
                        std::snprintf(buf, sizeof(buf),
                            "{\"i\":%zu,\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f,\"cx\":%.1f,\"cy\":%.1f,\"f\":%d,\"t\":%d,\"hnd\":%d,\"ov\":%d}",
                            i, it.x, it.y, it.w, it.h,
                            it.x + it.w * 0.5f, it.y + it.h * 0.5f,
                            it.focusable ? 1 : 0,
                            it.text_input ? 1 : 0,
                            it.hand ? 1 : 0,
                            it.overlay ? 1 : 0);
                        out << buf;
                    }
                    out << "]";
                    return out.str();
                });
            } else if (cmd == "MOVE" || cmd == "DOWN" || cmd == "UP") {
                float x = 0.0f, y = 0.0f;
                iss >> x >> y;
                reply = run_ui([&]() -> std::string {
                    PlatformEvent pe;
                    pe.position = Vec2{x, y};
                    pe.dpi_scale = window->dpi_scale();
                    if (cmd == "MOVE") {
                        pe.type = PlatformEventType::PointerMove;
                    } else if (cmd == "DOWN") {
                        pe.type = PlatformEventType::PointerDown;
                        pe.button = PointerButton::Primary;
                    } else {
                        pe.type = PlatformEventType::PointerUp;
                        pe.button = PointerButton::Primary;
                    }
                    window->handle_platform_event(pe);
                    window->request_redraw();
                    return "OK";
                });
            } else if (cmd == "SCROLL") {
                float x = 0.0f, y = 0.0f, dy = -120.0f;
                iss >> x >> y >> dy;
                reply = run_ui([&]() -> std::string {
                    PlatformEvent pe;
                    pe.type = PlatformEventType::PointerScroll;
                    pe.position = Vec2{x, y};
                    pe.delta = Vec2{0.0f, dy};
                    pe.dpi_scale = window->dpi_scale();
                    window->handle_platform_event(pe);
                    window->request_redraw();
                    return "OK";
                });
            } else if (cmd == "TYPE") {
                std::string text;
                std::getline(iss, text);
                if (!text.empty() && text[0] == ' ') text.erase(0, 1);
                reply = run_ui([&]() -> std::string {
                    PlatformEvent pe;
                    pe.type = PlatformEventType::ImeCommit;
                    pe.text = text;
                    window->handle_platform_event(pe);
                    window->request_redraw();
                    return "OK";
                });
            } else {
                reply = "ERR unknown_cmd";
            }

            std::printf("%s\n", reply.c_str());
            std::fflush(stdout);
        }
    }).detach();
}

} // namespace
#endif

int main(int argc, char** argv) {
#if !defined(__EMSCRIPTEN__)
    std::setlocale(LC_ALL, "");
    bool automation_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--automation") == 0) {
            automation_mode = true;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

#if defined(__EMSCRIPTEN__)
    g_web_app = std::make_unique<Application>(create_platform_backend(false));
    auto& app = *g_web_app;
#else
    Application app(create_platform_backend(false));
#endif

    WindowDesc desc;
    desc.title = "KTUI Gallery";
    desc.size = Vec2{960.0f, 640.0f};
    desc.min_size = Vec2{460.0f, 400.0f};
    desc.resizable = true;

    auto window = app.create_window(desc);
    if (!window) return 1;

#if defined(__EMSCRIPTEN__)
    g_web_gallery = std::make_unique<gallery::GalleryApp>();
    auto& gallery = *g_web_gallery;
#else
    gallery::GalleryApp gallery;
#endif
    gallery.setup(app, window);

    window->show();
#if !defined(__EMSCRIPTEN__)
    std::atomic<bool> stop_automation{false};
    if (automation_mode) {
        start_automation_thread(app, window, gallery, stop_automation);
    }
    int rc = app.run();
    stop_automation.store(true);
    return rc;
#else
    return app.run();
#endif
}

#endif
