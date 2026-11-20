#pragma once

#include "kernetile/hal/hal_types.hpp"
#include <memory>
#include <span>
#include <string_view>

namespace kernetile::hal {

// 显存缓冲对象接口
class GpuBuffer {
public:
    virtual ~GpuBuffer() = default;

    [[nodiscard]] virtual void* map() = 0;
    virtual void unmap() = 0;
    [[nodiscard]] virtual uint64_t get_gpu_virtual_address() const noexcept = 0;
    [[nodiscard]] virtual size_t get_size_bytes() const noexcept = 0;
};

// 纹理对象接口
class GpuTexture {
public:
    virtual ~GpuTexture() = default;

    [[nodiscard]] virtual uint32_t get_width() const noexcept = 0;
    [[nodiscard]] virtual uint32_t get_height() const noexcept = 0;
    [[nodiscard]] virtual TextureFormat get_format() const noexcept = 0;
    [[nodiscard]] virtual ResourceState get_current_state() const noexcept = 0;
};

// 计算管线状态接口 (封装 Compute PSO & Root Signature)
class ComputePipeline {
public:
    virtual ~ComputePipeline() = default;
};

// 交换链接口
class Swapchain {
public:
    virtual ~Swapchain() = default;

    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual void present(bool vsync) = 0;
    [[nodiscard]] virtual uint32_t get_width() const noexcept = 0;
    [[nodiscard]] virtual uint32_t get_height() const noexcept = 0;
};

// 命令录制器接口
class CommandRecorder {
public:
    virtual ~CommandRecorder() = default;

    // 资源状态屏障转换
    virtual void resource_barrier(
        GpuTexture& texture, 
        ResourceState state_before, 
        ResourceState state_after) = 0;

    // 绑定计算管线
    virtual void set_compute_pipeline(const ComputePipeline& pipeline) = 0;

    // 绑定结构化缓冲 (SRV)
    virtual void set_storage_buffer(uint32_t slot, const GpuBuffer& buffer) = 0;

    // 绑定离屏存储纹理 (UAV)
    virtual void set_storage_texture(uint32_t slot, const GpuTexture& texture) = 0;

    // 派发计算工作组
    virtual void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) = 0;

    // 间接派发 (由 GPU 生成的参数缓冲驱动)
    virtual void dispatch_indirect(const GpuBuffer& args_buffer, uint64_t offset_bytes) = 0;

    // 纹理拷贝至回读缓冲 (用于测试与回读)
    virtual void copy_texture_to_buffer(const GpuTexture& src_texture, const GpuBuffer& dst_buffer) = 0;

    // 呈现阶段: 全屏 Quad Blit (将离屏 UNORM 存储纹理搬运至交换链)
    virtual void blit_to_swapchain(Swapchain& swapchain, const GpuTexture& src_offscreen) = 0;

    // 结束录制
    virtual void close() = 0;
};

// 图形抽象设备接口
class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    [[nodiscard]] virtual std::unique_ptr<GpuBuffer> create_buffer(const BufferDesc& desc) = 0;

    [[nodiscard]] virtual std::unique_ptr<GpuTexture> create_texture(const TextureDesc& desc) = 0;

    [[nodiscard]] virtual std::unique_ptr<ComputePipeline> create_compute_pipeline(
        std::span<const uint8_t> bytecode) = 0;

    [[nodiscard]] virtual std::unique_ptr<CommandRecorder> create_command_recorder() = 0;

    [[nodiscard]] virtual std::unique_ptr<Swapchain> create_swapchain(
        void* native_window_handle, 
        uint32_t width, 
        uint32_t height) = 0;

    virtual void submit(std::unique_ptr<CommandRecorder> recorder) = 0;
    virtual void wait_idle() = 0;
    [[nodiscard]] virtual std::string_view get_device_name() const noexcept = 0;
};

// 工厂方法: 创建 Direct3D 12 设备实现 (Windows 平台)
// prefer_warp = true 时优先强制使用微软 WARP 多线程软件光栅驱动
[[nodiscard]] std::unique_ptr<GraphicsDevice> create_d3d12_device(bool prefer_warp = false);

} // namespace kernetile::hal
