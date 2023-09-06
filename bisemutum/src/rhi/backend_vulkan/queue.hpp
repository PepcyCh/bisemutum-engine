#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/rhi/queue.hpp>

namespace bi::rhi {

struct QueueVulkan final : Queue {
    QueueVulkan(Ref<struct DeviceVulkan> device, uint32_t family_index);
    ~QueueVulkan() override;

    auto wait_idle() const -> void override;

    auto submit_command_buffer(
        CSpan<Box<CommandBuffer>> cmd_buffers,
        CSpan<CRef<Semaphore>> wait_semaphores,
        CSpan<CRef<Semaphore>> signal_semaphores,
        Option<Ref<Fence>> signal_fence
    ) const -> void override;

    auto raw() const -> VkQueue { return queue_; }

    auto raw_family_index() const -> uint32_t { return family_index_; }

private:
    Ref<DeviceVulkan> device_;
    VkQueue queue_;
    uint32_t family_index_;
};

}
