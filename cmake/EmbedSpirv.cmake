file(READ "${INPUT}" shader HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])" "0x\\4\\3\\2\\1,\n" words "${shader}")
file(WRITE "${OUTPUT}" "#pragma once\n#include <cstdint>\ninline constexpr uint32_t ui_compute_spirv[] = {\n${words}};\n")
