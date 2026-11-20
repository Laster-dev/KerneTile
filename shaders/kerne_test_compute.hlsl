// KerneTile: Test Compute Shader (16x16 Workgroup)
// Validates end-to-end integration of SDF, shadow, and color composition

#include "kerne_types.slang"
#include "kerne_sdf.slang"
#include "kerne_shadow.slang"
#include "kerne_color.slang"

StructuredBuffer<UiPrimitive> g_Primitives  : register(t0);
StructuredBuffer<uint>        g_TileIndices : register(t1);
StructuredBuffer<TileHeader>  g_TileHeaders : register(t2);
RWTexture2D<float4>           g_Output      : register(u0);

[numthreads(16, 16, 1)]
void cs_main(
    uint3 dispatch_thread_id : SV_DispatchThreadID,
    uint3 group_id           : SV_GroupID,
    uint3 group_thread_id    : SV_GroupThreadID,
    uint  group_index        : SV_GroupIndex)
{
    uint2 pixel_pos = dispatch_thread_id.xy;
    float2 p = float2(pixel_pos) + 0.5;

    // 单瓦片 ID 索引
    uint tile_id = group_id.y * 240 + group_id.x; // 假设 4K 列数 240
    TileHeader header = g_TileHeaders[tile_id];
    uint prim_count = header.count_and_flags & 0xFFFF;
    uint prim_offset = header.primitive_offset;

    float4 accum_color = float4(0.0, 0.0, 0.0, 0.0);

    for (uint i = 0; i < prim_count; ++i)
    {
        uint prim_index = g_TileIndices[prim_offset + i];
        UiPrimitive prim = g_Primitives[prim_index];

        // 1. SDF 几何抗锯齿覆盖率
        float2 half_size = float2(prim.max_x - prim.min_x, prim.max_y - prim.min_y) * 0.5;
        float2 center = float2(prim.min_x + prim.max_x, prim.min_y + prim.max_y) * 0.5;
        float4 radii = float4(prim.radius_top_left, prim.radius_top_right, prim.radius_bottom_right, prim.radius_bottom_left);

        float d = sdRoundedBox(p, center, half_size, radii);
        
        // 描边处理
        if (prim.border_width > 0.0) {
            d = opRing(d, prim.border_width);
        }
        
        float geom_coverage = evaluateCoverage(d);

        // 2. 解析阴影覆盖率
        float shadow_cov = 0.0;
        if (prim.shadow_blur > 0.0 || prim.shadow_spread > 0.0) {
            float2 shadow_p = p - float2(prim.shadow_offset_x, prim.shadow_offset_y);
            shadow_cov = evaluateAnalyticShadow(
                shadow_p, 
                float2(prim.min_x, prim.min_y), 
                float2(prim.max_x, prim.max_y), 
                prim.radius_top_left, 
                prim.shadow_blur, 
                prim.shadow_spread
            );
        }

        float total_coverage = max(geom_coverage, shadow_cov);

        // 3. 线性光预乘混合
        float4 src_color = clampPremul(float4(prim.color_r, prim.color_g, prim.color_b, prim.color_a)) * total_coverage;
        accum_color = blendOver(src_color, accum_color);

        if (accum_color.a >= 0.999) {
            break;
        }
    }

    // 最终编码回 sRGB 并写入存储纹理
    float3 final_srgb = linearToSrgb(accum_color.rgb);
    g_Output[pixel_pos] = float4(final_srgb, accum_color.a);
}
