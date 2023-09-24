#include "descriptor.hpp"

#include <format>

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>

#include "volk.h"
#include "device.hpp"
#include "utils.hpp"
#include "resource.hpp"
#include "sampler.hpp"

namespace bi::rhi {

DescriptorHeapVulkan::DescriptorHeapVulkan(Ref<DeviceVulkan> device, DescriptorHeapDesc const& desc) : device_(device) {
    auto buffer_size = device_->size_of_descriptor(DescriptorType::none) * desc.max_count;
    top_pos_ = buffer_size;

    if (desc.shader_visible) {
        type_ = desc.type == DescriptorHeapType::resource
            ? VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
            : VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
        VkBufferCreateInfo buffer_ci{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = buffer_size,
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | type_,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        VmaAllocationCreateInfo allocation_ci{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        };
        VmaAllocationInfo allocation_info{};
        vmaCreateBuffer(device_->allocator(), &buffer_ci, &allocation_ci, &gpu_buffer_, &allocation_, &allocation_info);
        mapped_cpu_buffer_ = reinterpret_cast<uint8_t*>(allocation_info.pMappedData);
    } else {
        cpu_buffer_ = Box<uint8_t[]>::make(buffer_size);
    }
}

DescriptorHeapVulkan::~DescriptorHeapVulkan() {
    if (allocation_) {
        vmaDestroyBuffer(device_->allocator(), gpu_buffer_, allocation_);
        gpu_buffer_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
    }
}

auto DescriptorHeapVulkan::size_of_descriptor(DescriptorType type) const -> uint32_t {
    return type == DescriptorType::none || type == DescriptorType::count
        ? 0u
        : device_->size_of_descriptor(type);
}

auto DescriptorHeapVulkan::allocate_descriptor(DescriptorType type) -> DescriptorHandle {
    auto size = size_of_descriptor(type);
    if (size == 0) { return {}; }
    if (curr_pos_ + size > top_pos_) { return {}; }
    DescriptorHandle handle{};
    if (gpu_buffer_) {
        handle.cpu = reinterpret_cast<uint64_t>(mapped_cpu_buffer_) + curr_pos_;
        handle.gpu = base_gpu_address() + curr_pos_;
    } else {
        handle.cpu = reinterpret_cast<uint64_t>(cpu_buffer_.get()) + curr_pos_;
    }
    curr_pos_ += size;
    return handle;
}

auto DescriptorHeapVulkan::allocate_descriptor(BindGroupLayout const& layout) -> DescriptorHandle {
    uint64_t size = 0;
    for (auto const& entry : layout) {
        auto type_size = size_of_descriptor(entry.type);
        size = aligned_size<uint64_t>(size, type_size);
        size += type_size * entry.count;
    }
    size = aligned_size(size, device_->descriptor_offset_alignment());
    size = aligned_size(size, 256ull);
    if (size == 0) { return {}; }
    if (curr_pos_ + size > top_pos_) { return {}; }
    DescriptorHandle handle{};
    if (gpu_buffer_) {
        handle.cpu = reinterpret_cast<uint64_t>(mapped_cpu_buffer_) + curr_pos_;
        handle.gpu = base_gpu_address() + curr_pos_;
    } else {
        handle.cpu = reinterpret_cast<uint64_t>(cpu_buffer_.get()) + curr_pos_;
    }
    curr_pos_ += size;
    return handle;
}

auto DescriptorHeapVulkan::base_gpu_address() const -> uint64_t {
    VkBufferDeviceAddressInfo bda_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = gpu_buffer_,
    };
    return vkGetBufferDeviceAddress(device_->raw(), &bda_info);
}

}
