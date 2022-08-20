#include "context.hpp"

#include "device.hpp"
#include "command.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

FrameContextVulkan::FrameContextVulkan(Ref<DeviceVulkan> device) : device_(device) {
    VkCommandPoolCreateInfo graphics_command_pool_ci {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = device->GetQueue(QueueType::eGraphics).AsRef().CastTo<QueueVulkan>()->RawFamilyIndex(),
    };
    vkCreateCommandPool(device->Raw(), &graphics_command_pool_ci, nullptr, &graphics_command_pool_);

    descriptor_pool_ = Ptr<DescriptorSetPoolVulkan>::Make(device, DescriptorPoolSizesVulkan::kDefault);
}

FrameContextVulkan::~FrameContextVulkan() {
    vkDestroyCommandPool(device_->Raw(), graphics_command_pool_, nullptr);
}

void FrameContextVulkan::Reset() {
    vkResetCommandPool(device_->Raw(), graphics_command_pool_, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

Ptr<CommandEncoder> FrameContextVulkan::GetCommandEncoder(QueueType queue) {
    VkCommandBufferAllocateInfo command_buffer_ci {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = graphics_command_pool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device_->Raw(), &command_buffer_ci, &command_buffer);
    return Ptr<CommandEncoderVulkan>::Make(device_, RefThis(), command_buffer);
}

VkDescriptorSet FrameContextVulkan::GetDescriptorSet(VkDescriptorSetLayout layout_vk, const DescriptorSetLayout &layout,
    const ShaderParams &values) {
    if (auto it = descriptor_sets_.find(values); it != descriptor_sets_.end()) {
        return it->second;
    }
    auto descriptor_set = descriptor_pool_->AllocateAndWriteSet(layout_vk, layout, values);
    descriptor_sets_.insert({values, descriptor_set});
    return descriptor_set;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
