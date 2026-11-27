#include "kernetile/text/font_engine.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <list>
#include <unordered_map>

namespace kernetile::text {

namespace {

constexpr size_t kTextLayoutCacheCapacity = 4096;

uint64_t hash_layout_key(std::string_view text, std::string_view family,
                         float size, uint16_t weight, bool italic, bool tabular_nums) noexcept {
    uint64_t h = 0xcbf29ce484222325ULL;
    auto mix_u64 = [&h](uint64_t v) noexcept {
        h ^= v;
        h *= 0x100000001b3ULL;
    };
    auto mix_sv = [&mix_u64](std::string_view sv) noexcept {
        mix_u64(static_cast<uint64_t>(sv.size()));
        for (unsigned char c : sv) {
            mix_u64(static_cast<uint64_t>(c));
        }
    };
    uint32_t size_bits = 0;
    std::memcpy(&size_bits, &size, sizeof(size_bits));
    uint64_t meta = (static_cast<uint64_t>(size_bits) << 32u) |
                    (static_cast<uint64_t>(weight) << 8u) |
                    (italic ? 2u : 0u) |
                    (tabular_nums ? 1u : 0u);
    mix_u64(meta);
    mix_sv(family);
    mix_sv(text);
    return h;
}

} // namespace

ITextEngine& shared_text_engine() {
    static thread_local auto engine = create_text_engine();
    return *engine;
}

std::shared_ptr<const TextLayoutResult> layout_text(std::string_view text, float size,
                                                    uint16_t font_weight, bool italic,
                                                    std::string_view font_family,
                                                    bool tabular_nums) {
    struct Entry {
        uint64_t hash;
        std::string text;
        std::string family;
        float size;
        uint16_t weight;
        bool italic;
        bool tabular_nums;
        std::shared_ptr<const TextLayoutResult> layout;
    };
    struct LayoutCache {
        std::list<Entry> lru;
        std::unordered_multimap<uint64_t, std::list<Entry>::iterator> index;
        LayoutCache() { index.reserve(kTextLayoutCacheCapacity); }
    };
    static thread_local LayoutCache cache;

    const uint64_t key_hash = hash_layout_key(text, font_family, size, font_weight, italic, tabular_nums);
    auto range = cache.index.equal_range(key_hash);
    for (auto it = range.first; it != range.second; ++it) {
        const auto& entry = *(it->second);
        if (entry.size == size && entry.weight == font_weight && entry.italic == italic &&
            entry.tabular_nums == tabular_nums && entry.family == font_family && entry.text == text) {
            cache.lru.splice(cache.lru.begin(), cache.lru, it->second);
            return entry.layout;
        }
    }
    auto layout = std::make_shared<TextLayoutResult>();
    size_t start = 0;
    while (true) {
        const auto end = text.find_first_of("\r\n", start);
        const auto line = text.substr(start, end == std::string_view::npos ? end : end - start);
        auto shaped = shared_text_engine().shape_text(line, size, font_family, font_weight, italic, tabular_nums);
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
        cache.lru.push_front(Entry{key_hash, std::string(text), std::string(font_family), size, font_weight, italic, tabular_nums, layout});
        cache.index.emplace(key_hash, cache.lru.begin());
        if (cache.lru.size() > kTextLayoutCacheCapacity) {
            auto last = std::prev(cache.lru.end());
            auto ev_range = cache.index.equal_range(last->hash);
            for (auto it = ev_range.first; it != ev_range.second; ++it) {
                if (it->second == last) {
                    cache.index.erase(it);
                    break;
                }
            }
            cache.lru.pop_back();
        }
    }
    return layout;
}

} // namespace kernetile::text
