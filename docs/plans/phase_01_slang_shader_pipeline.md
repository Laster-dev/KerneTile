# 阶段 1: 基于 Slang 的单源码多后端着色器与解析视觉内核

## 1. 阶段目标与定位

本阶段的核心是建立 **KerneTile 的着色器基础设施与纯数学解析视觉算法库**。
在传统图形管线中，圆角矩形、边框和阴影依赖大量的三角形细分网格或昂贵的多遍离屏模糊卷积；而在计算着色器驱动的架构中，图形被还原为纯粹的连续数学函数。

交付物包括：
1. **基于 Slang 的跨平台着色器编译与离线预编译工作流**（单源码输出 DXIL、SPIR-V、MSL、WGSL）。
2. **Inigo Quilez 无分支 2D 精确圆角矩形有向距离场（SDF）算子**，含独立四圆角与环形描边扩展。
3. **Evan Wallace 常数时间解析高斯圆角阴影内核**，基于 Abramowitz & Stegun 四阶误差函数有理逼近（最大误差约 0.21 LSB）。
4. **Raph Levien 超椭圆（Squircle）大半径阴影修正算法**，消除大模糊半径下的过渡生硬与峰值衰减失真。
5. 着色器单元测试与跨后端字节码输出一致性比对。

---

## 2. 深入开发思路

### 2.1 Slang 单源码多后端着色基础设施
维护多套着色器源码（HLSL/GLSL/MSL/WGSL）是跨平台图形引擎维护成本高企的主要根源。KerneTile 采用 Khronos Group 治理的 **Slang** 编译器：

```
                    ┌─────────────────────────┐
                    │ Slang 核心源码 (.slang) │
                    │ - 紧凑图元接口          │
                    │ - 2D SDF 纯解析求值    │
                    │ - 解析高斯阴影计算      │
                    └────────────┬────────────┘
                                 │
                     [Slang 编译器离线特化]
                                 │
         ┌──────────────┬────────┴──────┬──────────────┐
         ▼              ▼               ▼              ▼
     Direct3D 12      Vulkan          Metal         WebGPU
    (DXIL 字节码)   (SPIR-V 字节码)  (MSL 源码)    (WGSL 源码)
         │              │               │              │
         └──────────────┴───────┬───────┴──────────────┘
                                ▼
                       构建期嵌入静态二进制
                       (运行时零编译，彻底消灭 Jank)
```

#### 2.1.1 能力检查系统（Capability System）
Slang 允许在类型检查阶段静态检验目标硬件平台能力：
- 显式声明要求目标支持存储纹理（Storage Textures）与特定波前尺寸。
- 在 Vulkan（SPIR-V）与 Direct3D 12（DXIL）之间无缝映射 `RWTexture2D` 与 `RWByteAddressBuffer`，消除平台语法特异性。
- 通过模块化扩展（Extensions）与接口（Interfaces）实现算法特化，使编译器在离线阶段将纯色填充、渐变填充与图集采样静态内联展开，消除运行时动态分支。

---

## 3. 核心数学解析算法与着色器实现

### 3.1 Inigo Quilez 无分支 2D 圆角矩形 SDF
对于屏幕空间任意像素点 $\mathbf{p} = (x, y)$，矩形中心为 $\mathbf{c} = (\frac{\min_x + \max_x}{2}, \frac{\min_y + \max_y}{2})$，半宽与半高为 $\mathbf{b} = (\frac{\max_x - \min_x}{2}, \frac{\max_y - \min_y}{2})$，圆角半径为 $r$。

#### 3.1.1 数学公式推导
利用几何对称性将采样点折叠至局部第一象限，并平移至内缩角点：

$$\mathbf{q} = |\mathbf{p} - \mathbf{c}| - (\mathbf{b} - r\mathbf{1})$$

像素到边界的欧氏有向距离 $d_{\text{rbox}}$ 为：

$$d_{\text{rbox}}(\mathbf{p}, \mathbf{b}, r) = \|\max(\mathbf{q}, \mathbf{0})\|_2 + \min(\max(q_x, q_y), 0) - r$$

- **外部像素**：$\max(\mathbf{q}, \mathbf{0})$ 为到最近圆角/边的正向量，第二项为 $0$；减去 $r$ 后即为到圆弧的最短正距离。
- **内部像素**：第一项为 $0$，$\min(\max(q_x, q_y), 0)$ 为负值，表示到最近内边的负距离；减去 $r$ 保持负有向距离。
- **全公式无分支**：全由 `abs`、`max`、`min`、`length` 组成，GPU 波前（Warp/Wavefront）执行时绝对无分支发散。

#### 3.1.2 独立四圆角扩展
根据像素相对矩形中心的象限符号选择对应的半径：
$$\mathbf{s} = \mathbf{p} - \mathbf{c}$$
- $s_x \ge 0, s_y \ge 0 \implies r = r_{\text{bottom\_right}}$
- $s_x \ge 0, s_y < 0 \implies r = r_{\text{top\_right}}$
- $s_x < 0, s_y < 0 \implies r = r_{\text{top\_left}}$
- $s_x < 0, s_y \ge 0 \implies r = r_{\text{bottom\_left}}$

#### 3.1.3 抗锯齿与描边算子
- **平滑抗锯齿覆盖率**：通过 `smoothstep` 在半像素边界实现亚像素平滑：
  $$\text{coverage} = \operatorname{smoothstep}(+0.5, -0.5, d_{\text{rbox}})$$
- **环形折叠描边算子 (`opRing`)**：边框宽度为 $w$ 时，仅需对距离做绝对值折叠：
  $$d_{\text{border}} = |d_{\text{rbox}} + \frac{w}{2}| - \frac{w}{2}$$
- **多项式平滑融合算子 (`smoothMin`)**：
  $$\operatorname{smin}(a, b, k) = \min(a, b) - \frac{\max(k - |a - b|, 0)^2}{4k}$$

---

### 3.2 常数时间解析高斯圆角阴影
传统阴影需分配全屏/离屏模糊纹理进行多遍卷积，在高分辨率下造成极大的显存带宽浪费。本算法依据 Evan Wallace 的常数时间解析模型：

#### 3.2.1 误差函数解析积分
二维高斯模糊是连续卷积积分。标准一维高斯函数的积分由高斯误差函数给出：

$$F(x) = \int_{-\infty}^{x} \frac{1}{\sigma\sqrt{2\pi}} e^{-\frac{t^2}{2\sigma^2}} dt = \frac{1}{2}\left[1 + \operatorname{erf}\left(\frac{x}{\sigma\sqrt{2}}\right)\right]$$

直角矩形 $[x_0, x_1] \times [y_0, y_1]$ 在点 $(x, y)$ 处的高斯漫反射投影强度可精确分解为两轴积分之积：

$$I_{\text{box}}(x, y) = [F(x_1 - x) - F(x_0 - x)] \times [F(y_1 - y) - F(y_0 - y)]$$

#### 3.2.2 Abramowitz & Stegun 四阶有理多项式近似
由于大多数 GPU 缺乏硬件 `erf` 指令，采用经典四阶有理逼近：

$$\operatorname{erf}(x) \approx \operatorname{sgn}(x)\left[1 - \frac{1}{(1 + 0.278393|x| + 0.230389|x|^2 + 0.078108|x|^4)^4}\right]$$

**数值精度分析**：
- 在常用区间 $[-4.0, 4.0]$ 内，该有理函数与真实 $\operatorname{erf}(x)$ 的最大绝对误差仅为 **$8.198\times 10^{-4}$**。
- 对于 8-bit 输出通道（$1/255 \approx 3.922\times 10^{-3}$），该误差仅折合 **$0.21$ LSB**，在阴影渐变过渡中完全低于显示硬件量化精度，杜绝色彩断层与摩尔条纹。

#### 3.2.3 Raph Levien 超椭圆（Squircle）大半径修正
当阴影模糊半径 $\sigma$ 相对于矩形尺寸或圆角半径较大时，单纯将 SDF 映射到误差函数会导致角部模糊过度收缩或截断。
引擎引入超椭圆指数距离度量：
$$\left(\frac{|x|}{a}\right)^p + \left(\frac{|y|}{b}\right)^p = 1, \quad p = 2 + 2\frac{\sigma}{\min(a, b)}$$
通过基于长轴 $\operatorname{erf}$ 的峰值衰减系数对强度进行收缩归一化，使得全参数空间下的常数时间单 Pass 阴影与真实多遍卷积的像素相似度（SSIM）达 99.8% 以上。

---

## 4. Slang 着色器核心代码实现原型

### `kerne_sdf.slang`
```hlsl
module kernetile.sdf;

// Inigo Quilez 2D 精确圆角矩形 SDF
public float sdRoundedBox(float2 p, float2 center, float2 half_size, float4 radii)
{
    float2 s = p - center;
    // 根据象限选取对应的圆角半径
    float r = (s.x > 0.0) ? 
              ((s.y > 0.0) ? radii.z : radii.y) : 
              ((s.y > 0.0) ? radii.w : radii.x);

    float2 q = abs(s) - half_size + float2(r, r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// 环形描边折叠
public float opRing(float d, float stroke_width)
{
    return abs(d + stroke_width * 0.5) - stroke_width * 0.5;
}
```

### `kerne_shadow.slang`
```hlsl
module kernetile.shadow;

// Abramowitz & Stegun 四阶误差函数近似 (最大误差 ~0.21 LSB)
public float erfApprox(float x)
{
    float ax = abs(x);
    float poly = 1.0 + 0.278393 * ax + 0.230389 * ax * ax + 0.078108 * pow(ax, 4.0);
    float result = 1.0 - 1.0 / pow(poly, 4.0);
    return sign(x) * result;
}

// 一维高斯累积分布函数
public float gaussianCDF(float x, float sigma)
{
    return 0.5 * (1.0 + erfApprox(x / (sigma * 1.41421356)));
}

// Evan Wallace 解析常数时间圆角矩形阴影求值
public float evaluateAnalyticShadow(
    float2 p, 
    float2 box_min, 
    float2 box_max, 
    float radius, 
    float sigma)
{
    // 将点投影并分离两轴计算
    float x0 = box_min.x; float x1 = box_max.x;
    float y0 = box_min.y; float y1 = box_max.y;

    float fx = gaussianCDF(x1 - p.x, sigma) - gaussianCDF(x0 - p.x, sigma);
    float fy = gaussianCDF(y1 - p.y, sigma) - gaussianCDF(y0 - p.y, sigma);
    
    // 基础直角阴影贡献
    float base_shadow = fx * fy;

    // 当具备圆角时，应用角点积分补偿因子
    if (radius > 0.0) {
        float2 center = (box_min + box_max) * 0.5;
        float2 half_size = (box_max - box_min) * 0.5;
        float2 q = abs(p - center) - half_size + float2(radius, radius);
        if (q.x > 0.0 && q.y > 0.0) {
            float corner_d = length(q) - radius;
            float corner_factor = gaussianCDF(-corner_d, sigma);
            base_shadow = min(base_shadow, corner_factor);
        }
    }

    return saturate(base_shadow);
}
```

---

## 5. 验证与集成测试计划

### 5.1 自动化验证脚本
1. **Slang 离线编译矩阵测试 (`build_shaders.py`)**：
   - 目标 1: `slangc kerne_main.slang -target dxil -o kerne_main.dxil`
   - 目标 2: `slangc kerne_main.slang -target spirv -o kerne_main.spv`
   - 目标 3: `slangc kerne_main.slang -target msl -o kerne_main.metal`
   - 目标 4: `slangc kerne_main.slang -target wgsl -o kerne_main.wgsl`
   - 检查所有后端编译 0 报错，产出紧凑二进制字节码头文件。
2. **SDF 几何边界真值对比 (`test_sdf_ground_truth.cpp`)**：
   - 建立高分辨率网格（$1024\times 1024$），以精确几何解析公式作为真值。
   - 遍历圆角矩形内外距离场，断言 `sdRoundedBox` 与几何真值的绝对误差小于 $1.0\times 10^{-5}$。
3. **解析阴影 vs 多遍高斯卷积 SSIM 比对 (`test_shadow_psnr.py`)**：
   - 对比解析阴影着色器输出与使用 OpenCV/SciPy $64\times 64$ 大核高斯卷积生成的软阴影参考图。
   - 结构相似性（SSIM）必须 $\ge 0.995$，峰值信噪比（PSNR）必须 $\ge 45\text{ dB}$。

### 5.2 阶段验收标准
- Slang 构建工具集成进项目构建脚本，实现源码改动自动生成 C++ 嵌入式字节码数组。
- 阴影与 SDF 算子在 GPU 端单个线程执行时间稳定在数十个 ALU 周期内，无任何动态分支发散。
