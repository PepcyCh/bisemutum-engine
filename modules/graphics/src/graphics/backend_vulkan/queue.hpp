#pragma once

#include <volk.h>

#include "graphics/queue.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class QueueVulkan final : public Queue {
public:
    QueueVulkan(Ref<class DeviceVulkan> device, uint32_t family_index);
    ~QueueVulkan() override;

    void WaitIdle() const override;

    void SubmitCommandBuffer(Span<Ptr<CommandBuffer>> &&cmd_buffers, Span<Ref<Semaphore>> wait_semaphores = {},
        Span<Ref<Semaphore>> signal_semaphores = {}, Fence *signal_fence = nullptr) const override;

    VkQueue Raw() const { return queue_; }

    uint32_t RawFamilyIndex() const { return family_index_; }

private:
    Ref<DeviceVulkan> device_;
    VkQueue queue_;
    uint32_t family_index_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
