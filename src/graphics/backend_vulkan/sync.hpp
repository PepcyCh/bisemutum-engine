#pragma once

#include <volk.h>

#include "core/ptr.hpp"
#include "graphics/sync.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class FenceVulkan : public Fence {
public:
    FenceVulkan(Ref<class DeviceVulkan> device);
    ~FenceVulkan() override;

    void Reset() override;

    void SignalOn(const Queue *queue) override;

    void Wait(uint64_t timeout = ~0ull) override;

    bool IsFinished() override;

    VkFence Raw() const { return fence_; }

private:
    Ref<DeviceVulkan> device_;
    VkFence fence_;
};

class SemaphoreVulkan : public Semaphore {
public:
    SemaphoreVulkan(Ref<class DeviceVulkan> device);
    ~SemaphoreVulkan() override;

    VkSemaphore Raw() const { return semaphore_; }

private:
    Ref<DeviceVulkan> device_;
    VkSemaphore semaphore_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
