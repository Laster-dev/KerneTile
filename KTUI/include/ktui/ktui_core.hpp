#pragma once

// ==============================================================================
// KTUI 轻量核心聚合头 (ktui_core.hpp)
// 仅导出 App/Window、布局、主题、数据绑定、弹簧动效与最常用基础控件，
// 避免全量引入 DockPanel/Terminal/HexViewer 等重型专业控件，显著缩短业务编译时间。
// 需要完整控件集时可引入 <ktui/ktui.hpp> 或按需单独 #include 对应控件头文件。
// ==============================================================================

#include <ktui/core/strings.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/pal/platform_backend.hpp>
#include <ktui/pal/clipboard.hpp>

#include <ktui/binding/property.hpp>
#include <ktui/binding/computed.hpp>
#include <ktui/binding/observable_list.hpp>
#include <ktui/binding/binding.hpp>

#include <ktui/animation/spring_curve.hpp>
#include <ktui/animation/spring_presets.hpp>
#include <ktui/animation/animation.hpp>
#include <ktui/animation/frame_clock.hpp>
#include <ktui/animation/spring_caret.hpp>
#include <ktui/animation/tactile_press.hpp>

#include <ktui/theme/color_palette.hpp>
#include <ktui/theme/design_tokens.hpp>
#include <ktui/theme/theme.hpp>
#include <ktui/theme/theme_scope.hpp>

#include <ktui/app/application.hpp>
#include <ktui/app/window.hpp>

#include <ktui/widgets/widget.hpp>
#include <ktui/widgets/box/box_logic.hpp>
#include <ktui/widgets/hstack/hstack_logic.hpp>
#include <ktui/widgets/vstack/vstack_logic.hpp>
#include <ktui/widgets/zstack/zstack_logic.hpp>
#include <ktui/widgets/wrap/wrap_logic.hpp>
#include <ktui/widgets/layout_primitives/layout_primitives_logic.hpp>
#include <ktui/widgets/text/text_logic.hpp>
#include <ktui/widgets/button/button_logic.hpp>
#include <ktui/widgets/textbox/textbox_logic.hpp>
#include <ktui/widgets/checkbox/checkbox_logic.hpp>
#include <ktui/widgets/toggle_switch/toggle_switch_logic.hpp>
#include <ktui/widgets/slider/slider_logic.hpp>
#include <ktui/widgets/progress_bar/progress_bar_logic.hpp>
#include <ktui/widgets/scroll_view/scroll_view_logic.hpp>
#include <ktui/widgets/settings_card/settings_card_logic.hpp>
#include <ktui/widgets/toast/toast_logic.hpp>
#include <ktui/widgets/content_dialog/content_dialog_logic.hpp>
