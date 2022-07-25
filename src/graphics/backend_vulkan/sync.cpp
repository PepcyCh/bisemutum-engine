#include "sync.hpp"

#include "device.hpp"
#include "queue.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

FenceVulkan::FenceVulkan(DeviceVulkan *device) {
    device_ = device;
    VkFenceCreateInfo fenc_ci {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    vkCreateFence(device_->Raw(), &fenc_ci, nullptr, &fence_);
}

FenceVulkan::~FenceVulkan() {
    vkDestroyFence(device_->Raw(), fence_, nullptr);
}

void FenceVulkan::Reset() {
    vkResetFences(device_->Raw(), 1, &fence_);
}

void FenceVulkan::SignalOn(const Queue *queue) {
    auto queue_vk = static_cast<const QueueVulkan *>(queue);
    vkQueueSubmit(queue_vk->Raw(), 0, nullptr, fence_);
}

void FenceVulkan::Wait(uint64_t timeout) {
    vkWaitForFences(device_->Raw(), 1, &fence_, VK_FALSE, timeout);
}

bool FenceVulkan::IsFinished() {
    return vkGetFenceStatus(device_->Raw(), fence_) == VK_SUCCESS;
}

SemaphoreVulkan::SemaphoreVulkan(DeviceVulkan *device) {
    device_ = device;
    VkSemaphoreCreateInfo semaphore_ci {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    vkCreateSemaphore(device_->Raw(), &semaphore_ci, nullptr, &semaphore_);
}

SemaphoreVulkan::~SemaphoreVulkan() {
    vkDestroySemaphore(device_->Raw(), semaphore_, nullptr);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
