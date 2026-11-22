#pragma once

#include "kernetile/text/text_types.hpp"
#include <string_view>
#include <memory>

namespace kernetile::text {

class ITextEngine {
public:
    virtual ~ITextEngine() = default;

    // 复杂文本排版与塑形 (Shaping)
    [[nodiscard]] virtual TextLayoutResult shape_text(
        std::string_view utf8_text, 
        float font_size, 
        std::string_view font_family = "") = 0;

    // 单字形矢量轮廓光栅化 (Rasterize)
    [[nodiscard]] virtual FontRasterResult rasterize_glyph(
        uint32_t glyph_id, 
        float font_size,
        bool enable_stem_darkening = true) = 0;

    // 获取特定字号的基线与行度规参数
    [[nodiscard]] virtual FontMetrics get_metrics(
        float font_size, 
        std::string_view font_family = "") = 0;
};

// 工厂函数: 创建 Windows DirectWrite 原生硬件文本引擎 (0 MB 额外依赖)
[[nodiscard]] std::unique_ptr<ITextEngine> create_directwrite_engine();

// 自动工厂: 根据当前操作系统与驱动环境选择最优引擎
[[nodiscard]] std::unique_ptr<ITextEngine> create_text_engine();

ITextEngine& shared_text_engine();
std::shared_ptr<const TextLayoutResult> layout_text(std::string_view text, float size);
std::vector<size_t> grapheme_boundaries(std::string_view text);

} // namespace kernetile::text
