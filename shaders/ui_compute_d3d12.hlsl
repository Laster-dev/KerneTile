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
    uint tile_count, groups_x, clip_count, mica_enabled, render_flags, pad1;
    uint4 mica_grid[4];
};
KT_BINDING(8) Texture2D<float4> color_atlas : register(t5);
#if !defined(KT_VULKAN)
Texture2D<float4> scene_tex : register(t6);
Texture2D<float4> blur_tex : register(t7);
#endif
groupshared uint tile_indices[64];

float4 unpack_rgba8(uint packed) {
    return float4(packed & 255u, (packed >> 8u) & 255u, (packed >> 16u) & 255u, (packed >> 24u) & 255u) / 255.0;
}

uint mica_cell(uint ix, uint iy) {
    uint4 row = mica_grid[iy];
    return (ix == 0u) ? row.x : ((ix == 1u) ? row.y : ((ix == 2u) ? row.z : row.w));
}

float4 sample_mica(float2 p) {
    float2 uv = saturate(p / max(float2(extent), float2(1.0, 1.0))) * 3.0;
    uint2 i0 = min(uint2(uv), uint2(2u, 2u));
    float2 f = uv - float2(i0);
    float4 c00 = unpack_rgba8(mica_cell(i0.x, i0.y));
    float4 c10 = unpack_rgba8(mica_cell(i0.x + 1u, i0.y));
    float4 c01 = unpack_rgba8(mica_cell(i0.x, i0.y + 1u));
    float4 c11 = unpack_rgba8(mica_cell(i0.x + 1u, i0.y + 1u));
    return lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y);
}

// Apple G2 连续曲率超椭圆圆角 SDF (与 CpuTileRasterizer / WGSL 严格一致)
// 常规圆角采用 L4 超椭圆 (1.35x 延伸 + 一阶梯度归一化，零折痕等宽描边)；圆形/胶囊 (r/min_half >= 0.90) 平滑保持正圆
float box_distance(float2 p, float4 bounds, float4 radii) {
    float2 half_size = (bounds.zw - bounds.xy) * 0.5;
    float2 delta = p - (bounds.xy + bounds.zw) * 0.5;
    float raw_r = delta.x > 0 ? (delta.y > 0 ? radii.z : radii.y)
                              : (delta.y > 0 ? radii.w : radii.x);
    float2 ad = abs(delta);
    float min_half = min(half_size.x, half_size.y);
    float r = clamp(raw_r, 0.0, min_half);
    float2 q2 = ad - half_size + r;
    float d_circle = length(max(q2, 0.0)) + min(max(q2.x, q2.y), 0.0) - r;
    float result = d_circle;
    [branch]
    if (r > 0.001) {
        float ratio = r / max(min_half, 1e-4);
        float squircle_t = saturate((0.90 - ratio) * 4.0);
        [branch]
        if (squircle_t > 0.001) {
            float r_eff = min(min_half, r * 1.35);
            float2 q4 = ad - half_size + r_eff;
            float d_squircle;
            if (q4.x > 0.0 && q4.y > 0.0) {
                float2 uv = q4 / r_eff;
                float2 uv2 = uv * uv;
                float2 uv4 = uv2 * uv2;
                float s4 = uv4.x + uv4.y;
                float s2 = sqrt(s4);
                float d4 = sqrt(s2);
                float denom = s2 * d4;
                float grad = (denom > 1e-6) ? (sqrt(uv4.x * uv2.x + uv4.y * uv2.y) / denom) : 1.0;
                d_squircle = (d4 - 1.0) * r_eff / max(grad, 0.5);
            } else {
                d_squircle = max(q4.x, q4.y) - r_eff;
            }
            result = lerp(d_circle, d_squircle, squircle_t);
        }
    }
    return result;
}

// Abramowitz & Stegun 7.1.27 四阶有理近似 (与 CpuTileRasterizer::erf_approx 严格一致，无 exp() 开销，最大误差 ~0.21 LSB)
float2 erf_approx(float2 x) {
    float2 v = abs(x);
    float2 v2 = v * v;
    float2 t = 1.0 / (1.0 + 0.278393 * v + 0.230389 * v2 + 0.000972 * v2 * v + 0.078108 * v2 * v2);
    float2 t2 = t * t;
    return sign(x) * (1.0 - t2 * t2);
}

float3 srgb_to_linear_rgb(float3 c) {
    float3 lin_lo = saturate(c) / 12.92;
    float3 lin_hi = pow(max(saturate(c) + 0.055, 0.0) / 1.055, 2.4);
    return float3(
        c.r <= 0.04045 ? lin_lo.r : lin_hi.r,
        c.g <= 0.04045 ? lin_lo.g : lin_hi.g,
        c.b <= 0.04045 ? lin_lo.b : lin_hi.b
    );
}

float3 linear_to_srgb_rgb(float3 c) {
    float3 s_lo = saturate(c) * 12.92;
    float3 s_hi = 1.055 * pow(max(saturate(c), 0.0), 1.0 / 2.4) - 0.055;
    return saturate(float3(
        c.r <= 0.0031308 ? s_lo.r : s_hi.r,
        c.g <= 0.0031308 ? s_lo.g : s_hi.g,
        c.b <= 0.0031308 ? s_lo.b : s_hi.b
    ));
}

float3 linear_srgb_to_oklab(float3 c) {
    float l = 0.4122214708 * c.r + 0.5363325363 * c.g + 0.0514459929 * c.b;
    float m = 0.2119034982 * c.r + 0.6806995451 * c.g + 0.1073969566 * c.b;
    float s = 0.0883024619 * c.r + 0.2817188376 * c.g + 0.6299787005 * c.b;
    float l_ = pow(max(l, 0.0), 1.0 / 3.0);
    float m_ = pow(max(m, 0.0), 1.0 / 3.0);
    float s_ = pow(max(s, 0.0), 1.0 / 3.0);
    return float3(
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_
    );
}

float3 oklab_to_linear_srgb(float3 lab) {
    float l_ = lab.x + 0.3963377774 * lab.y + 0.2158037573 * lab.z;
    float m_ = lab.x - 0.1055613458 * lab.y - 0.0638541728 * lab.z;
    float s_ = lab.x - 0.0894841775 * lab.y - 1.2914855480 * lab.z;
    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;
    return saturate(float3(
        +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
    ));
}

float4 lerp_oklab_premul(float4 p0, float4 p1, float t) {
    t = saturate(t);
    float a = saturate(lerp(p0.a, p1.a, t));
    float3 s0 = (p0.a > 1e-4) ? saturate(p0.rgb / p0.a) : float3(0.0, 0.0, 0.0);
    float3 s1 = (p1.a > 1e-4) ? saturate(p1.rgb / p1.a) : float3(0.0, 0.0, 0.0);
    float3 eff0 = (p0.a <= 1e-4 && p1.a > 1e-4) ? s1 : s0;
    float3 eff1 = (p1.a <= 1e-4 && p0.a > 1e-4) ? s0 : s1;
    float3 lab0 = linear_srgb_to_oklab(srgb_to_linear_rgb(eff0));
    float3 lab1 = linear_srgb_to_oklab(srgb_to_linear_rgb(eff1));
    float3 mid_srgb = linear_to_srgb_rgb(oklab_to_linear_srgb(lerp(lab0, lab1, t)));
    return float4(mid_srgb * a, a);
}

float eval_font_gamma_cov(float cov, float font_px, float4 fg_pm, float4 bg_pm) {
    float out_cov = cov;
    if (cov > 0.0001 && cov < 0.9999) {
        float size_w = saturate((28.0 - font_px) / 16.0);
        if (size_w > 0.001) {
            float3 fg_rgb = (fg_pm.a > 1e-4) ? saturate(fg_pm.rgb / fg_pm.a) : float3(0.0, 0.0, 0.0);
            float fg_luma = dot(fg_rgb, float3(0.2126, 0.7152, 0.0722));
            float bg_a = saturate(bg_pm.a);
            float bg_luma = (bg_pm.a > 1e-4)
                ? saturate(dot(bg_pm.rgb / bg_pm.a, float3(0.2126, 0.7152, 0.0722)))
                : 0.5;
            float polarity = clamp(fg_luma - bg_luma, -1.0, 1.0) * bg_a;
            float gamma = (polarity >= 0.0)
                ? (1.0 + 0.36 * size_w * polarity)
                : (1.0 / (1.0 + 0.48 * size_w * (-polarity)));
            if (abs(gamma - 1.0) > 0.001) {
                out_cov = saturate(pow(max(cov, 1e-5), gamma));
            }
        }
    }
    return out_cov;
}

float eval_acrylic_grain(float2 p, float scale) {
    float sc = max(scale, 1.0);
    uint gx = uint(max(0, int(floor(p.x / sc))));
    uint gy = uint(max(0, int(floor(p.y / sc))));
    uint h = gx * 747796405u + gy * 2891336453u + 277803737u;
    h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
    h = (h >> 22u) ^ h;
    float u0 = float(h & 65535u) / 65535.0;
    float u1 = float((h >> 16u) & 65535u) / 65535.0;
    return (u0 + u1) - 1.0;
}

float coverage(float2 p, Primitive prim) {
    float out_cov = 0.0;
    [branch]
    if (prim.flags_clip & 16u) {
        float2 size = prim.bounds.zw - prim.bounds.xy;
        if (all(p >= prim.bounds.xy) && all(p <= prim.bounds.zw) && all(size > 0)) {
            float2 uv0 = float2(prim.uv_min & 65535u, prim.uv_min >> 16u);
            float2 uv1 = float2(prim.uv_max & 65535u, prim.uv_max >> 16u);
            out_cov = atlas.SampleLevel(atlas_sampler, lerp(uv0, uv1, (p - prim.bounds.xy) / size) / 65535.0, 0);
        }
    } else {
        float max_r = max(max(prim.radii.x, prim.radii.y), max(prim.radii.z, prim.radii.w));
        float distance;
        [branch]
        if (max_r <= 0.001) {
            // 直边矩形零圆角快路径：免除 Squircle 与开方开销
            float2 d_box = abs(p - (prim.bounds.xy + prim.bounds.zw) * 0.5) - (prim.bounds.zw - prim.bounds.xy) * 0.5;
            distance = max(d_box.x, d_box.y);
        } else {
            distance = box_distance(p, prim.bounds, prim.radii);
        }
        if (prim.effects.x > 0) {
            float half_border = prim.effects.x * 0.5;
            distance = abs(distance + half_border) - half_border;
        }
        out_cov = saturate(0.5 - distance);
    }
    return out_cov;
}

float clip_coverage(float2 p, uint index) {
    float alpha = 1.0;
    [loop]
    for (uint depth = 0; depth < 64 && index != 65535u && index < clip_count; ++depth) {
        Clip clip = clips[index];
        // 矩形裁剪快速拒绝，仅圆角裁剪进入 box_distance
        if (any(p < clip.bounds.xy) || any(p > clip.bounds.zw)) return 0.0;
        [branch]
        if (clip.radius > 0.001) {
            if (box_distance(p, clip.bounds, clip.radius.xxxx) > 0) return 0.0;
        }
        [branch]
        if ((clip.flags & 6u) != 0u) {
            if ((clip.flags & 2u) != 0u) {
                float top_f = float(clip.reserved & 65535u) / 16.0;
                if (top_f > 0.05) {
                    float u = saturate((p.y - clip.bounds.y) / top_f);
                    alpha *= u * u * (3.0 - 2.0 * u);
                }
            }
            if ((clip.flags & 4u) != 0u) {
                float bot_f = float(clip.reserved >> 16u) / 16.0;
                if (bot_f > 0.05) {
                    float u = saturate((clip.bounds.w - p.y) / bot_f);
                    alpha *= u * u * (3.0 - 2.0 * u);
                }
            }
        }
        index = clip.parent;
    }
    return alpha;
}

[numthreads(16, 16, 1)]
void compose(uint3 group : SV_GroupID, uint3 local : SV_GroupThreadID, uint thread : SV_GroupIndex) {
#if !defined(KT_VULKAN)
    uint pass_mode = pad1 >> 28u;
    uint split_id = pad1 & 0x0FFFFFFFu;
    [branch]
    if (pass_mode == 3u || pass_mode == 4u) {
        uint2 gid = group.xy * 16u + local.xy;
        uint dst_w, dst_h;
        output.GetDimensions(dst_w, dst_h);
        if (gid.x < dst_w && gid.y < dst_h) {
            uint sw, sh;
            blur_tex.GetDimensions(sw, sh);
            float2 src_dim = max(float2(sw, sh), float2(1.0, 1.0));
            float2 dst_dim = max(float2(dst_w, dst_h), float2(1.0, 1.0));
            float2 active_uv = (split_id != 0u) ? (float2(extent) / src_dim) : float2(1.0, 1.0);
            float2 uv_max = max(active_uv - 0.5 / src_dim, float2(0.0, 0.0));
            float2 uv = ((float2(gid) + 0.5) / dst_dim) * active_uv;
            float2 dir = (pass_mode == 3u) ? float2(1.0, 0.0) : float2(0.0, 1.0);
            float2 texel = dir * (1.9 / dst_dim) * active_uv;
            static const float w[7] = { 0.13625, 0.12889, 0.10932, 0.08316, 0.05674, 0.03472, 0.01904 };
            float4 acc = blur_tex.SampleLevel(atlas_sampler, clamp(uv, float2(0.0, 0.0), uv_max), 0) * w[0];
            [unroll]
            for (int k = 1; k <= 6; ++k) {
                float2 off = texel * float(k);
                acc += (blur_tex.SampleLevel(atlas_sampler, clamp(uv + off, float2(0.0, 0.0), uv_max), 0)
                      + blur_tex.SampleLevel(atlas_sampler, clamp(uv - off, float2(0.0, 0.0), uv_max), 0)) * w[k];
            }
            output[gid] = acc;
        }
        return;
    }
#endif
    uint tile_index = group.y * groups_x + group.x;
    if (tile_index >= tile_count) return;
    Tile tile = tiles[tile_index];
    uint2 pixel = uint2(tile.x, tile.y) * 16 + local.xy;
    float2 p = float2(pixel) + 0.5;
    float4 color = float4(0, 0, 0, 0);
#if !defined(KT_VULKAN)
    if (pass_mode == 2u) {
        uint2 cpx = min(pixel, max(extent, uint2(1u, 1u)) - uint2(1u, 1u));
        color = scene_tex.Load(int3(cpx, 0));
    } else
#endif
    if (mica_enabled != 0u) {
        color = sample_mica(p);
        if ((render_flags & 4u) != 0u) {
            float g_off = eval_acrylic_grain(p, 1.0) * 0.018 * color.a;
            color.rgb = clamp(color.rgb + g_off, 0.0, color.a);
        }
    }
    [loop]
    for (uint start = 0; start < tile.count; start += 64) {
        uint count = min(64u, tile.count - start);
        if (thread < count) tile_indices[thread] = indices[tile.offset + start + thread];
        GroupMemoryBarrierWithGroupSync();
        for (uint i = 0; i < count; ++i) {
            uint prim_id = tile_indices[i];
#if !defined(KT_VULKAN)
            if (pass_mode == 1u && prim_id >= split_id) continue;
            if (pass_mode == 2u && prim_id < split_id) continue;
#endif
            Primitive prim = primitives[prim_id];
            float clip_a = clip_coverage(p, prim.flags_clip >> 16u);
            if (clip_a <= 0.0001) continue;
            [branch]
            if ((prim.flags_clip & 0x7672u) == 0u && prim.effects.y > 0) {
                float2 origin = float2(prim.effects.w, prim.shadow_y);
                float spread = prim.effects.z, sigma = prim.effects.y * sqrt(2.0);
                float2 amount = (erf_approx((p - prim.bounds.xy - origin + spread) / sigma) -
                                 erf_approx((p - prim.bounds.zw - origin - spread) / sigma)) * 0.5;
                float alpha = prim.color.a * amount.x * amount.y * clip_a;
                color = float4(0, 0, 0, alpha) + color * (1.0 - alpha);
            }
            float4 source;
            [branch]
            if ((prim.flags_clip & 0x7662u) == 0u) {
                // 最热快路径 (占 UI 图元 >95%): 普通矩形/圆角矩形/描边/字形
                float cov = coverage(p, prim);
                [branch]
                if ((render_flags & 2u) != 0u && (prim.flags_clip & 0x0110u) == 0x0110u) {
                    float font_px = (prim.effects.x > 0.5) ? prim.effects.x : 13.0;
                    cov = eval_font_gamma_cov(cov, font_px, prim.color, color);
                }
                source = prim.color * cov;
            } else [branch] if (prim.flags_clip & 32u) {
                float2 size = prim.bounds.zw - prim.bounds.xy;
                float cov = saturate(0.5 - box_distance(p, prim.bounds, prim.radii));
                if (cov > 0.0 && all(size > 0)) {
                    float2 uv0 = float2(prim.uv_min & 65535u, prim.uv_min >> 16u);
                    float2 uv1 = float2(prim.uv_max & 65535u, prim.uv_max >> 16u);
                    float4 tex = color_atlas.SampleLevel(atlas_sampler, lerp(uv0, uv1, (p - prim.bounds.xy) / size) / 65535.0, 0);
                    float4 tex_pm = float4(tex.rgb * tex.a, tex.a);
                    source = tex_pm * prim.color * cov;
                } else {
                    source = float4(0, 0, 0, 0);
                }
            } else if (prim.flags_clip & 512u) {
                source = float4(0, 0, 0, 0);
                float reveal_radius = prim.effects.y;
                if (reveal_radius > 0.001 && all(p >= prim.bounds.xy) && all(p <= prim.bounds.zw)) {
                    float dist = length(p - float2(prim.effects.w, prim.shadow_y));
                    if (dist < reveal_radius) {
                        float u = saturate(1.0 - dist / reveal_radius);
                        float radial = u * u;
                        float4 eval_bounds = prim.bounds;
                        float4 eval_radii = prim.radii;
                        uint clip_idx = prim.flags_clip >> 16u;
                        if (clip_idx != 65535u && clip_idx < clip_count) {
                            Clip c = clips[clip_idx];
                            if ((c.flags & 1u) != 0u) {
                                eval_bounds = c.bounds;
                                eval_radii = c.radius.xxxx;
                            }
                        }
                        float d = box_distance(p, eval_bounds, eval_radii);
                        if (prim.effects.x > 0.0 || (prim.flags_clip & 4u) != 0u) {
                            float bw = (prim.effects.x > 0.0) ? prim.effects.x : 1.0;
                            float half_border = bw * 0.5;
                            d = abs(d + half_border) - half_border;
                        }
                        float geom_cov = saturate(0.5 - d);
                        float intensity = (prim.effects.z > 0.0) ? prim.effects.z : 1.0;
                        float cov = saturate(geom_cov * radial * intensity);
                        source = prim.color * cov;
                    }
                }
            } else if ((prim.flags_clip & 1024u) != 0u && (prim.flags_clip & 16u) == 0u) {
                source = float4(0, 0, 0, 0);
                float rim_w = max(prim.effects.x, 1.0);
                float half_rim = rim_w * 0.5;
                float fresnel_glow = max(prim.effects.z, 0.0);
                float d = box_distance(p, prim.bounds, prim.radii);
                float d_rim = abs(d + half_rim) - half_rim;
                float stroke_cov = saturate(0.5 - d_rim);
                float glow_cov = (d <= 0.0 && fresnel_glow > 0.001) ? (exp(d * 0.55) * fresnel_glow * 0.35) : 0.0;
                if (stroke_cov > 0.0001 || glow_cov > 0.0001) {
                    float2 ldir = float2(prim.effects.w, prim.shadow_y);
                    float l_len = length(ldir);
                    ldir = (l_len > 1e-4) ? (ldir / l_len) : float2(0.0, -1.0);
                    float nx = box_distance(p + float2(0.5, 0.0), prim.bounds, prim.radii) -
                               box_distance(p - float2(0.5, 0.0), prim.bounds, prim.radii);
                    float ny = box_distance(p + float2(0.0, 0.5), prim.bounds, prim.radii) -
                               box_distance(p - float2(0.0, 0.5), prim.bounds, prim.radii);
                    float n_len = length(float2(nx, ny));
                    float2 nrm = (n_len > 1e-5) ? (float2(nx, ny) / n_len) : float2(0.0, -1.0);
                    float ndotl = clamp(dot(nrm, ldir), -1.0, 1.0);
                    float top_factor = pow(saturate(ndotl * 0.5 + 0.5), 1.8);
                    float grazing = 1.0 - abs(ndotl);
                    float top_w = saturate(top_factor + 0.22 * grazing * saturate(ndotl + 0.35));
                    float bot_w = pow(saturate(-ndotl), 1.4);
                    float4 bot_col = float4(
                        float(prim.uv_min & 65535u),
                        float(prim.uv_min >> 16u),
                        float(prim.uv_max & 65535u),
                        float(prim.uv_max >> 16u)
                    ) / 65535.0;
                    float4 rim_col = prim.color * top_w + bot_col * bot_w;
                    rim_col.a = saturate(rim_col.a);
                    rim_col.rgb = clamp(rim_col.rgb, 0.0, rim_col.a);
                    float4 stroke_src = rim_col * stroke_cov;
                    float4 glow_src = prim.color * (saturate(ndotl) * glow_cov);
                    glow_src.a = saturate(glow_src.a);
                    glow_src.rgb = clamp(glow_src.rgb, 0.0, glow_src.a);
                    source = stroke_src + glow_src * (1.0 - stroke_src.a);
                    source.a = saturate(source.a);
                    source.rgb = clamp(source.rgb, 0.0, source.a);
                }
            } else if ((prim.flags_clip & 8192u) != 0u && (prim.flags_clip & 16u) == 0u) {
                source = float4(0, 0, 0, 0);
                float cov_outer = saturate(0.5 - box_distance(p, prim.bounds, prim.radii));
                if (cov_outer > 0.0001) {
                    float2 p_s = p - float2(prim.effects.w, prim.shadow_y);
                    float d_in = box_distance(p_s, prim.bounds, prim.radii) + prim.effects.z;
                    float inv_s = 1.0 / (max(prim.effects.y, 0.25) * 1.41421356);
                    float erfv = erf_approx(float2(d_in * inv_s, 0.0)).x;
                    float inset_a = saturate(0.5 * (1.0 + erfv)) * cov_outer;
                    source = prim.color * inset_a;
                }
            } else if ((prim.flags_clip & 16384u) != 0u && (prim.flags_clip & 16u) == 0u) {
                source = float4(0, 0, 0, 0);
                float4 a_bounds = prim.radii;
                float4 b_norm = float4(
                    float(prim.uv_min & 65535u),
                    float(prim.uv_min >> 16u),
                    float(prim.uv_max & 65535u),
                    float(prim.uv_max >> 16u)
                ) / 65535.0;
                float4 b_bounds = float4(
                    lerp(prim.bounds.x, prim.bounds.z, b_norm.x),
                    lerp(prim.bounds.y, prim.bounds.w, b_norm.y),
                    lerp(prim.bounds.x, prim.bounds.z, b_norm.z),
                    lerp(prim.bounds.y, prim.bounds.w, b_norm.w)
                );
                float ra = max(prim.effects.z, 0.0);
                float rb = max(prim.effects.w, 0.0);
                float dA = box_distance(p, a_bounds, ra.xxxx);
                float dB = box_distance(p, b_bounds, rb.xxxx);
                float k = max(prim.effects.y, 0.0);
                float h = (k > 0.001) ? saturate(0.5 + 0.5 * (dB - dA) / k) : ((dA <= dB) ? 1.0 : 0.0);
                float d_fused = lerp(dB, dA, h) - k * h * (1.0 - h);
                if (prim.effects.x > 0.0) {
                    float hb = prim.effects.x * 0.5;
                    d_fused = abs(d_fused + hb) - hb;
                }
                float cov = saturate(0.5 - d_fused);
                if (cov > 0.0001) {
                    float4 base_col = prim.color;
                    if (prim.shadow_y > 0.5) {
                        uint pk = uint(round(prim.shadow_y)) - 1u;
                        float ab = float((pk >> 18u) & 63u) / 63.0;
                        float3 srgb_b = float3(float(pk & 63u), float((pk >> 6u) & 63u), float((pk >> 12u) & 63u)) / 63.0;
                        float4 col_b = float4(srgb_b * ab, ab);
                        base_col = lerp_oklab_premul(col_b, prim.color, h);
                    }
                    source = base_col * cov;
                }
            } else [branch] if ((prim.flags_clip & 64u) != 0u && (prim.flags_clip & 16u) == 0u) {
                float cov = coverage(p, prim);
#if !defined(KT_VULKAN)
                if (pass_mode == 2u) {
                    float2 uv = saturate(p / max(float2(extent), float2(1.0, 1.0)));
                    float4 blurred = blur_tex.SampleLevel(atlas_sampler, uv, 0);
                    float3 tint_rgb = prim.color.rgb / max(prim.color.a, 0.001);
                    float tint_luma = dot(tint_rgb, float3(0.299, 0.587, 0.114));
                    float bg_luma = dot(blurred.rgb, float3(0.299, 0.587, 0.114));
                    float3 lum_bg = saturate(blurred.rgb + (tint_luma - bg_luma) * 0.55);
                    float3 frosted_rgb = prim.color.rgb + lum_bg * (1.0 - prim.color.a);
                    float grain = eval_acrylic_grain(p, 1.0) * 0.015;
                    frosted_rgb = saturate(frosted_rgb + grain);
                    float4 frosted = float4(frosted_rgb, prim.color.a + blurred.a * (1.0 - prim.color.a));
                    float ca = cov * clip_a;
                    color = frosted * ca + color * (1.0 - ca);
                    continue;
                }
#endif
                source = prim.color * cov;
            } else [branch] if ((prim.flags_clip & 4096u) != 0u && (prim.flags_clip & 16u) == 0u) {
                float cov = coverage(p, prim);
                float grain_scale = max(prim.effects.y, 1.0);
                float grain_amp = clamp(prim.effects.z, 0.0, 0.25);
                float g_off = eval_acrylic_grain(p, grain_scale) * grain_amp * prim.color.a;
                float4 base_col = prim.color;
                base_col.rgb = clamp(base_col.rgb + g_off, 0.0, base_col.a);
                source = base_col * cov;
            } else [branch] if ((prim.flags_clip & 2u) != 0u && (prim.flags_clip & 16u) == 0u) {
                float2 g0 = float2(prim.effects.w, prim.shadow_y);
                float2 g1 = float2(prim.effects.y, prim.effects.z);
                float2 gdir = g1 - g0;
                float len2 = dot(gdir, gdir);
                if (len2 <= 1e-6) {
                    g0 = prim.bounds.xy;
                    gdir = float2(0.0, prim.bounds.w - prim.bounds.y);
                    len2 = dot(gdir, gdir);
                }
                float t = (len2 > 1e-6) ? saturate(dot(p - g0, gdir) / len2) : 0.0;
                float4 c1 = float4(
                    float(prim.uv_min & 65535u),
                    float(prim.uv_min >> 16u),
                    float(prim.uv_max & 65535u),
                    float(prim.uv_max >> 16u)
                ) / 65535.0;
                float4 base_color = (prim.flags_clip & 2048u) != 0u
                    ? lerp_oklab_premul(prim.color, c1, t)
                    : lerp(prim.color, c1, t);
                source = base_color * coverage(p, prim);
            } else {
                float cov = coverage(p, prim);
                if ((render_flags & 2u) != 0u && (prim.flags_clip & 0x0110u) == 0x0110u) {
                    float font_px = (prim.effects.x > 0.5) ? prim.effects.x : 13.0;
                    cov = eval_font_gamma_cov(cov, font_px, prim.color, color);
                }
                source = prim.color * cov;
            }
            source *= clip_a;
            color = source + color * (1.0 - source.a);
        }
        GroupMemoryBarrierWithGroupSync();
    }
#if !defined(KT_VULKAN)
    if (pass_mode != 1u && (render_flags & 1u) != 0u && color.a > 0.0) {
#else
    if ((render_flags & 1u) != 0u && color.a > 0.0) {
#endif
        float ign = frac(52.9829189 * frac(dot(float2(pixel), float2(0.06711056, 0.00583715))));
        color.rgb = saturate(color.rgb + (ign - 0.5) / 255.0);
    }
    #if defined(KT_VULKAN)
    color = round(saturate(color) * 255.0) / 255.0;
    #endif
    if (all(pixel < extent)) output[pixel] = color;
}
