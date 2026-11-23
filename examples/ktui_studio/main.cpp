#include <ktui/ktui.hpp>
#include <clocale>
#include <memory>
#include <string>
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

namespace {

struct StudioApp {
    Application* app{nullptr};
    std::shared_ptr<Window> window;
    int count{0};
    int child_window_seq{1};
    std::vector<std::shared_ptr<Property<std::string>>> child_notes;

    Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    Property<bool> step_ten{false};
    Property<bool> locked{false};
    Property<int> render_mode{1};
    Property<std::string> input_text{"KerneTile KTUI"};
    Property<std::string> input_preview{"输入预览: KerneTile KTUI"};
    Property<std::string> theme_btn_text{"切换浅色主题"};

    Property<Color> window_bg{Color::transparent()};
    Property<Color> card_bg{Color{1.0f, 1.0f, 1.0f, 0.045f}};
    Property<Color> card_border{Color{1.0f, 1.0f, 1.0f, 0.07f}};
    Property<Color> muted_text{Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f}};

    void refresh_count() {
        int step = step_ten.get() ? 10 : 1;
        count_text.set("当前计数: " + std::to_string(count) + "   (步长: " + std::to_string(step) + ")" +
                       (locked.get() ? "  [已锁定]" : ""));
    }

    void setup(Application& application, std::shared_ptr<Window> win) {
        app = &application;
        window = std::move(win);

        app->theme.subscribe([this](const Theme& t) {
            if (t.is_dark) {
#if defined(__ANDROID__)
                window_bg.set(Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f});
                card_bg.set(Color{0x27 / 255.0f, 0x28 / 255.0f, 0x2C / 255.0f, 1.0f});
                card_border.set(Color{1.0f, 1.0f, 1.0f, 0.06f});
#else
                window_bg.set(Color::transparent());
                card_bg.set(Color{1.0f, 1.0f, 1.0f, 0.045f});
                card_border.set(Color{1.0f, 1.0f, 1.0f, 0.07f});
#endif
                muted_text.set(Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f});
                theme_btn_text.set("切换浅色主题");
            } else {
#if defined(__ANDROID__)
                window_bg.set(Color{0xEA / 255.0f, 0xED / 255.0f, 0xF1 / 255.0f, 1.0f});
                card_bg.set(Color{0xFA / 255.0f, 0xFB / 255.0f, 0xFD / 255.0f, 1.0f});
                card_border.set(Color{0.0f, 0.0f, 0.0f, 0.07f});
#else
                window_bg.set(Color::transparent());
                card_bg.set(Color{1.0f, 1.0f, 1.0f, 0.65f});
                card_border.set(Color{0.0f, 0.0f, 0.0f, 0.07f});
#endif
                muted_text.set(Color{0x59 / 255.0f, 0x60 / 255.0f, 0x6A / 255.0f, 1.0f});
                theme_btn_text.set("切换深色主题");
            }
        });

        auto card = [this](WidgetPtr child) {
            return Box {
                .background_color = Bind(card_bg),
                .border_color = Bind(card_border),
                .border_width = 1.0f,
                .corner_radius = 5.0f,
                .padding = EdgeInsets(12.0f, 14.0f),
                .child = std::move(child)
            };
        };

        auto spawn_child_window = [this, card]() {
            int idx = child_window_seq++;
            WindowDesc child_desc;
            child_desc.title = "工具窗口 #" + std::to_string(idx);
            child_desc.size = Vec2{360.0f, 240.0f};
            child_desc.min_size = Vec2{260.0f, 180.0f};
            child_desc.resizable = true;

            auto child_win = app->create_window(child_desc);
            if (!child_win) return;

            auto note = std::make_shared<Property<std::string>>("窗口 #" + std::to_string(idx) + " 独立输入");
            child_notes.push_back(note);
            child_win->set_content(Box {
                .background_color = Bind(window_bg),
                .padding = EdgeInsets(14.0f, 16.0f),
                .child = VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 10.0f,
                    .content = {
                        card(VStack {
                            .align_items = AlignItems::Stretch,
                            .spacing = 8.0f,
                            .content = {
                                Text { .text = "独立无边框子窗口 (共享全局计数绑定)", .color = Bind(muted_text) },
                                Text { .text = Bind(count_text), .font_size = 14.0f },
                                HStack {
                                    .align_items = AlignItems::Center,
                                    .spacing = 8.0f,
                                    .content = {
                                        Button {
                                            .text = "+1",
                                            .variant = ButtonVariant::Accent,
                                            .on_click = [this]() { count += 1; refresh_count(); }
                                        },
                                        Button {
                                            .text = "-1",
                                            .on_click = [this]() { count -= 1; refresh_count(); }
                                        }
                                    }
                                }
                            }
                        }),
                        TextInput {
                            .text = Bind(*note),
                            .placeholder = "在此窗口独立输入..."
                        }
                    }
                }
            });
            child_win->show();
        };

#if !defined(__ANDROID__)
        // 在标题栏右侧添加自定义操作按钮 (位于置顶 Pin 按钮左侧，对齐 CUI.Core)
        window->titlebar().add_action("+", "新建工具窗口", spawn_child_window);
#endif

        window->set_content(Box {
            .background_color = Bind(window_bg),
            .padding = EdgeInsets(16.0f, 18.0f),
            .child = VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
#if defined(__ANDROID__)
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
#endif
                    card(VStack {
                        .spacing = 10.0f,
                        .content = {
                            Text { .text = Bind(count_text), .font_size = 14.0f },
                            HStack {
                                .align_items = AlignItems::Center,
                                .spacing = 8.0f,
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
#if !defined(__ANDROID__)
                                    ,
                                    Button {
                                        .text = "+ 新建窗口",
                                        .on_click = spawn_child_window
                                    }
#endif
                                }
                            }
                        }
                    }),
                    card(HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 20.0f,
                        .content = {
                            Checkbox { .checked = Bind(step_ten), .label = "步长 10 (+10)", .on_change = [this](bool) { refresh_count(); } },
                            ToggleSwitch { .checked = Bind(locked), .label = "锁定按钮", .on_change = [this](bool) { refresh_count(); } }
                        }
                    }),
                    card(RadioGroup {
                        .selected_value = Bind(render_mode),
                        .options = {
                            RadioOption {
                                .label = "节能",
                                .value = 0,
                                .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                                    float cx = r.min_x + r.width() * 0.5f;
                                    float cy = r.min_y + r.height() * 0.5f;
                                    ctx.draw_rect(Rect::from_xywh(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f), col, 7.0f, 1.8f);
                                    ctx.draw_line(PointF{cx, cy - 3.5f}, PointF{cx, cy + 3.5f}, col, 1.8f);
                                }
                            },
                            RadioOption {
                                .label = "均衡",
                                .value = 1,
                                .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                                    float cx = r.min_x + r.width() * 0.5f;
                                    float cy = r.min_y + r.height() * 0.5f;
                                    ctx.draw_rect(Rect::from_xywh(cx - 8.0f, cy - 5.0f, 16.0f, 10.0f), col, 3.0f, 1.8f);
                                    ctx.draw_rect(Rect::from_xywh(cx - 3.0f, cy - 2.0f, 6.0f, 4.0f), col, 1.0f, 0.0f);
                                }
                            },
                            RadioOption {
                                .label = "性能",
                                .value = 2,
                                .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                                    float cx = r.min_x + r.width() * 0.5f;
                                    float cy = r.min_y + r.height() * 0.5f;
                                    ctx.draw_line(PointF{cx + 1.5f, cy - 8.0f}, PointF{cx - 4.5f, cy + 0.5f}, col, 2.0f);
                                    ctx.draw_line(PointF{cx - 4.5f, cy + 0.5f}, PointF{cx + 1.0f, cy + 0.5f}, col, 2.0f);
                                    ctx.draw_line(PointF{cx + 1.0f, cy + 0.5f}, PointF{cx - 1.5f, cy + 8.0f}, col, 2.0f);
                                }
                            }
                        },
                        .tile_width = 84.0f,
                        .tile_height = 68.0f
                    }),
                    card(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 8.0f,
                        .content = {
                            Text { .text = "单行文本输入 (支持撤销/重做、双向绑定):", .color = Bind(muted_text) },
                            TextInput {
                                .text = Bind(input_text),
                                .placeholder = "键入中文或英文...",
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
    }
};

} // namespace

#if defined(__ANDROID__)

namespace {

struct AndroidStudioState {
    std::unique_ptr<Application> app;
    std::shared_ptr<Window> window;
    StudioApp studio;

    void init_ui() {
        app = std::make_unique<Application>(create_platform_backend(false));

        WindowDesc desc;
        desc.title = "KerneTile Studio";
        desc.size = Vec2{412.0f, 915.0f};
        desc.resizable = false;
        desc.custom_titlebar = false;

        window = app->create_window(desc);
        if (!window) return;

        studio.setup(*app, window);
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

#else // !defined(__ANDROID__)

int main() {
    std::setlocale(LC_ALL, "");

    Application app(create_platform_backend(false));

    WindowDesc desc;
    desc.title = "KerneTile Studio";
    desc.size = Vec2{520.0f, 470.0f};
    desc.min_size = Vec2{380.0f, 380.0f};
    desc.resizable = true;

    auto window = app.create_window(desc);
    if (!window) return 1;

    StudioApp studio;
    studio.setup(app, window);

    window->show();
    return app.run();
}

#endif
