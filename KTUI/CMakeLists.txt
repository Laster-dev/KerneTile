# KTUI 子工程构建配置

set(KTUI_PAL_SOURCES
    src/pal/headless_backend.cpp
    src/pal/platform_backend.cpp
    src/pal/mica_wallpaper.cpp
    src/app/application.cpp
    src/presentation/svg_engine.cpp
)

if (WIN32)
    list(APPEND KTUI_PAL_SOURCES src/pal/win32_backend.cpp)
elseif (ANDROID)
    list(APPEND KTUI_PAL_SOURCES src/pal/android_backend.cpp)
endif()

add_library(ktui_pal STATIC ${KTUI_PAL_SOURCES})
target_include_directories(ktui_pal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanosvg
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(ktui_pal PUBLIC kernetile_core kernetile_engine)
if (WIN32)
    target_link_libraries(ktui_pal PUBLIC user32.lib gdi32.lib imm32.lib ole32.lib windowscodecs.lib)
elseif (EMSCRIPTEN)
    # Emscripten HTML5 / Canvas 2D 库由编译器内置提供
elseif (ANDROID)
    target_link_libraries(ktui_pal PUBLIC android log)
else()
    find_package(X11)
    if (X11_FOUND)
        target_include_directories(ktui_pal PRIVATE ${X11_INCLUDE_DIR})
        target_compile_definitions(ktui_pal PRIVATE KTUI_HAS_X11=1)
        target_link_libraries(ktui_pal PUBLIC ${X11_LIBRARIES} xcb Xau Xdmcp pthread)
    else()
        target_link_libraries(ktui_pal PUBLIC pthread)
    endif()
endif()
