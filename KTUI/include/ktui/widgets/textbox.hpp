#pragma once

// 桌面级现代单行文本输入框控件体系 (TextBox Component)
// 遵循三大架构原则：
// 1. 逻辑、样式、数据三层物理分离三个文件 (textbox_data.hpp, textbox_style.hpp, textbox_logic.hpp)
// 2. 一个控件一个专属独立目录 (ktui/widgets/textbox/)
// 3. 风格完全参照 D:\CUI\CUI.Core\ui (Fluent 规范、底部动态高亮聚焦线、密码模式、选区与 IME 支持)

#include "textbox/textbox_data.hpp"
#include "textbox/textbox_style.hpp"
#include "textbox/textbox_logic.hpp"
