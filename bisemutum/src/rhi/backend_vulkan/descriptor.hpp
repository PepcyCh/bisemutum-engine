#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <bisemutum/rhi/descriptor.hpp>
#include <bisemutum/prelude/ref.hpp>
#include <bisemutum/prelude/box.hpp>

namespace bi::rhi {

struct DescriptorHeapVulkan final : DescriptorHeap {
    DescriptorHeapVulkan(Ref<struct DeviceVulkan> device, DescriptorHeapDesc const& desc);
    ~DescriptorHeapVulkan() override;

    auto size_of_descriptor(DescriptorType type) const -> uint32_t override;

    auto allocate_descriptor(DescriptorType type) -> DescriptorHandle override;
    auto allocate_descriptor(BindGroupLayout const& layout) -> DescriptorHandle override;

    auto type() -> VkBufferUsageFlags { return type_; }

    auto base_gpu_address() const -> uint64_t;

private:
    Ref<DeviceVulkan> device_;
    VkBufferUsageFlags type_ = 0;
    VkBuffer gpu_buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    Box<uint8_t[]> cpu_buffer_;
    uint8_t* mapped_cpu_buffer_ = nullptr;

    uint32_t curr_pos_ = 0;
    uint32_t top_pos_ = 0;
};

}
