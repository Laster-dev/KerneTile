#pragma once

#include <cstdint>

namespace kernetile::text {

// 紧凑 8x8 ASCII 标准点阵字体 (涵盖 32 ' ' 到 126 '~')
// 每字符占用 8 字节，按行高位在左排列
extern const uint8_t g_ascii_8x8[96][8];

} // namespace kernetile::text
