#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "core/container.hpp"
#include "graphics/resource.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class BufferVulkan : public Buffer {
public:
    BufferVulkan(class DeviceVulkan *device, const BufferDesc &desc);
    ~BufferVulkan() override;

    VkBuffer Raw() const { return buffer_; }

private:
    DeviceVulkan *device_;
    VkBuffer buffer_;
    VmaAllocation allocation_;

    void *mapped_ptr_ = nullptr;
};

struct TextureViewVulkanDesc {
    uint32_t base_layer = 0;
    uint32_t layers = 0;
    uint32_t base_level = 0;
    uint32_t levels = 0;

    bool operator==(const TextureViewVulkanDesc &rhs) const = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

template<>
struct std::hash<bismuth::gfx::TextureViewVulkanDesc> {
    size_t operator()(const bismuth::gfx::TextureViewVulkanDesc &v) const noexcept {
        return bismuth::Hash(v.base_layer, v.layers, v.base_level, v.levels);
    }
};

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class TextureVulkan : public Texture {
public:
    TextureVulkan(class DeviceVulkan *device, const TextureDesc &desc);
    // external image
    TextureVulkan(class DeviceVulkan *device, VkImage raw_image, const TextureDesc &desc);
    ~TextureVulkan() override;

    VkImage Raw() const { return image_; }

    VkImageView GetView(const TextureViewVulkanDesc &view_desc) const;

    const TextureDesc &Desc() const { return desc_; }

    VkFormat Format() const;

    VkImageLayout Layout() const { return layout_; }

    VkImageAspectFlags GetAspect() const;

    void GetDepthAndLayer(uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers);

private:
    DeviceVulkan *device_;
    VkImage image_;
    VmaAllocation allocation_;
    TextureDesc desc_;
    VkImageLayout layout_;

    mutable HashMap<TextureViewVulkanDesc, VkImageView> cached_views_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
