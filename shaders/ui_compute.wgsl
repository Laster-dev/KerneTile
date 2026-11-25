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
    pad0: u32,
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
var<workgroup> tile_indices: array<u32, 64>;
var<workgroup> tile_count_shared: u32;

fn box_distance(p: vec2f, bounds: vec4f, radii: vec4f) -> f32 {
    let center = (bounds.xy + bounds.zw) * 0.5;
    let delta = p - center;
    let radius = select(select(radii.x, radii.w, delta.y > 0.0),
                        select(radii.y, radii.z, delta.y > 0.0), delta.x > 0.0);
    let q = abs(delta) - (bounds.zw - bounds.xy) * 0.5 + radius;
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - radius;
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
            if (prim.effects.y > 0.0) {
                let origin = vec2f(prim.effects.w, prim.shadow_y);
                let spread = prim.effects.z;
                let sigma = prim.effects.y * sqrt(2.0);
                let amount = (erf_approx((p - prim.bounds.xy - origin + spread) / sigma)
                    - erf_approx((p - prim.bounds.zw - origin - spread) / sigma)) * 0.5;
                let alpha = prim.color.a * amount.x * amount.y;
                color = vec4f(0.0, 0.0, 0.0, alpha) + color * (1.0 - alpha);
            }
            let source = prim.color * coverage(p, prim);
            color = source + color * (1.0 - source.a);
        }
        workgroupBarrier();
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
