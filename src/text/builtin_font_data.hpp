#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace kernetile::text {

// 紧凑 8x8 ASCII 标准点阵字体 (涵盖 32 ' ' 到 126 '~')
// 每字符占用 8 字节，按行高位在左排列
extern const uint8_t g_ascii_8x8[96][8];

// 常见汉字 16x16 点阵字典 (每字 32 字节，高品质点阵)
const uint8_t* find_builtin_cjk_glyph(uint32_t codepoint);

} // namespace kernetile::text
