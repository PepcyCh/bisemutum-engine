#include "sync.hpp"

#include "volk.h"
#include "device.hpp"
#include "queue.hpp"

namespace bi::rhi {

FenceVulkan::FenceVulkan(Ref<DeviceVulkan> device) : device_(device) {
    VkFenceCreateInfo fenc_ci {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    vkCreateFence(device_->raw(), &fenc_ci, nullptr, &fence_);
}

FenceVulkan::~FenceVulkan() {
    if (fence_) {
        vkDestroyFence(device_->raw(), fence_, nullptr);
        fence_ = VK_NULL_HANDLE;
    }
}

void FenceVulkan::reset() {
    vkResetFences(device_->raw(), 1, &fence_);
}

void FenceVulkan::signal_on(Ref<const Queue> queue) {
    if (signaled_) {
        return;
    }
    auto queue_vk = queue.cast_to<const QueueVulkan>();
    vkQueueSubmit(queue_vk->raw(), 0, nullptr, fence_);
    signaled_ = true;
}

void FenceVulkan::wait(uint64_t timeout) {
    if (signaled_) {
        vkWaitForFences(device_->raw(), 1, &fence_, VK_FALSE, timeout);
        vkResetFences(device_->raw(), 1, &fence_);
        signaled_ = false;
    }
}

bool FenceVulkan::is_finished() {
    return vkGetFenceStatus(device_->raw(), fence_) == VK_SUCCESS;
}

SemaphoreVulkan::SemaphoreVulkan(Ref<DeviceVulkan> device) : device_(device) {
    VkSemaphoreCreateInfo semaphore_ci {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    vkCreateSemaphore(device_->raw(), &semaphore_ci, nullptr, &semaphore_);
}

SemaphoreVulkan::~SemaphoreVulkan() {
    if (semaphore_) {
        vkDestroySemaphore(device_->raw(), semaphore_, nullptr);
        semaphore_ = VK_NULL_HANDLE;
    }
}

}
