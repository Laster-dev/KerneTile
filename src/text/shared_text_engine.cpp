#include "kernetile/text/font_engine.hpp"

#include <algorithm>
#include <list>

namespace kernetile::text {

ITextEngine& shared_text_engine() {
    static thread_local auto engine = create_text_engine();
    return *engine;
}

std::shared_ptr<const TextLayoutResult> layout_text(std::string_view text, float size, uint16_t font_weight, bool italic) {
    struct Entry {
        std::string text;
        float size;
        uint16_t weight;
        bool italic;
        std::shared_ptr<const TextLayoutResult> layout;
    };
    static thread_local std::list<Entry> cache;
    auto hit = std::find_if(cache.begin(), cache.end(), [&](const auto& entry) {
        return entry.size == size && entry.weight == font_weight && entry.italic == italic && entry.text == text;
    });
    if (hit != cache.end()) {
        cache.splice(cache.begin(), cache, hit);
        return cache.front().layout;
    }
    auto layout = std::make_shared<TextLayoutResult>();
    size_t start = 0;
    while (true) {
        const auto end = text.find_first_of("\r\n", start);
        const auto line = text.substr(start, end == std::string_view::npos ? end : end - start);
        auto shaped = shared_text_engine().shape_text(line, size, "", font_weight, italic);
        layout->metrics = shaped.metrics;
        if (shaped.carets.empty()) {
            auto boundaries = grapheme_boundaries(line);
            float x = 0.0f;
            size_t glyph = 0;
            size_t byte = 0;
            for (auto boundary : boundaries) {
                while (byte < boundary && glyph < shaped.glyphs.size()) {
                    ++byte;
                    while (byte < line.size() && (static_cast<uint8_t>(line[byte]) & 0xc0) == 0x80) ++byte;
                    x += shaped.glyphs[glyph++].advance_x;
                }
                shaped.carets.push_back({boundary, x});
            }
        }
        for (auto glyph : shaped.glyphs) {
            glyph.y += layout->total_height;
            glyph.cluster += start;
            layout->glyphs.push_back(glyph);
        }
        for (auto caret : shaped.carets) layout->carets.push_back({caret.byte + start, caret.x});
        layout->total_width = std::max(layout->total_width, shaped.total_width);
        layout->total_height += shaped.total_height;
        if (end == std::string_view::npos) break;
        start = end + 1;
        if (text[end] == '\r' && start < text.size() && text[start] == '\n') ++start;
    }
    if (text.size() <= 16384) {
        cache.push_front({std::string(text), size, font_weight, italic, layout});
        if (cache.size() > 128) cache.pop_back();
    }
    return layout;
}

} // namespace kernetile::text
