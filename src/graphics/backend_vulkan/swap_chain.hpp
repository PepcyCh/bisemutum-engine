#pragma once

#include <volk.h>

#include "graphics/swap_chain.hpp"
#include "resource.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class SwapChainVulkan : public SwapChain {
public:
    SwapChainVulkan(Ref<class DeviceVulkan> device, Ref<class QueueVulkan> queue, uint32_t width, uint32_t height);
    ~SwapChainVulkan();

    void Resize(uint32_t width, uint32_t height) override;

    bool AcquireNextTexture(Ref<Semaphore> acquired_semaphore) override;

    void Present(Span<Ref<Semaphore>> wait_semaphores) override;

    Ref<Texture> GetCurrentTexture() override { return textures_[curr_texture_].AsRef(); }

    VkSwapchainKHR Raw() const { return swap_chain_; }

private:
    void CreateSwapChain(uint32_t width, uint32_t height, VkSurfaceTransformFlagBitsKHR transform);

    Ref<DeviceVulkan> device_;
    Ref<QueueVulkan> queue_;
    
    VkSwapchainKHR swap_chain_;
    VkSurfaceKHR surface_;

    uint32_t width_;
    uint32_t height_;
    VkSurfaceTransformFlagBitsKHR transform_;

    uint32_t num_textures_;
    uint32_t curr_texture_ = 0;
    Vec<Ptr<TextureVulkan>> textures_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
