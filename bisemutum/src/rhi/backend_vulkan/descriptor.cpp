#include "descriptor.hpp"

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>

#include "volk.h"
#include "device.hpp"

namespace bi::rhi {

auto to_vk_descriptor_type(DescriptorType type) -> VkDescriptorType {
    switch (type) {
        case DescriptorType::sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::uniform_buffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::read_only_storage_buffer:
        case DescriptorType::read_write_storage_buffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::sampled_texture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::read_only_storage_texture:
        case DescriptorType::read_write_storage_texture:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::acceleration_structure:
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default: unreachable();
    }
}


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

        start_handle_.cpu = reinterpret_cast<uint64_t>(allocation_info.pMappedData);
        start_handle_.gpu = base_gpu_address();
    } else {
        cpu_buffer_ = Box<std::byte[]>::make(buffer_size);

        start_handle_.cpu = reinterpret_cast<uint64_t>(cpu_buffer_.get());
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

auto DescriptorHeapVulkan::size_of_descriptor(BindGroupLayout const& layout) const -> uint32_t {
    auto desc_set_layout = device_->require_descriptor_set_layout(layout);
    VkDeviceSize size = 0;
    vkGetDescriptorSetLayoutSizeEXT(device_->raw(), desc_set_layout, &size);
    return size;
}

auto DescriptorHeapVulkan::alignment_of_descriptor(BindGroupLayout const& layout) const -> uint32_t {
    uint32_t alignment = device_->descriptor_offset_alignment();
    for (auto& entry : layout) {
        alignment = std::max(alignment, size_of_descriptor(entry.type));
    }
    return alignment;
}

auto DescriptorHeapVulkan::base_gpu_address() const -> uint64_t {
    VkBufferDeviceAddressInfo bda_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = gpu_buffer_,
    };
    return vkGetBufferDeviceAddress(device_->raw(), &bda_info);
}


DescriptorHeapVulkanLegacy::DescriptorHeapVulkanLegacy(Ref<struct DeviceVulkan> device, DescriptorHeapDesc const& desc)
    : device_(device)
{
    std::vector<VkDescriptorPoolSize> pool_sizes{
        {VK_DESCRIPTOR_TYPE_SAMPLER, desc.max_count},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, desc.max_count},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, desc.max_count},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, desc.max_count},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, desc.max_count},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, desc.max_count},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, desc.max_count},
    };
    if (device_->properties().raytracing_pipeline) {
        pool_sizes.push_back(
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 8}
        );
    }
    VkDescriptorPoolCreateInfo desc_pool_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = desc.shader_visible ? 0u : VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = desc.max_count,
        .poolSizeCount = desc.type == DescriptorHeapType::sampler
            ? 1u
            : static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    vkCreateDescriptorPool(device_->raw(), &desc_pool_ci, nullptr, &desc_pool_);

    allocated_sets_.resize(desc.max_count, VK_NULL_HANDLE);

    std::fill(single_descriptor_layouts_.begin(), single_descriptor_layouts_.end(), VK_NULL_HANDLE);
    if (!desc.shader_visible) {
        auto create_single_descriptor_layout = [this](DescriptorType desc_type) {
            VkDescriptorSetLayoutBinding binding_info{
                .binding = 0,
                .descriptorType = to_vk_descriptor_type(desc_type),
                .descriptorCount = 1,
                .stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .pImmutableSamplers = nullptr,
            };
            VkDescriptorSetLayoutCreateInfo set_layout_ci{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .bindingCount = 1,
                .pBindings = &binding_info,
            };
            vkCreateDescriptorSetLayout(
                device_->raw(), &set_layout_ci, nullptr, &single_descriptor_layouts_[static_cast<size_t>(desc_type)]
            );
        };
        create_single_descriptor_layout(DescriptorType::sampler);
        create_single_descriptor_layout(DescriptorType::uniform_buffer);
        create_single_descriptor_layout(DescriptorType::read_write_storage_buffer);
        create_single_descriptor_layout(DescriptorType::sampled_texture);
        create_single_descriptor_layout(DescriptorType::read_write_storage_texture);
        if (device_->properties().raytracing_pipeline) {
            create_single_descriptor_layout(DescriptorType::acceleration_structure);
        }
    }
}

DescriptorHeapVulkanLegacy::~DescriptorHeapVulkanLegacy() {
    for (auto& set_layout : single_descriptor_layouts_) {
        if (set_layout) {
            vkDestroyDescriptorSetLayout(device_->raw(), set_layout, nullptr);
            set_layout = VK_NULL_HANDLE;
        }
    }
    if (desc_pool_) {
        vkDestroyDescriptorPool(device_->raw(), desc_pool_, nullptr);
        desc_pool_ = VK_NULL_HANDLE;
    }
}

auto DescriptorHeapVulkanLegacy::start_address() const -> DescriptorHandle {
    auto addr = reinterpret_cast<uint64_t>(allocated_sets_.data());
    return DescriptorHandle{
        .cpu = addr,
        .gpu = addr,
    };
}

auto DescriptorHeapVulkanLegacy::allocate_descriptor_at(DescriptorHandle handle, DescriptorType type) -> void {
    if (type == DescriptorType::read_only_storage_buffer) {
        type = DescriptorType::read_write_storage_buffer;
    }
    if (type == DescriptorType::read_only_storage_texture) {
        type = DescriptorType::read_write_storage_texture;
    }

    VkDescriptorSetAllocateInfo set_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = desc_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &single_descriptor_layouts_[static_cast<size_t>(type)],
    };
    vkAllocateDescriptorSets(device_->raw(), &set_ci, reinterpret_cast<VkDescriptorSet*>(handle.cpu));
}

auto DescriptorHeapVulkanLegacy::allocate_descriptor_at(DescriptorHandle handle, BindGroupLayout const& layout) -> void {
    auto set_layout = device_->require_descriptor_set_layout(layout);
    VkDescriptorSetAllocateInfo set_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = desc_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout,
    };
    vkAllocateDescriptorSets(device_->raw(), &set_ci, reinterpret_cast<VkDescriptorSet*>(handle.cpu));
}

auto DescriptorHeapVulkanLegacy::free_descriptor_at(DescriptorHandle handle) -> void {
    auto desc_set = *reinterpret_cast<VkDescriptorSet*>(handle.cpu);
    vkFreeDescriptorSets(device_->raw(), desc_pool_, 1, &desc_set);
}

auto DescriptorHeapVulkanLegacy::reset() -> void {
    vkResetDescriptorPool(device_->raw(), desc_pool_, 0);
}

auto DescriptorHeapVulkanLegacy::allocate_descriptor_raw(VkDescriptorSetLayout layout) -> VkDescriptorSet {
    if (curr_pos_ == allocated_sets_.size()) { return VK_NULL_HANDLE; }

    VkDescriptorSetAllocateInfo set_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = desc_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    vkAllocateDescriptorSets(device_->raw(), &set_ci, &allocated_sets_[curr_pos_]);
    ++curr_pos_;
    return allocated_sets_[curr_pos_ - 1];
}

}
