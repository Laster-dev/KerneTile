#pragma once

#include "kernetile/text/font_engine.hpp"
#include <string>
#include <vector>
#include <memory>

namespace kernetile::text {

// 基于 stb_truetype 的跨平台高精度矢量字体引擎
// 100% 纯 C++ 实现，零外部动态库依赖，支持标准 TTF / TTC / OTF 矢量字库
class VectorTextEngine : public ITextEngine {
public:
    explicit VectorTextEngine(const std::string& font_path = "");
    ~VectorTextEngine() override;

    FontMetrics get_metrics(float font_size, std::string_view font_family = "") override;
    TextLayoutResult shape_text(std::string_view utf8_text, float font_size, std::string_view font_family = "") override;
    FontRasterResult rasterize_glyph(uint32_t glyph_id, float font_size, bool enable_stem_darkening = true) override;

    bool is_font_loaded() const noexcept { return m_font_loaded; }
    const std::string& get_loaded_font_path() const noexcept { return m_loaded_path; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_font_loaded{false};
    std::string m_loaded_path;
};

// 工厂方法: 创建矢量字体引擎，若字体加载失败则回退至内置点阵引擎
std::unique_ptr<ITextEngine> create_vector_text_engine(const std::string& font_path = "");

} // namespace kernetile::text
