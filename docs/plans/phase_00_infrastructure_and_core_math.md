# 阶段 0: 工程基础设施、核心数学与紧致内存布局

## 1. 阶段目标与定位

本阶段是 KerneTile 引擎的基石，主要目标是建立现代化的工程脚手架、严密的数学运算库，以及定义全管线通用的紧凑数据结构。
由于 KerneTile 采用计算着色器驱动的分块范式，CPU 与 GPU 之间、主线程与渲染线程之间的数据交换极其频繁，因此必须在阶段 0 从二进制与字节对齐层面确立不可动摇的数据契约。

交付物包括：
1. 模块化构建系统配置（支持 C++20 / 编译期优化 / 跨平台交叉编译）。
2. 高性能 2D 几何与颜色数学库（含 IEC 61966-2-1 严格色彩转换与 Porter-Duff 预乘代数）。
3. 严格对齐的核心内存布局：**80 字节 `UiPrimitive`**、**32 字节 `ClipRect`** 与 **8 字节 `TileHeader`**。
4. 单元测试套件：验证色彩空间转换精度、合法预乘域钳制与内存对齐断言。

---

## 2. 深入开发思路

### 2.1 现代工程基建与编译配置
- **现代 C++20 标准与特性**：全面采用 `std::span`、概念约束（Concepts）、模块化头文件、结构化绑定与 constexpr 编译期求值。
- **构建工具选型**：提供标准 `CMakeLists.txt`（兼容主流 IDE 与 CI）与轻量快速的 `xmake.lua` 双配置。
- **严格编译警告与静态分析**：启用 `-Wall -Wextra -Wpedantic`（MSVC 下 `/W4 /permissive-`），开启未定义行为检查（UBSan）与地址消毒（ASan）。
- **SIMD 指令集自适应检测**：在构建配置中注入针对 x86-64（AVX2 / FMA）与 ARM64（NEON）的原生向量化支持，为后续 CPU 回退路径预留编译分支。

### 2.2 色彩空间与预乘 Alpha 理论与数学实现
在 2D 界面合成中，直通 Alpha（Straight Alpha）和 Gamma 空间直接混合是导致“黑边渗溢”、“边缘暗轮廓”及“跨后端显示分歧”的罪魁祸首。KerneTile 全管线强制执行 **IEC 61966-2-1 线性光空间下的预乘 Alpha（Premultiplied Alpha）**。

#### 2.2.1 IEC 61966-2-1 转换函数
非线性 sRGB 颜色通道 $C_{\text{sRGB}} \in [0, 1]$ 转换至线性光强度 $C_{\text{lin}} \in [0, 1]$：

$$C_{\text{lin}} = \begin{cases} \frac{C_{\text{sRGB}}}{12.92}, & C_{\text{sRGB}} \le 0.04045 \\ \left(\frac{C_{\text{sRGB}} + 0.055}{1.055}\right)^{2.4}, & C_{\text{sRGB}} > 0.04045 \end{cases}$$

线性光强度 $C_{\text{lin}}$ 编码回非线性 sRGB 颜色通道 $C_{\text{sRGB}}$：

$$C_{\text{sRGB}} = \begin{cases} 12.92\,C_{\text{lin}}, & C_{\text{lin}} \le 0.0031308 \\ 1.055\,C_{\text{lin}}^{1/2.4} - 0.055, & C_{\text{lin}} > 0.0031308 \end{cases}$$

**数学连续性关键验证**：
在分段阈值 $C_{\text{lin}} = 0.0031308$ 处：
- 线性段：$12.92 \times 0.0031308 = 0.040449936$
- 幂函数段：$1.055 \times (0.0031308)^{1/2.4} - 0.055 \approx 0.040449907$
- 两者连续性绝对偏差小于 $2.85\times 10^{-8}$，保证低亮度阴影区与高光区平滑衔接无断层。

#### 2.2.2 预乘 Alpha 四通道代数与边界防范
设源像素为预乘四元组 $(C_a, \alpha_a)$，目标像素为 $(C_b, \alpha_b)$（其中颜色通道已经过 Alpha 预乘，即 $C_a = c_a \cdot \alpha_a$），经典的 Porter-Duff Source-Over 运算为：

$$C_o = C_a + C_b(1 - \alpha_a)$$
$$\alpha_o = \alpha_a + \alpha_b(1 - \alpha_a)$$

**边界防御设计（防色域越界分歧）**：
Skia 官方曾记录在 `kMultiply` 图层下由于输入非法预乘色（$C_a > \alpha_a$），导致 GPU 输出纯白而 CPU 输出纯黑的严重缺陷。
因此，KerneTile 在数学库和图元规范化入口处设置合法性校验断言与自动钳制：
```cpp
inline ColorPremul clamp_premul(float r, float g, float b, float a) noexcept {
    a = std::clamp(a, 0.0f, 1.0f);
    r = std::clamp(r, 0.0f, a);
    g = std::clamp(g, 0.0f, a);
    b = std::clamp(b, 0.0f, a);
    return ColorPremul{r, g, b, a};
}
```

---

## 3. 紧凑核心数据结构与内存布局规范

为了确保 Host-to-Device 传输带宽降至最低，且在 GPU 端与 LDS 读写保持最佳结构体对齐（16 字节对齐），内存结构必须严密设计并保证跨平台统一。

```
UiPrimitive (80 字节内存紧致布局):
+---------------------------------------------------------------------------------+
| Offset 00-15 (16B): Rect Bounds (min_x, min_y, max_x, max_y)                   |
+---------------------------------------------------------------------------------+
| Offset 16-31 (16B): Corner Radii (top_left, top_right, bot_right, bot_left)    |
+---------------------------------------------------------------------------------+
| Offset 32-47 (16B): Linear Premultiplied Color / Gradient Stop 0 (R, G, B, A)   |
+---------------------------------------------------------------------------------+
| Offset 48-63 (16B): Border & Shadow (border_width, shadow_blur, spread, pad)   |
+---------------------------------------------------------------------------------+
| Offset 64-79 (16B): Atlas UV Bounds (u0, v0, u1, v1) / Flags & ClipIndex        |
+---------------------------------------------------------------------------------+
```

### 3.1 核心 C++20 结构体定义

```cpp
#pragma once
#include <cstdint>
#include <type_traits>

namespace kernetile::core {

#pragma pack(push, 1)

// ============================================================================
// 80 字节 UiPrimitive 结构体定义 (与 GPU 着色器端严格二进制镜像)
// ============================================================================
struct alignas(16) UiPrimitive {
    // 00-15: 几何包围盒 (物理像素坐标)
    float min_x;
    float min_y;
    float max_x;
    float max_y;

    // 16-31: 四角独立圆角半径 (顺时针: 左上、右上、右下、左下)
    float radius_top_left;
    float radius_top_right;
    float radius_bottom_right;
    float radius_bottom_left;

    // 32-47: IEC 61966-2-1 线性光空间预乘颜色 (RGBA 各 32-bit float)
    float color_r;
    float color_g;
    float color_b;
    float color_a;

    // 48-63: SDF 描边与解析阴影控制参数
    float border_width;       // 等宽边框宽度 (<= 0 为实心填充)
    float shadow_blur;        // 高斯阴影模糊半径 sigma
    float shadow_spread;      // 阴影外扩距离
    float shadow_offset_x;    // 阴影水平偏移 (或备用属性)

    // 64-79: 动态图集 UV 映射与管线元标志
    uint16_t atlas_u0;        // 归一化 UV 坐标定点化映射 (0-65535 对应 0.0-1.0)
    uint16_t atlas_v0;
    uint16_t atlas_u1;
    uint16_t atlas_v1;
    uint16_t flags;           // 标志位: 纯色/渐变/图集字形/阴影/滤镜
    uint16_t clip_index;      // 关联的 ClipRect 索引
    float shadow_offset_y;    // 阴影垂直偏移
};

// ============================================================================
// 32 字节 ClipRect 结构体定义 (裁剪树节点)
// ============================================================================
struct alignas(16) ClipRect {
    // 00-15: 裁剪绝对包围盒
    float min_x;
    float min_y;
    float max_x;
    float max_y;

    // 16-31: 圆角与层级元数据
    float corner_radius;      // 裁剪圆角半径
    uint32_t parent_index;    // 父级裁剪矩形索引 (0xFFFFFFFF 表示顶层无父级)
    uint32_t flags;           // 标志位 (如是否圆角裁剪、是否反向遮罩)
    uint32_t reserved;        // 预留对齐填充
};

// ============================================================================
// 8 字节 TileHeader 结构体定义 (每个 16x16 瓦片的元数据头)
// ============================================================================
struct alignas(4) TileHeader {
    uint32_t primitive_offset; // 指向瓦片全局图元索引缓冲的起始偏移
    uint16_t primitive_count;  // 覆盖当前瓦片的图元总数
    uint16_t flags;            // 瓦片状态标志 (如是否完全不透明/是否脏块)
};

#pragma pack(pop)

// 编译期严格断言尺寸与对齐
static_assert(sizeof(UiPrimitive) == 80, "UiPrimitive must be exactly 80 bytes");
static_assert(sizeof(ClipRect) == 32, "ClipRect must be exactly 32 bytes");
static_assert(sizeof(TileHeader) == 8, "TileHeader must be exactly 8 bytes");
static_assert(alignof(UiPrimitive) == 16, "UiPrimitive alignment must be 16 bytes");
static_assert(alignof(ClipRect) == 16, "ClipRect alignment must be 16 bytes");

} // namespace kernetile::core
```

---

## 4. 算法与数学接口实现规范

### 4.1 颜色转换模块实现细节 (`color_space.hpp`)
```cpp
namespace kernetile::math {

constexpr float SRGB_THRESHOLD_LIN = 0.0031308f;
constexpr float SRGB_THRESHOLD_ENC = 0.04045f;

// 非线性 sRGB -> 线性光空间 (单通道)
[[nodiscard]] constexpr float srgb_to_linear(float c) noexcept {
    if (c <= SRGB_THRESHOLD_ENC) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// 线性光空间 -> 非线性 sRGB (单通道)
[[nodiscard]] constexpr float linear_to_srgb(float c) noexcept {
    if (c <= SRGB_THRESHOLD_LIN) {
        return c * 12.92f;
    }
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// 四通道直通颜色转化为线性预乘四元组
[[nodiscard]] inline core::UiPrimitive pack_premul_color(float r, float g, float b, float a) noexcept {
    float lin_r = srgb_to_linear(r) * a;
    float lin_g = srgb_to_linear(g) * a;
    float lin_b = srgb_to_linear(b) * a;
    return core::UiPrimitive{
        .color_r = lin_r,
        .color_g = lin_g,
        .color_b = lin_b,
        .color_a = a
    };
}

} // namespace kernetile::math
```

---

## 5. 验证与单元测试计划

### 5.1 自动化单元测试项
1. **内存尺寸与对齐测试 (`test_layout.cpp`)**：
   - 验证 `UiPrimitive`、`ClipRect`、`TileHeader` 的 `sizeof` 和 `alignof` 在不同架构平台（x86-64, aarch64）完全一致。
   - 验证结构体成员的 `offsetof`，确保 GPU 着色器结构体可以逐字段无差别反序列化。
2. **IEC 61966-2-1 连续性与往返精度测试 (`test_color.cpp`)**：
   - 覆盖区间 $[0.0, 1.0]$ 内 $100,000$ 个采样点的 `srgb_to_linear` 与 `linear_to_srgb` 往返往复误差。
   - 关键验证：往返最大绝对误差不得超过 $1.0\times 10^{-6}$。
   - 阈值边界连续性断言：在 $C_{\text{lin}} = 0.0031308$ 左右 $1.0\times 10^{-7}$ 步长内，验证一阶导数与求值跃迁连续。
3. **Porter-Duff 结合律与单色覆盖测试 (`test_premul.cpp`)**：
   - 验证预乘格式在连续半透明图层堆叠下的结合律：`Over(Over(A, B), C) == Over(A, Over(B, C))`。
   - 验证 $C_a > \alpha_a$ 异常输入时的安全自动钳制，防止生成 NaN 或浮点溢出。

### 5.2 阶段验收标准
- 所有测试用例在 MSVC、GCC、Clang 编译器下 100% 通过无 Warning。
- 内存结构体头文件在跨语言测试中满足与 C99 及 Rust `#[repr(C, align(16))]` 严丝合缝匹配。
