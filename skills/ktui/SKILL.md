---
name: ktui
description: Complete self-contained guide for building lightweight Microsoft WinUI 3 / Fluent 2 C++20 desktop, Web, and cross-platform GUIs with KerneTile & KTUI (`import kernetile;` or `kernetile.hpp` + `kernetile.cpp`). Includes one-command SDK download, WinUI 3 NavigationView + Settings conventions, binary/RAM minimization flags, and exact-order C++20 struct references so AI agents can write full apps without reading kernetile.ixx.
---

# KerneTile & KTUI Development Skill

> **TOKEN-SAVER RULE FOR AI AGENTS**:
> **Do NOT read the entire `kernetile.ixx` or `kernetile.hpp`** (~70,000 lines / 2.9 MB). Everything required to design, lay out, bind, and compile full applications is already documented below in exact C++20 declaration order.
> Only if you need an unlisted advanced widget, read **lines 1–160** of `kernetile.ixx` (the generated Table of Contents) to jump to its declaration, and **NEVER read past `module :private;`**.

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
   - MSVC and Clang strictly require `.field = value` initializers to appear in the **exact declaration order** shown in Section 5 below. Never reorder fields.
2. **Zero Exceptions & Zero RTTI**:
   - Do **not** use `try`, `catch`, `throw`, or `dynamic_cast`. Check pointers/booleans directly. This keeps binaries compilable with `/EHs-c- /GR- /D_HAS_EXCEPTIONS=0`.
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
6. **Titlebar Defaults**:
   - Custom titlebars (`WindowDesc::custom_titlebar = true`) show only **Icon + Title + Minimize + Maximize + Close** by default.
   - Pin (Always-on-top) and Theme toggle buttons on the titlebar are **off by default** — only call `win->titlebar().set_pin_button_visible(true)` or `set_theme_button_visible(true)` when explicitly desired (prefer putting Theme/Pin switches in the Settings page!).

---

## 2. Microsoft WinUI 3 App Design Conventions (Default Behavior)

Unless the user asks for a bare single-widget demo, structure every desktop application to match **Microsoft WinUI 3 / Fluent 2** app standards (like Windows Calculator, Settings, Terminal):

1. **Shell & Material**:
   - Use a `NavigationView` shell with left pane (`NavigationPaneDisplayMode::Auto` or `LeftCompact` for tool-focused apps), `is_back_button_visible = true`, and **`is_settings_visible = true`**.
   - Keep `WindowBackdrop::Mica` enabled (default on `Window`) and wrap page cards in `Box { .theme_card = true, .corner_radius = 6.0f, .padding = EdgeInsets(12.0f, 16.0f), ... }` or `SettingsCard`.
   - Set a fitting Unicode/text glyph icon on the titlebar: `win->titlebar().set_icon_text("...");`.
2. **Standard Settings Page (`page_id == "__settings__"`)**:
   Whenever `NavigationView` is used, **always implement the `"__settings__"` route** inside `ScrollView` using `SettingsCard` and `SettingsExpander`, covering:
   - **外观与材质 (Appearance & Backdrop)**:
     - App Theme selector (`ComboBox` or `SegmentedControl`: 深色 / 浅色) -> `app.set_theme(dark ? Theme::dark() : Theme::light());` (gives smooth Fluent theme ripple).
     - Window Backdrop selector (`ComboBox`: Mica 云母 / MicaAlt 深层云母 / Solid 纯色) -> `win->set_backdrop(...)`.
   - **窗口与标题栏 (Window & Titlebar)**:
     - Always on Top toggle (`ToggleSwitch`) -> `win->set_always_on_top(on)`.
     - Titlebar shortcuts toggles (`ToggleSwitch`) -> `win->titlebar().set_pin_button_visible(on)` / `set_theme_button_visible(on)`.
   - **关于本应用 (About)** (`SettingsExpander` or `SettingsCard`):
     - App title & version (e.g. `v1.0`), engine line `"由 KerneTile & KTUI 强力驱动 (C++20 / 零外部依赖)"`, keyboard shortcuts summary, and a copy/reset button with `Toast::show(...)`.
3. **Visual Polish & Ergonomics**:
   - Always wrap scrollable pages and dense grids inside `ScrollView` so resizing the window smaller never clips or crushes controls.
   - Use generous height or auto-height (omit `.height`) on `Box` containers holding large display fonts (`font_size >= 28`) because `Box` has `clip_content = true` by default.
   - Provide `Toast::show(...)` / `InfoBar` feedback on user actions, `ContentDialog::show(...)` for destructive confirmations, and `Ctrl+C`/`Ctrl+V`/hotkey ergonomics where appropriate.

---

## 3. Binary Size & Memory Minimization ("把内存和磁盘还给用户")

KerneTile + KTUI apps target **~580–700 KB** single-exe binaries and **~8.2 MB RAM** (Pure-GDI). Follow these rules to keep user builds ultra-small:

### A. Build Profiles (MSVC & Clang-CL)

- **Smallest Single-File Module (`kernetile.ixx`) — MSVC D3D12 (~620 KB)**:
  ```powershell
  cl /nologo /std:c++20 /O1 /GL /GF /Gw /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /interface /c kernetile.ixx
  cl /nologo /std:c++20 /O1 /GL /GF /Gw /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 main.cpp kernetile.obj /Fe:app.exe /link /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO
  ```
- **Ultra-Low-RAM Pure-GDI Mode (~8.2 MB Private Working Set, ~580 KB, zero `d3d12.dll`/`dxgi.dll`)**:
  Add `/DKERNETILE_PURE_GDI=1` when compiling `kernetile.ixx` or `kernetile.cpp`:
  ```powershell
  cl /nologo /std:c++20 /O1 /GL /GF /Gw /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 /DKERNETILE_PURE_GDI=1 /interface /c kernetile.ixx
  cl /nologo /std:c++20 /O1 /GL /GF /Gw /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 main.cpp kernetile.obj /Fe:app_gdi.exe /link /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO
  ```
- **Clang-CL Extreme Minimization (~500 KB)**:
  Replace `-O2` with `-Oz -flto=thin -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -Wl,/OPT:REF,/OPT:ICF`.
- **Dual-File Equivalent**:
  ```powershell
  cl /nologo /std:c++20 /O1 /GL /GF /Gw /utf-8 /EHs-c- /GR- /D_HAS_EXCEPTIONS=0 main.cpp kernetile.cpp /Fe:app.exe /link /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO
  ```
- **Linux (Clang + LLD)**:
  ```bash
  clang++ -std=c++20 -Oz -flto=thin -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections main.cpp kernetile.cpp -o app -Wl,--gc-sections -Wl,--icf=all -s $(pkg-config --cflags --libs pangocairo) -lX11 -lXi -lpthread -ldl
  ```

### B. User-Code Size Discipline (Avoid C++ Bloat)

1. **Avoid `<iostream>`, `<sstream>`, `<regex>`, `<filesystem>`** unless truly required — `<iostream>` + `<regex>` alone can inflate a binary by **+200–400 KB**! Prefer `<cstdio>` (`std::snprintf`), `<string>`, `<string_view>`, `<charconv>`, and `<cmath>`.
2. **Avoid heavy `std::variant` / deep generic instantiations** in UI pages; use `ktui::Property<T>` bindings and clean flat structs.
3. **Optional modules stay OFF by default**: Only define `-DKTUI_ENABLE_SYSTEM_TRAY=1` or `-DKTUI_ENABLE_RICH_CLIPBOARD=1` when actually using `SystemTray` or `Clipboard::set_image / get_files`.
4. **Release Images/GIFs**: `ImageBuffer::load_file(path, max_dim)` caps GIF animated frames at a 48 MB memory budget and downscales huge 4K wallpapers automatically (default `max_dim = 2048`).

---

## 4. Minimal WinUI 3 App Template (With NavigationView + Settings)

```cpp
#include <string>
import kernetile; // or #include "kernetile.hpp"

int main() {
    ktui::Application app;
    ktui::WindowDesc desc;
    desc.title = "My WinUI 3 App";
    desc.size = {900.0f, 620.0f};
    desc.min_size = {560.0f, 420.0f};
    auto win = app.create_window(desc);
    if (!win) return 1;

    win->titlebar().set_icon_text("❖");

    ktui::Property<std::string> nav_selected{"home"};
    ktui::Property<bool> is_dark{true};
    ktui::Property<bool> always_on_top{false};
    ktui::Property<int> backdrop_idx{0}; // 0=Mica, 1=MicaAlt, 2=None

    auto build_settings_page = [&]() -> ktui::WidgetPtr {
        return ktui::ScrollView {
            .child = ktui::Box {
                .padding = ktui::EdgeInsets(18.0f, 24.0f),
                .child = ktui::VStack {
                    .align_items = ktui::AlignItems::Stretch,
                    .spacing = 10.0f,
                    .content = {
                        ktui::Text { .text = "设置", .font_size = 24.0f, .font_weight = ktui::FontWeight::SemiBold },
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
                            .description = "调整 Windows 11 自算云母 (Mica) 与纯色背景效果",
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
                    .padding = ktui::EdgeInsets(18.0f, 24.0f),
                    .child = ktui::Text { .text = "欢迎使用 KTUI!", .font_size = 22.0f, .font_weight = ktui::FontWeight::SemiBold }
                }
            };
        }
    }.bind_two_way(nav_selected));

    win->show();
    return app.run();
}
```

---

## 5. Complete Exact-Order Struct & Enum Reference

> **All member fields below are listed in EXACT C++20 declaration order.** Omit trailing/optional fields freely, never swap two fields!

### 5.1 Core Types, Application & Window

- `Vec2 { float x{0}, y{0}; }`, `PointF` (`Vec2` alias), `EdgeInsets(all)` / `EdgeInsets(vert, horiz)` / `EdgeInsets(l, t, r, b)`
- `Color { float r, g, b, a; }` (`Color::white()`, `Color::black()`, `Color::transparent()`)
- `WindowBackdrop`: `None`, `Mica`, `MicaAlt`
- `WindowDesc`:
  `title`, `size`, `min_size`, `max_size`, `position`, `resizable`, `decorated`, `custom_titlebar`, `transparent`, `always_on_top`, `type` (`WindowType::Normal/ToolWindow/ModalDialog/Popup`), `parent_handle`
- `Application`:
  - `create_window(WindowDesc)`, `close_window(id)`, `main_window()`, `windows()`
  - `set_theme(Theme)` (smooth ripple), `set_theme_with_ripple(Theme, Vec2)`, `theme` (`Property<Theme>`)
  - `run()`, `quit(code)`, `dispatcher().post(fn)` (marshal background thread work onto UI thread)
- `Window`:
  - `set_content(WidgetPtr)`, `show()`, `hide()`, `close()`, `set_size(Vec2)`, `size()`, `center_on_screen()`
  - `set_always_on_top(bool)`, `set_backdrop(WindowBackdrop)`, `set_title(str)`, `titlebar()`
  - `on_resize(fn)`, `on_close(fn)`, `on_files_dropped(Fn<void(const std::vector<std::string>&, PointF)>)`
- `WindowTitlebar` (`win->titlebar()`):
  - `set_icon_text(str)`, `add_action(glyph, tooltip, on_click)`
  - `set_pin_button_visible(bool)` *(default false)*, `set_theme_button_visible(bool)` *(default false)*
  - `set_minimize_button_visible(bool)`, `set_maximize_button_visible(bool)`, `set_close_button_visible(bool)`
- `Property<T>`: `.get()`, `.set(v)`, `.subscribe(fn)`; bind via `.bind(&RenderCls::setter, prop)` or `.bind_two_way(prop)`
- `Clipboard`: `Clipboard::set_text(str)`, `Clipboard::get_text()`

### 5.2 Layouts & Containers

- `VStack`: `justify`, `align_items`, `spacing`, `padding`, `reverse`, `content`
- `HStack`: `justify`, `align_items`, `spacing`, `padding`, `reverse`, `content`
  *(Enums: `JustifyContent::{FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly}`, `AlignItems::{FlexStart, FlexEnd, Center, Stretch}`)*
- `Box`: `width`, `height`, `min_width`, `max_width`, `min_height`, `max_height`, `background_color`, `border_color`, `border_width`, `corner_radius`, `padding`, `clip_content`, `theme_card`, `child`, `alignment`
- `Grid`: `columns`, `min_item_width`, `column_weights`, `column_spacing`, `row_spacing`, `cell_height`, `padding`, `content`
- `GridItem`: `column_span`, `child`
- `Wrap`: `direction`, `spacing`, `run_spacing`, `alignment`, `cross_alignment`, `content`
- `ZStack`: `alignment`, `fit` (`StackFit::Loose / Expand`), `padding`, `content`
- `Positioned`: `left`, `top`, `right`, `bottom`, `width`, `height`, `child`
- `Expanded`: `flex`, `align_self`, `child`
- `Flexible`: `flex_grow`, `flex_shrink`, `fit` (`FlexFit::Loose / Tight`), `align_self`, `child`
- `Spacer`: `flex`, `min_length`
- `Align`: `alignment`, `width_factor`, `height_factor`, `child`
- `Padding`: `insets`, `child`
- `AspectRatio`: `ratio`, `child`
- `Visibility`: `visible`, `maintain_size`, `child`
- `ScrollView`: `width`, `height`, `max_height`, `child`
- `SplitView`: `vertical`, `ratio`, `min_first`, `min_second`, `splitter_size`, `first`, `second`, `on_ratio_changed`

### 5.3 Basic Input Controls

- `Button`: `text`, `icon`, `variant` (`Standard/Accent/Subtle/Danger`), `size` (`Compact/Normal/Large`), `height`, `corner_radius`, `padding`, `background_color`, `hover_color`, `active_color`, `text_color`, `border_color`, `border_width`, `font_size`, `on_click`, `disabled`, `loading`, `focusable`, `enable_ripple`
- `ToggleButton`: `checked`, `text`, `svg_path`, `disabled`, `on_change`
- `SegmentedItem`: `label`, `svg_path`
- `SegmentedControl`: `items`, `selected_index`, `width`, `on_selected_index_changed`
- `DropDownButton`: `text`, `svg_path`, `items` (`std::vector<MenuFlyoutItem>`), `disabled`
- `SplitButton`: `text`, `svg_path`, `accent`, `items`, `disabled`, `on_click`
- `Checkbox`: `checked`, `state` (`CheckState::Unchecked/Checked/Indeterminate`), `is_three_state`, `label`, `size`, `corner_radius`, ..., `disabled`, `on_change`, `on_state_change`
- `ToggleSwitch`: `checked`, `label`, `disabled`, `track_width`, `track_height`, `font_size`, `gap`, `on_change`
- `RadioButton`: `selected_value`, `value`, `label`, `disabled`, `font_size`, `on_change`
- `RadioGroup`: `selected_value`, `options` (`{RadioOption{.label, .value}}`), `horizontal`, `spacing`, `font_size`, `on_change`
- `RadioCard`: `selected_value`, `value`, `label`, `icon_text`, `icon_painter`, `disabled`, `tile_width`, `tile_height`, `corner_radius`, `font_size`, `on_change`
- `ComboBoxItem`: `label`, `disabled`
- `ComboBox`: `items`, `selected_index`, `placeholder`, `width`, `height`, `item_height`, `font_size`, `max_visible_items`, `max_dropdown_height`, `disabled`, `on_change`
- `Slider`: `value`, `min_value`, `max_value`, `step`, `track_height`, `thumb_radius`, `width`, `disabled`, `on_change`, `on_change_end`, `orientation`, `height`, `tick_frequency`, `show_ticks`, `range_mode`, `range_start`, `range_end`, `on_range_changed`
- `RatingControl`: `value`, `max_rating`, `is_clear_enabled`, `is_read_only`, `allow_half`, `caption`, `on_value_changed`

### 5.4 Date, Time & Color Pickers

- `CalendarDatePicker`: `date`, `placeholder`, `header`, `min_date`, `max_date`, `width`, `disabled`, `on_date_changed`, `range_mode`, `start_date`, `end_date`, `on_range_changed` (`CalendarDate::today()`, `.format_iso()`)
- `TimePicker`: `time` (`TimeOfDay{h, m}`), `header`, `use_24h`, `minute_increment`, `width`, `disabled`, `on_time_changed`
- `ColorPicker`: `color`, `header`, `is_alpha_enabled`, `width`, `disabled`, `on_color_changed`

### 5.5 Text & Number Controls

- `Text`: `text`, `font_size`, `color`, `font_weight` (`Thin/Light/Regular/Medium/SemiBold/Bold`), `overflow` (`Clip/Ellipsis/Wrap`)
- `TextBox`: `text`, `placeholder`, `height`, `corner_radius`, `background_color`, `border_color`, `focused_border_color`, `text_color`, `placeholder_color`, `font_size`, `is_password`, `is_read_only`, `disabled`, `show_reveal_button`, `on_change`, `on_submit`
- `TextArea`: `text`, `placeholder`, `header`, `width`, `height`, `disabled`, `on_change`
- `PasswordBox`: `password`, `placeholder`, `header`, `reveal_mode` (`Peek/Hidden/Visible`), `max_length`, `width`, `disabled`, `on_change`, `on_submit`
- `NumberBox`: `value`, `minimum`, `maximum`, `small_change`, `large_change`, `precision`, `accepts_expression`, `spin_button_placement` (`Inline/Compact/Hidden`), `placeholder`, `header`, `width`, `disabled`, `on_value_changed`

### 5.6 Navigation, Settings & Collections

- `NavigationIcon`: `None`, `Home`, `Controls`, `Inputs`, `Progress`, `ComboBox`, `Layout`, `Palette`, `Settings`, `Info`, `Star`, `Search`, `Folder`, `Code`, `Sparkles`, `BasicInput`, `Collections`, `DateTime`, `Dialogs`, `LayoutPanel`, `Media`, `MenusToolbars`, `Motion`, `Navigation`, `Scrolling`, `System`, `Text`, `Windowing`
- `NavigationViewItem`: `id`, `label`, `kind`, `icon`, `badge`, `disabled`, `expanded`, `children`, `icon_painter` (helpers: `::header(title)`, `::separator()`)
- `NavigationView`: `menu_items`, `footer_items`, `selected_id`, `pane_title`, `header`, `pane_display_mode` (`Auto/Left/LeftCompact/LeftMinimal/Top`), `is_pane_open`, `is_pane_toggle_button_visible`, `is_back_button_visible`, `is_back_enabled`, `is_settings_visible`, `is_search_box_visible`, `search_placeholder`, `settings_label`, `open_pane_length`, `compact_pane_length`, `compact_mode_threshold_width`, `expanded_mode_threshold_width`, `content`, `page_builder`, `on_selection_changed`, `on_back_requested`, `on_pane_toggled`
- `SettingsCard`: `header`, `description`, `header_icon_svg`, `action_icon_svg`, `is_click_enabled`, `disabled`, `is_nested`, `content`, `on_click`
- `SettingsExpander`: `header`, `description`, `header_icon_svg`, `expanded`, `disabled`, `header_content`, `items`, `on_expanded_changed`
- `Expander`: `header`, `subtitle`, `svg_path`, `is_expanded`, `disabled`, `content_padding`, `content`, `on_expanded_changed`
- `TabViewItem`: `id`, `header`, `svg_path`, `is_closable`
- `TabView`: `tabs`, `selected_index`, `is_add_tab_button_visible`, `height`, `content_builder`, `on_selection_changed`, `on_tab_close`, `on_add_tab`
- `BreadcrumbItem`: `id`, `label`, `svg_path`, `siblings`
- `BreadcrumbBar`: `items`, `font_size`, `on_item_clicked`, `on_sibling_selected`
- `DataGridColumn`: `title`, `width`, `sortable`, `numeric`
- `DataGrid`: `columns`, `rows`, `row_height`, `width`, `height`, `on_row_selected`, `cell_overflow`
- `TreeViewNode`: `id`, `label`, `svg_path`, `is_expanded`, `is_checked`, `children`
- `TreeView`: `nodes`, `selection_mode` (`Single/Multiple/None`), `selected_id`, `width`, `height`, `on_selection_changed`, `on_checked_changed`

### 5.7 Dialogs, Menus, Status & Notifications

- `ContentDialog::show(ContentDialogDesc{ .title = ..., .content = ..., .primary_button_text = ..., .secondary_button_text = ..., .close_button_text = "取消", .width = 420.0f, .on_result = [](ContentDialogResult r){} })` (`Primary/Secondary/Close/None`)
- `FileDialog`: `open_file(opt, cb)`, `save_file(opt, cb)`, `select_folder(opt, cb)` (`FileDialogOptions`: `kind`, `title`, `default_dir`, `default_name`, `filters`, `owner_window_handle`)
- `CommandBarItem`: `label`, `svg_path`, `shortcut`, `is_separator`, `is_toggle`, `checked`, `disabled`, `on_click`, `on_toggle` (`::separator()`)
- `CommandBar`: `content`, `primary_commands`, `secondary_commands`, `default_label_position` (`Right/Bottom/Collapsed`), `width`
- `MenuFlyoutItem`: `label`, `shortcut`, `svg_path`, `is_separator`, `is_checkable`, `checked`, `disabled`, `on_click`, `is_radio`, `keep_open_on_click`, `on_toggle`, `sub_items`, `trailing_widget`, `custom_widget`, `custom_height` (`::separator()`)
- `InfoBar`: `title`, `message`, `severity` (`Informational/Success/Warning/Error`), `is_open`, `is_closable`, `action_text`, `on_action`, `on_close`
- `InfoBadge`: `value`, `severity` (`Attention/Informational/Success/Caution/Critical`), `svg_path`, `max_value`
- `ProgressBar`: `value`, `indeterminate`, `state`, `track_height`, `width`, `corner_radius`, `disabled`
- `ProgressRing`: `value`, `indeterminate`, `size`, `stroke_width`, `show_track`, `disabled`
- `Toast::show(ToastDesc{ .title = ..., .message = ..., .severity = ToastSeverity::Success })`
- `Tooltip`: `text`, `delay_sec`, `child`

### 5.8 Media (PNG / JPG / BMP / GIF) & Drag-and-Drop

- `Image`: `source`, `src` (`.png/.jpg/.jpeg/.gif/.bmp`), `bmp_path`, `max_dim`, `auto_play`, `width`, `height`, `stretch` (`ImageStretch::None/Fill/Uniform/UniformToFill`), `corner_radius`, `tint`, `opacity`, `border_color`, `border_width`
- `Svg`: `src`, `xml`, `path`, `viewbox_size`, `stroke_width`, `fill`, `width`, `height`, `tint`, `rotation`
- `OS File Drop`: `win->on_files_dropped([&](const std::vector<std::string>& files, ktui::PointF pos) { ... });`
- `In-Window Drag`: On `RenderBox`: `is_draggable = true`, `on_drag_start` (`ev.accepted = true; ev.payload.mime_type = ...;`), `on_drag_end`; on drop target: `is_drop_target = true`, `on_drag_enter`, `on_drag_over` (`ev.accepted = true`), `on_drag_leave`, `on_drop`.
