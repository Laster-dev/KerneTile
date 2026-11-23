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
                })
            ]).then(function(pipelines) {
                var shared = {
                    device: device,
                    format: format,
                    compute: pipelines[0],
                    blit: pipelines[1],
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
    if (state.atlas) state.atlas.destroy();
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
    unsigned mica_enabled, const uint32_t* mica_grid), {
    var state = Module['ktuiGpuStates'] && Module['ktuiGpuStates'].get(id);
    if (!state || !state.ready) return 0;
    var canvas = state.canvas;
    var device = state.device;
    try {
        if (!state.output || canvas.width !== width || canvas.height !== height) {
            canvas.width = width;
            canvas.height = height;
            if (state.output) state.output.destroy();
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
        if (tile_count) {
            var firstAtlasUpload = !state.atlas;
            if (firstAtlasUpload) {
                state.atlas = device.createTexture({
                    size: [atlas_width, atlas_height], format: 'r8unorm',
                    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING
                });
                state.atlasView = state.atlas.createView();
            }
            var uploadAtlas = function(x, y, w, h) {
                var offset = atlas + y * atlas_width + x;
                var bytes = HEAPU8.subarray(offset, offset + (h - 1) * atlas_width + w);
                device.queue.writeTexture({texture: state.atlas, origin: [x, y]}, bytes,
                    {bytesPerRow: atlas_width, rowsPerImage: h}, [w, h]);
            };
            if (firstAtlasUpload) {
                uploadAtlas(0, 0, atlas_width, atlas_height);
            } else {
                var rect = atlas_updates >>> 2;
                for (var i = 0; i < update_count; ++i, rect += 4)
                    uploadAtlas(HEAPU32[rect], HEAPU32[rect + 1], HEAPU32[rect + 2], HEAPU32[rect + 3]);
            }

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
            if (mica_enabled && mica_grid) {
                var mIdx = mica_grid >>> 2;
                for (var mi = 0; mi < 16; ++mi) uniformData[8 + mi] = HEAPU32[mIdx + mi];
            }
            var data = [
                upload(0, HEAPU8.subarray(prim, prim + prim_bytes), 80, GPUBufferUsage.STORAGE),
                upload(1, HEAPU8.subarray(indices, indices + index_bytes), 4, GPUBufferUsage.STORAGE),
                upload(2, HEAPU8.subarray(tiles, tiles + tile_count * 16), 16, GPUBufferUsage.STORAGE),
                upload(3, HEAPU8.subarray(clips, clips + clip_bytes), 32, GPUBufferUsage.STORAGE),
                state.atlasView,
                state.sampler,
                state.outputView,
                upload(4, uniformData, 96, GPUBufferUsage.UNIFORM)
            ];
            var group = device.createBindGroup({
                layout: state.compute.getBindGroupLayout(0),
                entries: data.map(function(resource, binding) { return {binding: binding, resource: resource}; })
            });
            var compute = encoder.beginComputePass();
            compute.setPipeline(state.compute);
            compute.setBindGroup(0, group);
            compute.dispatchWorkgroups(groupsX, Math.ceil(tile_count / groupsX));
            compute.end();
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
        var left = target.offsetLeft + 'px';
        var top = target.offsetTop + 'px';
        var cssW = target.clientWidth + 'px';
        var cssH = target.clientHeight + 'px';
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
        return render_webgpu(m_id, frame.width, frame.height,
            frame.primitives.data(), frame.primitives.size_bytes(),
            frame.indices.data(), frame.indices.size_bytes(),
            frame.tiles.data(), frame.tiles.size(),
            frame.clips.data(), frame.clips.size_bytes(),
            frame.atlas.data(), frame.atlas_width, frame.atlas_height,
            frame.atlas_updates.data(), frame.atlas_updates.size(),
            frame.mica_enabled ? 1u : 0u, frame.mica_grid.data()) != 0;
    }

private:
    int m_id;
};
} // namespace

std::unique_ptr<Backend> create(void* window, void*) {
    if (window) {
        if (int id = start_webgpu(shader_source, static_cast<int>(reinterpret_cast<uintptr_t>(window)))) {
            return std::make_unique<WebBackend>(id);
        }
    }
    return nullptr;
}
} // namespace kernetile::compute
