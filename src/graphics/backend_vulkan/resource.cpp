#include "resource.hpp"

#include "device.hpp"
#include "utils.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

VkBufferUsageFlags ToVkBufferUsage(BitFlags<BufferUsage> usage) {
    VkBufferUsageFlags vk_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (usage.Contains(BufferUsage::eVertex)) {
        vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (usage.Contains(BufferUsage::eIndex)) {
        vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (usage.Contains(BufferUsage::eUniform)) {
        vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (usage.Contains(BufferUsage::eStorage)) {
        vk_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (usage.Contains(BufferUsage::eIndirect)) {
        vk_usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    if (usage.Contains(BufferUsage::eAccelerationStructure)) {
        vk_usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    }

    return vk_usage;
}

VmaMemoryUsage ToVmaMemoruUsage(BufferMemoryProperty prop) {
    switch (prop) {
        case BufferMemoryProperty::eGpuOnly: return VMA_MEMORY_USAGE_GPU_ONLY;
        case BufferMemoryProperty::eCpuToGpu: return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case BufferMemoryProperty::eGpuToCpu: return VMA_MEMORY_USAGE_GPU_TO_CPU;
    }
    Unreachable();
}

VkImageType ToVkImageType(TextureDimension dim) {
    switch (dim) {
        case TextureDimension::e1D: return VK_IMAGE_TYPE_1D;
        case TextureDimension::e2D: return VK_IMAGE_TYPE_2D;
        case TextureDimension::e3D: return VK_IMAGE_TYPE_3D;
    }
    Unreachable();
}

VkImageUsageFlags ToVkImageUsage(BitFlags<TextureUsage> usage) {
    VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage.Contains(TextureUsage::eSampled)) {
        vk_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (usage.Contains(TextureUsage::eStorage)) {
        vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (usage.Contains(TextureUsage::eColorAttachment)) {
        vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (usage.Contains(TextureUsage::eDepthStencilAttachment)) {
        vk_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    return vk_usage;
}

VkImageAspectFlags ToVkImageAspect(ResourceFormat format) {
    return IsColorFormat(format) ? VK_IMAGE_ASPECT_COLOR_BIT
        : IsDepthOnlyFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT
        : (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

}

BufferVulkan::BufferVulkan(Ref<DeviceVulkan> device, const BufferDesc &desc) : device_(device), size_(desc.size) {
    VkBufferCreateInfo buffer_ci {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = desc.size,
        .usage = ToVkBufferUsage(desc.usages),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VmaAllocationCreateInfo allocation_ci {
        .flags = desc.persistently_mapped ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0u,
        .usage = ToVmaMemoruUsage(desc.memory_property),
    };

    VmaAllocationInfo allocation_info {};
    vmaCreateBuffer(device_->Allocator(), &buffer_ci, &allocation_ci, &buffer_, &allocation_, &allocation_info);
    mapped_ptr_ = allocation_info.pMappedData;

    if (!desc.name.empty()) {
        VkDebugUtilsObjectNameInfoEXT name_info {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = VK_OBJECT_TYPE_BUFFER,
            .objectHandle = reinterpret_cast<uint64_t>(buffer_),
            .pObjectName = desc.name.c_str(),
        };
        vkSetDebugUtilsObjectNameEXT(device_->Raw(), &name_info);
    }
}

BufferVulkan::~BufferVulkan() {
    vmaDestroyBuffer(device_->Allocator(), buffer_, allocation_);
}


TextureVulkan::TextureVulkan(Ref<DeviceVulkan> device, const TextureDesc &desc) : device_(device) {
    desc_ = desc;

    VkImageCreateInfo image_ci {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = ToVkImageType(desc.dim),
        .format = ToVkFormat(desc.format),
        .extent = {
            desc.extent.width,
            desc.extent.height,
            desc.dim == TextureDimension::e3D ? desc.extent.depth_or_layers : 1,
        },
        .mipLevels = desc.levels,
        .arrayLayers = desc.dim == TextureDimension::e3D ? 1 : desc.extent.depth_or_layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = ToVkImageUsage(desc.usages),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
    };

    VmaAllocationCreateInfo allocation_ci {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    vmaCreateImage(device_->Allocator(), &image_ci, &allocation_ci, &image_, &allocation_, nullptr);

    if (!desc.name.empty()) {
        VkDebugUtilsObjectNameInfoEXT name_info {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = VK_OBJECT_TYPE_IMAGE,
            .objectHandle = reinterpret_cast<uint64_t>(image_),
            .pObjectName = desc.name.c_str(),
        };
        vkSetDebugUtilsObjectNameEXT(device_->Raw(), &name_info);
    }
}

TextureVulkan::TextureVulkan(Ref<DeviceVulkan> device, VkImage raw_image, const TextureDesc &desc)
    : device_(device) {
    image_ = raw_image;
    allocation_ = nullptr;
    desc_ = desc;
}

TextureVulkan::~TextureVulkan() {
    for (const auto &[_, image_view] : cached_views_) {
        vkDestroyImageView(device_->Raw(), image_view, nullptr);
    }

    if (allocation_) {
        vmaDestroyImage(device_->Allocator(), image_, allocation_);
    }
}

VkImageView TextureVulkan::GetView(const TextureViewVulkanDesc &view_desc) const {
    if (auto it = cached_views_.find(view_desc); it != cached_views_.end()) {
        return it->second;
    }

    VkImageViewCreateInfo image_view_ci {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image_,
        .viewType = view_desc.type,
        .format = view_desc.format,
        .components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_R },
        .subresourceRange = {
            .aspectMask = ToVkImageAspect(desc_.format),
            .baseMipLevel = view_desc.base_level,
            .levelCount = view_desc.levels,
            .baseArrayLayer = view_desc.base_layer,
            .layerCount = view_desc.layers,
        },
    };

    VkImageView image_view;
    vkCreateImageView(device_->Raw(), &image_view_ci, nullptr, &image_view);
    cached_views_.insert({view_desc, image_view});
    return image_view;
}

VkFormat TextureVulkan::RawFormat() const {
    return ToVkFormat(desc_.format);
}

VkImageAspectFlags TextureVulkan::GetAspect() const {
    return ToVkImageAspect(desc_.format);
}

void TextureVulkan::GetDepthAndLayer(uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers) const {
    if (desc_.dim == TextureDimension::e3D) {
        depth = depth_or_layers;
        layers = 1;
    } else {
        depth = 1;
        layers = depth_or_layers;
    }
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
