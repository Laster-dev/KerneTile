# KTUI 阶段 7：C++20 指定初始化器控件库与端到端综合测试

## 1. 阶段目标与核心交付物

本阶段的目标是**完成全套标准开箱即用控件库的落地**，彻底践行 **C++20 指定初始化器（Designated Initializers）** 语法与 **控件三层分离规范（Model + Behavior + Painter）**，并交付一个涵盖数据绑定、物理弹簧动画、暗黑主题切换、文本编辑与平滑滚动的端到端综合应用。

### 核心交付物：
1. **聚合容器组件（C++20 指定初始化器）**：
   - [include/ktui/widgets/vstack.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/vstack.hpp)：垂直排列容器 `VStack`；
   - [include/ktui/widgets/hstack.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/hstack.hpp)：水平排列容器 `HStack`；
   - [include/ktui/widgets/zstack.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/zstack.hpp)：层叠排列容器 `ZStack`；
   - [include/ktui/widgets/box.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/box.hpp)：万能修饰盒 `Box`（背景、圆角、边框、阴影、毛玻璃）；
2. **三层分离交互控件**：
   - [include/ktui/widgets/text.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/text.hpp)：文本组件（支持单向绑定与类型转换器）；
   - [include/ktui/widgets/button.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/button.hpp)：交互按钮（状态机动效与可用性绑定）；
   - [include/ktui/widgets/text_input.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/text_input.hpp)：文本编辑框（双向绑定与 IME 状态机联动）；
   - [include/ktui/widgets/checkbox.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/checkbox.hpp)：复选框（布尔双向绑定）；
   - [include/ktui/widgets/slider.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/slider.hpp)：滑块（浮点双向绑定与拖拽阻尼）；
   - [include/ktui/widgets/scroll_view.hpp](file:///d:/KerneTile/KTUI/include/ktui/widgets/scroll_view.hpp)：平滑滚动容器（硬件级联裁剪与自适应滚动条）；
3. **顶层单包含总头文件**：
   - [include/ktui/ktui.hpp](file:///d:/KerneTile/KTUI/include/ktui/ktui.hpp)：开发者单头文件引入全套声明式能力；
4. **端到端综合测试与演示**：
   - `tests/test_ktui_e2e_app.cpp`：端到端综合测试套件。

---

## 2. 控件三层分离规范在指定初始化器中的体现

以 `Button` 为例：
1. **数据层 (Data)**：包含标签文本 `std::string` 或绑定的属性；
2. **逻辑层 (`ButtonBehavior`)**：
   - 维护 `is_hovered`, `is_pressed`, `is_focused`, `is_disabled` 状态；
   - 处理按键与指针点击事件；
3. **视觉层 (`ButtonPainter`)**：
   - 提取当前状态并调用物理弹簧动画计算色彩与阴影插值；
   - 发射底层的 80B `UiPrimitive`。

开发者调用的语法形态极度简洁优雅：
```cpp
Button {
    .label = "添加任务",
    .style = ButtonStyle::Primary,
    .enabled = bind(vm.can_add),
    .on_click = [&] { vm.add_task(); }
}
```

---

## 3. 具体实现步骤与细节要求

### 步骤 7.1：实现容器组件的聚合初始化定义
```cpp
struct VStack {
    float spacing = 0.0f;
    EdgeInsets padding = EdgeInsets::zero();
    AlignItems align_items = AlignItems::Stretch;
    JustifyContent justify_content = JustifyContent::Start;
    Color background = Colors::Transparent;
    std::vector<WidgetPtr> content;

    operator WidgetPtr() const; // 隐式转换为统一 WidgetPtr，支持无缝嵌套
};
```

### 步骤 7.2：实现交互控件
- **`TextInput`**：
  - 构造中接收 `Binding<std::string>`；
  - 内部 `TextEditBehavior` 维护光标位置 `m_cursor_index`；
  - 响应阶段 8 的 IME 拼音预编辑与汉字上屏；
- **`ScrollView`**：
  - 内部持有 `ScrollBehavior`，响应鼠标滚轮 `delta_y` 与触控惯性；
  - 在 `paint` 中向 KerneTile 推入 `ClipRect`，内容超出部分 100% 硬件丢弃。

### 步骤 7.3：编写顶层单头文件 `ktui.hpp`
- 聚合包含 `pal`, `binding`, `animation`, `app`, `theme`, `tree`, `layout`, `event`, `widgets`；
- 导出统一命名空间 `namespace ktui`。

---

## 4. 验证计划与测试用例

在 `tests/test_ktui_e2e_app.cpp` 中编写端到端综合测试：
1. **端到端应用界面搭建**：
   - 编写包含标题、双向绑定输入框、添加按钮、任务列表与暗黑主题切换的完整应用；
2. **用户交互全流程模拟**：
   - 模拟在 `TextInput` 中打字 `"买牛奶"`，验证 ViewModel 的 `input_text` 实时变为 `"买牛奶"`；
   - 验证添加按钮从禁用变为可用；
   - 模拟点击添加按钮，验证列表新增项，列表高度动态展开，触发物理弹簧位移动效；
3. **像素渲染与无内存泄漏校验**：
   - 调用 `KerneTileRenderer` 输出离屏图像，比对文字、圆角与阴影像素全部正确呈现；
   - 退出应用，验证所有保留式节点与显存资源安全析构，**0 内存泄漏，0 显存泄漏**。
