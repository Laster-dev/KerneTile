#pragma once

#include <ktui/pal/font_resolver.hpp>
#include <ktui/pal/platform_types.hpp>
#include <kernetile/text/font_engine.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#define KTUI_TM_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define KTUI_TM_NOINLINE __attribute__((noinline))
#else
#define KTUI_TM_NOINLINE
#endif

namespace ktui {

class TextMeasure {
public:
    KTUI_TM_NOINLINE static Vec2 measure(std::string_view text, float font_size,
                        FontWeight weight = FontWeight::Regular, bool italic = false,
                        std::string_view font_family = "", bool tabular_nums = false) {
        const auto layout = kernetile::text::layout_text(
            text, font_size, static_cast<uint16_t>(weight), italic, font_family, tabular_nums);
        return {layout->total_width, layout->total_height};
    }

    KTUI_TM_NOINLINE static float measure_width(std::string_view text, float font_size,
                               FontWeight weight = FontWeight::Regular, bool italic = false,
                               std::string_view font_family = "", bool tabular_nums = false) {
        return measure(text, font_size, weight, italic, font_family, tabular_nums).x;
    }

    KTUI_TM_NOINLINE static float cursor_x(std::string_view text, float font_size, size_t byte,
                                           FontWeight weight = FontWeight::Regular, bool italic = false,
                                           std::string_view font_family = "", bool tabular_nums = false) {
        const auto layout = kernetile::text::layout_text(
            text, font_size, static_cast<uint16_t>(weight), italic, font_family, tabular_nums);
        const auto it = std::upper_bound(layout->carets.begin(), layout->carets.end(), byte,
            [](size_t offset, const kernetile::text::CaretStop& stop) { return offset < stop.byte; });
        return it == layout->carets.begin() ? 0.0f : std::prev(it)->x;
    }

    KTUI_TM_NOINLINE static size_t hit_test_cursor(std::string_view text, float font_size, float x,
                                                   FontWeight weight = FontWeight::Regular, bool italic = false,
                                                   std::string_view font_family = "", bool tabular_nums = false) {
        const auto layout = kernetile::text::layout_text(
            text, font_size, static_cast<uint16_t>(weight), italic, font_family, tabular_nums);
        const auto it = std::min_element(layout->carets.begin(), layout->carets.end(), [x](auto a, auto b) {
            return std::abs(a.x - x) < std::abs(b.x - x);
        });
        return it == layout->carets.end() ? 0 : it->byte;
    }

    KTUI_TM_NOINLINE static std::vector<std::string> wrap_lines(
        std::string_view text, float font_size, float max_width,
        FontWeight weight = FontWeight::Regular, bool italic = false,
        std::string_view font_family = "", bool tabular_nums = false)
    {
        std::vector<std::string> lines;
        if (text.empty()) return lines;
        if (!(max_width > 0.0f) || !std::isfinite(max_width)) {
            lines.emplace_back(text);
            return lines;
        }
        const auto stops = kernetile::text::grapheme_boundaries(text);
        size_t line_start = 0;
        size_t pos = 0;
        while (pos < text.size()) {
            auto it = std::upper_bound(stops.begin(), stops.end(), pos);
            size_t next = (it != stops.end()) ? *it : text.size();
            if (text[pos] == '\n' || text[pos] == '\r') {
                lines.emplace_back(text.substr(line_start, pos - line_start));
                if (text[pos] == '\r' && next < text.size() && text[next] == '\n') {
                    next = *std::upper_bound(stops.begin(), stops.end(), next);
                }
                line_start = next;
                pos = next;
                continue;
            }
            float w = measure_width(text.substr(line_start, next - line_start),
                                    font_size, weight, italic, font_family, tabular_nums);
            if (w > max_width && pos > line_start) {
                lines.emplace_back(text.substr(line_start, pos - line_start));
                line_start = pos;
            } else {
                pos = next;
            }
        }
        if (line_start < text.size()) {
            lines.emplace_back(text.substr(line_start));
        }
        return lines;
    }
};

} // namespace ktui
