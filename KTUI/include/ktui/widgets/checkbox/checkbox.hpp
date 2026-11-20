#pragma once

// ============================================================================
// KerneTile UI Framework - Checkbox (三层解耦与 CUI 规范体系)
// 1. 数据层 (Data):   checkbox_data.hpp   - CheckState 枚举与 CheckboxData 契约
// 2. 样式层 (Style):  checkbox_style.hpp  - CheckboxStyleResolver 样式解析器
// 3. 逻辑层 (Logic):  checkbox_logic.hpp  - RenderCheckbox、MotionTween 动效与 DSL 包装
// ============================================================================

#include "checkbox_data.hpp"
#include "checkbox_style.hpp"
#include "checkbox_logic.hpp"
