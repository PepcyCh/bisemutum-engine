#pragma once

#include "graphics/pipeline.hpp"
#include "shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class RenderPipelineVulkan : public RenderPipeline {
public:
    RenderPipelineVulkan(Ref<class DeviceVulkan> device, const RenderPipelineDesc &desc);
    ~RenderPipelineVulkan() override;

    void SetTargetFormats(Span<ResourceFormat> color_formats, ResourceFormat depth_stencil_format);

    const RenderPipelineDesc &Desc() const { return desc_; }

    VkPipeline RawPipeline() const { return pipeline_; }

    VkPipelineLayout RawPipelineLayout() const { return pipeline_layout_; }

    VkDescriptorSetLayout RawSetLayout(uint32_t set_index) const { return set_layouts_[set_index]; }

private:
    Ref<DeviceVulkan> device_;
    RenderPipelineDesc desc_;

    Vec<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;
};

class ComputePipelineVulkan : public ComputePipeline {
public:
    ComputePipelineVulkan(Ref<class DeviceVulkan> device, const ComputePipelineDesc &desc);
    ~ComputePipelineVulkan() override;

    const ComputePipelineDesc &Desc() const { return desc_; }

    VkPipeline RawPipeline() const { return pipeline_; }

    VkPipelineLayout RawPipelineLayout() const { return pipeline_layout_; }

    VkDescriptorSetLayout RawSetLayout(uint32_t set_index) const { return set_layouts_[set_index]; }

    uint32_t LocalSizeX() const { return desc_.thread_group.x; }
    uint32_t LocalSizeY() const { return desc_.thread_group.y; }
    uint32_t LocalSizeZ() const { return desc_.thread_group.z; }

private:
    Ref<DeviceVulkan> device_;
    ComputePipelineDesc desc_;

    Vec<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
