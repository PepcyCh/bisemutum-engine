#include "swap_chain.hpp"

#include "core/logger.hpp"
#include "utils.hpp"
#include "device.hpp"
#include "sync.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

VkImageUsageFlags ToVkImageUsage(BitFlags<TextureUsage> usage) {
    VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usage.Contains(TextureUsage::eSampled)) {
        vk_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (usage.Contains(TextureUsage::eStorage)) {
        vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (usage.Contains(TextureUsage::eDepthStencilAttachment)) {
        vk_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    return vk_usage;
}

}

SwapChainVulkan::SwapChainVulkan(Ref<DeviceVulkan> device, const SwapChainDesc &desc)
    : device_(device), queue_(desc.queue.CastTo<QueueVulkan>()), usages_(desc.usages) {
    surface_ = device_->RawSurface();

    VkSurfaceCapabilitiesKHR surface_caps {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->RawPhysicalDevice(), surface_, &surface_caps);
    swap_chain_ = VK_NULL_HANDLE;

    CreateSwapChain(desc.width, desc.height, surface_caps.currentTransform);

    BI_INFO(gGraphicsLogger, "Swapchain created with {} textures, {} x {}", num_textures_, width_, height_);
}

SwapChainVulkan::~SwapChainVulkan() {
    vkDestroySwapchainKHR(device_->Raw(), swap_chain_, nullptr);
}

void SwapChainVulkan::CreateSwapChain(uint32_t width, uint32_t height, VkSurfaceTransformFlagBitsKHR transform) {
    uint32_t queue_family_index = queue_->RawFamilyIndex();
    VkSwapchainCreateInfoKHR swap_chain_ci {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = device_->RawSurface(),
        .minImageCount = kNumSwapChainBuffers,
        .imageFormat = device_->RawSurfaceFormat(),
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = { width, height },
        .imageArrayLayers = 1,
        .imageUsage = ToVkImageUsage(usages_),
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queue_family_index,
        .preTransform = transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .oldSwapchain = swap_chain_,
    };
    vkCreateSwapchainKHR(device_->Raw(), &swap_chain_ci, nullptr, &swap_chain_);

    vkGetSwapchainImagesKHR(device_->Raw(), swap_chain_, &num_textures_, nullptr);
    Vec<VkImage> images(num_textures_);
    vkGetSwapchainImagesKHR(device_->Raw(), swap_chain_, &num_textures_, images.data());
    TextureDesc texture_desc {
        .name = "",
        .extent = { width, height },
        .levels = 1,
        .format = FromVkFormat(device_->RawSurfaceFormat()),
        .dim = TextureDimension::e2D,
        .usages = TextureUsage::eColorAttachment,
    };
    textures_.resize(num_textures_);
    for (uint32_t i = 0; i < num_textures_; i++) {
        textures_[i] = Ptr<TextureVulkan>::Make(device_, images[i], texture_desc);
    }

    width_ = width;
    height_ = height;
    transform_ = transform;
}

void SwapChainVulkan::Resize(uint32_t width, uint32_t height) {
    if (width != width_ || height != height_) {
        CreateSwapChain(width, height, transform_);
        
        BI_INFO(gGraphicsLogger, "Swapchain resized to {} x {} with {} textures", width_, height_, kNumSwapChainBuffers);
    }
}

bool SwapChainVulkan::AcquireNextTexture(Ref<Semaphore> acquired_semaphore) {
    auto acquired_semaphore_vk = static_cast<SemaphoreVulkan *>(acquired_semaphore.Get())->Raw();

    VkResult result = vkAcquireNextImageKHR(device_->Raw(), swap_chain_, ~0ull,
        acquired_semaphore_vk, VK_NULL_HANDLE, &curr_texture_);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
        VkSurfaceCapabilitiesKHR surface_caps {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->RawPhysicalDevice(), surface_, &surface_caps);
        if (surface_caps.currentExtent.width == ~0u) {
            return false;
        }

        if (surface_caps.currentExtent.width != width_ || surface_caps.currentExtent.height != height_
            || result == VK_ERROR_OUT_OF_DATE_KHR) {
            queue_->WaitIdle();

            CreateSwapChain(surface_caps.currentExtent.width, surface_caps.currentExtent.height,
                surface_caps.currentTransform);

            result = vkAcquireNextImageKHR(device_->Raw(), swap_chain_, ~0ull,
                acquired_semaphore_vk, VK_NULL_HANDLE, &curr_texture_);
        }
    }
    if (result != VK_SUCCESS) {
        return false;
    }

    return true;
}

void SwapChainVulkan::Present(Span<Ref<Semaphore>> wait_semaphores) {
    Vec<VkSemaphore> wait_semaphores_vk(wait_semaphores.Size());
    for (size_t i = 0; i < wait_semaphores_vk.size(); i++) {
        wait_semaphores_vk[i] = wait_semaphores[i].CastTo<SemaphoreVulkan>()->Raw();
    }

    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores_vk.size()),
        .pWaitSemaphores = wait_semaphores_vk.data(),
        .swapchainCount = 1,
        .pSwapchains = &swap_chain_,
        .pImageIndices = &curr_texture_,
        .pResults = nullptr,
    };
    VkResult result = vkQueuePresentKHR(queue_->Raw(), &present_info);
    // TODO - present result
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
