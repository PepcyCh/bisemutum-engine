#pragma once

#include "graphics/context.hpp"
#include "descriptor.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class FrameContextVulkan : public FrameContext, public RefFromThis<FrameContextVulkan> {
public:
    FrameContextVulkan(Ref<class DeviceVulkan> device);
    ~FrameContextVulkan() override;

    void Reset() override;

    Ptr<class CommandEncoder> GetCommandEncoder(QueueType queue = QueueType::eGraphics) override;

    // DescriptorSetVulkan *GetDescriptorSet(RenderPipelineVulkan *pipeline, uint32_t set_index);
    // DescriptorSetVulkan *GetDescriptorSet(ComputePipelineVulkan *pipeline, uint32_t set_index);

    VkDescriptorSet GetDescriptorSet(VkDescriptorSetLayout layout_vk, const DescriptorSetLayout &layout,
        const ShaderParams &values);

private:
    Ref<DeviceVulkan> device_;

    VkCommandPool graphics_command_pool_;

    Ptr<DescriptorSetPoolVulkan> descriptor_pool_;
    HashMap<ShaderParams, VkDescriptorSet> descriptor_sets_;

    // HashMap<std::pair<RenderPipelineVulkan *, uint32_t>, DescriptorSetVulkan> render_descriptor_sets_;
    // HashMap<std::pair<ComputePipelineVulkan *, uint32_t>, DescriptorSetVulkan> compute_descriptor_sets_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
