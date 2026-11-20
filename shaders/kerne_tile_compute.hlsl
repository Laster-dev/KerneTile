// KerneTile: 16x16 Tile Compute Shader with LDS Cooperative Prefetching
// 256 threads per workgroup = 16x16 pixel tile
// BATCH_SIZE = 64 reduces global memory fetches by 256x

#include "kerne_types.slang"
#include "kerne_sdf.slang"
#include "kerne_shadow.slang"
#include "kerne_color.slang"

struct DirtyTileInfo {
    uint tx;
    uint ty;
    uint tile_id;
    uint pad;
};

StructuredBuffer<UiPrimitive>   g_Primitives  : register(t0);
StructuredBuffer<uint>          g_TileIndices : register(t1);
StructuredBuffer<TileHeader>    g_TileHeaders : register(t2);
StructuredBuffer<DirtyTileInfo> g_DirtyTiles  : register(t3); // 脏瓦片元数据 (tx, ty, tile_id)
RWTexture2D<float4>             g_Output      : register(u0);

// 片上共享内存 (LDS): 缓存单批 64 个图元索引 (仅占 256 字节!)
groupshared uint s_indices[64];

[numthreads(16, 16, 1)]
void cs_tile_render(
    uint3 group_id        : SV_GroupID,
    uint3 group_thread_id : SV_GroupThreadID,
    uint  local_index     : SV_GroupIndex)
{
    // 从间接派发列表中读取当前脏瓦片的实际网格元数据
    uint dirty_index = group_id.x;
    DirtyTileInfo dt = g_DirtyTiles[dirty_index];

    // 当前线程对应的绝对屏幕像素坐标 (无除法/取模运算)
    uint2 pixel_pos = uint2(dt.tx, dt.ty) * 16 + group_thread_id.xy;
    float2 p = float2(pixel_pos) + 0.5;

    // 读取当前瓦片的元数据头
    TileHeader header = g_TileHeaders[dt.tile_id];
    uint prim_count = header.count_and_flags & 0xFFFF;
    uint prim_offset = header.primitive_offset;

    float4 accum_color = float4(0.0, 0.0, 0.0, 0.0);

    // 以 BATCH_SIZE = 64 分批协同处理瓦片图元列表
    for (uint batch_start = 0; batch_start < prim_count; batch_start += 64)
    {
        uint current_batch_size = min(64u, prim_count - batch_start);

        // --- 1. 协同加载阶段 (前 64 个线程合并访存拉入 LDS) ---
        if (local_index < current_batch_size) {
            s_indices[local_index] = g_TileIndices[prim_offset + batch_start + local_index];
        }

        // 片上内存屏障：等待 LDS 数据就位 (全部 256 线程协同同步)
        GroupMemoryBarrierWithGroupSync();

        // --- 2. 广播与并行求值阶段 (全瓦片 256 线程同时从 LDS 读取求值) ---
        for (uint i = 0; i < current_batch_size; ++i)
        {
            uint prim_id = s_indices[i];
            UiPrimitive prim = g_Primitives[prim_id];

            // 1. 若存在软阴影，首先将其作为底层阴影投影合成
            if (prim.shadow_blur > 0.0 || prim.shadow_spread > 0.0) {
                float2 shadow_p = p - float2(prim.shadow_offset_x, prim.shadow_offset_y);
                float shadow_cov = evaluateAnalyticShadow(
                    shadow_p,
                    float2(prim.min_x, prim.min_y),
                    float2(prim.max_x, prim.max_y),
                    prim.radius_top_left,
                    prim.shadow_blur,
                    prim.shadow_spread
                );
                // 默认投影半透明黑色 (0, 0, 0, 0.4) 按照阴影覆盖率混合
                float4 shadow_col = float4(0.0, 0.0, 0.0, 0.4) * shadow_cov;
                accum_color = blendOver(shadow_col, accum_color);
            }

            // 2. 几何 SDF 覆盖率与色彩合成
            float2 half_size = float2(prim.max_x - prim.min_x, prim.max_y - prim.min_y) * 0.5;
            float2 center = float2(prim.min_x + prim.max_x, prim.min_y + prim.max_y) * 0.5;
            float4 radii = float4(prim.radius_top_left, prim.radius_top_right, prim.radius_bottom_right, prim.radius_bottom_left);

            float d = sdRoundedBox(p, center, half_size, radii);
            if (prim.border_width > 0.0) {
                d = opRing(d, prim.border_width);
            }
            float geom_coverage = evaluateCoverage(d);

            // 线性预乘 Alpha 混合 (Porter-Duff Over: 新图元叠加在已有色彩之上)
            float4 src_color = clampPremul(float4(prim.color_r, prim.color_g, prim.color_b, prim.color_a)) * geom_coverage;
            accum_color = blendOver(src_color, accum_color);
        }

        // 屏障同步，准备下一批次加载 (所有线程无分支执行屏障，绝对安全无死锁)
        GroupMemoryBarrierWithGroupSync();
    }

    // 写入离屏存储纹理 (DXGI_FORMAT_R8G8B8A8_UNORM)
    g_Output[pixel_pos] = accum_color;
}
