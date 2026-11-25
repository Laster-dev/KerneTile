cmake_minimum_required(VERSION 3.20)
project(KerneTile VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CI 增量编译加速：当环境变量 CCACHE_ENABLE=1 且存在 ccache 时自动启用内容哈希缓存
if (DEFINED ENV{CCACHE_ENABLE})
    find_program(KT_CCACHE ccache)
    if (KT_CCACHE)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${KT_CCACHE}")
        message(STATUS "KerneTile: ccache incremental build enabled (${KT_CCACHE})")
    endif()
endif()

# 统一编译器警告、MSVC 静态运行时 (/MT)、零异常/零RTTI、体积优化与严格标准检查
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _HAS_EXCEPTIONS=0 _HAS_STATIC_RTTI=0)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        string(REPLACE "/O2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        add_compile_options(
            /W3 /utf-8 /EHs-c- /GR-
            -Wno-missing-field-initializers
            $<$<CONFIG:Release>:/GS->
            $<$<CONFIG:Release>:/clang:-Oz>
            $<$<CONFIG:Release>:/clang:-ffunction-sections>
            $<$<CONFIG:Release>:/clang:-fdata-sections>
            $<$<CONFIG:Release>:/clang:-fno-asynchronous-unwind-tables>
            $<$<CONFIG:Release>:/clang:-fno-unwind-tables>
            $<$<CONFIG:Release>:/clang:-flto=thin>
        )
        # Hybrid CRT + LLD ICF/GC: 零 VC++ Redist 依赖 + Clang -Oz/ThinLTO 极致代码压缩
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /NODEFAULTLIB:libucrt.lib ucrt.lib")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /NODEFAULTLIB:libucrt.lib ucrt.lib")
    else()
        string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REPLACE "/Ob2" "/Ob1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        add_compile_options(
            /W4 /permissive- /utf-8 /EHs-c- /GR-
            $<$<CONFIG:Release>:/Gy>
            $<$<CONFIG:Release>:/Gw>
            $<$<CONFIG:Release>:/GL>
            $<$<CONFIG:Release>:/Zc:inline>
            $<$<CONFIG:Release>:/GS->
        )
        set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
        # Hybrid CRT: 静态链接 libvcruntime/libcpmt (零 VC++ Redist 依赖)，动态链接 Windows 10/11 系统自带的 ucrtbase.dll
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF=5 /NODEFAULTLIB:libucrt.lib ucrt.lib")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF=5 /NODEFAULTLIB:libucrt.lib ucrt.lib")
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers -fno-exceptions -fno-rtti)
    if (ANDROID)
        # Android NDK (Clang + LLD): -Oz + ThinLTO + ICF + 隐藏非 JNI 符号 + 全剥离
        string(REPLACE "-O3" "-Oz" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REPLACE "-O2" "-Oz" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        add_compile_options(
            $<$<CONFIG:Release>:-Oz>
            $<$<CONFIG:Release>:-flto=thin>
            $<$<CONFIG:Release>:-ffunction-sections>
            $<$<CONFIG:Release>:-fdata-sections>
            $<$<CONFIG:Release>:-fno-asynchronous-unwind-tables>
            $<$<CONFIG:Release>:-fno-unwind-tables>
            $<$<CONFIG:Release>:-fvisibility=hidden>
            $<$<CONFIG:Release>:-fvisibility-inlines-hidden>
        )
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto=thin -Wl,--gc-sections -Wl,--icf=all -Wl,--exclude-libs,ALL -Wl,--strip-all")
    elseif (NOT EMSCRIPTEN)
        option(KERNETILE_ZIG_CXX "Build with zig c++ for portable glibc targeting" OFF)
        if (KERNETILE_ZIG_CXX)
            # zig c++ 内置 Clang 19 + LLD + 静态 libc++，可锁定 glibc 2.28 跨发行版运行
            string(REPLACE "-O3" "-Oz" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
            string(REPLACE "-O2" "-Oz" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
            add_compile_options(
                $<$<CONFIG:Release>:-Oz>
                $<$<CONFIG:Release>:-ffunction-sections>
                $<$<CONFIG:Release>:-fdata-sections>
                $<$<CONFIG:Release>:-fno-asynchronous-unwind-tables>
                $<$<CONFIG:Release>:-fno-unwind-tables>
            )
            set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -Wl,--strip-all")
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            string(REPLACE "-O3" "-Oz" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
            string(REPLACE "-O2" "-Oz" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
            add_compile_options(
                $<$<CONFIG:Release>:-Oz>
                $<$<CONFIG:Release>:-ffunction-sections>
                $<$<CONFIG:Release>:-fdata-sections>
                $<$<CONFIG:Release>:-fno-asynchronous-unwind-tables>
                $<$<CONFIG:Release>:-fno-unwind-tables>
                $<$<CONFIG:Release>:-flto=thin>
            )
            set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=thin -fuse-ld=lld -Wl,--gc-sections -Wl,--icf=all -Wl,--strip-all")
        else()
            string(REPLACE "-O3" "-Os" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
            add_compile_options(
                $<$<CONFIG:Release>:-Os>
                $<$<CONFIG:Release>:-ffunction-sections>
                $<$<CONFIG:Release>:-fdata-sections>
                $<$<CONFIG:Release>:-fno-asynchronous-unwind-tables>
                $<$<CONFIG:Release>:-fno-unwind-tables>
                $<$<CONFIG:Release>:-flto=auto>
            )
            set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=auto -Wl,--gc-sections -s")
        endif()
    else()
        # Emscripten / WebAssembly: -Oz + LTO + 现代 Wasm 指令集 (bulk-memory/nontrapping-fptoint/sign-ext/WASM_BIGINT) + emmalloc + 无 POSIX FS
        string(REPLACE "-O3" "-Oz" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REPLACE "-O2" "-Oz" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        add_compile_options(
            $<$<CONFIG:Release>:-Oz>
            $<$<CONFIG:Release>:-flto>
            $<$<CONFIG:Release>:-ffunction-sections>
            $<$<CONFIG:Release>:-fdata-sections>
            $<$<CONFIG:Release>:-mbulk-memory>
            $<$<CONFIG:Release>:-mnontrapping-fptoint>
            $<$<CONFIG:Release>:-msign-ext>
            $<$<CONFIG:Release>:-mmutable-globals>
        )
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Oz -flto -mbulk-memory -mnontrapping-fptoint -msign-ext -mmutable-globals -Wl,--gc-sections -Wl,--strip-debug -sASSERTIONS=0 -sDISABLE_EXCEPTION_CATCHING=1 -sSUPPORT_LONGJMP=0 -sMALLOC=emmalloc -sFILESYSTEM=0 -sEVAL_CTORS=1 -sWASM_BIGINT=1 -sMIN_CHROME_VERSION=91 -sMIN_FIREFOX_VERSION=89 -sMIN_SAFARI_VERSION=150000")
    endif()
endif()

# 跨发行版便携链接策略
# 工业标准实践 (AppImage/Chrome/Qt 方案): 静态绑定 C++ 运行时 (-static-libstdc++ -static-libgcc)，动态绑定系统 glibc 与 libX11
# 1. 彻底杜绝全静态链接 glibc 时 dlopen(libXcursor) 引起的 __libc_early_init (SIGFPE 算术异常崩溃)
# 2. 避免目标机缺失对应版本的 libstdc++.so.6
# 3. 二进制体积从 4.5MB 锐减至几百 KB
option(KERNETILE_STATIC "Enable full static linking (including libc)" OFF)
if (KERNETILE_STATIC AND NOT WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
else()
    if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT KERNETILE_ZIG_CXX)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
    endif()
endif()

option(KERNETILE_GPU_COMPUTE "Enable GPU tile composition for the window renderer" ON)

# 核心接口库
add_library(kernetile_core INTERFACE)
target_include_directories(kernetile_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 瓦片划分与脏块管理库 (Tile)
set(TILE_SOURCES
    src/tile/tile_binner.cpp
    src/tile/dirty_tracker.cpp
)
add_library(kernetile_tile STATIC ${TILE_SOURCES})
target_include_directories(kernetile_tile PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(kernetile_tile PUBLIC kernetile_core)

# 硬件抽象层库 (HAL)
set(HAL_SOURCES
    src/hal/null/null_device.cpp
)
if (WIN32)
    list(APPEND HAL_SOURCES src/hal/d3d12/d3d12_device.cpp)
endif()

add_library(kernetile_hal STATIC ${HAL_SOURCES})
target_include_directories(kernetile_hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/null
)
if (WIN32)
    target_include_directories(kernetile_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/d3d12)
    target_link_libraries(kernetile_hal PUBLIC d3d12.lib dxgi.lib)
elseif (EMSCRIPTEN)
    # Emscripten 环境使用零依赖纯内存 NullDevice 设备与多线程/单线程 CPU 瓦片光栅器
elseif (ANDROID)
    target_link_libraries(kernetile_hal PUBLIC android log)
else()
    target_link_libraries(kernetile_hal PUBLIC pthread)
endif()
target_link_libraries(kernetile_hal PUBLIC kernetile_core)

# 文本排版与光栅化引擎库 (Text)
set(TEXT_SOURCES
    src/text/shared_text_engine.cpp
)
if (WIN32)
    list(APPEND TEXT_SOURCES src/text/dwrite_text_engine.cpp)
elseif (EMSCRIPTEN)
    list(APPEND TEXT_SOURCES src/text/web_text_engine.cpp)
elseif (ANDROID)
    list(APPEND TEXT_SOURCES src/text/android_text_engine.cpp)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(KT_PANGO REQUIRED IMPORTED_TARGET pangocairo)
    list(APPEND TEXT_SOURCES src/text/pango_text_engine.cpp)
endif()
add_library(kernetile_text STATIC ${TEXT_SOURCES})
target_include_directories(kernetile_text PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/text
)
target_link_libraries(kernetile_text PUBLIC kernetile_core)
if (WIN32)
    target_link_libraries(kernetile_text PRIVATE dwrite.lib gdi32.lib user32.lib)
elseif (TARGET PkgConfig::KT_PANGO)
    target_link_libraries(kernetile_text PRIVATE PkgConfig::KT_PANGO ${CMAKE_DL_LIBS})
endif()

# 视觉特效与滤镜库 (FX)
set(FX_SOURCES
    src/fx/dual_kawase_blur.cpp
    src/fx/clip_stack.cpp
)
add_library(kernetile_fx STATIC ${FX_SOURCES})
target_include_directories(kernetile_fx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fx
)
target_link_libraries(kernetile_fx PUBLIC kernetile_core)

# 统一输入系统库 (Input)
set(INPUT_SOURCES
    src/input/pointer_pipeline.cpp
    src/input/ime_state_machine.cpp
)
add_library(kernetile_input STATIC ${INPUT_SOURCES})
target_include_directories(kernetile_input PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input
)
target_link_libraries(kernetile_input PUBLIC kernetile_core)

# 平台呈现与同步库 (Platform)
set(PLATFORM_SOURCES
    src/platform/presentation_sync.cpp
)
add_library(kernetile_platform STATIC ${PLATFORM_SOURCES})
target_include_directories(kernetile_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)
target_link_libraries(kernetile_platform PUBLIC kernetile_core)

# 纯 CPU 软件光栅回退库 (CPU)
set(CPU_SOURCES
    src/cpu/cpu_rasterizer.cpp
)
add_library(kernetile_cpu STATIC ${CPU_SOURCES})
target_include_directories(kernetile_cpu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpu
)
target_link_libraries(kernetile_cpu PUBLIC kernetile_core kernetile_tile kernetile_fx)
# 保持纯 CPU 软件瓦片光栅器的最高运行性能
if (MSVC)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(kernetile_cpu PRIVATE $<$<CONFIG:Release>:/clang:-O3>)
    else()
        target_compile_options(kernetile_cpu PRIVATE $<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/Ob2>)
    endif()
elseif (EMSCRIPTEN)
    target_compile_options(kernetile_cpu PRIVATE $<$<CONFIG:Release>:-O2>)
else()
    target_compile_options(kernetile_cpu PRIVATE $<$<CONFIG:Release>:-O3>)
endif()

# KerneTile 引擎统一门面库 (Engine)
set(ENGINE_SOURCES src/renderer.cpp)
if (KERNETILE_GPU_COMPUTE AND WIN32)
    file(GLOB KT_WIN_SDK_BIN_DIRS "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
    list(SORT KT_WIN_SDK_BIN_DIRS ORDER DESCENDING)
    find_program(KT_FXC fxc HINTS ${KT_WIN_SDK_BIN_DIRS})
    list(APPEND ENGINE_SOURCES src/compute/ui_compute_d3d12.cpp)
    if (KT_FXC)
        set(KT_DXBC "${CMAKE_CURRENT_BINARY_DIR}/generated/ui_compute.dxbc")
        set(KT_DXBC_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/ui_compute_dxbc.hpp")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        add_custom_command(OUTPUT "${KT_DXBC_HEADER}"
            COMMAND "${KT_FXC}" /nologo /T cs_5_1 /E compose /O3 /Fo "${KT_DXBC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui_compute_d3d12.hlsl"
            COMMAND ${CMAKE_COMMAND} "-DINPUT=${KT_DXBC}" "-DOUTPUT=${KT_DXBC_HEADER}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedDxbc.cmake"
            DEPENDS shaders/ui_compute_d3d12.hlsl cmake/EmbedDxbc.cmake
            VERBATIM)
        list(APPEND ENGINE_SOURCES "${KT_DXBC_HEADER}")
        set(KT_PRECOMPILED_DXBC ON)
    else()
        set(KT_SHADER shaders/ui_compute_d3d12.hlsl)
    endif()
elseif (KERNETILE_GPU_COMPUTE AND EMSCRIPTEN)
    set(KT_SHADER shaders/ui_compute.wgsl)
    list(APPEND ENGINE_SOURCES src/compute/ui_compute_web.cpp)
elseif (KERNETILE_GPU_COMPUTE AND ANDROID)
    find_program(KT_GLSLC glslc
        HINTS
            "${ANDROID_NDK}/shader-tools/${ANDROID_HOST_TAG}"
            "${ANDROID_NDK}/shader-tools/windows-x86_64"
            "${ANDROID_NDK}/shader-tools/linux-x86_64"
            "${ANDROID_NDK}/shader-tools/darwin-x86_64"
    )
    find_program(KT_GLSLANG glslangValidator)
    if (KT_GLSLC OR KT_GLSLANG)
        list(APPEND ENGINE_SOURCES src/compute/ui_compute_vulkan.cpp)
        set(KT_SPIRV "${CMAKE_CURRENT_BINARY_DIR}/generated/ui_compute.spv")
        set(KT_SPIRV_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/ui_compute_spirv.hpp")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        if (KT_GLSLC)
            set(KT_SPIRV_CMD "${KT_GLSLC}" -x hlsl -fshader-stage=comp -fentry-point=compose -DKT_VULKAN=1 -fhlsl-offsets
                -o "${KT_SPIRV}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui_compute_d3d12.hlsl")
        else()
            set(KT_SPIRV_CMD "${KT_GLSLANG}" -D -DKT_VULKAN=1 -V -S comp -e compose --hlsl-offsets
                -o "${KT_SPIRV}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui_compute_d3d12.hlsl")
        endif()
        add_custom_command(OUTPUT "${KT_SPIRV_HEADER}"
            COMMAND ${KT_SPIRV_CMD}
            COMMAND ${CMAKE_COMMAND} "-DINPUT=${KT_SPIRV}" "-DOUTPUT=${KT_SPIRV_HEADER}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
            DEPENDS shaders/ui_compute_d3d12.hlsl cmake/EmbedSpirv.cmake
            VERBATIM)
        list(APPEND ENGINE_SOURCES "${KT_SPIRV_HEADER}")
        set(KT_ANDROID_VULKAN ON)
        message(STATUS "KerneTile Android: Vulkan GPU Compute enabled (${KT_GLSLC}${KT_GLSLANG})")
    else()
        message(STATUS "KerneTile Android: neither glslc nor glslangValidator found, using CPU tile rasterizer")
        list(APPEND ENGINE_SOURCES src/compute/ui_compute_null.cpp)
    endif()
elseif (KERNETILE_GPU_COMPUTE AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT KERNETILE_STATIC)
    find_package(Vulkan REQUIRED)
    find_program(KT_GLSLANG glslangValidator REQUIRED)
    list(APPEND ENGINE_SOURCES src/compute/ui_compute_vulkan.cpp)
    set(KT_SPIRV "${CMAKE_CURRENT_BINARY_DIR}/generated/ui_compute.spv")
    set(KT_SPIRV_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/ui_compute_spirv.hpp")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    add_custom_command(OUTPUT "${KT_SPIRV_HEADER}"
        COMMAND "${KT_GLSLANG}" -D -DKT_VULKAN=1 -V -S comp -e compose --hlsl-offsets
                -o "${KT_SPIRV}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui_compute_d3d12.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${KT_SPIRV}" "-DOUTPUT=${KT_SPIRV_HEADER}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
        DEPENDS shaders/ui_compute_d3d12.hlsl cmake/EmbedSpirv.cmake
        VERBATIM)
    list(APPEND ENGINE_SOURCES "${KT_SPIRV_HEADER}")
else()
    list(APPEND ENGINE_SOURCES src/compute/ui_compute_null.cpp)
endif()
if (KT_SHADER)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${KT_SHADER}")
    file(READ "${KT_SHADER}" KT_UI_SHADER)
    configure_file(src/compute/ui_compute_shader.hpp.in generated/ui_compute_shader.hpp @ONLY)
endif()
add_library(kernetile_engine STATIC ${ENGINE_SOURCES})
target_include_directories(kernetile_engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
if (KERNETILE_GPU_COMPUTE AND WIN32)
    if (KT_PRECOMPILED_DXBC)
        target_compile_definitions(kernetile_engine PRIVATE KT_PRECOMPILED_DXBC=1)
    else()
        target_link_libraries(kernetile_engine PUBLIC d3dcompiler.lib)
    endif()
elseif (KT_ANDROID_VULKAN)
    target_link_libraries(kernetile_engine PRIVATE vulkan android)
elseif (TARGET Vulkan::Vulkan)
    target_link_libraries(kernetile_engine PRIVATE Vulkan::Vulkan)
endif()
target_include_directories(kernetile_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(kernetile_engine PUBLIC
    kernetile_core
    kernetile_hal
    kernetile_tile
    kernetile_text
    kernetile_fx
    kernetile_input
    kernetile_platform
    kernetile_cpu
)

# ==================== KTUI 跨平台现代 C++20 声明式 UI 框架 ====================
add_subdirectory(KTUI)

# KTUI Gallery 桌面 / Web / Android 官方控件陈列室应用程序
set(KTUI_GALLERY_SOURCES
    examples/ktui_gallery/main.cpp
    examples/ktui_gallery/gallery_app.cpp
    examples/ktui_gallery/pages/home_page.cpp
    examples/ktui_gallery/pages/layout_pages.cpp
    examples/ktui_gallery/pages/basic_input_pages.cpp
    examples/ktui_gallery/pages/progress_page.cpp
    examples/ktui_gallery/pages/dialogs_menus_page.cpp
    examples/ktui_gallery/pages/settings_about_pages.cpp
)
if (ANDROID)
    add_library(ktui_gallery SHARED ${KTUI_GALLERY_SOURCES})
else()
    add_executable(ktui_gallery ${KTUI_GALLERY_SOURCES})
endif()
target_include_directories(ktui_gallery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.16" AND NOT KERNETILE_ZIG_CXX)
    target_precompile_headers(ktui_gallery PRIVATE <ktui/ktui.hpp>)
endif()
target_link_libraries(ktui_gallery PRIVATE
    ktui_pal
    kernetile_engine
)
if (MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ktui_gallery PRIVATE $<$<CONFIG:Release>:/Ob0>)
endif()
if (ANDROID)
    target_link_libraries(ktui_gallery PRIVATE android log)
endif()
if (EMSCRIPTEN)
    set_target_properties(ktui_gallery PROPERTIES
        SUFFIX ".js"
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/ktui_gallery/shell.html"
    )
    target_link_options(ktui_gallery PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "-sNO_EXIT_RUNTIME=1"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createKtuiApp"
        "-sENVIRONMENT=web"
        "-sEXPORTED_RUNTIME_METHODS=['stringToUTF8','UTF8ToString','lengthBytesUTF8']"
    )
    add_custom_command(TARGET ktui_gallery POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/ktui_gallery/shell.html"
            "$<TARGET_FILE_DIR:ktui_gallery>/ktui_gallery.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/ktui_gallery/shell.html"
            "$<TARGET_FILE_DIR:ktui_gallery>/index.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/ktui_gallery/wallpaper_dark.jpg"
            "$<TARGET_FILE_DIR:ktui_gallery>/wallpaper_dark.jpg"
    )
endif()

# ==================== 自动化单元测试套件 (默认关闭，按需开启) ====================
option(BUILD_TESTING "Enable KerneTile and KTUI unit test suite" OFF)

if (BUILD_TESTING)
    if (EMSCRIPTEN)
        # WebAssembly 环境下通过 node 驱动跨平台测试
        set(CMAKE_CROSSCOMPILING_EMULATOR "node")
    endif()
    # 启用自动化测试
    enable_testing()

    function(add_kernetile_test test_name source_file)
        add_executable(${test_name} ${source_file})
        target_link_libraries(${test_name} PRIVATE kernetile_engine)
        add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endfunction()

    function(add_ktui_test test_name source_file)
        add_executable(${test_name} ${source_file})
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
            ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/src
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(${test_name} PRIVATE ${ARGN})
        add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endfunction()

    # 阶段 0 测试
    add_kernetile_test(test_layout tests/test_layout.cpp)
    add_kernetile_test(test_color tests/test_color.cpp)
    add_kernetile_test(test_premul tests/test_premul.cpp)

    # 阶段 1 测试
    add_kernetile_test(test_sdf tests/test_sdf.cpp)
    add_kernetile_test(test_shadow tests/test_shadow.cpp)
    if (WIN32)
        add_kernetile_test(test_shader_compilation tests/test_shader_compilation.cpp)
    endif()

    # 阶段 2 测试 (Windows D3D12 硬件测试)
    if (WIN32)
        add_kernetile_test(test_hal_lifecycle tests/test_hal_lifecycle.cpp)
        add_kernetile_test(test_compute_uav_readback tests/test_compute_uav_readback.cpp)
    endif()

    # 阶段 3 测试
    add_kernetile_test(test_tile_binning tests/test_tile_binning.cpp)
    add_kernetile_test(test_dirty_tracking tests/test_dirty_tracking.cpp)
    if (WIN32)
        add_kernetile_test(test_gpu_tile_compute tests/test_gpu_tile_compute.cpp)
    endif()

    # 阶段 4 测试
    add_kernetile_test(test_vring_throughput tests/test_vring_throughput.cpp)
    add_kernetile_test(test_notify_suppression tests/test_notify_suppression.cpp)

    # 阶段 5 测试
    add_kernetile_test(test_shelf_packing_ratio tests/test_shelf_packing_ratio.cpp)
    add_kernetile_test(test_atlas_bleeding tests/test_atlas_bleeding.cpp)
    add_kernetile_test(test_dynamic_atlas_lifecycle tests/test_dynamic_atlas_lifecycle.cpp)

    # 阶段 6 测试
    add_kernetile_test(test_stem_darkening tests/test_stem_darkening.cpp)
    add_kernetile_test(test_text_shaping tests/test_text_shaping.cpp)
    add_kernetile_test(test_text_pipeline tests/test_text_pipeline.cpp)

    # 阶段 7 测试
    add_kernetile_test(test_kawase_blur tests/test_kawase_blur.cpp)
    add_kernetile_test(test_clip_stack tests/test_clip_stack.cpp)

    # 阶段 8 测试
    add_kernetile_test(test_pointer_pipeline tests/test_pointer_pipeline.cpp)
    add_kernetile_test(test_ime_state_machine tests/test_ime_state_machine.cpp)
    add_kernetile_test(test_platform_sync tests/test_platform_sync.cpp)

    # 阶段 9 测试
    if (WIN32)
        add_kernetile_test(test_warp_adapter tests/test_warp_adapter.cpp)
    endif()
    add_kernetile_test(test_cpu_rasterizer tests/test_cpu_rasterizer.cpp)
    add_kernetile_test(test_backend_conformance tests/test_backend_conformance.cpp)

    # 阶段 10 测试与基准套件
    add_kernetile_test(test_renderer_lifecycle tests/test_renderer_lifecycle.cpp)
    add_kernetile_test(bench_4k_scenarios tests/bench_4k_scenarios.cpp)

    # KTUI 自动化单元测试套件 (统一通过 add_ktui_test 宏注册)
    add_ktui_test(test_ktui_pal_headless     tests/test_ktui_pal_headless.cpp     ktui_pal kernetile_core)
    add_ktui_test(test_ktui_data_binding     tests/test_ktui_data_binding.cpp     kernetile_core)
    add_ktui_test(test_ktui_spring_physics   tests/test_ktui_spring_physics.cpp   kernetile_core)
    add_ktui_test(test_ktui_app_and_theme    tests/test_ktui_app_and_theme.cpp    ktui_pal kernetile_core)
    add_ktui_test(test_ktui_flex_layout      tests/test_ktui_flex_layout.cpp      kernetile_core)
    add_ktui_test(test_ktui_dirty_and_render tests/test_ktui_dirty_and_render.cpp ktui_pal kernetile_engine)
    add_ktui_test(test_ktui_e2e_app          tests/test_ktui_e2e_app.cpp          ktui_pal kernetile_engine)
    add_ktui_test(test_ktui_button_and_motion tests/test_ktui_button_and_motion.cpp ktui_pal kernetile_engine)
    add_ktui_test(test_ktui_textbox          tests/test_ktui_textbox.cpp          ktui_pal kernetile_engine)
    add_ktui_test(test_ktui_checkbox         tests/test_ktui_checkbox.cpp         ktui_pal kernetile_engine)
    add_ktui_test(test_ktui_slider_progress  tests/test_ktui_slider_progress.cpp  ktui_pal kernetile_engine)
    add_ktui_test(test_ktui_combobox         tests/test_ktui_combobox.cpp         ktui_pal kernetile_engine)
    add_ktui_test(test_ktui_navigation_view  tests/test_ktui_navigation_view.cpp  ktui_pal kernetile_engine)
    add_ktui_test(test_ktui_overlays_and_menus tests/test_ktui_overlays_and_menus.cpp ktui_pal kernetile_engine)
endif()


