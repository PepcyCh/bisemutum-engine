#pragma once

#include <unordered_map>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <bisemutum/rhi/resource.hpp>

namespace bi::rhi {

struct BufferVulkan final : Buffer {
    BufferVulkan(Ref<struct DeviceVulkan> device, BufferDesc const& desc);
    ~BufferVulkan() override;

    auto map() -> void* override;

    auto unmap() -> void override;

    auto raw() const -> VkBuffer { return buffer_; }

    auto address() const -> uint64_t;

private:
    Ref<DeviceVulkan> device_;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;

    void* mapped_ptr_ = nullptr;
    bool persistently_mapped_ = false;
};


struct TextureViewKeyVulkan final {
    uint32_t base_level;
    uint32_t num_levels;
    uint32_t base_layer;
    uint32_t num_layers;
    VkFormat format;
    VkImageViewType view_type;
    VkImageAspectFlags aspect;

    auto operator==(TextureViewKeyVulkan const& rhs) const -> bool = default;
};

}

template <>
struct std::hash<bi::rhi::TextureViewKeyVulkan> final {
    auto operator()(bi::rhi::TextureViewKeyVulkan const& v) const noexcept -> size_t;
};

namespace bi::rhi {

struct TextureVulkan final : Texture {
    TextureVulkan(Ref<struct DeviceVulkan> device, TextureDesc const& desc);
    // external image
    TextureVulkan(Ref<struct DeviceVulkan> device, VkImage raw_image, TextureDesc const& desc);
    ~TextureVulkan() override;

    auto raw() const -> VkImage { return image_; }

    auto raw_format() const -> VkFormat;

    auto get_aspect(bool single_aspect = false) const -> VkImageAspectFlags;

    auto get_view(
        uint32_t base_level, uint32_t num_levels, uint32_t base_layer, uint32_t num_layers,
        VkFormat format, VkImageViewType view_type, bool single_aspect
    ) -> VkImageView;

    auto get_depth_and_layer(uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers, uint32_t another = 1) const -> void;

    auto get_current_layout() const -> VkImageLayout { return current_layout_; }
    auto set_current_layout(VkImageLayout layout) -> void { current_layout_ = layout; }

private:
    Ref<DeviceVulkan> device_;
    VkImage image_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;

    VkImageLayout current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;

    std::unordered_map<TextureViewKeyVulkan, VkImageView> views_;
};

}
