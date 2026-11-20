#pragma once

#include <string>
#include <cstdint>

namespace ktui {

// 通用字体类别
enum class FontFamilyCategory {
    System,      // 系统默认 UI 字体 (Windows: Segoe UI, macOS: SF Pro, Linux: Noto Sans)
    SansSerif,   // 无衬线体
    Serif,       // 衬线体
    Monospace    // 等宽代码字体 (Consolas, Menlo, Noto Sans Mono)
};

// 字体粗细
enum class FontWeight : uint16_t {
    Thin = 100,
    Light = 300,
    Regular = 400,
    Medium = 500,
    SemiBold = 600,
    Bold = 700,
    ExtraBold = 800,
    Black = 900
};

// 跨平台字体回退解析器接口
class IFontResolver {
public:
    virtual ~IFontResolver() = default;

    // 解析通用字体类别到当前操作系统的最佳可用字体名
    virtual std::string resolve_font(FontFamilyCategory category, FontWeight weight = FontWeight::Regular) = 0;

    // 根据字体名进行回退匹配，若不存在返回默认或 "builtin"
    virtual std::string resolve_font_by_name(const std::string& family_name) = 0;
};

} // namespace ktui
