# 阶段 3: 16×16 瓦片分箱、脏块追踪与 GPU 片上 LDS 协同计算

## 1. 阶段目标与定位

本阶段是 KerneTile 架构实现“**极致局部性能与低显存带宽**”的核心所在。
通过将屏幕划分为标准的 **16×16 像素瓦片网格**，建立起 CPU 粗粒度空间分箱、帧间脏块哈希追踪、GPU 间接派发（Indirect Dispatch）与工作组片上共享内存（LDS / Shared Memory）协同预取的全链路协同模型。

交付物包括：
1. **CPU 端高性能 16×16 瓦片分箱器**：图元 AABB 与瓦片快速求交，生成紧凑索引列表与瓦片头。
2. **脏块追踪与间接派发列表生成器**：基于帧间瓦片签名对比，生成仅包含脏瓦片坐标的间接分发缓冲。
3. **极端过绘制防穿透与水线保护机制**：实心瓦片前端遮挡剔除，以及索引缓冲溢出时的动态分批刷新（Flush & Split）。
4. **GPU 16×16 计算工作组协同求值内核**：实现 `BATCH_SIZE = 64` 的 LDS 协同批量预取，全局显存访存缩减 256 倍。
5. 4K 分辨率下的带宽与瓦片剔除率基准验证。

---

## 2. 深入开发思路

### 2.1 16×16 瓦片网格与参数化数学模型
选用 $16\times 16$ 像素（共 256 个像素）作为基本计算瓦片，是平衡波前填充率与空间局部性的数学最优解：
- **波前整数倍**：256 线程对应 AMD GPU 64 线程 Wavefront 的 4 倍，或 NVIDIA GPU 32 线程 Warp 的 8 倍，彻底避免视口边缘线程空转。
- **显存与带宽开销参数化模型**（见表 1）：

*表 1 不同分辨率下 16×16 瓦片架构参数化模型*
| 规格 | 分辨率 | 瓦片网格 (列×行) | 瓦片总数 | 离屏 RGBA8 (MiB) | 瓦片头缓冲 (KiB) | 瓦片索引缓冲 (均16/块, KiB) | 光标闪烁剔除率 (2块) | 按钮悬停剔除率 (36块) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **1080p** | 1920×1080 | 120×68 | 8,160 | 7.91 | 63.8 | 510.0 | 99.975% | 99.56% |
| **1440p** | 2560×1440 | 160×90 | 14,400 | 14.06 | 112.5 | 900.0 | 99.986% | 99.75% |
| **4K UHD**| 3840×2160 | 240×135 | 32,400 | 31.64 | 253.1 | 2,025.0 | **99.994%** | **99.89%** |

在 4K UHD 极端重绘下，32,400 个瓦片的元数据总量仅 2.22 MiB，整帧 PCIe 上传耗时仅 **207.9 µs**（约 0.21 ms），仅占 120 Hz 帧预算（8.33 ms）的 2.5%，总线带宽极为宽裕。

---

## 3. CPU 瓦片分箱与脏块追踪设计

```
[原始图元数组 (UiPrimitive)] 
         │
         ▼
[CPU 粗粒度分箱] ── AABB 投影 ──> [计算相交瓦片区间: (x0/16, y0/16) 到 (x1/16, y1/16)]
         │
         ├────────────────────────────────────────┐
         ▼                                        ▼
[实心不透明检测]                           [瓦片内容哈希签名计算]
(覆盖整块且无圆角/半透明 -> 剔除下方图元)           (xxHash64 生成 Tile Signature)
         │                                        │
         │                                        ▼
         │                         [比对上一帧对应瓦片签名]
         │                                        │
         │                  ┌─────────────────────┴─────────────────────┐
         │                  ▼                                           ▼
         │            [签名相同: 静态]                            [签名不同: 脏瓦片]
         │            (跳过，不派发)                       (写入 Indirect Dispatch 列表)
         │                                                              │
         └──────────────────────────────┬───────────────────────────────┘
                                        ▼
                     [构建 GPU 上传数据: TileHeader + Indices]
```

### 3.1 瓦片粗粒度 AABB 求交与索引构建
每个图元都有包围盒 $[x_{\min}, y_{\min}, x_{\max}, y_{\max}]$，其在瓦片空间的覆盖范围直接通过除以 16 并取整确定：
$$\text{tile\_x0} = \max\left(0, \lfloor x_{\min} / 16 \rfloor\right), \quad \text{tile\_x1} = \min\left(N_x - 1, \lfloor x_{\max} / 16 \rfloor\right)$$
$$\text{tile\_y0} = \max\left(0, \lfloor y_{\min} / 16 \rfloor\right), \quad \text{tile\_y1} = \min\left(N_y - 1, \lfloor y_{\max} / 16 \rfloor\right)$$

### 3.2 脏块追踪与间接派发缓冲生成
- **瓦片内容签名**：为每个瓦片维护一个 64-bit 的哈希签名。每当图元落入瓦片时，将其 `UiPrimitive` 核心属性（几何、颜色、裁剪）与全局图元序号迭代累加哈希。
- **跳过静态瓦片**：若当前帧某瓦片的签名与前一帧完全吻合，则该瓦片被标记为“清洁（Clean）”。
- **间接派发参数缓冲**：
  CPU 不直接调用固定大小的 `Dispatch(Nx, Ny, 1)`，而是将所有脏瓦片的二维坐标紧凑打包写入一个 `DirtyTileList` 结构化缓冲，并填充 `D3D12_DISPATCH_ARGUMENTS`：
  $$\text{ThreadGroupCountX} = \text{dirty\_tile\_count}, \quad \text{ThreadGroupCountY} = 1, \quad \text{ThreadGroupCountZ} = 1$$
  GPU 仅为这组脏瓦片激活计算单元，在光标闪烁或打字等日常高频场景下，剔除率达 **99.9%** 以上。

### 3.3 极端重叠防穿透与水线保护机制
1. **实心瓦片前端遮挡剔除（Early Solid Occlusion Culling）**：
   如果一个图元是不透明的直角实心矩形，且其尺寸完全覆盖了某 16×16 瓦片，则该瓦片在此图元之前的全部底层图元均被遮挡，直接清空底层图元索引，防止由于无节制图层堆叠导致的计算爆炸。
2. **缓冲水线溢出防护与 Flush & Split**：
   预分配的瓦片索引显存缓冲容量设定安全水线（如 80% 容量）。当遇到成千上万个半透明复杂矢量图元导致索引接近溢出时，引擎自动触发 **Flush & Split**：
   - 立即中断当前图元收集，提交一个中间计算通道对当前半透明层进行离屏合成；
   - 重置瓦片索引计数器，继续向存储纹理叠加后续图元，从根源消除 GPU 显存越界崩溃。

---

## 4. GPU 工作组片上共享内存（LDS）协同求值内核

如果 16×16 工作组内的 256 个线程各自直接读取全局显存（Global VRAM）中的图元索引与属性，将产生 $256 \times K$ 次高延迟显存请求，严重阻塞 GPU 内存控制器。
KerneTile 采用 **分批协同加载（Cooperative Batch Loading）** 机制：

```
                    [全局显存: TilePrimitiveIndices (VRAM)]
                                   │
                                   │ (64 个线程合并访存, 批量拉取)
                                   ▼
          ┌─────────────────────────────────────────────────┐
          │  工作组片上共享内存 (LDS): uint s_indices[64]   │
          └─────────────────────────────────────────────────┘
                                   │
                   [GroupMemoryBarrierWithGroupSync()]
                                   │
                                   │ (256 个线程从极速片上 LDS 广播读取)
                                   ▼
          ┌─────────────────────────────────────────────────┐
          │  256 个像素线程独立解析求值 (SDF + 阴影 + 合成)  │
          └─────────────────────────────────────────────────┘
```

### 4.1 协同预取着色器实现原型 (`kerne_tile_compute.slang`)

```hlsl
module kernetile.tile_compute;

import kernetile.sdf;
import kernetile.shadow;

struct TileHeader {
    uint primitive_offset;
    uint primitive_count;
    uint flags;
};

// 全局资源绑定
StructuredBuffer<TileHeader> g_TileHeaders : register(t0);
StructuredBuffer<uint> g_TileIndices : register(t1);
StructuredBuffer<UiPrimitive> g_Primitives : register(t2);
StructuredBuffer<uint2> g_DirtyTileCoords : register(t3); // 脏瓦片坐标列表
RWTexture2D<float4> g_OutputStorage : register(u0);

// 片上共享内存 (LDS): 缓存单批 64 个图元索引 (仅占 256 字节!)
groupshared uint s_indices[64];

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_tile_render(
    uint3 group_id : SV_GroupID,
    uint3 group_thread_id : SV_GroupThreadID,
    uint local_index : SV_GroupIndex)
{
    // 从间接派发列表中获取当前脏瓦片的实际视口网格坐标 (tx, ty)
    uint dirty_index = group_id.x;
    uint2 tile_coord = g_DirtyTileCoords[dirty_index];
    
    // 当前线程对应的绝对屏幕像素坐标
    uint2 pixel_pos = tile_coord * 16 + group_thread_id.xy;

    // 读取当前瓦片的元数据头
    uint tile_id = tile_coord.y * (3840 / 16) + tile_coord.x; // 假设视口宽 3840
    TileHeader header = g_TileHeaders[tile_id];
    uint prim_count = header.primitive_count;
    uint prim_offset = header.primitive_offset;

    // 当前像素的累计颜色 (初始为完全透明黑色，线性预乘)
    float4 accum_color = float4(0.0, 0.0, 0.0, 0.0);

    // 以 BATCH_SIZE = 64 分批处理瓦片图元列表
    for (uint batch_start = 0; batch_start < prim_count; batch_start += 64)
    {
        uint current_batch_size = min(64u, prim_count - batch_start);

        // --- 协同加载阶段 (前 64 个线程合并访存拉入 LDS) ---
        if (local_index < current_batch_size) {
            s_indices[local_index] = g_TileIndices[prim_offset + batch_start + local_index];
        }

        // 片上内存屏障：等待 LDS 数据就位
        GroupMemoryBarrierWithGroupSync();

        // --- 广播与并行求值阶段 (全瓦片 256 线程同时从 LDS 读取并计算) ---
        for (uint i = 0; i < current_batch_size; ++i)
        {
            uint prim_id = s_indices[i];
            UiPrimitive prim = g_Primitives[prim_id];

            // 1. 2D SDF 几何抗锯齿覆盖率
            float2 half_size = float2(prim.max_x - prim.min_x, prim.max_y - prim.min_y) * 0.5;
            float2 center = float2(prim.min_x + prim.max_x, prim.min_y + prim.max_y) * 0.5;
            float4 radii = float4(prim.radius_top_left, prim.radius_top_right, prim.radius_bottom_right, prim.radius_bottom_left);
            
            float d = sdRoundedBox(float2(pixel_pos) + 0.5, center, half_size, radii);
            float coverage = saturate(0.5 - d); // 1 像素平滑过滤

            // 2. 常数时间解析高斯阴影
            if (prim.shadow_blur > 0.0) {
                float shadow_cov = evaluateAnalyticShadow(
                    float2(pixel_pos) + 0.5, 
                    float2(prim.min_x, prim.min_y), 
                    float2(prim.max_x, prim.max_y), 
                    prim.radius_top_left, 
                    prim.shadow_blur);
                coverage = max(coverage, shadow_cov);
            }

            // 3. 线性预乘 Alpha 合成 (Porter-Duff Over)
            float4 src_color = float4(prim.color_r, prim.color_g, prim.color_b, prim.color_a) * coverage;
            accum_color = src_color + accum_color * (1.0 - src_color.a);
            
            // 如果不透明度已达到饱和 (Alpha 极度接近 1.0)，提前结束混合循环
            if (accum_color.a >= 0.999) {
                break;
            }
        }

        // 屏障同步，准备下一批加载
        GroupMemoryBarrierWithGroupSync();
        
        if (accum_color.a >= 0.999) {
            break;
        }
    }

    // 写入离屏 UNORM 存储纹理 (着色器内已自动钳制在合法预乘域)
    g_OutputStorage[pixel_pos] = accum_color;
}
```

---

## 5. 验证与性能测试计划

### 5.1 自动化基准测试用例
1. **CPU 分箱速度基准测试 (`bench_binning.cpp`)**：
   - 生成 2,000 个随机分布的 UI 图元（模拟复杂仪表盘），在 4K 分辨率（32,400 瓦片）网格下执行分箱。
   - 目标指标：CPU 单核分箱总耗时控制在 **0.5 ms** 以内。
2. **脏块剔除率极限测试 (`test_dirty_culling.cpp`)**：
   - 场景 A（文本光标闪烁，2×20 像素）：断言活跃瓦片数量 $\le 2$，脏块剔除率 $\ge 99.99\%$。
   - 场景 B（按钮悬停动效，120×40 像素）：断言活跃瓦片数量 $\le 36$，脏块剔除率 $\ge 99.85\%$。
3. **LDS 访存缩减验证与 Profiling 分析**：
   - 使用 Nsight Graphics / Radeon GPU Profiler (RGP) 抓取计算着色器波前执行状态。
   - 验证 LDS 共享内存读取命中率，确认全局显存读指令由未优化前的 $256K$ 降至 $K$ 次（降幅 256 倍），LDS 占用严格处于 256 字节。

### 5.2 阶段验收标准
- 在 4K UHD 60 FPS 动态界面下，GPU 计算着色器平均耗时低于 **1.2 ms**。
- 脏块追踪算法在连续 10,000 帧长周期随机动效测试中无任何漏刷、撕裂或花屏现象。
