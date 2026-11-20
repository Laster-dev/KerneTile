#pragma once

#include "spring_curve.hpp"

namespace ktui::Springs {

// 默认自然弹簧 (轻微欠阻尼，轻快有质感)
inline const SpringConfig Default{ 180.0f, 24.0f, 1.0f };

// 快速归位 (临界阻尼 ζ=1.0，最快收敛，绝对零超调，适用于按钮轻拍、菜单弹出)
inline const SpringConfig Snappy = SpringConfig::with_damping_ratio(300.0f, 1.0f);

// 丝滑优雅 (临界阻尼 ζ=1.0，柔和无冲头，适用于页面转场、对话框淡入)
inline const SpringConfig Smooth = SpringConfig::with_damping_ratio(120.0f, 1.0f);

// 活力弹性 (欠阻尼 ζ=0.5，显著弹跳质感，适用于下拉刷新、徽标通知撞击)
inline const SpringConfig Bouncy = SpringConfig::with_damping_ratio(180.0f, 0.5f);

// 跟手交互 (临界阻尼 ζ=1.0，高刚度，零滞后跟随，适用于手势拖拽、滑块拖动)
inline const SpringConfig Interactive = SpringConfig::with_damping_ratio(350.0f, 1.0f);

// 柔和慢速 (临界阻尼 ζ=1.0，低刚度，适用于抽屉展开、大面板折叠)
inline const SpringConfig Gentle = SpringConfig::with_damping_ratio(80.0f, 1.0f);

} // namespace ktui::Springs
