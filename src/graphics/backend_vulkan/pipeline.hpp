#pragma once

#include "graphics/pipeline.hpp"
#include "shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class RenderPipelineVulkan : public RenderPipeline {
public:
    RenderPipelineVulkan(Ref<class DeviceVulkan> device, const RenderPipelineDesc &desc);
    ~RenderPipelineVulkan() override;

    VkPipeline RawPipeline() const { return pipeline_; }

    VkPipelineLayout RawPipelineLayout() const { return pipeline_layout_; }

    VkDescriptorSetLayout RawSetLayout(uint32_t set_index) const { return set_layouts_[set_index]; }

    const ShaderInfoVulkan &ShaderInfo() const { return shader_info_; }

private:
    Ref<DeviceVulkan> device_;
    RenderPipelineDesc desc_;
    ShaderInfoVulkan shader_info_;

    Vec<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;
};

class ComputePipelineVulkan : public ComputePipeline {
public:
    ComputePipelineVulkan(Ref<class DeviceVulkan> device, const ComputePipelineDesc &desc);
    ~ComputePipelineVulkan() override;

    VkPipeline RawPipeline() const { return pipeline_; }

    VkPipelineLayout RawPipelineLayout() const { return pipeline_layout_; }

    VkDescriptorSetLayout RawSetLayout(uint32_t set_index) const { return set_layouts_[set_index]; }

    const ShaderInfoVulkan &ShaderInfo() const { return shader_info_; }

    uint32_t LocalSizeX() const { return shader_info_.compute_local_size_x; }
    uint32_t LocalSizeY() const { return shader_info_.compute_local_size_y; }
    uint32_t LocalSizeZ() const { return shader_info_.compute_local_size_z; }

private:
    Ref<DeviceVulkan> device_;
    ComputePipelineDesc desc_;
    ShaderInfoVulkan shader_info_;

    Vec<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
