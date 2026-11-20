# KTUI 子工程构建配置

set(KTUI_PAL_SOURCES
    src/pal/headless_backend.cpp
    src/pal/platform_backend.cpp
    src/app/application.cpp
)

if (WIN32)
    list(APPEND KTUI_PAL_SOURCES src/pal/win32_backend.cpp)
endif()

add_library(ktui_pal STATIC ${KTUI_PAL_SOURCES})
target_include_directories(ktui_pal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(ktui_pal PUBLIC kernetile_core kernetile_engine)
if (WIN32)
    target_link_libraries(ktui_pal PUBLIC user32.lib gdi32.lib imm32.lib)
endif()
