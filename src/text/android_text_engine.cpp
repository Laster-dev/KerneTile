#if defined(__ANDROID__)

#include "kernetile/text/font_engine.hpp"
#include <jni.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace kernetile::text {

namespace {

std::atomic<JavaVM*> g_jvm{nullptr};
jclass g_bridge_class{nullptr};
jmethodID g_measure_method{nullptr};
jmethodID g_raster_method{nullptr};

JNIEnv* get_jni_env(bool& did_attach) {
    did_attach = false;
    JavaVM* vm = g_jvm.load(std::memory_order_acquire);
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

void release_jni_env(bool did_attach) {
    if (did_attach) {
        if (JavaVM* vm = g_jvm.load(std::memory_order_acquire)) {
            vm->DetachCurrentThread();
        }
    }
}

void measure_jni(const std::string& utf8, float size, float out_values[3]) {
    out_values[0] = static_cast<float>(utf8.size()) * size * 0.55f;
    out_values[1] = size * 0.8f;
    out_values[2] = size * 0.2f;

    bool attached = false;
    JNIEnv* env = get_jni_env(attached);
    if (!env || !g_bridge_class || !g_measure_method) {
        release_jni_env(attached);
        return;
    }

    jstring jtext = env->NewStringUTF(utf8.c_str());
    if (jtext) {
        auto jarr = static_cast<jfloatArray>(
            env->CallStaticObjectMethod(g_bridge_class, g_measure_method, jtext, static_cast<jfloat>(size)));
        if (jarr && !env->ExceptionCheck()) {
            if (env->GetArrayLength(jarr) >= 3) {
                env->GetFloatArrayRegion(jarr, 0, 3, out_values);
            }
            env->DeleteLocalRef(jarr);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        env->DeleteLocalRef(jtext);
    }
    release_jni_env(attached);
}

FontRasterResult raster_jni(const std::string& utf8, float size) {
    FontRasterResult result{};
    bool attached = false;
    JNIEnv* env = get_jni_env(attached);
    if (!env || !g_bridge_class || !g_raster_method) {
        release_jni_env(attached);
        return result;
    }

    jstring jtext = env->NewStringUTF(utf8.c_str());
    jintArray jbox = env->NewIntArray(4);
    if (jtext && jbox) {
        auto jbytes = static_cast<jbyteArray>(
            env->CallStaticObjectMethod(g_bridge_class, g_raster_method, jtext, static_cast<jfloat>(size), jbox));
        if (jbytes && !env->ExceptionCheck()) {
            jint box[4]{0, 0, 0, 0};
            env->GetIntArrayRegion(jbox, 0, 4, box);
            const jsize len = env->GetArrayLength(jbytes);
            if (box[0] > 0 && box[1] > 0 && len >= box[0] * box[1]) {
                result.width = static_cast<uint32_t>(box[0]);
                result.height = static_cast<uint32_t>(box[1]);
                result.bearing_x = box[2];
                result.bearing_y = box[3];
                result.r8_bitmap.resize(static_cast<size_t>(box[0]) * static_cast<size_t>(box[1]));
                env->GetByteArrayRegion(jbytes, 0, static_cast<jsize>(result.r8_bitmap.size()),
                                        reinterpret_cast<jbyte*>(result.r8_bitmap.data()));
            }
            env->DeleteLocalRef(jbytes);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
    if (jbox) env->DeleteLocalRef(jbox);
    if (jtext) env->DeleteLocalRef(jtext);
    release_jni_env(attached);
    return result;
}

class AndroidTextEngine final : public ITextEngine {
public:
    FontMetrics get_metrics(float size, std::string_view) override {
        float values[3]{};
        measure_jni("M", size, values);
        return {size, values[1], -values[2], values[1] + values[2]};
    }

    TextLayoutResult shape_text(std::string_view text, float size, std::string_view family) override {
        TextLayoutResult result;
        result.metrics = get_metrics(size, family);
        result.total_height = result.metrics.line_height;
        const auto stops = grapheme_boundaries(text);
        for (size_t i = 1; i < stops.size(); ++i) {
            const auto offset = stops[i - 1];
            const std::string cluster(text.substr(offset, stops[i] - offset));
            auto [it, inserted] = m_ids.try_emplace(cluster, glyph_base + static_cast<uint32_t>(m_clusters.size()));
            if (inserted) m_clusters.push_back(cluster);
            float values[3]{};
            measure_jni(cluster, size, values);
            result.carets.push_back({offset, result.total_width});
            GlyphPlacement glyph{};
            glyph.glyph_id = it->second;
            glyph.x = result.total_width;
            glyph.y = result.metrics.ascender;
            glyph.advance_x = values[0];
            glyph.cluster = offset;
            result.glyphs.push_back(glyph);
            result.total_width += values[0];
        }
        result.carets.push_back({text.size(), result.total_width});
        return result;
    }

    FontRasterResult rasterize_glyph(uint32_t id, float size, bool) override {
        std::string text;
        if (id >= glyph_base && id - glyph_base < m_clusters.size()) {
            text = m_clusters[id - glyph_base];
        } else if (id > 0 && id < 128) {
            text.push_back(static_cast<char>(id));
        }
        if (text.empty()) return {};
        return raster_jni(text, size);
    }

private:
    static constexpr uint32_t glyph_base = 1u << 31;
    std::unordered_map<std::string, uint32_t> m_ids;
    std::vector<std::string> m_clusters;
};

} // namespace

void android_init_text_bridge(JNIEnv* env, jclass bridge_class) {
    if (!env || !bridge_class) return;
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) == JNI_OK) {
        g_jvm.store(vm, std::memory_order_release);
    }
    if (g_bridge_class) {
        env->DeleteGlobalRef(g_bridge_class);
        g_bridge_class = nullptr;
    }
    g_bridge_class = static_cast<jclass>(env->NewGlobalRef(bridge_class));
    g_measure_method = env->GetStaticMethodID(g_bridge_class, "measureText", "(Ljava/lang/String;F)[F");
    g_raster_method = env->GetStaticMethodID(g_bridge_class, "rasterizeGlyph", "(Ljava/lang/String;F[I)[B");
}

std::unique_ptr<ITextEngine> create_text_engine() {
    return std::make_unique<AndroidTextEngine>();
}

std::unique_ptr<ITextEngine> create_directwrite_engine() {
    return nullptr;
}

std::vector<size_t> grapheme_boundaries(std::string_view text) {
    std::vector<size_t> boundaries;
    boundaries.push_back(0);
    size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<uint8_t>(text[i]);
        size_t len = 1;
        if ((lead & 0xE0) == 0xC0) len = 2;
        else if ((lead & 0xF0) == 0xE0) len = 3;
        else if ((lead & 0xF8) == 0xF0) len = 4;
        i = std::min(text.size(), i + len);
        boundaries.push_back(i);
    }
    return boundaries;
}

} // namespace kernetile::text

#endif // defined(__ANDROID__)
