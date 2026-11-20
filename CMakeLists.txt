cmake_minimum_required(VERSION 3.20)
project(KerneTile VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 统一编译器警告与严格标准检查
if (MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

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
    src/hal/d3d12/d3d12_device.cpp
)

add_library(kernetile_hal STATIC ${HAL_SOURCES})
target_include_directories(kernetile_hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/d3d12
)
target_link_libraries(kernetile_hal PUBLIC kernetile_core)

if (WIN32)
    target_link_libraries(kernetile_hal PUBLIC d3d12.lib dxgi.lib)
endif()

# 文本排版与光栅化引擎库 (Text)
set(TEXT_SOURCES
    src/text/builtin_text_engine.cpp
    src/text/dwrite_text_engine.cpp
)
add_library(kernetile_text STATIC ${TEXT_SOURCES})
target_include_directories(kernetile_text PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/text
)
target_link_libraries(kernetile_text PUBLIC kernetile_core)
if (WIN32)
    target_link_libraries(kernetile_text PUBLIC dwrite.lib)
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

# KerneTile 引擎统一门面库 (Engine)
set(ENGINE_SOURCES
    src/renderer.cpp
)
add_library(kernetile_engine STATIC ${ENGINE_SOURCES})
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

# 启用自动化测试
enable_testing()

function(add_kernetile_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} PRIVATE kernetile_engine kernetile_hal kernetile_tile kernetile_text kernetile_fx kernetile_input kernetile_platform kernetile_cpu)
    add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# 阶段 0 测试
add_kernetile_test(test_layout tests/test_layout.cpp)
add_kernetile_test(test_color tests/test_color.cpp)
add_kernetile_test(test_premul tests/test_premul.cpp)

# 阶段 1 测试
add_kernetile_test(test_sdf tests/test_sdf.cpp)
add_kernetile_test(test_shadow tests/test_shadow.cpp)
add_kernetile_test(test_shader_compilation tests/test_shader_compilation.cpp)

# 阶段 2 测试
add_kernetile_test(test_hal_lifecycle tests/test_hal_lifecycle.cpp)
add_kernetile_test(test_compute_uav_readback tests/test_compute_uav_readback.cpp)

# 阶段 3 测试
add_kernetile_test(test_tile_binning tests/test_tile_binning.cpp)
add_kernetile_test(test_dirty_tracking tests/test_dirty_tracking.cpp)
add_kernetile_test(test_gpu_tile_compute tests/test_gpu_tile_compute.cpp)

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
add_kernetile_test(test_warp_adapter tests/test_warp_adapter.cpp)
add_kernetile_test(test_cpu_rasterizer tests/test_cpu_rasterizer.cpp)
add_kernetile_test(test_backend_conformance tests/test_backend_conformance.cpp)

# 阶段 10 测试与基准套件
add_kernetile_test(test_renderer_lifecycle tests/test_renderer_lifecycle.cpp)
add_kernetile_test(bench_4k_scenarios tests/bench_4k_scenarios.cpp)

# ==================== KTUI 跨平台现代 C++20 声明式 UI 框架 ====================
add_subdirectory(KTUI)

# KTUI 阶段 1 测试
add_executable(test_ktui_pal_headless tests/test_ktui_pal_headless.cpp)
target_include_directories(test_ktui_pal_headless PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_ktui_pal_headless PRIVATE ktui_pal kernetile_core)
add_test(NAME test_ktui_pal_headless COMMAND test_ktui_pal_headless WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# KTUI 阶段 2 测试
add_executable(test_ktui_data_binding tests/test_ktui_data_binding.cpp)
target_include_directories(test_ktui_data_binding PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_ktui_data_binding PRIVATE kernetile_core)
add_test(NAME test_ktui_data_binding COMMAND test_ktui_data_binding WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# KTUI 阶段 3 测试
add_executable(test_ktui_spring_physics tests/test_ktui_spring_physics.cpp)
target_include_directories(test_ktui_spring_physics PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_ktui_spring_physics PRIVATE kernetile_core)
add_test(NAME test_ktui_spring_physics COMMAND test_ktui_spring_physics WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# KTUI 阶段 4 测试
add_executable(test_ktui_app_and_theme tests/test_ktui_app_and_theme.cpp)
target_include_directories(test_ktui_app_and_theme PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_ktui_app_and_theme PRIVATE ktui_pal kernetile_core)
add_test(NAME test_ktui_app_and_theme COMMAND test_ktui_app_and_theme WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# KTUI 阶段 5 测试
add_executable(test_ktui_flex_layout tests/test_ktui_flex_layout.cpp)
target_include_directories(test_ktui_flex_layout PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_ktui_flex_layout PRIVATE kernetile_core)
add_test(NAME test_ktui_flex_layout COMMAND test_ktui_flex_layout WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# KTUI 阶段 6 测试
add_executable(test_ktui_dirty_and_render tests/test_ktui_dirty_and_render.cpp)
target_include_directories(test_ktui_dirty_and_render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_ktui_dirty_and_render PRIVATE
    ktui_pal
    kernetile_engine
    kernetile_hal
    kernetile_tile
    kernetile_text
    kernetile_fx
    kernetile_input
    kernetile_platform
    kernetile_cpu
    kernetile_core
)
add_test(NAME test_ktui_dirty_and_render COMMAND test_ktui_dirty_and_render WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# KTUI 阶段 7 测试 (端到端实战验证)
add_executable(test_ktui_e2e_app tests/test_ktui_e2e_app.cpp)
target_include_directories(test_ktui_e2e_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_ktui_e2e_app PRIVATE
    ktui_pal
    kernetile_engine
    kernetile_hal
    kernetile_tile
    kernetile_text
    kernetile_fx
    kernetile_input
    kernetile_platform
    kernetile_cpu
    kernetile_core
)
add_test(NAME test_ktui_e2e_app COMMAND test_ktui_e2e_app WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# KTUI 桌面成品可执行 Demo 应用程序
add_executable(ktui_studio_demo examples/ktui_studio/main.cpp)
target_include_directories(ktui_studio_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/include
    ${CMAKE_CURRENT_SOURCE_DIR}/KTUI/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ktui_studio_demo PRIVATE
    ktui_pal
    kernetile_engine
    kernetile_hal
    kernetile_tile
    kernetile_text
    kernetile_fx
    kernetile_input
    kernetile_platform
    kernetile_cpu
    kernetile_core
)


