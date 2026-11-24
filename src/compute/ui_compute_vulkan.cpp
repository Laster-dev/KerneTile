#if defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#include <android/native_window.h>
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "ui_compute.hpp"
#include <vulkan/vulkan.h>
#include "ui_compute_spirv.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <vector>

namespace kernetile::compute {
namespace {
bool check(VkResult result, const char* operation) noexcept {
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::fprintf(stderr, "KerneTile Vulkan: %s (%d)\n", operation ? operation : "", static_cast<int>(result));
        return false;
    }
    return true;
}

struct Buffer {
    VkBuffer handle{};
    VkDeviceMemory memory{};
    void* bytes{};
    size_t size{};
};
struct Image {
    VkImage handle{};
    VkDeviceMemory memory{};
    VkImageView view{};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

class VulkanBackend final : public Backend {
public:
    ~VulkanBackend() override {
        if (m_device) {
            vkDeviceWaitIdle(m_device);
            destroy(m_output);
            destroy(m_atlas);
            for (auto& buffer : m_buffers) destroy(buffer);
            if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            vkDestroySemaphore(m_device, m_acquired, nullptr);
            vkDestroySemaphore(m_device, m_finished, nullptr);
            vkDestroyCommandPool(m_device, m_pool, nullptr);
            vkDestroySampler(m_device, m_sampler, nullptr);
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
            vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
            vkDestroyDescriptorSetLayout(m_device, m_set_layout, nullptr);
            vkDestroyDevice(m_device, nullptr);
        }
        if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        if (m_instance) vkDestroyInstance(m_instance, nullptr);
    }

    void setup(void* window, void* display) {
#if defined(__ANDROID__)
        (void)display;
        const char* instance_ext[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
#else
        const char* instance_ext[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME};
#endif
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance.pApplicationInfo = &app;
        instance.enabledExtensionCount = 2;
        instance.ppEnabledExtensionNames = instance_ext;
        check(vkCreateInstance(&instance, nullptr, &m_instance), "Vulkan instance");
#if defined(__ANDROID__)
        VkAndroidSurfaceCreateInfoKHR surface{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
        surface.window = static_cast<ANativeWindow*>(window);
        check(vkCreateAndroidSurfaceKHR(m_instance, &surface, nullptr, &m_surface), "Android Vulkan surface");
#else
        VkXlibSurfaceCreateInfoKHR surface{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
        surface.dpy = static_cast<Display*>(display);
        surface.window = reinterpret_cast<uintptr_t>(window);
        check(vkCreateXlibSurfaceKHR(m_instance, &surface, nullptr, &m_surface), "X11 Vulkan surface");
#endif
        pick_device();
        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = m_family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        const char* device_ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device.queueCreateInfoCount = 1;
        device.pQueueCreateInfos = &queue;
        device.enabledExtensionCount = 1;
        device.ppEnabledExtensionNames = &device_ext;
        check(vkCreateDevice(m_physical, &device, nullptr, &m_device), "Vulkan device");
        vkGetDeviceQueue(m_device, m_family, 0, &m_queue);
        vkGetPhysicalDeviceMemoryProperties(m_physical, &m_memory);

        VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool.queueFamilyIndex = m_family;
        check(vkCreateCommandPool(m_device, &pool, nullptr, &m_pool), "Vulkan command pool");
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = m_pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(m_device, &allocation, &m_cmd), "Vulkan command buffer");
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(m_device, &semaphore, nullptr, &m_acquired), "Vulkan acquire semaphore");
        check(vkCreateSemaphore(m_device, &semaphore, nullptr, &m_finished), "Vulkan present semaphore");
        create_pipeline();
    }

    bool ready() const noexcept override { return !m_failed; }

    bool submit(const Frame& frame) override {
        if (m_failed) return false;
        if (!check(vkQueueWaitIdle(m_queue), "Wait for Vulkan frame")) { m_failed = true; return false; }
        if (!m_swapchain || frame.width != m_width || frame.height != m_height || m_recreate) {
            resize(frame);
            if (m_failed || !m_swapchain) return false;
        }
        uint32_t index = 0;
        auto acquire = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_acquired, VK_NULL_HANDLE, &index);
        if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
            resize(frame);
            if (m_failed || !m_swapchain) return false;
            acquire = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_acquired, VK_NULL_HANDLE, &index);
        }
        if (!check(acquire, "Acquire Vulkan backbuffer") ||
            !check(vkResetCommandPool(m_device, m_pool, 0), "Reset Vulkan command pool")) {
            m_failed = true; return false;
        }
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (!check(vkBeginCommandBuffer(m_cmd, &begin), "Begin Vulkan frame")) { m_failed = true; return false; }
        if (!frame.tiles.empty()) compose(frame);
        transition(m_output.handle, m_output.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        m_output.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        const auto back = m_backbuffers[index];
        transition(back, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageBlit blit{};
        blit.srcSubresource = blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[1] = {static_cast<int>(frame.width), static_cast<int>(frame.height), 1};
        blit.dstOffsets[1] = {static_cast<int>(m_extent.width), static_cast<int>(m_extent.height), 1};
        vkCmdBlitImage(m_cmd, m_output.handle, m_output.layout, back,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
        transition(back, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                   VK_ACCESS_TRANSFER_WRITE_BIT, 0);
        if (!check(vkEndCommandBuffer(m_cmd), "End Vulkan frame")) { m_failed = true; return false; }

        const VkPipelineStageFlags wait = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = submit.signalSemaphoreCount = submit.commandBufferCount = 1;
        submit.pWaitSemaphores = &m_acquired;
        submit.pWaitDstStageMask = &wait;
        submit.pCommandBuffers = &m_cmd;
        submit.pSignalSemaphores = &m_finished;
        if (!check(vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE), "Submit Vulkan frame")) { m_failed = true; return false; }
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = present.swapchainCount = 1;
        present.pWaitSemaphores = &m_finished;
        present.pSwapchains = &m_swapchain;
        present.pImageIndices = &index;
        const auto status = vkQueuePresentKHR(m_queue, &present);
        m_recreate = status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_SUBOPTIMAL_KHR;
        if (!m_recreate && !check(status, "Present Vulkan frame")) { m_failed = true; return false; }
        return true;
    }

private:
    static constexpr VkFormat output_format = VK_FORMAT_R32G32B32A32_SFLOAT;
    static constexpr std::array<VkDescriptorType, 8> descriptor_types{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};

    void pick_device() {
        uint32_t count = 0;
        check(vkEnumeratePhysicalDevices(m_instance, &count, nullptr), "Enumerate Vulkan devices");
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(m_instance, &count, devices.data()), "Read Vulkan devices");
        for (auto device : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) continue;
            VkFormatProperties output{};
            vkGetPhysicalDeviceFormatProperties(device, output_format, &output);
            constexpr auto flags = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT;
            if ((output.optimalTilingFeatures & flags) != flags) continue;
            uint32_t families = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &families, nullptr);
            std::vector<VkQueueFamilyProperties> queues(families);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &families, queues.data());
            for (uint32_t i = 0; i < families; ++i) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present);
                constexpr auto required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
                if (present && queues[i].queueCount && (queues[i].queueFlags & required) == required) {
                    m_physical = device;
                    m_family = i;
                    return;
                }
            }
        }
        std::fprintf(stderr, "KerneTile Vulkan: No hardware Vulkan device with presentation\n");
        m_failed = true;
    }

    void create_pipeline() {
        std::array<VkDescriptorSetLayoutBinding, 8> bindings{};
        for (uint32_t i = 0; i < bindings.size(); ++i)
            bindings[i] = {i, descriptor_types[i], 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        set.bindingCount = static_cast<uint32_t>(bindings.size());
        set.pBindings = bindings.data();
        check(vkCreateDescriptorSetLayout(m_device, &set, nullptr, &m_set_layout), "Vulkan descriptor layout");
        const VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets = 1;
        pool.poolSizeCount = static_cast<uint32_t>(std::size(sizes));
        pool.pPoolSizes = sizes;
        check(vkCreateDescriptorPool(m_device, &pool, nullptr, &m_descriptor_pool), "Vulkan descriptor pool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = m_descriptor_pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &m_set_layout;
        check(vkAllocateDescriptorSets(m_device, &allocation, &m_descriptors), "Vulkan descriptors");
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &m_set_layout;
        check(vkCreatePipelineLayout(m_device, &layout, nullptr, &m_pipeline_layout), "Vulkan compute layout");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.minFilter = sampler.magFilter = VK_FILTER_LINEAR;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check(vkCreateSampler(m_device, &sampler, nullptr, &m_sampler), "Vulkan atlas sampler");
        VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        shader.codeSize = sizeof(ui_compute_spirv);
        shader.pCode = ui_compute_spirv;
        VkShaderModule module{};
        check(vkCreateShaderModule(m_device, &shader, nullptr, &module), "Vulkan compute shader");
        VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline.layout = m_pipeline_layout;
        pipeline.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline.stage.module = module;
        pipeline.stage.pName = "compose";
        const auto result = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &m_pipeline);
        vkDestroyShaderModule(m_device, module, nullptr);
        check(result, "Vulkan compute pipeline");
    }

    void resize(const Frame& frame) {
        const bool changed_size = frame.width != m_width || frame.height != m_height;
        VkSurfaceCapabilitiesKHR caps{};
        check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical, m_surface, &caps), "Vulkan surface capabilities");
        if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
            m_failed = true; return;
        }
        uint32_t count = 0;
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &count, nullptr), "List Vulkan formats");
        std::vector<VkSurfaceFormatKHR> formats(count);
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &count, formats.data()), "Read Vulkan formats");
        auto selected = std::find_if(formats.begin(), formats.end(), [&](auto format) {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(m_physical, format.format, &properties);
            return (format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
                (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);
        });
        if (selected == formats.end()) { m_failed = true; return; }
        m_extent = caps.currentExtent;
        if (m_extent.width == UINT32_MAX)
            m_extent = {std::clamp(frame.width, caps.minImageExtent.width, caps.maxImageExtent.width),
                        std::clamp(frame.height, caps.minImageExtent.height, caps.maxImageExtent.height)};
        if (!m_extent.width || !m_extent.height) { m_failed = true; return; }
        VkSwapchainCreateInfoKHR swap{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swap.surface = m_surface;
        swap.minImageCount = caps.maxImageCount ? std::min(caps.minImageCount + 1, caps.maxImageCount) : caps.minImageCount + 1;
        swap.imageFormat = selected->format;
        swap.imageColorSpace = selected->colorSpace;
        swap.imageExtent = m_extent;
        swap.imageArrayLayers = 1;
        swap.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swap.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swap.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : caps.currentTransform;
        const auto alpha = static_cast<uint32_t>(caps.supportedCompositeAlpha);
        swap.compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(alpha & (0u - alpha));
        swap.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swap.clipped = VK_TRUE;
        swap.oldSwapchain = m_swapchain;
        VkSwapchainKHR replacement{};
        check(vkCreateSwapchainKHR(m_device, &swap, nullptr, &replacement), "Create Vulkan swapchain");
        if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = replacement;
        check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr), "Count Vulkan backbuffers");
        m_backbuffers.resize(count);
        check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, m_backbuffers.data()), "Read Vulkan backbuffers");
        m_recreate = false;
        m_width = frame.width;
        m_height = frame.height;
        if (changed_size || !m_output.handle) {
            destroy(m_output);
            image(m_output, m_width, m_height, output_format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        }
    }

    VkDeviceMemory allocate(VkMemoryRequirements memory, VkMemoryPropertyFlags flags) {
        for (uint32_t i = 0; i < m_memory.memoryTypeCount; ++i) {
            if (!(memory.memoryTypeBits & (1u << i)) || (m_memory.memoryTypes[i].propertyFlags & flags) != flags) continue;
            VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            info.allocationSize = memory.size;
            info.memoryTypeIndex = i;
            VkDeviceMemory result{};
            check(vkAllocateMemory(m_device, &info, nullptr, &result), "Allocate Vulkan resource");
            return result;
        }
        m_failed = true;
        return VK_NULL_HANDLE;
    }

    void destroy(Buffer& buffer) {
        if (buffer.bytes) vkUnmapMemory(m_device, buffer.memory);
        vkDestroyBuffer(m_device, buffer.handle, nullptr);
        if (buffer.memory) vkFreeMemory(m_device, buffer.memory, nullptr);
        buffer = {};
    }
    void destroy(Image& image) {
        vkDestroyImageView(m_device, image.view, nullptr);
        vkDestroyImage(m_device, image.handle, nullptr);
        if (image.memory) vkFreeMemory(m_device, image.memory, nullptr);
        image = {};
    }

    void image(Image& out, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage) {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {width, height, 1};
        info.mipLevels = info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage;
        check(vkCreateImage(m_device, &info, nullptr, &out.handle), "Create Vulkan image");
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(m_device, out.handle, &requirements);
        out.memory = allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkBindImageMemory(m_device, out.handle, out.memory, 0), "Bind Vulkan image");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = out.handle;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(m_device, &view, nullptr, &out.view), "View Vulkan image");
    }

    Buffer& upload(size_t slot, VkBufferUsageFlags usage, size_t required) {
        auto& buffer = m_buffers[slot];
        if (buffer.size >= required) return buffer;
        destroy(buffer);
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = required;
        info.usage = usage;
        check(vkCreateBuffer(m_device, &info, nullptr, &buffer.handle), "Create Vulkan upload buffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(m_device, buffer.handle, &requirements);
        buffer.memory = allocate(requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkBindBufferMemory(m_device, buffer.handle, buffer.memory, 0), "Bind Vulkan upload buffer");
        check(vkMapMemory(m_device, buffer.memory, 0, VK_WHOLE_SIZE, 0, &buffer.bytes), "Map Vulkan upload buffer");
        buffer.size = required;
        return buffer;
    }

    void transition(VkImage image, VkImageLayout previous, VkImageLayout next,
                    VkAccessFlags from, VkAccessFlags to) {
        if (previous == next) return;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = previous == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : from;
        barrier.dstAccessMask = to;
        barrier.oldLayout = previous;
        barrier.newLayout = next;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(m_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void upload_atlas(const Frame& frame) {
        const bool first = !m_atlas.handle;
        if (first) image(m_atlas, frame.atlas_width, frame.atlas_height, VK_FORMAT_R8_UNORM,
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        if (!first && frame.atlas_updates.empty()) return;
        const atlas::RectU32 full{0, 0, frame.atlas_width, frame.atlas_height};
        const auto rects = first ? std::span(&full, 1) : frame.atlas_updates;
        size_t total = 0;
        std::vector<VkBufferImageCopy> regions;
        for (const auto& rect : rects) {
            VkBufferImageCopy region{};
            region.bufferOffset = total;
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageOffset = {static_cast<int>(rect.x), static_cast<int>(rect.y), 0};
            region.imageExtent = {rect.w, rect.h, 1};
            regions.push_back(region);
            total += (size_t(rect.w) * rect.h + 3) & ~size_t(3);
        }
        auto& buffer = upload(5, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, total);
        for (size_t i = 0; i < rects.size(); ++i) {
            const auto& rect = rects[i];
            auto* dest = static_cast<uint8_t*>(buffer.bytes) + regions[i].bufferOffset;
            for (uint32_t row = 0; row < rect.h; ++row) {
                const auto* source = frame.atlas.data() + size_t(rect.y + row) * frame.atlas_width + rect.x;
                std::memcpy(dest + size_t(row) * rect.w, source, rect.w);
            }
        }
        transition(m_atlas.handle, m_atlas.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        vkCmdCopyBufferToImage(m_cmd, buffer.handle, m_atlas.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(regions.size()), regions.data());
        m_atlas.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transition(m_atlas.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_atlas.layout,
                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    }

    void compose(const Frame& frame) {
        upload_atlas(frame);
        const auto count = static_cast<uint32_t>(frame.tiles.size());
        const auto groups_x = std::min(count, 1024u);
        std::array<uint32_t, 24> params{};
        params[0] = frame.width;
        params[1] = frame.height;
        params[2] = count;
        params[3] = groups_x;
        params[4] = static_cast<uint32_t>(frame.clips.size());
        params[5] = frame.mica_enabled ? 1u : 0u;
        for (size_t i = 0; i < 16; ++i) params[8 + i] = frame.mica_grid[i];
        const std::array<std::span<const std::byte>, 5> input{std::as_bytes(frame.primitives),
            std::as_bytes(frame.indices), std::as_bytes(frame.tiles), std::as_bytes(frame.clips),
            std::as_bytes(std::span(params))};
        std::array<VkDescriptorBufferInfo, 5> buffers{};
        std::array<VkWriteDescriptorSet, 8> writes{};
        for (uint32_t i = 0; i < writes.size(); ++i) {
            writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[i].dstSet = m_descriptors;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = descriptor_types[i];
        }
        for (size_t i = 0; i < input.size(); ++i) {
            const bool uniform = i == 4;
            const auto required = std::max<size_t>(input[i].size(), 4);
            auto& buffer = upload(i, uniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, required);
            if (!input[i].empty()) std::memcpy(buffer.bytes, input[i].data(), input[i].size());
            buffers[i] = {buffer.handle, 0, required};
            writes[uniform ? 7 : i].pBufferInfo = &buffers[i];
        }
        transition(m_output.handle, m_output.layout, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        m_output.layout = VK_IMAGE_LAYOUT_GENERAL;
        const VkDescriptorImageInfo atlas{VK_NULL_HANDLE, m_atlas.view, m_atlas.layout};
        const VkDescriptorImageInfo sampler{m_sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
        const VkDescriptorImageInfo output{VK_NULL_HANDLE, m_output.view, m_output.layout};
        writes[4].pImageInfo = &atlas;
        writes[5].pImageInfo = &sampler;
        writes[6].pImageInfo = &output;
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout, 0, 1, &m_descriptors, 0, nullptr);
        vkCmdDispatch(m_cmd, groups_x, (count + groups_x - 1) / groups_x, 1);
    }

    VkInstance m_instance{};
    VkSurfaceKHR m_surface{};
    VkPhysicalDevice m_physical{};
    VkPhysicalDeviceMemoryProperties m_memory{};
    VkDevice m_device{};
    VkQueue m_queue{};
    uint32_t m_family{};
    VkSwapchainKHR m_swapchain{};
    VkExtent2D m_extent{};
    std::vector<VkImage> m_backbuffers;
    VkCommandPool m_pool{};
    VkCommandBuffer m_cmd{};
    VkSemaphore m_acquired{}, m_finished{};
    VkDescriptorSetLayout m_set_layout{};
    VkDescriptorPool m_descriptor_pool{};
    VkDescriptorSet m_descriptors{};
    VkPipelineLayout m_pipeline_layout{};
    VkPipeline m_pipeline{};
    VkSampler m_sampler{};
    Image m_atlas, m_output;
    std::array<Buffer, 6> m_buffers{};
    uint32_t m_width{}, m_height{};
    bool m_failed = false, m_recreate = false;
};
} // namespace

std::unique_ptr<Backend> create(void* window, void* display) {
#if defined(__ANDROID__)
    if (!window) return nullptr;
#else
    if (!window || !display) return nullptr;
#endif
    auto backend = std::make_unique<VulkanBackend>();
    backend->setup(window, display);
    return backend->ready() ? std::move(backend) : nullptr;
}
} // namespace kernetile::compute
