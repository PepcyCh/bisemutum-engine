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
    available_command_buffer_index_ = 0;

    descriptor_pool_ = Ptr<DescriptorSetPoolVulkan>::Make(device, DescriptorPoolSizesVulkan::kDefault);
}

FrameContextVulkan::~FrameContextVulkan() {
    vkDestroyCommandPool(device_->Raw(), graphics_command_pool_, nullptr);
}

void FrameContextVulkan::Reset() {
    vkResetCommandPool(device_->Raw(), graphics_command_pool_, 0);
    available_command_buffer_index_ = 0;
}

Ptr<CommandEncoder> FrameContextVulkan::GetCommandEncoder(QueueType queue) {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    if (available_command_buffer_index_ < allocated_command_buffers_.size()) {
        command_buffer = allocated_command_buffers_[available_command_buffer_index_];
        ++available_command_buffer_index_;
    } else {
        VkCommandBufferAllocateInfo command_buffer_ci {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = graphics_command_pool_,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        vkAllocateCommandBuffers(device_->Raw(), &command_buffer_ci, &command_buffer);

        allocated_command_buffers_.push_back(command_buffer);
        ++available_command_buffer_index_;
    }

    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(command_buffer, &begin_info);

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
