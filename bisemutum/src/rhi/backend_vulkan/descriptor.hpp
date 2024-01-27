#pragma once

#include <array>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <bisemutum/rhi/descriptor.hpp>
#include <bisemutum/prelude/ref.hpp>
#include <bisemutum/prelude/box.hpp>

namespace bi::rhi {

auto to_vk_descriptor_type(DescriptorType type) -> VkDescriptorType;

struct DescriptorHeapVulkan final : DescriptorHeap {
    DescriptorHeapVulkan(Ref<struct DeviceVulkan> device, DescriptorHeapDesc const& desc);
    ~DescriptorHeapVulkan() override;

    auto total_heap_size() const -> uint64_t override { return top_pos_; }

    auto size_of_descriptor(DescriptorType type) const -> uint32_t override;
    auto size_of_descriptor(BindGroupLayout const& layout) const -> uint32_t override;
    auto alignment_of_descriptor(DescriptorType type) const -> uint32_t override {
        return size_of_descriptor(type);
    }
    auto alignment_of_descriptor(BindGroupLayout const& layout) const -> uint32_t override;

    auto start_address() const -> DescriptorHandle override { return start_handle_; }

    auto type() -> VkBufferUsageFlags { return type_; }

    auto base_gpu_address() const -> uint64_t;

private:
    Ref<DeviceVulkan> device_;
    VkBufferUsageFlags type_ = 0;
    VkBuffer gpu_buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    Box<std::byte[]> cpu_buffer_;

    DescriptorHandle start_handle_{};
    uint64_t top_pos_ = 0;
};

struct DescriptorHeapVulkanLegacy final : DescriptorHeap {
    DescriptorHeapVulkanLegacy(Ref<struct DeviceVulkan> device, DescriptorHeapDesc const& desc);
    ~DescriptorHeapVulkanLegacy() override;

    auto total_heap_size() const -> uint64_t override { return allocated_sets_.size() * sizeof(VkDescriptorSet); }

    auto size_of_descriptor(DescriptorType type) const -> uint32_t override { return sizeof(VkDescriptorSet); }
    auto size_of_descriptor(BindGroupLayout const& layout) const -> uint32_t override { return sizeof(VkDescriptorSet); }
    auto alignment_of_descriptor(DescriptorType type) const -> uint32_t override {
        return size_of_descriptor(type);
    }
    auto alignment_of_descriptor(BindGroupLayout const& layout) const -> uint32_t override {
        return size_of_descriptor(layout);
    }

    auto start_address() const -> DescriptorHandle override;

    auto allocate_descriptor_at(DescriptorHandle handle, DescriptorType type) -> void override;
    auto allocate_descriptor_at(DescriptorHandle handle, BindGroupLayout const& layout) -> void override;
    auto free_descriptor_at(DescriptorHandle handle) -> void override;
    auto reset() -> void override;

    auto allocate_descriptor_raw(VkDescriptorSetLayout layout) -> VkDescriptorSet;

private:
    Ref<DeviceVulkan> device_;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;

    std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorType::count)> single_descriptor_layouts_;

    std::vector<VkDescriptorSet> allocated_sets_;
    uint32_t curr_pos_ = 0;
};

}
