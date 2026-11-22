struct Primitive {
    float4 bounds, radii, color, effects;
    uint uv_min, uv_max, flags_clip;
    float shadow_y;
};
struct Tile { uint x, y, offset, count; };
struct Clip { float4 bounds; float radius; uint parent, flags, reserved; };

#if defined(KT_VULKAN)
#define KT_BINDING(index) [[vk::binding(index, 0)]]
#else
#define KT_BINDING(index)
#endif

KT_BINDING(0) StructuredBuffer<Primitive> primitives : register(t0);
KT_BINDING(1) StructuredBuffer<uint> indices : register(t1);
KT_BINDING(2) StructuredBuffer<Tile> tiles : register(t2);
KT_BINDING(3) StructuredBuffer<Clip> clips : register(t3);
KT_BINDING(4) Texture2D<float> atlas : register(t4);
KT_BINDING(5) SamplerState atlas_sampler : register(s0);
KT_BINDING(6) RWTexture2D<float4> output : register(u0);
KT_BINDING(7) cbuffer Frame : register(b0) {
    uint2 extent;
    uint tile_count, groups_x, clip_count;
};
groupshared uint tile_indices[64];

float box_distance(float2 p, float4 bounds, float4 radii) {
    float2 delta = p - (bounds.xy + bounds.zw) * 0.5;
    float radius = delta.x > 0 ? (delta.y > 0 ? radii.z : radii.y)
                               : (delta.y > 0 ? radii.w : radii.x);
    float2 q = abs(delta) - (bounds.zw - bounds.xy) * 0.5 + radius;
    return length(max(q, 0)) + min(max(q.x, q.y), 0) - radius;
}

float2 erf_approx(float2 x) {
    float2 v = abs(x);
    float2 t = 1.0 / (1.0 + 0.3275911 * v);
    return sign(x) * (1.0 - (((((1.061405429 * t - 1.453152027) * t)
        + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * exp(-v * v));
}

float coverage(float2 p, Primitive prim) {
    if (prim.flags_clip & 16u) {
        float2 size = prim.bounds.zw - prim.bounds.xy;
        if (any(p < prim.bounds.xy) || any(p > prim.bounds.zw) || any(size <= 0)) return 0;
        float2 uv0 = float2(prim.uv_min & 65535u, prim.uv_min >> 16u);
        float2 uv1 = float2(prim.uv_max & 65535u, prim.uv_max >> 16u);
        return atlas.SampleLevel(atlas_sampler, lerp(uv0, uv1, (p - prim.bounds.xy) / size) / 65535.0, 0);
    }
    float distance = box_distance(p, prim.bounds, prim.radii);
    if (prim.effects.x > 0) {
        float half_border = prim.effects.x * 0.5;
        distance = abs(distance + half_border) - half_border;
    }
    return saturate(0.5 - distance);
}

bool clipped(float2 p, uint index) {
    [loop]
    for (uint depth = 0; depth < 64 && index != 65535u && index < clip_count; ++depth) {
        Clip clip = clips[index];
        if (box_distance(p, clip.bounds, clip.radius.xxxx) > 0) return true;
        index = clip.parent;
    }
    return false;
}

[numthreads(16, 16, 1)]
void compose(uint3 group : SV_GroupID, uint3 local : SV_GroupThreadID, uint thread : SV_GroupIndex) {
    uint tile_index = group.y * groups_x + group.x;
    if (tile_index >= tile_count) return;
    Tile tile = tiles[tile_index];
    uint2 pixel = uint2(tile.x, tile.y) * 16 + local.xy;
    float2 p = float2(pixel) + 0.5;
    float4 color = 0;
    [loop]
    for (uint start = 0; start < tile.count; start += 64) {
        uint count = min(64u, tile.count - start);
        if (thread < count) tile_indices[thread] = indices[tile.offset + start + thread];
        GroupMemoryBarrierWithGroupSync();
        for (uint i = 0; i < count; ++i) {
            Primitive prim = primitives[tile_indices[i]];
            if (clipped(p, prim.flags_clip >> 16u)) continue;
            if (prim.effects.y > 0) {
                float2 origin = float2(prim.effects.w, prim.shadow_y);
                float spread = prim.effects.z, sigma = prim.effects.y * sqrt(2.0);
                float2 amount = (erf_approx((p - prim.bounds.xy - origin + spread) / sigma) -
                                 erf_approx((p - prim.bounds.zw - origin - spread) / sigma)) * 0.5;
                float alpha = prim.color.a * amount.x * amount.y;
                color = float4(0, 0, 0, alpha) + color * (1.0 - alpha);
            }
            float4 source = prim.color * coverage(p, prim);
            color = source + color * (1.0 - source.a);
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (all(pixel < extent)) output[pixel] = color;
}
