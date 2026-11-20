# 阶段 9: WARP 与自适应并发 SIMD CPU 软件光栅回退

## 1. 阶段目标与定位

本阶段的核心是为 KerneTile 引擎打造坚不可摧的 **无 GPU 硬件时的全功能软件回退（Software Fallback）能力**。
在无物理独显/核显的服务器（Headless）、CI/CD 自动化构建虚拟机、老旧嵌入式设备，或显卡驱动崩溃受限的安全模式下，引擎必须能够无缝平滑降级，且保证输出画面的几何精度与色彩空间与 GPU 硬件路径绝对一致。

交付物包括：
1. **Windows 平台 Direct3D 12 WARP 软件光栅适配器驱动**：零修改复用全套 Slang/DXIL 计算内核。
2. **跨平台纯 CPU 16×16 瓦片光栅器**：借鉴 Vello CPU 稀疏条带（Sparse Strips）与 Blend2D SIMD 矢量化思想。
3. **自适应并发线程调度器（Adaptive Concurrency Dispatcher）**：针对微小脏区（1–4 线程）与大面积重绘（8 线程）自适应收缩扩展，规避线程同步开销反噬。
4. **CPU 与 GPU 像素级色彩一致性检验套件**（最大色差控制在 $\le 1\text{ LSB}$）。

---

## 2. 深入开发思路

### 2.1 软硬件回退双轨策略

```
                              [设备能力自检与适配器探测]
                                         │
                 ┌───────────────────────┴───────────────────────┐
                 ▼ (检测到现代独立/集成 GPU)                     ▼ (无可用计算 GPU / 虚拟机 / CI)
       [硬件计算管线 (GPU Path)]                           [软件回退管线 (Fallback Path)]
       - Direct3D 12 / Vulkan / Metal                                    │
       - 16×16 瓦片工作组 (LDS 协同预取)                 ┌───────────────┴───────────────┐
       - GPU 间接派发调度                                ▼ (Windows 平台)                ▼ (Linux/macOS/嵌入式)
                                                 [D3D12 WARP 驱动]             [多线程 SIMD CPU 瓦片光栅]
                                                 - 复用相同 DXIL 计算内核      - AVX2 / NEON 向量化
                                                 - 微软内置高度多线程优化      - 自适应并发线程调度 (1-8T)
```

### 2.2 Windows 平台最优解：Direct3D 12 WARP
Windows 高级光栅化平台（WARP）是微软自 Windows 8 以来内置的高性能纯 CPU 光栅化实现（在 Windows 11 中已全面支持 Direct3D 12 Feature Level 12_2）：
- **零算法维护成本**：WARP 直接以多线程 JIT 方式在 CPU 上执行 DXIL 计算着色器。
- **无缝接入**：在阶段 2 的 HAL 中，当探测到物理适配器不可用时，通过 `IDXGIFactory4::EnumWarpAdapter` 获得 WARP 适配器并创建同一套 `D3D12Device`。阶段 1 与阶段 3 的 Slang 计算着色器无需任何分支代码修改即可在 WARP 上高速跑满 CPU 核心。

---

### 2.3 跨平台 CPU 瓦片光栅化：借鉴 Vello CPU 与 Blend2D
在非 Windows 平台，必须依靠自研的纯 CPU 光栅器。
ETH Zürich 硕士论文（Laurenz Stampfl, 2025）将 Vello CPU 接入 `bl_bench` 进行对比实测，揭示出两条极具指导意义的客观规律：
1. **分块条带结合 SIMD 的绝对优势**：面对复杂密集形状，传统单线程扫描线（如 Skia Raster 与 Cairo）在 256×256 尺寸下耗时高达 422 ms 与 813 ms，而基于分块条带的 Vello CPU 仅需 100 ms（领先 4.22×–8.13×）。在 8×8 至 32×32 的小尺寸图标负载下，分块 SIMD 更是全面反超运行时 JIT 的 Blend2D。
2. **多核扩展的边界陷阱（同步开销反噬）**：
   - 在 32×32 至 256×256 较大尺寸负载下，8 线程多核能维持 **5.1×–5.3×** 的高加速比；
   - 但在极小（8×8）尺寸下，8 线程耗时（1.40 ms，加速比 2.4×）**反而显著慢于 4 线程（1.05 ms，加速比 3.3×）**！这是由于小负载下工作窃取与线程屏障等待损耗远超计算量。

#### 自适应并发调度算法（Adaptive Concurrency）
针对上述实验规律，KerneTile 软件回退路径确立自适应瓦片分发规则：
```cpp
uint32_t calculate_worker_threads(uint32_t dirty_tile_count, uint32_t cpu_cores) noexcept {
    if (dirty_tile_count <= 8) {
        return 1; // 极小局部更新 (如光标闪烁): 纯单线程执行，零上下文切换
    } else if (dirty_tile_count <= 64) {
        return std::clamp(cpu_cores / 4, 2u, 4u); // 轻微交互: 2-4 线程
    } else {
        return std::clamp(cpu_cores, 4u, 8u);     // 大面积/全屏重绘: 启动最大 8 线程
    }
}
```

---

## 3. CPU 瓦片光栅器 SIMD 核心实现原型

CPU 端通过 AVX2 / NEON 向量寄存器同时处理单瓦片内 8 个像素的 SDF 求值与预乘 Alpha 混合：

```cpp
#pragma once
#include <immintrin.h>
#include "kerne_core.hpp"

namespace kernetile::cpu {

// AVX2 批量计算 8 个像素的有向距离场覆盖率 (伪代码框架)
inline __m256 evaluate_sdf_coverage_avx2(
    __m256 px, __m256 py, 
    float center_x, float center_y, 
    float half_w, float half_h, 
    float radius)
{
    __m256 cx = _mm256_set1_ps(center_x);
    __m256 cy = _mm256_set1_ps(center_y);
    __m256 hw = _mm256_set1_ps(half_w - radius);
    __m256 hh = _mm256_set1_ps(half_h - radius);
    __m256 r  = _mm256_set1_ps(radius);
    __m256 zero = _mm256_setzero_ps();

    // q = abs(p - c) - (b - r)
    __m256 qx = _mm256_sub_ps(_mm256_andnot_ps(_mm256_set1_ps(-0.0f), _mm256_sub_ps(px, cx)), hw);
    __m256 qy = _mm256_sub_ps(_mm256_andnot_ps(_mm256_set1_ps(-0.0f), _mm256_sub_ps(py, cy)), hh);

    // length(max(q, 0.0)) + min(max(qx, qy), 0.0) - r
    __m256 max_qx = _mm256_max_ps(qx, zero);
    __m256 max_qy = _mm256_max_ps(qy, zero);
    __m256 len = _mm256_sqrt_ps(_mm256_add_ps(_mm256_mul_ps(max_qx, max_qx), _mm256_mul_ps(max_qy, max_qy)));
    __m256 max_component = _mm256_max_ps(qx, qy);
    __m256 inner = _mm256_min_ps(max_component, zero);
    
    __m256 dist = _mm256_sub_ps(_mm256_add_ps(len, inner), r);

    // 平滑抗锯齿覆盖率: saturate(0.5 - dist)
    __m256 cov = _mm256_sub_ps(_mm256_set1_ps(0.5f), dist);
    return _mm256_max_ps(zero, _mm256_min_ps(_mm256_set1_ps(1.0f), cov));
}

} // namespace kernetile::cpu
```

---

## 4. 跨平台色彩与像素一致性保障

为了消除不同渲染后端之间的微小色彩分歧，CPU 回退路径严守与 GPU 计算着色器完全相同的数学参数：
1. **严格复用 Abramowitz & Stegun 逼近系数**：CPU 端计算解析阴影时，必须调用与 Slang 着色器完全一致的多项式系数（`0.278393`、`0.230389`、`0.078108`），禁止在 CPU 端盲目调用系统 `std::erf`，杜绝浮点微小差异累积。
2. **定点化色深防溢出**：所有中间混合全程保持 32-bit float，最后写出内存缓冲时通过四舍五入并钳制（`round-to-nearest-even`）转化为 8-bit RGBA。

---

## 5. 验证与一致性测试计划

### 5.1 自动化对比与回归测试用例
1. **GPU vs WARP vs CPU 像素级金标比对 (`test_backend_conformance.cpp`)**：
   - 构建全要素测试用例（含圆角矩形、等宽描边、外扩软阴影、文本字形、毛玻璃背景）。
   - 分别在 Direct3D 12 独显硬件、D3D12 WARP 软件光栅、以及纯 CPU SIMD 瓦片光栅器下执行离屏渲染。
   - 逐像素提取 RGBA8 差值图。
   - 验收指标：**全屏最大绝对像素色差必须 $\le 1\text{ LSB}$（无任何可见肉眼差异），SSIM 结构相似度 $\ge 0.9995$**。
2. **CPU 回退自适应并发性能测试 (`bench_cpu_scalability.cpp`)**：
   - 验证光标微观更新场景（2 瓦片）下单线程执行耗时控制在 **$15\,\mu\text{s}$** 以内，无多线程唤醒抖动。
   - 验证整屏重绘场景（4K 分辨率）下 8 线程加速比达到 **$4.8\times–5.2\times$**。

### 5.2 阶段验收标准
- 在完全禁用 GPU（模拟 Headless 服务器）环境下，引擎能自动检测并平滑切入 CPU 回退路径，保证所有 UI 组件绘制与布局测试 100% 正常运行。
- 回退路径无任何外部庞大依赖，单核 CPU 光栅化吞吐满足日常办公场景 60 FPS 基本流畅度。
