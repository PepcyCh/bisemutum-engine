#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/rhi/swapchain.hpp>

#include "resource.hpp"
#include "queue.hpp"

namespace bi::rhi {

struct SwapchainVulkan final : Swapchain {
    SwapchainVulkan(Ref<struct DeviceVulkan> device, SwapchainDesc const& desc);
    ~SwapchainVulkan();

    auto resize(uint32_t width, uint32_t height) -> void override;

    auto acquire_next_texture(Ref<Semaphore> acquired_semaphore) -> bool override;

    auto present(CSpan<Ref<Semaphore>> wait_semaphores) -> void override;

    auto current_texture() -> Ref<Texture> override { return textures_[curr_texture_].ref(); }

    auto format() const -> ResourceFormat override;

    auto raw() const -> VkSwapchainKHR { return swapchain_; }

private:
    auto create_surface(SwapchainDesc const& desc) -> void;

    auto create_swapchain(uint32_t width, uint32_t height, VkSurfaceTransformFlagBitsKHR transform) -> void;

    Ref<DeviceVulkan> device_;
    Ref<QueueVulkan> queue_;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkFormat surface_format_ = VK_FORMAT_B8G8R8A8_SRGB;
    VkPresentModeKHR present_mode_ = VK_PRESENT_MODE_FIFO_KHR;

    uint32_t width_;
    uint32_t height_;
    VkSurfaceTransformFlagBitsKHR transform_;

    uint32_t num_textures_;
    uint32_t curr_texture_ = 0;
    std::vector<Box<TextureVulkan>> textures_;
};

}
