#include "kernetile/text/font_engine.hpp"
#include <emscripten.h>
#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace kernetile::text {
namespace {

EM_JS(size_t, segment, (const char* source, size_t length, size_t* boundaries), {
    var text = new TextDecoder().decode(HEAPU8.subarray(source, source + length));
    if (typeof Intl.Segmenter === 'function' && !Module['ktSegmenter'])
        Module['ktSegmenter'] = new Intl.Segmenter(undefined, {granularity: 'grapheme'});
    var chunks = Module['ktSegmenter']
        ? Array.from(Module['ktSegmenter'].segment(text), item => item.segment)
        : Array.from(text);
    var offset = 0;
    HEAPU32[boundaries >> 2] = 0;
    for (var i = 0; i < chunks.length; ++i) {
        offset += lengthBytesUTF8(chunks[i]);
        HEAPU32[(boundaries >> 2) + i + 1] = offset;
    }
    return chunks.length + 1;
});

EM_JS(void, measure, (const char* source, float size, int weight, int italic, float* values), {
    var canvas = Module['ktTextCanvas'];
    if (!canvas && typeof document !== 'undefined')
        canvas = Module['ktTextCanvas'] = document.createElement('canvas');
    var text = UTF8ToString(source);
    var ctx = canvas && canvas.getContext('2d', {willReadFrequently: true});
    if (ctx) {
        ctx.font = (italic ? 'italic ' : '') + (weight > 0 ? weight : 400) + ' ' + size + 'px "Microsoft YaHei UI", "Segoe UI", system-ui, sans-serif';
        var metrics = ctx.measureText(text);
        HEAPF32[values >> 2] = metrics.width;
        HEAPF32[(values >> 2) + 1] = metrics.fontBoundingBoxAscent ?? size * 0.8;
        HEAPF32[(values >> 2) + 2] = metrics.fontBoundingBoxDescent ?? size * 0.2;
    } else {
        HEAPF32[values >> 2] = Array.from(text).length * size * 0.55;
        HEAPF32[(values >> 2) + 1] = size * 0.8;
        HEAPF32[(values >> 2) + 2] = size * 0.2;
    }
});

EM_JS(int, raster, (const char* source, float size, int weight, int italic, int* box, uint8_t* dest, size_t capacity), {
    var canvas = Module['ktTextCanvas'];
    if (!canvas && typeof document !== 'undefined')
        canvas = Module['ktTextCanvas'] = document.createElement('canvas');
    if (!canvas || (!dest && capacity)) return 0;
    var text = UTF8ToString(source);
    var ctx = canvas.getContext('2d', {willReadFrequently: true});
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    var fontSpec = (italic ? 'italic ' : '') + (weight > 0 ? weight : 400) + ' ' + size + 'px "Microsoft YaHei UI", "Segoe UI", system-ui, sans-serif';
    ctx.font = fontSpec;
    var m = ctx.measureText(text);
    var left = Math.floor(-m.actualBoundingBoxLeft);
    var top = Math.floor(-m.actualBoundingBoxAscent);
    var width = Math.ceil(m.actualBoundingBoxRight) - left;
    var height = Math.ceil(m.actualBoundingBoxDescent) - top;
    if (!(width > 0 && height > 0) || width > 2048 || height > 2048) return 0;
    HEAP32[box >> 2] = width;
    HEAP32[(box >> 2) + 1] = height;
    HEAP32[(box >> 2) + 2] = left;
    HEAP32[(box >> 2) + 3] = -top;
    if (capacity < width * height) return width * height;

    // 基于 devicePixelRatio 的离屏 Canvas 超采样 (2x~4x 渲染后盒式滤波降采样至目标物理像素尺寸，提升小字清晰度与边缘锐度)
    var dpr = (typeof window !== 'undefined' && window.devicePixelRatio) ? window.devicePixelRatio : 1;
    var ss = Math.max(2, Math.min(4, Math.ceil(dpr * 1.5)));
    while (ss > 1 && (width * ss > 4096 || height * ss > 4096)) ss--;

    var sw = width * ss;
    var sh = height * ss;
    canvas.width = sw;
    canvas.height = sh;
    ctx.setTransform(ss, 0, 0, ss, 0, 0);
    ctx.font = fontSpec;
    // 使用纯黑 #000 而非 #fff：Chromium/Skia 在绘制白字时会套用白底反转 Gamma 增粗表导致 Alpha 偏厚偏黑，
    // 而 #000 走正常线性覆盖率表，与 Windows DirectWrite CreateAlphaTexture 输出的字重几乎一致
    ctx.fillStyle = '#000';
    ctx.fillText(text, -left, -top);
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    var data = ctx.getImageData(0, 0, sw, sh).data;
    if (ss === 1) {
        for (var i = 0; i < width * height; ++i) HEAPU8[dest + i] = data[i * 4 + 3];
    } else {
        var area = ss * ss;
        var half = area >> 1;
        for (var y = 0; y < height; ++y) {
            var baseY = y * ss;
            var rowOut = dest + y * width;
            for (var x = 0; x < width; ++x) {
                var baseX = x * ss;
                var sum = 0;
                for (var sy = 0; sy < ss; ++sy) {
                    var rowIdx = ((baseY + sy) * sw + baseX) * 4 + 3;
                    for (var sx = 0; sx < ss; ++sx) {
                        sum += data[rowIdx + sx * 4];
                    }
                }
                HEAPU8[rowOut + x] = (sum + half) / area | 0;
            }
        }
    }
    return width * height;
});

class BrowserTextEngine final : public ITextEngine {
public:
    FontMetrics get_metrics(float size, std::string_view, uint16_t font_weight, bool italic) override {
        float values[3]{};
        measure("M", size, static_cast<int>(font_weight > 0 ? font_weight : 400), italic ? 1 : 0, values);
        return {size, values[1], -values[2], values[1] + values[2]};
    }

    TextLayoutResult shape_text(std::string_view text, float size, std::string_view family, uint16_t font_weight, bool italic) override {
        const uint16_t w = font_weight > 0 ? font_weight : 400;
        TextLayoutResult result;
        result.metrics = get_metrics(size, family, w, italic);
        result.total_height = result.metrics.line_height;
        const auto stops = grapheme_boundaries(text);
        for (size_t i = 1; i < stops.size(); ++i) {
            const auto offset = stops[i - 1];
            const std::string cluster(text.substr(offset, stops[i] - offset));
            std::string key = std::to_string(w) + (italic ? 'I' : 'N') + '\x1F' + cluster;
            auto [it, inserted] = m_ids.try_emplace(key, glyph_base + static_cast<uint32_t>(m_clusters.size()));
            if (inserted) m_clusters.push_back(ClusterKey{cluster, w, italic});
            float values[3]{};
            measure(cluster.c_str(), size, static_cast<int>(w), italic ? 1 : 0, values);
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
        uint16_t weight = 400;
        bool italic = false;
        if (id >= glyph_base && id - glyph_base < m_clusters.size()) {
            text = m_clusters[id - glyph_base].text;
            weight = m_clusters[id - glyph_base].weight;
            italic = m_clusters[id - glyph_base].italic;
        } else if (id > 0 && id < 128) {
            text.push_back(static_cast<char>(id));
        }
        if (text.empty()) return {};
        int box[4]{};
        const int count = raster(text.c_str(), size, static_cast<int>(weight), italic ? 1 : 0, box, nullptr, 0);
        if (!count) return {};
        FontRasterResult result{static_cast<uint32_t>(box[0]), static_cast<uint32_t>(box[1]),
                                box[2], box[3], std::vector<uint8_t>(count)};
        raster(text.c_str(), size, static_cast<int>(weight), italic ? 1 : 0, box, result.r8_bitmap.data(), result.r8_bitmap.size());
        return result;
    }

private:
    struct ClusterKey { std::string text; uint16_t weight; bool italic; };
    static constexpr uint32_t glyph_base = 1u << 31;
    std::unordered_map<std::string, uint32_t> m_ids;
    std::vector<ClusterKey> m_clusters;
};
} // namespace

std::unique_ptr<ITextEngine> create_text_engine() { return std::make_unique<BrowserTextEngine>(); }
std::unique_ptr<ITextEngine> create_directwrite_engine() { return nullptr; }

std::vector<size_t> grapheme_boundaries(std::string_view text) {
    if (text.empty()) return {0};
    std::vector<size_t> result(text.size() + 1);
    result.resize(segment(text.data(), text.size(), result.data()));
    result.back() = text.size();
    return result;
}
} // namespace kernetile::text
