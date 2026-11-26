# KTUI 子工程构建配置

# 按需可选编译的桌面集成模块（默认关闭，确保核心二进制零额外体积开销）
option(KTUI_ENABLE_SYSTEM_TRAY "Enable optional native OS system tray icon module" OFF)
option(KTUI_ENABLE_RICH_CLIPBOARD "Enable optional native clipboard image/file format module" OFF)
option(KTUI_ENABLE_INSPECTOR "Enable Debug-only F12 DevTools UI Inspector overlay (strictly stripped in Release)" ON)

set(KTUI_PAL_SOURCES
    src/pal/headless_backend.cpp
    src/pal/platform_backend.cpp
    src/pal/file_dialog.cpp
    src/pal/mica_wallpaper.cpp
    src/app/application.cpp
    src/app/window.cpp
    src/app/devtools_window.cpp
    src/presentation/svg_engine.cpp
    src/presentation/image_decoder.cpp
)

if (KTUI_ENABLE_SYSTEM_TRAY)
    list(APPEND KTUI_PAL_SOURCES src/pal/system_tray.cpp)
endif()
if (KTUI_ENABLE_RICH_CLIPBOARD)
    list(APPEND KTUI_PAL_SOURCES src/pal/rich_clipboard.cpp)
endif()

if (WIN32)
    list(APPEND KTUI_PAL_SOURCES src/pal/win32_backend.cpp)
elseif (ANDROID)
    list(APPEND KTUI_PAL_SOURCES src/pal/android_backend.cpp)
elseif (EMSCRIPTEN)
    list(APPEND KTUI_PAL_SOURCES src/pal/web_backend.cpp)
else()
    list(APPEND KTUI_PAL_SOURCES src/pal/linux_backend.cpp)
endif()

add_library(ktui_pal STATIC ${KTUI_PAL_SOURCES})
target_include_directories(ktui_pal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanosvg
    ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(ktui_pal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)
target_link_libraries(ktui_pal PUBLIC kernetile_core kernetile_engine)
if (KTUI_ENABLE_SYSTEM_TRAY)
    target_compile_definitions(ktui_pal PUBLIC KTUI_ENABLE_SYSTEM_TRAY=1)
endif()
if (KTUI_ENABLE_RICH_CLIPBOARD)
    target_compile_definitions(ktui_pal PUBLIC KTUI_ENABLE_RICH_CLIPBOARD=1)
endif()
if (KTUI_ENABLE_INSPECTOR)
    # 严格限制仅 Debug 配置生效，Release 模式 (NDEBUG) 下彻底零代码、零体积编入
    target_compile_definitions(ktui_pal PUBLIC $<$<CONFIG:Debug>:KTUI_ENABLE_INSPECTOR=1>)
endif()
if (WIN32)
    target_link_libraries(ktui_pal PUBLIC user32.lib gdi32.lib imm32.lib ole32.lib windowscodecs.lib)
elseif (EMSCRIPTEN)
    # Emscripten HTML5 / Canvas 2D 库由编译器内置提供
elseif (ANDROID)
    target_link_libraries(ktui_pal PUBLIC android log)
else()
    find_package(X11)
    if (X11_FOUND)
        if (X11_INCLUDE_DIR AND NOT X11_INCLUDE_DIR STREQUAL "/usr/include")
            target_include_directories(ktui_pal SYSTEM PRIVATE ${X11_INCLUDE_DIR})
        endif()
        target_compile_definitions(ktui_pal PRIVATE KTUI_HAS_X11=1)
        target_link_libraries(ktui_pal PUBLIC ${X11_LIBRARIES} xcb Xau Xdmcp pthread)
        if (X11_Xi_FOUND)
            target_compile_definitions(ktui_pal PRIVATE KTUI_HAS_XI2=1)
            target_link_libraries(ktui_pal PUBLIC ${X11_Xi_LIB})
        endif()
    else()
        target_link_libraries(ktui_pal PUBLIC pthread)
    endif()
endif()
