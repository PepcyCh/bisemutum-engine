#include "swapchain.hpp"

#include "volk.h"
#include "utils.hpp"
#include "device.hpp"
#include "sync.hpp"

namespace bi::rhi {

SwapchainVulkan::SwapchainVulkan(Ref<DeviceVulkan> device, SwapchainDesc const& desc)
    : device_(device), queue_(desc.queue.cast_to<QueueVulkan>())
{
    create_surface(desc);

    VkSurfaceCapabilitiesKHR surface_caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->raw_physical_device(), surface_, &surface_caps);
    swapchain_ = VK_NULL_HANDLE;

    create_swapchain(
        surface_caps.currentExtent.width, surface_caps.currentExtent.height, surface_caps.currentTransform
    );
}

SwapchainVulkan::~SwapchainVulkan() {
    if (swapchain_) {
        vkDestroySwapchainKHR(device_->raw(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    if (surface_) {
        vkDestroySurfaceKHR(device_->raw_instance(), surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
}

auto SwapchainVulkan::create_surface(SwapchainDesc const& desc) -> void {
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surface_ci{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = GetModuleHandle(nullptr),
        .hwnd = reinterpret_cast<HWND>(desc.window_handle.win32_window),
    };
    vkCreateWin32SurfaceKHR(device_->raw_instance(), &surface_ci, nullptr, &surface_);
#else
    VkXcbSurfaceCreateInfoKHR surface_ci{
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .connection = reinterpret_cast<xcb_connection_t*>(desc.window_handle.xcb_connection),
        .window = desc.window_handle.xcb_window,
    };
    vkCreateXcbSurfaceKHR(device_->raw_instance(), &surface_ci, nullptr, &surface_);
#endif

    uint32_t num_available_formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        device_->raw_physical_device(), surface_, &num_available_formats, nullptr
    );
    std::vector<VkSurfaceFormatKHR> available_formats(num_available_formats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        device_->raw_physical_device(), surface_, &num_available_formats, available_formats.data()
    );
    constexpr VkFormat surface_formats[]{
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
    };
    for (auto format : surface_formats) {
        bool found = false;
        for (auto const& available_format : available_formats) {
            if (available_format.format == format) {
                surface_format_ = format;
                found = true;
                break;
            }
        }
        if (found) {
            break;
        }
    }

    uint32_t num_available_presents;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device_->raw_physical_device(), surface_, &num_available_presents, nullptr
    );
    std::vector<VkPresentModeKHR> available_presents(num_available_presents);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device_->raw_physical_device(), surface_, &num_available_presents, available_presents.data()
    );
    constexpr VkPresentModeKHR presents[]{
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
    };
    for (auto present : presents) {
        bool found = false;
        for (auto available_present : available_presents) {
            if (available_present == present) {
                found = true;
                present_mode_ = present;
                break;
            }
        }
        if (found) {
            break;
        }
    }
}

void SwapchainVulkan::create_swapchain(uint32_t width, uint32_t height, VkSurfaceTransformFlagBitsKHR transform) {
    uint32_t queue_family_index = queue_->raw_family_index();
    auto old_swapchain = swapchain_;
    VkSwapchainCreateInfoKHR swapchain_ci{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
        .minImageCount = 3,
        .imageFormat = surface_format_,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {width, height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queue_family_index,
        .preTransform = transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode_,
        .oldSwapchain = old_swapchain,
    };
    vkCreateSwapchainKHR(device_->raw(), &swapchain_ci, nullptr, &swapchain_);
    if (old_swapchain) {
        vkDestroySwapchainKHR(device_->raw(), old_swapchain, nullptr);
    }

    vkGetSwapchainImagesKHR(device_->raw(), swapchain_, &num_textures_, nullptr);
    std::vector<VkImage> images(num_textures_);
    vkGetSwapchainImagesKHR(device_->raw(), swapchain_, &num_textures_, images.data());
    TextureDesc texture_desc{
        .extent = {width, height},
        .levels = 1,
        .format = from_vk_format(surface_format_),
        .dim = TextureDimension::d2,
        .usages = TextureUsage::color_attachment,
    };
    textures_.resize(num_textures_);
    for (uint32_t i = 0; i < num_textures_; i++) {
        textures_[i] = Box<TextureVulkan>::make(device_, images[i], texture_desc);
    }

    width_ = width;
    height_ = height;
    transform_ = transform;
}

void SwapchainVulkan::resize(uint32_t width, uint32_t height) {
    if (width != width_ || height != height_) {
        create_swapchain(width, height, transform_);
    }
}

auto SwapchainVulkan::acquire_next_texture(Ref<Semaphore> acquired_semaphore) -> bool {
    auto acquired_semaphore_vk = acquired_semaphore.cast_to<SemaphoreVulkan>()->raw();
    auto result = vkAcquireNextImageKHR(
        device_->raw(), swapchain_, ~0ull, acquired_semaphore_vk, VK_NULL_HANDLE, &curr_texture_
    );
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
        VkSurfaceCapabilitiesKHR surface_caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->raw_physical_device(), surface_, &surface_caps);
        if (surface_caps.currentExtent.width == ~0u) {
            return false;
        }

        if (
            surface_caps.currentExtent.width != width_ || surface_caps.currentExtent.height != height_
            || result == VK_ERROR_OUT_OF_DATE_KHR
        ) {
            queue_->wait_idle();

            create_swapchain(
                surface_caps.currentExtent.width, surface_caps.currentExtent.height,
                surface_caps.currentTransform
            );

            result = vkAcquireNextImageKHR(
                device_->raw(), swapchain_, ~0ull, acquired_semaphore_vk, VK_NULL_HANDLE, &curr_texture_
            );
        }
    }
    if (result != VK_SUCCESS) {
        return false;
    }

    return true;
}

auto SwapchainVulkan::present(CSpan<Ref<Semaphore>> wait_semaphores) -> void {
    std::vector<VkSemaphore> wait_semaphores_vk(wait_semaphores.size());
    for (size_t i = 0; i < wait_semaphores_vk.size(); i++) {
        wait_semaphores_vk[i] = wait_semaphores[i].cast_to<SemaphoreVulkan>()->raw();
    }

    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores_vk.size()),
        .pWaitSemaphores = wait_semaphores_vk.data(),
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &curr_texture_,
        .pResults = nullptr,
    };
    auto result = vkQueuePresentKHR(queue_->raw(), &present_info);
    // TODO - check present result
}

auto SwapchainVulkan::format() const -> ResourceFormat {
    return from_vk_format(surface_format_);
}

}
