#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "graphics/device.hpp"
#include "queue.hpp"
#include "sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class DeviceVulkan : public Device, public RefFromThis<DeviceVulkan> {
public:
    DeviceVulkan(const DeviceDesc &desc);
    ~DeviceVulkan() override;

    static Ptr<DeviceVulkan> Create(const DeviceDesc &desc);

    Ptr<Queue> GetQueue(QueueType type) override;

    Ptr<SwapChain> CreateSwapChain(Ref<Queue> queue, uint32_t width, uint32_t height) override;

    Ptr<Fence> CreateFence() override;

    Ptr<Semaphore> CreateSemaphore() override;

    Ptr<Buffer> CreateBuffer(const BufferDesc &desc) override;

    Ptr<Texture> CreateTexture(const TextureDesc &desc) override;

    Ptr<Sampler> CreateSampler(const SamplerDesc &desc) override;

    Ptr<ShaderModule> CreateShaderModule(const Vec<uint8_t> &src_bytes) override;

    Ptr<RenderPipeline> CreateRenderPipeline(const RenderPipelineDesc &desc) override;

    Ptr<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc &desc) override;

    Ptr<FrameContext> CreateFrameContext() override;

    VkDevice Raw() const { return device_; }
    VkPhysicalDevice RawPhysicalDevice() const { return physical_device_; }

    VmaAllocator Allocator() const { return allocator_; }

    VkSurfaceKHR RawSurface() const { return surface_; }
    VkFormat RawSurfaceFormat() const { return surface_format_; }

private:
    void PickDevice(const DeviceDesc &desc);
    void InitializeAllocator();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physical_device_props_;
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_;

    VkDebugUtilsMessengerEXT debug_utils_messenger_ = VK_NULL_HANDLE;

    uint32_t queue_family_indices[3];

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkFormat surface_format_ = VK_FORMAT_B8G8R8A8_SRGB;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
