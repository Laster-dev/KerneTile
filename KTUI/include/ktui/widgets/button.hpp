#pragma once

// 桌面级现代按钮控件体系 (Button Component)
// 遵循三大架构原则：
// 1. 逻辑、样式、数据三层物理分离三个文件 (button_data.hpp, button_style.hpp, button_logic.hpp)
// 2. 一个控件一个专属独立目录 (ktui/widgets/button/)
// 3. 风格完全参照 D:\CUI\CUI.Core\ui (Fluent 规范、水波纹点击反馈、多变体与紧凑尺寸)

#include "button/button_data.hpp"
#include "button/button_style.hpp"
#include "button/button_logic.hpp"
