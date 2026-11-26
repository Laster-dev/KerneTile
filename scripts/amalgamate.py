#!/usr/bin/env python3
"""
KerneTile + KTUI Single-File Amalgamation Generator
===================================================
Generates a drop-in two-file distribution:
  - kernetile.hpp : All public KerneTile & KTUI C++20 APIs
  - kernetile.cpp : Complete cross-platform implementation (CPU/D3D12/WebGPU/Pure-GDI,
                    NanoSVG, trimmed stb_image PNG/JPG/BMP/GIF, embedded shaders)
  - example_hello.cpp : Minimal 15-line quick-start example

Usage:
  python scripts/amalgamate.py --out dist/amalgamation
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

PUBLIC_INCLUDE_DIRS = [
    ROOT / "include",
    ROOT / "KTUI" / "include",
]

PRIVATE_INCLUDE_DIRS = [
    ROOT / "include",
    ROOT / "KTUI" / "include",
    ROOT / "KTUI" / "src",
    ROOT / "KTUI" / "third_party" / "nanosvg",
    ROOT / "KTUI" / "third_party" / "stb",
    ROOT / "src" / "hal" / "null",
    ROOT / "src" / "hal" / "d3d12",
    ROOT / "src" / "text",
    ROOT / "src" / "fx",
    ROOT / "src" / "input",
    ROOT / "src" / "platform",
    ROOT / "src" / "cpu",
    ROOT / "src" / "compute",
]

INCLUDE_RE = re.compile(r'^(\s*)#\s*include\s*([<"])([^>"]+)[>"]')
PRAGMA_ONCE_RE = re.compile(r'^\s*#\s*pragma\s+once\b')


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def resolve_include(inc_name: str, current_file: Path, search_dirs: list[Path]) -> Path | None:
    # 1. Relative to current file directory
    rel = (current_file.parent / inc_name).resolve()
    if rel.is_file():
        return rel
    # 2. Search configured include directories
    for d in search_dirs:
        cand = (d / inc_name).resolve()
        if cand.is_file():
            return cand
    return None


def make_shader_header_inline(shader_rel: str) -> str:
    shader_text = read_text(ROOT / shader_rel)
    return (
        "namespace kernetile::compute {\n"
        'inline constexpr char shader_source[] = R"KT_AMALG_SHADER(\n'
        + shader_text
        + '\n)KT_AMALG_SHADER";\n'
        "} // namespace kernetile::compute\n"
    )


def expand_file(
    path: Path,
    search_dirs: list[Path],
    visited: set[Path],
    out_lines: list[str],
    *,
    is_source: bool = False,
    shader_rel: str | None = None,
    allow_reinclude_third_party: bool = False,
) -> None:
    resolved = path.resolve()
    if not is_source:
        if resolved in visited:
            return
        visited.add(resolved)

    rel_display = resolved.relative_to(ROOT).as_posix()
    out_lines.append(f"// ===== BEGIN {rel_display} =====\n")

    text = read_text(resolved)
    is_third_party_file = "third_party" in resolved.parts
    if is_third_party_file:
        # nanosvgrast.h includes "nanosvg.h", which is already inlined right before it.
        if resolved.name == "nanosvgrast.h":
            text = re.sub(r'^\s*#\s*include\s*"nanosvg\.h"\s*$', '// (nanosvg.h already inlined)', text, flags=re.MULTILINE)
        out_lines.append(text)
        if not text.endswith("\n"):
            out_lines.append("\n")
        out_lines.append(f"// ===== END {rel_display} =====\n\n")
        return

    for line in text.splitlines(keepends=True):
        if not is_source and PRAGMA_ONCE_RE.match(line):
            continue

        m = INCLUDE_RE.match(line)
        if m:
            inc_name = m.group(3)
            # Generated shader header
            if inc_name == "ui_compute_shader.hpp" and shader_rel:
                out_lines.append(f"// Inlined shader from {shader_rel}\n")
                out_lines.append(make_shader_header_inline(shader_rel))
                continue

            target = resolve_include(inc_name, resolved, search_dirs)
            if target is not None:
                # For third-party single-header implementations (nanosvg / stb_image),
                # expand at the exact point where *_IMPLEMENTATION is defined.
                is_third_party = "third_party" in target.parts
                if is_third_party and allow_reinclude_third_party:
                    expand_file(
                        target,
                        search_dirs,
                        visited,
                        out_lines,
                        is_source=True,
                        shader_rel=shader_rel,
                    )
                else:
                    expand_file(
                        target,
                        search_dirs,
                        visited,
                        out_lines,
                        is_source=False,
                        shader_rel=shader_rel,
                    )
                continue

        out_lines.append(line)

    if not text.endswith("\n"):
        out_lines.append("\n")
    out_lines.append(f"// ===== END {rel_display} =====\n\n")


def generate_public_header() -> tuple[str, set[Path]]:
    visited: set[Path] = set()
    lines: list[str] = [
        "// ============================================================================\n",
        "// KerneTile + KTUI Single-File Public Header (kernetile.hpp)\n",
        "// Auto-generated by scripts/amalgamate.py — DO NOT EDIT MANUALLY.\n",
        "//\n",
        "// Usage:\n",
        '//   1. #include "kernetile.hpp" in your C++20 source files.\n',
        "//   2. Add kernetile.cpp to your project / compiler command line.\n",
        "//   3. Optional compile-time macros (define before compiling kernetile.cpp):\n",
        "//        -DKERNETILE_PURE_GDI=1        (Windows pure-GDI CPU mode, zero D3D12)\n",
        "//        -DKTUI_ENABLE_SYSTEM_TRAY=1   (Enable optional native OS system tray)\n",
        "//        -DKTUI_ENABLE_RICH_CLIPBOARD=1 (Enable optional clipboard image/file API)\n",
        "// ============================================================================\n",
        "#pragma once\n\n",
        "#if defined(_WIN32)\n",
        "#  ifndef WIN32_LEAN_AND_MEAN\n",
        "#    define WIN32_LEAN_AND_MEAN\n",
        "#  endif\n",
        "#  ifndef NOMINMAX\n",
        "#    define NOMINMAX\n",
        "#  endif\n",
        "#endif\n\n",
    ]

    public_entries = [
        ROOT / "include" / "kernetile" / "renderer.hpp",
        ROOT / "KTUI" / "include" / "ktui" / "ktui.hpp",
        ROOT / "KTUI" / "include" / "ktui" / "pal" / "system_tray.hpp",
    ]
    for entry in public_entries:
        expand_file(entry, PUBLIC_INCLUDE_DIRS, visited, lines, is_source=False)

    return "".join(lines), visited


def generate_implementation_cpp(public_visited: set[Path]) -> str:
    visited: set[Path] = set(public_visited)
    lines: list[str] = [
        "// ============================================================================\n",
        "// KerneTile + KTUI Single-File Implementation (kernetile.cpp)\n",
        "// Auto-generated by scripts/amalgamate.py — DO NOT EDIT MANUALLY.\n",
        "// ============================================================================\n\n",
        "#if defined(_WIN32)\n",
        "#  ifndef WIN32_LEAN_AND_MEAN\n",
        "#    define WIN32_LEAN_AND_MEAN\n",
        "#  endif\n",
        "#  ifndef NOMINMAX\n",
        "#    define NOMINMAX\n",
        "#  endif\n",
        "#  ifndef _CRT_SECURE_NO_WARNINGS\n",
        "#    define _CRT_SECURE_NO_WARNINGS\n",
        "#  endif\n",
        "#endif\n\n",
        '#include "kernetile.hpp"\n\n',
        "// Auto-link required Windows system libraries on MSVC / Clang-CL\n",
        "#if defined(_MSC_VER) && defined(_WIN32)\n",
        '#  pragma comment(lib, "user32.lib")\n',
        '#  pragma comment(lib, "gdi32.lib")\n',
        '#  pragma comment(lib, "imm32.lib")\n',
        '#  pragma comment(lib, "ole32.lib")\n',
        '#  pragma comment(lib, "dwrite.lib")\n',
        '#  pragma comment(lib, "windowscodecs.lib")\n',
        "#  if !defined(KERNETILE_PURE_GDI)\n",
        '#    pragma comment(lib, "d3d12.lib")\n',
        '#    pragma comment(lib, "dxgi.lib")\n',
        '#    pragma comment(lib, "d3dcompiler.lib")\n',
        "#  endif\n",
        "#  if defined(KTUI_ENABLE_SYSTEM_TRAY)\n",
        '#    pragma comment(lib, "shell32.lib")\n',
        "#  endif\n",
        "#endif\n\n",
    ]

    def append_unit(rel_path: str, *, shader_rel: str | None = None, reinclude_tp: bool = False) -> None:
        expand_file(
            ROOT / rel_path,
            PRIVATE_INCLUDE_DIRS,
            visited,
            lines,
            is_source=True,
            shader_rel=shader_rel,
            allow_reinclude_third_party=reinclude_tp,
        )
        # Guard against Windows header macro pollution between units
        lines.append(
            "#if defined(_WIN32)\n"
            "#  undef min\n"
            "#  undef max\n"
            "#  undef DrawText\n"
            "#  undef CreateWindow\n"
            "#  undef LoadImage\n"
            "#endif\n\n"
        )

    # 1. Core cross-platform engine units
    common_sources = [
        "src/tile/tile_binner.cpp",
        "src/tile/dirty_tracker.cpp",
        "src/hal/null/null_device.cpp",
        "src/text/shared_text_engine.cpp",
        "src/fx/dual_kawase_blur.cpp",
        "src/fx/clip_stack.cpp",
        "src/input/pointer_pipeline.cpp",
        "src/input/ime_state_machine.cpp",
        "src/platform/presentation_sync.cpp",
        "src/cpu/cpu_rasterizer.cpp",
        "src/renderer.cpp",
        "KTUI/src/pal/headless_backend.cpp",
        "KTUI/src/pal/platform_backend.cpp",
        "KTUI/src/pal/file_dialog.cpp",
        "KTUI/src/pal/mica_wallpaper.cpp",
        "KTUI/src/app/application.cpp",
        "KTUI/src/app/window.cpp",
    ]
    for src in common_sources:
        append_unit(src)

    # Third-party single-header implementations (nanosvg + trimmed stb_image)
    append_unit("KTUI/src/presentation/svg_engine.cpp", reinclude_tp=True)
    append_unit("KTUI/src/presentation/image_decoder.cpp", reinclude_tp=True)

    # 2. Optional desktop modules
    lines.append("#if defined(KTUI_ENABLE_SYSTEM_TRAY)\n")
    append_unit("KTUI/src/pal/system_tray.cpp")
    lines.append("#endif // KTUI_ENABLE_SYSTEM_TRAY\n\n")

    lines.append("#if defined(KTUI_ENABLE_RICH_CLIPBOARD)\n")
    append_unit("KTUI/src/pal/rich_clipboard.cpp")
    lines.append("#endif // KTUI_ENABLE_RICH_CLIPBOARD\n\n")

    # 3. Platform-specific backends (Windows / Emscripten / Android / Linux)
    lines.append("#if defined(_WIN32)\n")
    append_unit("src/text/dwrite_text_engine.cpp")
    append_unit("KTUI/src/pal/win32_backend.cpp")
    lines.append("#  if !defined(KERNETILE_PURE_GDI)\n")
    append_unit("src/hal/d3d12/d3d12_device.cpp")
    append_unit("src/compute/ui_compute_d3d12.cpp", shader_rel="shaders/ui_compute_d3d12.hlsl")
    lines.append("#  else\n")
    append_unit("src/compute/ui_compute_null.cpp")
    lines.append("#  endif\n")

    lines.append("#elif defined(__EMSCRIPTEN__)\n")
    append_unit("src/text/web_text_engine.cpp")
    append_unit("src/compute/ui_compute_web.cpp", shader_rel="shaders/ui_compute.wgsl")
    append_unit("KTUI/src/pal/web_backend.cpp")

    lines.append("#elif defined(__ANDROID__)\n")
    append_unit("src/text/android_text_engine.cpp")
    append_unit("src/compute/ui_compute_null.cpp")
    append_unit("KTUI/src/pal/android_backend.cpp")

    lines.append("#else // Linux\n")
    append_unit("src/text/pango_text_engine.cpp")
    append_unit("src/compute/ui_compute_null.cpp")
    append_unit("KTUI/src/pal/linux_backend.cpp")
    lines.append("#endif\n")

    return "".join(lines)


SYS_INCLUDE_LINE_RE = re.compile(r'^\s*#\s*include\s*<[^>]+>.*$')
LOCAL_HPP_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"kernetile\.hpp"\s*$')


def strip_includes_for_module(text: str) -> str:
    """Remove #include <...> and #pragma once / #include "kernetile.hpp" when placing code inside a C++20 module."""
    out: list[str] = []
    for line in text.splitlines(keepends=True):
        if PRAGMA_ONCE_RE.match(line):
            continue
        if SYS_INCLUDE_LINE_RE.match(line):
            continue
        if LOCAL_HPP_INCLUDE_RE.match(line):
            continue
        out.append(line)
    return "".join(out)


def generate_cpp20_module(hpp_text: str, cpp_text: str) -> str:
    """Generate a single-file C++20 module (kernetile.ixx) with an exact line-numbered AI TOC and module :private;."""
    toc_placeholder = "// __KT_AI_TOC_PLACEHOLDER__"
    global_fragment = f"""// ============================================================================
// KerneTile + KTUI Single-File C++20 Module (kernetile.ixx)
// Auto-generated by scripts/amalgamate.py — DO NOT EDIT MANUALLY.
//
// Usage (just ONE file & ONE line in your C++20 code):
//   #include <string>
//   import kernetile;
//
{toc_placeholder}
//
// MSVC quick compile:
//   cl /std:c++20 /O1 /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /interface /c kernetile.ixx
//   cl /std:c++20 /O1 /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 example_module.cpp kernetile.obj /Fe:hello_mod.exe
// ============================================================================
module;

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#endif

// Standard C / C++20 headers
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <clocale>
#include <cmath>
#include <concepts>
#include <condition_variable>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwctype>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <numbers>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#  include <emmintrin.h>
#endif
#if defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
#  include <arm_neon.h>
#endif
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#  include <intrin.h>
#endif

// Platform headers
#if defined(_WIN32)
#  include <windows.h>
#  include <dwrite_1.h>
#  include <imm.h>
#  include <shellapi.h>
#  include <shlobj.h>
#  include <shobjidl.h>
#  include <wincodec.h>
#  include <wrl/client.h>
#  if !defined(KERNETILE_PURE_GDI)
#    include <d3d12.h>
#    include <d3dcompiler.h>
#    include <dxgi1_6.h>
#  endif
#  undef min
#  undef max
#  undef DrawText
#  undef CreateWindow
#  undef LoadImage
#elif defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  include <emscripten/html5.h>
#elif defined(__ANDROID__)
#  include <android/keycodes.h>
#  include <android/log.h>
#  include <android/native_window.h>
#  include <jni.h>
#else
#  include <dlfcn.h>
#  include <pango/pangocairo.h>
#  include <poll.h>
#  include <unistd.h>
#  include <X11/Xatom.h>
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <X11/cursorfont.h>
#  include <X11/extensions/XInput2.h>
#  include <X11/keysym.h>
#endif

export module kernetile;

export {{
"""

    interface_body = strip_includes_for_module(hpp_text)
    private_banner = (
        "\n} // export\n\n"
        "// ============================================================================\n"
        "// PRIVATE IMPLEMENTATION SECTION (module :private;)\n"
        "// AI / USER NOTE: STOP READING HERE — Everything below is internal C++20\n"
        "// module implementation (~38% of file) and NEVER needs to be read to use KTUI!\n"
        "// ============================================================================\n"
        "module :private;\n\n"
    )
    private_body = strip_includes_for_module(cpp_text)
    raw_text = global_fragment + interface_body + private_banner + private_body

    # Build an exact line-numbered Table of Contents so AI tools can read just the header
    # or jump straight to any declaration without ever reading implementation code.
    watch_markers = [
        ("export {  (Public API Surface Start)", "export {"),
        ("WindowDesc / Color / Rect / Enums", "// ===== BEGIN KTUI/include/ktui/pal/platform_types.hpp ====="),
        ("Property<T> Reactive Binding", "// ===== BEGIN KTUI/include/ktui/binding/property.hpp ====="),
        ("WindowTitlebar (Opt-in Pin/Theme)", "// ===== BEGIN KTUI/include/ktui/app/window_titlebar.hpp ====="),
        ("Window Class", "// ===== BEGIN KTUI/include/ktui/app/window.hpp ====="),
        ("Application Class", "// ===== BEGIN KTUI/include/ktui/app/application.hpp ====="),
        ("Layouts: Box / VStack / HStack / Grid", "// ===== BEGIN KTUI/include/ktui/widgets/box.hpp ====="),
        ("Layouts: Grid & GridItem", "// ===== BEGIN KTUI/include/ktui/widgets/grid.hpp ====="),
        ("Layouts: ScrollView / SplitView / Dock", "// ===== BEGIN KTUI/include/ktui/widgets/scroll_view.hpp ====="),
        ("Controls: Button", "// ===== BEGIN KTUI/include/ktui/widgets/button/button_logic.hpp ====="),
        ("Controls: Text / TextBox / NumberBox", "// ===== BEGIN KTUI/include/ktui/widgets/text.hpp ====="),
        ("Controls: NavigationView", "// ===== BEGIN KTUI/include/ktui/widgets/navigation_view/navigation_view_logic.hpp ====="),
        ("Controls: SettingsCard / Expander", "// ===== BEGIN KTUI/include/ktui/widgets/settings_card.hpp ====="),
        ("Controls: TabView / DataGrid / TreeView", "// ===== BEGIN KTUI/include/ktui/widgets/tab_view.hpp ====="),
        ("Dialogs: ContentDialog / FileDialog", "// ===== BEGIN KTUI/include/ktui/widgets/content_dialog.hpp ====="),
        ("Dialogs: Native FileDialog", "// ===== BEGIN KTUI/include/ktui/pal/file_dialog.hpp ====="),
        ("Media: Image (PNG/JPG/BMP/GIF) & Svg", "// ===== BEGIN KTUI/include/ktui/widgets/image.hpp ====="),
        ("module :private;  (STOP READING HERE)", "module :private;"),
    ]
    raw_lines = raw_text.splitlines()
    found: list[tuple[str, int]] = []
    for label, marker in watch_markers:
        for idx, line in enumerate(raw_lines, start=1):
            if line.strip() == marker:
                found.append((label, idx))
                break

    found.sort(key=lambda item: item[1])
    toc_header = [
        "// AI / TOKEN-SAVER NOTICE:",
        "//   Prefer reading skills/ktui/SKILL.md instead of scanning this file.",
        "//   If inspecting declarations here, NEVER read past `module :private;`.",
        "//",
        "// TABLE OF CONTENTS (Exact Line Numbers):",
    ]
    # Account for replacing the 1 placeholder line with len(toc_lines) lines
    shift = len(toc_header) + len(found) - 1
    toc_lines = toc_header + [f"//   Line {ln + shift:>6d} : {lbl}" for lbl, ln in found]
    return raw_text.replace(toc_placeholder, "\n".join(toc_lines), 1)


EXAMPLE_MODULE_CPP = """#include <string>
import kernetile;

int main() {
    ktui::Application app;
    ktui::WindowDesc desc;
    desc.title = "KerneTile KTUI C++20 Module Demo";
    desc.size = {520.0f, 320.0f};
    auto win = app.create_window(desc);
    if (!win) return 1;

    ktui::Property<std::string> status{"一行 import kernetile; 零头文件解析开销！"};
    int clicks = 0;

    win->set_content(ktui::Box {
        .padding = ktui::EdgeInsets(24.0f),
        .child = ktui::VStack {
            .align_items = ktui::AlignItems::Center,
            .spacing = 14.0f,
            .content = {
                ktui::Text { .text = "Hello, C++20 Module!", .font_size = 22.0f, .font_weight = ktui::FontWeight::SemiBold },
                ktui::Text { .font_size = 13.0f }.bind(&ktui::RenderText::set_text, status),
                ktui::Button {
                    .text = "点我计数",
                    .variant = ktui::ButtonVariant::Accent,
                    .on_click = [&]() {
                        clicks++;
                        status.set("已点击 " + std::to_string(clicks) + " 次 — import kernetile; 编译成功！");
                    }
                }
            }
        }
    });

    win->show();
    return app.run();
}
"""


EXAMPLE_HELLO_CPP = """#include "kernetile.hpp"

int main() {
    ktui::Application app;
    ktui::WindowDesc desc;
    desc.title = "KerneTile KTUI Single-File Demo";
    desc.size = {520.0f, 320.0f};
    auto win = app.create_window(desc);
    if (!win) return 1;

    ktui::Property<std::string> status{"点击下方按钮体验单文件版 KerneTile + KTUI"};
    int clicks = 0;

    win->set_content(ktui::Box {
        .padding = ktui::EdgeInsets(24.0f),
        .child = ktui::VStack {
            .align_items = ktui::AlignItems::Center,
            .spacing = 14.0f,
            .content = {
                ktui::Text { .text = "Hello, KerneTile & KTUI!", .font_size = 22.0f, .font_weight = ktui::FontWeight::SemiBold },
                ktui::Text { .font_size = 13.0f }.bind(&ktui::RenderText::set_text, status),
                ktui::Button {
                    .text = "点我计数",
                    .variant = ktui::ButtonVariant::Accent,
                    .on_click = [&]() {
                        clicks++;
                        status.set("已点击 " + std::to_string(clicks) + " 次 — 零配置单文件构建成功！");
                    }
                }
            }
        }
    });

    win->show();
    return app.run();
}
"""

README_MD = """# KerneTile + KTUI Single-File Distribution

提供两种极简接入方式，无需 CMake 子工程，按需任选其一：

## 方式一：C++20 单文件模块（推荐，仅 1 个文件 `kernetile.ixx`，增量编译极快）

把 `kernetile.ixx` 拖进 Visual Studio 2022/2026（C++20）项目，业务代码只需一行 `import kernetile;`：

- **MSVC 命令行编译（D3D12 版）**：
  ```powershell
  cl /std:c++20 /O2 /utf-8 /EHsc /interface /c kernetile.ixx
  cl /std:c++20 /O2 /utf-8 /EHsc example_module.cpp kernetile.obj /Fe:hello_mod.exe
  ```
- **MSVC 命令行编译（纯 GDI 极致低内存版，零 D3D12/DXGI）**：
  ```powershell
  cl /std:c++20 /O2 /utf-8 /EHsc /DKERNETILE_PURE_GDI=1 /interface /c kernetile.ixx
  cl /std:c++20 /O2 /utf-8 /EHsc example_module.cpp kernetile.obj /Fe:hello_mod_gdi.exe
  ```

## 方式二：传统双文件版（`kernetile.hpp` + `kernetile.cpp`，100% 兼容任意 C++20 构建系统）

- **默认 D3D12 硬件加速版**（`kernetile.cpp` 内置 `#pragma comment(lib, ...)` 自动链接系统库）：
  ```powershell
  cl /std:c++20 /O2 /utf-8 /EHsc example_hello.cpp kernetile.cpp /Fe:hello.exe
  ```
- **Windows 纯 GDI 极致低内存版**：
  ```powershell
  cl /std:c++20 /O2 /utf-8 /EHsc /DKERNETILE_PURE_GDI=1 example_hello.cpp kernetile.cpp /Fe:hello_gdi.exe
  ```

## 可选编译宏
- `-DKERNETILE_PURE_GDI=1`：Windows 纯 GDI + CPU 瓦片光栅模式（不依赖 D3D12/DXGI）
- `-DKTUI_ENABLE_SYSTEM_TRAY=1`：启用可选原生系统托盘模块
- `-DKTUI_ENABLE_RICH_CLIPBOARD=1`：启用可选剪贴板图像/文件模块
"""


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate KerneTile + KTUI single-file amalgamation")
    parser.add_argument("--out", default="dist/amalgamation", help="Output directory")
    args = parser.parse_args()

    out_dir = Path(args.out)
    if not out_dir.is_absolute():
        out_dir = ROOT / out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    hpp_text, pub_visited = generate_public_header()
    cpp_text = generate_implementation_cpp(pub_visited)
    ixx_text = generate_cpp20_module(hpp_text, cpp_text)

    hpp_path = out_dir / "kernetile.hpp"
    cpp_path = out_dir / "kernetile.cpp"
    ixx_path = out_dir / "kernetile.ixx"
    ex_path = out_dir / "example_hello.cpp"
    ex_mod_path = out_dir / "example_module.cpp"
    readme_path = out_dir / "README.md"

    hpp_path.write_text(hpp_text, encoding="utf-8", newline="\n")
    cpp_path.write_text(cpp_text, encoding="utf-8", newline="\n")
    ixx_path.write_text(ixx_text, encoding="utf-8", newline="\n")
    ex_path.write_text(EXAMPLE_HELLO_CPP, encoding="utf-8", newline="\n")
    ex_mod_path.write_text(EXAMPLE_MODULE_CPP, encoding="utf-8", newline="\n")
    readme_path.write_text(README_MD, encoding="utf-8", newline="\n")

    print(f"Generated {hpp_path} ({hpp_path.stat().st_size / 1024:.1f} KB, {len(pub_visited)} headers)")
    print(f"Generated {cpp_path} ({cpp_path.stat().st_size / 1024:.1f} KB)")
    print(f"Generated {ixx_path} ({ixx_path.stat().st_size / 1024:.1f} KB)")
    print(f"Generated {ex_path}")
    print(f"Generated {ex_mod_path}")
    print(f"Generated {readme_path}")


if __name__ == "__main__":
    main()
