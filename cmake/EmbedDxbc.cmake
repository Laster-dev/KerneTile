file(READ "${INPUT}" shader HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${shader}")
file(WRITE "${OUTPUT}" "#pragma once\n#include <cstdint>\ninline constexpr uint8_t ui_compute_dxbc[] = {\n${bytes}\n};\n")
