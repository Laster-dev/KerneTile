#pragma once

// ==============================================================================
// KTUI - 现代化极简 C++20 声明式桌面 UI 框架 (单头文件全功能导出)
// 特性：
// 1. 纯 C++20 指定初始化器声明式语法 (彻底废弃 .children 链式调用)
// 2. 桌面级高信息密度与硬朗微圆角美学 (2~4px 紧凑圆角，4~8px 紧凑间距，杜绝移动端大圆角)
// 3. 响应式双向数据绑定 (Property / Computed / ObservableList)
// 4. 解析物理弹簧动画引擎 (二阶常微分闭式解，C1 连续极速打断)
// 5. 跨平台抽象层与 Headless 自动化测试支持
// 6. 双通道 Flex 弹性布局引擎与侵入式四指针双向链表场景图 (Relayout Boundary 隔离)
// 7. 双级脏区自适应合成管线，无缝对接 KerneTile 16x16 瓦片分箱与 GPGPU 2D 计算着色器
// ==============================================================================

// 跨平台抽象与类型
#include <ktui/pal/platform_types.hpp>
#include <ktui/pal/platform_backend.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/pal/font_resolver.hpp>

// 响应式数据绑定与状态管理
#include <ktui/binding/property.hpp>
#include <ktui/binding/computed.hpp>
#include <ktui/binding/observable_list.hpp>
#include <ktui/binding/binding.hpp>

// 物理弹簧动画系统
#include <ktui/animation/spring_curve.hpp>
#include <ktui/animation/spring_presets.hpp>
#include <ktui/animation/animation.hpp>
#include <ktui/animation/animation_controller.hpp>

// 设计令牌与主题级联系统
#include <ktui/theme/color_palette.hpp>
#include <ktui/theme/design_tokens.hpp>
#include <ktui/theme/theme.hpp>
#include <ktui/theme/theme_scope.hpp>

// 全局应用宿主与窗口调度
#include <ktui/app/dispatcher.hpp>
#include <ktui/app/application.hpp>
#include <ktui/app/window.hpp>

// 布局与场景树核心
#include <ktui/layout/box_constraints.hpp>
#include <ktui/layout/flex_types.hpp>
#include <ktui/layout/flex_engine.hpp>
#include <ktui/tree/render_node.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/tree/render_flex.hpp>
#include <ktui/tree/overlay.hpp>

// 事件与焦点管线
#include <ktui/event/event_types.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/event/hit_test_tree.hpp>
#include <ktui/event/event_dispatcher.hpp>
#include <ktui/event/focus_tree.hpp>

// 渲染合成与绘制上下文
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/ui_compositor.hpp>

// C++20 指定初始化器声明式桌面控件库
#include <ktui/widgets/widget.hpp>
#include <ktui/widgets/box.hpp>
#include <ktui/widgets/vstack.hpp>
#include <ktui/widgets/hstack.hpp>
#include <ktui/widgets/zstack.hpp>
#include <ktui/widgets/text.hpp>
#include <ktui/widgets/button.hpp>
#include <ktui/widgets/checkbox.hpp>
#include <ktui/widgets/toggle_switch.hpp>
#include <ktui/widgets/radio.hpp>
#include <ktui/widgets/textbox.hpp>
#include <ktui/widgets/text_input.hpp>
#include <ktui/widgets/slider.hpp>
#include <ktui/widgets/progress_bar.hpp>
#include <ktui/widgets/progress_ring.hpp>
#include <ktui/widgets/scroll_view.hpp>
#include <ktui/widgets/combobox.hpp>
#include <ktui/widgets/svg.hpp>
#include <ktui/widgets/info_bar.hpp>
#include <ktui/widgets/content_dialog.hpp>
#include <ktui/widgets/menu_flyout.hpp>
#include <ktui/widgets/dropdown_button.hpp>
#include <ktui/widgets/navigation_view.hpp>
