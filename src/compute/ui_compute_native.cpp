#include "ui_compute.hpp"
#include "ui_compute_shader.hpp"

#include <wgpu.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <memory>
#include <type_traits>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace kernetile::compute {
namespace {

template <auto Release>
struct Deleter {
    template <typename Handle>
    void operator()(Handle handle) const { Release(handle); }
};

template <typename Handle, auto Release>
using Owned = std::unique_ptr<std::remove_pointer_t<Handle>, Deleter<Release>>;

using Buffer = Owned<WGPUBuffer, wgpuBufferRelease>;
using Texture = Owned<WGPUTexture, wgpuTextureRelease>;
using View = Owned<WGPUTextureView, wgpuTextureViewRelease>;

class NativeBackend final : public Backend {
public:
    bool init(void* window, void* display) {
        WGPUInstanceExtras extras{};
        extras.chain.sType = static_cast<WGPUSType>(WGPUSType_InstanceExtras);
        WGPURequestAdapterOptions options{};
        WGPUBackendType expected_backend;
        WGPUSurfaceDescriptor surface{};
#ifdef _WIN32
        (void)display;
        extras.backends = WGPUInstanceBackend_DX12;
        expected_backend = WGPUBackendType_D3D12;
        WGPUSurfaceDescriptorFromWindowsHWND native{};
        native.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
        native.hwnd = window;
        native.hinstance = GetModuleHandleW(nullptr);
#else
        if (!display) return false;
        extras.backends = WGPUInstanceBackend_Vulkan;
        expected_backend = WGPUBackendType_Vulkan;
        WGPUSurfaceDescriptorFromXlibWindow native{};
        native.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
        native.window = reinterpret_cast<uintptr_t>(window);
        native.display = display;
#endif
        WGPUInstanceDescriptor instance{};
        instance.nextInChain = &extras.chain;
        m_instance.reset(wgpuCreateInstance(&instance));
        if (!m_instance) return false;
        surface.nextInChain = &native.chain;
        m_surface.reset(wgpuInstanceCreateSurface(m_instance.get(), &surface));
        if (!m_surface) return false;
        options.compatibleSurface = m_surface.get();
        WGPUAdapter adapter = nullptr;
        // wgpu-native 0.19 completes these two requests inline.
        wgpuInstanceRequestAdapter(m_instance.get(), &options,
            [](WGPURequestAdapterStatus, WGPUAdapter value, const char*, void* result) {
                *static_cast<WGPUAdapter*>(result) = value;
            }, &adapter);
        m_adapter.reset(adapter);
        if (!m_adapter) return false;
        WGPUAdapterProperties properties{};
        wgpuAdapterGetProperties(adapter, &properties);
        if (properties.backendType != expected_backend ||
            properties.adapterType == WGPUAdapterType_CPU) return false;

        WGPUDevice device = nullptr;
        WGPUDeviceDescriptor device_desc{};
        device_desc.deviceLostCallback = [](WGPUDeviceLostReason, const char*, void* self) {
            static_cast<NativeBackend*>(self)->m_failed = true;
        };
        device_desc.deviceLostUserdata = this;
        wgpuAdapterRequestDevice(adapter, &device_desc,
            [](WGPURequestDeviceStatus, WGPUDevice value, const char*, void* result) {
                *static_cast<WGPUDevice*>(result) = value;
            }, &device);
        m_device.reset(device);
        if (!m_device) return false;
        wgpuDeviceSetUncapturedErrorCallback(device,
            [](WGPUErrorType, const char* message, void* self) {
                std::fprintf(stderr, "KerneTile GPU: %s\n", message ? message : "unknown error");
                static_cast<NativeBackend*>(self)->m_failed = true;
            }, this);
        m_queue.reset(wgpuDeviceGetQueue(device));

        WGPUSurfaceCapabilities caps{};
        wgpuSurfaceGetCapabilities(m_surface.get(), adapter, &caps);
        if (caps.formatCount) m_format = caps.formats[0];
        for (size_t i = 0; i < caps.formatCount; ++i) {
            if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm ||
                caps.formats[i] == WGPUTextureFormat_RGBA8Unorm) {
                m_format = caps.formats[i];
                break;
            }
        }
        if (caps.alphaModeCount) m_alpha = caps.alphaModes[0];
        for (size_t i = 0; i < caps.alphaModeCount; ++i) {
            if (caps.alphaModes[i] == WGPUCompositeAlphaMode_Opaque) m_alpha = caps.alphaModes[i];
        }
        wgpuSurfaceCapabilitiesFreeMembers(caps);
        if (m_format == WGPUTextureFormat_Undefined) return false;

        WGPUShaderModuleWGSLDescriptor wgsl{};
        wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgsl.code = shader_source;
        WGPUShaderModuleDescriptor shader_desc{};
        shader_desc.nextInChain = &wgsl.chain;
        Owned<WGPUShaderModule, wgpuShaderModuleRelease> shader(wgpuDeviceCreateShaderModule(device, &shader_desc));
        if (!shader || m_failed) return false;

        WGPUComputePipelineDescriptor compute{};
        compute.compute.module = shader.get();
        compute.compute.entryPoint = "compose";
        m_compute.reset(wgpuDeviceCreateComputePipeline(device, &compute));
        if (!m_compute || m_failed) return false;

        WGPUColorTargetState target{};
        target.format = m_format;
        target.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fragment{};
        fragment.module = shader.get();
        fragment.entryPoint = m_format == WGPUTextureFormat_BGRA8UnormSrgb ||
                              m_format == WGPUTextureFormat_RGBA8UnormSrgb ? "present_srgb" : "present";
        fragment.targetCount = 1;
        fragment.targets = &target;
        WGPURenderPipelineDescriptor blit{};
        blit.vertex.module = shader.get();
        blit.vertex.entryPoint = "fullscreen";
        blit.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        blit.primitive.frontFace = WGPUFrontFace_CCW;
        blit.primitive.cullMode = WGPUCullMode_None;
        blit.multisample.count = 1;
        blit.multisample.mask = ~0u;
        blit.fragment = &fragment;
        m_blit.reset(wgpuDeviceCreateRenderPipeline(device, &blit));
        if (!m_blit || m_failed) return false;

        m_compute_layout.reset(wgpuComputePipelineGetBindGroupLayout(m_compute.get(), 0));
        m_blit_layout.reset(wgpuRenderPipelineGetBindGroupLayout(m_blit.get(), 0));
        WGPUSamplerDescriptor sampler{};
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = WGPUAddressMode_ClampToEdge;
        sampler.magFilter = sampler.minFilter = WGPUFilterMode_Linear;
        sampler.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        sampler.lodMaxClamp = 1;
        sampler.maxAnisotropy = 1;
        m_sampler.reset(wgpuDeviceCreateSampler(device, &sampler));
        return ready();
    }

    bool ready() const noexcept override { return m_sampler && !m_failed; }

    bool submit(const Frame& frame) override {
        if (!ready()) return false;
        if (!m_output || frame.width != m_width || frame.height != m_height) {
            configure(frame.width, frame.height);
            m_output = texture(frame.width, frame.height, WGPUTextureFormat_RGBA8Unorm,
                               WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding);
            if (!m_output || m_failed) return false;
            m_output_view.reset(wgpuTextureCreateView(m_output.get(), nullptr));
            WGPUBindGroupEntry entry{};
            entry.textureView = m_output_view.get();
            m_present_group = bind(m_blit_layout.get(), std::span(&entry, 1));
        }

        WGPUSurfaceTexture surface_texture{};
        wgpuSurfaceGetCurrentTexture(m_surface.get(), &surface_texture);
        if (surface_texture.status == WGPUSurfaceGetCurrentTextureStatus_Outdated ||
            surface_texture.status == WGPUSurfaceGetCurrentTextureStatus_Lost) {
            if (surface_texture.texture) wgpuTextureRelease(surface_texture.texture);
            surface_texture = {};
            configure(frame.width, frame.height);
            wgpuSurfaceGetCurrentTexture(m_surface.get(), &surface_texture);
        }
        Texture drawable(surface_texture.texture);
        if (!drawable || surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) return false;
        View drawable_view(wgpuTextureCreateView(drawable.get(), nullptr));
        Owned<WGPUCommandEncoder, wgpuCommandEncoderRelease> encoder(wgpuDeviceCreateCommandEncoder(m_device.get(), nullptr));
        if (!frame.tiles.empty() && !compute(encoder.get(), frame)) return false;

        WGPURenderPassColorAttachment color{};
        color.view = drawable_view.get();
        color.loadOp = WGPULoadOp_Clear;
        color.storeOp = WGPUStoreOp_Store;
        WGPURenderPassDescriptor desc{};
        desc.colorAttachmentCount = 1;
        desc.colorAttachments = &color;
        Owned<WGPURenderPassEncoder, wgpuRenderPassEncoderRelease> pass(wgpuCommandEncoderBeginRenderPass(encoder.get(), &desc));
        wgpuRenderPassEncoderSetPipeline(pass.get(), m_blit.get());
        wgpuRenderPassEncoderSetBindGroup(pass.get(), 0, m_present_group.get(), 0, nullptr);
        wgpuRenderPassEncoderDraw(pass.get(), 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass.get());
        Owned<WGPUCommandBuffer, wgpuCommandBufferRelease> commands(wgpuCommandEncoderFinish(encoder.get(), nullptr));
        WGPUCommandBuffer list = commands.get();
        wgpuQueueSubmit(m_queue.get(), 1, &list);
        wgpuSurfacePresent(m_surface.get());
        wgpuDevicePoll(m_device.get(), false, nullptr);
        return ready();
    }

private:
    using Group = Owned<WGPUBindGroup, wgpuBindGroupRelease>;

    void configure(uint32_t width, uint32_t height) {
        WGPUSurfaceConfiguration config{};
        config.device = m_device.get();
        config.width = width;
        config.height = height;
        config.format = m_format;
        config.alphaMode = m_alpha;
        config.usage = WGPUTextureUsage_RenderAttachment;
        config.presentMode = WGPUPresentMode_Fifo;
        wgpuSurfaceConfigure(m_surface.get(), &config);
        m_width = width;
        m_height = height;
    }

    Texture texture(uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsageFlags usage) {
        WGPUTextureDescriptor desc{};
        desc.size = {width, height, 1};
        desc.format = format;
        desc.dimension = WGPUTextureDimension_2D;
        desc.mipLevelCount = desc.sampleCount = 1;
        desc.usage = usage;
        return Texture(wgpuDeviceCreateTexture(m_device.get(), &desc));
    }

    Group bind(WGPUBindGroupLayout layout, std::span<const WGPUBindGroupEntry> entries) {
        WGPUBindGroupDescriptor desc{};
        desc.layout = layout;
        desc.entries = entries.data();
        desc.entryCount = entries.size();
        return Group(wgpuDeviceCreateBindGroup(m_device.get(), &desc));
    }

    void upload(size_t slot, WGPUBufferUsageFlags usage, std::span<const std::byte> data, size_t minimum) {
        const size_t required = std::max(minimum, data.size());
        if (required > m_capacity[slot]) {
            WGPUBufferDescriptor desc{};
            desc.size = std::max(required, m_capacity[slot] * 2);
            desc.usage = usage | WGPUBufferUsage_CopyDst;
            m_buffers[slot].reset(wgpuDeviceCreateBuffer(m_device.get(), &desc));
            m_capacity[slot] = desc.size;
        }
        if (!data.empty()) wgpuQueueWriteBuffer(m_queue.get(), m_buffers[slot].get(), 0, data.data(), data.size());
    }

    bool compute(WGPUCommandEncoder encoder, const Frame& frame) {
        const bool first_atlas_upload = !m_atlas;
        if (first_atlas_upload) {
            m_atlas = texture(frame.atlas_width, frame.atlas_height, WGPUTextureFormat_R8Unorm,
                              WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
            if (!m_atlas || m_failed) return false;
            m_atlas_view.reset(wgpuTextureCreateView(m_atlas.get(), nullptr));
        }
        auto upload_atlas = [&](const atlas::RectU32& rect) {
            WGPUImageCopyTexture dest{};
            dest.texture = m_atlas.get();
            dest.aspect = WGPUTextureAspect_All;
            dest.origin = {rect.x, rect.y, 0};
            WGPUTextureDataLayout layout{};
            layout.bytesPerRow = frame.atlas_width;
            layout.rowsPerImage = rect.h;
            WGPUExtent3D size{rect.w, rect.h, 1};
            const auto data = frame.atlas.subspan(size_t(rect.y) * frame.atlas_width + rect.x,
                size_t(rect.h - 1) * frame.atlas_width + rect.w);
            wgpuQueueWriteTexture(m_queue.get(), &dest, data.data(), data.size(), &layout, &size);
        };
        if (first_atlas_upload) {
            upload_atlas({0, 0, frame.atlas_width, frame.atlas_height});
        } else {
            for (const auto& rect : frame.atlas_updates) upload_atlas(rect);
        }

        const auto count = static_cast<uint32_t>(frame.tiles.size());
        const uint32_t groups_x = std::min(count, 1024u);
        const std::array<uint32_t, 4> params{frame.width, frame.height, count, groups_x};
        upload(0, WGPUBufferUsage_Storage, std::as_bytes(frame.primitives), sizeof(core::UiPrimitive));
        upload(1, WGPUBufferUsage_Storage, std::as_bytes(frame.indices), sizeof(uint32_t));
        upload(2, WGPUBufferUsage_Storage, std::as_bytes(frame.tiles), sizeof(Tile));
        upload(3, WGPUBufferUsage_Storage, std::as_bytes(frame.clips), sizeof(core::ClipRect));
        upload(4, WGPUBufferUsage_Uniform, std::as_bytes(std::span(params)), sizeof(params));
        if (m_failed) return false;

        std::array<WGPUBindGroupEntry, 8> entries{};
        for (uint32_t i = 0; i < entries.size(); ++i) entries[i].binding = i;
        const std::array<size_t, 4> sizes{std::max(frame.primitives.size_bytes(), sizeof(core::UiPrimitive)),
            std::max(frame.indices.size_bytes(), sizeof(uint32_t)), frame.tiles.size_bytes(),
            std::max(frame.clips.size_bytes(), sizeof(core::ClipRect))};
        for (uint32_t i = 0; i < sizes.size(); ++i) {
            entries[i].buffer = m_buffers[i].get();
            entries[i].size = sizes[i];
        }
        entries[4].textureView = m_atlas_view.get();
        entries[5].sampler = m_sampler.get();
        entries[6].textureView = m_output_view.get();
        entries[7].buffer = m_buffers[4].get();
        entries[7].size = sizeof(params);

        auto group = bind(m_compute_layout.get(), entries);
        if (!group || m_failed) return false;
        Owned<WGPUComputePassEncoder, wgpuComputePassEncoderRelease> pass(wgpuCommandEncoderBeginComputePass(encoder, nullptr));
        wgpuComputePassEncoderSetPipeline(pass.get(), m_compute.get());
        wgpuComputePassEncoderSetBindGroup(pass.get(), 0, group.get(), 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(pass.get(), groups_x, (count + groups_x - 1) / groups_x, 1);
        wgpuComputePassEncoderEnd(pass.get());
        return !m_failed;
    }

    std::atomic_bool m_failed{false};
    Owned<WGPUInstance, wgpuInstanceRelease> m_instance;
    Owned<WGPUSurface, wgpuSurfaceRelease> m_surface;
    Owned<WGPUAdapter, wgpuAdapterRelease> m_adapter;
    Owned<WGPUDevice, wgpuDeviceRelease> m_device;
    Owned<WGPUQueue, wgpuQueueRelease> m_queue;
    Owned<WGPUComputePipeline, wgpuComputePipelineRelease> m_compute;
    Owned<WGPURenderPipeline, wgpuRenderPipelineRelease> m_blit;
    Owned<WGPUBindGroupLayout, wgpuBindGroupLayoutRelease> m_compute_layout, m_blit_layout;
    Owned<WGPUSampler, wgpuSamplerRelease> m_sampler;
    Texture m_output, m_atlas;
    View m_output_view, m_atlas_view;
    Group m_present_group;
    std::array<Buffer, 5> m_buffers;
    std::array<size_t, 5> m_capacity{};
    WGPUTextureFormat m_format = WGPUTextureFormat_Undefined;
    WGPUCompositeAlphaMode m_alpha = WGPUCompositeAlphaMode_Auto;
    uint32_t m_width = 0, m_height = 0;
};
} // namespace

std::unique_ptr<Backend> create(void* window, void* display) {
    auto backend = std::make_unique<NativeBackend>();
    if (!window || !backend->init(window, display)) return nullptr;
    return backend;
}
} // namespace kernetile::compute
