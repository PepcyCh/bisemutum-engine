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

    auto static_samplers_set() const -> uint32_t;

private:
    Ref<DeviceVulkan> device_;

    std::vector<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

struct ComputePipelineVulkan final : ComputePipeline {
    ComputePipelineVulkan(Ref<DeviceVulkan> device, ComputePipelineDesc const& desc);
    ~ComputePipelineVulkan() override;

    auto raw() const -> VkPipeline { return pipeline_; }

    auto raw_layout() const -> VkPipelineLayout { return pipeline_layout_; }

    auto raw_set_layout(uint32_t set_index) const -> VkDescriptorSetLayout { return set_layouts_[set_index]; }

    auto static_samplers_set() const -> uint32_t;

private:
    Ref<DeviceVulkan> device_;

    std::vector<VkDescriptorSetLayout> set_layouts_;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

}
