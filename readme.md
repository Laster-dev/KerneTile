# KerneTile & KTUI

<h2 align="center">🚀 把内存和磁盘还给用户！</h2>

<p align="center">
  <strong>极致轻量、高性能、跨平台的 C++20 计算着色器驱动 2D 界面渲染引擎与 Fluent 2 / WinUI 3 声明式 UI 框架</strong>
</p>

<p align="center">
  <a href="https://laster-dev.github.io/KerneTile/"><img src="https://img.shields.io/badge/Live%20Demo-WebAssembly%20Online-0078D4?style=for-the-badge&logo=webassembly" alt="Live Demo"></a>
  <a href="https://github.com/Laster-dev/KerneTile/releases"><img src="https://img.shields.io/badge/Download-Single--File%20SDK-6F42C1?style=for-the-badge" alt="Single File SDK"></a>
  <a href="https://github.com/Laster-dev/KerneTile/actions/workflows/build-release.yml"><img src="https://img.shields.io/github/actions/workflow/status/Laster-dev/KerneTile/build-release.yml?branch=main&style=for-the-badge&label=Multi--Platform%20CI" alt="CI Status"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20%20(Modules%20%7C%20Zero--Dep)-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/Pure%20GDI%20RAM-~8.2%20MB-10893E?style=for-the-badge" alt="RAM">
  <img src="https://img.shields.io/badge/Min%20App-~600%20KB-10893E?style=for-the-badge" alt="Binary Size">
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Android%20%7C%20Web-brightgreen?style=for-the-badge" alt="Platforms">
</p>

> 🌐 **在线体验（WebAssembly 单文件）**：[https://laster-dev.github.io/KerneTile/](https://laster-dev.github.io/KerneTile/)  
> 📦 **零配置开箱 SDK**：前往 [GitHub Releases](https://github.com/Laster-dev/KerneTile/releases) 直接下载单文件 C++20 模块 **`kernetile.ixx`**，或双文件压缩包 **`kernetile-hpp-cpp.zip`**（含 `kernetile.hpp` + `kernetile.cpp`）

---

## 为什么选择 KerneTile & KTUI？

当一个普通的桌面工具动辄塞进 150 MB 的 Electron 浏览器和 300 MB 的内存占用时，KerneTile 选择把一切还原到本质：

| 维度 | KerneTile & KTUI | 常见跨平台框架 (Electron / Qt / Flutter) |
| :--- | :--- | :--- |
| **最小应用体积** | **~600 KB**（单 exe，零外部 DLL / VC++ Redist 依赖） | 20 MB ~ 150 MB+ |
| **完整 60+ 控件 Gallery 体积** | **~1 MB**（Clang-CL `-Oz` ThinLTO） | 35 MB ~ 180 MB+ |
| **Windows 纯 GDI 内存占用** | **~8.2 MB** 私有工作集（零 D3D12/DXGI 驱动堆） | 60 MB ~ 350 MB+ |
| **引入方式** | **1 个 `kernetile.ixx`**（`import kernetile;`）或 **2 个文件**（`.hpp + .cpp`），无需 CMake | 复杂构建链 / 包管理器 / 几十个动态库 |
| **图片与动图** | 全平台内置 **PNG / JPG / BMP / GIF（含多帧动图）** + **NanoSVG 矢量** | 需额外引入图像/SVG 依赖 |
| **桌面原生能力** | 原生文件/文件夹对话框、OS 文件拖放、窗口内控件拖拽、多窗口、IME、可选系统托盘 & 富剪贴板 | 需插件或平台胶水层 |
| **渲染后端** | **D3D12 / Vulkan / WebGPU Compute** + 同源 **多线程 SIMD CPU 瓦片光栅器**（Pure-GDI / 嵌入式直出） | 强绑定重型 2D/3D 引擎 |

---

## 目录

- [30 秒上手：复制即可编译的最小示例](#30-秒上手复制即可编译的最小示例)
- [两种极简引入方式：`kernetile.ixx` vs `kernetile.hpp + kernetile.cpp` 怎么选](#两种极简引入方式kernetileixx-vs-kernetilehpp--kernetilecpp-怎么选)
- [让 AI 帮你写 KTUI 界面（Skill 与提示词）](#让-ai-帮你写-ktui-界面skill-与提示词)
- [核心技术栈与架构总览](#核心技术栈与架构总览)
- [渲染管线：计算分块、解析 SDF 与 CPU 光栅化](#渲染管线计算分块解析-sdf-与-cpu-光栅化)
- [字体渲染、动态图集与位图/GIF 动图](#字体渲染动态图集与位图gif-动图)
- [跨平台抽象设计（HAL & PAL）](#跨平台抽象设计hal--pal)
- [为什么体积与内存如此极致](#为什么体积与内存如此极致)
- [鸿蒙 / 嵌入式 / RTOS 等小众平台移植指南](#鸿蒙--嵌入式--rtos-等小众平台移植指南)
- [KTUI 声明式控件库（WinUI 3 分类）](#ktui-声明式控件库winui-3-分类)
- [从源码完整构建（CMake）](#从源码完整构建cmake)
- [设备公益赞助说明（macOS / iOS / HarmonyOS）](#设备公益赞助说明macos--ios--harmonyos)

---

## 30 秒上手：复制即可编译的最小示例

从 [Releases](https://github.com/Laster-dev/KerneTile/releases) 直接下载 **`kernetile.ixx`**（或解压 **`kernetile-hpp-cpp.zip`**）后，将下方代码保存为 `main.cpp`，**直接一条命令即可编译出原生 Windows 桌面应用**（无需配置任何第三方库或系统 `.lib`）：

### 写法 A：C++20 模块版（仅需 1 个 `kernetile.ixx`）

```cpp
#include <string>
import kernetile;

int main() {
    ktui::Application app;
    auto win = app.create_window({ .title = "Hello KerneTile", .size = {520.0f, 320.0f} });
    if (!win) return 1;

    ktui::Property<std::string> status{"把内存和磁盘还给用户！点击按钮试试："};
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
                    .text = "点击计数 +1",
                    .variant = ktui::ButtonVariant::Accent,
                    .on_click = [&]() {
                        status.set("已点击 " + std::to_string(++clicks) + " 次");
                    }
                }
            }
        }
    });

    win->show();
    return app.run();
}
```

### 写法 B：传统头文件版（`kernetile.hpp` + `kernetile.cpp`）

只需把上面的 `import kernetile;` 换成 `#include "kernetile.hpp"`（其他代码 100% 相同）：

```cpp
#include "kernetile.hpp"
// ... 其余 main() 代码完全一致 ...
```

---

## 两种极简引入方式：`kernetile.ixx` vs `kernetile.hpp + kernetile.cpp` 怎么选

GitHub Releases 直接提供两套零配置方案（独立 **`kernetile.ixx`** 与 **`kernetile-hpp-cpp.zip`**，也可以随时通过 `python scripts/amalgamate.py` 从源码一键重新生成）：

### 方案 1：导入 C++20 单文件模块（`kernetile.ixx`）

- **Visual Studio 2022 (17.8+) / VS 2026**：
  1. 新建 C++ 空项目，在项目属性中将 **C++ 语言标准** 设为 **`ISO C++20 (/std:c++20)`**（建议同时加上 `/utf-8` 以支持中文源码字面量）。
  2. 把 **`kernetile.ixx`** 直接拖进项目源文件列表（VS 会自动识别 `.ixx` 为 C++20 模块接口并自动链接所有 Windows 系统库）。
  3. 在你的 `.cpp` 中写 `import kernetile;` 即可编译运行！
- **MSVC 命令行编译**：
  ```powershell
  # 1. 编译一次模块接口（生成 kernetile.ifc + kernetile.obj，后续改业务代码无需重编此步）
  cl /std:c++20 /O2 /utf-8 /EHsc /interface /c kernetile.ixx

  # 2. 编译你的程序（D3D12 硬件加速版，~651 KB）
  cl /std:c++20 /O2 /utf-8 /EHsc main.cpp kernetile.obj /Fe:app.exe

  # 如需 Windows 纯 GDI 极致低内存版（~8 MB 内存，零 D3D12/DXGI），第 1 步加上 /DKERNETILE_PURE_GDI=1 即可：
  cl /std:c++20 /O2 /utf-8 /EHsc /DKERNETILE_PURE_GDI=1 /interface /c kernetile.ixx
  cl /std:c++20 /O2 /utf-8 /EHsc main.cpp kernetile.obj /Fe:app_gdi.exe
  ```
- **CMake 3.28+**：
  ```cmake
  add_executable(my_app main.cpp)
  target_sources(my_app PRIVATE FILE_SET CXX_MODULES FILES kernetile.ixx)
  target_compile_features(my_app PRIVATE cxx_std_20)
  ```

### 方案 2：导入传统双文件（`kernetile.hpp` + `kernetile.cpp`）

- **Visual Studio / 任意 IDE**：把 `kernetile.hpp` 和 `kernetile.cpp` 一起拖进项目，开启 C++20 和 `/utf-8`，代码里 `#include "kernetile.hpp"` 即可（MSVC 自动 `#pragma comment(lib, ...)` 链接全部系统库）。
- **MSVC 命令行一行直接编译**：
  ```powershell
  # 默认 D3D12 版（~648 KB）
  cl /std:c++20 /O2 /utf-8 /EHsc main.cpp kernetile.cpp /Fe:app.exe

  # Windows 纯 GDI 极致低内存版（~591 KB，~8.2 MB RAM）
  cl /std:c++20 /O2 /utf-8 /EHsc /DKERNETILE_PURE_GDI=1 main.cpp kernetile.cpp /Fe:app_gdi.exe
  ```
- **任意 CMake 版本（>= 3.16）**：
  ```cmake
  add_executable(my_app main.cpp kernetile.cpp)
  target_compile_features(my_app PRIVATE cxx_std_20)
  ```

### 这两种方案怎么选？

| 对比项 | `kernetile.ixx`（C++20 模块） | `kernetile.hpp` + `kernetile.cpp`（双文件） |
| :--- | :--- | :--- |
| **文件数量** | **1 个文件** | 2 个文件 |
| **增量编译速度** | **极快**（直接读二进制 `.ifc`，改业务代码 ~0.2~1s） | 快（首次编 `.cpp` 后仅重编业务 `.cpp`，配 PCH 更快） |
| **Windows 宏污染** | **完全隔离**（`<windows.h>` 的 `min`/`max` 不泄漏） | 需注意不要在业务头里误引未设 `NOMINMAX` 的 `<windows.h>` |
| **工具链兼容性** | 需要 VS 2022 17.8+ / VS 2026 / CMake 3.28+ | **100% 兼容所有 C++20 编译器、IDE、CMake、Make、xmake** |
| **推荐场景** | **VS 2022/2026 现代项目、追求极致清爽与极速增量编译** | **跨平台项目（Linux / Web / Android）、旧构建系统、追求零折腾兼容** |

> 💡 **可选编译宏**（按需传 `-D` 开启，默认关闭保持最小体积）：
> - `KERNETILE_PURE_GDI=1`：Windows 纯 GDI + 多线程 CPU 瓦片光栅模式（零 D3D12/DXGI，~8.2 MB 内存）
> - `KTUI_ENABLE_SYSTEM_TRAY=1`：启用原生系统托盘模块（`ktui::SystemTray`）
> - `KTUI_ENABLE_RICH_CLIPBOARD=1`：启用剪贴板位图与文件列表读写（`Clipboard::set_image / get_files`）
> - `KTUI_ENABLE_INSPECTOR=1`：**仅 Debug 模式生效**（Release 下自动彻底剥离，零体积开销），启用内置 `F12` DevTools（右侧控件树、Pick 拾取、盒模型约束与 `Shift+F12` 脏瓦片热力淡出图）

---

## 让 AI 帮你写 KTUI 界面（Skill 与提示词）

本仓库内置了专门面向 AI 编程助手（Antigravity / Claude Code / Cursor / Windsurf / Copilot / ChatGPT）的完整开发指南 Skill：

- **Agent Skill 文件**：[`skills/ktui/SKILL.md`](skills/ktui/SKILL.md)  
  包含完整的 C++20 指定初始化器字段顺序防踩坑指南、全部常用布局与控件速查表、响应式 `Property<T>` 双向绑定范式、图片/GIF/文件对话框/拖放标准模板。将 `skills/ktui` 复制到你的 Agent Skills 目录即可让 AI 自动掌握 KTUI。

- **通用 AI 系统提示词（直接复制给任意大模型即可上手）**：

```text
请使用 C++20 的 KerneTile & KTUI 框架编写桌面 UI（通过 `import kernetile;` 或 `#include "kernetile.hpp"` 引入，全部符号在 `ktui::` 命名空间下）。
必须严格遵守以下规范以确保一次编译通过：
1. 控件采用 C++20 指定初始化器聚合语法（如 `ktui::Button { .text = "确定", .variant = ktui::ButtonVariant::Accent, .on_click = [...](){} }`），字段顺序必须严格按结构体声明顺序书写（例如 `ToggleSwitch` 是 `.checked` 在 `.label` 前，`Slider` 是 `.min_value, .max_value, .step, .on_change`）。
2. 状态驱动使用 `ktui::Property<T>` + `.bind(&RenderXxx::set_xxx, prop)` 或 `.bind_two_way(prop)`；严禁在回调中销毁当前正在派发事件的节点（需在回调外或状态绑定更新）。
3. 常用布局：`VStack`/`HStack`（`.justify`, `.align_items`, `.spacing`, `.content`）、`Wrap`、`Grid`、`ZStack`、`Box`（`.width`, `.height`, `.background_color`, `.border_color`, `.border_width`, `.corner_radius`, `.padding`, `.theme_card`, `.child`, `.alignment`）、`Expanded`、`Spacer`、`ScrollView`、`SplitView`、`DockPanel`。
4. 图像与动图使用 `ktui::Image { .src = "path.png/.jpg/.gif/.bmp", .width = ..., .height = ..., .stretch = ktui::ImageStretch::Uniform }`；原生弹窗用 `ktui::FileDialog::open_file / save_file / select_folder`；系统文件拖放用 `win->on_files_dropped(...)`。
5. 优先编写零异常、零 RTTI 的现代 C++20 代码（不使用 `try/catch` 和 `dynamic_cast`），Windows 编译需带 `/utf-8`。
```

---

## 核心技术栈与架构总览

```text
+-----------------------------------------------------------------------+
|  KTUI 声明式 UI 层 (KTUI/include/ktui/)                               |
|  C++20 指定初始化器 DSL · Property<T> 绑定 · Flex/Grid/DockPanel 布局 |
|  Spring 动效 · 60+ WinUI 3 控件 · W3C Pointer Events · 拖放 · IME     |
+-----------------------------------+-----------------------------------+
                                    | 扁平化 UiPrimitive[] + ClipRect[]
                                    v
+-----------------------------------------------------------------------+
|  KerneTile 渲染内核 (include/kernetile/, src/)                        |
|  16×16 Tile Binning 脏瓦片剔除 · 遮挡剔除 · 动图同槽位原地覆写        |
|  SDF 解析几何 · 解析高斯阴影 · Dual Kawase 模糊 · R8/RGBA8 动态图集   |
+------------------+----------------------------------+-----------------+
                   |                                  |
                   v                                  v
+--------------------------------------+  +-----------------------------+
|  GPU HAL (include/kernetile/hal/)    |  |  CPU 软件光栅化 (src/cpu/)  |
|  · D3D12 Compute (HLSL / DXBC)       |  |  · 持久化工作窃取线程池     |
|  · Vulkan Compute (SPIR-V)           |  |  · L1 SoA SIMD 瓦片合成     |
|  · WebGPU Compute (WGSL)             |  |  · Pure-GDI / 嵌入式直出    |
+--------------------------------------+  +-----------------------------+
                   |
                   v
+-----------------------------------------------------------------------+
|  平台抽象层 PAL (KTUI/include/ktui/pal/, KTUI/src/pal/)               |
|  Win32 (DIBSection BitBlt) · Linux (X11) · Android (NDK) · Web (WASM) |
|  原生 FileDialog · OS FileDrop · 可选 SystemTray & RichClipboard      |
+-----------------------------------------------------------------------+
```

| 模块 | 技术选型与说明 |
| :--- | :--- |
| **语言标准** | C++20（支持 `/EHs-c-` 零异常与 `/GR-` 零 RTTI，同时 100% 兼容普通 `/EHsc` 项目） |
| **着色器体系** | HLSL / DXBC（D3D12）+ SPIR-V（Vulkan）+ WGSL（WebGPU）+ 同源 CPU 光栅器 |
| **图像与矢量** | 精确裁剪 `stb_image`（PNG / JPG / BMP / 多帧 GIF 动图，48 MB 硬预算保护）+ 轻量嵌入式 NanoSVG |
| **文本引擎** | 对接系统原生排版（DirectWrite / Pango / Android / Browser Canvas），零 ICU 体积 |
| **构建与发行** | C++20 单文件模块 `kernetile.ixx`、双文件 `kernetile.hpp + kernetile.cpp`、CMake 3.20+ / Ninja |

---

## 渲染管线：计算分块、解析 SDF 与 CPU 光栅化

### 1. 16×16 Tile-Based Compute 管线
1. **图元扁平化**：控件树输出定长紧凑的 `UiPrimitive` 数组与 `ClipRect` 裁剪表。
2. **瓦片分箱、遮挡剔除与脏块追踪**：仅将与每个 16×16 瓦片相交的图元归箱；对完全覆盖瓦片的不透明实心/圆角背景执行提前遮挡剔除；帧间 FNV-1a 内容签名比对，静态区域直接跳过计算，局部交互可剔除 **99%+** 无效瓦片。
3. **计算着色器单遍合成**：每个 GPU 工作组（16×16 = 256 线程）负责一个脏瓦片，并行逐像素合成输出至离屏 `R8G8B8A8_UNORM` 存储纹理后极速呈现。

### 2. 数学解析绘制与色彩空间
- **解析 SDF 几何**：圆角矩形、描边、分割线均由无分支 2D 有向距离场逐像素解析求值，任意 DPI 缩放无锯齿。
- **单 Pass 解析高斯阴影**：基于误差函数（`erf`）四阶有理逼近，单遍输出平滑圆角软阴影。
- **线性光空间预乘 Alpha**：全链路严格遵循 sRGB ↔ 线性光转换与 Porter-Duff 预乘 Alpha 合成，杜绝半透明黑边。

### 3. 高性能多线程 CPU 瓦片光栅化（Pure-GDI / 跨平台软光栅）
- 持久化工作窃取线程池（无每帧线程创建开销）+ L1 缓存友好的 SoA 瓦片缓冲 + 图元特化快速路径（全覆盖 SIMD 填充、1:1 字形直拷、圆角中段 1D 扫描、列提升阴影 `erf`）。
- Windows 纯 GDI 模式下通过持久 `CreateDIBSection` + `BitBlt` 仅刷新脏矩形区域，无需 D3D12/DXGI 即可保持流畅交互与 **~8.2 MB** 极低内存占用。

---

## 字体渲染、动态图集与位图/GIF 动图

- **系统原生文本引擎（`src/text/`）**：DirectWrite（Windows）/ Pango（Linux）/ Android / 浏览器 Canvas（Web），零字体包体积。
- **双动态图集（R8 字形 + RGBA8 彩色位图）**：
  - 货架装箱（Shelf Packing）+ 1px 防渗色边距（字形透明清零，彩色图像边缘像素复制防双线性暗边）。
  - **动图零频闪原地覆写**：多帧 GIF 共用 1 个图集槽位，切帧时通过 `DynamicAtlas::update_region()` 原地更新像素并仅标脏动图覆盖瓦片，彻底杜绝图集膨胀、满载 `reset()` 频闪与内存泄漏。
  - **48 MB 动图硬预算**：高帧数/高分辨率 GIF 自动等比缩放与均匀抽帧（保持原速播放），4K 静态壁纸自动按 `max_dim` 降采样。

---

## 跨平台抽象设计（HAL & PAL）

- **图形硬件抽象层 HAL（`include/kernetile/hal/hal.hpp`）**：仅抽象 6 个极简纯虚接口（`GraphicsDevice`、`GpuBuffer`、`GpuTexture`、`ComputePipeline`、`CommandRecorder`、`Swapchain`），无需任何 3D 顶点/光栅状态机。
- **平台抽象层 PAL（`KTUI/include/ktui/pal/`）**：统一窗口生命周期、DPI 缩放、无边框自定义标题栏（Win11 Snap Layouts）、W3C Pointer Events、IME、原生 `FileDialog`（延迟按需加载 `shell32.dll`，零启动依赖）、OS 文件拖放（`WM_DROPFILES` / HTML5 Drop）、可选 `SystemTray` 与 `RichClipboard`。

---

## 为什么体积与内存如此极致

1. **零重型第三方依赖**：不引入 Chromium、Skia、ICU、HarfBuzz、Boost、Qt 或脚本虚拟机。
2. **可选纯 GDI 模式（`-DKERNETILE_WINDOWS_PURE_GDI=ON` / `-DKERNETILE_PURE_GDI=1`）**：彻底移除 `d3d12.dll` / `dxgi.dll` 及显卡驱动 `PAGE_WRITECOMBINE` 堆分配，将 Windows 私有工作集压至 **~8.2 MB**。
3. **零异常 + 零 RTTI + SDF 解析绘制**：消除展开表与皮肤贴图资源冗余；所有重型桌面集成（如系统托盘、富剪贴板）默认编译期关闭，按需开启。
4. **Hybrid CRT 与 ThinLTO 链接裁剪**：Windows 零 VC++ Redist 依赖，未使用的控件与解码器自动 `/OPT:REF` 零字节裁剪。

---

## 鸿蒙 / 嵌入式 / RTOS 等小众平台移植指南

KerneTile 渲染内核为纯标准 C++20，内置多线程/单线程 CPU 瓦片光栅器，接入任何新平台（**HarmonyOS / OpenHarmony、嵌入式 Linux DRM/Framebuffer、QNX、RTOS 工业屏**）只需两步：
1. **图形输出**：复用 CPU 瓦片光栅器，实现 `IPlatformWindow::present_rgba8_region()` 将脏矩形 RGBA8 刷写至屏幕 Framebuffer（如 `/dev/fb0`、DRM 或鸿蒙 `OHNativeWindow`）；带 GPU 平台可再对接 6 个 HAL Compute 接口。
2. **事件与窗口**：参考 `KTUI/src/pal/headless_backend.cpp`（百余行）实现 `IPlatformBackend` + `IPlatformWindow` 转发触摸/按键/VSync 事件即可。

---

## KTUI 声明式控件库（WinUI 3 分类）

Gallery 侧边栏完全对齐 **WinUI 3 Gallery** 官方分类与线性图标（60+ 控件与布局）：

- **基本输入**：`Button`、`ToggleButton`、`HoldConfirmButton`、`DropDownButton`、`SegmentedControl`、`CheckBox`、`ToggleSwitch`、`RadioButton`、`RadioCard`、`ComboBox`、`Slider`、`MinimapTimelineSlider`、`ColorPicker`、`RatingControl`
- **集合**：`TreeView`、`VirtualList`（10 万行 O(1) 虚拟化）、`DataGrid`、`FlipView`、`InspectorGrid`、`HexViewer`
- **日期和时间**：`CalendarDatePicker`、`TimePicker`
- **对话框和浮出控件**：`ContentDialog`、`FileDialog`（原生文件/文件夹选择）、`TeachingTip`、`SpotlightTour`
- **布局**：`Flex (HStack / VStack)`、`Wrap`、`Grid`、`ZStack & Positioned`、`Align & AspectRatio`、`SplitView`、`DockPanel`（拖拽停靠 + 独立浮动窗）、`Visibility`、`Expander`、`SettingsCard`
- **媒体**：`Image`（PNG / JPG / BMP / 多帧 GIF 动图 / 壁纸铺底）、`NanoSVG`、`PersonPicture`
- **菜单和工具栏**：`CommandBar`、`MenuFlyout`、`StatusBar`
- **导航**：`NavigationView`、`TabView`、`BreadcrumbBar`、`PipsPager`
- **状态和信息**：`InfoBar`、`InfoBadge`、`ProgressBar`、`ProgressRing`、`Toast`、`Tooltip`
- **系统**：`Drag & Drop`（OS 文件拖放 + 窗口内控件拖拽）、`Terminal`
- **文本**：`Label`、`RichTextBlock`、`TextBox`、`TextArea`、`AutoSuggestBox`、`GhostTextBox`、`SyntaxFilterBar`、`QueryFilterBuilder`、`IpPortInput`、`PasswordBox`、`NumberBox`、`ScrubNumberBox`

---

## 从源码完整构建（CMake）

```powershell
# Windows D3D12 完整版（Clang-CL + Ninja 最小体积）
cmake -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build-clang --target ktui_gallery

# Windows 纯 GDI 极致低内存版（~8.2 MB RAM，零 D3D12/DXGI）
cmake -B build-gdi -G Ninja -DCMAKE_BUILD_TYPE=Release -DKERNETILE_WINDOWS_PURE_GDI=ON `
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build-gdi --target ktui_gallery

# 一键生成单文件模块 kernetile.ixx 与双文件 kernetile.hpp + kernetile.cpp
python scripts/amalgamate.py --out dist/amalgamation

# 运行单元测试（配置时开启 -DBUILD_TESTING=ON）
ctest --test-dir build-clang --output-on-failure
```

```bash
# Linux (Clang + LLD)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target ktui_gallery

# WebAssembly (Emscripten)
emcmake cmake -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --target ktui_gallery
```

---

## 设备公益赞助说明（macOS / iOS / HarmonyOS）

目前作者个人**没有 Mac 电脑、没有 iPhone，也没有鸿蒙（HarmonyOS / OpenHarmony）真机设备**，因此以下平台的原生后端（Metal / AppKit / UIKit / 鸿蒙 NDK）暂时无法在本地完成真机开发与调试：

- **macOS**（Apple Silicon / Metal + AppKit）
- **iOS**（Metal + UIKit）
- **HarmonyOS / OpenHarmony**（Vulkan / `OHNativeWindow` + ArkUI XComponent）

如果你认可 KerneTile「把内存和磁盘还给用户」的极简理念，欢迎通过以下方式支持项目完成全平台拼图：
1. **公益赞助 / 闲置设备借测**：欢迎赞助一台二手/闲置的 Mac mini / MacBook、iPhone 或鸿蒙测试机用于项目开源适配（设备仅用于开源开发与 CI 联调，将在 README 致谢墙永久鸣谢）。
2. **社区共建 PR**：如果你手头有上述设备，非常欢迎参照上文的 [HAL & PAL 移植指南](#鸿蒙--嵌入式--rtos-等小众平台移植指南) 直接提交 PR 或在 Issue 中交流对接！
