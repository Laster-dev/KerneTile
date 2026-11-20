# KTUI (KerneTile UI Framework)

**KTUI** 是面向现代硬件的高性能跨平台现代 C++20 声明式 UI 框架，构建于 **KerneTile 2D 计算着色器渲染底座** 之上。

---

## 核心特性

- **C++20 指定初始化器声明式语法 (Designated Initializers)**：
  告别冗余样板代码，参数具名自解释，天然类似 QML / Flutter，无任何宏或外部预处理器依赖。
- **纯粹跨平台抽象层 (PAL: Platform Abstraction Layer)**：
  解耦单一系统。支持 Windows (Win32/D3D12/IMM32)、Linux (Wayland/Vulkan/FreeType)、macOS (Cocoa/Metal/CoreText) 与 **Headless（纯内存虚拟无头测试驱动，全平台 100% CI 自动化）**。
- **工业级数据绑定体系 (Data Binding Engine)**：
  统一 `bind(...)` 语法：输入框/复选框双向绑定自动回写并内置 **Transaction Token 防死锁环路熔断机制**；只读展示单向投影；Lambda 转换器绑定。
- **SOTA 物理弹簧动画系统 (Spring-first & Velocity Preservation)**：
  二阶常微分阻尼谐振子解析闭式解，打断时**瞬时速度 100% 保真（$C^1$ 连续）**；外观动效直接绕过 Flex 布局计算，仅重绘 16×16 局部瓦片，静止时 CPU 占用归零。
- **控件三层彻底分离 (The Control Triad)**：
  数据层（Model）+ 逻辑层（Behavior，无头可测）+ 视觉层（Painter，支持一键无损换肤）。
- **双级脏区确认与 KerneTile 16×16 瓦片网格融合**：
  布局脏（NeedsLayout 沿父链冒泡至 Relayout Boundary 阻断）与绘制脏（NeedsRepaint 仅局部标记）；结合历史包围盒联合 $\text{Old} \cup \text{New}$ 与阴影外扩，吸附至 16×16 网格驱动 GPU 计算。

---

## 声明式语法范例

```cpp
#include <ktui/ktui.hpp>

using namespace ktui;

// 1. 数据模型 (ViewModel)
struct TodoViewModel : public ViewModel {
    Property<std::string> input_text{""};
    Property<bool> can_add{false};
    ObservableList<TaskItem> tasks;

    void add_task() {
        if (!input_text.get().empty()) {
            tasks.push_back({input_text.get(), false});
            input_text.set("");
        }
    }
};

// 2. 声明式 UI 描述 (C++20 指定初始化器)
WidgetPtr build_todo_view(TodoViewModel& vm) {
    return VStack {
        .spacing = 16.0f,
        .padding = EdgeInsets{20.0f},
        .background = Colors::Slate900,
        .content = {
            // 标题
            Text {
                .text = "任务控制面板",
                .font_size = Typography::Heading1,
                .color = Colors::White,
                .bold = true
            },

            // 水平操作栏
            HStack {
                .spacing = 8.0f,
                .align_items = Align::Center,
                .content = {
                    TextInput {
                        .text = bind(vm.input_text),       // 双向数据绑定
                        .placeholder = "输入新任务...",
                        .corner_radius = 6.0f,
                        .flex_grow = 1.0f
                    },
                    Button {
                        .label = "添加",
                        .style = ButtonStyle::Primary,
                        .enabled = bind(vm.can_add),       // 状态绑定
                        .on_click = [&] { vm.add_task(); }
                    }
                }
            },

            // 滚动列表区 (硬件裁剪与惯性滚动)
            ScrollView {
                .flex_grow = 1.0f,
                .content = ForEach(vm.tasks, [](const TaskItem& item) {
                    return HStack {
                        .spacing = 12.0f,
                        .padding = EdgeInsets{8.0f},
                        .align_items = Align::Center,
                        .content = {
                            Checkbox { .checked = bind(item.is_completed) }, // 双向布尔绑定
                            Text {
                                .text = bind(item.title),
                                .strike_through = bind(item.is_completed),   // 划线状态绑定
                                .flex_grow = 1.0f
                            },
                            Button {
                                .label = "删除",
                                .style = ButtonStyle::Danger,
                                .on_click = [&] { item.remove(); }
                            }
                        }
                    };
                })
            }
        }
    };
}
```

---

## 详细实施文档与路线图

| 序号 | 阶段文档 | 核心交付内容 |
| :--- | :--- | :--- |
| **全景** | [00_ktui_architecture_overview.md](file:///d:/KerneTile/KTUI/docs/00_ktui_architecture_overview.md) | 全景架构体系、三大支柱与分层拓扑 |
| **专题 1** | [architecture_windowing_and_overlays.md](file:///d:/KerneTile/KTUI/docs/architecture_windowing_and_overlays.md) | 多窗口 (Multi-Window)、悬浮窗、右键菜单与失焦关闭 (Light Dismiss) |
| **专题 2** | [architecture_window_control_and_svg.md](file:///d:/KerneTile/KTUI/docs/architecture_window_control_and_svg.md) | SVG 矢量支持、Window 全功能接口大全与 Control 控件基类规范 |
| **阶段 1** | [phase_01_pal_and_headless.md](file:///d:/KerneTile/KTUI/docs/phase_01_pal_and_headless.md) | 跨平台 PAL、系统字体回退与 Headless 自动化测试后端 |
| **阶段 2** | [phase_02_reactive_binding_engine.md](file:///d:/KerneTile/KTUI/docs/phase_02_reactive_binding_engine.md) | 响应式核心、`bind(...)` 双向/单向/转换绑定与防环路熔断机制 |
| **阶段 3** | [phase_03_spring_physics_animation.md](file:///d:/KerneTile/KTUI/docs/phase_03_spring_physics_animation.md) | SOTA 解析物理弹簧动画引擎、速度保真打断与静止 0% CPU 沉睡 |
| **阶段 4** | [phase_04_application_and_theme_tokens.md](file:///d:/KerneTile/KTUI/docs/phase_04_application_and_theme_tokens.md) | 全局 Application 宿主、多线程 Dispatcher、设计令牌与 ThemeScope 局部级联 |
| **阶段 5** | [phase_05_render_tree_and_flex_layout.md](file:///d:/KerneTile/KTUI/docs/phase_05_render_tree_and_flex_layout.md) | 保留式 RenderTree 场景树、BoxConstraints 与纯 C++20 双通道 Flex 布局引擎 |
| **阶段 6** | [phase_06_dirty_pipeline_and_kerne_tile_integration.md](file:///d:/KerneTile/KTUI/docs/phase_06_dirty_pipeline_and_kerne_tile_integration.md) | 双级脏区计算、Capture/Bubble 路由、IME 状态机与 KerneTile 16×16 瓦片渲染闭环 |
| **阶段 7** | [phase_07_designated_initializer_widgets_and_e2e.md](file:///d:/KerneTile/KTUI/docs/phase_07_designated_initializer_widgets_and_e2e.md) | C++20 指定初始化器控件库与端到端综合应用压测 |
