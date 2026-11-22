#pragma once

#include <ktui/pal/platform_types.hpp>
#include <kernetile/text/font_engine.hpp>
#include <algorithm>
#include <cmath>
#include <string_view>

namespace ktui {

class TextMeasure {
public:
    static Vec2 measure(std::string_view text, float font_size) {
        const auto layout = kernetile::text::layout_text(text, font_size);
        return {layout->total_width, layout->total_height};
    }

    static float measure_width(std::string_view text, float font_size) {
        return measure(text, font_size).x;
    }

    static float cursor_x(std::string_view text, float font_size, size_t byte) {
        const auto layout = kernetile::text::layout_text(text, font_size);
        const auto it = std::upper_bound(layout->carets.begin(), layout->carets.end(), byte,
            [](size_t offset, const kernetile::text::CaretStop& stop) { return offset < stop.byte; });
        return it == layout->carets.begin() ? 0.0f : std::prev(it)->x;
    }

    static size_t hit_test_cursor(std::string_view text, float font_size, float x) {
        const auto layout = kernetile::text::layout_text(text, font_size);
        const auto it = std::min_element(layout->carets.begin(), layout->carets.end(), [x](auto a, auto b) {
            return std::abs(a.x - x) < std::abs(b.x - x);
        });
        return it == layout->carets.end() ? 0 : it->byte;
    }
};

} // namespace ktui
