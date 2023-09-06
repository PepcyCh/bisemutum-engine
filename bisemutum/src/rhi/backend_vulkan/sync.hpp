#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/prelude/ref.hpp>
#include <bisemutum/rhi/sync.hpp>

namespace bi::rhi {

struct DeviceVulkan;

struct FenceVulkan final : Fence {
    FenceVulkan(Ref<DeviceVulkan> device);
    ~FenceVulkan() override;

    auto reset() -> void override;

    auto signal_on(Ref<const Queue> queue) -> void override;

    auto wait(uint64_t timeout) -> void override;

    auto is_finished() -> bool override;

    auto raw() const -> VkFence { return fence_; }

    auto set_signaled() -> void { signaled_ = true; }

private:
    Ref<DeviceVulkan> device_;
    VkFence fence_ = VK_NULL_HANDLE;
    bool signaled_ = false;
};

struct SemaphoreVulkan final : Semaphore {
    SemaphoreVulkan(Ref<DeviceVulkan> device);
    ~SemaphoreVulkan() override;

    auto raw() const -> VkSemaphore { return semaphore_; }

private:
    Ref<DeviceVulkan> device_;
    VkSemaphore semaphore_ = VK_NULL_HANDLE;
};

}
