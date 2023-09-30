#pragma once

#include <array>
#include <unordered_map>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <bisemutum/rhi/device.hpp>

namespace bi::rhi {

struct DescriptorHeapVulkanLegacy;

struct DescriptorSetLayoutHashHelper final {
    auto operator()(BindGroupLayout const& v) const noexcept -> size_t;

    auto operator()(BindGroupLayout const& a, BindGroupLayout const& b) const noexcept -> bool;
};

struct DeviceVulkan final : Device {
    DeviceVulkan(DeviceDesc const& desc);
    ~DeviceVulkan() override;

    static auto create(DeviceDesc const& desc) -> Box<DeviceVulkan>;

    auto get_backend() const -> Backend override { return Backend::vulkan; }

    auto get_queue(QueueType type) -> Ref<Queue> override;

    auto create_command_pool(CommandPoolDesc const& desc) -> Box<CommandPool> override;

    auto create_swapchain(SwapchainDesc const& desc) -> Box<Swapchain> override;

    auto create_fence() -> Box<Fence> override;

    auto create_semaphore() -> Box<Semaphore> override;

    auto create_buffer(BufferDesc const& desc) -> Box<Buffer> override;

    auto create_texture(TextureDesc const& desc) -> Box<Texture> override;

    auto create_sampler(SamplerDesc const& desc) -> Box<Sampler> override;

    auto create_descriptor_heap(DescriptorHeapDesc const& desc) -> Box<DescriptorHeap> override;

    auto create_shader_module(ShaderModuleDesc const& desc) -> Box<ShaderModule> override;

    auto create_graphics_pipeline(GraphicsPipelineDesc const& desc) -> Box<GraphicsPipeline> override;

    auto create_compute_pipeline(ComputePipelineDesc const& desc) -> Box<ComputePipeline> override;

    auto create_descriptor(BufferDescriptorDesc const& buffer_desc, DescriptorHandle handle) -> void override;
    auto create_descriptor(TextureDescriptorDesc const& texture_desc, DescriptorHandle handle) -> void override;
    auto create_descriptor(Ref<Sampler> sampler, DescriptorHandle handle) -> void override;

    auto copy_descriptors(
        DescriptorHandle dst_desciptor,
        CSpan<DescriptorHandle> src_descriptors,
        BindGroupLayout const& bind_group_layout
    ) -> void override;

    auto initialize_pipeline_cache_from(std::string_view cache_file_path) -> void override;

    auto raw() const -> VkDevice { return device_; }
    auto raw_physical_device() const -> VkPhysicalDevice { return physical_device_; }
    auto raw_instance() const -> VkInstance { return instance_; }

    auto allocator() const -> VmaAllocator { return allocator_; }

    auto size_of_descriptor(DescriptorType type) const -> uint32_t {
        return descriptor_sizes_[static_cast<size_t>(type)];
    }
    auto descriptor_offset_alignment() const -> uint64_t { return descriptor_offset_alignment_; }

    auto pipeline_cache() const -> VkPipelineCache { return pipeline_cache_; }

    auto use_descriptor_buffer() const -> bool { return support_descriptor_buffer_; }

    auto require_descriptor_set_layout(BindGroupLayout const& layout) -> VkDescriptorSetLayout;

    auto immutable_samplers_heap() -> Ptr<DescriptorHeapVulkanLegacy>;

private:
    auto initialize_device_properties() -> void;

    auto pick_device(DeviceDesc const& desc) -> void;
    auto init_allocator() -> void;
    auto init_descriptor_sizes() -> void;

    auto save_pipeline_cache(bool destroy) -> void;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physical_device_props_;
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = nullptr;

    VkDebugUtilsMessengerEXT debug_utils_messenger_ = VK_NULL_HANDLE;

    std::array<uint32_t, 3> queue_family_indices_;
    std::array<Box<struct QueueVulkan>, 3> queues_;

    bool support_descriptor_buffer_ = false;
    std::array<uint32_t, static_cast<size_t>(DescriptorType::count)> descriptor_sizes_;
    uint64_t descriptor_offset_alignment_;

    VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
    std::string pipeline_cache_file_path_;

    std::unordered_map<
        BindGroupLayout, VkDescriptorSetLayout,
        DescriptorSetLayoutHashHelper, DescriptorSetLayoutHashHelper
    > cached_desc_set_layouts_;
    Box<DescriptorHeapVulkanLegacy> immutable_samplers_heap_;
};

}
