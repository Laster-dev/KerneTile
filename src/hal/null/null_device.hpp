#pragma once

#include "kernetile/hal/hal.hpp"
#include <vector>
#include <cstring>

namespace kernetile::hal::null {

class NullBuffer final : public GpuBuffer {
public:
    explicit NullBuffer(const BufferDesc& desc)
        : m_size(desc.size_bytes), m_storage(desc.size_bytes, 0) {}

    void* map() override { return m_storage.data(); }
    void unmap() override {}
    uint64_t get_gpu_virtual_address() const noexcept override {
        return reinterpret_cast<uint64_t>(m_storage.data());
    }
    size_t get_size_bytes() const noexcept override { return m_size; }

private:
    size_t m_size;
    std::vector<uint8_t> m_storage;
};

class NullTexture final : public GpuTexture {
public:
    explicit NullTexture(const TextureDesc& desc)
        : m_width(desc.width), m_height(desc.height), m_format(desc.format),
          m_state(ResourceState::Common) {}

    uint32_t get_width() const noexcept override { return m_width; }
    uint32_t get_height() const noexcept override { return m_height; }
    TextureFormat get_format() const noexcept override { return m_format; }
    ResourceState get_current_state() const noexcept override { return m_state; }

    void set_state(ResourceState state) noexcept { m_state = state; }

private:
    uint32_t m_width;
    uint32_t m_height;
    TextureFormat m_format;
    ResourceState m_state;
};

class NullComputePipeline final : public ComputePipeline {
public:
    NullComputePipeline() = default;
};

class NullSwapchain final : public Swapchain {
public:
    NullSwapchain(uint32_t w, uint32_t h) : m_width(w), m_height(h) {}

    void resize(uint32_t width, uint32_t height) override {
        m_width = width;
        m_height = height;
    }

    void present(bool /*vsync*/) override {}
    uint32_t get_width() const noexcept override { return m_width; }
    uint32_t get_height() const noexcept override { return m_height; }

private:
    uint32_t m_width;
    uint32_t m_height;
};

class NullCommandRecorder final : public CommandRecorder {
public:
    NullCommandRecorder() = default;

    void resource_barrier(GpuTexture& texture, ResourceState /*before*/, ResourceState after) override {
        static_cast<NullTexture&>(texture).set_state(after);
    }

    void set_compute_pipeline(const ComputePipeline& /*pipeline*/) override {}
    void set_storage_buffer(uint32_t /*slot*/, const GpuBuffer& /*buffer*/) override {}
    void set_storage_texture(uint32_t /*slot*/, const GpuTexture& /*texture*/) override {}
    void dispatch(uint32_t /*gx*/, uint32_t /*gy*/, uint32_t /*gz*/) override {}
    void dispatch_indirect(const GpuBuffer& /*args_buffer*/, uint64_t /*offset_bytes*/) override {}
    void copy_texture_to_buffer(const GpuTexture& /*src*/, const GpuBuffer& /*dst*/) override {}
    void blit_to_swapchain(Swapchain& /*swapchain*/, const GpuTexture& /*src*/) override {}
    void close() override {}
};

class NullDevice final : public GraphicsDevice {
public:
    NullDevice() = default;
    ~NullDevice() override = default;

    std::unique_ptr<GpuBuffer> create_buffer(const BufferDesc& desc) override {
        return std::make_unique<NullBuffer>(desc);
    }

    std::unique_ptr<GpuTexture> create_texture(const TextureDesc& desc) override {
        return std::make_unique<NullTexture>(desc);
    }

    std::unique_ptr<ComputePipeline> create_compute_pipeline(std::span<const uint8_t> /*bytecode*/) override {
        return std::make_unique<NullComputePipeline>();
    }

    std::unique_ptr<CommandRecorder> create_command_recorder() override {
        return std::make_unique<NullCommandRecorder>();
    }

    std::unique_ptr<Swapchain> create_swapchain(void* /*native_window_handle*/, uint32_t width, uint32_t height) override {
        return std::make_unique<NullSwapchain>(width, height);
    }

    void submit(std::unique_ptr<CommandRecorder> /*recorder*/) override {}
    void wait_idle() override {}

    std::string_view get_device_name() const noexcept override {
        return "KerneTile Universal Software/Headless Device (Linux/POSIX)";
    }
};

[[nodiscard]] std::unique_ptr<GraphicsDevice> create_null_device();

} // namespace kernetile::hal::null
