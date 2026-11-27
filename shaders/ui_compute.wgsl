struct Primitive {
    bounds: vec4f,
    radii: vec4f,
    color: vec4f,
    effects: vec4f,
    uv_min: u32,
    uv_max: u32,
    flags_clip: u32,
    shadow_y: f32,
}
struct Clip {
    bounds: vec4f,
    radius: f32,
    parent: u32,
    flags: u32,
    reserved: u32,
}
struct Tile {
    x: u32,
    y: u32,
    offset: u32,
    count: u32,
}
struct Frame {
    extent: vec2u,
    tile_count: u32,
    groups_x: u32,
    clip_count: u32,
    mica_enabled: u32,
    render_flags: u32,
    pad1: u32,
    mica_grid: array<vec4u, 4>,
}
@group(0) @binding(0) var<storage, read> primitives: array<Primitive>;
@group(0) @binding(1) var<storage, read> indices: array<u32>;
@group(0) @binding(2) var<storage, read> tiles: array<Tile>;
@group(0) @binding(3) var<storage, read> clips: array<Clip>;
@group(0) @binding(4) var atlas: texture_2d<f32>;
@group(0) @binding(5) var atlas_sampler: sampler;
@group(0) @binding(6) var output: texture_storage_2d<rgba8unorm, write>;
@group(0) @binding(7) var<uniform> frame: Frame;
@group(0) @binding(8) var color_atlas: texture_2d<f32>;
var<workgroup> tile_indices: array<u32, 64>;
var<workgroup> tile_count_shared: u32;

// Apple G2 连续曲率超椭圆圆角 SDF (与 D3D12/Vulkan HLSL 及 CPU 光栅器严格一致)
fn box_distance(p: vec2f, bounds: vec4f, radii: vec4f) -> f32 {
    let half_size = (bounds.zw - bounds.xy) * 0.5;
    let delta = p - (bounds.xy + bounds.zw) * 0.5;
    let raw_r = select(select(radii.x, radii.w, delta.y > 0.0),
                       select(radii.y, radii.z, delta.y > 0.0), delta.x > 0.0);
    let ad = abs(delta);
    let min_half = min(half_size.x, half_size.y);
    let r = clamp(raw_r, 0.0, min_half);
    let q2 = ad - half_size + r;
    let d_circle = length(max(q2, vec2f(0.0))) + min(max(q2.x, q2.y), 0.0) - r;
    if (r <= 0.001) {
        return d_circle;
    }

    let ratio = r / max(min_half, 1e-4);
    let squircle_t = clamp((0.90 - ratio) * 4.0, 0.0, 1.0);
    if (squircle_t <= 0.001) {
        return d_circle;
    }

    let r_eff = min(min_half, r * 1.35);
    let q4 = ad - half_size + r_eff;
    var d_squircle: f32;
    if (q4.x > 0.0 && q4.y > 0.0) {
        let uv = q4 / r_eff;
        let uv2 = uv * uv;
        let uv4 = uv2 * uv2;
        let s4 = uv4.x + uv4.y;
        let s2 = sqrt(s4);
        let d4 = sqrt(s2);
        let denom = s2 * d4;
        let grad = select(1.0, sqrt(uv4.x * uv2.x + uv4.y * uv2.y) / denom, denom > 1e-6);
        d_squircle = (d4 - 1.0) * r_eff / max(grad, 0.5);
    } else {
        d_squircle = max(q4.x, q4.y) - r_eff;
    }
    return mix(d_circle, d_squircle, squircle_t);
}

fn clip_coverage(p: vec2f, first_clip: u32) -> f32 {
    var alpha = 1.0;
    var current = first_clip;
    for (var depth = 0u; depth < 64u && current != 65535u && current < arrayLength(&clips); depth++) {
        let clip = clips[current];
        if (box_distance(p, clip.bounds, vec4f(clip.radius)) > 0.0) {
            return 0.0;
        }
        if ((clip.flags & 6u) != 0u) {
            if ((clip.flags & 2u) != 0u) {
                let top_f = f32(clip.reserved & 65535u) / 16.0;
                if (top_f > 0.05) {
                    let u = clamp((p.y - clip.bounds.y) / top_f, 0.0, 1.0);
                    alpha *= u * u * (3.0 - 2.0 * u);
                }
            }
            if ((clip.flags & 4u) != 0u) {
                let bot_f = f32(clip.reserved >> 16u) / 16.0;
                if (bot_f > 0.05) {
                    let u = clamp((clip.bounds.w - p.y) / bot_f, 0.0, 1.0);
                    alpha *= u * u * (3.0 - 2.0 * u);
                }
            }
        }
        current = clip.parent;
    }
    return alpha;
}

// Abramowitz & Stegun 7.1.27 四阶有理近似 (与 HLSL / CpuTileRasterizer::erf_approx 严格一致)
fn erf_approx(x: vec2f) -> vec2f {
    let v = abs(x);
    let v2 = v * v;
    let t = 1.0 / (1.0 + 0.278393 * v + 0.230389 * v2 + 0.000972 * v2 * v + 0.078108 * v2 * v2);
    let t2 = t * t;
    return sign(x) * (1.0 - t2 * t2);
}

fn srgb_to_linear_rgb(c: vec3f) -> vec3f {
    let sc = clamp(c, vec3f(0.0), vec3f(1.0));
    return select(pow((sc + 0.055) / 1.055, vec3f(2.4)), sc / 12.92, sc <= vec3f(0.04045));
}

fn linear_to_srgb_rgb(c: vec3f) -> vec3f {
    let lc = clamp(c, vec3f(0.0), vec3f(1.0));
    return clamp(select(1.055 * pow(lc, vec3f(1.0 / 2.4)) - 0.055, lc * 12.92, lc <= vec3f(0.0031308)), vec3f(0.0), vec3f(1.0));
}

fn linear_srgb_to_oklab(c: vec3f) -> vec3f {
    let l = 0.4122214708 * c.r + 0.5363325363 * c.g + 0.0514459929 * c.b;
    let m = 0.2119034982 * c.r + 0.6806995451 * c.g + 0.1073969566 * c.b;
    let s = 0.0883024619 * c.r + 0.2817188376 * c.g + 0.6299787005 * c.b;
    let l_ = pow(max(l, 0.0), 1.0 / 3.0);
    let m_ = pow(max(m, 0.0), 1.0 / 3.0);
    let s_ = pow(max(s, 0.0), 1.0 / 3.0);
    return vec3f(
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_
    );
}

fn oklab_to_linear_srgb(lab: vec3f) -> vec3f {
    let l_ = lab.x + 0.3963377774 * lab.y + 0.2158037573 * lab.z;
    let m_ = lab.x - 0.1055613458 * lab.y - 0.0638541728 * lab.z;
    let s_ = lab.x - 0.0894841775 * lab.y - 1.2914855480 * lab.z;
    let l = l_ * l_ * l_;
    let m = m_ * m_ * m_;
    let s = s_ * s_ * s_;
    return clamp(vec3f(
        4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
    ), vec3f(0.0), vec3f(1.0));
}

fn lerp_oklab_premul(p0: vec4f, p1: vec4f, t: f32) -> vec4f {
    let tc = clamp(t, 0.0, 1.0);
    let a = clamp(mix(p0.a, p1.a, tc), 0.0, 1.0);
    let s0 = select(vec3f(0.0), clamp(p0.rgb / p0.a, vec3f(0.0), vec3f(1.0)), p0.a > 1e-4);
    let s1 = select(vec3f(0.0), clamp(p1.rgb / p1.a, vec3f(0.0), vec3f(1.0)), p1.a > 1e-4);
    let eff0 = select(s0, s1, p0.a <= 1e-4 && p1.a > 1e-4);
    let eff1 = select(s1, s0, p1.a <= 1e-4 && p0.a > 1e-4);
    let lab0 = linear_srgb_to_oklab(srgb_to_linear_rgb(eff0));
    let lab1 = linear_srgb_to_oklab(srgb_to_linear_rgb(eff1));
    let mid_srgb = linear_to_srgb_rgb(oklab_to_linear_srgb(mix(lab0, lab1, tc)));
    return vec4f(mid_srgb * a, a);
}

fn eval_font_gamma_cov(cov: f32, font_px: f32, fg_pm: vec4f, bg_pm: vec4f) -> f32 {
    if (cov <= 0.0001 || cov >= 0.9999) { return cov; }
    let size_w = clamp((28.0 - font_px) / 16.0, 0.0, 1.0);
    if (size_w <= 0.001) { return cov; }
    let fg_rgb = select(vec3f(0.0), clamp(fg_pm.rgb / fg_pm.a, vec3f(0.0), vec3f(1.0)), fg_pm.a > 1e-4);
    let luma_w = vec3f(0.2126, 0.7152, 0.0722);
    let fg_luma = dot(fg_rgb, luma_w);
    let bg_a = clamp(bg_pm.a, 0.0, 1.0);
    let bg_luma = select(0.5, clamp(dot(bg_pm.rgb / bg_pm.a, luma_w), 0.0, 1.0), bg_pm.a > 1e-4);
    let polarity = clamp(fg_luma - bg_luma, -1.0, 1.0) * bg_a;
    let gamma = select(1.0 / (1.0 + 0.48 * size_w * (-polarity)), 1.0 + 0.36 * size_w * polarity, polarity >= 0.0);
    if (abs(gamma - 1.0) <= 0.001) { return cov; }
    return clamp(pow(max(cov, 1e-5), gamma), 0.0, 1.0);
}

fn eval_acrylic_grain(p: vec2f, scale: f32) -> f32 {
    let sc = max(scale, 1.0);
    let gx = u32(max(0, i32(floor(p.x / sc))));
    let gy = u32(max(0, i32(floor(p.y / sc))));
    var h = gx * 747796405u + gy * 2891336453u + 277803737u;
    h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
    h = (h >> 22u) ^ h;
    let u0 = f32(h & 65535u) / 65535.0;
    let u1 = f32((h >> 16u) & 65535u) / 65535.0;
    return (u0 + u1) - 1.0;
}

fn coverage(p: vec2f, prim: Primitive) -> f32 {
    if ((prim.flags_clip & 16u) != 0u) {
        let extent = prim.bounds.zw - prim.bounds.xy;
        if (any(p < prim.bounds.xy) || any(p > prim.bounds.zw) || any(extent <= vec2f(0.0))) {
            return 0.0;
        }
        let uv0 = vec2f(f32(prim.uv_min & 65535u), f32(prim.uv_min >> 16u));
        let uv1 = vec2f(f32(prim.uv_max & 65535u), f32(prim.uv_max >> 16u));
        let uv = mix(uv0, uv1, (p - prim.bounds.xy) / extent) / 65535.0;
        return textureSampleLevel(atlas, atlas_sampler, uv, 0.0).r;
    }
    let max_r = max(max(prim.radii.x, prim.radii.y), max(prim.radii.z, prim.radii.w));
    var distance: f32;
    if (max_r <= 0.001) {
        let d_box = abs(p - (prim.bounds.xy + prim.bounds.zw) * 0.5) - (prim.bounds.zw - prim.bounds.xy) * 0.5;
        distance = max(d_box.x, d_box.y);
    } else {
        distance = box_distance(p, prim.bounds, prim.radii);
    }
    if (prim.effects.x > 0.0) {
        let half_border = prim.effects.x * 0.5;
        distance = abs(distance + half_border) - half_border;
    }
    return clamp(0.5 - distance, 0.0, 1.0);
}

fn unpack_rgba8(packed: u32) -> vec4f {
    return vec4f(
        f32(packed & 255u),
        f32((packed >> 8u) & 255u),
        f32((packed >> 16u) & 255u),
        f32((packed >> 24u) & 255u)
    ) / 255.0;
}

fn mica_cell(ix: u32, iy: u32) -> u32 {
    let row = frame.mica_grid[iy];
    if (ix == 0u) { return row.x; }
    if (ix == 1u) { return row.y; }
    if (ix == 2u) { return row.z; }
    return row.w;
}

fn sample_mica(p: vec2f) -> vec4f {
    let uv = clamp(p / max(vec2f(frame.extent), vec2f(1.0)), vec2f(0.0), vec2f(1.0)) * 3.0;
    let i0 = min(vec2u(uv), vec2u(2u, 2u));
    let f = uv - vec2f(i0);
    let c00 = unpack_rgba8(mica_cell(i0.x, i0.y));
    let c10 = unpack_rgba8(mica_cell(i0.x + 1u, i0.y));
    let c01 = unpack_rgba8(mica_cell(i0.x, i0.y + 1u));
    let c11 = unpack_rgba8(mica_cell(i0.x + 1u, i0.y + 1u));
    return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
}

@compute @workgroup_size(16, 16)
fn compose(@builtin(workgroup_id) group: vec3u,
           @builtin(local_invocation_id) local: vec3u,
           @builtin(local_invocation_index) thread: u32) {
    let tile_index = group.y * frame.groups_x + group.x;
    if (tile_index >= frame.tile_count) {
        return;
    }
    let tile = tiles[tile_index];
    if (thread == 0u) { tile_count_shared = tile.count; }
    let primitive_count = workgroupUniformLoad(&tile_count_shared);
    let pixel = vec2u(tile.x, tile.y) * 16u + local.xy;
    let p = vec2f(pixel) + 0.5;
    var color = select(vec4f(0.0), sample_mica(p), frame.mica_enabled != 0u);
    if (frame.mica_enabled != 0u && (frame.render_flags & 4u) != 0u) {
        let g_off = eval_acrylic_grain(p, 1.0) * 0.018 * color.a;
        color = vec4f(clamp(color.rgb + vec3f(g_off), vec3f(0.0), vec3f(color.a)), color.a);
    }

    for (var start = 0u; start < primitive_count; start += 64u) {
        let count = min(64u, primitive_count - start);
        if (thread < count) {
            tile_indices[thread] = indices[tile.offset + start + thread];
        }
        workgroupBarrier();
        for (var i = 0u; i < count; i++) {
            let prim = primitives[tile_indices[i]];
            let clip_a = clip_coverage(p, prim.flags_clip >> 16u);
            if (clip_a <= 0.0001) {
                continue;
            }
            if ((prim.flags_clip & 0x7632u) == 0u && prim.effects.y > 0.0) {
                let origin = vec2f(prim.effects.w, prim.shadow_y);
                let spread = prim.effects.z;
                let sigma = prim.effects.y * sqrt(2.0);
                let amount = (erf_approx((p - prim.bounds.xy - origin + spread) / sigma)
                    - erf_approx((p - prim.bounds.zw - origin - spread) / sigma)) * 0.5;
                let alpha = prim.color.a * amount.x * amount.y * clip_a;
                color = vec4f(0.0, 0.0, 0.0, alpha) + color * (1.0 - alpha);
            }
            var source: vec4f;
            if ((prim.flags_clip & 32u) != 0u) {
                let extent = prim.bounds.zw - prim.bounds.xy;
                let cov = clamp(0.5 - box_distance(p, prim.bounds, prim.radii), 0.0, 1.0);
                if (cov > 0.0 && all(extent > vec2f(0.0))) {
                    let uv0 = vec2f(f32(prim.uv_min & 65535u), f32(prim.uv_min >> 16u));
                    let uv1 = vec2f(f32(prim.uv_max & 65535u), f32(prim.uv_max >> 16u));
                    let uv = mix(uv0, uv1, (p - prim.bounds.xy) / extent) / 65535.0;
                    let tex = textureSampleLevel(color_atlas, atlas_sampler, uv, 0.0);
                    let tex_pm = vec4f(tex.rgb * tex.a, tex.a);
                    source = tex_pm * prim.color * cov;
                } else {
                    source = vec4f(0.0);
                }
            } else if ((prim.flags_clip & 512u) != 0u) {
                source = vec4f(0.0);
                let reveal_radius = prim.effects.y;
                if (reveal_radius > 0.001 && all(p >= prim.bounds.xy) && all(p <= prim.bounds.zw)) {
                    let dist = length(p - vec2f(prim.effects.w, prim.shadow_y));
                    if (dist < reveal_radius) {
                        let u = clamp(1.0 - dist / reveal_radius, 0.0, 1.0);
                        let radial = u * u;
                        var eval_bounds = prim.bounds;
                        var eval_radii = prim.radii;
                        let clip_idx = prim.flags_clip >> 16u;
                        if (clip_idx != 65535u && clip_idx < arrayLength(&clips)) {
                            let c = clips[clip_idx];
                            if ((c.flags & 1u) != 0u) {
                                eval_bounds = c.bounds;
                                eval_radii = vec4f(c.radius);
                            }
                        }
                        var d = box_distance(p, eval_bounds, eval_radii);
                        if (prim.effects.x > 0.0 || (prim.flags_clip & 4u) != 0u) {
                            let bw = select(1.0, prim.effects.x, prim.effects.x > 0.0);
                            let half_border = bw * 0.5;
                            d = abs(d + half_border) - half_border;
                        }
                        let geom_cov = clamp(0.5 - d, 0.0, 1.0);
                        let intensity = select(1.0, prim.effects.z, prim.effects.z > 0.0);
                        let cov = clamp(geom_cov * radial * intensity, 0.0, 1.0);
                        source = prim.color * cov;
                    }
                }
            } else if ((prim.flags_clip & 1024u) != 0u && (prim.flags_clip & 16u) == 0u) {
                source = vec4f(0.0);
                let rim_w = max(prim.effects.x, 1.0);
                let half_rim = rim_w * 0.5;
                let fresnel_glow = max(prim.effects.z, 0.0);
                let d = box_distance(p, prim.bounds, prim.radii);
                let d_rim = abs(d + half_rim) - half_rim;
                let stroke_cov = clamp(0.5 - d_rim, 0.0, 1.0);
                let glow_cov = select(0.0, exp(d * 0.55) * fresnel_glow * 0.35, d <= 0.0 && fresnel_glow > 0.001);
                if (stroke_cov > 0.0001 || glow_cov > 0.0001) {
                    let raw_ldir = vec2f(prim.effects.w, prim.shadow_y);
                    let l_len = length(raw_ldir);
                    let ldir = select(vec2f(0.0, -1.0), raw_ldir / l_len, l_len > 1e-4);
                    let nx = box_distance(p + vec2f(0.5, 0.0), prim.bounds, prim.radii) -
                             box_distance(p - vec2f(0.5, 0.0), prim.bounds, prim.radii);
                    let ny = box_distance(p + vec2f(0.0, 0.5), prim.bounds, prim.radii) -
                             box_distance(p - vec2f(0.0, 0.5), prim.bounds, prim.radii);
                    let n_vec = vec2f(nx, ny);
                    let n_len = length(n_vec);
                    let nrm = select(vec2f(0.0, -1.0), n_vec / n_len, n_len > 1e-5);
                    let ndotl = clamp(dot(nrm, ldir), -1.0, 1.0);
                    let top_factor = pow(clamp(ndotl * 0.5 + 0.5, 0.0, 1.0), 1.8);
                    let grazing = 1.0 - abs(ndotl);
                    let top_w = clamp(top_factor + 0.22 * grazing * clamp(ndotl + 0.35, 0.0, 1.0), 0.0, 1.0);
                    let bot_w = pow(clamp(-ndotl, 0.0, 1.0), 1.4);
                    let bot_col = vec4f(
                        f32(prim.uv_min & 65535u),
                        f32(prim.uv_min >> 16u),
                        f32(prim.uv_max & 65535u),
                        f32(prim.uv_max >> 16u)
                    ) / 65535.0;
                    let rim_raw = prim.color * top_w + bot_col * bot_w;
                    let rim_a = clamp(rim_raw.a, 0.0, 1.0);
                    let rim_col = vec4f(clamp(rim_raw.rgb, vec3f(0.0), vec3f(rim_a)), rim_a);
                    let stroke_src = rim_col * stroke_cov;
                    let glow_raw = prim.color * (clamp(ndotl, 0.0, 1.0) * glow_cov);
                    let glow_a = clamp(glow_raw.a, 0.0, 1.0);
                    let glow_src = vec4f(clamp(glow_raw.rgb, vec3f(0.0), vec3f(glow_a)), glow_a);
                    let combined = stroke_src + glow_src * (1.0 - stroke_src.a);
                    let src_a = clamp(combined.a, 0.0, 1.0);
                    source = vec4f(clamp(combined.rgb, vec3f(0.0), vec3f(src_a)), src_a);
                }
            } else if ((prim.flags_clip & 8192u) != 0u && (prim.flags_clip & 16u) == 0u) {
                source = vec4f(0.0);
                let cov_outer = clamp(0.5 - box_distance(p, prim.bounds, prim.radii), 0.0, 1.0);
                if (cov_outer > 0.0001) {
                    let p_s = p - vec2f(prim.effects.w, prim.shadow_y);
                    let d_in = box_distance(p_s, prim.bounds, prim.radii) + prim.effects.z;
                    let inv_s = 1.0 / (max(prim.effects.y, 0.25) * 1.41421356);
                    let erfv = erf_approx(vec2f(d_in * inv_s, 0.0)).x;
                    let inset_a = clamp(0.5 * (1.0 + erfv), 0.0, 1.0) * cov_outer;
                    source = prim.color * inset_a;
                }
            } else if ((prim.flags_clip & 16384u) != 0u && (prim.flags_clip & 16u) == 0u) {
                source = vec4f(0.0);
                let a_bounds = prim.radii;
                let b_norm = vec4f(
                    f32(prim.uv_min & 65535u),
                    f32(prim.uv_min >> 16u),
                    f32(prim.uv_max & 65535u),
                    f32(prim.uv_max >> 16u)
                ) / 65535.0;
                let b_bounds = vec4f(
                    mix(prim.bounds.x, prim.bounds.z, b_norm.x),
                    mix(prim.bounds.y, prim.bounds.w, b_norm.y),
                    mix(prim.bounds.x, prim.bounds.z, b_norm.z),
                    mix(prim.bounds.y, prim.bounds.w, b_norm.w)
                );
                let ra = max(prim.effects.z, 0.0);
                let rb = max(prim.effects.w, 0.0);
                let dA = box_distance(p, a_bounds, vec4f(ra));
                let dB = box_distance(p, b_bounds, vec4f(rb));
                let k = max(prim.effects.y, 0.0);
                let h = select(select(0.0, 1.0, dA <= dB), clamp(0.5 + 0.5 * (dB - dA) / k, 0.0, 1.0), k > 0.001);
                var d_fused = mix(dB, dA, h) - k * h * (1.0 - h);
                if (prim.effects.x > 0.0) {
                    let hb = prim.effects.x * 0.5;
                    d_fused = abs(d_fused + hb) - hb;
                }
                let cov = clamp(0.5 - d_fused, 0.0, 1.0);
                if (cov > 0.0001) {
                    var base_col = prim.color;
                    if (prim.shadow_y > 0.5) {
                        let pk = u32(round(prim.shadow_y)) - 1u;
                        let ab = f32((pk >> 18u) & 63u) / 63.0;
                        let srgb_b = vec3f(f32(pk & 63u), f32((pk >> 6u) & 63u), f32((pk >> 12u) & 63u)) / 63.0;
                        let col_b = vec4f(srgb_b * ab, ab);
                        base_col = lerp_oklab_premul(col_b, prim.color, h);
                    }
                    source = base_col * cov;
                }
            } else if ((prim.flags_clip & 4096u) != 0u && (prim.flags_clip & 16u) == 0u) {
                let cov = coverage(p, prim);
                let grain_scale = max(prim.effects.y, 1.0);
                let grain_amp = clamp(prim.effects.z, 0.0, 0.25);
                let g_off = eval_acrylic_grain(p, grain_scale) * grain_amp * prim.color.a;
                let base_rgb = clamp(prim.color.rgb + vec3f(g_off), vec3f(0.0), vec3f(prim.color.a));
                source = vec4f(base_rgb, prim.color.a) * cov;
            } else if ((prim.flags_clip & 2u) != 0u && (prim.flags_clip & 16u) == 0u) {
                var g0 = vec2f(prim.effects.w, prim.shadow_y);
                let g1 = vec2f(prim.effects.y, prim.effects.z);
                var gdir = g1 - g0;
                var len2 = dot(gdir, gdir);
                if (len2 <= 1e-6) {
                    g0 = prim.bounds.xy;
                    gdir = vec2f(0.0, prim.bounds.w - prim.bounds.y);
                    len2 = dot(gdir, gdir);
                }
                let t = select(0.0, clamp(dot(p - g0, gdir) / len2, 0.0, 1.0), len2 > 1e-6);
                let c1 = vec4f(
                    f32(prim.uv_min & 65535u),
                    f32(prim.uv_min >> 16u),
                    f32(prim.uv_max & 65535u),
                    f32(prim.uv_max >> 16u)
                ) / 65535.0;
                let base_color = select(mix(prim.color, c1, t), lerp_oklab_premul(prim.color, c1, t), (prim.flags_clip & 2048u) != 0u);
                source = base_color * coverage(p, prim);
            } else {
                var cov = coverage(p, prim);
                if ((frame.render_flags & 2u) != 0u && (prim.flags_clip & 0x0110u) == 0x0110u) {
                    let font_px = select(13.0, prim.effects.x, prim.effects.x > 0.5);
                    cov = eval_font_gamma_cov(cov, font_px, prim.color, color);
                }
                source = prim.color * cov;
            }
            source *= clip_a;
            color = source + color * (1.0 - source.a);
        }
        workgroupBarrier();
    }
    if ((frame.render_flags & 1u) != 0u && color.a > 0.0) {
        let ign = fract(52.9829189 * fract(dot(vec2f(pixel), vec2f(0.06711056, 0.00583715))));
        color = vec4f(clamp(color.rgb + vec3f((ign - 0.5) / 255.0), vec3f(0.0), vec3f(1.0)), color.a);
    }
    if (all(pixel < frame.extent)) {
        textureStore(output, vec2i(pixel), color);
    }
}

@group(0) @binding(0) var composed: texture_2d<f32>;

@vertex
fn fullscreen(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    var positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(positions[index], 0.0, 1.0);
}

fn srgb_to_linear(c: vec3f) -> vec3f {
    return select(c / 12.92, pow((c + 0.055) / 1.055, vec3f(2.4)), c > vec3f(0.04045));
}

@fragment
fn present(@builtin(position) pixel: vec4f) -> @location(0) vec4f {
    return textureLoad(composed, vec2i(pixel.xy), 0);
}

@fragment
fn present_srgb(@builtin(position) pixel: vec4f) -> @location(0) vec4f {
    let value = textureLoad(composed, vec2i(pixel.xy), 0);
    return vec4f(srgb_to_linear(value.rgb), value.a);
}
