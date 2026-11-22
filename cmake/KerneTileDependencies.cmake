include(FetchContent)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(FT_DISABLE_ZLIB ON CACHE BOOL "")
set(FT_DISABLE_BZIP2 ON CACHE BOOL "")
set(FT_DISABLE_PNG ON CACHE BOOL "")
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "")
set(FT_DISABLE_BROTLI ON CACHE BOOL "")
set(HB_BUILD_SUBSET OFF CACHE BOOL "")
set(HB_HAVE_FREETYPE OFF CACHE BOOL "")

FetchContent_Declare(kt_freetype
    URL https://codeload.github.com/freetype/freetype/tar.gz/refs/tags/VER-2-13-3
    URL_HASH SHA256=bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103)
FetchContent_Declare(kt_harfbuzz
    URL https://codeload.github.com/harfbuzz/harfbuzz/tar.gz/refs/tags/10.4.0
    URL_HASH SHA256=0d25a3f74af4e8744700ac19050af5a80ae330378a5802a5cd71e523bb6fda1f)
FetchContent_Declare(kt_utf8proc
    URL https://codeload.github.com/JuliaStrings/utf8proc/tar.gz/refs/tags/v2.10.0
    URL_HASH SHA256=6f4f1b639daa6dca9f80bc5db1233e9cbaa31a67790887106160b33ef743f136)
set(UTF8PROC_INSTALL OFF CACHE BOOL "")
FetchContent_MakeAvailable(kt_freetype kt_harfbuzz kt_utf8proc)

set(KERNETILE_UI_FONT "" CACHE FILEPATH "Font file for the shared KTUI text engine")
set(KERNETILE_UI_FONT_LICENSE "" CACHE FILEPATH "License for an overridden KTUI font")
if (NOT KERNETILE_UI_FONT)
    set(font_dir "${CMAKE_CURRENT_BINARY_DIR}/fonts")
    file(MAKE_DIRECTORY "${font_dir}")
    set(KERNETILE_UI_FONT "${font_dir}/NotoSansSC-Regular.otf")
    set(KERNETILE_UI_FONT_LICENSE "${font_dir}/OFL.txt")
    set(font_url "https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans")
    file(DOWNLOAD "${font_url}/SubsetOTF/SC/NotoSansSC-Regular.otf" "${KERNETILE_UI_FONT}"
        EXPECTED_HASH SHA256=faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9 TLS_VERIFY ON)
    file(DOWNLOAD "${font_url}/LICENSE" "${KERNETILE_UI_FONT_LICENSE}"
        EXPECTED_HASH SHA256=6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2 TLS_VERIFY ON)
endif()
if (NOT EXISTS "${KERNETILE_UI_FONT}")
    message(FATAL_ERROR "KERNETILE_UI_FONT must point to an existing font file")
endif()

option(KERNETILE_GPU_COMPUTE "Enable GPU tile composition for the window renderer" ON)
set(KERNETILE_WGPU_NATIVE_DIR "" CACHE PATH "Directory containing the wgpu-native 0.19.4.1 SDK")
if (KERNETILE_GPU_COMPUTE AND NOT KERNETILE_STATIC AND NOT EMSCRIPTEN AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$")
        set(wgpu_arch x86_64)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
        set(wgpu_arch aarch64)
    endif()
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        unset(wgpu_arch)
    endif()
    if (NOT KERNETILE_WGPU_NATIVE_DIR AND wgpu_arch)
        if (WIN32)
            set(wgpu_platform windows)
        else()
            set(wgpu_platform linux)
        endif()
        set(wgpu_hash)
        if (wgpu_arch STREQUAL "x86_64")
            if (WIN32)
                set(wgpu_hash URL_HASH SHA256=9e1591d60c2d2ee20d6d4a63bc01c7c5eecf7734761673160aa639e550a1ba4d)
            else()
                set(wgpu_hash URL_HASH SHA256=7d73bd7af2be60b632e5ab814996acb381d1b459975d6629f91c468049c8866a)
            endif()
        endif()
        FetchContent_Declare(kt_wgpu
            URL https://github.com/gfx-rs/wgpu-native/releases/download/v0.19.4.1/wgpu-${wgpu_platform}-${wgpu_arch}-release.zip
            ${wgpu_hash})
        FetchContent_MakeAvailable(kt_wgpu)
        set(KERNETILE_WGPU_NATIVE_DIR "${kt_wgpu_SOURCE_DIR}")
    endif()
    if (KERNETILE_WGPU_NATIVE_DIR)
        add_library(kt_wgpu_native SHARED IMPORTED)
        set_target_properties(kt_wgpu_native PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${KERNETILE_WGPU_NATIVE_DIR}")
        if (WIN32)
            set_target_properties(kt_wgpu_native PROPERTIES
                IMPORTED_LOCATION "${KERNETILE_WGPU_NATIVE_DIR}/wgpu_native.dll"
                IMPORTED_IMPLIB "${KERNETILE_WGPU_NATIVE_DIR}/wgpu_native.dll.lib")
        else()
            set_target_properties(kt_wgpu_native PROPERTIES
                IMPORTED_LOCATION "${KERNETILE_WGPU_NATIVE_DIR}/libwgpu_native.so"
                IMPORTED_NO_SONAME TRUE)
        endif()
    endif()
endif()
