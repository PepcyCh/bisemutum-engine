#include "queue.hpp"

#include "volk.h"
#include "device.hpp"
#include "command.hpp"
#include "sync.hpp"

namespace bi::rhi {

QueueVulkan::QueueVulkan(Ref<DeviceVulkan> device, uint32_t family_index) : device_(device) {
    family_index_ = family_index;
    vkGetDeviceQueue(device_->raw(), family_index, 0, &queue_);
}

QueueVulkan::~QueueVulkan() {
    queue_ = VK_NULL_HANDLE;
}

void QueueVulkan::wait_idle() const {
    vkQueueWaitIdle(queue_);
}

auto QueueVulkan::submit_command_buffer(
    CSpan<Box<CommandBuffer>> cmd_buffers,
    CSpan<CRef<Semaphore>> wait_semaphores,
    CSpan<CRef<Semaphore>> signal_semaphores,
    Option<Ref<Fence>> signal_fence
) const -> void {
    std::vector<VkCommandBuffer> cmd_buffers_vk(cmd_buffers.size());
    for (size_t i = 0; i < cmd_buffers.size(); i++) {
        cmd_buffers_vk[i] = cmd_buffers[i].ref().cast_to<CommandBufferVulkan>()->raw();
    }

    std::vector<VkSemaphore> wait_semaphores_vk(wait_semaphores.size());
    std::vector<VkPipelineStageFlags> wait_stages(wait_semaphores.size());
    for (size_t i = 0; i < wait_semaphores.size(); i++) {
        wait_semaphores_vk[i] = wait_semaphores[i].cast_to<const SemaphoreVulkan>()->raw();
        wait_stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
    std::vector<VkSemaphore> signal_semaphores_vk(wait_semaphores.size());
    for (size_t i = 0; i < signal_semaphores.size(); i++) {
        signal_semaphores_vk[i] = signal_semaphores[i].cast_to<const SemaphoreVulkan>()->raw();
    }
    VkFence signal_fence_vk = VK_NULL_HANDLE;
    if (signal_fence) {
        auto fence_vk = signal_fence.value().cast_to<FenceVulkan>();
        signal_fence_vk = fence_vk->raw();
        fence_vk->set_signaled();
    }

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores_vk.size()),
        .pWaitSemaphores = wait_semaphores_vk.data(),
        .pWaitDstStageMask = wait_stages.data(),
        .commandBufferCount = static_cast<uint32_t>(cmd_buffers_vk.size()),
        .pCommandBuffers = cmd_buffers_vk.data(),
        .signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size()),
        .pSignalSemaphores = signal_semaphores_vk.data(),
    };
    vkQueueSubmit(queue_, 1, &submit_info, signal_fence_vk);
}

}
