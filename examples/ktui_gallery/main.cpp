#include "gallery_app.hpp"
#include <clocale>
#include <memory>
#include <string>

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

int main() {
    std::setlocale(LC_ALL, "");

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
    return app.run();
}

#endif
