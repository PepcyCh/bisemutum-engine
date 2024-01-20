#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/rhi/pipeline.hpp>

namespace bi::rhi {

struct DeviceVulkan;

struct GraphicsPipelineVulkan final : GraphicsPipeline {
    GraphicsPipelineVulkan(Ref<DeviceVulkan> device, GraphicsPipelineDesc const& desc);
    ~GraphicsPipelineVulkan() override;

    auto raw() const -> VkPipeline { return pipeline_; }

    auto raw_layout() const -> VkPipelineLayout { return pipeline_layout_; }

    auto raw_set_layout(uint32_t set_index) const -> VkDescriptorSetLayout { return set_layouts_[set_index]; }

    auto push_constants_stages() const -> VkShaderStageFlags;

    auto static_samplers_set() const -> uint32_t;
    auto immutable_samplers_desc_set() const -> VkDescriptorSet { return immutable_samplers_set_; }

private:
    Ref<DeviceVulkan> device_;

    std::vector<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSet immutable_samplers_set_ = VK_NULL_HANDLE;
};

struct ComputePipelineVulkan final : ComputePipeline {
    ComputePipelineVulkan(Ref<DeviceVulkan> device, ComputePipelineDesc const& desc);
    ~ComputePipelineVulkan() override;

    auto raw() const -> VkPipeline { return pipeline_; }

    auto raw_layout() const -> VkPipelineLayout { return pipeline_layout_; }

    auto raw_set_layout(uint32_t set_index) const -> VkDescriptorSetLayout { return set_layouts_[set_index]; }

    auto push_constants_stages() const -> VkShaderStageFlags;

    auto static_samplers_set() const -> uint32_t;
    auto immutable_samplers_desc_set() const -> VkDescriptorSet { return immutable_samplers_set_; }

private:
    Ref<DeviceVulkan> device_;

    std::vector<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSet immutable_samplers_set_ = VK_NULL_HANDLE;
};

struct RaytracingPipelineVulkan final : RaytracingPipeline {
    RaytracingPipelineVulkan(Ref<DeviceVulkan> device, RaytracingPipelineDesc const& desc);
    ~RaytracingPipelineVulkan() override;

    auto get_shader_binding_table_sizes() const -> RaytracingShaderBindingTableSizes override;

    auto get_shader_handle(
        RaytracingShaderBindingTableType type, uint32_t from_index, uint32_t count, void* dst_data
    ) const -> void override;

    auto raw() const -> VkPipeline { return pipeline_; }

    auto raw_layout() const -> VkPipelineLayout { return pipeline_layout_; }

    auto raw_set_layout(uint32_t set_index) const -> VkDescriptorSetLayout { return set_layouts_[set_index]; }

    auto push_constants_stages() const -> VkShaderStageFlags;

    auto static_samplers_set() const -> uint32_t;
    auto immutable_samplers_desc_set() const -> VkDescriptorSet { return immutable_samplers_set_; }

private:
    Ref<DeviceVulkan> device_;

    std::vector<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSet immutable_samplers_set_ = VK_NULL_HANDLE;
};

}
