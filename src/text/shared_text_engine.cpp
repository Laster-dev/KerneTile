#include "kernetile/text/font_engine.hpp"
#include "builtin_text_engine.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ot.h>
#include <utf8proc.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <unordered_map>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace kernetile::text {
namespace {

constexpr uint32_t glyph_index_marker = 1u << 31;

struct Character {
    uint32_t codepoint;
    size_t offset;
};

std::vector<Character> decode(std::string_view text) {
    std::vector<Character> result;
    size_t offset = 0;
    while (offset < text.size()) {
        utf8proc_int32_t ch = 0;
        auto size = utf8proc_iterate(reinterpret_cast<const uint8_t*>(text.data() + offset),
                                     static_cast<utf8proc_ssize_t>(text.size() - offset), &ch);
        result.push_back({size > 0 ? static_cast<uint32_t>(ch) : 0xfffdu, offset});
        offset += size > 0 ? static_cast<size_t>(size) : 1;
    }
    return result;
}

std::filesystem::path font_path() {
    std::filesystem::path executable;
#ifdef _WIN32
    std::array<wchar_t, 32768> path{};
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (size && size < path.size()) executable = std::wstring_view(path.data(), size);
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
    std::error_code error;
    executable = std::filesystem::read_symlink("/proc/self/exe", error);
#endif
    if (!executable.empty()) {
        auto adjacent = executable.parent_path() / "ktui-font.otf";
        std::error_code error;
        if (std::filesystem::exists(adjacent, error)) return adjacent;
    }
    return KT_UI_FONT_PATH;
}

class SharedTextEngine final : public ITextEngine {
public:
    SharedTextEngine() {
        std::ifstream input(font_path(), std::ios::binary | std::ios::ate);
        const auto size = input.tellg();
        if (size <= 0) throw std::runtime_error("Cannot open the KTUI font");
        m_data.resize(static_cast<size_t>(size));
        input.seekg(0);
        if (!input.read(m_data.data(), size)) throw std::runtime_error("Cannot read the KTUI font");
        if (FT_Init_FreeType(&m_library)) throw std::runtime_error("Cannot initialize FreeType");
        if (FT_New_Memory_Face(m_library, reinterpret_cast<const FT_Byte*>(m_data.data()),
                               static_cast<FT_Long>(m_data.size()), 0, &m_face)) {
            FT_Done_FreeType(m_library);
            throw std::runtime_error("Cannot open the KTUI font face");
        }
        auto* blob = hb_blob_create(m_data.data(), static_cast<unsigned>(m_data.size()),
                                    HB_MEMORY_MODE_READONLY, nullptr, nullptr);
        auto* face = hb_face_create(blob, 0);
        m_font = hb_font_create(face);
        hb_ot_font_set_funcs(m_font);
        hb_face_destroy(face);
        hb_blob_destroy(blob);
    }

    ~SharedTextEngine() override {
        hb_font_destroy(m_font);
        FT_Done_Face(m_face);
        FT_Done_FreeType(m_library);
    }

    FontMetrics get_metrics(float size, std::string_view) override {
        const float scale = size / static_cast<float>(m_face->units_per_EM);
        return {size, scale * m_face->ascender, scale * m_face->descender, scale * m_face->height};
    }

    TextLayoutResult shape_text(std::string_view text, float size, std::string_view family) override {
        if (text.find_first_of("\r\n") != std::string_view::npos) {
            return shape_multiline(text, size, family);
        }
        TextLayoutResult result;
        result.metrics = get_metrics(size, family);
        result.total_height = result.metrics.line_height;
        const auto characters = decode(text);
        const auto boundaries = grapheme_boundaries(text);
        result.carets.reserve(boundaries.size());
        for (auto byte : boundaries) result.carets.push_back({byte, 0.0f});

        auto script_for = [](uint32_t ch) {
            return hb_unicode_script(hb_unicode_funcs_get_default(), ch);
        };
        auto strong_script = [](hb_script_t script) {
            return script != HB_SCRIPT_COMMON && script != HB_SCRIPT_INHERITED && script != HB_SCRIPT_UNKNOWN;
        };
        hb_font_set_scale(m_font, std::lround(size * 64), std::lround(size * 64));
        size_t first = 0;
        while (first < characters.size()) {
            hb_script_t script = HB_SCRIPT_COMMON;
            size_t last = first;
            while (last < characters.size()) {
                const auto candidate = script_for(characters[last].codepoint);
                if (strong_script(candidate)) {
                    if (strong_script(script) && script != candidate &&
                        std::binary_search(boundaries.begin(), boundaries.end(), characters[last].offset)) break;
                    if (!strong_script(script)) script = candidate;
                }
                ++last;
            }
            const size_t end_byte = last == characters.size() ? text.size() : characters[last].offset;
            shape_run(text, characters[first].offset, end_byte, script, result);
            first = last;
        }
        return result;
    }

    FontRasterResult rasterize_glyph(uint32_t id, float size, bool) override {
        const auto index = id & glyph_index_marker ? id & ~glyph_index_marker : FT_Get_Char_Index(m_face, id);
        if (FT_Set_Char_Size(m_face, 0, std::lround(size * 64), 72, 72) ||
            FT_Load_Glyph(m_face, index, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) ||
            FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL)) return {};
        const auto& glyph = *m_face->glyph;
        const auto& bitmap = glyph.bitmap;
        FontRasterResult result{bitmap.width, bitmap.rows, glyph.bitmap_left, glyph.bitmap_top, {}};
        if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY && bitmap.width && bitmap.rows) {
            result.r8_bitmap.resize(static_cast<size_t>(bitmap.width) * bitmap.rows);
            for (uint32_t row = 0; row < bitmap.rows; ++row) {
                const auto source_row = bitmap.pitch >= 0 ? row : bitmap.rows - row - 1;
                const auto* source = bitmap.buffer + size_t(source_row) * std::abs(bitmap.pitch);
                std::copy_n(source, bitmap.width, result.r8_bitmap.data() + size_t(row) * bitmap.width);
            }
        }
        return result;
    }

private:
    TextLayoutResult shape_multiline(std::string_view text, float size, std::string_view family) {
        TextLayoutResult result;
        result.metrics = get_metrics(size, family);
        std::unordered_map<std::string_view, TextLayoutResult> lines;
        size_t start = 0;
        while (true) {
            const auto end = text.find_first_of("\r\n", start);
            const auto line = text.substr(start, end == std::string_view::npos ? end : end - start);
            auto [entry, inserted] = lines.try_emplace(line);
            if (inserted) entry->second = shape_text(line, size, family);
            const auto& layout = entry->second;
            result.total_width = std::max(result.total_width, layout.total_width);
            for (auto glyph : layout.glyphs) {
                glyph.y += result.total_height;
                glyph.cluster += start;
                result.glyphs.push_back(glyph);
            }
            for (auto caret : layout.carets) {
                caret.byte += start;
                result.carets.push_back(caret);
            }
            result.total_height += layout.total_height;
            if (end == std::string_view::npos) break;
            start = end + 1;
            if (text[end] == '\r' && start < text.size() && text[start] == '\n') ++start;
        }
        return result;
    }

    void shape_run(std::string_view text, size_t start, size_t end, hb_script_t script, TextLayoutResult& result) {
        auto* buffer = hb_buffer_create();
        hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
        const auto direction = hb_script_get_horizontal_direction(script);
        hb_buffer_set_direction(buffer, direction == HB_DIRECTION_RTL ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer, script);
        hb_buffer_set_language(buffer, hb_language_from_string("und", -1));
        hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.size()),
                           static_cast<unsigned>(start), static_cast<int>(end - start));
        hb_shape(m_font, buffer, nullptr, 0);

        struct Cluster { float left; float right; };
        std::map<size_t, Cluster> clusters;
        unsigned count = 0;
        const auto* info = hb_buffer_get_glyph_infos(buffer, &count);
        const auto* positions = hb_buffer_get_glyph_positions(buffer, nullptr);
        float x = result.total_width;
        for (unsigned i = 0; i < count; ++i) {
            const float advance = positions[i].x_advance / 64.0f;
            auto [cluster, added] = clusters.try_emplace(info[i].cluster, Cluster{x, x + advance});
            if (!added) cluster->second.right = x + advance;
            GlyphPlacement glyph{};
            glyph.glyph_id = glyph_index_marker | info[i].codepoint;
            glyph.x = x + positions[i].x_offset / 64.0f;
            glyph.y = result.metrics.ascender - positions[i].y_offset / 64.0f;
            glyph.advance_x = advance;
            glyph.advance_y = positions[i].y_advance / 64.0f;
            glyph.cluster = info[i].cluster;
            hb_glyph_extents_t extents{};
            if (hb_font_get_glyph_extents(m_font, info[i].codepoint, &extents)) {
                glyph.width = extents.width / 64.0f;
                glyph.height = -extents.height / 64.0f;
                glyph.bearing_x = extents.x_bearing / 64.0f;
                glyph.bearing_y = extents.y_bearing / 64.0f;
            }
            result.glyphs.push_back(glyph);
            x += advance;
        }
        hb_buffer_destroy(buffer);
        result.total_width = x;

        for (auto it = clusters.begin(); it != clusters.end(); ++it) {
            const auto next = std::next(it);
            const auto stop = next == clusters.end() ? end : next->first;
            auto begin = std::lower_bound(result.carets.begin(), result.carets.end(), it->first,
                [](const CaretStop& caret, size_t byte) { return caret.byte < byte; });
            auto limit = std::upper_bound(begin, result.carets.end(), stop,
                [](size_t byte, const CaretStop& caret) { return byte < caret.byte; });
            const auto divisions = std::max<ptrdiff_t>(1, limit - begin - 1);
            const auto [left, right] = it->second;
            for (auto caret = begin; caret != limit; ++caret) {
                const float fraction = static_cast<float>(caret - begin) / static_cast<float>(divisions);
                caret->x = direction == HB_DIRECTION_RTL ? std::lerp(right, left, fraction)
                                                         : std::lerp(left, right, fraction);
            }
        }
    }

    std::vector<char> m_data;
    FT_Library m_library{};
    FT_Face m_face{};
    hb_font_t* m_font{};
};
} // namespace

std::vector<size_t> grapheme_boundaries(std::string_view text) {
    std::vector<size_t> result{0};
    const auto characters = decode(text);
    utf8proc_int32_t state = 0;
    for (size_t i = 1; i < characters.size(); ++i) {
        if (utf8proc_grapheme_break_stateful(characters[i - 1].codepoint, characters[i].codepoint, &state))
            result.push_back(characters[i].offset);
    }
    if (!text.empty()) result.push_back(text.size());
    return result;
}

std::unique_ptr<ITextEngine> create_text_engine() {
    try {
        return std::make_unique<SharedTextEngine>();
    } catch (const std::runtime_error&) {
        return create_builtin_engine();
    }
}

ITextEngine& shared_text_engine() {
    static thread_local auto engine = create_text_engine();
    return *engine;
}

std::shared_ptr<const TextLayoutResult> layout_text(std::string_view text, float size) {
    struct Entry {
        std::string text;
        float size;
        std::shared_ptr<const TextLayoutResult> layout;
    };
    static thread_local std::list<Entry> cache;
    auto hit = std::find_if(cache.begin(), cache.end(), [&](const auto& entry) {
        return entry.size == size && entry.text == text;
    });
    if (hit != cache.end()) {
        cache.splice(cache.begin(), cache, hit);
        return cache.front().layout;
    }
    auto layout = std::make_shared<TextLayoutResult>(shared_text_engine().shape_text(text, size));
    if (layout->carets.empty()) {
        const auto boundaries = grapheme_boundaries(text);
        const float step = boundaries.size() > 1 ? layout->total_width / (boundaries.size() - 1) : 0.0f;
        for (size_t i = 0; i < boundaries.size(); ++i) layout->carets.push_back({boundaries[i], i * step});
    }
    if (text.size() <= 16384) {
        cache.push_front({std::string(text), size, layout});
        if (cache.size() > 128) cache.pop_back();
    }
    return layout;
}

} // namespace kernetile::text
