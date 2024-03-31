#include "resource.hpp"

#include <bisemutum/prelude/hash.hpp>
#include <bisemutum/prelude/misc.hpp>

#include "volk.h"
#include "device.hpp"
#include "utils.hpp"

auto std::hash<bi::rhi::TextureViewKeyVulkan>::operator()(
    bi::rhi::TextureViewKeyVulkan const& v
) const noexcept -> size_t {
    return bi::hash(v.base_level, v.num_levels, v.base_layer, v.num_layers, v.format, v.view_type, v.aspect);
}

namespace bi::rhi {

namespace {

auto to_vk_buffer_usage(BitFlags<BufferUsage> usage) -> VkBufferUsageFlags{
    VkBufferUsageFlags vk_usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (usage.contains_any(BufferUsage::vertex)) {
        vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (usage.contains_any(BufferUsage::index)) {
        vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (usage.contains_any(BufferUsage::uniform)) {
        vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (usage.contains_any(BufferUsage::storage_read)) {
        vk_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (usage.contains_any(BufferUsage::indirect)) {
        vk_usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    if (usage.contains_any(BufferUsage::acceleration_structure)) {
        vk_usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    }
    if (usage.contains_any(BufferUsage::acceleration_structure_build)) {
        vk_usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }
    if (usage.contains_any(BufferUsage::shader_binding_table)) {
        vk_usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    }
    return vk_usage;
}

auto to_vk_image_type(TextureDimension dim) -> VkImageType {
    switch (dim) {
        case TextureDimension::d1: return VK_IMAGE_TYPE_1D;
        case TextureDimension::d2: return VK_IMAGE_TYPE_2D;
        case TextureDimension::d3: return VK_IMAGE_TYPE_3D;
    }
    unreachable();
}

auto to_vk_image_usage(BitFlags<TextureUsage> usage) -> VkImageUsageFlags {
    VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage.contains_any(TextureUsage::sampled)) {
        vk_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (usage.contains_any(TextureUsage::storage_read)) {
        vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (usage.contains_any(TextureUsage::color_attachment)) {
        vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (usage.contains_any(TextureUsage::depth_stencil_attachment)) {
        vk_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    return vk_usage;
}

auto to_vk_image_aspect(ResourceFormat format) -> VkImageAspectFlags {
    return is_color_format(format) ? VK_IMAGE_ASPECT_COLOR_BIT
        : is_depth_only_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT
        : (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

}

BufferVulkan::BufferVulkan(Ref<DeviceVulkan> device, BufferDesc const& desc) : device_(device) {
    desc_ = desc;
    VkBufferCreateInfo buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = desc_.size,
        .usage = to_vk_buffer_usage(desc_.usages),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VmaAllocationCreateInfo allocation_ci{};
    switch (desc.memory_property) {
        case BufferMemoryProperty::gpu_only:
            allocation_ci.flags = 0;
            allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            break;
        case BufferMemoryProperty::cpu_to_gpu:
            allocation_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            break;
        case BufferMemoryProperty::gpu_to_cpu:
            allocation_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            break;
        default: unreachable();
    }
    if (desc_.persistently_mapped) {
        allocation_ci.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo allocation_info{};
    vmaCreateBuffer(device_->allocator(), &buffer_ci, &allocation_ci, &buffer_, &allocation_, &allocation_info);
    mapped_ptr_ = allocation_info.pMappedData;
    persistently_mapped_ = desc_.persistently_mapped;
}

BufferVulkan::~BufferVulkan() {
    if (allocation_) {
        unmap();
        vmaDestroyBuffer(device_->allocator(), buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
    }
}

auto BufferVulkan::map() -> void* {
    if (mapped_ptr_ == nullptr) {
        vmaMapMemory(device_->allocator(), allocation_, &mapped_ptr_);
    }
    return mapped_ptr_;
}

auto BufferVulkan::unmap() -> void {
    if (!persistently_mapped_ && mapped_ptr_) {
        vmaUnmapMemory(device_->allocator(), allocation_);
        mapped_ptr_ = nullptr;
    }
}

auto BufferVulkan::address() const -> uint64_t {
    VkBufferDeviceAddressInfo bda_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer_,
    };
    return vkGetBufferDeviceAddress(device_->raw(), &bda_info);
}


TextureVulkan::TextureVulkan(Ref<DeviceVulkan> device, TextureDesc const& desc) : device_(device) {
    desc_ = desc;
    VkImageCreateInfo image_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = to_vk_image_type(desc_.dim),
        .format = to_vk_format(desc_.format),
        .extent = {
            desc_.extent.width,
            desc_.extent.height,
            desc_.dim == TextureDimension::d3 ? desc_.extent.depth_or_layers : 1,
        },
        .mipLevels = desc_.levels,
        .arrayLayers = desc_.dim == TextureDimension::d3 ? 1 : desc_.extent.depth_or_layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = to_vk_image_usage(desc_.usages),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    current_layout_ = image_ci.initialLayout;

    if (
        desc.dim == TextureDimension::d2
        && desc.extent.width == desc.extent.height
        && desc.extent.depth_or_layers >= 6
    ) {
        image_ci.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo allocation_ci{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    vmaCreateImage(device_->allocator(), &image_ci, &allocation_ci, &image_, &allocation_, nullptr);
}

TextureVulkan::TextureVulkan(Ref<DeviceVulkan> device, VkImage raw_image, TextureDesc const& desc) : device_(device) {
    desc_ = desc;
    image_ = raw_image;
    allocation_ = nullptr;
}

TextureVulkan::~TextureVulkan() {
    for (auto [_, view] : views_) {
        vkDestroyImageView(device_->raw(), view, nullptr);
    }
    views_.clear();
    if (allocation_) {
        vmaDestroyImage(device_->allocator(), image_, allocation_);
        allocation_ = VK_NULL_HANDLE;
    }
    image_ = VK_NULL_HANDLE;
}

auto TextureVulkan::raw_format() const -> VkFormat{
    return to_vk_format(desc_.format);
}

auto TextureVulkan::get_aspect(bool single_aspect) const -> VkImageAspectFlags {
    auto aspect = to_vk_image_aspect(desc_.format);
    if (single_aspect && ((aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0)) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    return aspect;
}

auto TextureVulkan::get_view(
    uint32_t base_level, uint32_t num_levels, uint32_t base_layer, uint32_t num_layers,
    VkFormat format, VkImageViewType view_type, bool single_aspect
) -> VkImageView {
    auto aspect = get_aspect(single_aspect);
    auto [view_it, create] = views_.try_emplace({
        base_level, num_levels, base_layer, num_layers, format, view_type, aspect
    });
    if (create) {
        VkImageViewCreateInfo view_ci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image_,
            .viewType = view_type,
            .format = format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A,
            },
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = base_level,
                .levelCount = num_levels,
                .baseArrayLayer = base_layer,
                .layerCount = num_layers,
            },
        };
        vkCreateImageView(device_->raw(), &view_ci, nullptr, &view_it->second);
    }
    return view_it->second;
}

void TextureVulkan::get_depth_and_layer(
    uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers, uint32_t another
) const {
    if (desc_.dim == TextureDimension::d3) {
        depth = depth_or_layers;
        layers = another;
    } else {
        depth = another;
        layers = depth_or_layers;
    }
}

}
