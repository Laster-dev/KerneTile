#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace kernetile::text {

struct GlyphPlacement {
    uint32_t glyph_id{0};       // 字符代码点或字体内部 Glyph Index
    float x{0.0f};               // 局部排版起始 X 坐标
    float y{0.0f};               // 基线 Y 坐标
    float advance_x{0.0f};       // 水平推进步长
    float advance_y{0.0f};       // 垂直推进步长
    float width{0.0f};           // 光栅化字形宽度 (像素)
    float height{0.0f};          // 光栅化字形高度 (像素)
    float bearing_x{0.0f};       // 水平前导距离 (Bearing X)
    float bearing_y{0.0f};       // 垂直基线距离 (Bearing Y, 从基线向上为正)
    size_t cluster{0};
};

struct FontMetrics {
    float font_size{16.0f};
    float ascender{13.0f};       // 上行高度 (基线到字形最高点)
    float descender{-3.0f};      // 下行深度 (基线到字形最低点，通常为负值)
    float line_height{18.0f};    // 推荐行高
};

struct CaretStop {
    size_t byte;
    float x;
};

struct TextLayoutResult {
    std::vector<GlyphPlacement> glyphs;
    float total_width{0.0f};
    float total_height{0.0f};
    FontMetrics metrics;
    std::vector<CaretStop> carets;
};

struct FontRasterResult {
    uint32_t width{0};           // 光栅化点阵宽度
    uint32_t height{0};          // 光栅化点阵高度
    int32_t bearing_x{0};        // 左侧偏移
    int32_t bearing_y{0};        // 顶部相对于基线的偏移 (向上为正)
    std::vector<uint8_t> r8_bitmap; // 8-bit 单通道灰度覆盖率点阵 (0~255)
};

} // namespace kernetile::text
