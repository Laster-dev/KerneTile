# 阶段 7: Dual Kawase 多级降采样模糊与视觉滤镜

## 1. 阶段目标与定位

本阶段的核心是为 KerneTile 引擎构建高性能、极低带宽消耗的 **非局域视觉滤镜管线**，特别是针对现代操作系统的毛玻璃（Backdrop Blur / Acrylic）与复杂阴影光晕。
对于解析阴影（阶段 1），我们可以直接通过常数时间误差函数数学求值；但对于**任意动态底层背景的毛玻璃模糊**，必须读取底层已合成像素执行滤波。如果在 4K 分辨率下使用传统的大核高斯卷积（如 $64\times 64$ 采样核），显存带宽将瞬时被打满导致严重掉帧。

交付物包括：
1. **Marius Bjørge SIGGRAPH 2015 Dual Kawase 多级滤波管线**：实现降采样金字塔与升采样重构。
2. **多档工程质量预设**（低档 3 遍、中档 5 遍、高档 7 遍调度器）。
3. **计算着色器同步屏障（Compute Barrier）与离屏背景捕获流**。
4. **嵌套裁剪栈（`ClipRect` 树）硬件加速过滤**。
5. 4K 复杂毛玻璃窗口帧耗时与带宽压测套件。

---

## 2. 深入开发思路

### 2.1 为什么选择 Dual Kawase 双阶段滤波？
在移动端、低功耗笔记本与 4K 高分辨率下，显存带宽是决定实时交互帧率的生死线。
- **传统多遍二维高斯模糊**：即便分解为水平与垂直两遍，在 $\sigma = 32$ 时单像素仍需数十次纹理采样，且全程在全分辨率（$3840\times 2160$）下进行，带宽开销达数百兆字节/帧。
- **Dual Kawase 算法革命**：
  将滤波解耦为**对称的几何降采样（Downsample）与升采样（Upsample）**。

```
Dual Kawase 纹理金字塔与派发管线:
[全分辨率背景输入 (3840×2160)]
         │
         │ (Downsample Pass 1: 半分辨率, 4 采样点硬件插值)
         ▼
[1/2 分辨率纹理 (1920×1080)]
         │
         │ (Downsample Pass 2: 1/4 分辨率, 4 采样点硬件插值)
         ▼
[1/4 分辨率纹理 (960×540)] ◄── 最低带宽滤波核心
         │
         │ (Upsample Pass 1: 升采样平滑融合, 8 采样点加权)
         ▼
[1/2 分辨率重构纹理 (1920×1080)]
         │
         │ (Upsample Pass 2: 升采样全屏重构)
         ▼
[毛玻璃最终合成纹理 (3840×2160)] ── 注入主计算着色器与前景 Alpha 混合
```

#### 2.1.1 核心采样数学与硬件双线性插值
- **降采样通道**：利用硬件双线性纹理采样器（Bilinear Filter），将采样点偏移至四个半像素对角线处。仅需 **4 次纹理 Fetch**，即可在降采样的同时等效融合 $4\times 4 = 16$ 个原始像素的加权信息。
- **升采样通道**：采用 8 采样点星型（Tent Filter）邻域加权插值：中心点权重为 4，正交轴权重为 2，对角线权重为 1。消除由于低分辨率拉伸产生的棋盘格与网格走样伪影。

---

### 2.2 三档质量预设与派发策略
根据界面控件对模糊半径与视觉细腻度的不同诉求，提供自适应质量分级：

*表 1 Dual Kawase 质量分级与 GPU 派发轮次*
| 质量档位 | 派发通道数 (Passes) | 金字塔深度 | 最小中间纹理分辨率 (以 4K 为例) | 适用场景 |
| :--- | :--- | :--- | :--- | :--- |
| **低档 (Low)** | 3 次（1 降 1 升 1 拷） | 1/2 | $1920\times 1080$ | 轻微暗淡背景、轻量下拉菜单、移动端节电模式 |
| **中档 (Medium)** | 5 次（2 降 2 升 1 拷） | 1/4 | $960\times 540$ | 标准对话框、抽屉侧边栏、控制面板磨砂 |
| **高档 (High)** | 7 次（3 降 3 升 1 拷） | 1/8 | $480\times 270$ | 大半径 Acrylic 窗口、漫反射环境发光、重度毛玻璃卡片 |

在 4K 分辨率下，高档 Dual Kawase 的核心采样绝大多数发生在前所未有的 $480\times 270$ 微小纹理上，将全屏模糊耗时从传统高斯模糊的 $12\text{ ms}$ 断崖式压减至 **$0.6\text{ ms}$** 以内！

---

## 3. 着色器实现原型与金字塔算法

### 3.1 降采样着色器 (`kerne_kawase_down.slang`)
```hlsl
module kernetile.kawase_down;

Texture2D<float4> g_InputTexture : register(t0);
SamplerState g_LinearSampler : register(s0);
RWTexture2D<float4> g_OutputTexture : register(u0);

cbuffer BlurUniforms : register(b0) {
    float2 g_TexelSize; // 输入纹理单像素 UV 尺寸 (1/w, 1/h)
    float g_Offset;     // 采样步进偏移
};

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_kawase_down(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 out_pixel = dispatch_thread_id.xy;
    // 输出纹理中心对应的 UV
    float2 uv = (float2(out_pixel) + 0.5) * (g_TexelSize * 2.0);

    // 4 采样点半对角线偏移采样 (硬件双线性自动均值)
    float2 half_pixel = g_TexelSize * g_Offset;
    float4 sum = g_InputTexture.SampleLevel(g_LinearSampler, uv, 0) * 4.0;
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv - half_pixel, 0);
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + half_pixel, 0);
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(half_pixel.x, -half_pixel.y), 0);
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(-half_pixel.x, half_pixel.y), 0);

    g_OutputTexture[out_pixel] = sum * 0.125; // 归一化输出
}
```

### 3.2 升采样着色器 (`kerne_kawase_up.slang`)
```hlsl
module kernetile.kawase_up;

Texture2D<float4> g_InputTexture : register(t0);
SamplerState g_LinearSampler : register(s0);
RWTexture2D<float4> g_OutputTexture : register(u0);

cbuffer BlurUniforms : register(b0) {
    float2 g_TexelSize; // 输入纹理尺寸
    float g_Offset;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_kawase_up(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 out_pixel = dispatch_thread_id.xy;
    float2 uv = (float2(out_pixel) + 0.5) * (g_TexelSize * 0.5);

    float2 d = g_TexelSize * g_Offset;

    // 8 采样点加权重构滤波
    float4 sum = 0.0;
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(-d.x * 2.0, 0.0), 0);
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(-d.x, d.y), 0) * 2.0;
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(0.0, d.y * 2.0), 0);
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(d.x, d.y), 0) * 2.0;
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(d.x * 2.0, 0.0), 0);
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(d.x, -d.y), 0) * 2.0;
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(0.0, -d.y * 2.0), 0);
    sum += g_InputTexture.SampleLevel(g_LinearSampler, uv + float2(-d.x, -d.y), 0) * 2.0;

    g_OutputTexture[out_pixel] = sum / 12.0;
}
```

---

## 4. 嵌套 ClipRect 硬件过滤求值

当界面涉及多层嵌套容器、滚动列表或复杂圆角弹窗时，每个图元均携带一个 16-bit 的 `clip_index`。

```hlsl
// 计算着色器内部硬件裁剪过滤
bool isPixelCulledByClip(float2 pixel_pos, uint clip_index)
{
    while (clip_index != 0xFFFFFFFF)
    {
        ClipRect clip = g_ClipRects[clip_index];
        // 直角或圆角裁剪测试
        if (pixel_pos.x < clip.min_x || pixel_pos.x > clip.max_x ||
            pixel_pos.y < clip.min_y || pixel_pos.y > clip.max_y)
        {
            return true; // 落在裁剪盒外，直接剔除
        }
        
        // 如果具备圆角裁剪
        if (clip.corner_radius > 0.0) {
            float2 center = float2(clip.min_x + clip.max_x, clip.min_y + clip.max_y) * 0.5;
            float2 half_size = float2(clip.max_x - clip.min_x, clip.max_y - clip.min_y) * 0.5;
            float d = sdRoundedBox(pixel_pos, center, half_size, clip.corner_radius.xxxx);
            if (d > 0.0) return true;
        }

        // 递归迭代至父级裁剪矩形
        clip_index = clip.parent_index;
    }
    return false;
}
```

---

## 5. 验证与性能测试计划

### 5.1 自动化基准与性能测试
1. **4K Acrylic 全屏毛玻璃耗时压测 (`bench_dual_kawase.cpp`)**：
   - 载入一张 4K 复杂壁纸背景。
   - 触发高档（7 Passes）Dual Kawase 模糊并全屏合成。
   - 记录 GPU 计算通道耗时与显存带宽。
   - 验收指标：在高档预设下，主流独显（RTX 3060 / RX 6600 级）全通道耗时必须 **$< 0.8\text{ ms}$**，集显（Intel Iris Xe / Apple M1）耗时 **$< 2.5\text{ ms}$**。
2. **边缘无撕裂与闪烁测试 (`test_blur_boundary.cpp`)**：
   - 在高对比度边缘（全白背景贴近全黑）进行大半径模糊测试。
   - 验证中间纹理边界寻址（Wrap Mode 必须为 ClampToEdge），检查四个角与边界区域是否有黑色发暗暗边渗溢。
3. **嵌套 ClipRect 深度压测 (`test_nested_clips.cpp`)**：
   - 构造 16 层嵌套的滚动圆角容器。
   - 断言：像素裁剪判断逻辑全展开在硬件寄存器内，单瓦片无线程死锁，计算无性能悬崖。

### 5.2 阶段验收标准
- 毛玻璃滤镜在频繁拖动移动时，完全融入阶段 3 的 16×16 瓦片网格与脏块剔除管线，仅对毛玻璃覆盖的相交瓦片执行合成，避免全屏盲目重构。
- 模糊过渡平滑无阶梯感，SSIM 相对参考级高斯卷积高于 0.992。
