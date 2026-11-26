---
name: ktui
description: Complete self-contained guide for building lightweight, high-craft Microsoft WinUI 3 / Fluent 2 C++20 desktop, Web, and cross-platform GUIs with KerneTile & KTUI (`import kernetile;` or `kernetile.hpp` + `kernetile.cpp`). Includes one-command SDK download, WinUI 3 NavigationView + Settings conventions, full widget catalog & scenario selection guide, Hybrid CRT binary/RAM minimization flags, and exact-order C++20 struct references so AI agents can write full apps without reading kernetile.ixx.
---

# KerneTile & KTUI Development Skill

> **TOKEN-SAVER RULE FOR AI AGENTS**:
> **Do NOT read the entire `kernetile.ixx` or `kernetile.hpp`** (~70,000 lines / 2.9 MB). Everything required to choose, design, lay out, bind, and compile full applications is already documented below in exact C++20 declaration order.
> Only if you need an obscure sub-field not listed below, read **lines 1–160** of `kernetile.ixx` (the generated Table of Contents) to jump to its declaration, and **NEVER read past `module :private;`**.

- **Repository**: https://github.com/Laster-dev/KerneTile
- **Releases**: https://github.com/Laster-dev/KerneTile/releases

---

## 0. Quick Download (One-Command SDK Setup)

Before writing code in a new project, fetch the latest SDK files directly into the workspace:

### Option A: Single-File C++20 Module (`kernetile.ixx` — Recommended)

- **Windows (PowerShell)**:
  ```powershell
  curl.exe -fsSL -o kernetile.ixx https://github.com/Laster-dev/KerneTile/releases/latest/download/kernetile.ixx
  ```
- **Linux / macOS / Git Bash**:
  ```bash
  curl -fsSL -o kernetile.ixx https://github.com/Laster-dev/KerneTile/releases/latest/download/kernetile.ixx
  ```

### Option B: Dual-File (`kernetile.hpp` + `kernetile.cpp`)

- **Windows (PowerShell)**:
  ```powershell
  curl.exe -fsSL -o kernetile-hpp-cpp.zip https://github.com/Laster-dev/KerneTile/releases/latest/download/kernetile-hpp-cpp.zip && tar -xf kernetile-hpp-cpp.zip && del kernetile-hpp-cpp.zip
  ```
- **Linux / macOS / Git Bash**:
  ```bash
  curl -fsSL -o kernetile-hpp-cpp.zip https://github.com/Laster-dev/KerneTile/releases/latest/download/kernetile-hpp-cpp.zip && unzip -o kernetile-hpp-cpp.zip && rm kernetile-hpp-cpp.zip
  ```

> For the rolling `main` build, replace `releases/latest/download` with `releases/download/latest`.

---

## 1. Hard Rules (Strict Compiler & Runtime Safety)

1. **C++20 Designated Initializer Member Order**:
   - MSVC and Clang strictly require `.field = value` initializers to appear in the **exact declaration order** shown in Section 7 below. Never reorder fields.
2. **Zero Exceptions & Zero RTTI**:
   - Do **not** use `try`, `catch`, `throw`, or `dynamic_cast`. Check pointers/booleans directly. This keeps binaries compilable with `/EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /D_HAS_STATIC_RTTI=0`.
3. **Include Order with C++20 Modules**:
   - Put any standard headers (`#include <string>`, `<cmath>`, `<vector>`, etc.) **before** `import kernetile;`:
     ```cpp
     #include <string>
     #include <vector>
     import kernetile;
     ```
4. **Windows Encoding**:
   - Always pass `/utf-8` on MSVC when source code contains Chinese or Unicode text.
5. **Safe Widget / Child Mutation During Events**:
   - Never destroy a `RenderBox` inside its own `on_click` / `on_drop` if trailing hooks (like `on_drag_end`) will still touch it; drive UI updates via `ktui::Property<T>` bindings or rebuild inside `on_drag_end`.
6. **Titlebar Cleanliness (STRICT)**:
   - Custom titlebars (`WindowDesc::custom_titlebar = true`) MUST stay clean: **Icon + Title + Minimize + Maximize + Close** only.
   - **NEVER** call `win->titlebar().set_pin_button_visible(true)` or `set_theme_button_visible(true)` by default. Put Theme & Always-on-Top switches inside the `"__settings__"` page instead.

---

## 2. High-Craft 2D Fluent UI Layout & Visual Hierarchy Rules (CRITICAL)

AI-generated desktop UIs often look amateurish because controls stretch across huge windows, keypads mix boxed and unboxed buttons, and all text has equal visual weight. **Strictly enforce these 2D layout and visual design laws:**

### A. Anti-Bloat & Window Sizing Discipline

1. **Compact Viewport Proportions**:
   - Compact utilities (calculator, converter, timer, launcher): `~440–780px` width, `min_size = {360, 500}` (or `{660, 540}` with split drawer).
   - Dashboards / IDEs / Multi-pane editors: `~960–1280px` width. Never leave mostly empty whitespace around tiny controls.
2. **Visual Surface Restraint (Fluent 2 Layering)**:
   - Use **at most 2 nested visual surfaces** (`Window Mica Base` -> `Card` -> `Controls`). Do **not** wrap buttons inside bordered cards inside another bordered container.
   - Memory / toolbars (`MC / MR / M+`) use borderless `ButtonVariant::Subtle` (`corner_radius = 4.0f`).
   - Primary execute action (`=`, Save, Run) uses **1** `ButtonVariant::Accent`; numeric/primary grid uses `ButtonVariant::Standard`.

### B. 2D Spacing, Typography & Clipping Safety

1. **Consistent Spacing Scale (`4 / 8 / 12 / 16 / 24 px`)**:
   - Dense keypad/matrix: `4–6px` gap; form rows: `8–12px`; cards: `padding = EdgeInsets(14.0f, 16.0f)`, `corner_radius = 8.0f`; outer page: `EdgeInsets(20.0f, 24.0f)`.
2. **Typography Hierarchy**:
   - Hero readout / metric: `32–44px` Bold; Page title: `20–24px` SemiBold; Card header: `14–16px` SemiBold; Body: `13–14px` Regular; Muted caption/formula: `12px`, `ThemeContext::current().colors.text_secondary`.
3. **Prevent Text Wrapping & Clipping Bugs**:
   - Give toolbar buttons sufficient horizontal padding (`EdgeInsets(6.0f, 12.0f)`) and avoid squishing `HStack` items without `Spacer()`.
   - Never hardcode a tight `.height` (like `height = 36.0f`) on a `Box` containing `font_size >= 28.0f` text (`Box` clips content by default) — omit `.height` or leave at least `font_size * 1.45f`.
4. **Navigation vs Tabs vs Segment Discipline (WinUI 3)**:
   - Use **`Pivot`** (borderless underline tabs) for local panel headers (`[ 历史记录 | 内存 ]`, `[ 日期差值 | 加减天数 ]`) — **NEVER** use boxed `SegmentedControl` for panel headers!
   - Reserve **`SegmentedControl`** strictly for compact form/card discrete toggles (e.g., `浅色 / 深色`, `列表 / 网格`).

---

## 3. Microsoft WinUI 3 App Shell & Settings Conventions

Unless the user asks for a bare single-widget demo, structure every desktop application to match **Microsoft WinUI 3 / Fluent 2** standards:

1. **Shell & Material**:
   - Use `NavigationView` with `NavigationPaneDisplayMode::Auto` (or `LeftCompact` for canvas/editor tools), `is_back_button_visible = true`, and **`is_settings_visible = true`**.
   - Keep `WindowBackdrop::Mica` enabled (default) and group sections into `Box { .theme_card = true, .corner_radius = 8.0f, ... }` or `SettingsCard`.
   - Set a crisp titlebar icon symbol: `win->titlebar().set_icon_text("❖");`.
2. **Mandatory Settings Route (`page_id == "__settings__"`)**:
   Whenever `NavigationView` is used, **always implement `"__settings__"`** inside `ScrollView` using `SettingsCard` and `SettingsExpander`:
   - **外观与材质 (Appearance & Backdrop)**: Theme (`ToggleSwitch` -> `app.set_theme(...)`), Backdrop (`ComboBox` -> `win->set_backdrop(...)`)
   - **窗口行为 (Window Behavior)**: Always on Top (`ToggleSwitch` -> `win->set_always_on_top(on)`), Titlebar shortcuts (`ToggleSwitch` -> `win->titlebar().set_pin_button_visible(on)` / `set_theme_button_visible(on)`)
   - **关于本应用 (About)** (`SettingsExpander`): version, `"由 KerneTile & KTUI 强力驱动"`, shortcuts.

---

## 4. Binary Size & Memory Minimization ("把内存和磁盘还给用户")

KerneTile + KTUI achieves **~500–650 KB** single-file Windows executables with **zero VC++ Redistributable dependencies** and **~8.2 MB RAM** (Pure-GDI). Unused widgets are automatically dead-stripped by `/GL` + `/OPT:REF`.

> **CRITICAL PITFALL (Why some builds balloon from ~550 KB to >1.1 MB)**:
> Passing plain `cl ... main.cpp kernetile.obj` defaults to **pure static `/MT`**, which embeds the entire `libucrt.lib` (~350–450 KB) into the `.exe`!
> **Always use KerneTile's Hybrid CRT** (`/MT` + `/NODEFAULTLIB:libucrt.lib ucrt.lib`): it statically links `libvcruntime`/`libcpmt` (so users need NO VC++ Redist) while dynamically calling `ucrtbase.dll` built into every Windows 10/11 OS!

### A. Build Profiles (MSVC, Clang-CL & Linux)

- **Smallest Single-File Module (`kernetile.ixx`) — MSVC D3D12 + Hybrid CRT (~530–650 KB)**:
  ```powershell
  cl /nologo /std:c++20 /O1 /Ob1 /GL /GF /Gy /Gw /GS- /Zc:inline /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /D_HAS_STATIC_RTTI=0 /interface /c kernetile.ixx
  cl /nologo /std:c++20 /O1 /Ob1 /GL /GF /Gy /Gw /GS- /Zc:inline /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /D_HAS_STATIC_RTTI=0 main.cpp kernetile.obj /Fe:app.exe /link /LTCG /OPT:REF /OPT:ICF=5 /INCREMENTAL:NO /NODEFAULTLIB:libucrt.lib ucrt.lib
  ```

- **Ultra-Low-RAM Pure-GDI Mode (~8.2 MB RAM, ~500–590 KB, zero `d3d12.dll`/`dxgi.dll`)**:
  ```powershell
  cl /nologo /std:c++20 /O1 /Ob1 /GL /GF /Gy /Gw /GS- /Zc:inline /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /D_HAS_STATIC_RTTI=0 /DKERNETILE_PURE_GDI=1 /interface /c kernetile.ixx
  cl /nologo /std:c++20 /O1 /Ob1 /GL /GF /Gy /Gw /GS- /Zc:inline /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /D_HAS_STATIC_RTTI=0 main.cpp kernetile.obj /Fe:app_gdi.exe /link /LTCG /OPT:REF /OPT:ICF=5 /INCREMENTAL:NO /NODEFAULTLIB:libucrt.lib ucrt.lib
  ```

- **Dual-File (`kernetile.hpp` + `kernetile.cpp`) Equivalent**:
  ```powershell
  cl /nologo /std:c++20 /O1 /Ob1 /GL /GF /Gy /Gw /GS- /Zc:inline /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /D_HAS_STATIC_RTTI=0 main.cpp kernetile.cpp /Fe:app.exe /link /LTCG /OPT:REF /OPT:ICF=5 /INCREMENTAL:NO /NODEFAULTLIB:libucrt.lib ucrt.lib
  ```

- **Clang-CL Extreme Size Mode (~420–520 KB, with Dual-File `kernetile.cpp`)**:
  ```powershell
  clang-cl /nologo /std:c++20 /MT /GS- /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /D_HAS_STATIC_RTTI=0 -Oz -flto -ffunction-sections -fdata-sections -fmerge-all-constants -fno-asynchronous-unwind-tables -fno-unwind-tables main.cpp kernetile.cpp /Fe:app.exe /link /OPT:REF /OPT:ICF /INCREMENTAL:NO /NODEFAULTLIB:libucrt.lib ucrt.lib
  ```

- **Linux (Clang + LLD)**:
  ```bash
  clang++ -std=c++20 -Oz -flto=thin -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections main.cpp kernetile.cpp -o app -Wl,--gc-sections -Wl,--icf=all -s $(pkg-config --cflags --libs pangocairo) -lX11 -lXi -lpthread -ldl
  ```

### B. User-Code Size & Memory Discipline

1. **Avoid `<iostream>`, `<sstream>`, `<regex>`, `<filesystem>`**: `<iostream>` + `<regex>` alone inflate binaries by **+200–400 KB**. Use `<cstdio>` (`std::snprintf`), `<string>`, `<string_view>`, `<charconv>`, `<cmath>`.
2. **Avoid heavy `std::variant` / deep template instantiations** in UI pages; drive state through `ktui::Property<T>` bindings.
3. **Never ship `/MT` without `/NODEFAULTLIB:libucrt.lib ucrt.lib`** on MSVC Release builds unless targeting pre-Win10 systems.
4. **Optional OS modules stay OFF by default**: Only define `-DKTUI_ENABLE_SYSTEM_TRAY=1` or `-DKTUI_ENABLE_RICH_CLIPBOARD=1` when using `SystemTray` or `Clipboard::set_image / get_files`.
5. **Large Images/GIFs**: `ImageBuffer::load_file(path, max_dim)` caps GIF memory at 48 MB and downscales huge bitmaps automatically (default `max_dim = 2048`).

---

## 5. High-Craft WinUI 3 App Template (`NavigationView` + `Pivot` + Settings)

```cpp
#include <string>
import kernetile; // or #include "kernetile.hpp"

int main() {
    ktui::Application app;
    ktui::WindowDesc desc;
    desc.title = "My WinUI 3 App";
    desc.size = {880.0f, 620.0f};
    desc.min_size = {520.0f, 420.0f};

    auto win = app.create_window(desc);
    if (!win) return 1;

    win->titlebar().set_icon_text("❖");

    ktui::Property<std::string> nav_selected{"home"};
    ktui::Property<int> sub_tab{0};
    ktui::Property<bool> is_dark{true};
    ktui::Property<bool> always_on_top{false};
    ktui::Property<int> backdrop_idx{0}; // 0=Mica, 1=MicaAlt, 2=None

    auto build_settings_page = [&]() -> ktui::WidgetPtr {
        return ktui::ScrollView {
            .child = ktui::Box {
                .padding = ktui::EdgeInsets(20.0f, 28.0f),
                .child = ktui::VStack {
                    .align_items = ktui::AlignItems::Stretch,
                    .spacing = 12.0f,
                    .content = {
                        ktui::Text { .text = "设置", .font_size = 22.0f, .font_weight = ktui::FontWeight::SemiBold },
                        ktui::SettingsCard {
                            .header = "应用主题",
                            .description = "在浅色与深色 Fluent 2 主题之间即时切换（带波纹过渡）",
                            .content = ktui::ToggleSwitch {
                                .checked = is_dark.get(),
                                .label = "深色模式",
                                .on_change = [&](bool dark) {
                                    app.set_theme(dark ? ktui::Theme::dark() : ktui::Theme::light());
                                }
                            }.bind_two_way(is_dark)
                        },
                        ktui::SettingsCard {
                            .header = "窗口背景材质",
                            .description = "调整 Windows 11 原生云母 (Mica) 与纯色背景效果",
                            .content = ktui::ComboBox {
                                .items = {
                                    ktui::ComboBoxItem{ .label = "Mica 云母" },
                                    ktui::ComboBoxItem{ .label = "MicaAlt 深层云母" },
                                    ktui::ComboBoxItem{ .label = "纯色背景" }
                                },
                                .selected_index = backdrop_idx.get(),
                                .width = 160.0f,
                                .on_change = [&](int idx, const std::string&) {
                                    static constexpr ktui::WindowBackdrop kMap[] = {
                                        ktui::WindowBackdrop::Mica,
                                        ktui::WindowBackdrop::MicaAlt,
                                        ktui::WindowBackdrop::None
                                    };
                                    if (idx >= 0 && idx < 3) win->set_backdrop(kMap[idx]);
                                }
                            }.bind_two_way(backdrop_idx)
                        },
                        ktui::SettingsCard {
                            .header = "窗口置顶",
                            .description = "使应用窗口始终保持在其他窗口最前方",
                            .content = ktui::ToggleSwitch {
                                .checked = always_on_top.get(),
                                .on_change = [&](bool on) { win->set_always_on_top(on); }
                            }.bind_two_way(always_on_top)
                        },
                        ktui::SettingsExpander {
                            .header = "关于本应用",
                            .description = "v1.0.0 — 由 KerneTile & KTUI (C++20) 强力驱动",
                            .expanded = true,
                            .items = {
                                ktui::SettingsCard {
                                    .header = "极致轻量引擎",
                                    .description = "零外部依赖 / 16x16 Tile 光栅管线 / 支持 D3D12 与 ~8MB Pure-GDI",
                                    .is_nested = true
                                }
                            }
                        }
                    }
                }
            }
        };
    };

    win->set_content(ktui::NavigationView {
        .menu_items = {
            ktui::NavigationViewItem{ .id = "home", .label = "主页", .icon = ktui::NavigationIcon::Home }
        },
        .selected_id = "home",
        .pane_title = "My App",
        .is_back_button_visible = true,
        .is_settings_visible = true,
        .page_builder = [&](const std::string& id) -> ktui::WidgetPtr {
            if (id == "__settings__") return build_settings_page();
            return ktui::ScrollView {
                .child = ktui::Box {
                    .padding = ktui::EdgeInsets(20.0f, 28.0f),
                    .child = ktui::VStack {
                        .align_items = ktui::AlignItems::Stretch,
                        .spacing = 14.0f,
                        .content = {
                            ktui::HStack {
                                .align_items = ktui::AlignItems::Center,
                                .content = {
                                    ktui::Text { .text = "主页", .font_size = 22.0f, .font_weight = ktui::FontWeight::SemiBold },
                                    ktui::Spacer {},
                                    ktui::Pivot {
                                        .items = {
                                            ktui::PivotItem{ .header = "概览" },
                                            ktui::PivotItem{ .header = "活动记录" }
                                        }
                                    }.bind_two_way(sub_tab)
                                }
                            },
                            ktui::Box {
                                .theme_card = true,
                                .corner_radius = 8.0f,
                                .padding = ktui::EdgeInsets(16.0f, 20.0f),
                                .child = ktui::Text { .text = "在此构建具备清晰视觉层级的 2D Fluent 界面。", .font_size = 14.0f }
                            }
                        }
                    }
                }
            };
        }
    }.bind_two_way(nav_selected));

    win->show();
    return app.run();
}
```

---

## 6. Widget Catalog & Scenario Selection Playbook ("什么场景该用什么控件")

Always pick the idiomatic WinUI 3 / Fluent 2 widget for the interaction instead of forcing generic buttons or bordered boxes:

### 6.1 导航与视图切换 (Navigation & Tabs — Do NOT Mix Up!)

| 场景 | 正确控件 | 为什么 / 注意事项 |
| :--- | :--- | :--- |
| 应用主侧边栏 / 顶部主导航 + 自动设置页 | `NavigationView` | 支持响应式折叠、返回栈与固定 `"__settings__"` 入口 |
| 局部面板 / 侧栏头部切换（计算器 `[历史记录 \| 内存]`、`[差值 \| 加减天数]`） | **`Pivot`** | **WinUI 3 无边框下划线页签**，自然贴合文字宽 + 主题色滑动下划线，可只作头部或配 `.content_builder` |
| 设置项/卡片/工具条内的紧凑互斥开关（`浅色 \| 深色`、`列表 \| 网格`） | `SegmentedControl` | 轻量凹槽 + 悬浮卡片滑块，只适合 2~4 个短名词枚举，不要当页签头部 |
| 多文档 / 浏览器 / 终端可关闭与可新建标签 | `TabView` | 支持标签融底、`×` 关闭与 `+` 新建 |
| 文件夹 / 层级路径导航 | `BreadcrumbBar` | 点击路径节点跳回并支持同级下拉 |
| 多页卡片轮播 / 图库 / 新手引导分页 | `FlipView` + `PipsPager` | 左右翻页与圆点分页器 |
| 桌面级传统菜单栏 / 紧凑操作工具栏 | `MenuBar` / `CommandBar` | 标准文件/编辑顶栏用 `MenuBar`，图标操作栏用 `CommandBar`（自动溢出折叠） |

### 6.2 布局容器 (Layout & Structure)

| 场景 | 正确控件 | 说明 |
| :--- | :--- | :--- |
| 垂直/水平线性排布 | `VStack` / `HStack` | 配合 `Expanded`、`Flexible`、`Spacer`、`AlignItems` |
| 等分键位矩阵 / 多列表格布局（计算器键盘、仪表盘卡片） | `Grid` + `GridItem` | 支持固定列数、列权重 `column_weights`、跨列 `column_span` 与自适应 `min_item_width` |
| 自动换行标签云 / 紧凑筛选器列表 | `Wrap` | 超出宽度自动换行 |
| 圆角背景卡片 / 描边 / 阴影 / 剪裁容器 | `Box` | `.theme_card = true` 自动适配深浅色 Fluent 卡片材质；注意 `clip_content` 默认开启 |
| 图层叠加 / 悬浮角标 / 绝对定位 | `ZStack` + `Positioned` | 背景层与悬浮控件叠加 |
| 内边距 / 居中 / 对齐 / 宽高比 / 显隐 / 分割线 | `Padding`, `Center`, `Align`, `AspectRatio`, `Visibility`, `Divider` | `Align` 仅在父级 Tight 约束下扩展，不会撑爆有限列宽 |
| 超出视口滚动区域 | `ScrollView` | 大表单、Settings 页面与长内容标配 |
| 可拖拽调宽/调高的双栏分屏 | `SplitView` | 主从视图、侧栏历史抽屉、代码+预览分屏 |
| IDE / 工作站多停靠面板（可拖拽停靠与拖出独立窗口） | `DockPanel` + `DockPane` | 类似 VS Code / Visual Studio 停靠工作区 |

### 6.3 操作、开关与选择 (Actions & Toggles)

| 场景 | 正确控件 | 说明 |
| :--- | :--- | :--- |
| 常规触发按钮 / 工具栏按钮 | `Button` | 主操作 `Accent`、次要 `Standard`、无边框工具键 `Subtle`、危险操作 `Danger` |
| 状态锁存开关按钮（科学计算器 `2nd`/`HYP`、格式 `B/I/U`） | `ToggleButton` | 自带按压选中状态与双向 `bool` 绑定 |
| 带下拉菜单的按钮 / 主操作+下拉扩展 | `DropDownButton` / `SplitButton` | 支持嵌入 `MenuFlyoutItem` 子菜单 |
| 高危操作防误触长按确认（格式化、清空、删库） | `HoldConfirmButton` | 支持胶囊进度条 `PillBar` 与环形 `CircularRing` |
| 开关设置项（即时生效） | `ToggleSwitch` | Windows 11 设置页标准开关 |
| 多选勾选（含半选三态） | `Checkbox` | 支持 `Unchecked/Checked/Indeterminate` |
| 单选互斥（垂直列表 / 横向 / 图标卡片） | `RadioButton`, `RadioGroup`, `RadioCard` | `RadioCard` 适合视觉主题/对齐方式等图卡单选 |
| 下拉单选枚举（5~30 项） | `ComboBox` | 节省纵向空间 |
| 连续数值拖动调整 / 区间选择 | `Slider` | 支持水平/垂直、刻度线与双滑块区间模式 `range_mode` |
| 1~5 星评分 | `RatingControl` | 支持半星与清除 |

### 6.4 文本、数值、日期与专业输入 (Text, Numeric & Pro Inputs)

| 场景 | 正确控件 | 说明 |
| :--- | :--- | :--- |
| 单行文本 / 密码 / 多行编辑器 | `TextBox`, `PasswordBox`, `TextArea` | 带标准 Fluent 2 焦点底线；`TextArea` 内置撤销重做栈 |
| 带实时下拉候选补全的搜索框 | `AutoSuggestBox` | 支持搜索图标、输入建议过滤与提交回调 |
| 命令栏 / AI 幽灵内联补全输入框（按 Tab 接受） | `GhostTextBox` | 终端命令或参数预测输入 |
| 精确数值输入 + 表达式求值 + 微调步进按钮 | `NumberBox` | 支持四则表达式、`precision` 与 `spin_button_placement` |
| 媒体/CAD 参数鼠标左右拖拽极速调参 | `ScrubNumberBox` | 按住标签左右拖拽实时调值，Shift 微调 |
| 专用 IPv4 + 端口分段输入框 | `IpPortInput` | 自动校验点分段跳跃与端口 |
| 结构化筛选芯片构建 / 类 Wireshark 语法过滤栏 | `QueryFilterBuilder` / `SyntaxFilterBar` | 支持 `key:value` 胶囊或语法高亮校验表达式 |
| 日期 / 时间 / RGBA 取色器 | `CalendarDatePicker`, `TimePicker`, `ColorPicker` | `CalendarDatePicker` 支持单选与起止日期区间 `range_mode` |
| 紧凑底部状态栏 / 波形时间轴区间缩略图滑块 | `StatusBar` / `MinimapTimelineSlider` | 日志/图表时间窗口缩放与底部状态分段 |

### 6.5 数据展示、富文本与设置页 (Data Display, Rich Content & Settings)

| 场景 | 正确控件 | 说明 |
| :--- | :--- | :--- |
| 普通文本 / 带徽章表单标签 / 选区富文本与内联代码 | `Text`, `Label`, `RichTextBlock` | `RichTextBlock` 支持粗斜体、下划线、代码块与超链接选区 |
| Microsoft 设置页条目与可折叠子项 | `SettingsCard`, `SettingsExpander`, `Expander` | 原生 WinUI 3 设置页面与可折叠区域 |
| 百万行超长列表高性能虚拟化滚动 | `VirtualList` | 仅渲染可视区 + `overscan` 行 |
| 多列可排序表格（任务管理器、账单） | `DataGrid` | 支持列宽、排序与数值右对齐 |
| 树形目录 / 嵌套大纲 | `TreeView` | 支持单选/多选勾选与 SVG 节点图标 |
| 属性检查器面板（Key-Value 双栏编辑、可重置默认值） | `InspectorGrid` | 适合调试器、节点属性、配置面板 |
| 内存/二进制文件查看与编辑（支持大端/小端预览） | `HexViewer` | 十六进制 + ASCII 双视图编辑 |
| 完整 VT100/ANSI 嵌入式终端仿真器 | `Terminal` | 支持 256/RGB 色彩、选区复制、内联搜索与 `ConPTY` |
| 图片、SVG 矢量图标与用户头像（含在线状态） | `Image`, `Svg`, `PersonPicture` | `Image` 原生解码 PNG/JPG/BMP/GIF 动图 |

### 6.6 弹窗、提示与进度反馈 (Overlays, Dialogs & Feedback)

| 场景 | 正确控件 | 说明 |
| :--- | :--- | :--- |
| 模态确认弹窗 / 表单对话框 | `ContentDialog::show(...)` | 主/次/取消三按钮异步结果 |
| OS 原生打开文件/保存/选目录对话框 | `FileDialog::open_file / save_file / select_folder` | 原生 Win32 / Linux 对话框 |
| 右键菜单 / 上下文弹出菜单 | `MenuFlyout` / `ContextMenuArea` | 支持快捷键提示、复选/单选组、子菜单与嵌套自定义控件 |
| 悬停简短说明 / 锚点功能引导气泡 / 聚光灯分步导览 | `Tooltip`, `TeachingTip`, `SpotlightTour` | 新手引导优先用 `TeachingTip` 或 `SpotlightTour` |
| 右上角自动消失轻通知 / 页面内嵌状态横幅 / 数字红点 | `Toast::show(...)`, `InfoBar`, `InfoBadge` | 短操作反馈用 `Toast`，持久警示用 `InfoBar` |
| 确定百分比进度 / 不确定加载动画 | `ProgressBar`, `ProgressRing` | 行内条用 `ProgressBar`，紧凑加载转圈用 `ProgressRing` |

---

## 7. Complete Exact-Order C++20 Struct & Enum Reference

> **All member fields below are listed in EXACT C++20 declaration order.** Omit trailing/optional fields freely, never swap two fields!

### 7.1 Core Types, Application & Window

* `Vec2 { float x{0}, y{0}; }`, `PointF` (`Vec2` alias), `EdgeInsets(all)` / `EdgeInsets(vert, horiz)` / `EdgeInsets(l, t, r, b)`
* `Color { float r, g, b, a; }` (`Color::white()`, `Color::black()`, `Color::transparent()`)
* `WindowBackdrop`: `None`, `Mica`, `MicaAlt`
* `WindowDesc`:
  `title`, `size`, `min_size`, `max_size`, `position`, `resizable`, `decorated`, `custom_titlebar`, `transparent`, `always_on_top`, `type` (`WindowType::Normal/ToolWindow/ModalDialog/Popup`), `parent_handle`
* `Application`:
  * `create_window(WindowDesc)`, `close_window(id)`, `main_window()`, `windows()`
  * `set_theme(Theme)` (smooth ripple), `set_theme_with_ripple(Theme, Vec2)`, `theme` (`Property<Theme>`)
  * `run()`, `quit(code)`, `dispatcher().post(fn)` (marshal background thread work onto UI thread)
* `Window`:
  * `set_content(WidgetPtr)`, `show()`, `hide()`, `close()`, `set_size(Vec2)`, `size()`, `center_on_screen()`
  * `set_always_on_top(bool)`, `set_backdrop(WindowBackdrop)`, `set_title(str)`, `titlebar()`
  * `on_resize(fn)`, `on_close(fn)`, `on_files_dropped(Fn<void(const std::vector<std::string>&, PointF)>)`
* `WindowTitlebar` (`win->titlebar()`):
  * `set_icon_text(str)`, `add_action(glyph, tooltip, on_click)`
  * `set_pin_button_visible(bool)` *(default false)*, `set_theme_button_visible(bool)` *(default false)*
  * `set_minimize_button_visible(bool)`, `set_maximize_button_visible(bool)`, `set_close_button_visible(bool)`
* `Property<T>`: `.get()`, `.set(v)`, `.subscribe(fn)`; bind via `.bind(&RenderCls::setter, prop)` or `.bind_two_way(prop)`
* `Clipboard`: `Clipboard::set_text(str)`, `Clipboard::get_text()`

### 7.2 Layouts & Containers

* `VStack`: `justify`, `align_items`, `spacing`, `padding`, `reverse`, `content`
* `HStack`: `justify`, `align_items`, `spacing`, `padding`, `reverse`, `content`
  *(Enums: `JustifyContent::{FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly}`, `AlignItems::{FlexStart, FlexEnd, Center, Stretch}`)*
* `Box`: `width`, `height`, `min_width`, `max_width`, `min_height`, `max_height`, `background_color`, `border_color`, `border_width`, `corner_radius`, `padding`, `clip_content`, `theme_card`, `child`, `alignment`
* `Grid`: `columns`, `min_item_width`, `column_weights`, `column_spacing`, `row_spacing`, `cell_height`, `padding`, `content`
* `GridItem`: `column_span`, `child`
* `Wrap`: `direction`, `spacing`, `run_spacing`, `alignment`, `cross_alignment`, `content`
* `ZStack`: `alignment`, `fit` (`StackFit::Loose / Expand`), `padding`, `content`
* `Positioned`: `left`, `top`, `right`, `bottom`, `width`, `height`, `child`
* `Expanded`: `flex`, `align_self`, `child`
* `Flexible`: `flex_grow`, `flex_shrink`, `fit` (`FlexFit::Loose / Tight`), `align_self`, `child`
* `Spacer`: `flex`, `min_length`
* `Align`: `alignment`, `width_factor`, `height_factor`, `child`
* `Center`: `width_factor`, `height_factor`, `child`
* `Padding`: `insets`, `child`
* `AspectRatio`: `ratio`, `child`
* `Divider`: `vertical`, `thickness`, `length`, `indent`, `end_indent`, `color`
* `Visibility`: `visible`, `maintain_size`, `child`
* `ScrollView`: `width`, `height`, `max_height`, `child`
* `SplitView`: `vertical`, `ratio`, `min_first`, `min_second`, `splitter_size`, `first`, `second`, `on_ratio_changed`
* `DockPane`: `id`, `title`, `zone` (`DockZone::Left/Right/Top/Bottom/Center`), `content`, `closable`
* `DockPanel`: `panes`, `left_width`, `right_width`, `top_height`, `bottom_height`, `padding`, `allow_detach_window`, `on_detach_pane`

### 7.3 Basic Input Controls & Toggles

* `Button`: `text`, `icon`, `variant` (`Standard/Accent/Subtle/Danger`), `size` (`Compact/Normal/Large`), `height`, `corner_radius`, `padding`, `background_color`, `hover_color`, `active_color`, `text_color`, `border_color`, `border_width`, `font_size`, `on_click`, `disabled`, `loading`, `focusable`, `enable_ripple`
* `ToggleButton`: `checked`, `text`, `svg_path`, `disabled`, `on_change`
* `HoldConfirmButton`: `text`, `holding_text`, `confirmed_text`, `hold_duration_sec`, `danger`, `shape` (`HoldConfirmShape::PillBar/CircularRing`), `diameter`, `on_confirmed`
* `SegmentedItem`: `label`, `svg_path`
* `SegmentedControl`: `items`, `selected_index`, `width`, `on_selected_index_changed`
* `DropDownButton`: `text`, `svg_path`, `items` (`std::vector<MenuFlyoutItem>`), `disabled`
* `SplitButton`: `text`, `svg_path`, `accent`, `items`, `disabled`, `on_click`
* `Checkbox`: `checked`, `state` (`CheckState::Unchecked/Checked/Indeterminate`), `is_three_state`, `label`, `size`, `corner_radius`, `box_color`, `check_color`, `border_color`, `text_color`, `font_size`, `gap`, `disabled`, `on_change`, `on_state_change`
* `ToggleSwitch`: `checked`, `label`, `disabled`, `track_width`, `track_height`, `font_size`, `gap`, `on_change`
* `RadioButton`: `selected_value`, `value`, `label`, `disabled`, `font_size`, `on_change`
* `RadioGroup`: `selected_value`, `options` (`{RadioOption{.label, .value}}`), `horizontal`, `spacing`, `font_size`, `on_change`
* `RadioCard`: `selected_value`, `value`, `label`, `icon_text`, `icon_painter`, `disabled`, `tile_width`, `tile_height`, `corner_radius`, `font_size`, `on_change`
* `ComboBoxItem`: `label`, `disabled`
* `ComboBox`: `items`, `selected_index`, `placeholder`, `width`, `height`, `item_height`, `font_size`, `max_visible_items`, `max_dropdown_height`, `disabled`, `on_change`
* `Slider`: `value`, `min_value`, `max_value`, `step`, `track_height`, `thumb_radius`, `width`, `disabled`, `on_change`, `on_change_end`, `orientation`, `height`, `tick_frequency`, `show_ticks`, `range_mode`, `range_start`, `range_end`, `on_range_changed`
* `MinimapTimelineSlider`: `samples`, `start_ratio`, `end_ratio`, `anomaly_threshold`, `axis_labels`, `on_range_changed`
* `RatingControl`: `value`, `max_rating`, `is_clear_enabled`, `is_read_only`, `allow_half`, `caption`, `on_value_changed`

### 7.4 Date, Time & Color Pickers

* `CalendarDatePicker`: `date`, `placeholder`, `header`, `min_date`, `max_date`, `width`, `disabled`, `on_date_changed`, `range_mode`, `start_date`, `end_date`, `on_range_changed` (`CalendarDate::today()`, `.format_iso()`)
* `TimePicker`: `time` (`TimeOfDay{hour, minute}`), `header`, `use_24h`, `minute_increment`, `width`, `disabled`, `on_time_changed`
* `ColorPicker`: `color`, `header`, `is_alpha_enabled`, `width`, `disabled`, `on_color_changed`

### 7.5 Text, Numeric & Developer Inputs

* `Text`: `text`, `font_size`, `color`, `font_weight` (`Thin/Light/Regular/Medium/SemiBold/Bold`), `overflow` (`Clip/Ellipsis/Wrap`)
* `Label`: `text`, `variant`, `pill`, `required`, `svg_path`, `font_size`, `disabled`, `font_weight`, `bold`, `italic`, `underline`, `strikethrough`
* `RichTextRun`: `text`, `font_size`, `font_weight`, `bold`, `italic`, `underline`, `strikethrough`, `is_code`, `color`, `highlight`, `link_url`
* `RichTextParagraph`: `runs`, `spacing_after`
* `RichTextBlock`: `paragraphs`, `runs`, `font_size`, `line_height`, `is_text_selection_enabled`, `max_width`
* `TextBox`: `text`, `placeholder`, `height`, `corner_radius`, `background_color`, `border_color`, `focused_border_color`, `text_color`, `placeholder_color`, `font_size`, `is_password`, `is_read_only`, `disabled`, `show_reveal_button`, `on_change`, `on_submit`
* `TextArea`: `text`, `placeholder`, `header`, `width`, `height`, `disabled`, `on_change`
* `PasswordBox`: `password`, `placeholder`, `header`, `reveal_mode` (`Peek/Hidden/Visible`), `max_length`, `width`, `disabled`, `on_change`, `on_submit`
* `AutoSuggestBox`: `text`, `placeholder`, `header`, `items`, `width`, `disabled`, `on_text_changed`, `on_suggestion_chosen`, `on_query_submitted`
* `GhostTextBox`: `text`, `placeholder`, `suggestions`, `predictor`, `on_changed`, `on_accepted`
* `NumberBox`: `value`, `minimum`, `maximum`, `small_change`, `large_change`, `precision`, `accepts_expression`, `spin_button_placement` (`Inline/Compact/Hidden`), `placeholder`, `header`, `width`, `disabled`, `on_value_changed`
* `ScrubNumberBox`: `label`, `value`, `min_value`, `max_value`, `step`, `precision`, `unit`, `on_changed`
* `IpPortInput`: `ip`, `port`, `show_port`, `on_changed`
* `QueryFilterField`: `field`, `label`, `options`, `pill_color`
* `QueryFilterToken`: `field`, `value`
* `QueryFilterBuilder`: `schema`, `initial_tokens`, `placeholder`, `on_changed`
* `SyntaxFilterBar`: `text`, `known_fields`, `placeholder`, `on_evaluated`

### 7.6 Navigation, Settings, Collections & Pro Viewers

* `NavigationIcon`: `None`, `Home`, `Controls`, `Inputs`, `Progress`, `ComboBox`, `Layout`, `Palette`, `Settings`, `Info`, `Star`, `Search`, `Folder`, `Code`, `Sparkles`, `BasicInput`, `Collections`, `DateTime`, `Dialogs`, `LayoutPanel`, `Media`, `MenusToolbars`, `Motion`, `Navigation`, `Scrolling`, `System`, `Text`, `Windowing`
* `NavigationViewItem`: `id`, `label`, `kind`, `icon`, `badge`, `disabled`, `expanded`, `children`, `icon_painter` (`::header(title)`, `::separator()`)
* `NavigationView`: `menu_items`, `footer_items`, `selected_id`, `pane_title`, `header`, `pane_display_mode` (`Auto/Left/LeftCompact/LeftMinimal/Top`), `is_pane_open`, `is_pane_toggle_button_visible`, `is_back_button_visible`, `is_back_enabled`, `is_settings_visible`, `is_search_box_visible`, `search_placeholder`, `settings_label`, `open_pane_length`, `compact_pane_length`, `compact_mode_threshold_width`, `expanded_mode_threshold_width`, `content`, `page_builder`, `on_selection_changed`, `on_back_requested`, `on_pane_toggled`
* `PivotItem`: `header`, `svg_path`, `id`, `disabled`
* `Pivot`: `items`, `selected_index`, `width`, `height`, `font_size`, `item_spacing`, `equal_width`, `show_divider`, `content_builder`, `on_selection_changed`
* `TabViewItem`: `id`, `header`, `svg_path`, `is_closable`
* `TabView`: `tabs`, `selected_index`, `is_add_tab_button_visible`, `height`, `content_builder`, `on_selection_changed`, `on_tab_close`, `on_add_tab`
* `SettingsCard`: `header`, `description`, `header_icon_svg`, `action_icon_svg`, `is_click_enabled`, `disabled`, `is_nested`, `content`, `on_click`
* `SettingsExpander`: `header`, `description`, `header_icon_svg`, `expanded`, `disabled`, `header_content`, `items`, `on_expanded_changed`
* `Expander`: `header`, `subtitle`, `svg_path`, `is_expanded`, `disabled`, `content_padding`, `content`, `on_expanded_changed`
* `BreadcrumbItem`: `id`, `label`, `svg_path`, `siblings`
* `BreadcrumbBar`: `items`, `font_size`, `on_item_clicked`, `on_sibling_selected`
* `FlipView`: `items`, `selected_index`, `width`, `height`, `on_selected_index_changed`
* `PipsPager`: `number_of_pages`, `selected_index`, `max_visible_pips`, `show_nav_buttons`, `on_selected_index_changed`
* `VirtualList`: `item_count`, `item_height`, `width`, `height`, `overscan`, `item_builder`
* `DataGridColumn`: `title`, `width`, `sortable`, `numeric`
* `DataGrid`: `columns`, `rows`, `row_height`, `width`, `height`, `on_row_selected`, `cell_overflow`
* `TreeViewNode`: `id`, `label`, `svg_path`, `is_expanded`, `is_checked`, `children`
* `TreeView`: `nodes`, `selection_mode` (`Single/Multiple/None`), `selected_id`, `width`, `height`, `on_selection_changed`, `on_checked_changed`
* `InspectorRow`: `key`, `value`, `default_value`, `type_tag`, `read_only`
* `InspectorGrid`: `rows`, `key_column_ratio`, `on_value_changed`
* `HexViewer`: `bytes`, `editable`, `big_endian`, `on_byte_edited`
* `Terminal`: `backend`, `controller`, `color_scheme`, `cursor_style`, `font_size`, `max_scrollback`, `copy_on_select`, `flex_grow`, `on_title_changed`, `on_exit`
* `StatusBarSlot`: `id`, `text`, `align_right`, `indicator_color`, `options`
* `StatusBar`: `slots`

### 7.7 Dialogs, Menus, Status & Notifications

* `ContentDialog::show(ContentDialogDesc{ .title = ..., .content = ..., .primary_button_text = ..., .secondary_button_text = ..., .close_button_text = "取消", .width = 420.0f, .on_result = [](ContentDialogResult r){} })` (`Primary/Secondary/Close/None`)
* `FileDialog`: `open_file(opt, cb)`, `save_file(opt, cb)`, `select_folder(opt, cb)` (`FileDialogOptions`: `kind`, `title`, `default_dir`, `default_name`, `filters`, `owner_window_handle`)
* `MenuBarItem`: `title`, `items`, `disabled`; `MenuBar`: `items`
* `CommandBarItem`: `label`, `svg_path`, `shortcut`, `is_separator`, `is_toggle`, `checked`, `disabled`, `on_click`, `on_toggle` (`::separator()`)
* `CommandBar`: `content`, `primary_commands`, `secondary_commands`, `default_label_position` (`Right/Bottom/Collapsed`), `width`
* `MenuFlyoutItem`: `label`, `shortcut`, `svg_path`, `is_separator`, `is_checkable`, `checked`, `disabled`, `on_click`, `is_radio`, `keep_open_on_click`, `on_toggle`, `sub_items`, `trailing_widget`, `custom_widget`, `custom_height` (`::separator()`)
* `ContextMenuArea`: `items`, `child`
* `TeachingTipDesc`: `title`, `subtitle`, `svg_path`, `action_text`, `close_text`, `is_light_dismiss_enabled`, `preferred_placement`, `width`
* `SpotlightStep`: `title`, `description`, `target_rect_fn`, `hole_padding`, `action_hint` (`SpotlightStep::for_node(title, desc, node)`)
* `InfoBar`: `title`, `message`, `severity` (`Informational/Success/Warning/Error`), `is_open`, `is_closable`, `action_text`, `on_action`, `on_close`
* `InfoBadge`: `value`, `severity` (`Attention/Informational/Success/Caution/Critical`), `svg_path`, `max_value`
* `ProgressBar`: `value`, `indeterminate`, `state`, `track_height`, `width`, `corner_radius`, `disabled`
* `ProgressRing`: `value`, `indeterminate`, `size`, `stroke_width`, `show_track`, `disabled`
* `Toast::show(ToastDesc{ .title = ..., .message = ..., .severity = ToastSeverity::Success })`
* `Tooltip`: `text`, `delay_sec`, `child`

### 7.8 Media (PNG / JPG / BMP / GIF), Avatar & Drag-and-Drop

* `Image`: `source`, `src` (`.png/.jpg/.jpeg/.gif/.bmp`), `bmp_path`, `max_dim`, `auto_play`, `width`, `height`, `stretch` (`ImageStretch::None/Fill/Uniform/UniformToFill`), `corner_radius`, `tint`, `opacity`, `border_color`, `border_width`
* `Svg`: `src`, `xml`, `path`, `viewbox_size`, `stroke_width`, `fill`, `width`, `height`, `tint`, `rotation`
* `PersonPicture`: `display_name`, `initials`, `diameter`, `presence`, `background_color`
* `OS File Drop`: `win->on_files_dropped([&](const std::vector<std::string>& files, ktui::PointF pos) { ... });`
* `In-Window Drag`: On `RenderBox`: `is_draggable = true`, `on_drag_start` (`ev.accepted = true; ev.payload.mime_type = ...;`), `on_drag_end`; on drop target: `is_drop_target = true`, `on_drag_enter`, `on_drag_over` (`ev.accepted = true`), `on_drag_leave`, `on_drop`.
