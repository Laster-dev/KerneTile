# KerneTile & KTUI

<p align="center">
  <strong>轻量、高性能、跨平台的 C++20 计算着色器驱动 2D 界面渲染引擎与 Fluent 2 声明式 UI 框架</strong>
</p>

<p align="center">
  <a href="https://laster-dev.github.io/KerneTile/"><img src="https://img.shields.io/badge/Live%20Demo-WebAssembly%20Online-0078D4?style=for-the-badge&logo=webassembly" alt="Live Demo"></a>
  <a href="https://github.com/Laster-dev/KerneTile/actions/workflows/deploy-pages.yml"><img src="https://img.shields.io/github/actions/workflow/status/Laster-dev/KerneTile/deploy-pages.yml?branch=main&style=for-the-badge&label=GitHub%20Pages%20CI" alt="CI Status"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20%20(No%20Exceptions%20%7C%20No%20RTTI)-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/Gallery%20Binary-~1.07%20MB-10893E?style=for-the-badge" alt="Binary Size">
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Android%20%7C%20Web-brightgreen?style=for-the-badge" alt="Platforms">
</p>

> 🌐 **在线体验（WebAssembly）**：[https://laster-dev.github.io/KerneTile/](https://laster-dev.github.io/KerneTile/)

---

## 目录

- [项目简介](#项目简介)
- [核心技术栈与架构总览](#核心技术栈与架构总览)
- [渲染管线：计算分块、解析 SDF 与 CPU 光栅化](#渲染管线计算分块解析-sdf-与-cpu-光栅化)
- [字体渲染与动态图集](#字体渲染与动态图集)
- [跨平台抽象设计（HAL & PAL）](#跨平台抽象设计hal--pal)
- [为什么编译体积如此之小（~1 MB）](#为什么编译体积如此之小1-mb)
- [鸿蒙 / 嵌入式 / RTOS 等小众平台移植指南](#鸿蒙--嵌入式--rtos-等小众平台移植指南)
- [KTUI 声明式控件库](#ktui-声明式控件库)
- [快速构建与运行](#快速构建与运行)
- [设备公益赞助说明（macOS / iOS / HarmonyOS）](#设备公益赞助说明macos--ios--harmonyos)

---

## 项目简介

**KerneTile** 是一套完全使用现代 **C++20**（零异常、零 RTTI）从零构建的 2D UI 渲染引擎与声明式界面框架：

- **KerneTile（渲染内核）**：面向现代图形硬件的计算着色器驱动（Compute-Shader-Driven）**16×16 Tile-Based** 2D 渲染管线，同时提供与 GPU 数学语义完全一致的 **纯 CPU 瓦片光栅化后端**。
- **KTUI（界面框架）**：基于 C++20 指定初始化器的声明式 UI DSL，内置 `Property<T>` 细粒度响应式绑定、Spring 弹簧物理动效、自研 Fluent 2 Mica / Acrylic 材质与 **60+ 原生桌面/工程控件**。
- **极致体积**：包含全部 60+ 控件与完整交互展示页的 `ktui_gallery.exe`（Windows x64 Release，无外部 VC++ Redist 依赖）单文件仅 **~1.07 MB**。

---

## 核心技术栈与架构总览

```text
+-----------------------------------------------------------------------+
|  KTUI 声明式 UI 层 (KTUI/include/ktui/)                               |
|  C++20 指定初始化器 DSL · Property<T> 绑定 · Flex/Grid/DockPanel 布局 |
|  Spring 动效 · 60+ Fluent 2 控件 · W3C Pointer Events · IME 状态机    |
+-----------------------------------+-----------------------------------+
                                    | 扁平化 UiPrimitive[] + ClipRect[]
                                    v
+-----------------------------------------------------------------------+
|  KerneTile 渲染内核 (include/kernetile/, src/)                        |
|  16×16 Tile Binning 脏瓦片剔除 · 无锁环形提交 (vring)                 |
|  SDF 解析几何 · 解析高斯阴影 · Dual Kawase 模糊 · Shelf 动态字形图集  |
+------------------+----------------------------------+-----------------+
                   |                                  |
                   v                                  v
+--------------------------------------+  +-----------------------------+
|  GPU HAL (include/kernetile/hal/)    |  |  CPU 软件光栅化 (src/cpu/)  |
|  · D3D12 Compute (HLSL / DXIL)       |  |  · 同源 16×16 瓦片光栅化    |
|  · Vulkan Compute (SPIR-V)           |  |  · 同源 SDF / 阴影 / 混合   |
|  · WebGPU Compute (WGSL)             |  |  · 零 GPU / 嵌入式直出 RGBA |
|  · Slang 单源码多后端着色器           |  +-----------------------------+
+--------------------------------------+
                   |
                   v
+-----------------------------------------------------------------------+
|  平台抽象层 PAL (KTUI/include/ktui/pal/, KTUI/src/pal/)               |
|  Win32 · Linux (X11/Wayland) · Android (NDK) · Web (Emscripten)       |
|  Headless (无头单元测试)                                              |
+-----------------------------------------------------------------------+
```

| 模块 | 技术选型与说明 |
| :--- | :--- |
| **语言标准** | C++20，强制关闭异常（`/EHs-c-` / `-fno-exceptions`）与 RTTI（`/GR-` / `-fno-rtti`） |
| **着色器体系** | Slang 统一源码（`shaders/*.slang`）+ HLSL/DXIL（D3D12）+ WGSL（WebGPU） |
| **矢量图标** | 轻量嵌入式 NanoSVG（无 Cairo/Skia 依赖），矢量路径按需光栅化进图集 |
| **文本引擎** | 优先对接系统原生文本排版（DirectWrite / Pango / Android / Browser Canvas），零 ICU 体积 |
| **构建系统** | CMake 3.20+ / Ninja / `clang-cl` / Clang / Emscripten / Android NDK / `zig c++` |

---

## 渲染管线：计算分块、解析 SDF 与 CPU 光栅化

### 1. 16×16 Tile-Based Compute 管线
传统 2D 渲染往往依赖大量三角形网格提交与多次状态切换。KerneTile 将整屏划分为 **16×16 像素瓦片网格**：
1. **图元扁平化**：控件树输出定长紧凑的 `UiPrimitive` 数组（圆角矩形、边框、渐变、阴影、文本字形、图标等）与 `ClipRect` 裁剪表。
2. **瓦片分箱与脏块剔除（Tile Binning & Dirty Tracking）**：仅将与每个 16×16 瓦片相交的图元索引归箱，并对比帧间瓦片内容签名，静态区域直接跳过计算；局部交互（如光标闪烁、按钮悬停）可剔除 **99%+** 的无效瓦片。
3. **计算着色器单遍合成**：每个 GPU 工作组（16×16 = 256 线程）负责一个脏瓦片，协同加载图元索引后并行逐像素合成，输出至离屏 `R8G8B8A8_UNORM` 存储纹理，最后通过极简全屏 Blit 呈现至系统交换链（彻底规避 sRGB 交换链禁用 UAV / `VK_IMAGE_USAGE_STORAGE_BIT` 的驱动兼容性坑）。

### 2. 数学解析绘制与色彩空间
- **解析 SDF 几何**：圆角矩形、描边、分割线均由无分支 2D 有向距离场（Signed Distance Field）逐像素解析求值，任意 DPI 缩放下无折线锯齿。
- **单 Pass 解析高斯阴影**：基于误差函数（`erf`）四阶有理逼近（最大误差约 0.21 LSB），单遍计算即可得到平滑圆角软阴影，无需多遍模糊离屏缓冲。
- **Dual Kawase 背景模糊**：用于 Fluent 2 Acrylic / Mica 材质，通过多级降采样 + 升采样显著降低大半径模糊带宽。
- **线性光空间预乘 Alpha**：全链路严格遵循 IEC 61966-2-1 sRGB ↔ 线性光转换与 Porter-Duff 预乘 Alpha 合成，杜绝半透明边缘黑边。

### 3. 纯 CPU 瓦片光栅化（Software Fallback）
- 位于 `src/cpu/`，与 GPU Compute 路径共享同一套图元编码、16×16 瓦片分箱、脏区域剔除、解析 SDF 与预乘混合公式。
- 无独立显卡、虚拟机、远程桌面、Web 无 WebGPU 环境或裸机嵌入式屏幕下，无缝输出 `RGBA8` 像素缓冲并通过 `present_rgba8_region()` 局部刷新到屏幕。

---

## 字体渲染与动态图集

KerneTile 刻意避开了传统跨平台框架动辄几十 MB 的全套 `FreeType + HarfBuzz + ICU` 捆绑包，而是采用 **系统原生文本引擎桥接 + 统一动态字形图集** 方案：

- **多平台系统文本引擎（`src/text/`）**：
  - **Windows**：DirectWrite（`dwrite_text_engine.cpp`）— 原生支持 CJK、ClearType/灰度抗锯齿与系统字体回退。
  - **Linux**：Pango/Cairo（`pango_text_engine.cpp`）— 复用系统桌面字体配置与复杂脚本塑形。
  - **Android**：Android 系统字体引擎（`android_text_engine.cpp`）。
  - **Web / WASM**：浏览器原生 2D Canvas 文本光栅化（`web_text_engine.cpp`），零字体文件下载开销。
- **Shelf Packing 动态图集（`src/text/shared_text_engine.cpp`）**：
  - 使用货架装箱（Shelf Packing）在线缓存高频字形，四周保留 1px 安全边距防止采样渗色。
  - 图集尺寸可按窗口级别独立配置（主窗口默认 `1024×1024`，次级工具窗口/浮动停靠窗自动降为 `512×512`，窗口关闭立即释放显存与帧缓冲，彻底杜绝多窗口显存膨胀）。
- **Stem Darkening（笔画加粗补偿）**：
  - 在线性光空间合成抗锯齿小字号文字时，自动施加笔画加粗补偿，兼顾低 DPI 屏幕的清晰度与字重饱满感。

---

## 跨平台抽象设计（HAL & PAL）

KerneTile 将“图形硬件”与“操作系统窗口/事件”彻底解耦为两层极简接口：

### 1. 图形硬件抽象层 HAL（`include/kernetile/hal/hal.hpp`）
仅抽象 6 个极简纯虚接口，不暴露任何复杂 3D 光栅管线状态机：

| 接口类 | 职责 |
| :--- | :--- |
| `GraphicsDevice` | 创建缓冲/纹理/计算管线/交换链与提交命令 |
| `GpuBuffer` | 结构化存储缓冲（SRV / Upload / Readback） |
| `GpuTexture` | 离屏存储纹理（UAV）与字形/图像图集纹理 |
| `ComputePipeline` | 封装单个 Compute Shader 管线状态（PSO） |
| `CommandRecorder` | `resource_barrier`、`set_compute_pipeline`、`set_storage_buffer/texture`、`dispatch`、`blit_to_swapchain` |
| `Swapchain` | `resize` 与 `present(vsync)` |

各图形后端映射方式：
- **Direct3D 12**（`src/hal/d3d12/`）：HLSL → DXIL 预编译字节码，Root Signature + Compute PSO + 离屏 `R8G8B8A8_UNORM` UAV + 全屏 Quad Blit；支持自动回退至 Windows 内置 **WARP** 软件光栅器。
- **Vulkan**：Slang → SPIR-V 字节码，Compute Pipeline + Storage Image (`VK_IMAGE_USAGE_STORAGE_BIT`) + Present Blit。
- **WebGPU**：Slang / 手写 WGSL（`shaders/ui_compute.wgsl`），`GPUComputePipeline` + Storage Texture 渲染至 `<canvas>`。
- **Null / CPU**（`src/hal/null/` + `src/cpu/`）：无 GPU 依赖，直接由 CPU 瓦片光栅化器生成 `RGBA8` 帧缓冲。

### 2. 平台窗口与事件抽象层 PAL（`KTUI/include/ktui/pal/platform_backend.hpp`）
所有操作系统差异均收敛在 `IPlatformBackend` 与 `IPlatformWindow` 中：
- **窗口管理**：尺寸、DPI 缩放、无边框自定义标题栏命中测试（`TitlebarHitTest`，支持 Win11 Snap Layouts）、多窗口生命周期。
- **统一输入事件**：鼠标、触控、滚轮统一抽象为 W3C Pointer Events 模型（`PointerDown` / `PointerMove` / `PointerUp` / `PointerScroll`），支持 `Mouse` / `Touch` / `Pen` 设备区分。
- **文本与 IME**：统一的键盘按键与 `ImeCommit` / 预编辑组合串事件桥接（Windows IMM32 / Linux Wayland & X11 / Android InputConnection / Web DOM Input）。
- **软件呈现直通**：`IPlatformWindow::present_rgba8_region(rgba, w, h, dirty_rect)` 让 CPU 光栅化结果可直接局部刷屏。

---

## 为什么编译体积如此之小（~1 MB）

传统跨平台 GUI 方案体积动辄数十 MB 甚至上百 MB：
- **Electron**：自带 Chromium + V8 + Node.js（通常 `80–150 MB`）。
- **Qt 6**：动态库 `Qt6Core` + `Qt6Gui` + `Qt6Widgets` + ICU 数据表（通常 `30–60 MB`）。
- **Flutter / Skia**：Dart VM + Skia 完整 2D/3D 管线 + ICU（通常 `15–30 MB`）。

KerneTile 完整 Gallery 应用（含 60+ 控件与全部演示页）仅 **~1.07 MB**，核心原因如下：

1. **零重型第三方依赖**：不引入 Chromium、Skia、ICU、HarfBuzz、Boost、Qt 或脚本虚拟机；文本直接桥接操作系统原生排版引擎，SVG 仅内置极简 NanoSVG。
2. **全链路零异常 + 零 RTTI**：全局启用 `/EHs-c-` / `-fno-exceptions`、`/GR-` / `-fno-rtti` 与 `-fno-unwind-tables`，彻底消除 C++ 异常展开表（`.pdata`/`.xdata`/`.eh_frame`）与 `typeinfo` 元数据。
3. **SDF 解析绘制替代贴图资源**：所有控件、圆角、边框、阴影、滚动条与动效均由数学公式实时绘制，零皮肤图片包与主题资源冗余。
4. **高复用控件架构**：扩展输入框（如 `GhostTextBox`、`IpPortInput`、`QueryFilterBuilder`、`SyntaxFilterBar`）直接单继承原生 `RenderTextBox`，零嵌套、零 `std::regex` 模板膨胀；组合控件复用原生 `CheckBox`、`NumberBox`、`MenuFlyout`。
5. **编译器与链接器极致瘦身**：
   - Windows：`clang-cl` + `-Oz` + `-ffunction-sections` / `-fdata-sections` + ThinLTO + `/OPT:REF /OPT:ICF` + **Hybrid CRT**（静态链接 `libvcruntime`/`libcpmt` 零 VC++ Redist 依赖，动态绑定 Windows 10/11 系统自带 `ucrtbase.dll`）。
   - Linux / Android / WebAssembly：Clang + LLD + `-Oz` + ThinLTO + `--gc-sections` + `--icf=all` + `--strip-all`（WASM 启用 `emmalloc` + `-sFILESYSTEM=0` + `bulk-memory`）。

---

## 鸿蒙 / 嵌入式 / RTOS 等小众平台移植指南

**完全支持，且移植门槛极低。** 因为 KerneTile 渲染内核是纯标准 C++20（无异常、无 RTTI、无重型系统库强绑定），且内置纯 CPU 瓦片光栅化器，接入任何新平台（如 **HarmonyOS / OpenHarmony、嵌入式 Linux DRM/Framebuffer、车机 QNX、RTOS 工业屏**）只需对接以下最小接口：

### 1. 图形输出对接（二选一）

| 目标硬件条件 | 对接方式 | 需实现内容 |
| :--- | :--- | :--- |
| **无 GPU / 轻量嵌入式 / 裸机屏**（最简单，推荐起步） | 复用内置 **CPU Tile Rasterizer** | 仅需实现 `IPlatformWindow::present_rgba8_region()`，将引擎输出的 `RGBA8` 脏矩形拷贝或 DMA 刷写到屏幕 Framebuffer（如 `/dev/fb0`、Linux DRM、SPI/RGB LCD 显存或鸿蒙 `OHNativeWindow`）。 |
| **带 GPU 加速**（如鸿蒙 Vulkan / 嵌入式 GPU） | 实现 `kernetile::hal::GraphicsDevice` | 实现 `hal.hpp` 中 6 个极简 Compute 接口（`GpuBuffer`、`GpuTexture`、`ComputePipeline`、`CommandRecorder`、`Swapchain`、`GraphicsDevice`），无需实现任何 3D 顶点/光栅状态机。 |

### 2. 系统平台事件对接（`KTUI/include/ktui/pal/platform_backend.hpp`）

实现一个继承自 `ktui::IPlatformBackend` + `ktui::IPlatformWindow` 的轻量类（参考 `KTUI/src/pal/headless_backend.cpp` 仅百余行）：

1. **窗口/屏幕信息**：返回屏幕逻辑宽高 `size()` 与 `dpi_scale()`。
2. **输入事件转发**：将平台触摸屏/鼠标/按键事件包装为 `PlatformEvent`（`PointerDown/Move/Up`、`KeyDown`、`ImeCommit`）送入分发回调。
3. **帧驱动与定时**：接入平台 VSync（如鸿蒙 `OH_NativeVSync`、嵌入式硬件定时器或主循环 `pump_events`）。
4. **字体对接（可选）**：接入平台字体 API（如鸿蒙 `OH_Drawing_Typography`），或直接接轻量 `stb_truetype` / 点阵字库即可出字。
5. **剪贴板（可选）**：如无系统剪贴板可留空或使用内存剪贴板。

> 对于 **HarmonyOS / OpenHarmony (NDK)**：通过 ArkUI `XComponent` 获取 `OHNativeWindow`，使用 `OH_NativeWindow_NativeWindowRequestBuffer` 拿到像素指针，即可在几百行代码内直接跑通 KerneTile CPU 管线；后续再挂载鸿蒙原生 Vulkan Compute 即可解锁全硬件加速。

---

## KTUI 声明式控件库

KTUI 采用贴近 **WinUI 3 / Fluent 2** 设计规范的 C++20 声明式语法，所有控件在 Gallery 中均按标准职责分类组织：

- **布局系统**：`HStack` / `VStack`、`Wrap`、`Grid`、`ZStack` / `Positioned`、`Align` / `AspectRatio`、`Expanded` / `Spacer`、`SplitView`、`DockPanel`（VS 风格拖拽停靠与独立浮动窗）、`Visibility`
- **基础输入**：`Button`、`ToggleButton`、`HoldConfirmButton`（胶囊/环形长按确认）、`SegmentedControl`、`DropDownButton`、`CheckBox`、`ToggleSwitch`、`RadioButton`、`RadioCard`、`ComboBox`、`Slider`、`MinimapTimelineSlider`、`CalendarDatePicker`、`TimePicker`、`ColorPicker`、`RatingControl`
- **文本与数值**：`Label`、`TextBox`、`TextArea`、`AutoSuggestBox`、`GhostTextBox`（逐词 Tab 补全）、`SyntaxFilterBar`（Wireshark 风格语法状态灯）、`QueryFilterBuilder`（胶囊分词查询）、`IpPortInput`、`PasswordBox`、`NumberBox`、`ScrubNumberBox`
- **导航与容器**：`NavigationView`、`TabView`、`BreadcrumbBar`（支持路径段同级兄弟下拉横跳）、`Expander`、`TreeView`、`VirtualList`（10 万行 O(1) 虚拟化）、`DataGrid`、`InspectorGrid`（属性网格，可调列宽并复用原生输入控件）、`HexViewer`（动态列数 + 拖动选区 + LE/BE 解码）、`FlipView`、`PipsPager`
- **菜单、工具栏与状态**：`CommandBar`、`MenuFlyout`、`StatusBar`、`ContentDialog`、`TeachingTip`、`SpotlightTour`、`Tooltip`、`InfoBar`、`InfoBadge`、`PersonPicture`、`Toast`、`ProgressBar`、`ProgressRing`、`Svg`

```cpp
// KTUI 声明式写法示例（C++20 指定初始化器 + 响应式绑定）
window->set_content(VStack {
    .align_items = AlignItems::Stretch,
    .spacing = 12.0f,
    .content = {
        Text { .text = "Hello KerneTile", .font_size = 24.0f, .font_weight = FontWeight::SemiBold },
        Button {
            .text = "点击计数 +1",
            .variant = ButtonVariant::Accent,
            .on_click = [&]() { count.set(count.get() + 1); }
        }
    }
});
```

---

## 快速构建与运行

### 环境要求
- CMake >= 3.20、Ninja
- 支持 C++20 的编译器：Windows 推荐 **Clang (`clang-cl`)** 或 MSVC 2022+；Linux 推荐 Clang / GCC 12+ / `zig c++`；Web 需要 Emscripten SDK。

### Windows（Clang-CL + Ninja，极致体积 Release 构建）
```powershell
cmake -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build-clang --target ktui_gallery
.\build-clang\ktui_gallery.exe
```

### 运行单元测试（CTest）
```powershell
ctest --test-dir build-clang --output-on-failure
```

### Linux / WebAssembly
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

如果你认可 KerneTile 的极简轻量理念，欢迎通过以下方式支持项目完成全平台拼图：
1. **公益赞助 / 闲置设备借测**：欢迎赞助一台二手/闲置的 Mac mini / MacBook、iPhone 或鸿蒙测试机用于项目开源适配（设备仅用于开源开发与 CI 联调，将在 README 致谢墙永久鸣谢）。
2. **社区共建 PR**：如果你手头有上述设备，非常欢迎参照上文的 [HAL & PAL 移植指南](#鸿蒙--嵌入式--rtos-等小众平台移植指南) 直接提交 PR 或在 Issue 中交流对接！
