---
name: ktui
description: Guide for building native C++20 desktop, Web, and cross-platform GUIs with KerneTile & KTUI (via C++20 module `import kernetile;` or single-header/source `kernetile.hpp` + `kernetile.cpp`). Use whenever creating windows, layouts, Fluent 2 / WinUI 3 controls, reactive bindings, images/GIFs, file dialogs, or drag-and-drop apps with KTUI.
---

# KerneTile & KTUI Development Skill

Use this skill to write clean, fast, first-try-compiling C++20 UI code with **KerneTile & KTUI**.

## 1. Hard Rules (Always Follow to Avoid Compile / Runtime Errors)

1. **C++20 Designated Initializer Member Order**:
   - MSVC and Clang strictly enforce that designated initializers (`.field = value`) appear in the **exact declaration order** of the struct. Never reorder fields arbitrarily.
2. **Zero Exceptions & Zero RTTI Friendly**:
   - Do not use `try` / `catch`, `throw`, or `dynamic_cast`. Check pointers/booleans directly.
3. **UTF-8 Source Encoding on Windows**:
   - Always compile with `/utf-8` on MSVC when source files contain Chinese or Unicode string literals.
4. **Importing the Library**:
   - **C++20 Module (Single File `kernetile.ixx`)**: Put any standard headers (`#include <string>`, etc.) **before** `import kernetile;`:
     ```cpp
     #include <string>
     import kernetile;
     ```
   - **Dual-File (`kernetile.hpp` + `kernetile.cpp`)**:
     ```cpp
     #include "kernetile.hpp"
     ```
   - Windows Pure-GDI low-memory mode (~8.2 MB RAM, zero D3D12/DXGI): compile `kernetile.ixx` or `kernetile.cpp` with `-DKERNETILE_PURE_GDI=1`.
5. **Safe Tree Mutation During Events**:
   - Never destroy the `RenderBox` that is currently dispatching a pointer/drag event inside its own `on_drop` / `on_click` callback if subsequent lifecycle hooks (like `on_drag_end`) will still run on it. Prefer updating `ktui::Property<T>` bindings or deferring child rebuilds to `on_drag_end`.

---

## 2. Minimal Application Skeleton

```cpp
#include <string>
import kernetile; // or #include "kernetile.hpp"

int main() {
    ktui::Application app;
    ktui::WindowDesc desc;
    desc.title = "My KTUI App";
    desc.size = {960.0f, 640.0f};
    desc.min_size = {480.0f, 320.0f};

    auto win = app.create_window(desc);
    if (!win) return 1;

    ktui::Property<std::string> status{"Ready"};
    win->set_content(ktui::Box {
        .padding = ktui::EdgeInsets(20.0f),
        .child = ktui::VStack {
            .align_items = ktui::AlignItems::Stretch,
            .spacing = 12.0f,
            .content = {
                ktui::Text { .text = "Dashboard", .font_size = 22.0f, .font_weight = ktui::FontWeight::SemiBold },
                ktui::Text { .font_size = 13.0f }.bind(&ktui::RenderText::set_text, status),
                ktui::Button {
                    .text = "Run Action",
                    .variant = ktui::ButtonVariant::Accent,
                    .on_click = [&]() { status.set("Completed!"); }
                }
            }
        }
    });

    win->show();
    return app.run();
}
```

### Build Commands (MSVC, No CMake Needed)

```powershell
# Option A: C++20 Module (kernetile.ixx)
cl /std:c++20 /O2 /utf-8 /EHsc /interface /c kernetile.ixx
cl /std:c++20 /O2 /utf-8 /EHsc main.cpp kernetile.obj /Fe:app.exe

# Option B: Dual-file (kernetile.hpp + kernetile.cpp)
cl /std:c++20 /O2 /utf-8 /EHsc main.cpp kernetile.cpp /Fe:app.exe
```

---

## 3. Reactive Data Binding (`ktui::Property<T>`)

All widgets inherit `BindableMixin` and support fine-grained one-way and two-way bindings without rebuilding the widget tree:

- **One-way binding**: `.bind(&RenderClass::setter, property)`
  ```cpp
  ktui::Property<std::string> msg{"Hello"};
  ktui::Text { .font_size = 14.0f }.bind(&ktui::RenderText::set_text, msg)
  ```
- **Two-way binding** (for `CheckBox`, `ToggleSwitch`, `Slider`, `TextBox`, `ComboBox`, `SegmentedControl`, `NavigationView`, etc.):
  ```cpp
  ktui::Property<bool> dark_mode{true};
  ktui::ToggleSwitch { .checked = true, .label = "深色模式" }.bind_two_way(dark_mode)
  ```
- **Subscribing to changes**:
  ```cpp
  dark_mode.subscribe([&](bool on) {
      app.theme.set(on ? ktui::Theme::dark() : ktui::Theme::light());
  });
  ```

---

## 4. Layout & Container Cheat Sheet (Exact Field Order)

- **`VStack` / `HStack`**:
  ```cpp
  ktui::HStack {
      .justify = ktui::JustifyContent::SpaceBetween, // FlexStart, Center, FlexEnd, SpaceBetween, SpaceEvenly
      .align_items = ktui::AlignItems::Center,       // FlexStart, Center, FlexEnd, Stretch
      .spacing = 10.0f,
      .content = { /* WidgetPtr... */ }
  }
  ```
- **`Box` (Card / Container)**:
  ```cpp
  ktui::Box {
      .width = 320.0f,           // omit or -1 for auto
      .height = 180.0f,
      .background_color = ktui::Color{0, 0, 0, 0.16f},
      .border_color = ktui::Color{1, 1, 1, 0.10f},
      .border_width = 1.0f,
      .corner_radius = 8.0f,
      .padding = ktui::EdgeInsets(12.0f, 16.0f),
      .theme_card = true,        // Fluent 2 automatic dark/light card surface
      .child = /* WidgetPtr */,
      .alignment = ktui::Alignment::center()
  }
  ```
- **`Expanded` / `Spacer` / `ScrollView` / `Wrap` / `Grid` / `ZStack` / `SplitView` / `DockPanel`**:
  - `ktui::Expanded { .child = ... }` fills remaining main-axis space inside `HStack` / `VStack`.
  - `ktui::ScrollView { .child = ... }` provides smooth wheel + drag scrolling.
  - `ktui::ZStack { .alignment = ktui::Alignment::center(), .content = { bg, fg } }` overlays widgets (ideal for wallpaper backgrounds).

---

## 5. Common Controls Cheat Sheet (Exact Field Order)

- **`Text`**:
  `ktui::Text { .text = "Title", .font_size = 16.0f, .color = ktui::Color::white(), .font_weight = ktui::FontWeight::SemiBold }`
- **`Button`**:
  `ktui::Button { .text = "Save", .variant = ktui::ButtonVariant::Accent, .on_click = [&]() {} }`
  (`ButtonVariant`: `Standard`, `Accent`, `Subtle`, `Danger`)
- **`ToggleSwitch`** (`.checked` BEFORE `.label`):
  `ktui::ToggleSwitch { .checked = true, .label = "Enable", .on_change = [&](bool on) {} }`
- **`CheckBox`**:
  `ktui::CheckBox { .label = "Remember me", .checked = false, .on_change = [&](bool on) {} }`
- **`Slider`**:
  `ktui::Slider { .min_value = 0.0f, .max_value = 100.0f, .step = 1.0f, .on_change = [&](float v) {} }.bind_two_way(val)`
- **`SegmentedControl`**:
  `ktui::SegmentedControl { .items = { ktui::SegmentedItem{ .label = "A" }, ktui::SegmentedItem{ .label = "B" } }, .on_selected_index_changed = [&](int idx) {} }.bind_two_way(idx_prop)`
- **`TextBox` / `PasswordBox` / `TextArea` / `NumberBox`**:
  `ktui::TextBox { .placeholder = "Search...", .on_change = [&](const std::string& s) {} }.bind_two_way(text_prop)`
- **`NavigationView`**:
  Use `ktui::NavigationViewItem { .id = "home", .label = "Home", .icon = ktui::NavigationIcon::Home }`, `ktui::NavigationViewItem::header("Controls")`, and `.page_builder = [&](const std::string& id) -> ktui::WidgetPtr { ... }`.
- **`Toast`** (static notification):
  `ktui::Toast::show(ktui::ToastDesc{ .title = "Saved", .message = "File written", .severity = ktui::ToastSeverity::Success });`
  (`ToastSeverity`: `Informational`, `Success`, `Warning`, `Error`)

---

## 6. Images, Wallpapers & Animated GIFs

`ktui::Image` decodes **PNG, JPG/JPEG, BMP, and multi-frame animated GIF** across all platforms via trimmed `stb_image`:

```cpp
// Load directly from disk (supports UTF-8 paths, auto-downscales huge 4K wallpapers)
ktui::Image {
    .src = "assets/wallpaper.jpg",
    .auto_play = true,
    .width = 480.0f,
    .height = 270.0f,
    .stretch = ktui::ImageStretch::UniformToFill, // None, Fill, Uniform, UniformToFill
    .corner_radius = 10.0f
}
```

- Dynamic updates via `ktui::Property<std::shared_ptr<ktui::ImageBuffer>>`:
  ```cpp
  auto img = ktui::ImageBuffer::load_file("anim.gif");
  img_prop.set(img);
  // Bind to Image:
  ktui::Image { .width = 320, .height = 200 }.bind(&ktui::RenderImage::set_source, img_prop)
  ```

---

## 7. Native File Dialogs & Drag-and-Drop

### Native File / Folder Dialogs (`ktui::FileDialog`)
```cpp
ktui::FileDialogOptions opt;
opt.title = "Select Image";
opt.filters = { {"Images", "*.png;*.jpg;*.jpeg;*.gif;*.bmp"}, {"All Files", "*.*"} };
if (win && win->platform_window()) {
    opt.owner_window_handle = win->platform_window()->get_native_handle();
}
ktui::FileDialog::open_file(opt, [&](const ktui::FileDialogResult& res) {
    if (res.accepted && !res.paths.empty()) {
        auto path = res.first_path();
    }
});
// Also: FileDialog::save_file(...), FileDialog::select_folder(...), kind = FileDialogKind::OpenMultipleFiles
```

### OS File Drop (`Window::on_files_dropped`)
```cpp
win->on_files_dropped([&](const std::vector<std::string>& files, ktui::PointF pos) {
    for (const auto& f : files) { /* handle dropped file path */ }
});
```

### In-Window Widget Drag & Drop (`RenderBox`)
- Source node: set `is_draggable = true`, implement `on_drag_start = [&](ktui::DragEvent& ev) { ev.accepted = true; ev.payload.mime_type = "application/x-item"; ev.payload.data = "id"; };` and `on_drag_end`.
- Target node: set `is_drop_target = true`, implement `on_drag_enter`, `on_drag_over` (`ev.accepted = true`), `on_drag_leave`, and `on_drop`.

---

## 8. Theme & Multi-Window

- Toggle dark / light theme live:
  `app.theme.set(is_dark ? ktui::Theme::dark() : ktui::Theme::light());`
- Spawn an independent secondary window:
  Call `auto child = app.create_window(child_desc); child->set_content(...); child->show();` and retain any reactive `Property<T>` objects while the window is open.
- Custom titlebar actions:
  `win->titlebar().add_action("+", "New Window", [&]() { ... });`
