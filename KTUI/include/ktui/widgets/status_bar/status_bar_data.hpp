#pragma once

#include "../widget.hpp"
#include "../menu_flyout/menu_flyout_logic.hpp"
#include <ktui/binding/binding.hpp>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 分段式状态栏槽位描述 (Slot)
struct StatusBarSlot {
    std::string id;
    std::string text;
    bool align_right{false};
    Color indicator_color{0.0f, 0.0f, 0.0f, 0.0f}; // a>0 时左侧绘制 6px 状态灯圆点
    std::vector<std::string> options;              // 非空时点击弹出 MenuFlyout 切换选项
    Fn<void(const std::string&)> on_selected{nullptr};
    Fn<void()> on_click{nullptr};
};

} // namespace ktui
