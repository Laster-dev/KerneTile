#include "ui_compute.hpp"
#include "ui_compute_shader.hpp"

#include <cstdint>
#include <emscripten.h>

namespace kernetile::compute {
namespace {

EM_JS(int, start_webgpu, (const char* code, int win_id), {
    var target = (Module['ktuiCanvases'] && Module['ktuiCanvases'].get(win_id)) || document.getElementById('canvas');
    if (!target || !navigator.gpu) return 0;
    var states = Module['ktuiGpuStates'] || (Module['ktuiGpuStates'] = new Map());
    var id = (Module['ktuiGpuNext'] || 0) + 1;
    Module['ktuiGpuNext'] = id;
    var state = { ready: false, target: target, buffers: [] };
    states.set(id, state);
    var source = UTF8ToString(code);
    var onError = function(error) {
        if (!states.has(id)) return;
        state.ready = false;
        if (state.canvas) state.canvas.style.display = 'none';
        console.warn('KerneTile WebGPU:', error);
    };

    // 多个 WebWindow 共享同一个 GPUDevice 与编译好的管线，避免多窗口触发浏览器设备数量上限
    if (!Module['ktuiGpuSharedPromise']) {
        Module['ktuiGpuSharedPromise'] = navigator.gpu.requestAdapter().then(function(adapter) {
            if (!adapter) throw new Error('No WebGPU adapter');
            return adapter.requestDevice();
        }).then(function(device) {
            var module = device.createShaderModule({code: source});
            var format = navigator.gpu.getPreferredCanvasFormat();
            return Promise.all([
                device.createComputePipelineAsync({layout: 'auto', compute: {module: module, entryPoint: 'compose'}}),
                device.createRenderPipelineAsync({
                    layout: 'auto',
                    vertex: {module: module, entryPoint: 'fullscreen'},
                    fragment: {module: module, entryPoint: 'present', targets: [{format: format}]}
                }),
                device.createComputePipelineAsync({layout: 'auto', compute: {module: module, entryPoint: 'blur_h'}}),
                device.createComputePipelineAsync({layout: 'auto', compute: {module: module, entryPoint: 'blur_v'}})
            ]).then(function(pipelines) {
                var shared = {
                    device: device,
                    format: format,
                    compute: pipelines[0],
                    blit: pipelines[1],
                    blurH: pipelines[2],
                    blurV: pipelines[3],
                    sampler: device.createSampler({minFilter: 'linear', magFilter: 'linear'})
                };
                device.lost.then(function(info) {
                    Module['ktuiGpuSharedPromise'] = null;
                    if (Module['ktuiGpuStates']) {
                        Module['ktuiGpuStates'].forEach(function(st) {
                            st.ready = false;
                            if (st.canvas) st.canvas.style.display = 'none';
                        });
                    }
                    console.warn('KerneTile WebGPU device lost:', info);
                });
                return shared;
            });
        }).catch(function(err) {
            Module['ktuiGpuSharedPromise'] = null;
            throw err;
        });
    }

    Module['ktuiGpuSharedPromise'].then(function(shared) {
        if (!states.has(id)) return;
        var canvas = document.createElement('canvas');
        canvas.setAttribute('aria-hidden', 'true');
        canvas.style.cssText = 'position:absolute;left:0;top:0;pointer-events:none;display:none';
        var parent = target.parentElement;
        if (!parent) throw new Error('Target canvas has no parent');
        if (getComputedStyle(parent).position === 'static') parent.style.position = 'relative';
        parent.insertBefore(canvas, target.nextSibling);
        state.device = shared.device;
        state.canvas = canvas;
        state.context = canvas.getContext('webgpu');
        if (!state.context) throw new Error('No WebGPU canvas context');
        state.context.configure({device: shared.device, format: shared.format, alphaMode: 'opaque'});
        state.compute = shared.compute;
        state.blit = shared.blit;
        state.blurH = shared.blurH;
        state.blurV = shared.blurV;
        state.sampler = shared.sampler;
        state.ready = true;
        if (window._ktuiRequestFrame) window._ktuiRequestFrame();
    }).catch(onError);
    return id;
});

EM_JS(int, webgpu_ready, (int id), {
    var states = Module['ktuiGpuStates'];
    var state = states && states.get(id);
    return state && state.ready ? 1 : 0;
});

EM_JS(void, stop_webgpu, (int id), {
    var states = Module['ktuiGpuStates'];
    var state = states && states.get(id);
    if (!state) return;
    states.delete(id);
    if (state.output) state.output.destroy();
    if (state.scene) state.scene.destroy();
    if (state.blurA) state.blurA.destroy();
    if (state.blurB) state.blurB.destroy();
    if (state.dummyTex) state.dummyTex.destroy();
    if (state.atlas) state.atlas.destroy();
    if (state.colorAtlas) state.colorAtlas.destroy();
    if (state.buffers) {
        for (var i = 0; i < state.buffers.length; ++i) {
            if (state.buffers[i]) state.buffers[i].destroy();
        }
    }
    if (state.canvas) state.canvas.remove();
});

EM_JS(int, render_webgpu, (int id, unsigned width, unsigned height,
    const void* prim, unsigned prim_bytes, const void* indices, unsigned index_bytes,
    const void* tiles, unsigned tile_count, const void* clips, unsigned clip_bytes,
    const void* atlas, unsigned atlas_width, unsigned atlas_height, const void* atlas_updates, unsigned update_count,
    unsigned mica_enabled, const uint32_t* mica_grid,
    const void* color_atlas, unsigned color_atlas_width, unsigned color_atlas_height,
    const void* color_atlas_updates, unsigned color_update_count, unsigned render_flags, unsigned backdrop_split), {
    var state = Module['ktuiGpuStates'] && Module['ktuiGpuStates'].get(id);
    if (!state || !state.ready) return 0;
    var canvas = state.canvas;
    var device = state.device;
    try {
        if (!state.dummyTex) {
            state.dummyTex = device.createTexture({
                size: [1, 1], format: 'rgba8unorm',
                usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST
            });
            state.dummyView = state.dummyTex.createView();
        }
        if (!state.output || canvas.width !== width || canvas.height !== height) {
            canvas.width = width;
            canvas.height = height;
            if (state.output) state.output.destroy();
            if (state.scene) { state.scene.destroy(); state.scene = null; }
            if (state.blurA) { state.blurA.destroy(); state.blurA = null; }
            if (state.blurB) { state.blurB.destroy(); state.blurB = null; }
            state.output = device.createTexture({
                size: [width, height], format: 'rgba8unorm',
                usage: GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING
            });
            state.outputView = state.output.createView();
            state.presentGroup = device.createBindGroup({
                layout: state.blit.getBindGroupLayout(0),
                entries: [{binding: 0, resource: state.outputView}]
            });
        }

        var encoder = device.createCommandEncoder();

        var recreateAtlas = !state.atlas || state.atlasW !== atlas_width || state.atlasH !== atlas_height;
        if (recreateAtlas) {
            if (state.atlas) state.atlas.destroy();
            state.atlas = device.createTexture({
                size: [Math.max(1, atlas_width), Math.max(1, atlas_height)], format: 'r8unorm',
                usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING
            });
            state.atlasView = state.atlas.createView();
            state.atlasW = atlas_width;
            state.atlasH = atlas_height;
        }
        if (atlas && atlas_width > 0 && atlas_height > 0) {
            var atlasPtr = atlas >>> 0;
            var uploadAtlas = function(x, y, w, h) {
                if (w <= 0 || h <= 0 || x + w > atlas_width || y + h > atlas_height) return;
                var offset = atlasPtr + y * atlas_width + x;
                var bytes = HEAPU8.subarray(offset, offset + (h - 1) * atlas_width + w);
                device.queue.writeTexture({texture: state.atlas, origin: [x, y]}, bytes,
                    {bytesPerRow: atlas_width, rowsPerImage: h}, [w, h]);
            };
            if (recreateAtlas) {
                uploadAtlas(0, 0, atlas_width, atlas_height);
            } else if (update_count > 0) {
                var rect = atlas_updates >>> 2;
                for (var i = 0; i < update_count; ++i, rect += 4) {
                    var rx = HEAPU32[rect], ry = HEAPU32[rect + 1], rw = HEAPU32[rect + 2], rh = HEAPU32[rect + 3];
                    uploadAtlas(rx, ry, rw, rh);
                    if (rx === 0 && ry === 0 && rw === atlas_width && rh === atlas_height) break;
                }
            }
        }

        var caW = (color_atlas && color_atlas_width > 0) ? color_atlas_width : 1;
        var caH = (color_atlas && color_atlas_height > 0) ? color_atlas_height : 1;
        var recreateColorAtlas = !state.colorAtlas || state.colorAtlasW !== caW || state.colorAtlasH !== caH;
        if (recreateColorAtlas) {
            if (state.colorAtlas) state.colorAtlas.destroy();
            state.colorAtlas = device.createTexture({
                size: [caW, caH], format: 'rgba8unorm',
                usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING
            });
            state.colorAtlasView = state.colorAtlas.createView();
            state.colorAtlasW = caW;
            state.colorAtlasH = caH;
        }
        if (color_atlas && color_atlas_width > 0 && color_atlas_height > 0) {
            var colorAtlasPtr = color_atlas >>> 0;
            var uploadColorAtlas = function(x, y, w, h) {
                if (w <= 0 || h <= 0 || x + w > color_atlas_width || y + h > color_atlas_height) return;
                var offset = colorAtlasPtr + (y * color_atlas_width + x) * 4;
                var bytes = HEAPU8.subarray(offset, offset + ((h - 1) * color_atlas_width + w) * 4);
                device.queue.writeTexture({texture: state.colorAtlas, origin: [x, y]}, bytes,
                    {bytesPerRow: color_atlas_width * 4, rowsPerImage: h}, [w, h]);
            };
            if (recreateColorAtlas) {
                uploadColorAtlas(0, 0, color_atlas_width, color_atlas_height);
            } else if (color_update_count > 0) {
                var crect = color_atlas_updates >>> 2;
                for (var ci = 0; ci < color_update_count; ++ci, crect += 4) {
                    var crx = HEAPU32[crect], cry = HEAPU32[crect + 1], crw = HEAPU32[crect + 2], crh = HEAPU32[crect + 3];
                    uploadColorAtlas(crx, cry, crw, crh);
                    if (crx === 0 && cry === 0 && crw === color_atlas_width && crh === color_atlas_height) break;
                }
            }
        }

        if (tile_count) {
            var upload = function(slot, bytes, minimum, usage) {
                var required = Math.max(minimum, bytes.byteLength);
                var buffer = state.buffers[slot];
                if (!buffer || buffer.size < required) {
                    var size = buffer ? Math.max(required, buffer.size * 2) : required;
                    if (buffer) buffer.destroy();
                    buffer = device.createBuffer({size: size, usage: usage | GPUBufferUsage.COPY_DST});
                    state.buffers[slot] = buffer;
                }
                if (bytes.byteLength) device.queue.writeBuffer(buffer, 0, bytes);
                return {buffer: buffer, size: required};
            };
            var groupsX = Math.min(tile_count, 1024);
            var uniformData = new Uint32Array(24);
            uniformData[0] = width;
            uniformData[1] = height;
            uniformData[2] = tile_count;
            uniformData[3] = groupsX;
            uniformData[4] = clip_bytes >>> 5;
            uniformData[5] = mica_enabled;
            uniformData[6] = render_flags;
            if (mica_enabled && mica_grid) {
                var mIdx = mica_grid >>> 2;
                for (var mi = 0; mi < 16; ++mi) uniformData[8 + mi] = HEAPU32[mIdx + mi];
            }
            var primPtr = prim >>> 0;
            var indicesPtr = indices >>> 0;
            var tilesPtr = tiles >>> 0;
            var clipsPtr = clips >>> 0;
            var buf0 = upload(0, HEAPU8.subarray(primPtr, primPtr + prim_bytes), 80, GPUBufferUsage.STORAGE);
            var buf1 = upload(1, HEAPU8.subarray(indicesPtr, indicesPtr + index_bytes), 4, GPUBufferUsage.STORAGE);
            var buf2 = upload(2, HEAPU8.subarray(tilesPtr, tilesPtr + tile_count * 16), 16, GPUBufferUsage.STORAGE);
            var buf3 = upload(3, HEAPU8.subarray(clipsPtr, clipsPtr + clip_bytes), 32, GPUBufferUsage.STORAGE);
            var groupsY = Math.ceil(tile_count / groupsX);

            if ((backdrop_split >>> 0) === 0xFFFFFFFF) {
                uniformData[7] = 0;
                var data = [
                    buf0, buf1, buf2, buf3,
                    state.atlasView, state.sampler, state.outputView,
                    upload(4, uniformData, 96, GPUBufferUsage.UNIFORM),
                    state.colorAtlasView, state.dummyView, state.dummyView
                ];
                var group = device.createBindGroup({
                    layout: state.compute.getBindGroupLayout(0),
                    entries: data.map(function(resource, binding) { return {binding: binding, resource: resource}; })
                });
                var compute = encoder.beginComputePass();
                compute.setPipeline(state.compute);
                compute.setBindGroup(0, group);
                compute.dispatchWorkgroups(groupsX, groupsY);
                compute.end();
            } else {
                var bw = Math.max(1, (width + 3) >> 2);
                var bh = Math.max(1, (height + 3) >> 2);
                if (!state.scene) {
                    state.scene = device.createTexture({
                        size: [width, height], format: 'rgba8unorm',
                        usage: GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING
                    });
                    state.sceneView = state.scene.createView();
                }
                if (!state.blurA) {
                    state.blurA = device.createTexture({
                        size: [bw, bh], format: 'rgba8unorm',
                        usage: GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING
                    });
                    state.blurAView = state.blurA.createView();
                    state.blurB = device.createTexture({
                        size: [bw, bh], format: 'rgba8unorm',
                        usage: GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING
                    });
                    state.blurBView = state.blurB.createView();
                }

                // Pass 1: 绘制主场景 (prim_id < backdrop_split) 到 scene 纹理
                var u1 = new Uint32Array(uniformData);
                u1[7] = ((1 << 28) | (backdrop_split & 0x0FFFFFFF)) >>> 0;
                var d1 = [
                    buf0, buf1, buf2, buf3,
                    state.atlasView, state.sampler, state.sceneView,
                    upload(4, u1, 96, GPUBufferUsage.UNIFORM),
                    state.colorAtlasView, state.dummyView, state.dummyView
                ];
                var g1 = device.createBindGroup({
                    layout: state.compute.getBindGroupLayout(0),
                    entries: d1.map(function(resource, binding) { return {binding: binding, resource: resource}; })
                });
                var c1 = encoder.beginComputePass();
                c1.setPipeline(state.compute);
                c1.setBindGroup(0, g1);
                c1.dispatchWorkgroups(groupsX, groupsY);
                c1.end();

                // Pass 2: 双轮 1/4 分辨率 13-tap 可分离高斯重度背景模糊
                var bGX = Math.ceil(bw / 16);
                var bGY = Math.ceil(bh / 16);
                var runBlur = function(pipeline, srcView, dstView) {
                    var bg = device.createBindGroup({
                        layout: pipeline.getBindGroupLayout(0),
                        entries: [
                            {binding: 0, resource: srcView},
                            {binding: 1, resource: state.sampler},
                            {binding: 2, resource: dstView}
                        ]
                    });
                    var cp = encoder.beginComputePass();
                    cp.setPipeline(pipeline);
                    cp.setBindGroup(0, bg);
                    cp.dispatchWorkgroups(bGX, bGY);
                    cp.end();
                };
                runBlur(state.blurH, state.sceneView, state.blurAView);
                runBlur(state.blurV, state.blurAView, state.blurBView);
                runBlur(state.blurH, state.blurBView, state.blurAView);
                runBlur(state.blurV, state.blurAView, state.blurBView);

                // Pass 3: 从 scene 纹理恢复背景并在弹出层区域采样重度模糊纹理合成前景
                var u2 = new Uint32Array(uniformData);
                u2[7] = ((2 << 28) | (backdrop_split & 0x0FFFFFFF)) >>> 0;
                var d2 = [
                    buf0, buf1, buf2, buf3,
                    state.atlasView, state.sampler, state.outputView,
                    upload(5, u2, 96, GPUBufferUsage.UNIFORM),
                    state.colorAtlasView, state.sceneView, state.blurBView
                ];
                var g2 = device.createBindGroup({
                    layout: state.compute.getBindGroupLayout(0),
                    entries: d2.map(function(resource, binding) { return {binding: binding, resource: resource}; })
                });
                var c2 = encoder.beginComputePass();
                c2.setPipeline(state.compute);
                c2.setBindGroup(0, g2);
                c2.dispatchWorkgroups(groupsX, groupsY);
                c2.end();
            }
        }

        var pass = encoder.beginRenderPass({colorAttachments: [{
            view: state.context.getCurrentTexture().createView(), loadOp: 'clear',
            storeOp: 'store', clearValue: {r: 0, g: 0, b: 0, a: 0}
        }]});
        pass.setPipeline(state.blit);
        pass.setBindGroup(0, state.presentGroup);
        pass.draw(3);
        pass.end();
        device.queue.submit([encoder.finish()]);

        var target = state.target;
        var dpr = Math.max(1, window.devicePixelRatio || 1);
        var left = target.offsetLeft + 'px';
        var top = target.offsetTop + 'px';
        var cssW = (width / dpr) + 'px';
        var cssH = (height / dpr) + 'px';
        if (canvas.style.left !== left) canvas.style.left = left;
        if (canvas.style.top !== top) canvas.style.top = top;
        if (canvas.style.width !== cssW) canvas.style.width = cssW;
        if (canvas.style.height !== cssH) canvas.style.height = cssH;
        if (canvas.style.display !== 'block') {
            canvas.style.display = 'block';
        }
        return 1;
    } catch (error) {
        state.ready = false;
        canvas.style.display = 'none';
        console.warn('KerneTile WebGPU:', error);
        return 0;
    }
});

class WebBackend final : public Backend {
public:
    explicit WebBackend(int id) : m_id(id) {}
    ~WebBackend() override { stop_webgpu(m_id); }
    bool ready() const noexcept override { return webgpu_ready(m_id) != 0; }

    bool submit(const Frame& frame) override {
        uint32_t backdrop_split = 0xFFFFFFFFu;
        for (uint32_t i = 0; i < static_cast<uint32_t>(frame.primitives.size()); ++i) {
            if ((frame.primitives[i].flags & static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter)) != 0) {
                backdrop_split = i;
                break;
            }
        }
        return render_webgpu(m_id, frame.width, frame.height,
            frame.primitives.data(), frame.primitives.size_bytes(),
            frame.indices.data(), frame.indices.size_bytes(),
            frame.tiles.data(), frame.tiles.size(),
            frame.clips.data(), frame.clips.size_bytes(),
            frame.atlas.data(), frame.atlas_width, frame.atlas_height,
            frame.atlas_updates.data(), frame.atlas_updates.size(),
            frame.mica_enabled ? 1u : 0u, frame.mica_grid.data(),
            frame.color_atlas.data(), frame.color_atlas_width, frame.color_atlas_height,
            frame.color_atlas_updates.data(), frame.color_atlas_updates.size(),
            frame.render_flags, backdrop_split) != 0;
    }

private:
    int m_id;
};
} // namespace

std::unique_ptr<Backend> create(void* window, void*, bool) {
    if (window) {
        if (int id = start_webgpu(shader_source, static_cast<int>(reinterpret_cast<uintptr_t>(window)))) {
            return std::make_unique<WebBackend>(id);
        }
    }
    return nullptr;
}
} // namespace kernetile::compute
