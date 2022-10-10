#include "queue.hpp"

#include "device.hpp"
#include "command.hpp"
#include "sync.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

QueueVulkan::QueueVulkan(Ref<DeviceVulkan> device, uint32_t family_index) : device_(device) {
    family_index_ = family_index;
    vkGetDeviceQueue(device_->Raw(), family_index, 0, &queue_);
}

QueueVulkan::~QueueVulkan() {
}

void QueueVulkan::WaitIdle() const {
    vkQueueWaitIdle(queue_);
}

void QueueVulkan::SubmitCommandBuffer(Span<Ptr<CommandBuffer>> &&cmd_buffers, Span<Ref<Semaphore>> wait_semaphores,
    Span<Ref<Semaphore>> signal_semaphores, Fence *signal_fence) const {
    Vec<VkCommandBuffer> cmd_buffers_vk(cmd_buffers.Size());
    for (size_t i = 0; i < cmd_buffers.Size(); i++) {
        cmd_buffers_vk[i] = cmd_buffers[i].AsRef().CastTo<CommandBufferVulkan>()->Raw();
    }

    Vec<VkSemaphore> wait_semaphores_vk(wait_semaphores.Size());
    Vec<VkPipelineStageFlags> wait_stages(wait_semaphores.Size());
    for (size_t i = 0; i < wait_semaphores.Size(); i++) {
        wait_semaphores_vk[i] = wait_semaphores[i].CastTo<SemaphoreVulkan>()->Raw();
        wait_stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
    Vec<VkSemaphore> signal_semaphores_vk(wait_semaphores.Size());
    for (size_t i = 0; i < signal_semaphores.Size(); i++) {
        signal_semaphores_vk[i] = signal_semaphores[i].CastTo<SemaphoreVulkan>()->Raw();
    }
    VkFence signal_fence_vk = VK_NULL_HANDLE;
    if (signal_fence) {
        auto fence_vk = static_cast<FenceVulkan *>(signal_fence);
        signal_fence_vk = fence_vk->Raw();
        fence_vk->SetSignaled();
    }

    VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores_vk.size()),
        .pWaitSemaphores = wait_semaphores_vk.data(),
        .pWaitDstStageMask = wait_stages.data(),
        .commandBufferCount = static_cast<uint32_t>(cmd_buffers_vk.size()),
        .pCommandBuffers = cmd_buffers_vk.data(),
        .signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.Size()),
        .pSignalSemaphores = signal_semaphores_vk.data(),
    };
    vkQueueSubmit(queue_, 1, &submit_info, signal_fence_vk);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
