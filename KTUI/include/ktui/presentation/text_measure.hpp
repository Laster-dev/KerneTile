#pragma once

#include <ktui/pal/platform_types.hpp>
#include <kernetile/text/font_engine.hpp>
#include <memory>
#include <mutex>
#include <string_view>

namespace ktui {

// 框架级精确文本测量服务 (TextMeasure)
// 直接复用 KerneTile 底层真实排版引擎 (ITextEngine::shape_text)，
// 确保中英文混排、窄字符 (i/l)、宽字符 (W/M) 与 CJK 全角汉字的布局宽度、居中定位、光标坐标 100% 与真实渲染一致。
class TextMeasure {
public:
    static Vec2 measure(std::string_view utf8_text, float font_size) {
        if (utf8_text.empty()) {
            return Vec2{0.0f, font_size * 1.30f};
        }
        std::lock_guard<std::mutex> lock(mutex());
        auto& eng = engine();
        auto layout = eng.shape_text(utf8_text, font_size);
        float h = (layout.total_height > 0.0f) ? layout.total_height : (font_size * 1.30f);
        return Vec2{layout.total_width, h};
    }

    static float measure_width(std::string_view utf8_text, float font_size) {
        if (utf8_text.empty()) return 0.0f;
        return measure(utf8_text, font_size).x;
    }

    // 根据点击的相对 x 坐标，利用真实字符 advance 精确反算 UTF-8 字节光标位置
    static size_t hit_test_cursor(std::string_view utf8_text, float font_size, float relative_x) {
        if (utf8_text.empty() || relative_x <= 0.0f) return 0;

        std::lock_guard<std::mutex> lock(mutex());
        auto& eng = engine();
        auto layout = eng.shape_text(utf8_text, font_size);

        size_t byte_pos = 0;
        float acc_x = 0.0f;
        size_t glyph_idx = 0;

        while (byte_pos < utf8_text.size() && glyph_idx < layout.glyphs.size()) {
            uint8_t b = static_cast<uint8_t>(utf8_text[byte_pos]);
            size_t char_bytes = 1;
            if ((b & 0xE0) == 0xC0) char_bytes = 2;
            else if ((b & 0xF0) == 0xE0) char_bytes = 3;
            else if ((b & 0xF8) == 0xF0) char_bytes = 4;
            if (byte_pos + char_bytes > utf8_text.size()) {
                char_bytes = utf8_text.size() - byte_pos;
            }

            float adv = layout.glyphs[glyph_idx].advance_x;
            if (acc_x + adv * 0.5f >= relative_x) {
                return byte_pos;
            }
            acc_x += adv;
            byte_pos += char_bytes;
            glyph_idx++;
        }
        return utf8_text.size();
    }

private:
    static std::mutex& mutex() {
        static std::mutex mtx;
        return mtx;
    }

    static kernetile::text::ITextEngine& engine() {
        static std::unique_ptr<kernetile::text::ITextEngine> eng = kernetile::text::create_text_engine();
        return *eng;
    }
};

} // namespace ktui
