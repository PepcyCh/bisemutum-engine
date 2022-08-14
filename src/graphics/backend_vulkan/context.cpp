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

    descriptor_pool_ = Ptr<DescriptorPoolVulkan>::Make(device, DescriptorPoolSizesVulkan::kDefault);
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

DescriptorSetVulkan *FrameContextVulkan::GetDescriptorSet(RenderPipelineVulkan *pipeline, uint32_t set_index) {
    auto key = std::make_pair(pipeline, set_index);
    if (auto it = render_descriptor_sets_.find(key); it != render_descriptor_sets_.end()) {
        return &it->second;
    }

    DescriptorSetVulkan set = descriptor_pool_->AllocateSet(pipeline, set_index);
    auto it = render_descriptor_sets_.insert({key, set}).first;
    return &it->second;
}

DescriptorSetVulkan *FrameContextVulkan::GetDescriptorSet(ComputePipelineVulkan *pipeline, uint32_t set_index) {
    auto key = std::make_pair(pipeline, set_index);
    if (auto it = compute_descriptor_sets_.find(key); it != compute_descriptor_sets_.end()) {
        return &it->second;
    }

    DescriptorSetVulkan set = descriptor_pool_->AllocateSet(pipeline, set_index);
    auto it = compute_descriptor_sets_.insert({key, set}).first;
    return &it->second;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
