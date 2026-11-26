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

fn clipped(p: vec2f, first_clip: u32) -> bool {
    var current = first_clip;
    for (var depth = 0u; depth < 64u && current != 65535u && current < arrayLength(&clips); depth++) {
        let clip = clips[current];
        if (box_distance(p, clip.bounds, vec4f(clip.radius)) > 0.0) {
            return true;
        }
        current = clip.parent;
    }
    return false;
}

// Abramowitz & Stegun 7.1.27 四阶有理近似 (与 HLSL / CpuTileRasterizer::erf_approx 严格一致)
fn erf_approx(x: vec2f) -> vec2f {
    let v = abs(x);
    let v2 = v * v;
    let t = 1.0 / (1.0 + 0.278393 * v + 0.230389 * v2 + 0.000972 * v2 * v + 0.078108 * v2 * v2);
    let t2 = t * t;
    return sign(x) * (1.0 - t2 * t2);
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
    var distance = box_distance(p, prim.bounds, prim.radii);
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

    for (var start = 0u; start < primitive_count; start += 64u) {
        let count = min(64u, primitive_count - start);
        if (thread < count) {
            tile_indices[thread] = indices[tile.offset + start + thread];
        }
        workgroupBarrier();
        for (var i = 0u; i < count; i++) {
            let prim = primitives[tile_indices[i]];
            if (clipped(p, prim.flags_clip >> 16u)) {
                continue;
            }
            if ((prim.flags_clip & 0x0202u) == 0u && prim.effects.y > 0.0) {
                let origin = vec2f(prim.effects.w, prim.shadow_y);
                let spread = prim.effects.z;
                let sigma = prim.effects.y * sqrt(2.0);
                let amount = (erf_approx((p - prim.bounds.xy - origin + spread) / sigma)
                    - erf_approx((p - prim.bounds.zw - origin - spread) / sigma)) * 0.5;
                let alpha = prim.color.a * amount.x * amount.y;
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
                let base_color = mix(prim.color, c1, t);
                source = base_color * coverage(p, prim);
            } else {
                source = prim.color * coverage(p, prim);
            }
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
