#pragma once

#include "spring_curve.hpp"

namespace ktui::Springs {

// 默认自然弹簧 (轻微欠阻尼，轻快有质感)
inline constexpr SpringConfig Default{ 180.0f, 24.0f, 1.0f };

// 快速归位 (临界阻尼 ζ=1.0，c = 2*sqrt(300)，最快收敛，绝对零超调，适用于按钮轻拍、菜单弹出)
inline constexpr SpringConfig Snappy{ 300.0f, 34.641016f, 1.0f };

// 丝滑优雅 (临界阻尼 ζ=1.0，c = 2*sqrt(120)，柔和无冲头，适用于页面转场、对话框淡入)
inline constexpr SpringConfig Smooth{ 120.0f, 21.908902f, 1.0f };

// 活力弹性 (欠阻尼 ζ=0.5，c = sqrt(180)，显著弹跳质感，适用于下拉刷新、徽标通知撞击)
inline constexpr SpringConfig Bouncy{ 180.0f, 13.416408f, 1.0f };

// 跟手交互 (临界阻尼 ζ=1.0，c = 2*sqrt(350)，高刚度，零滞后跟随，适用于手势拖拽、滑块拖动)
inline constexpr SpringConfig Interactive{ 350.0f, 37.416574f, 1.0f };

// 柔和慢速 (临界阻尼 ζ=1.0，c = 2*sqrt(80)，低刚度，适用于抽屉展开、大面板折叠)
inline constexpr SpringConfig Gentle{ 80.0f, 17.888544f, 1.0f };

} // namespace ktui::Springs
