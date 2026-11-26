#include "kernetile/text/font_engine.hpp"
#include <pango/pangocairo.h>
#include <dlfcn.h>
#include <algorithm>
#include <cmath>

namespace kernetile::text {
namespace {
float pixels(int value) { return static_cast<float>(value) / PANGO_SCALE; }

template<typename T> void unref(T* object) { if (object) g_object_unref(object); }
template<typename T> using GObjectPtr = std::unique_ptr<T, decltype(&unref<T>)>;

std::string desktop_font() {
    // GTK reads the desktop's XSettings / GSettings when it is available.
    // Keep the system module open after registering its GObject types.
    static const auto name = [] {
        auto* gtk = dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_LOCAL);
        if (!gtk) return std::string{};
        const auto init = reinterpret_cast<gboolean (*)(int*, char***)>(dlsym(gtk, "gtk_init_check"));
        const auto settings = reinterpret_cast<GObject* (*)()>(dlsym(gtk, "gtk_settings_get_default"));
        if (!init || !settings || !init(nullptr, nullptr)) return std::string{};
        auto* object = settings();
        if (!object) return std::string{};
        char* value = nullptr;
        g_object_get(object, "gtk-font-name", &value, nullptr);
        std::string result = value ? value : "";
        g_free(value);
        return result;
    }();
    return name;
}

class SystemTextEngine final : public ITextEngine {
public:
    SystemTextEngine() {
        const auto font = desktop_font();
        if (!font.empty()) {
            auto* desc = pango_font_description_from_string(font.c_str());
            pango_context_set_font_description(m_context.get(), desc);
            pango_font_description_free(desc);
        }
    }

    FontMetrics get_metrics(float size, std::string_view family, uint16_t font_weight, bool italic) override {
        auto layout = make_layout("M", size, family, font_weight, italic);
        return metrics(layout.get(), size);
    }

    TextLayoutResult shape_text(std::string_view text, float size, std::string_view family, uint16_t font_weight, bool italic, bool tabular_nums = false) override {
        auto layout = make_layout(text, size, family, font_weight, italic, tabular_nums);
        TextLayoutResult result;
        result.metrics = metrics(layout.get(), size);
        PangoRectangle logical{};
        pango_layout_get_extents(layout.get(), nullptr, &logical);
        result.total_width = pixels(logical.width);
        result.total_height = result.metrics.line_height;
        auto* iter = pango_layout_get_iter(layout.get());
        do {
            auto* run = pango_layout_iter_get_run_readonly(iter);
            if (!run) continue;
            pango_layout_iter_get_run_extents(iter, nullptr, &logical);
            float x = pixels(logical.x);
            const float baseline = pixels(pango_layout_iter_get_baseline(iter));
            for (int i = 0; i < run->glyphs->num_glyphs; ++i) {
                const auto& info = run->glyphs->glyphs[i];
                GlyphPlacement glyph{};
                glyph.glyph_id = remember(run->item->analysis.font, info.glyph);
                glyph.x = x + pixels(info.geometry.x_offset);
                glyph.y = baseline - pixels(info.geometry.y_offset);
                glyph.advance_x = pixels(info.geometry.width);
                glyph.cluster = run->item->offset + run->glyphs->log_clusters[i];
                result.glyphs.push_back(glyph);
                x += glyph.advance_x;
            }
        } while (pango_layout_iter_next_run(iter));
        pango_layout_iter_free(iter);
        for (const auto byte : grapheme_boundaries(text)) {
            PangoRectangle caret{};
            pango_layout_get_cursor_pos(layout.get(), static_cast<int>(byte), &caret, nullptr);
            result.carets.push_back({byte, pixels(caret.x)});
        }
        return result;
    }

    FontRasterResult rasterize_glyph(uint32_t id, float size, bool) override {
        if (id < glyph_base) {
            char text[6]{};
            int length = g_unichar_to_utf8(id, text);
            auto shaped = shape_text({text, static_cast<size_t>(length)}, size, {}, 400, false, false);
            if (shaped.glyphs.empty()) return {};
            id = shaped.glyphs.front().glyph_id;
        }
        if (id - glyph_base >= m_glyphs.size()) return {};
        const auto& glyph = m_glyphs[id - glyph_base];
        auto* desc = pango_font_describe_with_absolute_size(glyph.font.get());
        pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
        GObjectPtr<PangoFont> font(pango_context_load_font(m_context.get(), desc), unref<PangoFont>);
        pango_font_description_free(desc);
        if (!font) return {};
        PangoRectangle ink{};
        pango_font_get_glyph_extents(font.get(), glyph.index, &ink, nullptr);
        pango_extents_to_pixels(&ink, nullptr);
        if (ink.width <= 0 || ink.height <= 0) return {};
        auto* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, ink.width, ink.height);
        auto* cr = cairo_create(surface);
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
        cairo_move_to(cr, -ink.x, -ink.y);
        auto* glyphs = pango_glyph_string_new();
        pango_glyph_string_set_size(glyphs, 1);
        glyphs->glyphs[0] = {};
        glyphs->glyphs[0].glyph = glyph.index;
        pango_cairo_show_glyph_string(cr, font.get(), glyphs);
        pango_glyph_string_free(glyphs);
        cairo_surface_flush(surface);
        FontRasterResult result{static_cast<uint32_t>(ink.width), static_cast<uint32_t>(ink.height),
                                ink.x, -ink.y, {}};
        if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
            result.r8_bitmap.resize(size_t(ink.width) * ink.height);
            const auto* data = cairo_image_surface_get_data(surface);
            const int stride = cairo_image_surface_get_stride(surface);
            for (int row = 0; row < ink.height; ++row)
                std::copy_n(data + size_t(row) * stride, ink.width, result.r8_bitmap.data() + size_t(row) * ink.width);
        }
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return result;
    }

private:
    static constexpr uint32_t glyph_base = 1u << 31;
    struct Glyph { GObjectPtr<PangoFont> font; PangoGlyph index; };

    uint32_t remember(PangoFont* font, PangoGlyph index) {
        auto match = std::find_if(m_glyphs.begin(), m_glyphs.end(), [=](const Glyph& glyph) {
            return glyph.font.get() == font && glyph.index == index;
        });
        if (match != m_glyphs.end()) return glyph_base + static_cast<uint32_t>(match - m_glyphs.begin());
        m_glyphs.push_back({GObjectPtr<PangoFont>(static_cast<PangoFont*>(g_object_ref(font)), unref<PangoFont>), index});
        return glyph_base + static_cast<uint32_t>(m_glyphs.size() - 1);
    }

    GObjectPtr<PangoLayout> make_layout(std::string_view text, float size, std::string_view family, uint16_t font_weight, bool italic, bool tabular_nums = false) {
        GObjectPtr<PangoLayout> layout(pango_layout_new(m_context.get()), unref<PangoLayout>);
        auto* desc = pango_font_description_copy(pango_context_get_font_description(m_context.get()));
        if (!family.empty()) pango_font_description_set_family(desc, std::string(family).c_str());
        pango_font_description_set_weight(desc, static_cast<PangoWeight>(font_weight > 0 ? font_weight : 400));
        pango_font_description_set_style(desc, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
        pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
        pango_layout_set_font_description(layout.get(), desc);
        pango_font_description_free(desc);
        pango_layout_set_text(layout.get(), text.empty() ? "" : text.data(), static_cast<int>(text.size()));
        if (tabular_nums && !text.empty()) {
            if (auto* attrs = pango_attr_list_new()) {
                pango_attr_list_insert(attrs, pango_attr_font_features_new("tnum=1"));
                pango_layout_set_attributes(layout.get(), attrs);
                pango_attr_list_unref(attrs);
            }
        }
        return layout;
    }

    static FontMetrics metrics(PangoLayout* layout, float size) {
        PangoRectangle extents{};
        pango_layout_get_extents(layout, nullptr, &extents);
        const float baseline = pixels(pango_layout_get_baseline(layout));
        const float height = pixels(extents.height);
        return {size, baseline, baseline - height, height};
    }

    GObjectPtr<PangoContext> m_context{pango_font_map_create_context(pango_cairo_font_map_get_default()), unref<PangoContext>};
    std::vector<Glyph> m_glyphs;
};
} // namespace

std::unique_ptr<ITextEngine> create_text_engine() { return std::make_unique<SystemTextEngine>(); }
std::unique_ptr<ITextEngine> create_directwrite_engine() { return nullptr; }

std::vector<size_t> grapheme_boundaries(std::string_view text) {
    if (text.empty()) return {0};
    const auto count = static_cast<size_t>(g_utf8_strlen(text.data(), text.size())) + 1;
    std::vector<PangoLogAttr> attrs(count);
    pango_get_log_attrs(text.data(), static_cast<int>(text.size()), -1, pango_language_get_default(),
                        attrs.data(), static_cast<int>(count));
    std::vector<size_t> result;
    const char* current = text.data();
    for (size_t i = 0; i + 1 < count; ++i) {
        if (attrs[i].is_cursor_position) result.push_back(current - text.data());
        current = g_utf8_next_char(current);
    }
    if (result.empty() || result.front() != 0) result.insert(result.begin(), 0);
    result.push_back(text.size());
    return result;
}

} // namespace kernetile::text
