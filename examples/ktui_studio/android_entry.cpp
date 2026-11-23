#if defined(__ANDROID__)

#include <ktui/ktui.hpp>
#include <pal/android_backend.hpp>
#include <android/native_window_jni.h>
#include <jni.h>
#include <memory>
#include <string>

namespace kernetile::text {
void android_init_text_bridge(JNIEnv* env, jclass bridge_class);
}

namespace {

using namespace ktui;

struct AndroidStudioState {
    std::unique_ptr<Application> app;
    std::shared_ptr<Window> window;
    int count{0};
    Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    Property<bool> step_ten{false};
    Property<bool> locked{false};
    Property<std::string> input_text{"KerneTile Android"};
    Property<std::string> input_preview{"输入预览: KerneTile Android"};
    Property<std::string> theme_btn_text{"切换浅色主题"};
    Property<Color> window_bg{Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f}};
    Property<Color> card_bg{Color{0x27 / 255.0f, 0x28 / 255.0f, 0x2C / 255.0f, 1.0f}};
    Property<Color> card_border{Color{1.0f, 1.0f, 1.0f, 0.06f}};
    Property<Color> muted_text{Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f}};

    void refresh_count() {
        int step = step_ten.get() ? 10 : 1;
        count_text.set("当前计数: " + std::to_string(count) + "   (步长: " + std::to_string(step) + ")" +
                       (locked.get() ? "  [已锁定]" : ""));
    }

    void init_ui() {
        app = std::make_unique<Application>(create_platform_backend(false));

        WindowDesc desc;
        desc.title = "KerneTile Studio";
        desc.size = Vec2{412.0f, 915.0f};
        desc.resizable = false;
        desc.custom_titlebar = false;

        window = app->create_window(desc);
        if (!window) return;

        app->theme.subscribe([this](const Theme& t) {
            if (t.is_dark) {
                window_bg.set(Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f});
                card_bg.set(Color{0x27 / 255.0f, 0x28 / 255.0f, 0x2C / 255.0f, 1.0f});
                card_border.set(Color{1.0f, 1.0f, 1.0f, 0.06f});
                muted_text.set(Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f});
                theme_btn_text.set("切换浅色主题");
            } else {
                window_bg.set(Color{0xEA / 255.0f, 0xED / 255.0f, 0xF1 / 255.0f, 1.0f});
                card_bg.set(Color{0xFA / 255.0f, 0xFB / 255.0f, 0xFD / 255.0f, 1.0f});
                card_border.set(Color{0.0f, 0.0f, 0.0f, 0.07f});
                muted_text.set(Color{0x59 / 255.0f, 0x60 / 255.0f, 0x6A / 255.0f, 1.0f});
                theme_btn_text.set("切换深色主题");
            }
        });

        auto card = [this](WidgetPtr child) {
            return Box {
                .background_color = Bind(card_bg),
                .border_color = Bind(card_border),
                .border_width = 1.0f,
                .corner_radius = 8.0f,
                .padding = EdgeInsets(14.0f, 16.0f),
                .child = std::move(child)
            };
        };

        window->set_content(Box {
            .background_color = Bind(window_bg),
            .padding = EdgeInsets(24.0f, 16.0f, 16.0f, 16.0f),
            .child = VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 14.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Text { .text = "KerneTile Studio (Android)", .font_size = 18.0f },
                            Button {
                                .text = Bind(theme_btn_text),
                                .on_click = [this]() {
                                    app->set_theme(app->current_theme().is_dark ? Theme::light() : Theme::dark());
                                }
                            }
                        }
                    },
                    card(VStack {
                        .spacing = 12.0f,
                        .content = {
                            Text { .text = Bind(count_text), .font_size = 15.0f },
                            HStack {
                                .align_items = AlignItems::Center,
                                .spacing = 10.0f,
                                .content = {
                                    Button {
                                        .text = "+ 增加",
                                        .variant = ButtonVariant::Accent,
                                        .on_click = [this]() { count += step_ten.get() ? 10 : 1; refresh_count(); },
                                        .disabled = Bind(locked)
                                    },
                                    Button {
                                        .text = "- 减少",
                                        .on_click = [this]() { count -= step_ten.get() ? 10 : 1; refresh_count(); },
                                        .disabled = Bind(locked)
                                    },
                                    Button {
                                        .text = "重置",
                                        .variant = ButtonVariant::Danger,
                                        .on_click = [this]() { count = 0; refresh_count(); },
                                        .disabled = Bind(locked)
                                    }
                                }
                            }
                        }
                    }),
                    card(HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 20.0f,
                        .content = {
                            Checkbox { .checked = Bind(step_ten), .label = "步长 10 (+10)", .on_change = [this](bool) { refresh_count(); } },
                            Checkbox { .checked = Bind(locked), .label = "锁定按钮", .on_change = [this](bool) { refresh_count(); } }
                        }
                    }),
                    card(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 10.0f,
                        .content = {
                            Text { .text = "单行文本输入 (支持软键盘与双向绑定):", .color = Bind(muted_text) },
                            TextInput {
                                .text = Bind(input_text),
                                .placeholder = "点击此处唤起键盘输入...",
                                .on_change = [this](const std::string& val) {
                                    input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                                }
                            },
                            Text { .text = Bind(input_preview), .color = Color{0.38f, 0.78f, 0.98f, 1.0f} }
                        }
                    })
                }
            }
        });

        window->show();
        app->run();
    }
};

std::unique_ptr<AndroidStudioState> g_studio;

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
    if (!g_studio) {
        g_studio = std::make_unique<AndroidStudioState>();
        g_studio->init_ui();
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeSurfaceChanged(
    JNIEnv* env, jclass, jobject surface, jint width, jint height, jfloat density) {
    ANativeWindow* native_win = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_surface_changed(native_win, width, height, density);
    }
    if (native_win) {
        ANativeWindow_release(native_win);
    }
    if (g_studio && g_studio->window) {
        g_studio->window->request_redraw();
        g_studio->window->render_frame(true);
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
    if (g_studio && g_studio->app) {
        g_studio->app->step_frame();
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnTouch(
    JNIEnv*, jclass, jint action, jfloat x, jfloat y) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_touch(action, x, y);
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnKey(
    JNIEnv*, jclass, jint action, jint key_code, jboolean shift, jboolean ctrl, jboolean alt) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_key(action, key_code, shift == JNI_TRUE, ctrl == JNI_TRUE, alt == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_com_kernetile_studio_KerneTileBridge_nativeOnImeComposition(
    JNIEnv* env, jclass, jstring text, jint cursor_byte) {
    if (auto* backend = ktui::AndroidBackend::instance()) {
        backend->on_ime_composition(jstring_to_utf8(env, text), static_cast<size_t>(std::max(0, cursor_byte)));
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
    g_studio.reset();
}

} // extern "C"

#endif // defined(__ANDROID__)
